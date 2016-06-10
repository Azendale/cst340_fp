#include <iostream>
#include "Game.h"

extern "C"
{
	#include <getopt.h>
	#include <netdb.h>
	#include <sys/types.h>
	#include <netinet/in.h>
	#include <sys/socket.h>
	#include <errno.h>
	#include <string.h>
	#include <unistd.h>
	#include <netinet/in.h>
	#include <netinet/tcp.h>
}
// Contains an easy to use representation of the command line args
typedef struct
{
	char * port;
	char * address;
} program_options;

/****************************************************************
 * Set up our struct -- note that I expect this to point to argv memory,
 * so no destructor needed
 * 
 * Preconditions:
 *  options is a pointer to a block of memory at least
 *   sizeof(program_options) in size
 *
 * Postcondition:
 *      options intialized to defaults
 * 
 ****************************************************************/
void Init_program_options(program_options * options)
{
	options->port = NULL;
	options->address = NULL;
}

/****************************************************************
 * Parse command line args into the already allocatated program_options struct
 * 'options'
 * 
 * Preconditions:
 *  options is an intialized program_options struct
 * 
 * Postcondition:
 *      options populated with settings from command line
 ****************************************************************/
void parseOptions(int argc, char ** argv, program_options & options)
{
	int portNum = 0;
	int arg;
	while (-1 != (arg = getopt(argc, argv, "s:i:p:")))
	{
		if ('p' == arg)
		{
			portNum = atoi(optarg);
			if (portNum < 1 || portNum > 65535)
			{
				fprintf(stderr, "Invalid port number given.\n");
				exit(1);
			}
			options.port = optarg;
		}
		// Treat -s and -i the same since getaddrinfo can handle them both
		else if ('s' == arg || 'i' == arg)
		{
			options.address = optarg;
		}
	}
	if (NULL == (options.address))
	{
		fprintf(stderr, "You must set either a hostname or address with -s or"
		" -i.\n");
		exit(2);
	}
	if (NULL == (options.port))
	{
		fprintf(stderr, "You must set a port number with the -p option.\n");
		exit(4);
	}
}

int connectToServer(program_options & options)
{
	int sockfd = -1;
	// For critera for lookup
	struct addrinfo hints;
	struct addrinfo * destInfoResults;
	
	// Initialize the struct
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC; // Use IPv4 or IPv6, we don't care
	hints.ai_socktype = SOCK_STREAM; // Use tcp
	
	// Do the lookup
	int returnStatus = -1;
	if (0 != (returnStatus = getaddrinfo(options.address, options.port, &hints,
		&destInfoResults)) )
	{
		fprintf(stderr, "Couldn't get address lookup info: %s\n",
				gai_strerror(returnStatus));
		exit(8);
	}
	
	// Loop through results until one works
	bool connectSuccess = false;
	struct addrinfo * p = destInfoResults;
	for (; (!connectSuccess) && NULL != p; p = p->ai_next)
	{
		if (-1 != (sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)))
		{
			if (-1 != connect(sockfd, p->ai_addr, p->ai_addrlen))
			{
				connectSuccess = true;
			}
			else
			{
				close(sockfd);
				perror("Trouble connecting: ");
			}
		}
		else
		{
			perror("Trouble getting a socket: ");
		}
	}
	freeaddrinfo(destInfoResults);
	return sockfd;
}

int main(int argc, char ** argv)
{
	program_options options;
	Init_program_options(&options);
	parseOptions(argc, argv, options);
	int connection = connectToServer(options);
	
	Game game;
	
	
return 0;
}
