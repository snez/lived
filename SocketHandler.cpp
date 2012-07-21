#include "SocketHandler.h"

SocketHandler::SocketHandler(int sockfd) {
        _sockfd = sockfd;
}    

// Send a binary string over a socket
void SocketHandler::send(unsigned char * data, size_t len) 
{
	if (len <= 0) return;
	size_t total = 0, n = 0;
	
	long flags = fcntl(_sockfd, F_GETFD);
	fcntl(_sockfd, F_SETFL, flags | MSG_DONTWAIT);
	
	while (total < len) {
		n = ::send(_sockfd, &data[total], len, 0);
		if (n == -1) {
			perror("socket write");
			return;
		} else {
			total += n;
		}
	}
	
	fcntl(_sockfd, F_SETFD, flags);	
}

// Recieve data from the socket
string SocketHandler::recieve() {
	static const size_t bufsize = 1024;
	static long flags;
	char buf[bufsize];
	string data;
	int n;
		
	flags = fcntl(_sockfd, F_GETFD);
	fcntl(_sockfd, F_SETFL, flags | O_NONBLOCK);
	
	if ((n = recv(_sockfd, buf, bufsize, 0)) > 0) {
		buf[n] = '\0';
		data += buf;
	}
	
	fcntl(_sockfd, F_SETFD, flags);
	
	return data;
}

SocketHandler::~SocketHandler() {
        close(_sockfd);
}
