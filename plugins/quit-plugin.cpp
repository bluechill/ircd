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
	return "QUIT";
}

extern "C" IRC_Plugin::Result_Of_Call plugin_call(IRC_Plugin::Call_Type type, IRC_Server::User* user, std::vector<std::string> &parts, IRC_Server* link)
{
	using namespace std;
	
	if (type != IRC_Plugin::ON_RECIEVE_MESSAGE)
		return IRC_Plugin::NOT_HANDLED;
	
	if (strncasecmp(parts[0].c_str(), "QUIT", 4) != 0)
		return IRC_Plugin::NOT_HANDLED;
	
	string quit_message = "ERROR :Closing Link: " + user->nick + "[" + user->hostname + "] (Quit: ";
	
	string quit = "";
	
	if (parts.size() == 1)
		quit += " " + user->nick;
	else
	{
		for (vector<string>::iterator it = parts.begin()+1;it != parts.end();it++)
			quit += " " + *it;
	}
	
	quit.erase(quit.begin());
	
	quit_message += quit;
	
	quit_message += ")";
	quit_message += IRC_Server::irc_ending;
	
	vector<IRC_Server::Channel*>* channels = link->get_channels();
	
	link->lock_message_mutex();
	for (vector<IRC_Server::Channel*>::iterator it = user->channels.begin();it != user->channels.end();it++)
	{
		vector<IRC_Server::User*>::iterator channel_it = find((*it)->users.begin(), (*it)->users.end(), user);
		
		if (channel_it != (*it)->users.end())
			(*it)->users.erase(channel_it);
		else
			link->send_error_message(user, IRC_Server::ERR_NOTONCHANNEL, (*it)->name);
		
		if ((*it)->users.size() == 0)
		{
			vector<IRC_Server::Channel*>::iterator cit = find(channels->begin(), channels->end(), *it);
			
			if (cit != channels->end())
				channels->erase(cit);
		}
	}
	link->unlock_message_mutex();
	
	link->send_message(quit_message, user);
	
	IRC_Server::User* temp_user = new IRC_Server::User;
	temp_user->channels = user->channels;
	temp_user->nick = user->nick;
	temp_user->username = user->username;
	temp_user->hostname = user->hostname;
	
	link->disconnect_client(user->client);
	
	string output = ":" + temp_user->nick + "!" + temp_user->username + "@" + temp_user->hostname + " QUIT :" + quit + IRC_Server::irc_ending;
	
	link->broadcast_message(output, temp_user->channels);
	
	delete temp_user;
	
	return IRC_Plugin::HANDLED;
}
