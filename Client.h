#ifndef __CLIENT_H__
#define __CLIENT_H__

#include <iostream>
#include <iomanip>
#include <sstream>
#include <list>
#include <vector>
#include <time.h>
#include <dirent.h>
#include <sys/stat.h>
#include <pwd.h>
#include <grp.h>		
#include "Thread.h"
#include "SocketHandler.h"
#include "Server.h"
#include <fcntl.h>
#include <ext/hash_map>

extern pthread_cond_t 	g_wait;
extern pthread_mutex_t 	g_locklist;
extern pthread_mutex_t 	g_logMutex;

using namespace std;
using namespace __gnu_cxx;

#define DIGLEN(x) (strlen(itoa(x,10)))


struct eqstr
{
  bool operator()(const char* s1, const char* s2) const
  {
    return strcmp(s1, s2) == 0;
  }
};


class Client : public Thread
{
protected:
	// Members
	static bool running;
	static list<IncomingConnection*>* incomingConn;
	static pthread_mutex_t filesLock;
	string requestedFile;
	string requestedMethod;
	string httpVersion;
	SocketHandler *clientSocketHandler;
	fd_set readfds, writefds;
	string recvData;
	struct timeval timeout;
	int startPos, endPos;
	bool keepAlive;
	
	// Methods
	void run();
	bool ProcessRequest(string str, vector<string>& lines);		
	void ProcessGetRequest(vector<string>& lines);
	void ProcessHeadRequest(vector<string>& lines);
	void ProcessDeleteRequest(vector<string>& lines);
	bool CheckRequestFormat(string& s);
	void StripDoubleDots(string& s);
	int DisplayDirectoryContents(const string& path, bool GET = true);
	int DisplayFileContents(const string& fileName);	
	void ParseLines(vector<string>& lines, const string& str);		// Break the HTTP request into multiple lines
	string GetContentType(const string& uri);
	string CreateHeader(int httpCode, bool HEAD = false);
	string CreateDynamicHeader(const string& contentType, int contentLength);
	
public:
	static void setSharedList(std::list<IncomingConnection*>* in) { incomingConn = in; }
	static void setRunning(bool run) { running = run; }
	Client() { clientSocketHandler = 0; }
};

#endif

