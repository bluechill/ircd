#include "IRC_Server.h"

#include "IRC_Plugin.h"
#include "IRC_Service.h"

#include <iostream>
#include <vector>
#include <string>
#include <sstream>

#include <algorithm>

#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdlib.h>

#include <time.h>
#include <unistd.h>

#include <errno.h>

#include <assert.h>

#ifdef __MACH__

#include <mach/mach_time.h>
#define ORWL_NANO (+1.0E-9)
#define ORWL_GIGA UINT64_C(1000000000)

static double orwl_timebase = 0.0;
static uint64_t orwl_timestart = 0;

#endif

struct timespec IRC_Server::get_current_time()
{
	struct timespec t;
#ifdef __MACH__
	// be more careful in a multithreaded environement
	if (!orwl_timestart) {
		mach_timebase_info_data_t tb = { 0 };
		mach_timebase_info(&tb);
		orwl_timebase = tb.numer;
		orwl_timebase /= tb.denom;
		orwl_timestart = mach_absolute_time();
	}
	
	double diff = (mach_absolute_time() - orwl_timestart) * orwl_timebase;
	t.tv_sec = diff * ORWL_NANO;
	t.tv_nsec = diff - (t.tv_sec * ORWL_GIGA);
#else
	clock_gettime(CLOCK_MONOTONIC, &t);
#endif
	
	return t;
}

const std::string IRC_Server::irc_ending = "\r\n";

IRC_Server::IRC_Server(std::vector<std::string> &arguments)
{
	using namespace std;
	
	srand(time(NULL));
	
	pthread_mutex_init(&message_mutex, NULL);
	
	struct addrinfo hints, *info;
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
		cerr << "Get Address Info Error (LIST): '" << gai_strerror(gai_result) << "'" << endl;
		
		exit(1);
	}
	
	this->hostname = info->ai_canonname;
	
	conf = NULL;
	
	for (int i = 0;i < arguments.size();i++)
	{
		if (arguments[i] == "-config")
		{
			if (arguments.size() < (i+1))
			{
				cerr << "Must supply file name with the -config argument." << endl;
				continue;
			}
			
			if (conf)
			{
				delete conf;
				conf = NULL;
			}
			
			i++;
			
			conf = new Config(arguments[i]);
			if (!conf->open())
			{
				cerr << "Invalid config file '" << arguments[i+1] << endl;
				delete conf;	
				conf = NULL;
				continue;
			}
		}
		else if (arguments[i] == "-help" || arguments[i] == "-h")
		{
			cout << "IRCd Help: " << endl;
			cout << "\t-config\tSpecifies the config file to use.  Default is 'ircd.conf'" << endl;
			cout << "End of IRCd Help." << endl;
			exit(0);
		}
	}
	
	if (conf == NULL)
	{
		conf = new Config("ircd.conf");
		if (!conf->open())
		{
			cerr << "Unable to open any config.  Please make sure to specify the config using -config or rename/create the config file to be 'ircd.conf'" << endl;
			exit(1);
		}
	}
	
	string s_SSLEnabled = conf->getElement("SSL", 0, 0);
	string s_DualSSL = conf->getElement("DualSSL", 0, 0);
	string s_SSLOnly = conf->getElement("SSLOnly", 0, 0);
	
	string s_SSLPort = conf->getElement("SSLPort", 0, 0);
	string s_Cert = conf->getElement("SSLCertificate", 0, 0);
	string s_Key = conf->getElement("SSLKey", 0, 0);
	
	string s_Port = conf->getElement("Port", 0, 0);
	
	string s_IPv4Only = conf->getElement("IPv4Only", 0, 0);
	string s_IPv6Only = conf->getElement("IPv6Only", 0, 0);
	string s_DualIPv4IPv6 = conf->getElement("DualIPv4IPv6", 0, 0);
	
	ssl_enabled = false;
	dual_ssl = false;
	
	if (s_SSLEnabled.size() != 0)
	{
		if (strncasecmp(s_SSLEnabled.c_str(), "YES", 3) == 0)
			ssl_enabled = true;
		else if (strncasecmp(s_SSLEnabled.c_str(), "NO", 2) == 0)
			ssl_enabled = false;
		else
		{
			ssl_enabled = false;
			
			cout << "WARNING: Ignoring invalid SSL Enabled value of '" << s_SSLEnabled << "'" << endl;
		}
	}
	
	if (s_DualSSL.size() != 0)
	{
		if (strncasecmp(s_DualSSL.c_str(), "YES", 3) == 0)
			dual_ssl = true;
		else if (strncasecmp(s_DualSSL.c_str(), "NO", 2) == 0)
			dual_ssl = false;
		else
		{
			dual_ssl = false;
			
			cerr << "WARNING: Ignoring invalid Dual SSL value of '" << s_DualSSL << "'" << endl;
		}
	}
	
	bool ssl_only = false;
	
	if (s_SSLOnly.size() != 0)
	{
		if (strncasecmp(s_SSLOnly.c_str(), "YES", 3) == 0)
			ssl_only = true;
		else if (strncasecmp(s_SSLOnly.c_str(), "NO", 2) == 0)
			ssl_only = false;
		else
		{
			ssl_only = false;
			
			cerr << "WARNING: Ignoring invalid SSL Only value of '" << s_DualSSL << "'" << endl;
		}
	}
	
	int ssl_port = 0;
	if (s_SSLPort.size() != 0)
	{
		stringstream ss;
		ss << s_SSLPort;
		ss >> ssl_port;
		
		for (int i = 0;i < s_SSLPort.size();i++)
		{
			if (!isdigit(s_SSLPort[i]))
			{
				ssl_port = 0;
				cerr << "Ignoring invalid SSL Port value, '" << s_SSLPort << "' Must contain only digits!" << endl;
				break;
			}
		}
	}
	
	int port = 0;
	if (s_Port.size() != 0)
	{
		stringstream ss;
		ss << s_Port;
		ss >> port;
		
		for (int i = 0;i < s_Port.size();i++)
		{
			if (!isdigit(s_Port[i]))
			{
				ssl_port = 0;
				cerr << "Ignoring invalid Port value, '" << s_Port << "' Must contain only digits!" << endl;
				break;
			}
		}
	}
	
	if (dual_ssl && ssl_only)
	{
		cerr << "Conflicting arguments! Cannot be both a dual ssl server and an ssl only server!" << endl;
		exit(3);
	}
	
	bool IPv4Only = false;
	bool IPv6Only = false;
	bool DualIPv4IPv6 = false;
	
	if (s_IPv4Only.size() != 0)
	{
		if (strncasecmp(s_IPv4Only.c_str(), "YES", 3) == 0)
			IPv4Only = true;
		else if (strncasecmp(s_IPv4Only.c_str(), "NO", 2) == 0)
			IPv4Only = false;
		else
		{
			IPv4Only = false;
			
			cout << "WARNING: Ignoring invalid IPv4 Only value of '" << s_IPv4Only << "'" << endl;
		}
	}
	
	if (s_IPv6Only.size() != 0)
	{
		if (strncasecmp(s_IPv6Only.c_str(), "YES", 3) == 0)
			IPv6Only = true;
		else if (strncasecmp(s_IPv6Only.c_str(), "NO", 2) == 0)
			IPv6Only = false;
		else
		{
			IPv6Only = false;
			
			cerr << "WARNING: Ignoring invalid IPv6 Only value of '" << s_IPv6Only << "'" << endl;
		}
	}
	
	if (s_DualIPv4IPv6.size() != 0)
	{
		if (strncasecmp(s_DualIPv4IPv6.c_str(), "YES", 3) == 0)
			DualIPv4IPv6 = true;
		else if (strncasecmp(s_DualIPv4IPv6.c_str(), "NO", 2) == 0)
			DualIPv4IPv6 = false;
		else
		{
			DualIPv4IPv6 = false;
			
			cerr << "WARNING: Ignoring invalid Dual IPv4 IPv6 value of '" << s_DualIPv4IPv6 << "'" << endl;
		}
	}
	
	if (IPv4Only && IPv4Only)
	{
		cerr << "You may only set one of IPv4Only or IPv6Only to 'YES'!" << endl;
		exit(6);
	}
	
	if ((IPv4Only || IPv6Only) && DualIPv4IPv6)
	{
		cerr << "You may not set IPv4Only or IPv6Only to YES when also setting DualIPv4IPv6!" << endl;
		exit(7);
	}
	
	Server::Server_Type type;
	
	if (IPv4Only)
		type = Server::IPv4_Server;
	else if (IPv6Only)
		type = Server::IPv6_Server;
	else if (DualIPv4IPv6)
		type = Server::Dual_IPv4_IPv6_Server;
	else
	{
		cerr << "Defaulting to Dual IPv4 and IPv6 server" << endl;
		type = Server::Dual_IPv4_IPv6_Server;
	}
	
	string s_Verbose = conf->getElement("Verbose", 0, 0);
	
	if (s_Verbose.size() != 0)
	{
		if (strncasecmp(s_Verbose.c_str(), "YES", 3) == 0)
			verbose = true;
		else if (strncasecmp(s_Verbose.c_str(), "NO", 2) == 0)
			verbose = false;
		else
		{
			verbose = false;
			
			cerr << "WARNING: Ignoring invalid Verbose value of '" << s_Verbose << "'" << endl;
		}
	}
	else
		verbose = false;
	
	if (ssl_enabled || dual_ssl || ssl_only)
	{
		if (s_Cert.size() == 0)
		{
			cerr << "You must specify a certificate file for an SSL server!" << endl;
			exit(4);
		}
		
		if (s_Key.size() == 0)
		{
			cerr << "You must specify a key file for an SSL server!" << endl;
			exit(5);
		}
		
		ssl_server = new Server(ssl_port, type, true, s_Cert, s_Key, this, verbose);
	}
	else
		ssl_server = NULL;
	
	if (dual_ssl || !ssl_enabled || !ssl_only)
		server = new Server(port, type, this, verbose);
	else
		server = NULL;
	
	int last_line = 0;
	for (int i = 0;;i++)
	{
		int current_line = 0;
		string contents = conf->getElement("Plugin", last_line, &current_line);
		
		if (contents == "")
			break;
		
		last_line = current_line+1;
		
		IRC_Plugin* plugin = new IRC_Plugin(contents, this, false);
		
		if (!plugin->is_valid())
		{
			delete plugin;
			continue;
		}
		
		plugins.push_back(plugin);
	}
	
	last_line = 0;
	for (int i = 0;;i++)
	{
		int current_line = 0;
		string contents = conf->getElement("Service", last_line, &current_line);
		
		if (contents == "")
			break;
		
		last_line = current_line+1;
		
		IRC_Service* plugin = new IRC_Service(contents, this);
		
		if (!plugin->is_valid())
		{
			delete plugin;
			continue;
		}
		
		plugins.push_back(plugin);
	}
	
	last_line = 0;
	for (int i = 0;;i++)
	{
		int current_line = 0;
		string plugins_directory = conf->getElement("PluginDirectory", last_line, &current_line);
		
		if (plugins_directory == "")
			break;
		
		last_line = current_line+1;
		
		vector<string> files;
		int status = getdir(plugins_directory, files);
		
		if (status != 0)
		{
			cerr << "Error getting the files in the directory specified for plugins." << endl;
			continue;
		}
		
		for (vector<string>::iterator it = files.begin();it != files.end();it++)
		{
			string path = *it;
			
			bool isService = false;
			
			if (!has_extension(path, ".plugin") && !has_extension(path, ".service"))
				continue;
			else if (has_extension(path, ".service"))
				isService = true;
			
			IRC_Plugin* plugin;
			
			if (!isService)
				plugin = new IRC_Plugin(path, this, false);
			else
				plugin = new IRC_Service(path, this);
			
			if (!plugin->is_valid())
			{
				delete plugin;
				continue;
			}
			
			plugins.push_back(plugin);
		}
	}
	
	last_line = 0;
	for (int i = 0;;i++)
	{
		int current_line = 0;
		string plugins_directory = conf->getElement("ServiceDirectory", last_line, &current_line);
		
		if (plugins_directory == "")
			break;
		
		last_line = current_line+1;
		
		vector<string> files;
		int status = getdir(plugins_directory, files);
		
		if (status != 0)
		{
			cerr << "Error getting the files in the directory specified for services." << endl;
			continue;
		}
		
		for (vector<string>::iterator it = files.begin();it != files.end();it++)
		{
			string path = *it;
						
			if (!has_extension(path, ".service"))
				continue;
			
			IRC_Service* plugin = new IRC_Service(path, this);
			
			if (!plugin->is_valid())
			{
				delete plugin;
				continue;
			}
			
			plugins.push_back(plugin);
		}
	}
}

IRC_Server::~IRC_Server()
{
	pthread_exit(NULL);
	pthread_mutex_destroy(&message_mutex);
	
	delete conf;
	
	if (server)
		delete server;
	
	if (ssl_server)
		delete ssl_server;
	
	for (std::vector<User*>::iterator it = users.begin();it != users.end();it++)
		delete *it;
	
	for (std::vector<Channel*>::iterator it = channels.begin();it != channels.end();it++)
		delete *it;
	
	for (std::vector<IRC_Plugin*>::iterator it = plugins.begin();it != plugins.end();it++)
		delete *it;
}

void IRC_Server::on_client_connect(Server_Client_ID &client, Server* server)
{
	using namespace std;
	
	User* new_user = new User;
	new_user->nick = "";
	new_user->username = "";
	new_user->realname = "";
	new_user->client = client;
	new_user->hasRegistered = false;
	new_user->isService = false;
	
	assert(client >= 0);
	
	struct timespec current_time;
	
	current_time = get_current_time();
	
	new_user->ping_timer = (double(current_time.tv_sec) + double(current_time.tv_nsec)/double(1E9)) + 60;
	
	struct sockaddr_storage addr;
	socklen_t length = sizeof(addr);
	
	getpeername(server->client_id_to_socket(client), (struct sockaddr*)&addr, &length);
	
	struct hostent *hp;
	
	if (addr.ss_family == AF_INET)
	{
		struct sockaddr_in *s = (struct sockaddr_in *)&addr;
		
		if ((hp = gethostbyaddr((char *)&s->sin_addr, sizeof(s->sin_addr), AF_INET)))
			new_user->hostname = hp->h_name;
		else
		{
			cerr << "Error: " << hstrerror(h_errno) << endl;
			server->disconnect_client(client);
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
			server->disconnect_client(client);
			return;
		}
	}
	
	lock_message_mutex();
	users.push_back(new_user);
	unlock_message_mutex();
	
	for (vector<IRC_Plugin*>::iterator it = plugins.begin();it != plugins.end();it++)
	{
		IRC_Plugin* plugin = *it;
		
		vector<IRC_Plugin::Call_Type> types = plugin->get_supported_calls();
		
		vector<IRC_Plugin::Call_Type>::iterator types_it = find(types.begin(), types.end(), IRC_Plugin::ON_CLIENT_CONNECT);
		
		if (types_it != types.end())
		{
			vector<string> parts;
			IRC_Plugin::Result_Of_Call result = plugin->plugin_call(IRC_Plugin::ON_CLIENT_CONNECT, new_user, parts);
			
			if (result == IRC_Plugin::FAILURE)
				cerr << "Plugin failed (ON_CLIENT_CONNECT), see log for more details." << endl;
		}
	}
}

void IRC_Server::on_client_disconnect(Server_Client_ID &client, Server* server)
{
	using namespace std;
	
	lock_message_mutex();
	
	for (std::vector<User*>::iterator it = users.begin();it != users.end();it++)
	{
		if ((*it)->client == client)
		{
			for (vector<IRC_Plugin*>::iterator jt = plugins.begin();jt != plugins.end();jt++)
			{
				IRC_Plugin* plugin = *jt;
				
				vector<IRC_Plugin::Call_Type> types = plugin->get_supported_calls();
				
				vector<IRC_Plugin::Call_Type>::iterator types_it = find(types.begin(), types.end(), IRC_Plugin::ON_CLIENT_DISCONNECT);
				
				if (types_it != types.end())
				{
					vector<string> parts;
					unlock_message_mutex();
					IRC_Plugin::Result_Of_Call result = plugin->plugin_call(IRC_Plugin::ON_CLIENT_DISCONNECT, *it, parts);
					lock_message_mutex();
					
					if (result == IRC_Plugin::FAILURE)
						cerr << "Plugin failed (ON_CLIENT_DISCONNECT), see log for more details." << endl;
				}
			}
			
			delete *it;
			users.erase(it);
			break;
		}
	}
	unlock_message_mutex();
}

void IRC_Server::recieve_message(std::string &message, Server_Client_ID &client, Server* server)
{
	using namespace std;
	
	if (verbose)
		cout << message;
	
	if (message.find("\n") == string::npos)
	{
		if (verbose)
			cout << " <-- is an invalid command, missing trailing \n." << endl;
		
		return;
	}
	
	parse_message(message, client);
}

void IRC_Server::send_message(std::string &message, User* user)
{
	if (user == NULL)
		return;
	
	if (verbose)
		std::cout << message;
	
	if (server->client_id_to_socket(user->client) != -1)
		server->send_message(message, user->client);
	else if (ssl_server->client_id_to_socket(user->client) != -1)
		ssl_server->send_message(message, user->client);
	else
		std::cerr << "WARNING: Invalid client: '" << user->nick << "':" << user->hostname << " found while trying to send message: '" << message << "'" << std::endl;
}

void IRC_Server::broadcast_message(std::string &message, std::vector<User*> users, bool lock_message_mutex)
{
	if (lock_message_mutex)
		unlock_message_mutex();
	
	for (std::vector<User*>::iterator it = users.begin();it != users.end();it++)
		send_message(message, *it);
	
	if (lock_message_mutex)
		unlock_message_mutex();
}

void IRC_Server::broadcast_message(std::string &message, Channel* users, bool lock_message_mutex)
{
	broadcast_message(message, users->users, lock_message_mutex);
}

void IRC_Server::broadcast_message(std::string &message, std::vector<Channel*> channels, bool lock_message_mutex)
{
	for (std::vector<Channel*>::iterator it = channels.begin();it != channels.end();it++)
		broadcast_message(message, (*it), lock_message_mutex);
}

IRC_Server::Message_Type IRC_Server::string_to_message_type(std::string &message)
{
	if (strncasecmp(message.c_str(), "NICK", message.size()) == 0)
		return NICK;
	else if (strncasecmp(message.c_str(), "USER", message.size()) == 0)
		return USER;
	else if (strncasecmp(message.c_str(), "PONG", message.size()) == 0)
		return PONG;
	else if (strncasecmp(message.c_str(), "QUIT", message.size()) == 0)
		return QUIT;
	
	return UNKNOWN;
}

IRC_Server::User* IRC_Server::client_to_user(Server_Client_ID &client)
{
	unlock_message_mutex();
	for (std::vector<User*>::iterator it = users.begin();it != users.end();it++)
	{
		if ((*it)->client == client)
		{
			unlock_message_mutex();
			return *it;
		}
	}
	unlock_message_mutex();
	
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

void IRC_Server::parse_message(std::string &message, Server_Client_ID &client)
{
	using namespace std;
	
	assert(client >= 0);
	
	int to_erase = 2;
	if (message.find('\r') == string::npos)
		to_erase = 1;
	
	message.erase(message.size()-to_erase, to_erase);
	
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
				message += *jt + " ";
			
			message.erase(message.size()-1);
			
			parts.erase(it, parts.end());
			parts.push_back(message);
			
			break;
		}
	}
	
	User* user = client_to_user(client);
	
	if (user == NULL)
	{
		cerr << "Invalid user socket for server! : '" << message << "'" << endl;
		return;
	}
	
	assert(user->client >= 0);
	
	Message_Type type = string_to_message_type(parts[0]);
	
	if ((user->nick.size() == 0 || user->username.size() == 0) && (type != NICK && type != USER && type != QUIT && type != PONG))
	{
		send_error_message(user, ERR_NOTREGISTERED);
		
		return;
	}
	
	bool parsed_message = false;
	for (vector<IRC_Plugin*>::iterator it = plugins.begin();it != plugins.end();it++)
	{
		IRC_Plugin* plugin = *it;
		
		IRC_Plugin::Result_Of_Call result = plugin->plugin_call(IRC_Plugin::BEFORE_MESSAGE_PARSE, user, parts);
		
		if (result == IRC_Plugin::FAILURE)
			cerr << "Plugin failed (BEFORE_MESSAGE_PARSE), see log for more details." << endl;
		else if (result == IRC_Plugin::PARSED)
			parsed_message = true;
	}
	
	if (!parsed_message)
	{
		bool handled = false;
		
		for (vector<IRC_Plugin*>::iterator it = plugins.begin();it != plugins.end();it++)
		{
			IRC_Plugin* plugin = *it;
			
			IRC_Plugin::Result_Of_Call result = plugin->plugin_call(IRC_Plugin::ON_RECIEVE_MESSAGE, user, parts);
			
			if (result == IRC_Plugin::FAILURE)
				cerr << "Plugin failed (ON_RECIEVE_MESSAGE), see log for more details." << endl;
			
			if (result == IRC_Plugin::HANDLED)
			{
				handled = true;
				break;
			}
		}
		
		if (!handled)
			cerr << "Recieved potentially invalid message: '" << message << "'" << endl;
	}
	
	for (vector<IRC_Plugin*>::iterator it = plugins.begin();it != plugins.end();it++)
	{
		IRC_Plugin* plugin = *it;
		
		IRC_Plugin::Result_Of_Call result = plugin->plugin_call(IRC_Plugin::AFTER_MESSAGE_PARSE, user, parts);
		
		if (result == IRC_Plugin::FAILURE)
			cerr << "Plugin failed (AFTER_MESSAGE_PARSE), see log for more details." << endl;
	}
	
	if ((type == NICK || type == USER) && user->username.size() != 0 && user->nick.size() != 0 && !user->hasRegistered)
	{
		user->hasRegistered = true;
		
		vector<std::string> parts;
		for (vector<IRC_Plugin*>::iterator it = plugins.begin();it != plugins.end();it++)
		{
			IRC_Plugin* plugin = *it;
			
			IRC_Plugin::Result_Of_Call result = plugin->plugin_call(IRC_Plugin::ON_CLIENT_REGISTERED, user, parts);
			
			if (result == IRC_Plugin::FAILURE)
				cerr << "Plugin failed (ON_CLIENT_REGISTERED), see log for more details." << endl;
		}
	}
}

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
		case ERR_NOSUCHSERVICE:
		{
			output += arg1 + " :No such service";
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
		case ERR_BADMASK:
		{
			output += arg1 + " :Bad Server/host mask";
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
		case ERR_UNAVAILRESOURCE:
		{
			output += arg1 + " :Nick/channel is temporarily unavailable";
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
		case ERR_YOUWILLBEBANNED:
		{
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
		case ERR_BADCHANMASK:
		{
			output += arg1 + " :Bad Channel Mask";
			break;
		}
		case ERR_NOCHANMODES:
		{
			output += arg1 + " :Channel doesn't support modes";
			break;
		}
		case ERR_BANLISTFULL:
		{
			output += arg1 + " " + arg2 + " :Channel list is full";
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
		case ERR_RESTRICTED:
		{
			output += ":Your connection is restricted!";
			break;
		}
		case ERR_UNQOPPRIVSNEEDED:
		{
			output += ":You're not the original channel operator";
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

IRC_Server::User* IRC_Server::get_user_from_string(std::string username)
{
	for (std::vector<User*>::iterator it = users.begin();it != users.end();it++)
	{
		User* iUser = *it;
		if (iUser->nick == username)
			return iUser;
	}
	
	return NULL;
}

IRC_Server::Channel* IRC_Server::get_channel_from_string(std::string channel_name)
{
	for (std::vector<Channel*>::iterator it = channels.begin();it != channels.end();it++)
	{
		Channel* iChannel = *it;
		if (iChannel->name == channel_name)
			return iChannel;
	}
	
	return NULL;
}

void IRC_Server::disconnect_client(Server_Client_ID &client, std::string reason)
{	
	if (server->client_id_to_socket(client) != -1)
	{
		last_disconnect_reason = reason;
		
		server->disconnect_client(client);
	}
	else if (ssl_server->client_id_to_socket(client) != -1)
	{
		last_disconnect_reason = reason;
		
		ssl_server->disconnect_client(client);
	}
}
