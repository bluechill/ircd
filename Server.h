#ifndef SERVER_H
#define SERVER_H 1

#include <string>
#include <set>
#include <map>
#include <exception>

#include <pthread.h>

#include <openssl/ssl.h>
#include <openssl/err.h>

typedef long long Server_Client_ID;

class Server;

class Server_Delegate
{
public:
	virtual void on_client_connect(Server_Client_ID &client, Server* server) = 0;
	virtual void on_client_disconnect(Server_Client_ID &client, Server* server) = 0;
	
	virtual void recieve_message(std::string &message, Server_Client_ID &client, Server* server) = 0;
};

class Server
{
public:
	class Server_Exception : public std::exception
	{
	public:
		Server_Exception(std::string type)
		{
			this->type = type;
		}
		
		virtual ~Server_Exception() throw() {}
		
		virtual const char* what() const throw()
		{
			return type.c_str();
		}
	private:
		std::string type;
	};
	
	static Server_Exception IPv4_Initialization_Failure;
	static Server_Exception IPv6_Initialization_Failure;
	static Server_Exception Thread_Initialization_Failure;
	static Server_Exception Sending_Without_Existence;
	
	typedef enum {
		IPv4_Server = 0,
		IPv6_Server = 1,
		Dual_IPv4_IPv6_Server = 2
	} Server_Type;
	
	Server(int port, Server_Type type, Server_Delegate *delegate = NULL, bool verbose = true);
	Server(int port, Server_Type type, bool ssl, std::string cert_path, std::string key_path, Server_Delegate *delegate = NULL, bool verbose = true);
	virtual ~Server();

	virtual void on_client_connect(Server_Client_ID &client) {}
	virtual void on_client_disconnect(Server_Client_ID &client) {}

	virtual void recieve_message(std::string &message, Server_Client_ID &client) {}
	
	void broadcast_message(std::string &message);
	void broadcast_message_to_clients(std::string &message, std::set<Server_Client_ID> clients);

	void send_message(std::string &message, const Server_Client_ID &client);
	
	void accept_client(Server_Client_ID &client, SSL* ssl);
	void accept_client(Server_Client_ID &client, int client_socket);
	
	void disconnect_client(Server_Client_ID &client);
	
	pthread_mutex_t* get_mutex() { return &server_mutex; }
	
	Server_Delegate* get_delegate() { return delegate; }
	
	std::string get_cert_path() { return cert_path; }
	std::string get_key_path() { return key_path; }
	
	std::map<Server_Client_ID, int>* get_clients() { return &clients; }
	std::map<Server_Client_ID, SSL*>* get_ssl_clients() { return &ssl_clients; }
	
	int client_id_to_socket(Server_Client_ID &client);
	
protected:
	bool verbose;
	
private:
	pthread_t server_thread_ipv4;
	pthread_t server_thread_ipv6;
	
	pthread_mutex_t server_mutex;
	
	int server_sock_ipv4;
	int server_sock_ipv6;
	
	Server_Type server_type;
	
	bool ssl;
	Server_Delegate* delegate;
	
	std::map<Server_Client_ID, int> clients;
	std::map<Server_Client_ID, SSL*> ssl_clients;
	
	void create_server(int port, Server_Type type);
	
	std::string cert_path;
	std::string key_path;
	
public:
	static Server_Client_ID next_id();
};

#endif

