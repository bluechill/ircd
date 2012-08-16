#ifndef SERVER_H
#define SERVER_H 1

#include <string>
#include <set>
#include <exception>

#include <pthread.h>

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
	
	Server(int port, Server_Type type, bool verbose = true);
	~Server();

	virtual void on_client_connect(int client_sock) {}
	virtual void on_client_disconnect(int client_socket) {}

	virtual void recieve_message(std::string message, int client_sock) {}
	
	void broadcast_message(std::string &message);
	void broadcast_message_to_clients(std::string &message, std::set<int> sockets);

	void send_message(std::string &message, int client_socket);
	
	void accept_client(int client_sock);
	void disconnect_client(int client_sock);
	
	pthread_mutex_t* get_mutex() { return server_mutex; }
private:
	pthread_t server_thread_ipv4;
	pthread_t server_thread_ipv6;
	
	pthread_mutex_t* server_mutex;
	
	bool verbose;
	
	int server_sock_ipv4;
	int server_sock_ipv6;
	
	Server_Type server_type;
	
	std::set<int> clients;
};

#endif

