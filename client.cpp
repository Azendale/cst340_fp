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

std::string getUsername(std::string prompt)
{
	std::string returnVal;
	while (returnVal.length() < 1 || returnVal.length() >= MAX_NAME_LEN)
	{
		std::cout << prompt << " Input must be less than " << MAX_NAME_LEN << " characters long: ";
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

bool signEQunsign(int sign, unsigned int usign)
{
	if (sign < 0)
	{
		return false;
	}
	return ((unsigned int)sign) == usign;
}

std::string readLongString(int fd)
{
	int ReadCount = -1;
	uint32_t len;
	ReadCount = readBytes(fd, ((char *)&len), sizeof(uint32_t));
	if (ReadCount != sizeof(uint32_t))
	{
		return std::string("");
	}
	len = ntohl(len);
	len = len & LONG_TRANSFER_SIZE_MASK;
	if (len < 1)
	{
		return std::string("");
	}
	char * data = new char[len];
	if (!data)
	{
		return std::string("");
	}
	ReadCount = readBytes(fd, data, len);
	if (!signEQunsign(ReadCount, len))
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
	ReadCount = readBytes(fd, ((char *)&len), sizeof(uint32_t));
	if (ReadCount != sizeof(uint32_t))
	{
		return std::string("");
	}
	len = ntohl(len);
	len = len & TRANSFER_SIZE_MASK;
	if (len < 1)
	{
		return std::string("");
	}
	char * data = new char[len];
	if (!data)
	{
		return std::string("");
	}
	ReadCount = readBytes(fd, data, len);
	if (!signEQunsign(ReadCount, len))
	{
		return std::string("");
	}
	std::string returnVal(data, len);
	delete [] data;
	return returnVal;
}

int writeData(int fd, const char * buf, int bytes)
{
	int writtenThisTime = 0;
	int writtenTotal = 0;
	while (writtenTotal < bytes)
	{
		writtenThisTime = write(fd, buf+writtenTotal, bytes-writtenTotal);
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
	uint32_t len = str.length();
	action = action | htonl(len);
	if (sizeof(uint32_t) != writeData(fd, ((char *)&action), sizeof(uint32_t)))
	{
		return -1;
	}
	if (!signEQunsign(writeData(fd, str.c_str(), len), len))
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
	
	bool nameAccepted = false;
	while (!nameAccepted)
	{
		std::string username = getUsername("Enter the username you want to use.");
		if (0 > writeString(connection, username, ACTION_NAME_REQUEST))
		{
			return -1;
		}
		uint32_t response;
		if (sizeof(uint32_t) != readBytes(connection, ((char *)&response), sizeof(uint32_t)))
		{
			return -2;
		}
		response = ntohl(response);
		if (response & ACTION_NAME_IS_YOURS)
		{
			nameAccepted = true;
		}
		else if (response & ACTION_NAME_TAKEN)
		{
			nameAccepted = false;
		}
		else
		{
			// Unexpected response
			return -3;
		}
	}
	
	bool quit = false;
	while (!quit)
	{
		// Need to get list of other players
		uint32_t listReq = ACTION_REQ_PLAYERS_LIST;
		if (sizeof(uint32_t) != writeData(connection, (char *)&listReq, sizeof(uint32_t)))
		{
			quit = true;
			break;
		}
		std::cout << "Players list: \n";
		std::cout << readLongString(connection) << "\nPick a player to play, or wait to allow someone else to invite you:" << std::endl;
		fd_set readSet;
		FD_ZERO(&readSet);
		// Listen for user input or Network input
		FD_SET(0, &readSet);
		FD_SET(connection, &readSet);
		
		uint32_t request;
		short reqPtr = 0;
		char otherUser[MAX_NAME_LEN];
		short otherUserPtr = 0;
		memset(otherUser, 0, MAX_NAME_LEN);
		
		int continueRead = 1;
		
		while (select(10, &readSet, NULL, NULL, NULL) >= 0 && 1 == continueRead)
		{
			if (FD_ISSET(connection, &readSet))
			{
				// connection ready
				char letter;
				if (1 != read(0, &letter, 1))
				{
					continueRead = -3;
				}
				else
				{
					*(((char *)&request)+reqPtr) = letter;
					++reqPtr;
					if (reqPtr == sizeof(uint32_t))
					{
						continueRead = 2;
					}
				}
			}
			else if (FD_ISSET(0, &readSet))
			{
				// Stdin ready
				char letter;
				if (1 != read(0, &letter, 1))
				{
					continueRead = -1;
				}
				if ('\n' == letter)
				{
					continueRead = 2;
				}
				else
				{
					otherUser[otherUserPtr] = letter;
					++otherUserPtr;
					if (otherUserPtr == MAX_NAME_LEN)
					{
						continueRead = -2;
					}
				}
			}
			FD_ZERO(&readSet);
			FD_SET(0, &readSet);
			FD_SET(connection, &readSet);
		}
		// Need to select whether we read from the console or from the connection to see if we get asked to play or if we ask to play
		if (continueRead == 2)
		{
			// We picked a player
		}
		else if (continueRead == 3)
		{
			// Another player picked us
		}
		Game game;
	
	}
	
	// Close connection
	close(connection);
	return 0;
}
