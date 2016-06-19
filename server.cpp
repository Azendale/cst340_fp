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

/****************************************************************
 * Parse the port from the command line args
 * 
 * Preconditions:
 *  User properly specified port in the argc and argv given
 * Postcondition:
 *  returns the port or service name string, or an empty string on error
 ****************************************************************/
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

/****************************************************************
 * Add a FD to a set, and update the maxFd count if needed
 * 
 * Preconditions:
 *  fd a valid fd, 'set' a valid fd_set pointer
 * Postcondition:
 *  fd added to the set, maxFd updated if this is the highest FD seen so far
 ****************************************************************/
void fdAddSet(int newFd, fd_set * set)
{
	if (newFd > maxFd)
	{
		maxFd = newFd;
	}
	FD_SET(newFd, set);
}

/****************************************************************
 * Start listening on the specified port 
 * 
 * Preconditions:
 *  No one already listening on the same port, portstring a valid service 
 *  or port number
 * Postcondition:
 *  sockfd updated with the new filedescriptor
 ****************************************************************/
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

/****************************************************************
 * Accept a new connection
 * 
 * Preconditions:
 *  sockfd ready for a read by accept()
 * Postcondition:
 *  new file descriptor state added to the Fds list
 ****************************************************************/
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
		newConnection.SetRead(sizeof(uint32_t));
		Fds.push_back(newConnection);
		fdAddSet(newConnection.GetFD(), &readSet);
	}
	return 0;
}


/****************************************************************
 * Remove a Fd from the 'container' list
 * 
 * Preconditions:
 *  fd actually in one of the FdState wrappers in container
 * Postcondition:
 *  FdState wrapper for fd removed from the container
 ****************************************************************/
int removeFd(std::vector<FdState> & container, int fd)
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

/****************************************************************
 * Do our best to clean up from a connection
 * 
 * Preconditions:
 *  Hopefully none, cleanup function
 * Postcondition:
 *  fd removed from read and write sets, other players pointing to it set to
 *  nullptr, connection shut and closed, FdState removed from Fds
 ****************************************************************/
int abortConnection(FdState & state, fd_set & readSet, fd_set & writeSet)
{
	int returnVal = 0;
	// Remove from read set
	FD_CLR(state.GetFD(), &readSet);
	// Remove it from the write set
	FD_CLR(state.GetFD(), &writeSet);
	// Remove any partner pointers to this one
	for (auto& ostate: Fds)
	{
		if (ostate.GetOtherPlayer() == &state)
		{
			ostate.SetOtherPlayer(nullptr);
		}
	}
	// Shut the connection
	if (shutdown(state.GetFD(), SHUT_RDWR))
	{
		returnVal = 1;
	}
	// Remove from FD list
	if (close(state.GetFD()))
	{
		returnVal = 2;
	}
	if (removeFd(Fds, state.GetFD()))
	{
		returnVal = 3;
	}
	return returnVal;
}

/****************************************************************
 * Read a command from a connection for a client with no name set yet
 * 
 * Preconditions:
 *  FdState in FD_STATE_ANON state
 * Postcondition:
 *  command read from the connection, and state changed
 ****************************************************************/
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

/****************************************************************
 * Find a FdState in Fds by it's username
 * 
 * Preconditions:
 *  Searched for FdState is in FD_STATE_LOBBY
 * Postcondition:
 *  nullptr returned if not found, otherwise pointer to search result
 ****************************************************************/
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

/****************************************************************
 * Generate a list of players currently in the lobby
 * 
 * Preconditions:
 *  None?
 * Postcondition:
 *  String listing the players in the lobby returned
 ****************************************************************/
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

/****************************************************************
 * Read the username for this connection from the connection
 * 
 * Preconditions:
 *  Called after finishing read in FD_STATE_ANON_NAME_SIZE
 * Postcondition:
 *  username read from the connection, and state changed
 ****************************************************************/
void nameRead(FdState & state, fd_set & readSet, fd_set & writeSet)
{
	short readLen;
	char * nameResult = state.GetRead(readLen);
	if (readLen > 0 && readLen < MAX_NAME_LEN)
	{
		std::string reqName = std::string(nameResult, readLen);
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
		response = htonl(response);
		state.SetWrite((char *)(&response), sizeof(uint32_t));
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

/****************************************************************
 * Change the state of a connection after writing a username reject
 * 
 * Preconditions:
 * In state FD_STATE_NAME_REJECT after successfully writing rejection. Switches
 *  to listing in anon state
 *
 * Postcondition:
 *  connection in FD_STATE_ANON and ready to read another command
 ****************************************************************/
void nameRejectAfterWrite(FdState & state, fd_set & readSet, fd_set & writeSet)
{
	// Switch to read
	FD_CLR(state.GetFD(), &writeSet);
	fdAddSet(state.GetFD(), &readSet);
	// State: anon
	state.SetState(FD_STATE_ANON);
	// Read size:
	state.SetRead(sizeof(uint32_t));
}

/****************************************************************
 *  Change the state of a connection after writing a username accept
 * 
 * Preconditions:
 * In state FD_STATE_NAME_ACCEPT after successfully writing response. Switches
 * to listing in lobby mode
 *
 * Postcondition:
 *  connection in FD_STATE_LOBBY and ready to read another command
 ****************************************************************/
void nameAcceptAfterWrite(FdState & state, fd_set & readSet, fd_set & writeSet)
{
	// Switch to read to listen for name listing command
	FD_CLR(state.GetFD(), &writeSet);
	fdAddSet(state.GetFD(), &readSet);
	// State: lobby
	state.SetState(FD_STATE_LOBBY);
	// Read size:
	state.SetRead(sizeof(uint32_t));
}

/****************************************************************
 * Change the state of a connection after writing a response to a name request
 * 
 * Preconditions:
 * Called after finishing write in FD_STATE_NAME_ACCEPT or FD_STATE_NAME_REJECT
 * state
 *
 * Postcondition:
 *  connection set to read 32 bits, and state updated
 ****************************************************************/
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

/****************************************************************
 * Create and serialize a list of players in the lobby
 * 
 * Preconditions:
 *
 * Postcondition:
 *  returns the string that can be written to the connection to be deserialized
 *  on the other end (no need to write a length before the string, that is
 *  already embedded in the returned string)
 ****************************************************************/
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
	std::string returnString(((char *)&len), sizeof(uint32_t));
	returnString += nameList;
	return returnString;
}

/****************************************************************
 * Handle state change after reading in the FD_STATE_LOBBY state
 * 
 * Preconditions:
 * In state FD_STATE_LOBBY, after successfully reading
 *
 * Postcondition:
 *  connection set to either read the name of a player to play or handle a
 *  request for a list of players in the lobby.
 ****************************************************************/
void lobbyRead(FdState & state, fd_set & readSet, fd_set & writeSet)
{
	// FD can request to play other player
	// FD can request player list
	short readSize;
	char * readData = state.GetRead(readSize);
	if (sizeof(uint32_t) != readSize)
	{
		// Read command that was too large
		abortConnection(state, readSet, writeSet);
		return;
	}
	
	uint32_t request = *((uint32_t *)readData);
	request = ntohl(request);
	if (ACTION_REQ_PLAYERS_LIST == (request & ACTION_MASK))
	{
		std::string list = GenerateNetNameList();
		state.SetWrite(list.c_str(), (short)(list.length()));
		// Switch to write
		fdAddSet(state.GetFD(), &writeSet);
		FD_CLR(state.GetFD(), &readSet);
		state.SetState(FD_STATE_REQ_NAME_LIST);
	}
	else if (ACTION_PLAY_PLAYERNAME == (request & ACTION_MASK))
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

/****************************************************************
 * Handle state transition from FD_STATE_OPLYR_NAME_READ
 * 
 * Preconditions:
 * called in state FD_STATE_OPLYR_NAME_READ after reading the other player's
 *  name has finished
 *
 * Postcondition:
 *  other player invited to game if they exist and are in the right state,
 *  otherwise a no answer written to connection
 ****************************************************************/
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
	if (nullptr == otherFd || state.GetName() == otherPlayer)
	{
		// No such player, or they tried to play themselves
		uint32_t response = ACTION_INVITE_RESPONSE;
		response = response | INVITE_RESPONSE_NO;
		state.SetState(FD_STATE_GAME_REQ_REJECT);
		// Switch to write
		fdAddSet(state.GetFD(), &writeSet);
		FD_CLR(state.GetFD(), &readSet);
		response = htonl(response);
		state.SetWrite((char *)&response, sizeof(uint32_t));
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
		std::string inviteandname((char *)&invitation, sizeof(uint32_t));
		inviteandname += state.GetName();
		otherFd->SetWrite(inviteandname.c_str(), (short)inviteandname.length());
		
		// remember who we asked to play (so they can find us for the response)
		state.SetOtherPlayer(otherFd);
		// Not reading or writing anymore, waiting on other player
		state.SetState(FD_STATE_REQD_GAME);
		FD_CLR(state.GetFD(), &readSet);
	}
}

/****************************************************************
 * Find the Fd that invited the one pointed to by "invitee"
 * 
 * Preconditions:
 *  invitee a valid FdState pointer that previously invited us
 * Postcondition:
 *  nullptr returned if no such thing found, otherwise pointer to the result
 ****************************************************************/
FdState * findFdByPastInvitation(FdState * invitee)
{
	FdState * returnVal = nullptr;
	for (auto& state: Fds)
	{
		if (state.GetState() == FD_STATE_REQD_GAME && state.GetOtherPlayer() == invitee)
		{
			returnVal = &state;
		}
	}
	return returnVal;
}

/****************************************************************
 * Switches state of the invited player to reading their response after we have
 * sent them the invatiation. 
 * 
 * Preconditions:
 *  called when the invited player connection finishes a write in
 *  FD_STATE_GAME_WRITE
 * Postcondition:
 *  connection switched to reading, and state set to
 *  FD_STATE_GAME_INVITE_RESP_WAIT
 ****************************************************************/
void writeGameInvite(FdState & state, fd_set & readSet, fd_set & writeSet)
{
	// Switch to reading now
	FD_CLR(state.GetFD(), &writeSet);
	fdAddSet(state.GetFD(), &readSet);
	// Switch to the state that means we are waiting for a response
	state.SetState(FD_STATE_GAME_INVITE_RESP_WAIT);
}

/****************************************************************
 * Reads the response after a connection has been invited to a game
 * 
 * Preconditions:
 *  called after successful read in state FD_STATE_GAME_INVITE_RESP_WAIT
 * Postcondition:
 *  connection set to read 32 bits, and state updated
 ****************************************************************/
void readStateGameInvite(FdState & state, fd_set & readSet, fd_set & writeSet)
{
	// Need to find out if they said yes or no
	short readSize;
	char * readData = state.GetRead(readSize);
	if (readSize != sizeof(uint32_t))
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
		inviter->SetWrite(((char *)&inviterResponse), sizeof(uint32_t));
		fdAddSet(inviter->GetFD(), &writeSet);
		
		// This connection goes into FD_STATE_GAME_THISFD_MOVE
		state.SetState(FD_STATE_GAME_WAIT_THISFD_MOVE);
		state.SetRead(sizeof(uint32_t));
		// The other connection already has our address, but we need to set the reverse
		state.SetOtherPlayer(inviter);
		// This connection should already be in the read list
	}
	else
	{
		// Answered no
		// this connection goes into FD_STATE_LOBBY, and tries to read again
		state.SetState(FD_STATE_LOBBY);
		state.SetRead(sizeof(uint32_t));
		// (This connection should already be in the read list)
		
		// Other fd parter variable cleared
		inviter->SetOtherPlayer(nullptr);
		// Other FD goes into FD_STATE_GAME_REQ_REJECT
		inviter->SetState(FD_STATE_GAME_REQ_REJECT);
		uint32_t inviterResponse = ACTION_INVITE_RESPONSE;
		inviterResponse = inviterResponse | INVITE_RESPONSE_NO;
		inviterResponse = htonl(inviterResponse);
		inviter->SetWrite(((char *)&inviterResponse), sizeof(uint32_t));
		fdAddSet(inviter->GetFD(), &writeSet);
	}
}

/****************************************************************
 * Handle state change after we have successfully notified client of rejected
 * game invitation
 * 
 * Preconditions:
 *  called after successfull write in FD_STATE_GAME_REQ_REJECT
 * Postcondition:
 *  connection switched to reading, state set to FD_STATE_LOBBY
 ****************************************************************/
void afterWriteReject(FdState & state, fd_set & readSet, fd_set & writeSet)
{
	// Switch to reading
	FD_CLR(state.GetFD(), &writeSet);
	// Set up for a read from the lobby
	fdAddSet(state.GetFD(), &readSet);
	state.SetRead(sizeof(uint32_t));
	state.SetState(FD_STATE_LOBBY);
}

/****************************************************************
 * Handle state change after we have successfully notified client of accepted
 * game invitation
 * 
 * Preconditions:
 *  Should be called after successfull write in FD_STATE_GAME_REQ_ACCEPT
 * Postcondition:
 *  connection switched to waiting for other connection in the game
 ****************************************************************/
void afterWriteAccept(FdState & state, fd_set & readSet, fd_set & writeSet)
{
	// switch to state FD_STATE_GAME_OFD_MOVE (which is waiting for other person to move state)
	// Take this FD out of the write list, and don't at it to the read or write, because we are waiting on the other connection in the game
	FD_CLR(state.GetFD(), &writeSet);
	state.SetState(FD_STATE_GAME_WAIT_OFD_MOVE);
}

/****************************************************************
 * Handle state change after a read of a move from this FD
 * 
 * Preconditions:
 *  called after a read in state FD_STATE_GAME_THISFD_MOVE
 * Postcondition:
 *  Other player's connection set up to write the move we just recieved
 ****************************************************************/
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
	if (readSize != sizeof(uint32_t))
	{
		if (state.GetOtherPlayer())
		{
			abortConnection(*(state.GetOtherPlayer()), readSet, writeSet);
		}
		abortConnection(state, readSet, writeSet);
		return;
	}
	
	if (state.GetOtherPlayer())
	{
		// Set up other FD (whose state should be FD_STATE_GAME_OFD_MOVE) to write move
		state.GetOtherPlayer()->SetWrite(readData, readSize);
		
		// Put other FD in write mode
		fdAddSet(state.GetOtherPlayer()->GetFD(), &writeSet);
		// Other FD should already be in the state FD_STATE_GAME_OFD_MOVE
	}
	else
	{
		abortConnection(state, readSet, writeSet);
	}
}

/****************************************************************
 * Handle state change after writing results of this connections move to this
 * connection
 * 
 * Preconditions:
 *  called after write in state FD_STATE_GAME_THISFD_MOVE_RESULTS
 * Postcondition:
 *  connection moved to lobby if they won, otherwise switch the other Fd to
 *  reading move from client 
 ****************************************************************/
void thisFdMoveResultsWrite(FdState & state, fd_set & readSet, fd_set & writeSet)
{
	// If the game was won, set up a lobby read and go to state FD_STATE_LOBBY
	if (state.GetLastMoveWin())
	{
		state.SetState(FD_STATE_LOBBY);
		state.SetRead(sizeof(uint32_t));
		fdAddSet(state.GetFD(), &readSet);
		FD_CLR(state.GetFD(), &writeSet);
		state.ClearLastMoveWin();
	}
	else
	// Otherwise:
	{
		// Set this connection state to FD_STATE_GAME_OFD_MOVE
		state.SetState(FD_STATE_GAME_WAIT_OFD_MOVE);
		// Remove this connection from all lists
		FD_CLR(state.GetFD(), &readSet);
		FD_CLR(state.GetFD(), &writeSet);
		if (state.GetOtherPlayer())
		{
			// Set pair connection to state FD_STATE_GAME_THISFD_MOVE
			state.GetOtherPlayer()->SetState(FD_STATE_GAME_WAIT_THISFD_MOVE);
			// Put the other connection in the read list
			fdAddSet(state.GetOtherPlayer()->GetFD(), &readSet);
			state.GetOtherPlayer()->SetRead(sizeof(uint32_t));
		}
		else
		{
			abortConnection(state, readSet, writeSet);
		}
	}
}

/****************************************************************
 * Handle state change after the other connection's move has been written to
 * this connection
 * 
 * Preconditions:
 *  called after write in state FD_STATE_GAME_WAIT_OFD_MOVE
 * Postcondition:
 *  this connection set to read 32 bits and state set to
 *  FD_STATE_GAME_WAIT_OFD_MOVE_RESULTS 
 ****************************************************************/
// 
void oFdMoveWrite(FdState & state, fd_set & readSet, fd_set & writeSet)
{
	// Put this connection in read list and make sure it's not in the write list anymore
	fdAddSet(state.GetFD(), &readSet);
	FD_CLR(state.GetFD(), &writeSet);
	// set this connection's state to FD_STATE_GAME_OFD_MOVE_RESULTS
	state.SetRead(sizeof(uint32_t));
	state.SetState(FD_STATE_GAME_WAIT_OFD_MOVE_RESULTS);
}

/****************************************************************
 * handle state change after a read from this connection of the results of the
 * other FD's move
 * 
 * Preconditions:
 *  called after read from state FD_STATE_GAME_OFD_MOVE_RESULTS
 * Postcondition:
 *  if a win happened, then this connection moved to the lobby, other
 *  connection set to write result to client
 *  otherwise, other connection set to write result to client
 ****************************************************************/
void oFdMoveResultsRead(FdState & state, fd_set & readSet, fd_set & writeSet)
{
	short readLen;
	uint32_t result;
	char * readData = state.GetRead(readLen);
	if (readLen != sizeof(uint32_t))
	{
		if (state.GetOtherPlayer())
		{
			abortConnection(*(state.GetOtherPlayer()), readSet, writeSet);
		}
		abortConnection(state, readSet, writeSet);
		return;
	}
	
	result = ntohl(*((uint32_t *)readData));
	
	if (state.GetOtherPlayer())
	{
		// Check if that was a winning move. If so, set a flag on the other connection & remove the pair pointers, set this connection up for a lobby read
		if (result & WIN_YES)
		{
			state.GetOtherPlayer()->SetLastMoveWin();
			state.GetOtherPlayer()->SetWrite(readData, readLen);
			// Set other connection to state FD_STATE_GAME_WAIT_THISFD_MOVE_RESULTS
			state.GetOtherPlayer()->SetState(FD_STATE_GAME_WAIT_THISFD_MOVE_RESULTS);
			fdAddSet(state.GetOtherPlayer()->GetFD(), &writeSet);
			
			state.SetRead(sizeof(uint32_t));
			state.SetState(FD_STATE_LOBBY);
			state.GetOtherPlayer()->SetOtherPlayer(nullptr);
			state.SetOtherPlayer(nullptr);
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
	else
	{
		abortConnection(state, readSet, writeSet);
	}
}

/****************************************************************
 * Handle state change after writing the lobby name list to the connection 
 * 
 * Preconditions:
 *  called after write finishes in FD_STATE_REQ_NAME_LIST state
 * Postcondition:
 *  connection set to lobby state and prepared for a read of 32 bits
 ****************************************************************/
void afterNameListWrite(FdState & state, fd_set & readSet, fd_set & writeSet)
{
	// Take ourself out of the write list
	FD_CLR(state.GetFD(), &writeSet);
	// Set up for a lobby read
	fdAddSet(state.GetFD(), &readSet);
	state.SetState(FD_STATE_LOBBY);
	state.SetRead(sizeof(uint32_t));
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
	
	while (pselect(maxFd+1, &readSetSelectResults, &writeSetSelectResults, NULL, NULL, NULL) >= 0)
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
					else if (FD_STATE_GAME_WAIT_THISFD_MOVE == state)
					{
						thisFdMoveRead(it, readSet, writeSet);
					}
					else if (FD_STATE_GAME_WAIT_OFD_MOVE_RESULTS == state)
					{
						oFdMoveResultsRead(it, readSet, writeSet);
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
					if (FD_STATE_GAME_WAIT_THISFD_MOVE_RESULTS == state)
					{
						thisFdMoveResultsWrite(it, readSet, writeSet);
					}
					else if (FD_STATE_GAME_WAIT_OFD_MOVE == state)
					{
						oFdMoveWrite(it, readSet, writeSet);
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
					else if (FD_STATE_REQ_NAME_LIST == state)
					{
						afterNameListWrite(it, readSet, writeSet);
					}
				}
			}
		}
		// Retore the lists of things we want to check since pselect overwrites
		// the list to tell us what is ready to read/write
		readSetSelectResults = readSet;
		writeSetSelectResults = writeSet;
	}
}