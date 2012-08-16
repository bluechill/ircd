#ifndef IRC_SERVER_H
#define IRC_SERVER_H 1

#include "Server.h"

#include <string>
#include <set>

class IRC_Server : public Server
{
public:
	struct User;
	
	struct Channel {
		std::string name;
		std::set<char> modes;
		
		std::set<User*> users;
	};
	
	struct User {
		std::string nick;
		std::string username;
		
		int socket;
		
		std::set<char> modes;
		
		std::set<Channel*> channels;
	};
	
	IRC_Server();
	~IRC_Server();
	
	void on_client_connect(int &client_sock);
	void on_client_disconnect(int &client_socket);
	
	void recieve_message(std::string &message, int &client_sock);
	
	void send_message(std::string &message, User* user);
	void broadcast_message(std::string &message, std::set<User*> users);
	
	void broadcast_message(std::string &message, Channel* users);
	void broadcast_message(std::string &message, std::set<Channel*> channels);
	
private:
	std::set<User*> users;
	std::set<Channel*> channels;
	
	void parse_message(std::string &message);
};

#endif
