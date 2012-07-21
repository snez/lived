#ifndef __SERVER_H__
#define __SERVER_H__

#include <iostream>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <string>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h> 
#include <netinet/in.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "Common.h"

using namespace std;

////////////////////////////////////////////////////////////////////////////////
// Exceptions
////////////////////////////////////////////////////////////////////////////////

struct SocketException {};
struct ListenException {};
struct BindException {};
struct AcceptException {};
struct SetsockoptException {};

class IncomingConnection {
	int 			_sockfd;
	struct sockaddr_in *	_client;
	string 			_host;
public:
	IncomingConnection(const int s, struct sockaddr_in * clientptr);	// Constructor
	IncomingConnection(const IncomingConnection& l);			// Copy constructor
	const string& host();							// Returns the hostname of the client as a string
	const char * ip();							// Returns the ip of the client as a character string
	inline const int sockfd() { return _sockfd; }				// Returns the socket file descriptor
	~IncomingConnection();							// Destructor
};

class Server {
        int sockfd; 
        struct sockaddr_in server;
        size_t sin_size;
        bool _terminated;
        static const unsigned int backlog = 10; // The number of pending connections the queue will hold
public:
        Server(uint32_t port);
        IncomingConnection* Accept();
        void Rebind();
        void Terminate();
	static const string getName() { return "lived"; }
        ~Server();
};

#endif
