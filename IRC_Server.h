#ifndef IRC_SERVER_H
#define IRC_SERVER_H 1

#include "Server.h"

#include "Config.h"

class IRC_Plugin;

#include <string>
#include <set>
#include <map>

class IRC_Server : public Server
{
public:
	struct User;
	
	struct Channel {
		std::string name;
		std::vector<char> modes;
		
		std::string topic;
		
		std::vector<User*> users;
		std::map<User*, std::vector<char> > users_to_modes;
	};
	
	struct User {
		std::string nick;
		std::string username;
		std::string realname;
		std::string hostname;
		
		int socket;
		
		std::vector<char> modes;
		
		std::vector<Channel*> channels;
		
		double ping_timer;
		std::string ping_contents;
	};
	
	IRC_Server();
	~IRC_Server();
	
	void on_client_connect(int &client_sock);
	void on_client_disconnect(int &client_socket);
	
	void recieve_message(std::string &message, int &client_sock);
	
	void send_message(std::string &message, User* user);
	void broadcast_message(std::string &message, std::vector<User*> users, bool lock_message_mutex = true);
	
	void broadcast_message(std::string &message, Channel* users, bool lock_message_mutex = true);
	void broadcast_message(std::string &message, std::vector<Channel*> channels, bool lock_message_mutex = true);
	
	static const std::string irc_ending;
		
	std::string get_hostname() { return hostname; }
	
	enum Error_Type
	{
		ERR_NOSUCHNICK = 401,
		ERR_NOSUCHSERVER = 402,
		ERR_NOSUCHCHANNEL = 403,
		ERR_CANNOTSENDTOCHAN = 404,
		ERR_TOOMANYCHANNELS = 405,
		ERR_WASNOSUCHNICK = 406,
		ERR_TOOMANYTARGETS = 407,
		ERR_NOSUCHSERVICE = 408,
		ERR_NOORIGIN = 409,
		ERR_NORECIPIENT = 411,
		ERR_NOTEXTTOSEND = 412,
		ERR_NOTOPLEVEL = 413,
		ERR_WILDTOPLEVEL = 414,
		ERR_BADMASK = 415,
		ERR_UNKNOWNCOMMAND = 421,
		ERR_NOMOTD = 422,
		ERR_NOADMININFO = 423,
		ERR_FILEERROR = 424,
		ERR_NONICKNAMEGIVEN = 431,
		ERR_ERRONEUSNICKNAME = 432,
		ERR_NICKNAMEINUSE = 433,
		ERR_NICKCOLLISION = 436,
		ERR_UNAVAILRESOURCE = 437,
		ERR_USERNOTINCHANNEL = 441,
		ERR_NOTONCHANNEL = 442,
		ERR_USERONCHANNEL = 443,
		ERR_NOLOGIN = 444,
		ERR_SUMMONDISABLED = 445,
		ERR_USERSDISABLED = 446,
		ERR_NOTREGISTERED = 451,
		ERR_NEEDMOREPARAMS = 461,
		ERR_ALREADYREGISTRED = 462,
		ERR_NOPERMFORHOST = 463,
		ERR_PASSWDMISMATCH = 464,
		ERR_YOUREBANNEDCREEP = 465,
		ERR_YOUWILLBEBANNED = 466,
		ERR_KEYSET = 467,
		ERR_CHANNELISFULL = 471,
		ERR_UNKNOWNMODE = 472,
		ERR_INVITEONLYCHAN = 473,
		ERR_BANNEDFROMCHAN = 474,
		ERR_BADCHANNELKEY = 475,
		ERR_BADCHANMASK = 476,
		ERR_NOCHANMODES = 477,
		ERR_BANLISTFULL = 478,
		ERR_NOPRIVILEGES = 481,
		ERR_CHANOPPRIVSNEEDED = 482,
		ERR_CANTKILLSERVER = 483,
		ERR_RESTRICTED = 484,
		ERR_UNQOPPRIVSNEEDED = 485,
		ERR_NOOPERHOST = 491,
		ERR_UMODEUNKNOWNFLAG = 501,
		ERR_USERSDONTMATCH = 502
	};
	
	void send_error_message(User* user, Error_Type error, std::string arg1 = "", std::string arg2 = "");
	
	std::vector<User*>* get_users() { return &users; }
	std::vector<Channel*>* get_channels() { return &channels; }
	
	struct timespec get_current_time();
	
	void lock_message_mutex() { pthread_mutex_lock(&message_mutex); }
	void unlock_message_mutex() { pthread_mutex_unlock(&message_mutex); }
	
private:
	std::string hostname;
	
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
	
	Config conf;
	std::vector<IRC_Plugin*> plugins;
	
	pthread_mutex_t message_mutex;
};

#endif
