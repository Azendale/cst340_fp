#include <iostream>
#include <vector>
#include <ctype.h>
#include <stdlib.h>
#include <string>

std::string getPortString(int argc, char ** argv)
{
	int arg;
	std::string portString;
	while (-1 != (arg = getopt(argc, argv, "p:")))
	{
		if ('p' == arg)
		{
			portString = optarg;
		}
	}
	if (NULL == portString)
	{
		cerr << "No port number or service name set. Please specify it"
		" with -p <port_number>.\n");
	}
	return portString;
}

int main(int argc, char ** argv)
{
	std::string port = getPortString(argc, argv);
	
}