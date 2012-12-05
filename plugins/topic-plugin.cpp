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
	return "TOPIC";
}

extern "C" IRC_Plugin::Result_Of_Call plugin_call(IRC_Plugin::Call_Type type, IRC_Server::User* user, std::vector<std::string> &parts, IRC_Server* link)
{
	using namespace std;
	
	if (type != IRC_Plugin::ON_RECIEVE_MESSAGE)
		return IRC_Plugin::NOT_HANDLED;
	
	if (strncasecmp(parts[0].c_str(), "TOPIC", 5) != 0)
		return IRC_Plugin::NOT_HANDLED;

	if (parts.size() > 3)
	{
		if (parts.size() < 2)
			link->send_error_message(user, IRC_Server::ERR_NEEDMOREPARAMS, "TOPIC");
		
		return IRC_Plugin::HANDLED;
	}
	
	link->lock_message_mutex();
	
	vector<IRC_Server::Channel*>* channels = link->get_channels();
	
	string channels_string = parts[1];
	
	if (channels_string[0] != '#')
	{
		link->send_error_message(user, IRC_Server::ERR_NOTONCHANNEL);
		return IRC_Plugin::HANDLED;
	}
		
	for (int i = 0;i < channels->size();i++)
	{
		if (channels->at(i)->name == channels_string)
		{
			IRC_Server::Channel* chan = channels->at(i);
			
			if (parts.size() == 3)
			{
				if (find(chan->modes.begin(),chan->modes.end(), pair<char, string>('t', "")) != chan->modes.end())
				{
					for (int i = 0;i < chan->modes.size();i++)
					{
						if (chan->modes[i].first == 'h' ||
							chan->modes[i].first == 'o' ||
							chan->modes[i].first == 'a' ||
							chan->modes[i].first == 'q' ||
							find(user->modes.begin(), user->modes.end(), pair<char, string>('o', "")) != user->modes.end() ||
							find(user->modes.begin(), user->modes.end(), pair<char, string>('O', "")) != user->modes.end() )
							break;
						
						link->send_error_message(user, IRC_Server::ERR_CHANOPRIVSNEEDED, chan->name);
						return IRC_Plugin::HANDLED;
					}
				}
				
				string new_topic = parts[2];
								
				if (new_topic.size() > 256)
					new_topic.resize(256);
				
				string result = ":";
				result += user->nick;
				result += "!";
				result += user->username;
				result += "@";
				result += user->hostname;
				result += " TOPIC ";
				result += chan->name;
				result += " :";
				result += new_topic;
				result += IRC_Server::irc_ending;
				
				chan->topic = new_topic;
				
				link->broadcast_message(result, chan, false);
			}
			else
			{
				string result = ":";
				result += link->get_hostname();
				result += " 332 ";
				result += user->nick;
				result += " ";
				result += chan->name;
				result += " :";
				result += chan->topic;
				result += IRC_Server::irc_ending;
				
				link->send_message(result, user);
			}
		}
	}
	
	link->unlock_message_mutex();
	
	return IRC_Plugin::HANDLED;
}
