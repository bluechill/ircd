#ifndef IRC_SERVER_H
#define IRC_SERVER_H 1

#include "Server.h"

class IRC_Server : public Server
{
public:
	IRC_Server();
	~IRC_Server();
	
	void recieve_message(std::string message, int client_sock);
};

#endif
