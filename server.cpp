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
		}
	}
	else
	{
		Fds.push_back(FdState(acceptfd, FD_STATE_ANON));
		
	}
	return 0;
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
		for (const auto& it: Fds)
		{
			int thisFD = it.GetFD();
			short state = it.GetState();
			if (FD_ISSET(thisFD, &readSet))
			{
				// Fd ready for read
				if (FD_STATE_ACCEPT_SOCK == state)
				{
					
				}
				else if (FD_STATE_ANON == state)
				{
					
				}
				else if (FD_STATE_NAME_REQUESTED == state)
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
				FD_CLR(thisFD, &readSet);
			}
			if (FD_ISSET(thisFD, &writeSet))
			{
				// Fd ready for write
				FD_CLR(thisFD, &writeSet);
			}
		}
	}
}