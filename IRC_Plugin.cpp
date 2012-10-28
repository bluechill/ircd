#include "IRC_Plugin.h"

IRC_Plugin::IRC_Plugin(std::string path, IRC_Server* link, bool isService)
: Plugin(path)
{
	if (!is_valid())
		return;
	
	this->link = link;
	
	void* handle = get_plugin_handle();
	
	void* init_handle = dlsym(handle, "init");
	if (init_handle != NULL)
	{
		init = (init_function)init_handle;
		
		if (!isService)
			init(link);
	}
	else
		init = NULL;
	
	void* dealloc_handle = dlsym(handle, "dealloc");
	if (dealloc_handle != NULL)
		dealloc = (dealloc_function)dealloc_handle;
	else
		dealloc = NULL;
	
	void* nop_handle = dlsym(handle, "name_of_plugin");
	if (nop_handle == NULL)
	{
		valid = false;
		return;
	}
	
	nop = (name_of_plugin_function)nop_handle;
	
	if (isService)
		return;
	
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
{
	if (dealloc)
		dealloc();
}

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

std::string IRC_Plugin::get_name_of_plugin(bool absolute)
{
	if (!valid)
		return "FAILURE";
	
	if (absolute)
		return nop();
	
	return "Plugin (" + nop() + ")";
}
