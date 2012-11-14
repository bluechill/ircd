#include "IRC_Server.h"
#include "IRC_Plugin.h"

#include <string.h>

#include <vector>
#include <string>

extern "C" std::vector<IRC_Plugin::Call_Type> get_supported_calls()
{
	std::vector<IRC_Plugin::Call_Type> types;
	types.push_back(IRC_Plugin::ON_RECIEVE_MESSAGE);
	
	return types;
}

extern "C" std::string name_of_plugin()
{
	return "OPER";
}

extern "C" IRC_Plugin::Result_Of_Call plugin_call(IRC_Plugin::Call_Type type, IRC_Server::User* user, std::vector<std::string> &parts, IRC_Server* link)
{
	using namespace std;
	
	if (type != IRC_Plugin::ON_RECIEVE_MESSAGE)
		return IRC_Plugin::NOT_HANDLED;
	
	if (strncasecmp(parts[0].c_str(), "OPER", 4) != 0)
		return IRC_Plugin::NOT_HANDLED;
	
	if (parts.size() != 2 && parts.size() != 3)
	{
		if (parts.size() < 2)
			link->send_error_message(user, IRC_Server::ERR_NEEDMOREPARAMS, "OPER");
		
		return IRC_Plugin::HANDLED;
	}
	
	if (parts[1] != "bluechill" || parts[2] != "1234")
		return IRC_Plugin::HANDLED;
	
	user->modes.push_back(pair<char, string>('A', ""));
	
	return IRC_Plugin::HANDLED;
}
