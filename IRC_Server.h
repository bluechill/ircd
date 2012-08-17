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
		std::vector<char> modes;
		
		std::vector<User*> users;
	};
	
	struct User {
		std::string nick;
		std::string username;
		std::string realname;
		std::string hostname;
		
		int socket;
		
		std::vector<char> modes;
		
		std::vector<Channel*> channels;
	};
	
	IRC_Server();
	~IRC_Server();
	
	void on_client_connect(int &client_sock);
	void on_client_disconnect(int &client_socket);
	
	void recieve_message(std::string &message, int &client_sock);
	
	void send_message(std::string &message, User* user);
	void broadcast_message(std::string &message, std::vector<User*> users);
	
	void broadcast_message(std::string &message, Channel* users);
	void broadcast_message(std::string &message, std::vector<Channel*> channels);
	
	static const std::string irc_ending;
	
private:
	enum Message_Type
	{
		NICK = 0,
		USER,
		PONG,
		JOIN,
		PART,
		PRIVMSG,
		LIST,
		QUIT,
		UNKNOWN
	};
	
	static Message_Type string_to_message_type(std::string &message);
	
	User* sock_to_user(int &sock);
	Channel* channel_name_to_channel(std::string &channel);
	
	std::vector<User*> users;
	std::vector<Channel*> channels;
	
	void parse_message(std::string &message, int &client_sock);
};

#endif
