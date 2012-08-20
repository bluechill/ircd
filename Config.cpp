#include "Config.h"

#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>

//Initialize the config class with the path to a config file
Config::Config(std::string pathToConfigFile)
{
    //Use the standard namespace
    using namespace std;
	
	path = pathToConfigFile;
	
    //Open the files
    inputConfigFile.open(path.c_str(), ios::in);
    //We don't open the input file because we'll be instead overwriting the entire file so we can just truncate it
    
    //Read the file into our vector
    if (inputConfigFile.is_open())
    {
        //Line to hold the line in the file
        string line;
        
        //While we aren't at the end of the file
        while (inputConfigFile.good())
        {
            //get the line
            getline(inputConfigFile, line);
            //and add it to the vector
            file.push_back(line);
        }
        
        //Close it because we don't need it anymore.
        closeFiles();
    }
    else
    {
        //Invalid  file so tell the user
        cout << "Unable to open file " << pathToConfigFile << endl;
    }
}

//Dealloc method
Config::~Config()
{
    //Close the files
    closeFiles();
}

void Config::open(std::string pathToConfigFile)
{
    //Use the standard namespace
    using namespace std;
	
	path = pathToConfigFile;
    
    //Make sure the input file is closed already
    closeFiles();
    
    //Clear our vector
    file.clear();
    
    //Open the files
    inputConfigFile.open(path.c_str(), ios::in);
    
    //Read the file into our vector
    if (inputConfigFile.is_open())
    {
        //Line to hold the line in the file
        string line;
        
        //While we aren't at the end of the file
        while (inputConfigFile.good())
        {
            //get the line
            getline(inputConfigFile, line);
            //and add it to the vector
            file.push_back(line);
        }
        
        //Close it because we don't need it anymore.
        closeFiles();
    }
}

//Get an element from the config file (the part after the equals) returns "" if element doesn't exist
std::string Config::getElement(std::string element, int start_line, int* element_line)
{
    //Use the standard namespace
    using namespace std;
    
    //Loop through the vector of strings searching for a match
    for (int i = start_line; i < file.size();i ++)
    {
        //Get the line to check from our vector
        string line = file[i];
        //Check to see if we have found it
        unsigned long positionInLine = line.find(element);
        //Make sure it exists in the string and is at the 1st (.at(0) ) position
        if (positionInLine != string::npos && positionInLine == 0)
        {
            //We have it
            unsigned long positionOfEquals = line.find("=");
            //Check to see if it is properly formatted
            if (positionOfEquals != string::npos)
            {
                //It is properly formatted
                //Get the contents after the equals
                string contents = line.substr(positionOfEquals + 1, line.size() - positionOfEquals);
                
				if (element_line != NULL)
					*element_line = i;
				
                //Return it
                return contents;
            }
        }
    }
    
    return "";
}

//Set an element in the configuration file (will overwrite any existing element with the same name
void Config::setElement(std::string element, std::string contents)
{
    //Use the standard namespace
    using namespace std;
    
    //Combine the two strings to make it formatted to be read
    string finalFormat = element + "=" + contents;
    
    long positionToInsertAt = file.size();
    //Loop through the vector to see if the element exists
    for (int i = 0;i < file.size();i++)
    {
        //Get the line
        string line = file[i];
        
        //Try to find the element
        long positionOfElement = line.find(element);
        
        //Check to see if we've found it and it's at the first position
        if (positionOfElement != string::npos && positionOfElement == 0)
        {
            //We have it
            
            //Set our position to insert the element at
            positionToInsertAt = i;
            
            //Remove the line in the file
            file.erase(file.begin() + positionToInsertAt);
            
            //Add the line
            file.insert(file.begin() + positionToInsertAt, finalFormat);
        }
    }
    
    //Check to see if we have already inserted the line
    if (positionToInsertAt == file.size())
    {
        //We haven't so lets add it to the end
        file.push_back(finalFormat);
    }
    
    //Now lets write the new file
    outputConfigFile.open(path.c_str(), ios::in | ios::trunc /* this last flag is set so we overwrite the previous contents (delete the previous contents and then we add our own */);
    
    //Loop through the vector and replace the contents with the new contents
    for (int i = 0;i < file.size();i++)
    {
        //Write the line to the file
        outputConfigFile << file[i];
    }
}

//Close the files
void Config::closeFiles()
{
    //If the input file hasn't been closed
    if (inputConfigFile.is_open())
    {
        //Close it
        inputConfigFile.close();
    }
    
    //If the output file hasn't been closed
    if (outputConfigFile.is_open())
    {
        //Close it
        outputConfigFile.close();
    }
}

bool has_extension(std::string to_check, std::string ext)
{
	using namespace std;
	
	if (to_check.size() < ext.size())
		return false;
	
	size_t location = to_check.find(ext);
	
	if (location == string::npos)
		return false;
	
	if (location != to_check.size()-ext.size())
		return false;
	
	return true;
}

int getdir(std::string dir, std::vector<std::string> &files)
{
	using namespace std;
	
	DIR *dp;
    struct dirent *dirp;
    
	if((dp  = opendir(dir.c_str())) == NULL)
	{
        cout << "Error(" << errno << " : " << strerror(errno) << ") opening " << dir << endl;
        
		return errno;
    }
	
    while ((dirp = readdir(dp)) != NULL) {
		struct stat st_buf;
		
		string file_name = dir + "/" + string(dirp->d_name);
		
		int status = stat(file_name.c_str(), &st_buf);
		
		if (status != 0)
		{
			cerr << "Error stat'ing file (" + file_name + "): " << strerror(errno) << endl;
			continue;
		}
		
		if (S_ISDIR(st_buf.st_mode))
			continue;
		
        files.push_back(file_name);
    }
	
    closedir(dp);
	
    return 0;
}
