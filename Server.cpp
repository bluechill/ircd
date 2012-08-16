#include "Server.h"

#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

#include <errno.h>

#include <iostream>

#include <vector>

typedef struct {
	Server* server;
	int socket;
} Accept_Struct;

void* server_read_thread_function(void* accept)
{
	using namespace std;
	
	Accept_Struct* accept_struct = reinterpret_cast<Accept_Struct*>(accept);
	
	vector<string> messages;
	int sock = accept_struct->socket;
	Server* server = accept_struct->server;
	
	string message;
	
	while (true)
	{
		if (message.find('\n') == string::npos)
		{
			int bytes_recieved = 0;
			
			char buffer[512];
			bytes_recieved = recv(sock, buffer, 512, 0);
			buffer[bytes_recieved] = '\0';
			
			message += buffer;
		}
		else
		{
			int length = message.find('\n')+1;
			string final_message = message.substr(0, length);
			message.erase(message.begin(), message.begin()+length);
			
			server->recieve_message(final_message, sock);
		}
	}
	
	delete accept_struct;
	
	pthread_exit(NULL);
}

void* server_accept_thread_function(void* accept)
{
	Accept_Struct* accept_struct = reinterpret_cast<Accept_Struct*>(accept);
	
	accept_struct->server->on_client_connect(accept_struct->socket);
	
	delete accept_struct;
	
	pthread_exit(NULL);
}

void server_thread_function(Accept_Struct* accept_struct, bool ipv6)
{
	using namespace std;
	
	int server_socket = accept_struct->socket;
	Server* server = accept_struct->server;
	
	while (true)
	{
		int client_sock;
		
		struct sockaddr* from = NULL;
		struct sockaddr_in from_ipv4;
		struct sockaddr_in6 from_ipv6;
		
		unsigned int length = 0;
		
		if (ipv6)
		{
			from = (struct sockaddr*)&from_ipv6;
			length = sizeof(from_ipv6);
		}
		else
		{
			from = (struct sockaddr*)&from_ipv4;
			length = sizeof(from_ipv4);
		}
		
		
		client_sock = accept(server_socket, from, &length);
		if (client_sock == -1)
		{
			cerr << "Could not accept on socket: " << strerror(errno) << endl;
			cerr << "Exiting server thread IPv" << (ipv6 ? "6" : "4") << endl;
			break;
		}
		
		pthread_mutex_lock(server->get_mutex());
		
		server->accept_client(client_sock);
		
		pthread_mutex_unlock(server->get_mutex());
		
		pthread_t accept_thread;
		
		Accept_Struct* to_pass = new Accept_Struct;
		to_pass->server = accept_struct->server;
		to_pass->socket = client_sock;
		
		pthread_create(&accept_thread, NULL, server_accept_thread_function, (void*)to_pass);
		
		Accept_Struct* read_to_pass = new Accept_Struct;
		read_to_pass->server = accept_struct->server;
		read_to_pass->socket = client_sock;
		
		pthread_t read_thread;
		
		pthread_create(&read_thread, NULL, server_read_thread_function, (void*)read_to_pass);
	}
}

void* server_thread_function_ipv4(void* accept)
{
	Accept_Struct* accept_struct = reinterpret_cast<Accept_Struct*>(accept);
	
	server_thread_function(accept_struct, false);
	
	delete accept_struct;
	
	pthread_exit(NULL);
}

void* server_thread_function_ipv6(void* accept)
{
	Accept_Struct* accept_struct = reinterpret_cast<Accept_Struct*>(accept);
	
	server_thread_function(accept_struct, true);
	
	delete accept_struct;
	
	pthread_exit(NULL);
}

Server::Server_Exception Server::IPv4_Initialization_Failure("Failed to initialize IPv4 socket correctly.  See cerr for more details");
Server::Server_Exception Server::IPv6_Initialization_Failure("Failed to initialize IPv6 socket correctly.  See cerr for more details");
Server::Server_Exception Server::Thread_Initialization_Failure("Failed to initialize the server thread correctly.  See cerr for more details");
Server::Server_Exception Server::Sending_Without_Existence("Failed to send a message to a socket because this instance of the class does not have a client socket with that id.");

Server::Server(int port, Server_Type type, bool verbose)
{
	using namespace std;
	
	pthread_mutex_init(server_mutex, NULL);
	
	server_type = type;
	this->verbose = verbose;
	
	int status = 0;
	
	if (server_type == IPv4_Server || server_type == Dual_IPv4_IPv6_Server)
	{
		struct sockaddr_in server_name_ipv4;
		
		server_sock_ipv4 = socket(AF_INET, SOCK_STREAM, 0);
		if (server_sock_ipv4 == -1)
		{
			cerr << "Failed to create IPv4 socket: " << strerror(errno) << endl;
			throw IPv4_Initialization_Failure;
		}
		
		server_name_ipv4.sin_family = AF_INET;
		server_name_ipv4.sin_addr.s_addr = INADDR_ANY;
		server_name_ipv4.sin_port = htons(port);
		
		status = bind(server_sock_ipv4, (struct sockaddr*)&server_name_ipv4, sizeof(server_name_ipv4));
		if (status == -1)
		{
			cerr << "Failed to bind to socket (port: " << port << ") with error: " << strerror(errno) << endl;
			throw IPv4_Initialization_Failure;
		}
		
		status = listen(server_sock_ipv4, 5);
		if (status == -1)
		{
			cerr << "Failed to listen: " << strerror(errno) << endl;
			throw IPv4_Initialization_Failure;
		}
		
		Accept_Struct* accept_struct = new Accept_Struct;
		accept_struct->server = this;
		accept_struct->socket = server_sock_ipv4;
		
		status = pthread_create(&server_thread_ipv4, NULL, server_thread_function_ipv4, accept_struct);
		if (status)
		{
			cerr << "Failed to create server thread with error: " << status << " (\"" << strerror(errno) << "\")" << endl;
			throw Thread_Initialization_Failure;
		}
	}
	
	if (server_type == IPv6_Server || server_type == Dual_IPv4_IPv6_Server)
	{
		struct sockaddr_in6 server_name_ipv6;
		
		server_sock_ipv6 = socket(AF_INET6, SOCK_STREAM, 0);
		if (server_sock_ipv6 == -1)
		{
			cerr << "Failed to create IPv6 socket: " << strerror(errno) << endl;
			throw IPv6_Initialization_Failure;
		}
		
		server_name_ipv6.sin6_len = sizeof(struct sockaddr_in6);
		server_name_ipv6.sin6_family = AF_INET6;
		server_name_ipv6.sin6_addr = in6addr_any;
		server_name_ipv6.sin6_port = htons(port);
		
		status = bind(server_sock_ipv6, (struct sockaddr*)&server_name_ipv6, sizeof(server_name_ipv6));
		if (status == -1)
		{
			cerr << "Failed to bind to socket (port: " << port << ") with error: " << strerror(errno) << endl;
			throw IPv6_Initialization_Failure;
		}
		
		status = listen(server_sock_ipv6, 5);
		if (status == -1)
		{
			cerr << "Failed to listen: " << strerror(errno) << endl;
			throw IPv6_Initialization_Failure;
		}
		
		Accept_Struct* accept_struct = new Accept_Struct;
		accept_struct->server = this;
		accept_struct->socket = server_sock_ipv6;
		
		status = pthread_create(&server_thread_ipv6, NULL, server_thread_function_ipv6, accept_struct);
		if (status)
		{
			cerr << "Failed to create server thread with error: " << status << " (\"" << strerror(errno) << "\")" << endl;
			throw Thread_Initialization_Failure;
		}
	}
}

Server::~Server()
{
	using namespace std;
	
	pthread_exit(NULL);
	pthread_mutex_destroy(server_mutex);
	
	for (set<int>::iterator it = clients.begin();it != clients.end();it++)
		close(*it);
	
	if (server_type == IPv4_Server || server_type == Dual_IPv4_IPv6_Server)
		close(server_sock_ipv4);
	
	if (server_type == IPv6_Server || server_type == Dual_IPv4_IPv6_Server)
		close(server_sock_ipv6);
}

void Server::broadcast_message(std::string &message)
{
	using namespace std;
	
	const char* c_msg = message.c_str();
	size_t size = message.size();
	
	pthread_mutex_lock(server_mutex);
	
	for (set<int>::iterator it = clients.begin();it != clients.end();it++)
		write(*it, c_msg, size);
	
	pthread_mutex_unlock(server_mutex);
}

void Server::broadcast_message_to_clients(std::string &message, std::set<int> sockets)
{
	using namespace std;
	
	const char* c_msg = message.c_str();
	size_t size = message.size();
	
	pthread_mutex_lock(server_mutex);
	
	for (set<int>::iterator it = sockets.begin();it != sockets.end();it++)
	{
		if (clients.find(*it) == clients.end())
		{
			pthread_mutex_unlock(server_mutex);
			throw Sending_Without_Existence;
		}
		
		write(*it, c_msg, size);
	}
	
	pthread_mutex_unlock(server_mutex);
}

void Server::send_message(std::string &message, int client_socket)
{
	using namespace std;
	
	if (clients.find(client_socket) == clients.end())
		throw Sending_Without_Existence;
	
	write(client_socket, message.c_str(), message.size());
}

void Server::accept_client(int client_sock)
{
	clients.insert(client_sock);
}
