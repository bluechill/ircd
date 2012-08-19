#include "Plugin.h"

Plugin::Plugin(std::string path)
{
	valid = false;
	
	handle = dlopen(path.c_str(), RTLD_NOW);
	if (handle == NULL)
		return;
	
	valid = true;
}

Plugin::~Plugin()
{
	dlclose(handle);
}
	
void* Plugin::get_plugin_handle()
{
	return handle;
}
	
bool Plugin::is_valid()
{
	return valid;
}
	
void* Plugin::get_function(std::string function)
{
	if (!valid)
		return NULL;
	
	void* function_handle = dlsym(handle, function.c_str());
	
	return function_handle;
}
