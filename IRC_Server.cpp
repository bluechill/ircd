#include "IRC_Server.h"

#include <iostream>

IRC_Server::IRC_Server()
: Server(6667, Server::Dual_IPv4_IPv6_Server, true)
{}

IRC_Server::~IRC_Server()
{}

void IRC_Server::recieve_message(std::string message, int client_sock)
{
	std::cout << "Recieved message: '" << message << "'" << std::endl;
}
