#include <iostream>
#include <vector>
#include <string>

extern "C"
{
	#include <ctype.h>
	#include <unistd.h>
	#include <stdlib.h>
}

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

int main(int argc, char ** argv)
{
	std::string port = getPortString(argc, argv);
	if (port == "")
	{
		return 1;
	}
	
	
}