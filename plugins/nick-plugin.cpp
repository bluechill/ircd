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
	return "NICK";
}

extern "C" IRC_Plugin::Result_Of_Call plugin_call(IRC_Plugin::Call_Type type, IRC_Server::User* user, std::vector<std::string> &parts, IRC_Server* link)
{
	using namespace std;
	
	if (type != IRC_Plugin::ON_RECIEVE_MESSAGE)
		return IRC_Plugin::NOT_HANDLED;
	
	if (strncasecmp(parts[0].c_str(), "NICK", 4) != 0)
		return IRC_Plugin::NOT_HANDLED;
	
	if (parts.size() != 2)
	{
		if (parts.size() < 2)
			link->send_error_message(user, IRC_Server::ERR_NONICKNAMEGIVEN);
		
		return IRC_Plugin::HANDLED;
	}
	
	string new_nick = parts[1];
	
	if (new_nick.find_first_not_of("01234567890QWERTYUIOPASDFGHJKLZXCVBNMqwertyuiopasdfghjklzxcvbnm_-\[]{}`^|") != string::npos || new_nick.find_first_of("0123456789-_") == 0)
	{
		link->send_error_message(user, IRC_Server::ERR_ERRONEUSNICKNAME, new_nick);
		return IRC_Plugin::HANDLED;
	}
	
	if (new_nick.size() > 255)
	{
		link->send_error_message(user, IRC_Server::ERR_ERRONEUSNICKNAME, new_nick);
		return IRC_Plugin::HANDLED;
	}
	
	link->unlock_message_mutex();
	
	vector<IRC_Server::User*>* users = link->get_users();
	
	for (vector<IRC_Server::User*>::iterator it = users->begin();it != users->end();it++)
	{
		IRC_Server::User* exist_user = *it;
		
		if (exist_user->nick.size() != new_nick.size())
			continue;
		
		if (strncasecmp(exist_user->nick.c_str(), new_nick.c_str(), new_nick.size()) == 0)
		{
			if (new_nick == user->nick)
				break;
			
			link->send_error_message(user, IRC_Server::ERR_NICKNAMEINUSE, new_nick);
			
			link->unlock_message_mutex();
			
			return IRC_Plugin::HANDLED;
		}
	}
	
	link->unlock_message_mutex();
	
	string old_nick = user->nick;
	user->nick = new_nick;
	
	if (user->username.size() != 0 && old_nick.size() != 0)
	{
		string result = ":";
		result += old_nick;
		result += "!";
		result += user->username;
		result += "@";
		result += user->hostname;
		
		result += " NICK :";
		
		result += user->nick;
		
		result += IRC_Server::irc_ending;
		
		link->broadcast_message(result, user->channels);
	}
	else
	{
		string random = "";
		
		for (int i = 0;i < 8;i++)
		{
			int num = rand()%42 + 48;
			
			if (num >= 58 && num <= 64)
				num += 7;
			
			random += char(num);
		}
		
		string ping_message = "PING :" + random + IRC_Server::irc_ending;
		
		struct timespec current_time;
		
		current_time = link->get_current_time();
		
		user->ping_timer  = (double(current_time.tv_sec) + double(current_time.tv_nsec)/double(1E9)) - 35;
		user->ping_contents = random;
		
		link->send_message(ping_message, user);
	}
	
	return IRC_Plugin::HANDLED;
}
