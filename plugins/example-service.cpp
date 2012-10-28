#include "IRC_Server.h"
#include "IRC_Service.h"

#include <string.h>

#include <vector>
#include <string>

extern "C" std::string name_of_plugin()
{
	return "Example Service";
}

extern "C" void init(IRC_Server* link, IRC_Service* service)
{
	service->set_nick("ExampleServ");
	service->set_real_name("Example Service");
}

extern "C" IRC_Service::Service_Result parse_message(IRC_Server::User* user, std::vector<std::string> &parts, IRC_Server* link, IRC_Service* service)
{
	using namespace std;
	
	IRC_Service::Service_Result result;
	result.type = IRC_Service::NOT_HANDLED;
	
	if (parts.size() < 3)
		return result;
	
	if (parts[0] != "PRIVMSG")
		return result;
	
	if (parts[1] != service->get_nick())
		return result;
	
	result.message = "This is an Example Service to test out the functionality of the new IRC_Service extension.";
	result.user_or_channel = user->nick;
	result.type = IRC_Service::NOTICE;
	
	return result;
}
