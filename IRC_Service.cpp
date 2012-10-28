#include "IRC_Service.h"

IRC_Service::IRC_Service(std::string path, IRC_Server* link)
: IRC_Plugin(path, link, true)
{
	if (!is_valid())
		return;
	
	this->link = link;
	
	void* handle = get_plugin_handle();
	
	void* mhf_handle = dlsym(handle, "parse_message");
	if (mhf_handle == NULL)
	{
		valid = false;
		return;
	}
	
	mhf = (message_handler_function)mhf_handle;
	
	user_link = new IRC_Server::User;
	user_link->nick = "Service";
	user_link->username = "services";
	user_link->realname = "Service";
	user_link->hostname = link->get_hostname();
	
	user_link->socket = -1;
	
	user_link->ping_timer = -1;
	user_link->ping_contents = "";
	
	user_link->isService = true;
	
	link->lock_message_mutex();
	
	std::vector<IRC_Server::User*>* users = link->get_users();
	users->push_back(user_link);
	
	link->unlock_message_mutex();
	
	((service_init)init)(link, this);
}

IRC_Service::~IRC_Service()
{}

std::vector<IRC_Plugin::Call_Type> IRC_Service::get_supported_calls()
{
	if (!valid)
		return std::vector<Call_Type>();
	
	std::vector<IRC_Plugin::Call_Type> calls;
	calls.push_back(IRC_Plugin::ON_RECIEVE_MESSAGE);
	
	return calls;
}

IRC_Service::Result_Of_Call IRC_Service::plugin_call(Call_Type type, IRC_Server::User* user, std::vector<std::string> &parts)
{
	using namespace std;
	
	if (!valid)
		return IRC_Plugin::FAILURE;
	
	if (type != IRC_Plugin::ON_RECIEVE_MESSAGE)
		return IRC_Plugin::NOT_HANDLED;
	
	Service_Result result = mhf(user, parts, link, this);
	
	std::string final_result = ":" + get_nick() + "!" + user_link->username + "@" + link->get_hostname() + " ";
	
	if (result.type == IRC_Service::NOT_HANDLED)
		return IRC_Plugin::NOT_HANDLED;
	else if (result.type == IRC_Service::NO_RETURN)
		return IRC_Plugin::HANDLED;
	else if (result.type == IRC_Service::NOTICE)
		final_result += "NOTICE ";
	else if (result.type == IRC_Service::PRIVMSG)
		final_result += "PRIVMSG ";
	else
	{
		cerr << "Service Failure (" << get_name_of_plugin() << ") (Nick: " << get_nick() << "):" << "Invalid Message Type: " << type << "." << endl;
		return IRC_Plugin::FAILURE;
	}
	
	if (result.user_or_channel.size() == 0)
	{
		cerr << "Service Failure (" << get_name_of_plugin() << ") (Nick: " << get_nick() << "):" << "User or Channel Size is 0 (zero)." << endl;
		return IRC_Plugin::FAILURE;
	}
	
	final_result += result.user_or_channel;
	
	if (result.message.size() == 0)
	{
		cerr << "Service Failure (" << get_name_of_plugin() << ") (Nick: " << get_nick() << "):" << "Message returned is 0 (zero)." << endl;
		return FAILURE;
	}
	
	final_result += " :";
	
	final_result += result.message;
	final_result += IRC_Server::irc_ending;
	
	if (result.user_or_channel.find_first_of("#&", 0) == 0)
	{
		IRC_Server::Channel* channel_to_send_to = link->get_channel_from_string(result.user_or_channel);
		
		if (channel_to_send_to != NULL)
			link->broadcast_message(final_result, channel_to_send_to);
		else
		{
			cerr << "Service Failure (" << get_name_of_plugin() << ") (Nick: " << get_nick() << "):" << "No such channel called: '" << result.user_or_channel << endl;
			return FAILURE;
		}
	}
	else
	{
		IRC_Server::User* user_to_send_to = link->get_user_from_string(result.user_or_channel);
		
		if (user_to_send_to != NULL)
			link->send_message(final_result, user_to_send_to);
		else
		{
			cerr << "Service Failure (" << get_name_of_plugin() << ") (Nick: " << get_nick() << "):" << "No such user called: '" << result.user_or_channel << endl;
			return FAILURE;
		}
	}
	
	return HANDLED;
}

std::string IRC_Service::get_name_of_plugin()
{
	if (!valid)
		return "FAILURE";
	
	return "Service (" + this->IRC_Plugin::get_name_of_plugin(true) + ")";
}

std::string IRC_Service::get_nick()
{
	return user_link->nick;
}

void IRC_Service::set_nick(std::string nick)
{
	link->lock_message_mutex();
	
	user_link->nick = nick;
	
	link->unlock_message_mutex();
}

std::string IRC_Service::get_realname()
{
	return user_link->realname;
}

void IRC_Service::set_real_name(std::string realname)
{
	link->lock_message_mutex();
	
	user_link->realname = realname;
	
	link->unlock_message_mutex();
}
