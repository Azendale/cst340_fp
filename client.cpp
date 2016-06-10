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

void getPlayCoord(short &x, short &y)
{
	x = 0;
	y = 0;
	while (x < 1 || x > MAP_SIDE_SIZE)
	{
		std::cout << "Please enter the x coordinate you want to fire at: ";
		std::cin >> x;
	}
	
	while (y < 1 || y > MAP_SIDE_SIZE)
	{
		std::cout << "Please enter the y coordinate you want to fire at: ";
		std::cin >> y;
	}
}

uint32_t encodeMove(short x, short y)
{
	uint32_t ourMove = ACTION_MOVE;
	ourMove = ourMove | (MOVE_X_COORD_MASK_UNSHIFTED&(x<<MOVE_X_COORD_SHIFT));
	ourMove = ourMove | (MOVE_Y_COORD_MASK_UNSHIFTED&(y<<MOVE_Y_COORD_SHIFT));
	ourMove = htonl(ourMove);
	return ourMove;
}

void decodeMoveResults(uint32_t response, bool & hit, short & shipSize, bool & sink, bool & win)
{
	response = ntohl(response);
	if (response & WIN_YES)
	{
		win = true;
	}
	else
	{
		win = false;
	}
	if (response & MOVE_SINK_SHIP_MASK)
	{
		sink = true;
	}
	else
	{
		sink = false;
	}
	if (response & MOVE_HIT_SHIP_MASK)
	{
		// Set ship size
		hit = true;
		shipSize = ((response & MOVE_SIZE_OF_HIT_SHIP_MASK_UNSHIFTED)>>MOVE_SIZE_OF_HIT_SHIP_SHIFT);
	}
	else
	{
		hit = false;
	}
}

void outputMoveResults(bool hit, short shipSize, bool sink, bool win)
{
	if (win)
	{
		std::cout << "The last move was the winning move! Congratulations.\n";
	}
	if (sink)
	{
		std::cout << "The last move sunk the other player's " << shipSize << "-space ship.\n";
	}
	else if (hit)
	{
		std::cout << "The last move hit the other player's " << shipSize << "-space ship.\n";
	}
	else
	{
		std::cout << "The last move appears to have hit open ocean.\n";
	}
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
		
		// Need to select whether we read from the console or from the connection to see if we get asked to play or if we ask to play
		uint32_t serverRequest;
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
					*(((char *)&serverRequest)+reqPtr) = letter;
					++reqPtr;
					if (reqPtr == sizeof(uint32_t))
					{
						serverRequest = ntohl(serverRequest);
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
		
		if (continueRead != 2 && continueRead != 3)
		{
			quit = true;
			break;
		}
		
		if (continueRead == 2)
		{
			// We picked a player
			// Send server request
			uint32_t ourRequest = ACTION_PLAY_PLAYERNAME | (otherUserPtr & TRANSFER_SIZE_MASK);
			ourRequest = htonl(ourRequest);
			// write request type, and embed string length
			if (sizeof(uint32_t) != writeData(connection, (char *)&ourRequest, sizeof(uint32_t)))
			{
				quit = true;
				break;
			}
			// Write name string
			if (otherUserPtr != writeData(connection, otherUser, otherUserPtr))
			{
				quit = true;
				break;
			}
			
			// Get response back
			uint32_t response;
			if (sizeof(uint32_t) != readBytes(connection, (char *)&response, sizeof(uint32_t)))
			{
				quit = true;
				break;
			}
			response = ntohl(response);
			// If reject, start this loop over
			if (response & ACTION_INVITE_RESPONSE)
			{
				if (response & INVITE_RESPONSE_YES)
				{
					// Answer was yes
					// It's their turn now. We should go to the waiting for their move state after setting up our board.
				}
				else
				{
					// Answer was no
					continue; // Go back to the lobby and try again
				}
			}
			else
			{
				quit = true;
				break;
			}
		}
		else if (continueRead == 3)
		{
			// Another player picked us
			char * otherPlayerName = new char[serverRequest&TRANSFER_SIZE_MASK];
			// Get their name
			if (!signEQunsign((serverRequest&TRANSFER_SIZE_MASK), readBytes(connection, otherPlayerName, serverRequest&TRANSFER_SIZE_MASK)))
			{
				quit = true;
				break;
			}
			std::string otherPlayer(otherPlayerName, serverRequest&TRANSFER_SIZE_MASK);
			delete [] otherPlayerName;
			// Ask if the user wants to accept
			char userAnswer = 'u';
			while (userAnswer != 'Y' && userAnswer !='y' && userAnswer != 'n' && userAnswer != 'N')
			{
				std::cout << "\n\n" << otherPlayer << " has invited you to a game. Do you want to play? [y/n]\n";
				std::cin >> userAnswer;
			}
			// Send response
			if (userAnswer == 'Y' || userAnswer == 'y')
			{
				// respond with an accept
				uint32_t response = ACTION_INVITE_RESPONSE | INVITE_RESPONSE_YES;
				response = htonl(response);
				if (sizeof(uint32_t) != writeData(connection, (char *)&response, sizeof(uint32_t)))
				{
					quit = true;
					break;
				}
			}
			else
			{
				// Respond with a reject
				uint32_t response = ACTION_INVITE_RESPONSE | INVITE_RESPONSE_NO;
				response = htonl(response);
				if (sizeof(uint32_t) != writeData(connection, (char *)&response, sizeof(uint32_t)))
				{
					quit = true;
					break;
				}
				// if answer is no, start the loop over
				break;
			}
		}
		
		// Create the game
		Game game;
		// For storing values unpacked and packed in move responses
		bool hit;
		short hitShipSize;
		bool sink;
		bool win;
		// Place ships
		game.PlaceShips();
		game.PrintBoard();
		
		if (continueRead == 3)
		{
			// Do our first turn
			std::cout << "Because you were invited to a game, you get the first move. \n";
			short x, y;
			getPlayCoord(x, y);
			// Send our move
			// Encodes move, including net byte order
			uint32_t ourMove = encodeMove(x, y);
			if (sizeof(uint32_t) != writeData(connection, (char *)&ourMove, sizeof(uint32_t)))
			{
				quit = true;
				break;
			}
			// Get the results and print them
			uint32_t moveResults;
			if (sizeof(uint32_t) != readBytes(connection, (char *)&moveResults, sizeof(uint32_t)))
			{
				quit = true;
				break;
			}
			// Decodes from net order
			decodeMoveResults(moveResults, hit, hitShipSize, sink, win);
			outputMoveResults(hit, hitShipSize, sink, win);
			if (win)
			{
				// Go back to the lobby
				break;
			}
		}
		// Play the rest of the game
		bool won = false;
		while (!won && !quit)
		{
			// Loop though turns, starting with them sending us a turn
		}
	}
	
	// Close connection
	close(connection);
	return 0;
}
