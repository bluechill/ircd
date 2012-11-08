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
	return "USER";
}

extern "C" IRC_Plugin::Result_Of_Call plugin_call(IRC_Plugin::Call_Type type, IRC_Server::User* user, std::vector<std::string> &parts, IRC_Server* link)
{
	using namespace std;
	
	if (type != IRC_Plugin::ON_RECIEVE_MESSAGE)
		return IRC_Plugin::NOT_HANDLED;
	
	if (strncasecmp(parts[0].c_str(), "USER", 4) != 0)
		return IRC_Plugin::NOT_HANDLED;
	
	if (parts.size() != 5)
	{
		if (parts.size() < 5)
			link->send_error_message(user, IRC_Server::ERR_NEEDMOREPARAMS, "USER");
		
		return IRC_Plugin::HANDLED;
	}
	
	link->lock_message_mutex();
	if (user->username.size() != 0 && user->nick.size() != 0)
	{
		link->unlock_message_mutex();
		link->send_error_message(user, IRC_Server::ERR_ALREADYREGISTRED);
		return IRC_Plugin::HANDLED;
	}
	
	if (parts[1].find_first_not_of("0123456789qwertyuiopasdfghjklzxcvbnmQWERTYUIOPASDFGHJKLZXCVBNM_-.") != string::npos)
	{
		string output = "ERROR :Closing Link: " + user->nick + "[" + user->hostname + "] (Hostile username.  Please only use 0-9 a-z A-Z _ - and . in your username.)" + IRC_Server::irc_ending;
		
		link->unlock_message_mutex();
		link->send_message(output, user);
		link->disconnect_client(user->client);
		return IRC_Plugin::HANDLED;
	}
	
	user->username = parts[1];
	user->realname = parts[4];
	
	link->unlock_message_mutex();
	
	return IRC_Plugin::HANDLED;
}
