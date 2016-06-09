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

void fdAddSet(fd_set& set, int newFd, int & maxFd)
{
	if (newFd > maxFd)
	{
		maxFd = newFd;
	}
	FD_SET(newFd, &set);
}

int SetUpListing(std::string portString, int & maxFd, fd_set & readList, int & sockfd)
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
	fdAddSet(readList, sockfd, maxFd);
	
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
		FD_SET(newConnection.GetFD(), &readSet);
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
		if (state.GetName() == name)
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
		FD_SET(state.GetFD(), &writeSet);
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
	FD_SET(state.GetFD(), &readSet);
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
	FD_SET(state.GetFD(), &readSet);
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
	FD_SET(state.GetFD(), &readSet);
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
	}
	
	uint32_t request = *((uint32_t *)readData);
	request = ntohl(request);
	if ((ACTION_REQ_PLAYERS_LIST & ACTION_MASK) == request)
	{
		std::string list = GenerateNetNameList();
		state.SetWrite(list.c_str(), (short)(list.length()));
		// Switch to write
		FD_SET(state.GetFD(), &writeSet);
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

int main(int argc, char ** argv)
{
	int maxFd = 1;
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
	
	if (SetUpListing(port, maxFd, readSet, sockfd))
	{
		return -1;
	}
	
	while (pselect(1, &readSet, &writeSet, NULL, NULL, &oldset) >= 0)
	{
		// Do some processing. Note that the process will not be
		// interrupted while inside this loop.
		for (auto& it: Fds)
		{
			int thisFD = it.GetFD();
			short state = it.GetState();
			if (FD_ISSET(thisFD, &readSet))
			{
				int readResult = it.Read();
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
						
					}
					else if (FD_STATE_REQ_GAME == state)
					{
						
					}
					else if (FD_STATE_GAME_WAIT_THISFD_MOVE == state)
					{
						
					}
					else if (FD_STATE_GAME_WAIT_THISFD_MOVE_RESULTS == state)
					{
						
					}
					else if (FD_STATE_GAME_WAIT_OFD_MOVE == state)
					{
						
					}
					else if (FD_STATE_GAME_WAIT_OFD_MOVE_RESULTS == state)
					{
						
					}
					else if (FD_STATE_WAIT_QUIT_ACK == state)
					{
						
					}
				}
			}
			if (FD_ISSET(thisFD, &writeSet))
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
					else if (FD_STATE_REQ_GAME == state)
					{
						
					}
					else if (FD_STATE_GAME_WAIT_THISFD_MOVE == state)
					{
						
					}
					else if (FD_STATE_GAME_WAIT_THISFD_MOVE_RESULTS == state)
					{
						
					}
					else if (FD_STATE_GAME_WAIT_OFD_MOVE == state)
					{
						
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
				}
			}
		}
	}
}