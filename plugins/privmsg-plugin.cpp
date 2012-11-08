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
	return "PRIVMSG";
}

extern "C" IRC_Plugin::Result_Of_Call plugin_call(IRC_Plugin::Call_Type type, IRC_Server::User* user, std::vector<std::string> &parts, IRC_Server* link)
{
	using namespace std;
	
	if (type != IRC_Plugin::ON_RECIEVE_MESSAGE)
		return IRC_Plugin::NOT_HANDLED;
	
	if (strncasecmp(parts[0].c_str(), "PRIVMSG", 7) != 0)
		return IRC_Plugin::NOT_HANDLED;
	
	if (parts.size() != 3)
	{
		if (parts.size() < 3)
			link->send_error_message(user, IRC_Server::ERR_NEEDMOREPARAMS, "PRIVMSG");
		
		return IRC_Plugin::HANDLED;
	}
	
	string result = ":";
	result += user->nick;
	result += "!";
	result += user->username;
	result += "@";
	result += user->hostname;
	
	result += " PRIVMSG ";
	
	vector<IRC_Server::Channel*>* channels = link->get_channels();
	
	string channels_string = parts[1];
	for (int z = 0;z < channels_string.size();)
	{
		int next = channels_string.find(",", z);
		
		string channel = channels_string.substr(z, next);
		z = next+1;
		
		if (channel[0] != '#')
			continue;
		
		string message = result;
		message += channel;
		message += " :";
		message += parts[2];
		message += IRC_Server::irc_ending;
		
		link->lock_message_mutex();
		for (vector<IRC_Server::Channel*>::iterator it = channels->begin();it != channels->end();it++)
		{
			if ((*it)->name == channel)
			{
				vector<IRC_Server::Channel*>::iterator user_it = find(user->channels.begin(), user->channels.end(), *it);
				
				if (user_it != user->channels.end())
				{
					vector<IRC_Server::User*>::iterator channel_it = find((*it)->users.begin(), (*it)->users.end(), user);
					
					if (channel_it != (*it)->users.end())
					{
						vector<IRC_Server::User*> users = (*user_it)->users;
						vector<IRC_Server::User*>::iterator ut = find(users.begin(),users.end(),user);
						users.erase(ut);
						link->broadcast_message(message, users, false);
					}
				}
				
				break;
			}
		}
		link->unlock_message_mutex();
		
		if (next == string::npos)
			break;
	}
	
	return IRC_Plugin::HANDLED;
}
