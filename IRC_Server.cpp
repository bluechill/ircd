#include "IRC_Server.h"

#include <iostream>
#include <vector>
#include <string>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

const std::string IRC_Server::irc_ending = "\r\n";

IRC_Server::IRC_Server()
: Server(6667, Server::Dual_IPv4_IPv6_Server, true)
{}

IRC_Server::~IRC_Server()
{
	for (std::vector<User*>::iterator it = users.begin();it != users.end();it++)
		delete *it;
	
	for (std::vector<Channel*>::iterator it = channels.begin();it != channels.end();it++)
		delete *it;
}

void IRC_Server::on_client_connect(int &client_sock)
{
	User* new_user = new User;
	new_user->nick.push_back('\1');
	new_user->username.push_back('\1');
	new_user->realname.push_back('\1');
	new_user->socket = client_sock;
	
	struct sockaddr_storage addr;
	socklen_t length = sizeof(addr);
	
	getpeername(client_sock, (struct sockaddr*)&addr, &length);
	
	char ipstr[INET6_ADDRSTRLEN];
	
	if (addr.ss_family == AF_INET)
	{
		struct sockaddr_in *s = (struct sockaddr_in *)&addr;
		inet_ntop(AF_INET, &s->sin_addr, ipstr, sizeof(ipstr));
	}
	else
	{
		struct sockaddr_in6 *s = (struct sockaddr_in6 *)&addr;
		inet_ntop(AF_INET6, &s->sin6_addr, ipstr, sizeof(ipstr));
	}
	
	struct addrinfo* result = NULL;
	int status = getaddrinfo(ipstr, NULL, NULL, &result);
	
	char host[512];
	
	status = getnameinfo(result->ai_addr, result->ai_addrlen, host, 512, NULL, NULL, NULL);
	
	new_user->hostname = host;
	
	users.push_back(new_user);
}

void IRC_Server::on_client_disconnect(int &client_socket)
{
	for (std::vector<User*>::iterator it = users.begin();it != users.end();it++)
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
	
	if (message.find("\n") == string::npos)
	{
		if (verbose)
			cout << " which is invalid." << endl;
		
		return;
	}
	else if (verbose)
		cout << endl;
	
	parse_message(message, client_sock);
}

void IRC_Server::send_message(std::string &message, User* user)
{
	if (user == NULL)
		return;
	
	write(user->socket, message.c_str(), message.size());
}

void IRC_Server::broadcast_message(std::string &message, std::vector<User*> users)
{
	for (std::vector<User*>::iterator it = users.begin();it != users.end();it++)
		send_message(message, *it);
}

void IRC_Server::broadcast_message(std::string &message, Channel* users)
{
	broadcast_message(message, users->users);
}

void IRC_Server::broadcast_message(std::string &message, std::vector<Channel*> channels)
{
	for (std::vector<Channel*>::iterator it = channels.begin();it != channels.end();it++)
		broadcast_message(message, (*it));
}

IRC_Server::Message_Type IRC_Server::string_to_message_type(std::string &message)
{
	if (message.size() == 4)
	{
		if (strncasecmp(message.c_str(), "NICK", message.size()) == 0)
			return NICK;
		else if (strncasecmp(message.c_str(), "USER", message.size()) == 0)
			return USER;
		else if (strncasecmp(message.c_str(), "PONG", message.size()) == 0)
			return PONG;
		else if (strncasecmp(message.c_str(), "JOIN", message.size()) == 0)
			return JOIN;
		else if (strncasecmp(message.c_str(), "PART", message.size()) == 0)
			return PART;
		else if (strncasecmp(message.c_str(), "LIST", message.size()) == 0)
			return LIST;
		else if (strncasecmp(message.c_str(), "QUIT", message.size()) == 0)
			return QUIT;
	}
	else if (message.size() == 7)
	{
		if (strncasecmp(message.c_str(), "PRIVMSG", message.size()) == 0)
			return PRIVMSG;
	}
	
	return UNKNOWN;
}

IRC_Server::User* IRC_Server::sock_to_user(int &sock)
{
	for (std::vector<User*>::iterator it = users.begin();it != users.end();it++)
	{
		if ((*it)->socket == sock)
			return *it;
	}
	
	return NULL;
}

IRC_Server::Channel* IRC_Server::channel_name_to_channel(std::string &channel)
{
	for (std::vector<Channel*>::iterator it = channels.begin();it != channels.end();it++)
	{
		if ((*it)->name == channel)
			return *it;
	}
	
	return NULL;
}

void IRC_Server::parse_message(std::string &message, int &client_sock)
{
	using namespace std;
	
	message.erase(message.size()-2, 2);
	
	vector<string> parts;
	
	for (int i = 0;i < message.size();)
	{
		int next = message.find(' ', i);
		
		parts.push_back(message.substr(i, next-i));
		
		if (next == string::npos)
			break;
		
		i = ++next;
	}
	
	if (parts.size() == 0)
		return;
	
	for (vector<string>::iterator it = parts.begin();it != parts.end();it++)
	{
		if (it->at(0) == ':')
		{
			string message;
			
			it->erase(0,1);
			
			for (vector<string>::iterator jt = it;jt != parts.end();jt++)
				message += *jt;
			
			parts.erase(it, parts.end());
			parts.push_back(message);
			
			break;
		}
	}
	
	Message_Type type = string_to_message_type(parts[0]);
	
	User* user = sock_to_user(client_sock);
	
	if (user == NULL)
	{
		cerr << "Invalid user socket for server! : '" << message << "'" << endl;
		return;
	}
	
	switch (type)
	{
		case NICK:
		{
			if (parts.size() != 2)
				return;
			
			string old_nick = user->nick;
			user->nick = parts[1];
			
			if (user->username[0] != '\1')
			{
			
				string result = ":";
				result += old_nick;
				result += "!";
				result += user->username;
				result += "@";
				result += user->hostname;
				
				result += " NICK :";
				
				result += user->nick;
				
				result += irc_ending;
			
				broadcast_message(result, user->channels);
			}
			
			return;
		}
		case USER:
		{
			if (parts.size() != 5 || user->username[0] != '\1')
				return;
			
			user->username = parts[1];
			user->realname = parts[4];
			
			return;
		}
		case PONG:
		{
			
			return;
		}
		case JOIN:
		{
			if (parts.size() != 2 && parts.size() != 3)
				return;
			
			string result = ":";
			result += user->nick;
			result += "!";
			result += user->username;
			result += "@";
			result += user->hostname;

			result += " JOIN :";
			
			
			string channels_string = parts[1];
			for (int z = 0;z < channels_string.size();)
			{
				int next = channels_string.find(",", z);
				
				string channel = channels_string.substr(z, next);
				z = next+1;
				
				bool found = false;
				
				string message = result;
				message += channel;
				message += irc_ending;
				
				for (vector<Channel*>::iterator it = channels.begin();it != channels.end();it++)
				{
					if ((*it)->name == channel)
					{
						found = true;
						
						user->channels.push_back(*it);
						(*it)->users.push_back(user);
						
						broadcast_message(message, *it);
						break;
					}
				}
				
				if (!found)
				{
					Channel* new_channel = new Channel;
					new_channel->name = channel;
					new_channel->users.push_back(user);
					
					channels.push_back(new_channel);
					user->channels.push_back(new_channel);
					
					broadcast_message(message, new_channel);
				}
				
				if (next == string::npos)
					break;
			}
			
			return;
		}
		case PART:
		{
			
			return;
		}
		case LIST:
		{
			
			return;
		}
		case QUIT:
		{
			
			return;
		}
		case PRIVMSG:
		{
			
			return;
		}
		default:
		{
			cerr << "Invalid message: '" << message << "'" << endl;
			return;
		}
	}
}
