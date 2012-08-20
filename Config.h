#ifndef CONFIG_H
#define CONFIG_H

//Include string for std::string
#include <string>

//Include iostream for i/o
#include <iostream>

//Include fstream for file i/o
#include <fstream>

#include <vector>

//Configuration class
class Config {
private:
	//Configuration file variable
    std::ofstream outputConfigFile;
    std::ifstream inputConfigFile;
    
    //Contents of the config file
    std::vector<std::string> file;
    
	std::string path;
	
    //Close a file
    void closeFiles();
public:
	//Initialize the config class with the path to a config file
	Config(std::string pathToConfigFile);
	
	//Dealloc method
	~Config();
    
    //Open a new configuration file
    void open(std::string pathToConfigFile);
	
	//Get an element from the config file (the part after the equals) returns "" if element doesn't exist
	std::string getElement(std::string element, int start_line = 0, int* line = NULL);
	//Set an element in the configuration file (will overwrite any existing element with the same name
	void setElement(std::string element, std::string contents);
};

int getdir(std::string dir, std::vector<std::string> &files);
bool has_extension(std::string to_check, std::string ext);

#endif
