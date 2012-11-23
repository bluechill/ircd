#include "IRC_Server.h"
#include "IRC_Plugin.h"

#include <string.h>

#include <vector>
#include <string>
#include <iostream>
#include <fstream>
#include <sstream>

std::vector<std::string> motd;
std::string server_name;
std::string network_name;

extern "C" void init(IRC_Server* link)
{
	using namespace std;
	
	string motd_path = link->get_config()->getElement("Motd", 0, 0);
	if (motd_path == "")
		motd_path = "motd.txt";
	
	ifstream motd_file(motd_path.c_str());
	if (motd_file.is_open())
	{
		string line;
		while (motd_file.good())
		{
			getline(motd_file, line);
			motd.push_back(line);
			line = "";
		}
		motd_file.close();
	}
	else
	{
		cout << "WARNING: No valid MOTD file found.  Check ircd.conf!" << endl;
		motd.push_back("No MOTD.");
	}
	
	server_name = link->get_hostname();
	network_name = link->get_config()->getElement("Network_Name", 0, 0);
}

extern "C" void dealloc()
{}

extern "C" std::vector<IRC_Plugin::Call_Type> get_supported_calls()
{
	std::vector<IRC_Plugin::Call_Type> types;
	types.push_back(IRC_Plugin::ON_CLIENT_REGISTERED);
	
	return types;
}

extern "C" std::string name_of_plugin()
{
	return "CONREG";
}

extern "C" IRC_Plugin::Result_Of_Call plugin_call(IRC_Plugin::Call_Type type, IRC_Server::User* user, std::vector<std::string> &parts, IRC_Server* link)
{
	using namespace std;
	
	if (type != IRC_Plugin::ON_CLIENT_REGISTERED)
		return IRC_Plugin::NOT_HANDLED;
	
	string prefix = ":" + server_name + " ";
	
	string zero_zero_one = prefix + "001 " + user->nick + " :Welcome to the " + network_name + " " + user->nick + "!" + user->username + "@" + user->hostname + IRC_Server::irc_ending;
	string zero_zero_two = prefix + "002 " + user->nick + " :Your host is " + server_name + ", running " + link->get_version_string() + IRC_Server::irc_ending;
	
	stringstream ss;
	ss << link->get_users()->size();
	
	string two_five_one = prefix + "251 " + user->nick + " :There are " + ss.str() + " users online." + IRC_Server::irc_ending;
	
	string three_seven_five = prefix + "375 " + user->nick + " :- " + server_name + " Message of the Day -" + IRC_Server::irc_ending;
	
	vector<string> motd_final;
	for (int i = 0;i < motd.size();i++)
		motd_final.push_back(prefix + "372 " + user->nick + " :- " + motd[i] + IRC_Server::irc_ending);
	
	string three_seven_six = prefix + "376 " + user->nick + " :End of /MOTD command." + IRC_Server::irc_ending;
	
	link->send_message(zero_zero_one, user);
	link->send_message(zero_zero_two, user);
	link->send_message(two_five_one, user);
	link->send_message(three_seven_five, user);
	
	for (int i = 0;i < motd_final.size();i++)
		link->send_message(motd_final[i], user);
	
	link->send_message(three_seven_six, user);
	
	return IRC_Plugin::HANDLED;
}
