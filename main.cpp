#include "IRC_Server.h"
#include <iostream>
#include <vector>

int main(int argc, char* argv[])
{
	using namespace std;
	
	vector<string> arguments;
	for (int i = 0;i < argc;i++)
		arguments.push_back(argv[i]);
	
	IRC_Server server(arguments);
	
	sleep(-1);
	
	return 0;
}