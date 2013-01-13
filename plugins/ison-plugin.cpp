#include "IRC_Server.h"
#include "IRC_Plugin.h"

#include <string.h>

#include <vector>
#include <string>
#include <algorithm>

extern "C" std::vector<IRC_Plugin::Call_Type> get_supported_calls()
{
	std::vector<IRC_Plugin::Call_Type> types;
	types.push_back(IRC_Plugin::ON_RECIEVE_MESSAGE);
	
	return types;
}

extern "C" std::string name_of_plugin()
{
	return "ISON";
}

extern "C" IRC_Plugin::Result_Of_Call plugin_call(IRC_Plugin::Call_Type type, IRC_Server::User* user, std::vector<std::string> &parts, IRC_Server* link)
{
	using namespace std;
	
	if (type != IRC_Plugin::ON_RECIEVE_MESSAGE)
		return IRC_Plugin::NOT_HANDLED;
	
	if (strncasecmp(parts[0].c_str(), "ISON", 4) != 0)
		return IRC_Plugin::NOT_HANDLED;
	
	if (parts.size() < 2)
	{
		link->send_error_message(user, IRC_Server::ERR_NEEDMOREPARAMS, "ISON");
		
		return IRC_Plugin::HANDLED;
	}
	
	link->lock_message_mutex();
	
	string result = ":";
	result += link->get_hostname();
	result += " 303 ";
	result += user->nick;
	result += " :";
		
	vector<IRC_Server::User*>* users = link->get_users();
	
	for (int i = 1;i < parts.size();i++)
	{
		string one_user = parts[i];
		
		for (int z = 0;z < users->size();z++)
		{
			string nickname = users->at(z)->nick;
			
			if (nickname == one_user)
			{
				string push_back = "";
				
				if (i != 1)
					push_back += " ";
				
				push_back += nickname;
				
				result += push_back;
			}
		}
	}
	
	result += IRC_Server::irc_ending;
	
	link->send_message(result, user);
	
	link->unlock_message_mutex();
	
	return IRC_Plugin::HANDLED;
}
