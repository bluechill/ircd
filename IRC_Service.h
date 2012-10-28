#ifndef IRC_SERVICE_H
#define IRC_SERVICE_H

#include <vector>

#include "IRC_Plugin.h"
#include "IRC_Server.h"

class IRC_Service : public IRC_Plugin
{
public:
	
	IRC_Service(std::string path, IRC_Server* link);
	~IRC_Service();
	
	std::vector<Call_Type> get_supported_calls();
	
	Result_Of_Call plugin_call(Call_Type type, IRC_Server::User* user, std::vector<std::string> &parts);
	
	std::string get_name_of_plugin();
	
	enum Service_Supported_Calls
	{
		RECIEVE_ADDRESSED_MESSAGE = 0,
		RECIEVE_CHANNEL_MESSAGE = 1,
		GLOBAL_RECIEVE_MESSAGE = 2
	};
	
	enum Result_Type
	{
		NOT_HANDLED = -1,
		NO_RETURN = 0,
		NOTICE = 1,
		PRIVMSG = 2
	};
	
	struct Service_Result
	{
		std::string message;
		std::string user_or_channel;
		Result_Type type;
	};
	
	std::vector<IRC_Server::Channel*>* get_joined_channels();
	void join_channel(std::string channel);
	
	std::string get_nick();
	void set_nick(std::string nick);
	
	std::string get_realname();
	void set_real_name(std::string realname);
	
private:
	typedef Service_Result (*message_handler_function)(IRC_Server::User* user, std::vector<std::string> &parts, IRC_Server* link, IRC_Service* service);
	typedef void (*service_init)(IRC_Server* link, IRC_Service* service);
	
	message_handler_function mhf;
	
	IRC_Server* link;
	
	IRC_Server::User* user_link;
};

#endif
