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
	return "MODE";
}

std::vector<std::pair<char, std::string> >::iterator contains_modes(std::string modes, std::string argument, std::vector<std::pair<char, std::string> > &vector)
{
	for (std::vector<std::pair<char, std::string> >::iterator it = vector.begin();it != vector.end();it++)
	{
		for (int i = 0;i < modes.size();i++)
		{
			if (it->first == modes[i] && it->second == argument)
				return it;
		}
	}
	
	return vector.end();
}

std::vector<std::pair<char, std::string> >::iterator contains_modes(std::string modes, std::vector<std::pair<char, std::string> > &vector)
{
	for (std::vector<std::pair<char, std::string> >::iterator it = vector.begin();it != vector.end();it++)
	{
		for (int i = 0;i < modes.size();i++)
		{
			if (it->first == modes[i])
				return it;
		}
	}
	
	return vector.end();
}

std::vector<std::pair<char, std::string> >::iterator contains_mode(char mode, std::string argument, std::vector<std::pair<char, std::string> > &vector)
{
	for (std::vector<std::pair<char, std::string> >::iterator it = vector.begin();it != vector.end();it++)
	{
		if (it->first == mode && it->second == argument)
			return it;
	}
	
	return vector.end();
}

std::vector<std::pair<char, std::string> >::iterator contains_mode(char mode, std::vector<std::pair<char, std::string> > &vector)
{
	for (std::vector<std::pair<char, std::string> >::iterator it = vector.begin();it != vector.end();it++)
	{
		if (it->first == mode)
			return it;
	}
	
	return vector.end();
}

std::pair<std::vector<std::pair<char, std::string> >, std::string> parse_user_modes(std::string modes, IRC_Server::User* user, IRC_Server* link)
{
	using namespace std;
	
	vector<pair<char, string> > final_modes(user->modes);
	vector<pair<char, string> > added_modes;
	vector<pair<char, string> > removed_modes;
	
	string mode_chars = modes.substr(0,modes.find_first_of(" ", 0));
	vector<string> mode_arguments;
	
	for (int i = modes.find_first_of(" ", 0);i < modes.size() && i != string::npos;i = modes.find_first_of(" ", i+1))
		mode_arguments.push_back(modes.substr(i+1, modes.find_first_of(" ", i+1)-i-1));
	
	bool adding = true;
	for (int i = 0;i < mode_chars.size();i++)
	{
		if (mode_chars[i] == '+')
			adding = true;
		else if (mode_chars[i] == '-')
			adding = false;
		else
		{
			switch (mode_chars[i])
			{
					//Only Network Admin
				case 'N': //Network Staff
				case 'A': //Network Admin
				case 'S': //is a service
				{
					if (contains_modes("AN", user->modes) == user->modes.end())
						break;
				}
					//Only Net Staff & Above
					//Only local oper & global oper
				case 'o': //global oper
				{
					if (contains_modes("oAN", user->modes) == user->modes.end())
						break;
				}
				case 'h': //Availible to help
				case 'p': //Hides user's channels from whois
				case 'r': //registered
				case 's': //recieves server notices
				case 'w': //wallops
				case 'x': //User has a hidden host
				case 'v': //User has a modified host (vhost)
				case 'z': //securely connected
				case 'i': //invisible
				case 'I': //only invisible on join/part, still visible in whois, etc.
				case 'B': //is a bot
				case 'O': //local server oper
				{
					if (contains_modes("oOAN", user->modes) == user->modes.end())
						break;
				}
					//All users can add this
				case 'a': //away
				{
					if (adding)
					{
						if (contains_mode(mode_chars[i], final_modes) == final_modes.end())
						{
							pair<char, string> new_mode_pair;
							new_mode_pair.first = mode_chars[i];
							
							if (mode_chars[i] == 'a' && mode_arguments.size() > 0)
							{
								new_mode_pair.second = mode_arguments[0];
								mode_arguments.erase(mode_arguments.begin());
							}
							else if (mode_chars[i] == 'a' && mode_arguments.size() == 0)
								break;
							
							final_modes.push_back(new_mode_pair);
							added_modes.push_back(new_mode_pair);
						}
					}
					else
					{
						if (contains_mode(mode_chars[i], final_modes) != final_modes.end())
						{
							pair<char, string> new_mode_pair;
							new_mode_pair.first = mode_chars[i];
							
							final_modes.erase(contains_mode(mode_chars[i], final_modes));
							removed_modes.push_back(new_mode_pair);
						}
					}
					break;
				}
				default:
				{
					string character = "";
					character += mode_chars[i];
					link->send_error_message(user, IRC_Server::ERR_UNKNOWNMODE, character);
					break;
				}
			}
		}
	}
	
	string mode_string;
	
	if (added_modes.size() > 0)
	{
		mode_string += "+";
		
		for (int i = 0;i < added_modes.size();i++)
			mode_string += added_modes[i].first;
	}
	
	if (removed_modes.size() > 0)
	{
		mode_string += "-";
		
		for (int i = 0;i < removed_modes.size();i++)
			mode_string += removed_modes[i].first;
	}
	
	for (int i = 0;i < added_modes.size();i++)
	{
		if (added_modes[i].second != "")
			mode_string += " " + added_modes[i].second;
	}
	
	for (int i = 0;i < removed_modes.size();i++)
	{
		if (removed_modes[i].second != "")
			mode_string += " " + removed_modes[i].second;
	}
	
	pair<vector<pair<char, string> >, string> to_return;
	to_return.first = final_modes;
	to_return.second = mode_string;
	
	return to_return;
}

std::pair<std::vector<std::pair<char, std::string> >, std::string> parse_channel_modes(std::string modes, IRC_Server::Channel* chan, IRC_Server::User* user, IRC_Server* link)
{
	using namespace std;
	
	vector<pair<char, string> > final_modes(user->modes);
	vector<pair<char, string> > added_modes;
	vector<pair<char, string> > removed_modes;
	
	string mode_chars = modes.substr(0,modes.find_first_of(" ", 0));
	vector<string> mode_arguments;
	
	for (int i = modes.find_first_of(" ", 0);i < modes.size() && i != string::npos;i = modes.find_first_of(" ", i+1))
		mode_arguments.push_back(modes.substr(i+1, modes.find_first_of(" ", i+1)-i-1));
	
	bool adding = true;
	for (int i = 0;i < mode_chars.size();i++)
	{
		if (mode_chars[i] == '+')
			adding = true;
		else if (mode_chars[i] == '-')
			adding = false;
		else
		{
			switch (mode_chars[i])
			{
					//Only local & global oper + network staff & admin can set:
				case 'A': //Only availible to services, network, and server admins
				case 'O': //Only IRC Opers and above can join
				case 'r': //Registered channel
				{
					if (contains_modes("AN", user->modes) == user->modes.end())
						break;
				}
					//Only chan owner can set
				case 'q': //Channel owner
				{
					bool found = false;
					if (contains_modes("oOAN", user->modes) != user->modes.end())
						found = true;

					if (!found)
					{
						if (contains_mode('q', user->nick, chan->modes) != chan->modes.end())
							found = true;
					}
					
					if (!found)
						break;
				}
				case 'a': //Chan Admin
				{
					bool found = false;
					if (contains_modes("oOAN", user->modes) != user->modes.end())
						found = true;
					
					if (!found)
					{
						if (contains_modes("aq", user->nick, chan->modes) != chan->modes.end())
							found = true;
					}
					
					if (!found)
						break;
				}
				case 'o': //Chan OP
				{
					bool found = false;
					if (contains_modes("oOAN", user->modes) != user->modes.end())
						found = true;
					
					if (!found)
					{
						if (contains_modes("oaq", user->nick, chan->modes) != chan->modes.end())
							found = true;
					}
					
					if (!found)
						break;
				}
				case 'b': //Ban
				case 'e': //Ban exception
				case 'f': //Flood limit
				case 'h': //Half OP
				case 'i': //Invite only
				case 'I': //Invite mask
				case 'k': //Key lock
				case 'l': //Limit number of users in a channel
				case 'm': //Moderated (mute)
				case 'n': //No external messages
				case 's': //Secret channel, doesn't show up in LIST
				case 't': //Topic lock
				case 'u': //Audiotorium, only +h and above is shown in /names
				case 'v': //Voice
				{
					bool found = false;
					if (contains_modes("oOAN", user->modes) != user->modes.end())
						found = true;
					
					if (!found)
					{
						if (contains_modes("hoaq", user->nick, chan->modes) != chan->modes.end())
							found = true;
					}
					
					if (!found)
						break;
					
					if (adding)
					{
						if ((mode_chars[i] == 'f' || mode_chars[i] == 'k' || mode_chars[i] == 'l') && contains_mode(mode_chars[i], final_modes) != final_modes.end())
						{
							if (mode_arguments.size() > 0)
								final_modes.erase(contains_mode(mode_chars[i], final_modes));
						}
						
						if (contains_mode(mode_chars[i], final_modes) == final_modes.end() ||
							(string("qaohvbe").find(mode_chars[i]) != string::npos))
						{
							pair<char, string> new_mode_pair;
							new_mode_pair.first = mode_chars[i];
							
							bool should_break = false;
							switch (mode_chars[i])
							{
								case 'q':
								case 'a':
								case 'o':
								case 'h':
								case 'v':
								case 'b':
								case 'e':
								case 'f':
								case 'k':
								case 'l':
								{
									if (mode_arguments.size() == 0)
									{
										should_break = true;
										break;
									}
									
									new_mode_pair.second = mode_arguments[0];
									mode_arguments.erase(mode_arguments.begin());
									
									break;
								}
								default:
									break;
							}
							
							if (should_break)
								break;
							
							final_modes.push_back(new_mode_pair);
							added_modes.push_back(new_mode_pair);
						}
					}
					else
					{
						if (contains_mode(mode_chars[i], final_modes) != final_modes.end())
						{
							pair<char, string> new_mode_pair;
							new_mode_pair.second = mode_chars[i];
							
							bool should_break = false;
							switch (mode_chars[i])
							{
								case 'q':
								case 'a':
								case 'o':
								case 'h':
								case 'v':
								case 'b':
								case 'e':
								{
									if (mode_arguments.size() == 0)
									{
										should_break = true;
										break;
									}
									
									new_mode_pair.second = mode_arguments[0];
									mode_arguments.erase(mode_arguments.begin());
									
									break;
								}
								default:
									break;
							}
							
							if (should_break)
								break;
							
							if (new_mode_pair.second != "")
							{
								if (contains_mode(new_mode_pair.first, new_mode_pair.second, final_modes) == final_modes.end())
									break;
							}
							
							final_modes.erase(contains_mode(new_mode_pair.first, new_mode_pair.second, final_modes));
							removed_modes.push_back(new_mode_pair);
						}
					}
					break;
				}
				default:
				{
					string character = "";
					character += mode_chars[i];
					link->send_error_message(user, IRC_Server::ERR_UNKNOWNMODE, character);
					break;
				}
			}
		}
	}
	
	string mode_string;
	
	if (added_modes.size() > 0)
	{
		mode_string += "+";
		
		for (int i = 0;i < added_modes.size();i++)
			mode_string += added_modes[i].first;
	}
	
	if (removed_modes.size() > 0)
	{
		mode_string += "-";
		
		for (int i = 0;i < removed_modes.size();i++)
			mode_string += removed_modes[i].first;
	}
	
	for (int i = 0;i < added_modes.size();i++)
	{
		if (added_modes[i].second != "")
			mode_string += " " + added_modes[i].second;
	}
	
	for (int i = 0;i < removed_modes.size();i++)
	{
		if (removed_modes[i].second != "")
			mode_string += " " + removed_modes[i].second;
	}
	
	pair<vector<pair<char, string> >, string> to_return;
	to_return.first = final_modes;
	to_return.second = mode_string;
	
	return to_return;
}

extern "C" IRC_Plugin::Result_Of_Call plugin_call(IRC_Plugin::Call_Type type, IRC_Server::User* user, std::vector<std::string> &parts, IRC_Server* link)
{
	using namespace std;
	
	if (type != IRC_Plugin::ON_RECIEVE_MESSAGE)
		return IRC_Plugin::NOT_HANDLED;
	
	if (strncasecmp(parts[0].c_str(), "MODE", 4) != 0)
		return IRC_Plugin::NOT_HANDLED;
	
	if (parts.size() < 3)
		return IRC_Plugin::HANDLED;
	
	string result = ":";
	result += user->nick;
	result += "!";
	result += user->username;
	result += "@";
	result += user->hostname;
	
	result += " MODE ";
	
	bool isUserMode = false;
	
	if (parts[1][0] != '#')
		isUserMode = true;
	
	string user_channel = parts[1];
	string modes = "";
	for (int i = 2;i < parts.size();i++)
	{
		if (i != 2)
			modes += " ";
		
		modes += parts[i];
	}
	
	result += user_channel;
	result += " ";
	
	vector<char> modes_to_add;
	vector<char> modes_to_remove;
	
	IRC_Server::Channel* send_channel = NULL;
	IRC_Server::User* send_user = NULL;
		
	if (isUserMode)
	{
		vector<IRC_Server::User*>* users = link->get_users();
		
		for (vector<IRC_Server::User*>::iterator it = users->begin();it != users->end();it++)
		{
			if ((*it)->nick == user_channel)
			{
				send_user = *it;
				
				pair<vector<pair<char, string> >, string> user_return = parse_user_modes(modes, user, link);
				user->modes = user_return.first;
				result += user_return.second;
				
				break;
			}
		}
	}
	else
	{
		vector<IRC_Server::Channel*>* channels = link->get_channels();
		for (vector<IRC_Server::Channel*>::iterator it = channels->begin();it!= channels->end();it++)
		{
			if ((*it)->name == user_channel)
			{
				send_channel = *it;
				
				pair<vector<pair<char, string> >, string> user_return = parse_channel_modes(modes, (*it), user, link);
				user->modes = user_return.first;
				result += user_return.second;
				
				break;
			}
		}
	}
	
	result += "\r\n";
	
	if (send_channel)
		link->broadcast_message(result, send_channel, false);
	else if (send_user)
		link->send_message(result, send_user);
	else
	{
		link->send_error_message(user, IRC_Server::ERR_NOSUCHCHANNEL, user_channel);
		return IRC_Plugin::HANDLED;
	}
	
	link->unlock_message_mutex();
	
	return IRC_Plugin::HANDLED;
}
