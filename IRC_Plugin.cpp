#include "IRC_Plugin.h"

IRC_Plugin::IRC_Plugin(std::string path, IRC_Server* link)
: Plugin(path)
{
	if (!is_valid())
		return;
	
	this->link = link;
	
	void* handle = get_plugin_handle();
	
	void* spf_handle = dlsym(handle, "get_supported_calls");
	if (spf_handle == NULL)
	{
		valid = false;
		return;
	}
	
	scf = (supported_calls_function)spf_handle;
	
	void* pcf_handle = dlsym(handle, "plugin_call");
	if (pcf_handle == NULL)
	{
		valid = false;
		return;
	}
	
	pcf = (plugin_call_function)pcf_handle;
}

IRC_Plugin::~IRC_Plugin()
{}

std::vector<IRC_Plugin::Call_Type> IRC_Plugin::get_supported_calls()
{
	if (!valid)
		return std::vector<Call_Type>();
	
	return scf();
}

IRC_Plugin::Result_Of_Call IRC_Plugin::plugin_call(Call_Type type, IRC_Server::User* user, std::vector<std::string> &parts)
{
	if (!valid)
		return FAILURE;
	
	return pcf(type, user, parts, link);
}
