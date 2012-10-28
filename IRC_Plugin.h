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
		HANDLED = 1,
		PARSED = 2
	};
	
	IRC_Plugin(std::string path, IRC_Server* link, bool isService);
	~IRC_Plugin();
	
	virtual std::vector<Call_Type> get_supported_calls();
	
	virtual Result_Of_Call plugin_call(Call_Type type, IRC_Server::User* user, std::vector<std::string> &parts);
	
	virtual std::string get_name_of_plugin(bool absolute = false);
	
private:
	std::vector<Call_Type> supported_calls;
	
	typedef std::vector<Call_Type> (*supported_calls_function)();
	typedef Result_Of_Call (*plugin_call_function)(Call_Type type, IRC_Server::User* user, std::vector<std::string> &parts, IRC_Server* link);
	typedef std::string (*name_of_plugin_function)();
	typedef void (*init_function)(IRC_Server* link);
	typedef void (*dealloc_function)();
	
	IRC_Server* link;
	
protected:
	supported_calls_function scf;
	plugin_call_function pcf;
	name_of_plugin_function nop;
	init_function init;
	dealloc_function dealloc;
};

#endif
