#include "IRC_Server.h"
#include "IRC_Plugin.h"

#include <string.h>

#include <vector>
#include <string>
#include <sstream>

extern "C" std::vector<IRC_Plugin::Call_Type> get_supported_calls()
{
	std::vector<IRC_Plugin::Call_Type> types;
	types.push_back(IRC_Plugin::ON_RECIEVE_MESSAGE);
	
	return types;
}

extern "C" std::string name_of_plugin()
{
	return "LIST";
}

extern "C" IRC_Plugin::Result_Of_Call plugin_call(IRC_Plugin::Call_Type type, IRC_Server::User* user, std::vector<std::string> &parts, IRC_Server* link)
{
	using namespace std;
	
	if (type != IRC_Plugin::ON_RECIEVE_MESSAGE)
		return IRC_Plugin::NOT_HANDLED;
	
	if (strncasecmp(parts[0].c_str(), "LIST", 4) != 0)
		return IRC_Plugin::NOT_HANDLED;
	
	if (parts.size() != 1)
		return IRC_Plugin::HANDLED;
	
	string begin = ":" + link->get_hostname() + " 32";
	
	string start_of_list = begin + "1 " + user->nick + " Channel :Users  Name" + IRC_Server::irc_ending;
	
	string list_message_start = begin + "2 " + user->nick + " ";
	
	string end_of_list = begin + "3 " + user->nick + " :End of /LIST" + IRC_Server::irc_ending;
	
	link->send_message(start_of_list, user);
	
	vector<IRC_Server::Channel*>* channels = link->get_channels();
	link->lock_message_mutex();
	for (vector<IRC_Server::Channel*>::iterator it = channels->begin();it != channels->end();it++)
	{
		IRC_Server::Channel* chan = (*it);
		
		stringstream ss;
		ss << chan->users.size();
		
		string message = list_message_start + chan->name + " " + ss.str() + " :";
		
		string modes = "[+";
		
		for (vector<char>::iterator jt = chan->modes.begin();jt != chan->modes.end();jt++)
			modes += *jt;
		
		modes += "]";
		
		message += modes;
		
		message += " " + chan->topic + IRC_Server::irc_ending;
		
		link->unlock_message_mutex();
		link->send_message(message, user);
		link->lock_message_mutex();
	}
	
	link->unlock_message_mutex();
	link->send_message(end_of_list, user);
	
	return IRC_Plugin::HANDLED;
}
