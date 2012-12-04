#include "IRC_Server.h"
#include "IRC_Plugin.h"

#include <string.h>

#include <vector>
#include <string>

#include <errno.h>

static pthread_t ping_thread;

void* ping_thread_function(void* data)
{
	using namespace std;
	
	IRC_Server *link = reinterpret_cast<IRC_Server*>(data);
	vector<IRC_Server::User*>* users = link->get_users();
	
	struct timespec current_time;
	double double_current_time = 0.0;
	
	link->lock_message_mutex();
	
	for (vector<IRC_Server::User*>::iterator it = users->begin();it != users->end();it++)
		(*it)->ping_timer = -1;
	
	link->unlock_message_mutex();
	
	while (true)
	{
		sleep(1);
		
		current_time = link->get_current_time();
		
		double_current_time = (double(current_time.tv_sec) + double(current_time.tv_nsec)/double(1E9));
		
		link->lock_message_mutex();
		
		for (vector<IRC_Server::User*>::iterator it = users->begin();it != users->end();it++)
		{
			IRC_Server::User* user = *it;
			
			double user_time = user->ping_timer;
			
			if ((user_time - double_current_time) > double(30) &&
				(user_time - double_current_time) < double(35) &&
				!(user->nick.size() == 0 || user->username.size() == 0) &&
				user->ping_contents == "")
			{
				string ping_message = "PING :" + link->get_hostname() + IRC_Server::irc_ending;
				
				user->ping_timer = double_current_time + 30;
				user->ping_contents = link->get_hostname();

				link->send_message(ping_message, user);
			}
			else if ((user_time - double_current_time) < 0)
			{				
				string quit_message = "ERROR :Closing Link: " + user->nick + "[" + user->hostname + "] (Ping Timeout)" + IRC_Server::irc_ending;
				
				link->send_message(quit_message, user);
				
				IRC_Server::User* temp_user = new IRC_Server::User;
				temp_user->channels = user->channels;
				temp_user->nick = user->nick;
				temp_user->username = user->username;
				temp_user->hostname = user->hostname;
				
				link->unlock_message_mutex();
				link->disconnect_client(user->client, "PING");
				link->lock_message_mutex();
				it--;
				
				string output = ":" + temp_user->nick + "!" + temp_user->username + "@" + temp_user->hostname + " QUIT :Ping Timeout" + IRC_Server::irc_ending;
				
				link->broadcast_message(output, temp_user->channels, false);
			}
		}
		
		link->unlock_message_mutex();
	};
}

extern "C" void init(IRC_Server* link)
{
	using namespace std;
	
	int status = pthread_create(&ping_thread, NULL, ping_thread_function, link);
	if (status)
	{
		cerr << "Failed to create ping thread with error: " << status << " (\"" << strerror(errno) << "\")" << endl;
		throw Server::Thread_Initialization_Failure;
	}
}

extern "C" std::vector<IRC_Plugin::Call_Type> get_supported_calls()
{	
	std::vector<IRC_Plugin::Call_Type> types;
	types.push_back(IRC_Plugin::ON_RECIEVE_MESSAGE);
	
	return types;
}

extern "C" std::string name_of_plugin()
{
	return "PONG";
}

extern "C" IRC_Plugin::Result_Of_Call plugin_call(IRC_Plugin::Call_Type type, IRC_Server::User* user, std::vector<std::string> &parts, IRC_Server* link)
{
	using namespace std;
	
	if (type != IRC_Plugin::ON_RECIEVE_MESSAGE)
		return IRC_Plugin::NOT_HANDLED;
	
	if (strncasecmp(parts[0].c_str(), "PONG", 4) != 0)
		return IRC_Plugin::NOT_HANDLED;
	
	if (parts.size() != 2)
	{
		if (parts.size() == 1)
			link->send_error_message(user, IRC_Server::ERR_NEEDMOREPARAMS, "PONG");
		
		return IRC_Plugin::HANDLED;
	}
	
	link->lock_message_mutex();
	
	std::string contents = parts[1];
	
	if (user->ping_contents == contents)
	{
		struct timespec current_time;
		
		current_time = link->get_current_time();
		
		user->ping_timer  = (double(current_time.tv_sec) + double(current_time.tv_nsec)/double(1E9)) + 60;
		user->ping_contents = "";
	}
	
	link->unlock_message_mutex();
	
	return IRC_Plugin::HANDLED;
}
