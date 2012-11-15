#include "Server.h"

#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

#include <string.h>
#include <errno.h>

#include <iostream>

#include <vector>

#include <algorithm>

typedef struct {
	Server* server;
	int socket;
	SSL* ssl;
	bool isUsingSSL;
	Server_Client_ID client;
} Accept_Struct;

std::string ssl_error_code_to_string(int error_code)
{
	switch (error_code)
	{
		case SSL_ERROR_NONE:
			return "The TLS/SSL I/O operation completed.  There was no error.";
		case SSL_ERROR_ZERO_RETURN:
			return "The TLS/SSL connection has been closed.";
		case SSL_ERROR_WANT_READ:
			return "The operation did not complete; the same TLS/SSL I/O function should be called again later.  If, by then, the underlying BIO has data available for reading, then some TLS/SSL protocol progress will take place, i.e. at least part of an TLS/SSL record will be read.";
		case SSL_ERROR_WANT_WRITE:
			return "The operation did not complete; the same TLS/SSL I/O function should be called again later.  If, by then, the underlying BIO has data available for writing, then some TLS/SSL protocol progress will take place, i.e. at least part of an TLS/SSL record will be written.";
		case SSL_ERROR_WANT_CONNECT:
			return "The operation did not complete; the same TLS/SSL I/O function should be called again later. The underlying BIO was not connected yet to the peer and the call would block in connect(). The SSL function should be called again when the connection is established.";
		case SSL_ERROR_WANT_ACCEPT:
			return "The operation did not complete; the same TLS/SSL I/O function should be called again later. The underlying BIO was not connected yet to the peer and the call would block in accept(). The SSL function should be called again when the connection is established.";
		case SSL_ERROR_WANT_X509_LOOKUP:
			return "The operation did not complete because an application callback set by SSL_CTX_set_client_cert_cb() has asked to be called again. The TLS/SSL I/O function should be called again later.";
		case SSL_ERROR_SYSCALL:
			return "Some I/O error occurred. The OpenSSL error queue may contain more information on the error. If the error queue is empty (i.e. ERR_get_error() returns 0), ret can be used to find out more about the error: If ret == 0, an EOF was observed that violates the protocol. If ret == -1, the underlying BIO reported an I/O error (for socket I/O on Unix systems, consult errno for details).";
		case SSL_ERROR_SSL:
			return "A failure in the SSL library occurred, usually a protocol error. The OpenSSL error queue contains more information on the error.";
		default:
			return "Unknown Error Code.";
	}
}

void* server_read_thread_function(void* accept)
{
	using namespace std;
	
	Accept_Struct* accept_struct = reinterpret_cast<Accept_Struct*>(accept);
	
	vector<string> messages;
	int sock = accept_struct->socket;
	Server* server = accept_struct->server;
	SSL* ssl = accept_struct->ssl;
	bool isUsingSSL = accept_struct->isUsingSSL;
	Server_Client_ID client = accept_struct->client;
	
	string message;
	
	while (true)
	{
		if (message.find('\n') == string::npos)
		{
			int bytes_recieved = 0;
			
			char buffer[512];
			if (!isUsingSSL)
				bytes_recieved = recv(sock, buffer, 512, 0);
			else
				bytes_recieved = SSL_read(ssl, buffer, 512);
			
			if (bytes_recieved <= 0)
			{
				server->on_client_disconnect(client);
				
				if (server->get_delegate())
					server->get_delegate()->on_client_disconnect(client, server);
				
				server->disconnect_client(client);
				
				break;
			}
			
			buffer[bytes_recieved] = '\0';
			
			message += buffer;
		}
		else
		{
			int length = message.find('\n')+1;
			string final_message = message.substr(0, length);
			message.erase(message.begin(), message.begin()+length);
			
			server->recieve_message(final_message, client);
			
			if (server->get_delegate())
				server->get_delegate()->recieve_message(final_message, client, server);
		}
	}
	
	delete accept_struct;
	
	pthread_exit(NULL);
}

void server_thread_function(Accept_Struct* accept_struct, bool ipv6)
{
	using namespace std;
	
	int server_socket = accept_struct->socket;
	Server* server = accept_struct->server;
	bool isUsingSSL = accept_struct->isUsingSSL;
	int err = 0;
	
	SSL_CTX *ctx;
	
	if (isUsingSSL)
	{
		if ((err = ERR_get_error()) != 0)
		{
			cerr << "SSL Library Error (Library Init): " << ERR_error_string(err, NULL) << endl;
			return;
		}
		
		const SSL_METHOD *method = SSLv3_server_method();
		ctx = SSL_CTX_new(method);
		
		if ((err = ERR_get_error()) != 0)
		{
			cerr << "SSL Library Error (CTX New): " << ERR_error_string(err, NULL) << endl;
			return;
		}
		
		SSL_CTX_use_certificate_file(ctx, server->get_cert_path().c_str(), SSL_FILETYPE_PEM);
		
		if ((err = ERR_get_error()) != 0)
		{
			cerr << "SSL Library Error (Cert File): " << ERR_error_string(err, NULL) << endl;
			return;
		}
		
		SSL_CTX_use_PrivateKey_file(ctx, server->get_key_path().c_str(), SSL_FILETYPE_PEM);
		
		if ((err = ERR_get_error()) != 0)
		{
			cerr << "SSL Library Error (Key File): " << ERR_error_string(err, NULL) << endl;
			return;
		}
		
		if (!SSL_CTX_check_private_key(ctx))
		{
			cerr << "SSL Library Error (Failed to check Key File against cert): " << ERR_error_string(ERR_get_error(), NULL) << endl;
			return;
		}
	}
	
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
		
		Server_Client_ID id = Server::next_id();
		SSL* ssl = NULL;
		
		if (isUsingSSL)
		{
			ssl = SSL_new(ctx);
			SSL_set_fd(ssl, client_sock);
			err = SSL_accept(ssl);
			if (err != 1)
			{
				cerr << "SSL Library Error (SSL Accept): " << ssl_error_code_to_string(err) << endl;
				break;
			}
			
			server->accept_client(id, ssl);
		}
		else
			server->accept_client(id, client_sock);
		
		//pthread_t accept_thread;
		
		Accept_Struct* to_pass = new Accept_Struct;
		to_pass->server = accept_struct->server;
		to_pass->isUsingSSL = isUsingSSL;
		to_pass->client = id;
		to_pass->socket = client_sock;
		
		if (isUsingSSL)
			to_pass->ssl = ssl;
				
		accept_struct->server->on_client_connect(to_pass->client);
		
		if (accept_struct->server->get_delegate())
			accept_struct->server->get_delegate()->on_client_connect(to_pass->client, accept_struct->server);
		
		delete to_pass;
		
		//pthread_create(&accept_thread, NULL, server_accept_thread_function, (void*)to_pass);
		
		Accept_Struct* read_to_pass = new Accept_Struct;
		read_to_pass->server = accept_struct->server;
		read_to_pass->isUsingSSL = isUsingSSL;
		read_to_pass->client = id;
		read_to_pass->socket = client_sock;
		
		if (isUsingSSL)
			read_to_pass->ssl = ssl;
		
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

Server::Server(int port, Server::Server_Type type, Server_Delegate *delegate, bool verbose)
{
	pthread_mutex_init(&server_mutex, NULL);
	
	server_type = type;
	this->verbose = verbose;
	this->delegate = delegate;
	this->ssl = false;
	
	create_server(port, type);
}

Server::Server(int port, Server::Server_Type type, bool ssl, std::string cert_path, std::string key_path, Server_Delegate *delegate, bool verbose)
{
	pthread_mutex_init(&server_mutex, NULL);
	
	server_type = type;
	this->verbose = verbose;
	this->delegate = delegate;
	this->ssl = ssl;
	
	this->cert_path = cert_path;
	this->key_path = key_path;
	
	SSL_load_error_strings();
	SSL_library_init();
	OpenSSL_add_all_algorithms();
	OpenSSL_add_all_ciphers();
	OpenSSL_add_all_digests();
	
	create_server(port, type);
}

void Server::create_server(int port, Server::Server_Type server_type)
{
	using namespace std;
	
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
		accept_struct->isUsingSSL = ssl;
		
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
		
		//server_name_ipv6.sin6_len = sizeof(struct sockaddr_in6);
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
		accept_struct->isUsingSSL = ssl;
		
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
	pthread_mutex_destroy(&server_mutex);
	
	for (map<Server_Client_ID, int>::iterator it = clients.begin();it != clients.end();it++)
		close(it->second);
	
	for (map<Server_Client_ID, SSL*>::iterator it = ssl_clients.begin();it != ssl_clients.end();it++)
	{
		int s = SSL_get_fd(it->second);
		SSL_free(it->second);
		close(s);
	}
	
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
	
	pthread_mutex_lock(&server_mutex);
	
	for (std::map<Server_Client_ID, int>::iterator it = clients.begin();it != clients.end();it++)
		write(it->second, c_msg, size);
	
	for (std::map<Server_Client_ID, SSL*>::iterator it = ssl_clients.begin();it != ssl_clients.end();it++)
		SSL_write(it->second, c_msg, size);
	
	pthread_mutex_unlock(&server_mutex);
}

void Server::broadcast_message_to_clients(std::string &message, std::set<Server_Client_ID> clients)
{
	using namespace std;
	
	for (set<Server_Client_ID>::iterator it = clients.begin();it != clients.end();it++)
		send_message(message, *it);
}

void Server::send_message(std::string &message, const Server_Client_ID &client)
{
	using namespace std;
	
	pthread_mutex_lock(&server_mutex);
	
	if (clients.find(client) != clients.end())
		write(clients[client], message.c_str(), message.size());
	else if (ssl_clients.find(client) != ssl_clients.end())
		SSL_write(ssl_clients[client], message.c_str(), message.size());
	else
	{
		pthread_mutex_unlock(&server_mutex);
		throw Sending_Without_Existence;
	}
	
	pthread_mutex_unlock(&server_mutex);
}

void Server::accept_client(Server_Client_ID &client, SSL* ssl)
{
	pthread_mutex_lock(&server_mutex);
	
	ssl_clients[client] = ssl;
	
	pthread_mutex_unlock(&server_mutex);
}

void Server::accept_client(Server_Client_ID &client, int client_socket)
{
	pthread_mutex_lock(&server_mutex);
	
	clients[client] = client_socket;
	
	pthread_mutex_unlock(&server_mutex);
}

void Server::disconnect_client(Server_Client_ID &client)
{
	pthread_mutex_lock(&server_mutex);
	
	std::map<Server_Client_ID,int>::iterator it = clients.find(client);
	if (it != clients.end())
	{
		on_client_disconnect(client);
		
		if (delegate)
			delegate->on_client_disconnect(client, this);
		
		close(it->second);
		clients.erase(it);
	}
	else
	{
		std::map<Server_Client_ID,SSL*>::iterator s_it = ssl_clients.find(client);
		if (s_it != ssl_clients.end())
		{
			on_client_disconnect(client);
			
			if (delegate)
				delegate->on_client_disconnect(client, this);
			
			int s = SSL_get_fd(s_it->second);
			SSL_free(s_it->second);
			close(s);
			ssl_clients.erase(s_it);
		}
	}
	
	pthread_mutex_unlock(&server_mutex);
}

Server_Client_ID Server::next_id()
{
	static Server_Client_ID client_id = 0;
	
	return client_id++;
}

int Server::client_id_to_socket(Server_Client_ID &client)
{
	pthread_mutex_lock(&server_mutex);
	
	int socket = -1;
	
	std::map<Server_Client_ID,int>::iterator it = clients.find(client);
	if (it != clients.end())
		socket = it->second;
	else
	{
		std::map<Server_Client_ID,SSL*>::iterator s_it = ssl_clients.find(client);
		if (s_it != ssl_clients.end())
			socket = SSL_get_fd(s_it->second);
	}
	
	pthread_mutex_unlock(&server_mutex);
	
	return socket;
}
