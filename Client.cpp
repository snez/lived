#include "Client.h"

bool Client::running = false;
list<IncomingConnection*>* Client::incomingConn = 0;
pthread_mutex_t Client::filesLock = PTHREAD_MUTEX_INITIALIZER;

// Converts an integer into an array
static char* itoa(unsigned int val, const int base){	
	static char buf[32] = {0};	
	register int i = 30;	
	if (val == 0) val = 1;
	for(; val && i ; --i, val /= base)	
		buf[i] = "0123456789abcdef"[val % base];	
	return &buf[i+1];	
}

void Client::run() 
{ 
	static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
        
	while (running) 
	{
		pthread_mutex_lock(&lock);
		
		if (running) // got access but am i still running? 
			pthread_cond_wait(&g_wait, &lock);
		else 
		{
 			pthread_mutex_unlock(&lock);
			break;			
		}
			
		DBG(logWrite(cout << "Thread " << threadID << " was signaled\n");)
		
		// Lock on the linked list of incoming connections and retrieve one of them
  		pthread_mutex_lock(&g_locklist);
		
 		if (incomingConn->size() == 0)
		{
			pthread_mutex_unlock(&g_locklist);
 			pthread_mutex_unlock(&lock);
 			continue;
 		}

		IncomingConnection *clientConnection = incomingConn->front();
		incomingConn->pop_front();
		clientSocketHandler = new SocketHandler(clientConnection->sockfd());
 		
  		pthread_mutex_unlock(&g_locklist);

		pthread_mutex_unlock(&lock);
		if (!running)
			break;
        
		logWrite(cout << "Thread " << threadID << " serves host " << clientConnection->ip() << endl);
		
		// Get the HTTP Request
		recvData.clear();
		keepAlive = true;
		
		timeout.tv_sec = 15;
		timeout.tv_usec = 0;
		
		do {
			FD_ZERO(&readfds);
			FD_SET(clientConnection->sockfd(), &readfds);
			timeout.tv_sec = 15;
			timeout.tv_usec = 0;
			DBG(cout << timeout.tv_sec << ", " << timeout.tv_usec << ".\n";)
			switch (select(clientConnection->sockfd()+1, &readfds, 0, 0, &timeout)){
				case -1: running = false; break;
				case 0: keepAlive = false; break; // Timeout
				default: 
					recvData += clientSocketHandler->recieve();
					
					// If CRLF is in the string, we are ready to process it
					if (recvData.find("\r\n\r\n") < string::npos) 
					{						
						// Break the HTTP request into multiple lines
						vector<string> lines;
						lines.reserve(8);
						ParseLines(lines, recvData);
						
						// In the case that the only input was \r\n\r\n then close connection
						if (lines.size() == 0) {
							keepAlive = false;
							break;
						}							
						
						// Log the request in the log file
						char timebuf[20] = {0};
						time_t t = time(0);
						struct tm * timeinfo = localtime ( &t );
						strftime(timebuf, 20, "%Y-%b-%d %H:%M", timeinfo);
						logWrite(cout << clientConnection->ip() << " - [" << timebuf << "] - \"" << lines[0] << "\"\n";)
						
						if (ProcessRequest(recvData, lines) == true) {	// If we finished processing the request
							recvData.clear();	// Clear the failbits
							recvData = "";		// Reset the string
							timeout.tv_sec = 15;	// Also refresh the timeout for the next request
							timeout.tv_usec = 0;
							requestedMethod.clear();
							requestedFile.clear();
							httpVersion.clear();
							requestedMethod = requestedFile = httpVersion = "";
						}
					} 
					else if (recvData.length() > 0x4000) // Header is abnormally big so we close the connection
					{
						keepAlive = false;
					}
					
					break;							
			}
		} while (keepAlive && running);
		
		// Close Client Connection
		delete clientSocketHandler;
		delete clientConnection;
		clientSocketHandler = 0;
	}
	
	DBG(logWrite(cout << "Bye bye from Thread " << threadID << endl))
}


// Returns true if the string has been processed, false if it is an incomplete HTTP request
bool Client::ProcessRequest(string str, vector<string>& lines) {
	
	// Check if this is a keep-alive request
	string tmp;
	keepAlive = true;
	for (int i = 1; i < lines.size(); i++) {
		tmp = lines[i];
		for(unsigned int i = 0; i < tmp.length(); i++) {
			tmp[i] = tolower(tmp[i]);
		}
		if (tmp.find("connection: close") < string::npos) {
			keepAlive = false;
		}
	}
	
	// Check the validity of the request and set related member variables
	if (CheckRequestFormat(lines[0]) == false) {
		keepAlive = false;
		return true;
	}
		
	
	StripDoubleDots(requestedFile);
	
	string data;
	if (access((requestedFile).c_str(), R_OK) == -1) // check if the uri is valid. maybe leave it to process request...
	{
		perror(requestedFile.c_str());
		if (requestedMethod == "HEAD")
			data = CreateHeader(404,true);
		else
			data = CreateHeader(404);
		clientSocketHandler->send((unsigned char *)data.c_str(), data.length());
		return true;
	}
	
	// Check what kind of request did we get
	if (requestedMethod == "GET") ProcessGetRequest(lines);
	else if (requestedMethod == "HEAD") ProcessHeadRequest(lines);	
	else if ((requestedMethod == "DELETE") ||
		(requestedMethod == "OPTIONS") || 
		(requestedMethod == "POST") || 
		(requestedMethod == "PUT") || 
		(requestedMethod == "TRACE") || 
		(requestedMethod == "CONNECT")) {
		data = CreateHeader(501);
		clientSocketHandler->send((unsigned char *)data.c_str(), data.length());
	}
	else { 
		data = CreateHeader(400);
		clientSocketHandler->send((unsigned char *)data.c_str(), data.length());
	}
	
	return true;
}

// Check that the request line is METHOD SP URI SP HTTP CRLF
// Also sets requestedMethod requestedFile and httpVersion
bool Client::CheckRequestFormat(string& s) 
{
	if (s.find("\r") != string::npos || s.find("\n") != string::npos)
		return false;
		
	int pos = s.find(" "); // find the first space
		
	if (pos >= s.size()-1) // no space was found or found at end of line
		return false;
		
	requestedMethod = s.substr(0, pos);
	
	if (s.at(pos+1) != '/') // if first space is not followed by / its a bad request
		return false;
	
	int pos2 = s.find(" ",pos+1); // find next space which is after the uri
	
	if (pos2 == string::npos) // no space after uri
	{
		if (pos+2 == s.size()) 
			requestedFile = "./";
		else 
			requestedFile = s.substr(pos+2, s.size() - pos - 2);
		
		httpVersion = "HTTP/1.1";
	}
	else // space after uri
	{	
		if (pos2 == pos + 2)
			requestedFile = "./";
		else 
			requestedFile = s.substr(pos+2, pos2 - pos - 2); 		

		httpVersion = s.substr(pos2 + 1);
		if (httpVersion != "HTTP/1.1") // no trailing space is allowed
			return false;
	} 

	if (requestedFile.at(requestedFile.size()-1) == '/') // directory so try index.html
	{		
		if (access((requestedFile + "index.html").c_str(), R_OK) == 0)
			requestedFile += "index.html";
		else
			if (access((requestedFile + "index.htm").c_str(), R_OK) == 0)
				requestedFile += "index.htm";
	}	
	DBG(cout << "req method '" << requestedMethod << "' req file '" << requestedFile << "' http '" << httpVersion << "'\n";)
	
	return true;
}

// Break the HTTP request into multiple lines
void Client::ParseLines(vector<string>& lines, const string& str) {
	startPos = 0;
	endPos = 0;
	while ((startPos = str.find_first_not_of("\r\n", startPos)) != string::npos) {		
		if ((endPos = str.find("\r\n", startPos)) != string::npos) {
			lines.push_back(str.substr(startPos, endPos-startPos));
			startPos = endPos + 2;
		}
	}
}

string Client::GetContentType(const string& uri)
// assumes that the uri contains a filename
{
	int pos = uri.rfind("."); // find the last .
	if (pos < uri.size()-1)
	{
		string extension = uri.substr(pos+1);
		int size = extension.size();
		for (unsigned int i = 0; i < size; i++) 
			extension[i] = tolower(extension[i]);

		if (extension == "txt" || extension == "sed" || extension == "awk" || extension == "c" || extension == "cpp" || extension == "h")
			return "text/plain";
		else if (extension == "html" || extension == "htm")
			return "text/html";
		else if (extension == "jpeg" || extension == "jpg")
			return "image/jpeg";
		else if (extension == "gif")	
			return "image/gif";
		else if (extension == "pdf")
			return "application/pdf";		
	}

	return "application/octet-stream";
}


string Client::CreateHeader(int httpCode, bool HEAD)
{
	ostringstream header;
	string errMsg;
	header << "HTTP/1.1";
	switch (httpCode)
	{
		case 200: 
			header << " 200 OK\r\n"; 
			break;
		case 404: 
			header << " 404 Not Found\r\n"; 
			errMsg = "Not Found\n"; 
			break;
		case 501: 
			header << " 501 Not Implemented\r\n"; 
			errMsg = "Not Implemented\n"; 
			break;		
		default: 
			header << " 400 Bad Request\r\n"; 
			errMsg = "Bad Request\n";
			break;
	}
	
	header << "Connection: ";
	if (keepAlive)
		header << "keep-alive\r\n";
	else
		header << "close\r\n";

	header << "Server: " << Server::getName() << "\r\n";
	
	if (httpCode == 200)
	{
		struct stat buf;
		if (stat(requestedFile.c_str(), &buf) < 0) 
		{
			perror("stat error");
			header << "\r\n";
			return header.str();
		}
		
		header 	<< "Content-Type: " << GetContentType(requestedFile).c_str() << "\r\n"
			<< "Content-Length: " << buf.st_size << "\r\n"
			<< "\r\n";
	}
	else
	{
		header 	<< "Content-Type: text/plain\r\n"
			<< "Content-Length: " << errMsg.size() << "\r\n"
			<< "\r\n";
		if (!HEAD)
			header << errMsg;
	}
	
	return header.str();
}


string Client::CreateDynamicHeader(const string& contentType, int contentLength)
{
	string header = "HTTP/1.1 200 OK\r\n";
	header += "Connection: ";
	if (keepAlive)
		header += "keep-alive\r\n";
	else
		header += "close\r\n";
	header += "Server: " + Server::getName() + "\r\nContent-Type: " + contentType;
	char buffer[128];
	cout << "length: " << contentLength;
	sprintf(buffer,"\r\nContent-Length: %d\r\n\r\n", contentLength);
	return header + buffer;
}

void Client::ProcessGetRequest(vector<string>& lines) 
{	
	string data;
	
	if (requestedFile[requestedFile.size()-1] == '/') // directory
		DisplayDirectoryContents(requestedFile);
	else  // file
	{
		struct stat buf;
		if (stat(requestedFile.c_str(), &buf) < 0) 
		{
			perror("stat error");
			return;
		}
		if (S_ISREG(buf.st_mode))
		{
			data = CreateHeader(200);
			clientSocketHandler->send((unsigned char *)data.c_str(), data.length());
			DisplayFileContents(requestedFile);
		}
		else {
			data = CreateHeader(404);
			clientSocketHandler->send((unsigned char *)data.c_str(), data.length());			
		}
	}
}

void Client::ProcessHeadRequest(vector<string>& lines)
{
	string data;
	if (requestedFile[requestedFile.size()-1] == '/') // directory
		DisplayDirectoryContents(requestedFile,false);
	else  // file
	{
		struct stat buf;
		if (stat(requestedFile.c_str(), &buf) < 0) 
		{
			perror("stat error");
			return;
		}
		if (S_ISREG(buf.st_mode)) {			
			data = CreateHeader(200);
			clientSocketHandler->send((unsigned char *)data.c_str(), data.length());
		}
		else {
			data = CreateHeader(404);
			clientSocketHandler->send((unsigned char *)data.c_str(), data.length());			
		}
	}
}

// Evaluates strings like /something/../index.html into /index.html
void Client::StripDoubleDots(string& s) {
	startPos = 0;
	while ((endPos = s.find("../")) != string::npos) {
		if (endPos == 0) {
			s.replace(0, 3, "");
		} else {
			startPos = s.find_last_of("/", endPos-1);
			if (startPos != string::npos) {
				s.replace(startPos+1, endPos - startPos + 3, "");
			} 
		}
	}
}

int Client::DisplayFileContents(const string& fileName)
{
	int infd;
	size_t n;
	unsigned char buf[4096];

	if ( (infd = open(fileName.c_str(), O_RDONLY)) < 0)
	{
		perror("Failed opening file for reading");
		pthread_mutex_unlock(&filesLock);
		return -1;
	}

	while ( (n = read(infd, buf, sizeof(buf))) > 0) 
		clientSocketHandler->send(buf, n);

	close(infd);
}

int Client::DisplayDirectoryContents(const string& path, bool GET) {
	DIR *dirp = 0;
	struct dirent *direntp = 0;
	int indent_size = 0;
	struct passwd *pwd = 0;
	struct group *grp = 0;
	vector<struct stat *> entries;	// Pointers to struct stat
	vector<string*> entryNames;
	string data;
	
	// Open the directory for reading
	if ((dirp = opendir(path.c_str())) == NULL) 
		return -1;
		
	// For each directory entry, calculate the printing indentations and mark if its a directory in the directories array
	int count = 0;
	while ((direntp = readdir(dirp)) != NULL) {
		entries.push_back(new struct stat);
		if (lstat((path + string(direntp->d_name)).c_str(), entries[count]) == -1) {
			delete entries[count];
			entries.pop_back();
			continue;
		} else {
			entryNames.push_back(new string(direntp->d_name));
			if (DIGLEN(entries[count]->st_size) > indent_size) {
				indent_size = DIGLEN(entries[count]->st_size);
			}
			count++;
		}
	}
	
	ostringstream datastr;
	datastr << "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//EN\" \"http://www.w3.org/TR/html4/loose.dtd\">"
		<< "<html>"
		<< "<head>"
		<< "<meta http-equiv=\"Content-Type\" content=\"text/html; charset=iso-8859-1\">"
		<< "<title>" << path << "Untitled Document</title>"
		<< "</head>"
		<< "<body>"
		<< "<h1>Listing of directory /" << ( requestedFile == "./" ? "" : requestedFile ) << "</h1>"
		<< "<hr>"
		<< "<pre>" << endl;
	
	for (count = 0; count < entries.size(); count++) 
	{
		if(*(entryNames[count]) == "." || ! (S_ISREG(entries[count]->st_mode) || S_ISDIR(entries[count]->st_mode)) )
		{
			delete entryNames[count];
			delete entries[count];
			continue;
		}
		
		char* mTime = ctime(&entries[count]->st_mtime);
		mTime[strlen(mTime)-1] = '\0';
		datastr << " " << setw(indent_size+1) << entries[count]->st_size << " " << mTime;
		datastr << " " << "<a href=\"" << *(entryNames[count]); 
		
		if (S_ISDIR(entries[count]->st_mode)) 
			datastr << "/\">" << *(entryNames[count]) << "/";
		else
			datastr	<< "\">" << *(entryNames[count]);
			
		datastr << "</a>\r\n";

 		delete entryNames[count];
 		delete entries[count];
 	}	
	datastr << "</pre><hr></body></html>\r\n";

	data = CreateDynamicHeader("text/html",datastr.str().size());
	clientSocketHandler->send((unsigned char *)data.c_str(), data.length());
	if (GET)
		clientSocketHandler->send((unsigned char *)datastr.str().c_str(), datastr.str().length());
		
	closedir(dirp);
	return 0;
}


