#ifndef PLUGIN_H
#define PLUGIN_H

#include <string>

#include <dlfcn.h>

class Plugin
{
public:
	Plugin(std::string path);
	~Plugin();
	
	void* get_plugin_handle();
	
	bool is_valid();
	
	void* get_function(std::string function);
private:
	void* handle;
	
protected:
	bool valid;
};

#endif
