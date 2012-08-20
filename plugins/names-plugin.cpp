#include "IRC_Server.h"
#include "IRC_Plugin.h"

#include <vector>
#include <string>

extern "C" std::vector<IRC_Plugin::Call_Type> get_supported_calls()
{
	std::vector<IRC_Plugin::Call_Type> types;
	types.push_back(IRC_Plugin::ON_RECIEVE_MESSAGE);
	
	return types;
}

extern "C" IRC_Plugin::Result_Of_Call plugin_call(IRC_Plugin::Call_Type type, IRC_Server::User* user, std::vector<std::string> &parts, IRC_Server* link)
{
	using namespace std;
	
	if (type != IRC_Plugin::ON_RECIEVE_MESSAGE)
		return IRC_Plugin::NOT_HANDLED;
	
	if (parts[0].size() != 5)
		return IRC_Plugin::NOT_HANDLED;
	
	if (strncasecmp(parts[0].c_str(), "NAMES", 5) != 0)
		return IRC_Plugin::NOT_HANDLED;
	
	if (parts.size() == 0 || parts.size() > 2)
		return IRC_Plugin::HANDLED;
	
	string channel = "*";
	
	if (parts.size() == 2)
		channel = parts[1];
	
	if (channel.find(",") != string::npos)
	{
		link->send_error_message(user, IRC_Server::ERR_TOOMANYTARGETS, channel);
		return IRC_Plugin::HANDLED;
	}
	
	if (channel != "*" && channel[0] == '#')
	{
		string list_of_users = ":" + link->get_hostname() + " 353 " + user->nick + " = " + channel + " :";
		
		bool found_channel = false;
		
		for (vector<IRC_Server::Channel*>::iterator it = link->get_channels().begin();it != link->get_channels().end();it++)
		{
			IRC_Server::Channel* chan = *it;;
			
			if (chan->name.size() != channel.size())
				continue;
			
			if (strncasecmp(chan->name.c_str(), channel.c_str(), channel.size()) == 0)
			{
				found_channel = true;
				
				string users = "";
				
				bool found = false;
				
				for (vector<IRC_Server::User*>::iterator jt = chan->users.begin();jt != chan->users.end();jt++)
				{
					IRC_Server::User* usr = *jt;
					
					users += usr->nick;
					if ((jt + 1) != chan->users.end())
						users += " ";
					
					if (user->nick.size() != user->nick.size())
						continue;
					
					if (strncasecmp(usr->nick.c_str(), user->nick.c_str(), usr->nick.size()) == 0)
						found = true;
				}
				
				if (found)
					list_of_users += users + IRC_Server::irc_ending;
				
				break;
			}
		}
		
		if (found_channel)
			link->send_message(list_of_users, user);
	}
	
	string end_of_names = ":" + link->get_hostname() + " 366 " + user->nick + " " + channel + " :End of /NAMES list." + IRC_Server::irc_ending;
	link->send_message(end_of_names, user);
	
	return IRC_Plugin::HANDLED;
}