#include <iostream>
#include "Game.h"

extern "C"
{
#include <getopt.h>

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
void parseOptions(int argc, char ** argv, program_options * options)
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
			options->port = optarg;
		}
		// Treat -s and -i the same since getaddrinfo can handle them both
		else if ('s' == arg || 'i' == arg)
		{
			options->address = optarg;
		}
	}
	if (NULL == (options->address))
	{
		fprintf(stderr, "You must set either a hostname or address with -s or"
		" -i.\n");
		exit(2);
	}
	if (NULL == (options->port))
	{
		fprintf(stderr, "You must set a port number with the -p option.\n");
		exit(4);
	}
}

int main(int argc, char ** argv)
{
	program_options options;
	Init_program_options(&options);
	parseOptions(argc, argv, &options);
	
	Game game;
	
	
return 0;
}
