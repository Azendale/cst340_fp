/************************************
 * Author: Erik Andersen
 * Lab: CST340 Final Lab
 * 
 * 
 * Implements a battleship client. -p sets the port to connect to. -s sets the
 * server to connect to.
 * Input of coordinates are 1 based.
 ************************************/
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

/****************************************************************
 * Set up the connection to the server
 * 
 * Preconditions:
 *  options is an intialized program_options struct with the correct settings
 * 
 * Postcondition:
 *  connection opened and FD returned. May exit on failure
 ****************************************************************/
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

/****************************************************************
 * Read data from 'fd' into 'buff' of size 'toRead'
 * 
 * Preconditions:
 *  fd is an open file descriptor. buff is a buffer of at least 'toRead' bytes
 * Postcondition:
 *  returns number of read bytes, with read data written to buff
 * 
 ****************************************************************/
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

/****************************************************************
 * Checks if a signed and unsigned number are the same
 * 
 * Preconditions:
 *  None
 * Postcondition:
 *  returns true if the numbers are equivalent
 ****************************************************************/
bool signEQunsign(int sign, unsigned int usign)
{
	if (sign < 0)
	{
		return false;
	}
	return ((unsigned int)sign) == usign;
}

/****************************************************************
 * Reads a length and then string from a connection, and returns the resulting
 *  string. Handles strings up to LONG_TRANSFER_SIZE_MASK bytes
 * 
 * Preconditions:
 *  fd is open and readable
 * Postcondition:
 *  string that was read returned
 ****************************************************************/
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

/****************************************************************
 * Reads a length and then string from a connection, and returns the resulting
 *  string. Handles strings up to TRANSFER_SIZE_MASK bytes
 * 
 * Preconditions:
 *  fd is open and readable
 * Postcondition:
 *  string that was read returned
 ****************************************************************/
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

/****************************************************************
 * Writes data from 'buf' to fd, trying to transfer 'bytes' bytes
 * 
 * Preconditions:
 *  buff a valid pointer to at least 'bytes' bytes of memory. fd an open and
 *  writable fd
 * Postcondition:
 *  number of sucessfully written bytes returned, or -1 for an error
 ****************************************************************/
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

/****************************************************************
 * serialize a string to fd, with a header of action type 'action'
 * 
 * Preconditions:
 *  fd open and writable. Action a valid action as #defined by netDefines.h
 * Postcondition:
 *  Action, string length, and string itself written to connection or -1 returned
 *  for an error
 ****************************************************************/
int writeString(int fd, const std::string & str, uint32_t action)
{
	uint32_t len = str.length();
	action = action | len;
	action = htonl(action);
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

/****************************************************************
 * Request the x and y coordinates to fire at from the user
 * 
 * Preconditions:
 *  Game being played and it's the players turn
 * Postcondition:
 *  x and y coordinates stored in 'x' and  'y'
 ****************************************************************/
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

/****************************************************************
 * Encode the cordinates of a move
 * 
 * Preconditions:
 *  x and y <= MAP_SIDE_SIZE
 * Postcondition:
 *  encoded move returned
 ****************************************************************/
uint32_t encodeMove(short x, short y)
{
	uint32_t ourMove = ACTION_MOVE;
	ourMove = ourMove | (MOVE_X_COORD_MASK_UNSHIFTED&(x<<MOVE_X_COORD_SHIFT));
	ourMove = ourMove | (MOVE_Y_COORD_MASK_UNSHIFTED&(y<<MOVE_Y_COORD_SHIFT));
	ourMove = htonl(ourMove);
	return ourMove;
}

/****************************************************************
 * Decode a move's coordinates
 * 
 * Preconditions:
 *  response a valid move message in network order
 * Postcondition:
 *  x and y updated with parsed coordinates
 ****************************************************************/
void decodeMove(uint32_t response, short & x, short & y)
{
	response = ntohl(response);
	x = (response & MOVE_X_COORD_MASK_UNSHIFTED)>>MOVE_X_COORD_SHIFT;
	y = (response & MOVE_Y_COORD_MASK_UNSHIFTED)>>MOVE_Y_COORD_SHIFT;
}

/****************************************************************
 * Encode the results of a move
 * 
 * Preconditions:
 *  x and y <= MAP_SIDE_SIZE, shipSize <=5, if win == true; hit == true
 * Postcondition:
 *  encoded result of move returned, already converted to network order
 ****************************************************************/
uint32_t encodeMoveResults(short x, short y, bool hit, short shipSize, bool sink, bool win)
{
	uint32_t response = ACTION_MOVE_RESULTS;
	
	response = response | ((x<<MOVE_X_COORD_SHIFT) & MOVE_X_COORD_MASK_UNSHIFTED);
	response = response | ((y<<MOVE_Y_COORD_SHIFT) & MOVE_Y_COORD_MASK_UNSHIFTED);
	if (hit)
	{
		response = response | MOVE_HIT_SHIP_MASK;
		response = response | ((shipSize<<MOVE_SIZE_OF_HIT_SHIP_SHIFT)&MOVE_SIZE_OF_HIT_SHIP_MASK_UNSHIFTED);
	}
	if (sink)
	{
		response = response | MOVE_SINK_SHIP_MASK;
	}
	if (win)
	{
		response = response | WIN_BIT_MASK;
	}
	
	response = htonl(response);
	return response;
}

/****************************************************************
 * Decode the results of a move
 * 
 * Preconditions:
 *  response a valid network order encoded move results message
 * Postcondition:
 *  hit, shipSize, sink, and win all updated from the encoded values in response
 ****************************************************************/
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

/****************************************************************
 * Output the results of a move
 * 
 * Preconditions:
 *  if win == true, then sink == true. If sink == true; hit == true. If 
 *  hit==true, then shipSize set
 * Postcondition:
 *  Results of the move outputted to stdout 
 ****************************************************************/
void outputMoveResults(bool ourMove, bool hit, short shipSize, bool sink, bool win)
{
	if (ourMove)
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
	else
	{
		if (win)
		{
			std::cout << "The other player's last move just won. (Sorry, maybe next time, right?)\n";
		}
		if (sink)
		{
			std::cout << "The other player's move sunk your " << shipSize << "-space ship.\n";
		}
		else if (hit)
		{
			std::cout << "The other player's move hit your " << shipSize << "-space ship.\n";
		}
		else
		{
			std::cout << "The other player's move hit open ocean.\n";
		}
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
		listReq = htonl(listReq);
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
		
		while (1 == continueRead && select(10, &readSet, NULL, NULL, NULL) >= 0)
		{
			if (FD_ISSET(connection, &readSet))
			{
				// connection ready
				int readThisTime = read(connection, ((char *)&serverRequest)+reqPtr, sizeof(uint32_t)-reqPtr);
				if (readThisTime <= 0)
				{
					continueRead = -3;
				}
				else
				{
					reqPtr += readThisTime;
					if (reqPtr == sizeof(uint32_t))
					{
						serverRequest = ntohl(serverRequest);
						continueRead = 3;
					}
				}
			}
			else if (FD_ISSET(0, &readSet))
			{
				// Stdin ready
				int readThisTime = read(0, otherUser+otherUserPtr, MAX_NAME_LEN-otherUserPtr);
				if (readThisTime <= 0)
				{
					continueRead = -1;
				}
				else
				{
					otherUserPtr += readThisTime;
					continueRead = 2;
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
			// Strip newline if nessesary
			if ('\n' == otherUser[otherUserPtr-1])
			{
				--otherUserPtr;
			}
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
					std::cout << "Other player declined you invitation.\n";
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
				continue;
			}
		}
		
		// Create the game
		Game game;
		// For the cordinates of the last move (both ours and theirs)
		short x, y;
		// For storing values unpacked and packed in move responses
		bool hit;
		short hitShipSize;
		bool sink;
		bool win = false;
		// Place ships
		game.PlaceShips();
		game.PrintBoard();
		
		if (continueRead == 3)
		{
			// Do our first turn
			std::cout << "Because you were invited to a game, you get the first move. \n";
			getPlayCoord(x, y);
			game.SetPlayCoord(x-1, y-1);
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
			if (hit)
			{
				game.SetPlayHitCoord(x-1, y-1);
			}
			outputMoveResults(true, hit, hitShipSize, sink, win);
			if (win)
			{
				// Go back to the lobby
				std::cout << "Returning to Lobby.\n";
				win = false;
				break;
			}
		}
		// Play the rest of the game
		while (!win && !quit)
		{
			// Loop though turns, starting with them sending us a turn
			std::cout << "Other players turn.\n";
			uint32_t theirMove;
			if (sizeof(uint32_t) != readBytes(connection, (char *)&theirMove, sizeof(uint32_t)))
			{
				quit = true;
				break;
			}
			decodeMove(theirMove, x, y);
			
			// Calculate the results of their move
			game.CalculateMoveResults(x-1, y-1, hit, hitShipSize, sink, win);
			// Output the results of their move to our player
			outputMoveResults(false, hit, hitShipSize, sink, win);
			game.PrintBoard();
			
			uint32_t theirMoveResults = encodeMoveResults(x, y, hit, hitShipSize, sink, win);
			
			if (sizeof(uint32_t) != writeData(connection, (char *)&theirMoveResults, sizeof(uint32_t)))
			{
				quit = true;
				break;
			}
			if (win) // other player won
			{
				win = false;
				std::cout << "Returning to Lobby.\n";
				break; // go to the lobby
			}
			win = false;
			
			std::cout << "Our turn.\n";
			getPlayCoord(x, y);
			game.SetPlayCoord(x-1, y-1);
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
			if (hit)
			{
				game.SetPlayHitCoord(x-1, y-1);
			}
			outputMoveResults(true, hit, hitShipSize, sink, win);
			if (win)
			{
				win = false;
				// Go back to the lobby
				break;
			}
		}
	}
	
	// Close connection
	close(connection);
	return 0;
}
