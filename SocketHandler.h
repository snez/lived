#ifndef _SOCKETHANDLER_H_
#define _SOCKETHANDLER_H_

#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <string>
#include <sys/socket.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h> 
#include <iostream>
#include "Common.h"

using namespace std;

class SocketHandler {
        int _sockfd;
	fd_set readfds, writefds;
	struct timeval timeout;
public:
        SocketHandler(int sockfd);        
        char * Reverse(const char *str);        // Reverse the string in a character buffer        
        //void Send(const char *str);     // Send a piece of string over a socket
	void send(unsigned char * data, size_t len);     // Send a piece of string over a socket
	string recieve();
	int GetSocketFD() { return _sockfd; }
        ~SocketHandler();
};

#endif
