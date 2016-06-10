#include <iostream>
#include "Game.h"
#include "netDefines.h"

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
				perror("Trouble connecting");
			}
		}
		else
		{
			perror("Trouble getting a socket");
		}
	}
	freeaddrinfo(destInfoResults);
	
	if (!connectSuccess)
	{
		exit(16);
	}
	return sockfd;
}

std::string getUsername()
{
	std::string returnVal;
	while (returnVal.length() < 1 || returnVal.length() >= MAX_NAME_LEN)
	{
		std::cout << "Please enter the username you would like to use. It must be less than " << MAX_NAME_LEN << " characters long: ";
		std::cin >> returnVal;
	}
	return returnVal;
}

int readBytes(int fd, char * buff, int toRead)
{
	int readSoFar = 0;
	while (readSoFar < toRead)
	{
		int readThisTime = read(fd, buff, toRead-readSoFar);
		if (readThisTime > 0)
		{
			readSoFar += readThisTime;
		}
		else
		{
			return -1;
		}
	}
	return readSoFar;
}

std::string readLongString(int fd)
{
	int ReadCount = -1;
	uint32_t len;
	ReadCount = readBytes(fd, &len, 32/8);
	if (ReadCount != 32/8)
	{
		return std::string("");
	}
	len = ntohl(*(uint32_t *)data);
	len = len & LONG_TRANSFER_SIZE_MASK;
	if (len < 1)
	{
		return std::string("");
	}
	data = new char[len];
	if (!data)
	{
		return std::string("");
	}
	ReadCount = readBytes(fd, data, len);
	if (ReadCount != len)
	{
		return std::string("");
	}
	std::string returnVal(data, len);
	delete [] data;
	return returnVal;
}

std::string readShortString(int fd)
{
	int ReadCount = -1;
	uint32_t len;
	ReadCount = readBytes(fd, &len, 32/8);
	if (ReadCount != 32/8)
	{
		return std::string("");
	}
	len = ntohl(*(uint32_t *)data);
	len = len & TRANSFER_SIZE_MASK;
	if (len < 1)
	{
		return std::string("");
	}
	data = new char[len];
	if (!data)
	{
		return std::string("");
	}
	ReadCount = readBytes(fd, data, len);
	if (ReadCount != len)
	{
		return std::string("");
	}
	std::string returnVal(data, len);
	delete [] data;
	return returnVal;
}

int writeData(fd, char * buf, int bytes)
{
	int writtenThisTime = 0;
	while (writtenTotal < bytes)
	{
		writtenThisTime = write(fd, ((char *)&action)+writtenTotal, bytes-writtenTotal);
		if (writtenThisTime <= 0)
		{
			return -1;
		}
		writtenTotal += writtenThisTime;
	}
	return writtenTotal;
}

int writeString(int fd, const std::string & str, uint32_t action)
{
	int returnVal = 0;
	uint32_t len = str.length();
	len = htonl(len);
	action = action | len;
	if (32/8 != writeData(fd, action, 32/8))
	{
		return -1;
	}
	if (len != writeData(fd, str.c_str(), len))
	{
		return -2;
	}
	return str.length() + 4;
}

int main(int argc, char ** argv)
{
	program_options options;
	Init_program_options(&options);
	parseOptions(argc, argv, options);
	int connection = connectToServer(options);
	std::string username = getUsername();
	
	bool nameAccepted = false;
	while (!nameAccepted)
	{
		if (0 > writeString(connection, username, ACTION_NAME_REQUEST))
		{
			return -1;
		}
		uint32_t response;
		if (sizeof(uint32_t) != readBytes(connection, &response, sizeof(uint32_t)))
		{
			return -2;
		}
		response = ntohl(response);
		if (response & ACTION_NAME_IS_YOURS)
		{
			nameAccepted = true;
		}
		else if (reponse & ACTION_NAME_TAKEN)
		{
			nameAccepted = false;
		}
		else
		{
			// Unexpected response
			return -3;
		}
	}
	
	
	Game game;
	
	
return 0;
}
