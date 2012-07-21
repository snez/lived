#include "Server.h"

IncomingConnection::IncomingConnection(const int s, struct sockaddr_in * clientptr) {
	_sockfd = s;
	_host = "";
	_client = clientptr;
}

IncomingConnection::IncomingConnection(const IncomingConnection& l) {
	_sockfd = l._sockfd;
	_host = l._host;
	_client = l._client;
}

const string& IncomingConnection::host() {
	if (_host == "") {
		/* Find client's address */
		struct hostent *rem = 0;
		if ((rem = gethostbyaddr((char *) &_client->sin_addr.s_addr, sizeof (_client->sin_addr.s_addr), _client->sin_family)) == NULL)  {
			_host = inet_ntoa(_client->sin_addr);
		} else {
			_host = rem->h_name;
			delete rem;
		}		
	}
	return _host;
}

const char * IncomingConnection::ip() {
	return inet_ntoa(_client->sin_addr);
}

IncomingConnection::~IncomingConnection() {
	if (_client > 0) {
		delete _client;
	}
	close(_sockfd);
}

Server::Server(uint32_t port) {
	_terminated = false;
	sockfd = socket(PF_INET, SOCK_STREAM, 0); 
	if (sockfd == -1) throw SocketException();
	server.sin_family = AF_INET;                   // host byte order
	server.sin_port = htons(port);                 // short, network byte order
	server.sin_addr.s_addr = htonl(INADDR_ANY);    // fill in the IP address of the machine
	memset(&(server.sin_zero), '\0', 8);           // zero the rest of the struct
	
	if (bind(sockfd, (struct sockaddr *)&server, sizeof(struct sockaddr)) < 0) 
		throw BindException();
	if (listen(sockfd, backlog) < 0) 
		throw ListenException();
}

IncomingConnection* Server::Accept() {
	if (_terminated) throw AcceptException();
	
	struct sockaddr_in *client_in = new struct sockaddr_in;
	struct sockaddr *clientptr = (struct sockaddr *) client_in;
	static socklen_t clientlen = sizeof(struct sockaddr_in);
	static int newsockfd = 0;
	bzero(client_in->sin_zero, 8 * sizeof(unsigned char));
	
	if ((newsockfd = accept(sockfd, clientptr, &clientlen)) < 0)  
		throw AcceptException();
          
	Rebind();
	return new IncomingConnection(newsockfd, client_in);
}

void Server::Rebind() {
	static int yes=1;
	if (_terminated) return;
	if (setsockopt(sockfd, SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(int)) == -1) 
		throw SetsockoptException();
}

void Server::Terminate() {
	if (_terminated) return;
	close(sockfd);  
	_terminated = true;
}

Server::~Server() {
	Terminate();
}
	
