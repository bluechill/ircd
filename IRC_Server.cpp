#include "IRC_Server.h"

#include <iostream>
#include <vector>
#include <string>
#include <sstream>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <time.h>
#include <unistd.h>

#include <errno.h>

#ifdef __MACH__

#include <mach/mach_time.h>
#define ORWL_NANO (+1.0E-9)
#define ORWL_GIGA UINT64_C(1000000000)

static double orwl_timebase = 0.0;
static uint64_t orwl_timestart = 0;

struct timespec orwl_gettime(void) {
	// be more careful in a multithreaded environement
	if (!orwl_timestart) {
		mach_timebase_info_data_t tb = { 0 };
		mach_timebase_info(&tb);
		orwl_timebase = tb.numer;
		orwl_timebase /= tb.denom;
		orwl_timestart = mach_absolute_time();
	}
	struct timespec t;
	double diff = (mach_absolute_time() - orwl_timestart) * orwl_timebase;
	t.tv_sec = diff * ORWL_NANO;
	t.tv_nsec = diff - (t.tv_sec * ORWL_GIGA);
	return t;
}

#endif

const std::string IRC_Server::irc_ending = "\r\n";

void* ping_thread_function(void* data)
{
	using namespace std;
	
	IRC_Server::ping_thread_struct *pts = reinterpret_cast<IRC_Server::ping_thread_struct*>(data);
	
	vector<IRC_Server::User*>* users = pts->users;
	pthread_mutex_t *ping_mutex = pts->ping_mutex;
	IRC_Server* link = pts->server_handle;
	
	struct timespec current_time;
	double double_current_time = 0.0;
	
	pthread_mutex_lock(ping_mutex);
	
	for (vector<IRC_Server::User*>::iterator it = users->begin();it != users->end();it++)
		(*it)->ping_timer = -1;
	
	pthread_mutex_unlock(ping_mutex);
	
	while (true)
	{
		sleep(1);
		
#ifndef __MACH__
		clock_gettime(CLOCK_MONOTONIC, &current_time);
#else
		current_time = orwl_gettime();
#endif
		
		double_current_time = (double(current_time.tv_sec) + double(current_time.tv_nsec)/double(1E9));
		
		pthread_mutex_lock(ping_mutex);
		
		for (vector<IRC_Server::User*>::iterator it = users->begin();it != users->end();it++)
		{
			IRC_Server::User* user = *it;
			
			double user_time = user->ping_timer;
			
			if ((double_current_time - user_time) > double(30) &&
				(double_current_time - user_time) < double(35) &&
				!(user->nick.size() == 0 || user->username.size() == 0))
			{
				string ping_message = "PING :" + link->get_hostname() + IRC_Server::irc_ending;
				
				user->ping_timer -= 5;
				user->ping_contents = link->get_hostname();
				
				link->send_message(ping_message, user);
			}
			else if ((double_current_time - user_time) > double(65))
			{
				string quit_message = "ERROR :Closing Link: " + user->nick + "[" + user->hostname + "] (Ping Timeout)" + IRC_Server::irc_ending;
				
				link->send_message(quit_message, user);
				
				IRC_Server::User* temp_user = new IRC_Server::User;
				temp_user->channels = user->channels;
				temp_user->nick = user->nick;
				temp_user->username = user->username;
				temp_user->hostname = user->hostname;
				
				pthread_mutex_unlock(ping_mutex);
				link->disconnect_client(user->socket);
				it--;
				
				string output = ":" + temp_user->nick + "!" + temp_user->username + "@" + temp_user->hostname + " QUIT :Ping Timeout" + IRC_Server::irc_ending;
				
				link->broadcast_message(output, temp_user->channels);
				pthread_mutex_lock(ping_mutex);
			}
		}
		
		pthread_mutex_unlock(ping_mutex);
	};
	
	delete pts;
}

IRC_Server::IRC_Server()
: Server(6667, Server::Dual_IPv4_IPv6_Server, true)
{
	srand(time(NULL));
	
	pthread_mutex_init(&ping_mutex, NULL);
	
	struct addrinfo hints, *info, *p;
	int gai_result;
	
	char hostname[1024];
	hostname[1023] = '\0';
	gethostname(hostname, 1023);
	
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC; /*either IPV4 or IPV6*/
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_CANONNAME;
	
	if ((gai_result = getaddrinfo(hostname, "http", &hints, &info)) != 0)
	{
		std::cerr << "Get Address Info Error (LIST): '" << gai_strerror(gai_result) << "'" << std::endl;
		
		exit(1);
	}
	
	this->hostname = info->ai_canonname;
	
	ping_thread_struct *pts = new ping_thread_struct;
	pts->users = &users;
	pts->ping_mutex = &ping_mutex;
	pts->server_handle = this;
	
	int status = pthread_create(&ping_thread, NULL, ping_thread_function, pts);
	if (status)
	{
		std::cerr << "Failed to create ping thread with error: " << status << " (\"" << strerror(errno) << "\")" << std::endl;
		throw Thread_Initialization_Failure;
	}
}

IRC_Server::~IRC_Server()
{
	pthread_exit(NULL);
	pthread_mutex_destroy(&ping_mutex);
	
	for (std::vector<User*>::iterator it = users.begin();it != users.end();it++)
		delete *it;
	
	for (std::vector<Channel*>::iterator it = channels.begin();it != channels.end();it++)
		delete *it;
}

void IRC_Server::on_client_connect(int &client_sock)
{
	using namespace std;
	
	User* new_user = new User;
	new_user->nick = "";
	new_user->username = "";
	new_user->realname = "";
	new_user->socket = client_sock;
	
	struct timespec current_time;
	
#ifndef __MACH__
	clock_gettime(CLOCK_MONOTONIC, &current_time);
#else
	current_time = orwl_gettime();
#endif
	
	new_user->ping_timer = (double(current_time.tv_sec) + double(current_time.tv_nsec)/double(1E9));
	
	struct sockaddr_storage addr;
	socklen_t length = sizeof(addr);
	
	getpeername(client_sock, (struct sockaddr*)&addr, &length);
	
	struct hostent *hp;
	
	if (addr.ss_family == AF_INET)
	{
		struct sockaddr_in *s = (struct sockaddr_in *)&addr;
		
		if ((hp = gethostbyaddr((char *)&s->sin_addr, sizeof(s->sin_addr), AF_INET)))
			new_user->hostname = hp->h_name;
		else
		{
			cerr << "Error: " << hstrerror(h_errno) << endl;
			close(client_sock);
			return;
		}
	}
	else
	{
		struct sockaddr_in6 *s = (struct sockaddr_in6 *)&addr;
		
		if ((hp = gethostbyaddr((char *)&s->sin6_addr, sizeof(s->sin6_addr), AF_INET6)))
			new_user->hostname = hp->h_name;
		else
		{
			cerr << "Error: " << hstrerror(h_errno) << endl;
			close(client_sock);
			return;
		}
	}
	
	pthread_mutex_lock(&ping_mutex);
	users.push_back(new_user);
	pthread_mutex_unlock(&ping_mutex);
}

void IRC_Server::on_client_disconnect(int &client_socket)
{
	pthread_mutex_lock(&ping_mutex);
	for (std::vector<User*>::iterator it = users.begin();it != users.end();it++)
	{
		if ((*it)->socket == client_socket)
		{
			delete *it;
			users.erase(it);
			break;
		}
	}
	pthread_mutex_unlock(&ping_mutex);
}

void IRC_Server::recieve_message(std::string &message, int &client_sock)
{
	using namespace std;
	
	if (verbose)
		cout << "Recieved message: '" << message << "'";
	
	if (message.find("\n") == string::npos)
	{
		if (verbose)
			cout << " which is invalid." << endl;
		
		return;
	}
	else if (verbose)
		cout << endl;
	
	parse_message(message, client_sock);
}

void IRC_Server::send_message(std::string &message, User* user)
{
	if (user == NULL)
		return;
	
	write(user->socket, message.c_str(), message.size());
}

void IRC_Server::broadcast_message(std::string &message, std::vector<User*> users)
{
	pthread_mutex_lock(&ping_mutex);
	for (std::vector<User*>::iterator it = users.begin();it != users.end();it++)
		send_message(message, *it);
	pthread_mutex_unlock(&ping_mutex);
}

void IRC_Server::broadcast_message(std::string &message, Channel* users)
{
	broadcast_message(message, users->users);
}

void IRC_Server::broadcast_message(std::string &message, std::vector<Channel*> channels)
{
	for (std::vector<Channel*>::iterator it = channels.begin();it != channels.end();it++)
		broadcast_message(message, (*it));
}

IRC_Server::Message_Type IRC_Server::string_to_message_type(std::string &message)
{
	if (message.size() == 4)
	{
		if (strncasecmp(message.c_str(), "NICK", message.size()) == 0)
			return NICK;
		else if (strncasecmp(message.c_str(), "USER", message.size()) == 0)
			return USER;
		else if (strncasecmp(message.c_str(), "PONG", message.size()) == 0)
			return PONG;
		else if (strncasecmp(message.c_str(), "JOIN", message.size()) == 0)
			return JOIN;
		else if (strncasecmp(message.c_str(), "PART", message.size()) == 0)
			return PART;
		else if (strncasecmp(message.c_str(), "LIST", message.size()) == 0)
			return LIST;
		else if (strncasecmp(message.c_str(), "QUIT", message.size()) == 0)
			return QUIT;
	}
	else if (message.size() == 7)
	{
		if (strncasecmp(message.c_str(), "PRIVMSG", message.size()) == 0)
			return PRIVMSG;
	}
	
	return UNKNOWN;
}

IRC_Server::User* IRC_Server::sock_to_user(int &sock)
{
	pthread_mutex_lock(&ping_mutex);
	for (std::vector<User*>::iterator it = users.begin();it != users.end();it++)
	{
		if ((*it)->socket == sock)
		{
			pthread_mutex_unlock(&ping_mutex);
			return *it;
		}
	}
	pthread_mutex_unlock(&ping_mutex);
	
	return NULL;
}

IRC_Server::Channel* IRC_Server::channel_name_to_channel(std::string &channel)
{
	for (std::vector<Channel*>::iterator it = channels.begin();it != channels.end();it++)
	{
		if ((*it)->name == channel)
			return *it;
	}
	
	return NULL;
}

void IRC_Server::parse_message(std::string &message, int &client_sock)
{
	using namespace std;
	
	message.erase(message.size()-2, 2);
	
	vector<string> parts;
	
	for (int i = 0;i < message.size();)
	{
		int next = message.find(' ', i);
		
		parts.push_back(message.substr(i, next-i));
		
		if (next == string::npos)
			break;
		
		i = ++next;
	}
	
	if (parts.size() == 0)
		return;
	
	for (vector<string>::iterator it = parts.begin();it != parts.end();it++)
	{
		if (it->at(0) == ':')
		{
			string message;
			
			it->erase(0,1);
			
			for (vector<string>::iterator jt = it;jt != parts.end();jt++)
				message += *jt;
			
			parts.erase(it, parts.end());
			parts.push_back(message);
			
			break;
		}
	}
	
	Message_Type type = string_to_message_type(parts[0]);
	
	User* user = sock_to_user(client_sock);
	
	if (user == NULL)
	{
		cerr << "Invalid user socket for server! : '" << message << "'" << endl;
		return;
	}
	
	if ((user->nick.size() == 0 || user->username.size() == 0) && (type != NICK && type != USER && type != QUIT && type != PONG))
	{
		send_error_message(user, ERR_ALREADYREGISTRED);
		
		return;
	}
	
	switch (type)
	{
		case NICK:
		{
			parse_nick(user, parts);
			
			return;
		}
		case USER:
		{
			parse_user(user, parts);
			
			return;
		}
		case PONG:
		{
			parse_pong(user, parts);
			
			return;
		}
		case JOIN:
		{
			parse_join(user, parts);
			
			return;
		}
		case PART:
		{
			parse_part(user, parts);
			
			return;
		}
		case LIST:
		{
			parse_list(user, parts);
			
			return;
		}
		case QUIT:
		{
			parse_quit(user, parts);
			
			return;
		}
		case PRIVMSG:
		{
			parse_privmsg(user, parts);
			
			return;
		}
		default:
		{
			cerr << "Invalid message: '" << message << "'" << endl;
			return;
		}
	}
}

void IRC_Server::parse_nick(User* user, std::vector<std::string> parts)
{
	using namespace std;
	
	if (parts.size() != 2)
	{
		if (parts.size() < 2)
			send_error_message(user, ERR_NONICKNAMEGIVEN);
		
		return;
	}
	
	string new_nick = parts[1];
	
	if (new_nick.find_first_not_of("01234567890QWERTYUIOPASDFGHJKLZXCVBNMqwertyuiopasdfghjklzxcvbnm_-\[]{}`^|") != string::npos || new_nick.find_first_of("0123456789-_") == 0)
	{
		send_error_message(user, ERR_ERRONEUSNICKNAME, new_nick);
		return;
	}
	
	pthread_mutex_lock(&ping_mutex);
	
	for (vector<User*>::iterator it = users.begin();it != users.end();it++)
	{
		User* exist_user = *it;
		
		if (exist_user->nick.size() != new_nick.size())
			continue;
		
		if (strncasecmp(exist_user->nick.c_str(), new_nick.c_str(), new_nick.size()) == 0)
		{
			if (new_nick == user->nick)
				break;
			
			send_error_message(user, ERR_NICKNAMEINUSE, new_nick);
			
			pthread_mutex_unlock(&ping_mutex);
			
			return;
		}
	}
	
	pthread_mutex_unlock(&ping_mutex);
	
	string old_nick = user->nick;
	user->nick = new_nick;
	
	if (user->username.size() != 0)
	{
		string result = ":";
		result += old_nick;
		result += "!";
		result += user->username;
		result += "@";
		result += user->hostname;
		
		result += " NICK :";
		
		result += user->nick;
		
		result += irc_ending;
		
		broadcast_message(result, user->channels);
	}
	else
	{
		string random = "";
		
		for (int i = 0;i < 9;i++)
		{
			int num = rand()%42 + 48;
			
			if (num >= 58 && num <= 64)
				num += 7;
			
			random += char(num);
		}
		
		string ping_message = "PING :" + random + IRC_Server::irc_ending;
		
		struct timespec current_time;
		
#ifndef __MACH__
		clock_gettime(CLOCK_MONOTONIC, &current_time);
#else
		current_time = orwl_gettime();
#endif
		
		user->ping_timer  = (double(current_time.tv_sec) + double(current_time.tv_nsec)/double(1E9));
		user->ping_contents = random;
		
		send_message(ping_message, user);
	}
}

void IRC_Server::parse_user(User* user, std::vector<std::string> parts)
{
	using namespace std;
	
	if (parts.size() != 5)
	{
		if (parts.size() < 5)
			send_error_message(user, ERR_NEEDMOREPARAMS, "USER");
		
		return;
	}
	
	if (user->username.size() != 0 && user->nick.size() != 0)
	{
		send_error_message(user, ERR_ALREADYREGISTRED);
		return;
	}
	
	if (parts[1].find_first_not_of("0123456789qwertyuiopasdfghjklzxcvbnmQWERTYUIOPASDFGHJKLZXCVBNM_-.") != string::npos)
	{
		string output = "ERROR :Closing Link: " + user->nick + "[" + user->hostname + "] (Hostile username.  Please only use 0-9 a-z A-Z _ - and . in your username.)" + irc_ending;
		
		send_message(output, user);
		disconnect_client(user->socket);
		return;
	}
	
	user->username = parts[1];
	user->realname = parts[4];
}

void IRC_Server::parse_pong(User* user, std::vector<std::string> parts)
{
	using namespace std;
	
	if (parts.size() != 2)
		return;
	
	std::string contents = parts[1];
	
	if (user->ping_contents == contents)
	{
		struct timespec current_time;
		
#ifndef __MACH__
		clock_gettime(CLOCK_MONOTONIC, &current_time);
#else
		current_time = orwl_gettime();
#endif
		
		user->ping_timer  = (double(current_time.tv_sec) + double(current_time.tv_nsec)/double(1E9));
		user->ping_contents = "";
	}
}

void IRC_Server::parse_join(User* user, std::vector<std::string> parts)
{
	using namespace std;
	
	if (parts.size() != 2 && parts.size() != 3)
	{
		if (parts.size() < 2)
			send_error_message(user, ERR_NEEDMOREPARAMS, "JOIN");
		
		return;
	}
	
	string result = ":";
	result += user->nick;
	result += "!";
	result += user->username;
	result += "@";
	result += user->hostname;
	
	result += " JOIN :";
	
	string channels_string = parts[1];
	for (int z = 0;z < channels_string.size();)
	{
		int next = channels_string.find(",", z);
		
		string channel = channels_string.substr(z, next);
		z = next+1;
		
		if (channel[0] != '#')
		{
			send_error_message(user, ERR_NOSUCHCHANNEL, channel);
			
			continue;
		}
		
		bool found = false;
		
		string message = result;
		message += channel;
		message += irc_ending;
		
		for (vector<Channel*>::iterator it = channels.begin();it != channels.end();it++)
		{
			if ((*it)->name == channel)
			{
				found = true;
				
				user->channels.push_back(*it);
				(*it)->users.push_back(user);
				
				broadcast_message(message, *it);
				break;
			}
		}
		
		if (!found)
		{
			Channel* new_channel = new Channel;
			new_channel->name = channel;
			new_channel->users.push_back(user);
			
			channels.push_back(new_channel);
			user->channels.push_back(new_channel);
			
			broadcast_message(message, new_channel);
		}
		
		if (next == string::npos)
			break;
	}
}

void IRC_Server::parse_part(User* user, std::vector<std::string> parts)
{
	using namespace std;
	
	if (parts.size() != 2)
		return;
	
	string result = ":";
	result += user->nick;
	result += "!";
	result += user->username;
	result += "@";
	result += user->hostname;
	
	result += " PART ";
	
	string channels_string = parts[1];
	for (int z = 0;z < channels_string.size();)
	{
		int next = channels_string.find(",", z);
		
		string channel = channels_string.substr(z, next);
		z = next+1;
		
		if (channel[0] != '#')
		{
			send_error_message(user, ERR_NOSUCHCHANNEL, channel);
			
			continue;
		}
		
		string message = result;
		message += channel;
		message += irc_ending;
		
		bool found = false;
		for (vector<Channel*>::iterator it = channels.begin();it != channels.end();it++)
		{
			if ((*it)->name == channel)
			{
				vector<Channel*>::iterator user_it = find(user->channels.begin(), user->channels.end(), *it);
				
				if (user_it != user->channels.end())
				{
					vector<User*>::iterator channel_it = find((*it)->users.begin(), (*it)->users.end(), user);
					
					user->channels.erase(user_it);
					
					if (channel_it != (*it)->users.end())
					{
						broadcast_message(message, *it);
						
						(*it)->users.erase(channel_it);
					}
				}
				else
					send_error_message(user, ERR_NOTONCHANNEL, channel);
				
				if ((*it)->users.size() == 0)
					it = channels.erase(it);
				
				found = true;
				
				break;
			}
		}
		
		if (found == false)
		{
			send_error_message(user, ERR_NOSUCHCHANNEL, channel);
			continue;
		}
		
		if (next == string::npos)
			break;
	}
}

void IRC_Server::parse_privmsg(User* user, std::vector<std::string> parts)
{
	using namespace std;
	
	if (parts.size() != 3)
		return;
	
	string result = ":";
	result += user->nick;
	result += "!";
	result += user->username;
	result += "@";
	result += user->hostname;
	
	result += " PRIVMSG ";
	
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
		message += irc_ending;
		
		for (vector<Channel*>::iterator it = channels.begin();it != channels.end();it++)
		{
			if ((*it)->name == channel)
			{
				vector<Channel*>::iterator user_it = find(user->channels.begin(), user->channels.end(), *it);
				
				if (user_it != user->channels.end())
				{
					vector<User*>::iterator channel_it = find((*it)->users.begin(), (*it)->users.end(), user);
					
					if (channel_it != (*it)->users.end())
					{
						vector<User*> c_users = (*it)->users;
						
						c_users.erase(find(c_users.begin(),c_users.end(),user));
						
						broadcast_message(message, c_users);
					}
				}
				
				break;
			}
		}
		
		if (next == string::npos)
			break;
	}
}

void IRC_Server::parse_list(User* user, std::vector<std::string> parts)
{
	using namespace std;
	
	if (parts.size() != 1)
		return;
	
	string begin = ":" + hostname + " 32";
	
	string start_of_list = begin + "1 " + user->nick + " Channel :Users  Name" + irc_ending;
	
	string list_message_start = begin + "2 " + user->nick + " ";
	
	string end_of_list = begin + "3 " + user->nick + " :End of /LIST" + irc_ending;
	
	send_message(start_of_list, user);
	
	for (vector<Channel*>::iterator it = channels.begin();it != channels.end();it++)
	{
		Channel* chan = (*it);
		
		stringstream ss;
		ss << chan->users.size();
		
		string message = list_message_start + chan->name + " " + ss.str() + " :";
		
		string modes = "[+";
		
		for (vector<char>::iterator jt = chan->modes.begin();jt != chan->modes.end();jt++)
			modes += *jt;
		
		modes += "]";
		
		message += modes;
		
		message += " " + chan->topic + irc_ending;
		
		send_message(message, user);
	}
	
	send_message(end_of_list, user);
}

void IRC_Server::parse_names(User* user, std::vector<std::string> parts)
{
	using namespace std;
	
	if (parts.size() == 0 || parts.size() > 2)
		return;
	
	string channel = "*";
	
	if (parts.size() == 2)
		channel = parts[1];
	
	if (channel.find(",") != string::npos)
	{
		send_error_message(user, ERR_TOOMANYTARGETS, channel);
		return;
	}
	
	if (channel != "*" && channel[0] == '#')
	{
		string list_of_users = ":" + hostname + " 353 " + user->nick + " = " + channel + " :";
		
		for (vector<Channel*>::iterator it = channels.begin();it != channels.end();it++)
		{
			Channel* chan;
			
			if (chan->name.size() != channel.size())
				continue;
			
			if (strncasecmp(chan->name.c_str(), channel.c_str(), channel.size()) == 0)
			{
				string users = "";
				
				bool found = false;
				
				for (vector<User*>::iterator jt = chan->users.begin();jt != chan->users.end();jt++)
				{
					User* usr = *jt;
					
					users += usr->nick;
					if ((jt + 1) != chan->users.end())
						users += " ";
					
					if (user->nick.size() != user->nick.size())
						continue;
					
					if (strncasecmp(usr->nick.c_str(), user->nick.c_str(), usr->nick.size()) == 0)
						found = true;
				}
				
				if (found)
					list_of_users += users + irc_ending;
				
				break;
			}
		}
		
		send_message(list_of_users, user);
	}
	
	string end_of_names = ":" + hostname + " 366 " + user->nick + " " + channel + " :End of /NAMES list." + irc_ending;
	send_message(end_of_names, user);
}

void IRC_Server::parse_quit(User* user, std::vector<std::string> parts)
{
	using namespace std;
	
	string quit_message = "ERROR :Closing Link: " + user->nick + "[" + user->hostname + "] (Quit: ";
	
	string quit = "";
	
	if (parts.size() == 1)
		quit += user->nick;
	else
	{
		for (vector<string>::iterator it = parts.begin()+1;it != parts.end();it++)
			quit += *it;
	}
	
	quit_message += quit;
	
	quit_message += ")";
	quit_message += irc_ending;
	
	send_message(quit_message, user);
	
	User* temp_user = new User;
	temp_user->channels = user->channels;
	temp_user->nick = user->nick;
	temp_user->username = user->username;
	temp_user->hostname = user->hostname;
	
	disconnect_client(user->socket);
	
	string output = ":" + temp_user->nick + "!" + temp_user->username + "@" + temp_user->hostname + " QUIT :" + quit + irc_ending;
	
	broadcast_message(output, temp_user->channels);
}

/*
enum Error_Type
{
	ERR_NOSUCHNICK = 401,
	ERR_NOSUCHSERVER = 402,
	ERR_NOSUCHCHANNEL = 403,
	ERR_CANNOTSENDTOCHAN = 404,
	ERR_TOOMANYCHANNELS = 405,
	ERR_WASNOSUCHNICK = 406,
	ERR_TOOMANYTARGETS = 407,
	ERR_NOORIGIN = 409,
	ERR_NORECIPIENT = 411,
	ERR_NOTEXTTOSEND = 412,
	ERR_NOTOPLEVEL = 412,
	ERR_WILDTOPLEVEL = 414,
	ERR_UNKNOWNCOMMAND = 421,
	ERR_NOMOTD = 422,
	ERR_NOADMININFO = 423,
	ERR_FILEERROR = 424,
	ERR_NONICKNAMEGIVEN = 431,
	ERR_ERRONEUSNICKNAME = 431,
	ERR_NICKNAMEINUSE = 433,
	ERR_NICKCOLLISION = 436,
	ERR_USERNOTINCHANNEL = 441,
	ERR_NOTONCHANNEL = 442,
	ERR_USERONCHANNEL = 443,
	ERR_NOLOGIN = 444,
	ERR_SUMMONDISABLED = 445,
	ERR_USERSDISABLED = 446,
	ERR_NOTREGISTERED = 451,
	ERR_NEEDMOREPARAMS = 461,
	ERR_ALREADYREGISTRED = 461,
	ERR_NOPERMFORHOST = 463,
	ERR_PASSWDMISMATCH = 464,
	ERR_YOUREBANNEDCREEP = 465,
	ERR_KEYSET = 467,
	ERR_CHANNELISFULL = 471,
	ERR_UNKNOWNMODE = 472,
	ERR_INVITEONLYCHAN = 472,
	ERR_BANNEDFROMCHAN = 474,
	ERR_BADCHANNELKEY = 475,
	ERR_NOPRIVILEGES = 481,
	ERR_CHANOPPRIVSNEEDED = 482,
	ERR_CANTKILLSERVER = 483,
	ERR_NOOPERHOST = 491,
	ERR_UMODEUNKNOWNFLAG = 501,
	ERR_USERSDONTMATCH = 502
};
*/

void IRC_Server::send_error_message(User* user, Error_Type error, std::string arg1, std::string arg2)
{
	using namespace std;
	
	if (user == NULL)
		return;
	
	stringstream ss;
	ss << error;
	
	string output = ":" + hostname + " " + ss.str() + " " + user->nick + " ";
	
	switch (error)
	{
		case ERR_NOSUCHNICK:
		{
			output += arg1 + " :No such nick/channel";
			break;
		}
		case ERR_NOSUCHSERVER:
		{
			output += arg1 + " :No such server";
			break;
		}
		case ERR_NOSUCHCHANNEL:
		{
			output += arg1 + " :No such channel";
			break;
		}
		case ERR_CANNOTSENDTOCHAN:
		{
			output += arg1 + " :Cannot send to channel";
			break;
		}
		case ERR_TOOMANYCHANNELS:
		{
			output += arg1 + " :You have joined too many channels";
			break;
		}
		case ERR_WASNOSUCHNICK:
		{
			output += arg1 + " :There was no such nickname";
			break;
		}
		case ERR_TOOMANYTARGETS:
		{
			output += arg1 + " :Duplicate recipients. No message delivered";
			break;
		}
		case ERR_NOORIGIN:
		{
			output += ":No origin specified";
			break;
		}
		case ERR_NORECIPIENT:
		{
			output += ":No recipient given (" + arg1 + ")";
			break;
		}
		case ERR_NOTEXTTOSEND:
		{
			output += ":No text to send";
			break;
		}
		case ERR_NOTOPLEVEL:
		{
			output += arg1 + ":No toplevel domain specified";
			break;
		}
		case ERR_WILDTOPLEVEL:
		{
			output += arg1 + " :Wildcard in toplevel domain";
			break;
		}
		case ERR_UNKNOWNCOMMAND:
		{
			output += arg1 + " :Unknown command";
			break;
		}
		case ERR_NOMOTD:
		{
			output += ":MOTD File is missing";
			break;
		}
		case ERR_NOADMININFO:
		{
			output += arg1 + " :No administrative info available";
			break;
		}
		case ERR_FILEERROR:
		{
			output += ":File error doing " + arg1 + " on " + arg2;
			break;
		}
		case ERR_NONICKNAMEGIVEN:
		{
			output += ":No nickname given";
			break;
		}
		case ERR_ERRONEUSNICKNAME:
		{
			output += arg1 + " :Erroneus nickname";
			break;
		}
		case ERR_NICKNAMEINUSE:
		{
			output += arg1 + " :Nickname is already in use";
			break;
		}
		case ERR_NICKCOLLISION:
		{
			output += arg1 + " :Nickname collision KILL";
			break;
		}
		case ERR_USERNOTINCHANNEL:
		{
			output += arg1 + " " + arg2 + " :They aren't on that channel";
			break;
		}
		case ERR_NOTONCHANNEL:
		{
			output += arg1 + " :You're not on that channel";
			break;
		}
		case ERR_USERONCHANNEL:
		{
			output += arg1 + " " + arg2 + " :is already on channel";
			break;
		}
		case ERR_NOLOGIN:
		{
			output += arg1 + " :User not logged in";
			break;
		}
		case ERR_SUMMONDISABLED:
		{
			output += ":SUMMON has been disabled";
			break;
		}
		case ERR_USERSDISABLED:
		{
			output += ":USERS has been disabled";
			break;
		}
		case ERR_NOTREGISTERED:
		{
			output += ":You have not registered";
			break;
		}
		case ERR_NEEDMOREPARAMS:
		{
			output += arg1 + " :Not enough parameters";
			break;
		}
		case ERR_ALREADYREGISTRED:
		{
			output += ":You may not reregister";
			break;
		}
		case ERR_NOPERMFORHOST:
		{
			output += ":Your host isn't among the privileged";
			break;
		}
		case ERR_PASSWDMISMATCH:
		{
			output += ":Password incorrect";
			break;
		}
		case ERR_YOUREBANNEDCREEP:
		{
			output += ":You are banned from this server";
			break;
		}
		case ERR_KEYSET:
		{
			output += arg1 + " :Channel key already set";
			break;
		}
		case ERR_CHANNELISFULL:
		{
			output += arg1 + " :Cannot join channel (+l)";
			break;
		}
		case ERR_UNKNOWNMODE:
		{
			output += arg1 + " :is unknown mode char to me";
			break;
		}
		case ERR_INVITEONLYCHAN:
		{
			output += arg1 + " :Cannot join channel (+i)";
			break;
		}
		case ERR_BANNEDFROMCHAN:
		{
			output += arg1 + " :Cannot join channel (+b)";
			break;
		}
		case ERR_BADCHANNELKEY:
		{
			output += arg1 + " :Cannot join channel (+k)";
			break;
		}
		case ERR_NOPRIVILEGES:
		{
			output += ":Permission Denied- You're not an IRC operator";
			break;
		}
		case ERR_CHANOPPRIVSNEEDED:
		{
			output += arg1 + " :You're not channel operator";
			break;
		}
		case ERR_CANTKILLSERVER:
		{
			output += ":You can't kill a server!";
			break;
		}
		case ERR_NOOPERHOST:
		{
			output += ":No O-lines for your host";
			break;
		}
		case ERR_UMODEUNKNOWNFLAG:
		{
			output += ":Unknown MODE flag";
			break;
		}
		case ERR_USERSDONTMATCH:
		{
			output += ":Can't change mode for other users";
			break;
		}
		default:
		{
			cerr << "Invalid error code: " << error << endl;
			return;
		}
	}
	
	output += irc_ending;
	
	send_message(output, user);
}
