#ifndef IRC_PLUGIN_H
#define IRC_PLUGIN_H

#include "Plugin.h"

#include <vector>

#include "IRC_Server.h"

class IRC_Plugin : public Plugin
{
public:
	enum Call_Type
	{
		ON_CLIENT_CONNECT = 0,
		ON_CLIENT_DISCONNECT,
		ON_CLIENT_REGISTERED,
		
		BEFORE_MESSAGE_PARSE,
		AFTER_MESSAGE_PARSE,
		
		ON_RECIEVE_MESSAGE
	};
	
	enum Result_Of_Call
	{
		FAILURE = -1,
		NOT_HANDLED = 0,
		HANDLED = 1
	};
	
	IRC_Plugin(std::string path, IRC_Server* link);
	~IRC_Plugin();
	
	std::vector<Call_Type> get_supported_calls();
	
	Result_Of_Call plugin_call(Call_Type type, IRC_Server::User* user, std::vector<std::string> parts);
	
private:
	std::vector<Call_Type> supported_calls;
	
	typedef std::vector<Call_Type> (*supported_calls_function)();
	typedef Result_Of_Call (*plugin_call_function)(Call_Type type, IRC_Server::User* user, std::vector<std::string> parts, IRC_Server* link);
	
	supported_calls_function scf;
	plugin_call_function pcf;
	
	IRC_Server* link;
};

#endif