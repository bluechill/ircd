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
				!(user->nick[0] == '\1' || user->username[0] == '\1'))
			{
				string ping_message = "PING :" + link->get_hostname() + IRC_Server::irc_ending;
				
				user->ping_timer -= 5;
				user->ping_contents = link->get_hostname();
				
				link->send_message(ping_message, user);
			}
			else if ((double_current_time - user_time) > double(55))
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
	new_user->nick.push_back('\1');
	new_user->username.push_back('\1');
	new_user->realname.push_back('\1');
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
	
	if ((user->nick[0] == '\1' || user->username[0] == '\1') && (type != NICK && type != USER && type != QUIT && type != PONG))
	{
		string output = ":" + hostname + " 451 " + parts[0] + " :You have not registered" + irc_ending;
		
		send_message(output, user);
		
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
		return;
	
	string new_nick = parts[1];
	
	pthread_mutex_lock(&ping_mutex);
	
	for (vector<User*>::iterator it = users.begin();it != users.end();it++)
	{
		User* exist_user = *it;
		
		if (exist_user->nick.size() != new_nick.size())
			continue;
		
		if (strncasecmp(exist_user->nick.c_str(), new_nick.c_str(), new_nick.size()) == 0)
		{
			string output = ":" + hostname + " 433 ";
			
			if (user->nick[0] == '\1')
				output += "*";
			else
				output += user->nick;
			
			output += " " + new_nick + " :Nickname is already in use." + irc_ending;
			
			send_message(output, user);
			
			pthread_mutex_unlock(&ping_mutex);
			
			return;
		}
	}
	
	pthread_mutex_unlock(&ping_mutex);
	
	string old_nick = user->nick;
	user->nick = new_nick;
	
	if (user->username[0] != '\1')
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
	
	if (parts.size() != 5 || user->username[0] != '\1')
		return;
	
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
		return;
	
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
			string output = ":" + hostname + " 403 " + user->nick + " " + channel + " :No such channel";
			
			send_message(output, user);
			
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
			string output = ":" + hostname + " 403 " + user->nick + " " + channel + " :No such channel";
			
			send_message(output, user);
			
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
				
				if ((*it)->users.size() == 0)
					it = channels.erase(it);
				
				found = true;
				
				break;
			}
		}
		
		if (found == false)
		{
			string output = ":" + hostname + " 403 " + user->nick + " " + channel + " :No such channel";
			
			send_message(output, user);
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
