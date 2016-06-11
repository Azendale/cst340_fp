#include <iostream>
#include <vector>
#include <string>

extern "C"
{
	#include <ctype.h>
	#include <unistd.h>
	#include <stdlib.h>
	#include <getopt.h>
	#include <netdb.h>
	#include <netinet/in.h>
	#include <netinet/tcp.h>
	#include <errno.h>
	// for perror
	#include <stdio.h>
	#include <sys/socket.h>
	// For memset
	#include <string.h>
	#include <signal.h>
}

#include "FdState.h"
#include "netDefines.h"

static std::vector<FdState> Fds;
static int maxFd = 3;

std::string getPortString(int argc, char ** argv)
{
	int arg;
	char * portString = NULL;
	while (-1 != (arg = getopt(argc, argv, "p:")))
	{
		if ('p' == arg)
		{
			portString = optarg;
		}
	}
	if (NULL == portString)
	{
		std::cerr << "No port number or service name set. Please specify it with -p <port_number>.\n";
		return std::string("");
	}
	return std::string(portString);
}

void fdAddSet(int newFd, fd_set * set)
{
	if (newFd > maxFd)
	{
		maxFd = newFd;
	}
	FD_SET(newFd, set);
}

int SetUpListing(std::string portString, fd_set & readList, int & sockfd)
{
	// Gives getaddrinfo hints about the critera for the addresses it returns
	struct addrinfo hints;
	// Points to list of results from getaddrinfo
	struct addrinfo *serverinfo;
	
	// Erase the struct
	memset(&hints, 0, sizeof(hints));
	
	// Then set what we actually want:
	hints.ai_family = AF_INET6;
	// We want to do TCP
	hints.ai_socktype = SOCK_STREAM;
	// We want to listen (we are the server)
	hints.ai_flags = AI_PASSIVE;
	
	int status = 0;
	if (0 != (status = getaddrinfo(NULL, portString.c_str(), &hints, &serverinfo)))
	{
		// There was a problem, print it out
		std::cerr << "Trouble with getaddrinfo, error was " <<  gai_strerror(status) << ".\n";
	}
	
	struct addrinfo * current = serverinfo;
	// Traverse results list until one of them works to open
	sockfd = socket(current->ai_family, current->ai_socktype, current->ai_protocol);
	while (-1 == sockfd && NULL != current->ai_next)
	{
		current = current->ai_next;
		sockfd = socket(current->ai_family, current->ai_socktype, current->ai_protocol);
	}
	if (-1 == sockfd)
	{
		std::cerr <<  "We tried valliantly, but we were unable to open the socket with what getaddrinfo gave us.\n";
		freeaddrinfo(serverinfo);
		return 8;
	}
	
	// Ok, say we want a socket that abstracts away whether we are doing IPv6 or
	// IPv4 by just having it handle IPv4 mapped addresses for us
	int no = 0;
	if (0 > setsockopt(sockfd, IPPROTO_IPV6, IPV6_V6ONLY, (void *)&no, sizeof(no)))
	{
		std::cerr << "Trouble setting socket option to also listen on IPv4 in addition to IPv6. Falling back to IPv6 only.\n";
	}
	
	int yes = 1;
	if (0 > 
		setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (void *)&yes, sizeof(yes)))
	{
		std::cerr <<  "Couldn't set option to re-use addresses. The server will still try to start, but if the address & port has been in use recently (think last minute range), binding may fail.\n";
	}
	
	// Ok, so now we have a socket. Lets try to bind to it
	if (-1 == bind(sockfd, current->ai_addr, current->ai_addrlen))
	{
		// Couldn't bind
		std::cerr << "We couldn't bind to the socket.\n";
		// Also cleans up the memory pointed to by current
		freeaddrinfo(serverinfo);
		serverinfo = NULL;
		return 16;
	}
	
	// Also cleans up the memory pointed to by current
	freeaddrinfo(serverinfo);
	serverinfo = NULL;
	
	if (-1 == listen(sockfd, 10))
	{
		// Couldn't listen
		std::cerr << "Call to listen failed.\n";
		return 32;
	}
	
	// Been modifying reference to the accept var through sockfd reference all along
	
	// Add new FD to list with correct state
	Fds.push_back(FdState(sockfd, FD_STATE_ACCEPT_SOCK));
	fdAddSet(sockfd, &readList);
	
	return 0;
}

void sigint(int signal)
{
	// Close open FDs
}

int acceptConnection(int sockfd, fd_set & readSet)
{
	int acceptfd = -1;
	if (-1 == (acceptfd = accept(sockfd, NULL, NULL)))
	{
		perror("Trouble accept()ing a connection");
		// Things the man page says we should check for and try again after
		if (!(EAGAIN == errno || ENETDOWN == errno || EPROTO == errno || \
			ENOPROTOOPT == errno || EHOSTDOWN == errno || ENONET == errno \
			|| EHOSTUNREACH == errno || EOPNOTSUPP == errno || ENETUNREACH))
		{
			// Something the man page didn't list went wrong, let's give up
			close(sockfd);
			FD_CLR(sockfd, &readSet);
		}
	}
	else
	{
		FdState newConnection(acceptfd, FD_STATE_ANON);
		Fds.push_back(newConnection);
		newConnection.SetRead(sizeof(uint32_t));
		fdAddSet(newConnection.GetFD(), &readSet);
	}
	return 0;
}

int removeFd(std::vector<FdState> container, int fd)
{
	int returnVal = 1;
	for (std::vector<FdState>::iterator it = container.begin(); it!=container.end(); ++it)
	{
		if (fd == it->GetFD())
		{
			container.erase(it);
			returnVal = 0;
			break;
		}
	}
	return returnVal;
}

int abortConnection(FdState & state, fd_set & readSet, fd_set & writeSet)
{
	int returnVal = 0;
	// Remove from read set
	FD_CLR(state.GetFD(), &readSet);
	// Remove it from the write set
	FD_CLR(state.GetFD(), &writeSet);
	// Remove from FD list
	if (close(state.GetFD()))
	{
		returnVal = 1;
	}
	if (removeFd(Fds, state.GetFD()))
	{
		returnVal = 2;
	}
	return returnVal;
}

void anonRead(FdState & state, fd_set & readSet, fd_set & writeSet)
{
	uint32_t request;
	short readSize;
	char * result;
	result = state.GetRead(readSize);
	if (readSize != sizeof(uint32_t))
	{
		// Read amount was not the expected size
		abortConnection(state, readSet, writeSet);
		return;
	}
	request = *((uint32_t *)result);
	request= ntohl(request);
	if ((request & ACTION_MASK) == ACTION_NAME_REQUEST)
	{
		// Stop waiting for a read, or change what we are waiting for
		// Get size of name and check it for validness
		uint32_t nameSize = request & TRANSFER_SIZE_MASK;
		if (nameSize > 0 && nameSize < MAX_NAME_LEN)
		{
			// Set up for a transfer of nameSize
			state.SetState(FD_STATE_ANON_NAME_SIZE);
			state.SetRead(nameSize);
		}
		else
		{
			// Invalid name size, abort connection.
			abortConnection(state, readSet, writeSet);
		}
	}
	else
	{
		// All other requests are invalid state transitions
		abortConnection(state, readSet, writeSet);
	}
}

FdState * findByName(const std::string & name)
{
	for (auto& state: Fds)
	{
		if (state.GetName() == name && state.GetState() == FD_STATE_LOBBY)
		{
			return &state;
		}
	}
	return nullptr;
}

std::string getNameList()
{
	std::string returnVal("Available players:\n");
	for (auto& state: Fds)
	{
		if (state.GetState() == FD_STATE_LOBBY)
		{
			returnVal += state.GetName();
			returnVal += "\n";
		}
	}
	return returnVal;
}

// Called after finishing read in FD_STATE_ANON_NAME_SIZE
void nameRead(FdState & state, fd_set & readSet, fd_set & writeSet)
{
	short readLen;
	char * nameResult = state.GetRead(readLen);
	if (readLen > 0 && readLen < MAX_NAME_LEN)
	{
		std::string reqName = std::string(nameResult);
		uint32_t response = 0;
		if (findByName(reqName))
		{
			// Send error that the name is already taken
			response = ACTION_NAME_TAKEN;
			state.SetState(FD_STATE_NAME_REJECT);
		}
		else
		{
			// Give them the name
			state.SetName(reqName);
			// Tell them they are in the lobby
			response = ACTION_NAME_IS_YOURS;
			state.SetState(FD_STATE_NAME_ACCEPT);
		}
		state.SetWrite((char *)(&response), 32/8);
		// Not reading again until the write finishes
		FD_CLR(state.GetFD(), &readSet);
		fdAddSet(state.GetFD(), &writeSet);
	}
	else
	{
		// Name that was read was too big
		abortConnection(state, readSet, writeSet);
	}
}

// In state FD_STATE_NAME_REJECT after successfully writing rejection. Switches to listing in anon state
void nameRejectAfterWrite(FdState & state, fd_set & readSet, fd_set & writeSet)
{
	// Switch to read
	FD_CLR(state.GetFD(), &writeSet);
	fdAddSet(state.GetFD(), &readSet);
	// State: anon
	state.SetState(FD_STATE_ANON);
	// Read size:
	state.SetRead(32/8);
}

// In state FD_STATE_NAME_ACCEPT after successfully writing response. Switches to listing in lobby mode
void nameAcceptAfterWrite(FdState & state, fd_set & readSet, fd_set & writeSet)
{
	// Switch to read to listen for name listing command
	FD_CLR(state.GetFD(), &writeSet);
	fdAddSet(state.GetFD(), &readSet);
	// State: lobby
	state.SetState(FD_STATE_LOBBY);
	// Read size:
	state.SetRead(32/8);
}

// Called after finishing write in FD_STATE_NAME_ACCEPT or FD_STATE_NAME_REJECT state
void nameResponseWriteFinish(FdState & state, fd_set & readSet, fd_set & writeSet)
{
	if (state.GetState() == FD_STATE_NAME_ACCEPT)
	{
		// In Lobby now
		state.SetState(FD_STATE_LOBBY);
	}
	else if (state.GetState() == FD_STATE_NAME_REJECT)
	{
		// Anonymous again
		state.SetState(FD_STATE_ANON);
	}
	// Waiting for the client to send us another name or a command to list players
	// Either way, the size to read happens to be the same
	state.SetRead(sizeof(uint32_t));
	FD_CLR(state.GetFD(), &writeSet);
	fdAddSet(state.GetFD(), &readSet);
}

std::string GenerateNetNameList()
{
	std::string nameList = getNameList();
	uint32_t len = nameList.length();
	if (len >= LONG_TRANSFER_SIZE_MASK)
	{
		nameList = std::string("");
		len = 0;
	}
	len = htonl(len);
	// Pack bytes of length into first 4 chars of string
	std::string returnString(((char *)&len), 32/8);
	returnString += nameList;
	return returnString;
}

// In state FD_STATE_LOBBY, after successfully reading
void lobbyRead(FdState & state, fd_set & readSet, fd_set & writeSet)
{
	// FD can request to play other player
	// FD can request player list
	short readSize;
	char * readData = state.GetRead(readSize);
	if (32/8 != readSize)
	{
		// Read command that was too large
		abortConnection(state, readSet, writeSet);
		return;
	}
	
	uint32_t request = *((uint32_t *)readData);
	request = ntohl(request);
	if ((ACTION_REQ_PLAYERS_LIST & ACTION_MASK) == request)
	{
		std::string list = GenerateNetNameList();
		state.SetWrite(list.c_str(), (short)(list.length()));
		// Switch to write
		fdAddSet(state.GetFD(), &writeSet);
		FD_CLR(state.GetFD(), &readSet);
		state.SetState(FD_STATE_REQ_NAME_LIST);
	}
	else if ((ACTION_PLAY_PLAYERNAME & ACTION_MASK) == request)
	{
		uint32_t nameLen = request & TRANSFER_SIZE_MASK;
		if (nameLen < MAX_NAME_LEN)
		{
			// Switch to name reading state
			state.SetRead(nameLen);
			state.SetState(FD_STATE_OPLYR_NAME_READ);
		}
		else
		{
			// name too long
			abortConnection(state, readSet, writeSet);
		}
	}
	else
	{
		// Invalid state transition: wrong command
		abortConnection(state, readSet, writeSet);
	}
}

// called in state FD_STATE_OPLYR_NAME_READ after reading the other player's name has finished
void otherPlayerNameRead(FdState & state, fd_set & readSet, fd_set & writeSet)
{
	short nameLen;
	char * readData = state.GetRead(nameLen);
	if (nameLen <= 0 || nameLen >= MAX_NAME_LEN)
	{
		// Name was the wrong size
		abortConnection(state, readSet, writeSet);
		return;
	}
	
	std::string otherPlayer(readData, nameLen);
	FdState * otherFd = findByName(otherPlayer);
	if (nullptr == otherFd)
	{
		// No such player
		uint32_t response = ACTION_INVITE_RESPONSE;
		response = response | INVITE_RESPONSE_NO;
		state.SetState(FD_STATE_GAME_REQ_REJECT);
		// Switch to write
		fdAddSet(state.GetFD(), &writeSet);
		FD_CLR(state.GetFD(), &readSet);
		response = htonl(response);
		state.SetWrite((char *)&response, 32/8);
	}
	else
	{
		// Ask other player if they want to play
		// Switch to write with other player
		fdAddSet(otherFd->GetFD(), &writeSet);
		FD_CLR(otherFd->GetFD(), &readSet);
		otherFd->SetState(FD_STATE_GAME_INVITE);
		uint32_t invitation = ACTION_INVITE_REQ;
		uint32_t ourNameLen = state.GetName().length();
		invitation = invitation | ourNameLen;
		invitation = htonl(invitation);
		std::string inviteandname((char *)&invitation, 32/8);
		inviteandname += state.GetName();
		otherFd->SetWrite(inviteandname.c_str(), (short)inviteandname.length());
		
		// remember who we asked to play (so they can find us for the response)
		state.SetOtherPlayer(otherFd);
		// Not reading or writing anymore, waiting on other player
		state.SetState(FD_STATE_REQD_GAME);
		FD_CLR(state.GetFD(), &readSet);
	}
}

// Find the Fd that invited the one pointed to by "invitee"
FdState * findFdByPastInvitation(FdState * invitee)
{
	FdState * returnVal = nullptr;
	for (auto& state: Fds)
	{
		if (state.GetState() == FD_STATE_GAME_INVITE_RESP_WAIT && state.GetOtherPlayer() == invitee)
		{
			returnVal = &state;
		}
	}
	return returnVal;
}

// Switches state of the invited player to reading their response after we have sent them the invatiation. Should be called when the invited player connection finishes a write in FD_STATE_GAME_WRITE
void writeGameInvite(FdState & state, fd_set & readSet, fd_set & writeSet)
{
	// Switch to reading now
	FD_CLR(state.GetFD(), &writeSet);
	fdAddSet(state.GetFD(), &readSet);
	// Switch to the state that means we are waiting for a response
	state.SetState(FD_STATE_GAME_INVITE_RESP_WAIT);
}

// Reads the response after a connection has been invited to a game, should be called after successful read in state FD_STATE_GAME_INVITE_RESP_WAIT
void readStateGameInvite(FdState & state, fd_set & readSet, fd_set & writeSet)
{
	// Need to find out if they said yes or no
	short readSize;
	char * readData = state.GetRead(readSize);
	if (readSize != 32/8)
	{
		// Response read was not the right size
		abortConnection(state, readSet, writeSet);
		return;
	}
	
	uint32_t response = ntohl(*((uint32_t *)readData));
	if ((response & ACTION_MASK) != ACTION_INVITE_RESPONSE)
	{
		// Invalid state transition: not a response to the request
		abortConnection(state, readSet, writeSet);
		return;
	}
	
	// Find matching other connection
	FdState * inviter = findFdByPastInvitation(&state);
	if (nullptr == inviter)
	{
		abortConnection(state, readSet, writeSet);
		return;
	}
	
	if (response & INVITE_RESPONSE_YES)
	{
		// Answered yes
		// other connection goes into FD_STATE_GAME_REQ_ACCEPT, which will be followed by FD_STATE_GAME_OFD_MOVE when the write finishes
		inviter->SetState(FD_STATE_GAME_REQ_ACCEPT);
		uint32_t inviterResponse = ACTION_INVITE_RESPONSE;
		inviterResponse = inviterResponse | INVITE_RESPONSE_YES;
		inviterResponse = htonl(inviterResponse);
		inviter->SetWrite(((char *)&inviterResponse), 32/8);
		fdAddSet(inviter->GetFD(), &writeSet);
		
		// This connection goes into FD_STATE_GAME_THISFD_MOVE
		state.SetState(FD_STATE_GAME_WAIT_THISFD_MOVE);
		state.SetRead(32/8);
		// The other connection already has our address, but we need to set the reverse
		state.SetOtherPlayer(inviter);
		// This connection should already be in the read list
	}
	else
	{
		// Answered no
		// this connection goes into FD_STATE_LOBBY, and tries to read again
		state.SetState(FD_STATE_LOBBY);
		state.SetRead(32/8);
		// (This connection should already be in the read list)
		
		// Other fd parter variable cleared
		inviter->SetOtherPlayer(nullptr);
		// Other FD goes into FD_STATE_GAME_REQ_REJECT
		inviter->SetState(FD_STATE_GAME_REQ_REJECT);
		uint32_t inviterResponse = ACTION_INVITE_RESPONSE;
		inviterResponse = inviterResponse | INVITE_RESPONSE_NO;
		inviterResponse = htonl(inviterResponse);
		inviter->SetWrite(((char *)&inviterResponse), 32/8);
		fdAddSet(inviter->GetFD(), &writeSet);
	}
}

// Should be called after successfull write in FD_STATE_GAME_REQ_REJECT
void afterWriteReject(FdState & state, fd_set & readSet, fd_set & writeSet)
{
	// Switch to reading
	FD_CLR(state.GetFD(), &writeSet);
	// Set up for a read from the lobby
	fdAddSet(state.GetFD(), &readSet);
	state.SetRead(32/8);
	state.SetState(FD_STATE_LOBBY);
}


// Should be called after successfull write in FD_STATE_GAME_REQ_ACCEPT
void afterWriteAccept(FdState & state, fd_set & readSet, fd_set & writeSet)
{
	// switch to state FD_STATE_GAME_OFD_MOVE (which is waiting for other person to move state)
	// Take this FD out of the write list, and don't at it to the read or write, because we are waiting on the other connection in the game
	FD_CLR(state.GetFD(), &writeSet);
	state.SetState(FD_STATE_GAME_WAIT_OFD_MOVE);
}

// Called after a read of a move from this FD (called after a read in state FD_STATE_GAME_THISFD_MOVE)
void thisFdMoveRead(FdState & state, fd_set & readSet, fd_set & writeSet)
{
	// Clear this FD from read list so it is in no lists
	FD_CLR(state.GetFD(), &readSet);
	FD_CLR(state.GetFD(), &writeSet);
	// Put this FD in state FD_STATE_GAME_THISFD_MOVE_RESULTS
	state.SetState(FD_STATE_GAME_WAIT_THISFD_MOVE_RESULTS);
	
	// Get the move
	short readSize;
	char * readData = state.GetRead(readSize);
	if (readSize != 32/8)
	{
		
		abortConnection(*(state.GetOtherPlayer()), readSet, writeSet);
		abortConnection(state, readSet, writeSet);
		return;
	}
	
	// Set up other FD (whose state should be FD_STATE_GAME_OFD_MOVE) to write move
	state.GetOtherPlayer()->SetWrite(readData, readSize);
	
	// Put other FD in write mode
	fdAddSet(state.GetOtherPlayer()->GetFD(), &writeSet);
	// Other FD should already be in the state FD_STATE_GAME_OFD_MOVE
}

// Called after writing results of this connections move to this connection (called after write in state FD_STATE_GAME_THISFD_MOVE_RESULTS)
void thisFdMoveResultsWrite(FdState & state, fd_set & readSet, fd_set & writeSet)
{
	// If the game was won, set up a lobby read and go to state FD_STATE_LOBBY
	if (state.GetLastMoveWin())
	{
		state.SetState(FD_STATE_LOBBY);
		state.SetRead(32/8);
	}
	else
	// Otherwise:
	{
		// Set this connection state to FD_STATE_GAME_OFD_MOVE
		state.SetState(FD_STATE_GAME_WAIT_OFD_MOVE);
		// Remove this connection from all lists
		FD_CLR(state.GetFD(), &readSet);
		FD_CLR(state.GetFD(), &writeSet);
		// Set pair connection to state FD_STATE_GAME_THISFD_MOVE
		state.GetOtherPlayer()->SetState(FD_STATE_GAME_WAIT_THISFD_MOVE);
		// Put the other connection in the read list
		fdAddSet(state.GetOtherPlayer()->GetFD(), &readSet);
		state.GetOtherPlayer()->SetRead(32/8);
	}
}

// After the other connection's move has been written to this connection (called after write in state FD_STATE_GAME_WAIT_OFD_MOVE)
void oFdMoveWrite(FdState & state, fd_set & readSet, fd_set & writeSet)
{
	// Put this connection in read list and make sure it's not in the write list anymore
	fdAddSet(state.GetFD(), &readSet);
	FD_CLR(state.GetFD(), &writeSet);
	// set this connection's state to FD_STATE_GAME_OFD_MOVE_RESULTS
	state.SetRead(32/8);
	state.SetState(FD_STATE_GAME_WAIT_OFD_MOVE_RESULTS);
}

// Called after a read from this connection of the results of the other FD's move (called after read from state FD_STATE_GAME_OFD_MOVE_RESULTS)
void oFdMoveResultsRead(FdState & state, fd_set & readSet, fd_set & writeSet)
{
	short readLen;
	uint32_t result;
	char * readData = state.GetRead(readLen);
	if (readLen != 32/8)
	{
		abortConnection(*(state.GetOtherPlayer()), readSet, writeSet);
		abortConnection(state, readSet, writeSet);
		return;
	}
	
	result = ntohl(*((uint32_t *)readData));
	
	// Check if that was a winning move. If so, set a flag on the other connection & remove the pair pointers, set this connection up for a lobby read
	if (result & WIN_YES)
	{
		state.GetOtherPlayer()->SetLastMoveWin();
		state.GetOtherPlayer()->SetOtherPlayer(nullptr);
		state.SetOtherPlayer(nullptr);
		state.SetRead(32/8);
	}
	else
	{
		// Set other connection to state FD_STATE_GAME_WAIT_THISFD_MOVE_RESULTS
		state.GetOtherPlayer()->SetState(FD_STATE_GAME_WAIT_THISFD_MOVE_RESULTS);
		// Put other connection in write list and set it up with the results we just read
		fdAddSet(state.GetOtherPlayer()->GetFD(), &writeSet);
		state.GetOtherPlayer()->SetWrite(readData, readLen);
		
		// Remove this connection from the read list
		FD_CLR(state.GetFD(), &readSet);
	}
}

int main(int argc, char ** argv)
{
	int sockfd = -1;
	std::string port = getPortString(argc, argv);
	
	if (port == "")
	{
		return 1;
	}
	
	std::cout << "Battleship server starting, version " << GIT_VERSION << ".\n";
	
	// Install the signal handler for SIGINT.
	struct sigaction s;
	s.sa_handler = sigint;
	sigemptyset(&s.sa_mask);
	s.sa_flags = 0;
	sigaction(SIGINT, &s, NULL);
	
	// Block SIGTERM.
	sigset_t sigset, oldset;
	sigemptyset(&sigset);
	sigaddset(&sigset, SIGINT);
	sigprocmask(SIG_BLOCK, &sigset, &oldset);
	
	// Enter the pselect() loop, using the original mask as argument.
	fd_set readSet;
	FD_ZERO(&readSet);
	fd_set writeSet;
	FD_ZERO(&writeSet);
	
	if (SetUpListing(port, readSet, sockfd))
	{
		return -1;
	}
	
	fd_set readSetSelectResults = readSet;
	fd_set writeSetSelectResults = writeSet;
	
	while (pselect(maxFd+1, &readSetSelectResults, &writeSetSelectResults, NULL, NULL, &oldset) >= 0)
	{
		// Do some processing. Note that the process will not be
		// interrupted while inside this loop.
		for (auto& it: Fds)
		{
			int thisFD = it.GetFD();
			short state = it.GetState();
			if (FD_ISSET(thisFD, &readSetSelectResults))
			{
				int readResult;
				if (thisFD != sockfd)
				{
					readResult = it.Read();
				}
				else
				{
					// Treat the accept FD as a special case and act like it's ready without trying to read it
					readResult = 1;
				}
				if (readResult == 0)
				{
					// Need to read again, do nothing
				}
				else if (readResult < 0)
				{
					// End of connection or error reading
					abortConnection(it, readSet, writeSet);
				}
				else if (readResult == 1)
				{
					// We're done reading a chunk, handle the result
					if (FD_STATE_ACCEPT_SOCK == state)
					{
						acceptConnection(thisFD, readSet);
					}
					else if (FD_STATE_ANON == state)
					{
						anonRead(it, readSet, writeSet);
					}
					else if (FD_STATE_ANON_NAME_SIZE == state)
					{
						nameRead(it, readSet, writeSet);
					}
					else if (FD_STATE_LOBBY == state)
					{
						lobbyRead(it, readSet, writeSet);
					}
					else if (FD_STATE_REQD_GAME == state)
					{
						
					}
					else if (FD_STATE_GAME_WAIT_THISFD_MOVE == state)
					{
						thisFdMoveRead(it, readSet, writeSet);
					}
					else if (FD_STATE_GAME_WAIT_THISFD_MOVE_RESULTS == state)
					{
						
					}
					else if (FD_STATE_GAME_WAIT_OFD_MOVE == state)
					{
						
					}
					else if (FD_STATE_GAME_WAIT_OFD_MOVE_RESULTS == state)
					{
						oFdMoveResultsRead(it, readSet, writeSet);
					}
					else if (FD_STATE_WAIT_QUIT_ACK == state)
					{
						
					}
					else if (FD_STATE_OPLYR_NAME_READ == state)
					{
						otherPlayerNameRead(it, readSet, writeSet);
					}
					else if (FD_STATE_GAME_INVITE_RESP_WAIT == state)
					{
						readStateGameInvite(it, readSet, writeSet);
					}
				}
			}
			if (FD_ISSET(thisFD, &writeSetSelectResults))
			{
				int writeResult = it.Write();
				if (writeResult == 0)
				{
					// Need to write again, do nothing
				}
				else if (writeResult < 0)
				{
					// End of connection or error reading
					abortConnection(it, readSet, writeSet);
				}
				else if (writeResult == 1)
				{
					// Fd ready for write
					if (FD_STATE_ANON == state)
					{
						
					}
					else if (FD_STATE_LOBBY == state)
					{
						
					}
					else if (FD_STATE_REQD_GAME == state)
					{
						
					}
					else if (FD_STATE_GAME_WAIT_THISFD_MOVE == state)
					{
						
					}
					else if (FD_STATE_GAME_WAIT_THISFD_MOVE_RESULTS == state)
					{
						thisFdMoveResultsWrite(it, readSet, writeSet);
					}
					else if (FD_STATE_GAME_WAIT_OFD_MOVE == state)
					{
						oFdMoveWrite(it, readSet, writeSet);
					}
					else if (FD_STATE_GAME_WAIT_OFD_MOVE_RESULTS == state)
					{
						
					}
					else if (FD_STATE_WAIT_QUIT_ACK == state)
					{
						
					}
					else if (FD_STATE_NAME_REJECT == state)
					{
						nameRejectAfterWrite(it, readSet, writeSet);
					}
					else if (FD_STATE_NAME_ACCEPT == state)
					{
						nameAcceptAfterWrite(it, readSet, writeSet);
					}
					else if (FD_STATE_GAME_INVITE == state)
					{
						writeGameInvite(it, readSet, writeSet);
					}
					else if (FD_STATE_GAME_REQ_ACCEPT == state)
					{
						afterWriteAccept(it, readSet, writeSet);
					}
					else if (FD_STATE_GAME_REQ_REJECT == state)
					{
						afterWriteReject(it, readSet, writeSet);
					}
				}
			}
		}
		// Retore the lists of things we want to check
		readSetSelectResults = readSet;
		writeSetSelectResults = writeSet;
	}
}