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
	return "JOIN";
}

extern "C" IRC_Plugin::Result_Of_Call plugin_call(IRC_Plugin::Call_Type type, IRC_Server::User* user, std::vector<std::string> &parts, IRC_Server* link)
{
	using namespace std;
	
	if (type != IRC_Plugin::ON_RECIEVE_MESSAGE)
		return IRC_Plugin::NOT_HANDLED;
	
	if (strncasecmp(parts[0].c_str(), "JOIN", 4) != 0)
		return IRC_Plugin::NOT_HANDLED;
	
	if (parts.size() != 2 && parts.size() != 3)
	{
		if (parts.size() < 2)
			link->send_error_message(user, IRC_Server::ERR_NEEDMOREPARAMS, "JOIN");
		
		return IRC_Plugin::HANDLED;
	}
	
	string result = ":";
	result += user->nick;
	result += "!";
	result += user->username;
	result += "@";
	result += user->hostname;
	
	result += " JOIN :";
	
	vector<IRC_Server::Channel*>* channels = link->get_channels();
	
	string channels_string = parts[1];
	
	for (int z = 0;z < channels_string.size();)
	{
		link->lock_message_mutex();
		int next = channels_string.find(",", z);
		
		string channel = channels_string.substr(z, next);
		z = next+1;
		
		if (channel[0] != '#')
		{
			link->unlock_message_mutex();
			link->send_error_message(user, IRC_Server::ERR_NOSUCHCHANNEL, channel);
			
			continue;
		}
		
		bool found = false;
		
		string message = result;
		message += channel;
		message += IRC_Server::irc_ending;
		
		for (vector<IRC_Server::Channel*>::iterator it = channels->begin();it != channels->end();it++)
		{
			if ((*it)->name == channel)
			{
				found = true;
				
				vector<IRC_Server::User*>::iterator jt = find((*it)->users.begin(), (*it)->users.end(), user);
				
				if (jt != (*it)->users.end())
					break;
				
				user->channels.push_back(*it);
				(*it)->users.push_back(user);
				
				link->broadcast_message(message, *it, false);
				break;
			}
		}
		
		if (!found)
		{
			IRC_Server::Channel* new_channel = new IRC_Server::Channel;
			new_channel->name = channel;
			new_channel->users.push_back(user);
			
			channels->push_back(new_channel);
			user->channels.push_back(new_channel);
			
			link->broadcast_message(message, new_channel, false);
		}
		
		if (next == string::npos)
			break;
		
		link->unlock_message_mutex();
	}
	
	link->unlock_message_mutex();
	
	return IRC_Plugin::HANDLED;
}
