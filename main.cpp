/*
 * This software is licenced under the GNU GENERAL PUBLIC LICENSE (http://www.gnu.org/licenses/gpl.txt).
 *
 * @author Christos Constantinou, sliqer@gmail.com, May 15, 2007
 */

#include <iostream>
#include <csignal>
#include <unistd.h>
#include <errno.h> 
#include "Client.h"
#include "Server.h"
#include "Common.h"

using namespace std;

////////////////////////////////////////////////////////////////////////////////
// Configuration Options
////////////////////////////////////////////////////////////////////////////////

#define		DEFAULTPORT	80
#define		DEFAULTPOOLSIZE	40

////////////////////////////////////////////////////////////////////////////////
// Forward Declarations
////////////////////////////////////////////////////////////////////////////////

int Initialize();
void Cleanup();
void SignalHandler();

////////////////////////////////////////////////////////////////////////////////
// Global variables
////////////////////////////////////////////////////////////////////////////////

bool 		g_running 	= true;
string *	g_servedir 	= 0;  
Server *	g_server 	= 0;

FILE *		g_logstream 	= 0;         
string *	g_logfile 	= 0;
int 		g_logfd		= -1;	// Logfile file descriptor

pthread_mutex_t g_lock 		= PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t 	g_wait 		= PTHREAD_COND_INITIALIZER;
pthread_mutex_t g_locklist 	= PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t g_logMutex 	= PTHREAD_MUTEX_INITIALIZER;

////////////////////////////////////////////////////////////////////////////////
// Function Definitions
////////////////////////////////////////////////////////////////////////////////

void SignalHandler(int sig) {
	switch(sig) {
	case SIGHUP:
                g_running = false;
		Client::setRunning(false);
                g_server->Terminate();
		cerr << "hangup signal catched" << endl;
		break;
	case SIGTERM:
                g_running = false;
		Client::setRunning(false);
                g_server->Terminate();
		cerr << "terminate signal catched" << endl;
		break;
	}
}

int InitializeDaemon() {        
        int i, lfp;
        char str[10];

        // Become a daemon
        if(getppid() == 1) return(0); /* already a daemon */
        i = fork();
        if (i<0) {
                cerr << "fork() error\n";
                return(-1); 
        }
        if (i>0) {
		safe_delete(g_servedir);	// This should be the only var the parent has used
		exit(0); 			// Parent exits
	}
        
	// Child (daemon) continues 
	setsid(); /* obtain a new process group */
        
        // Reset all inherited file descriptors
       	for (i = getdtablesize()-1; i >= 3; i--) close(i); 
        
        // Set up log file
	umask(027);                     // Set newly created file permissions
        if (g_logfile != 0) {
                g_logstream = fopen(g_logfile->c_str(), "a");
                g_logfd = fileno(g_logstream);
                if(g_logfd == -1) {
                        perror(g_logfile->c_str());
                        return(-1);
                }
        }
        
        // Change into serving directory
        if (chdir(g_servedir->c_str()) == -1) {	
		perror(g_servedir->c_str());
                return(-1);
        }
        
        // Set up signal Handling
	signal(SIGCHLD,SIG_IGN); /* ignore child */
	signal(SIGTSTP,SIG_IGN); /* ignore tty signals */
	signal(SIGTTOU,SIG_IGN);
	signal(SIGTTIN,SIG_IGN);
	signal(SIGHUP,SignalHandler); /* catch hangup signal */
	signal(SIGTERM,SignalHandler); /* catch kill signal */
}

////////////////////////////////////////////////////////////////////////////////
// Main
////////////////////////////////////////////////////////////////////////////////

int main(int argc, char *argv[]) {
        if (argc < 2) {
                cout << "\nUsage: " << argv[0] << " <serve directory> [OPTIONS]\n\n" <<
                        "OPTIONS:\n" <<
                        "       -p <number>             Port number that the server will listen to. Default is 80.\n" <<
                        "       -t <number>             Size of thread pool. Default is 40.\n" <<
                        "       -l <file>               A writtable file for server messages.\n\n";
                return -1;
        }
        
        int err = 0;
	IncomingConnection *conn = 0;
	list<IncomingConnection*> incomingConnections;   
        uint32_t maxthreads = sysconf(_SC_THREAD_THREADS_MAX);
	
        // Allocate space for the parameters and set the default values
        string 		*parameters[2] 	= { 0 };
	uint16_t 	listenport 	= DEFAULTPORT;
	size_t 		poolsize 	= DEFAULTPOOLSIZE;        
	
        // Get the directory to serve and check if its readable
        g_servedir = new string(argv[1]);
        if (access(g_servedir->c_str(), R_OK) != 0) {
                perror(g_servedir->c_str());
		goto cleanup;
        }

        // Traverse the options if any, setting global variables accordingly
        for (int count = 2; count+1 < argc; count += 2) {          // We are reading the parameters 2 at a time
                parameters[0] = new string(argv[count]);
                parameters[1] = new string(argv[count+1]);
                if (*parameters[0] == "-p") {
                        listenport = atoi(parameters[1]->c_str());
                } else
                if (*parameters[0] == "-t") {                        
                        poolsize = atoi(parameters[1]->c_str());
                } else
                if (*parameters[0] == "-l") {
                        g_logfile = parameters[1];
                        delete parameters[0];
                        continue;
                }
                delete parameters[0];
                delete parameters[1];
        }
	
	// Check if the specified pool size is valid
        if (poolsize > maxthreads) {
                cerr << "This system does not allow more than " << maxthreads << " threads.\n";
		goto cleanup;
        }
     
        if (InitializeDaemon() == -1) goto cleanup; 

        //
        // Instantiate the webserver but do not accept connections yet
        //
        try {
                g_server = new Server(listenport);
        } catch (BindException&) {
                perror("bind");
                if (listenport < 1024)
                        cout << "You might want to try a port bigger than 1023\n";
                else
                        cout << "You might want to try a different port\n";
        } catch (SocketException&) {
                perror("socket");
        } catch (ListenException&) {
                perror("listen");
        } catch (SetsockoptException&) {
                perror("setsockopt");
        }
        if (g_server == 0) goto cleanup;
        
        cout << "Web server started on port " << listenport << endl;
        if (g_logfile != 0) cout << "Logfile is " << *g_logfile << endl;
 
	Client::setRunning(true);
	Client::setSharedList(&incomingConnections);
	
        //
        // Create the thread pool
        //
        Client *pool[poolsize];
        for (size_t i = 0; i < poolsize; i++) 
        {		
                pool[i] = new Client();
                try
                {
                        pool[i]->start();
                }
                catch(ThreadCreateException &e)
                {
                        e.print();
                }
        }
        
        //
        // Redirect standard file descriptors into a log file if there is one
        //
		
	int i;
	for (i = 0; i < 3; i++) close(i); 
	i = open("/dev/null",O_RDWR);   // stdin is /dev/null
	if (g_logfd >= 0) {
		dup2(g_logfd, 1);           // stdout is the logfile
		dup2(g_logfd, 2);           // stderr is the logfile
	} else {
		dup(i); dup(i);         // stdout and stderr is /dev/null                
	} 
        
        //
        // Start accepting connections
        //
	while (g_running) 
	{
		try 
		{
			conn = g_server->Accept(); // Wait for a client to connect
			
			pthread_mutex_lock(&g_locklist);
			incomingConnections.push_back(conn);
			pthread_mutex_unlock(&g_locklist);
				
			pthread_cond_signal(&g_wait);                     // Wake up one thread                
		} catch (AcceptException&) {
			if (!g_running) break;
			g_server->Rebind();
			DBG(perror("accept");)
		}
        }
        
        cleanup:
        DBG(cout << "cleaning..\n";)
	safe_delete(g_servedir);
	safe_delete(g_logfile);
	
        // Clear any awaiting connections
	while (incomingConnections.size() > 0) {		
		delete incomingConnections.front();
		incomingConnections.pop_front();
	}
	
	// Clear the thread pool
		
	Client::setRunning(false);		
        pthread_cond_broadcast(&g_wait);                                  // Wake up all threads that wait on the wait condition
        
	for (int i = 0; i < poolsize; i++)
	{
		if (pool[i]) 
		{	
			pool[i]->join();
			delete pool[i];
		}
	}	
	
        // Stop the server
        try {
                safe_delete(g_server);
                if (g_logstream > 0) fclose(g_logstream);                
        } catch (SetsockoptException&) {
                DBG(perror("setsockopt");)
        }
	
	return 0;
}
