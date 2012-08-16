#include "IRC_Server.h"

#include <iostream>

IRC_Server::IRC_Server()
: Server(6667, Server::Dual_IPv4_IPv6_Server, true)
{}

IRC_Server::~IRC_Server()
{
	for (std::set<User*>::iterator it = users.begin();it != users.end();it++)
		delete *it;
	
	for (std::set<Channel*>::iterator it = channels.begin();it != channels.end();it++)
		delete *it;
}

void IRC_Server::on_client_connect(int &client_sock)
{
	User* new_user = new User;
	new_user->nick.push_back('\1');
	new_user->username.push_back('\1');
	new_user->socket = client_sock;
	
	users.insert(new_user);
}

void IRC_Server::on_client_disconnect(int &client_socket)
{
	for (std::set<User*>::iterator it = users.begin();it != users.end();it++)
	{
		if ((*it)->socket == client_socket)
		{
			delete *it;
			users.erase(it);
			break;
		}
	}
}

void IRC_Server::recieve_message(std::string &message, int &client_sock)
{
	using namespace std;
	
	if (verbose)
		cout << "Recieved message: '" << message << "'";
	
	if (message.find("\r\n") == string::npos)
	{
		if (verbose)
			cout << " which is invalid." << endl;
		
		return;
	}
	else if (verbose)
		cout << endl;
	
	parse_message(message);
}

void IRC_Server::send_message(std::string &message, User* user)
{
	if (user == NULL)
		return;
	
	write(user->socket, message.c_str(), message.size());
}

void IRC_Server::broadcast_message(std::string &message, std::set<User*> users)
{
	for (std::set<User*>::iterator it = users.begin();it != users.end();it++)
		send_message(message, *it);
}

void IRC_Server::broadcast_message(std::string &message, Channel* users)
{
	broadcast_message(message, users->users);
}

void IRC_Server::broadcast_message(std::string &message, std::set<Channel*> channels)
{
	for (std::set<Channel*>::iterator it = channels.begin();it != channels.end();it++)
		broadcast_message(message, (*it));
}

void IRC_Server::parse_message(std::string &message)
{
	
}
