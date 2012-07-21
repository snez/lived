#ifndef __THREAD_H__
#define __THREAD_H__

#include <pthread.h>
#include <errno.h>
#include <string.h> /* For strerror */
#include <iostream>
#include <string>
#include "Common.h"

using std::string;

/******************************************************************************/

struct ThreadException 
{ 
private:
	int err;
	string msg;
	
public:
	ThreadException(string str,int error) { err = error; msg = str; }
	void print() { perror2(msg.c_str(),err); }
};

struct ThreadCreateException : public ThreadException 
{ 
	ThreadCreateException(int i) : ThreadException("pthread_create",i){} 
};

struct ThreadJoinException : public ThreadException 
{ 
	ThreadJoinException(int i) : ThreadException("pthread_join",i){} 
};

struct ThreadDetachException : public ThreadException 
{ 
	ThreadDetachException(int i) : ThreadException("pthread_detach",i){} 
};

/******************************************************************************/

class Thread 
{     
protected:
	bool active; // a thread is active from the time it is started until the run procedure finishes
	pthread_t threadID; // the thread id
	static void* threadProc(void* param); // thread starting procedure
	virtual void run() = 0;
	virtual void init()		{};	// called before starting the thread
	virtual void cleanUp() 	{};	// called before thread exiting
	
public:
	Thread() 				{ active = false; threadID = 0; }
	void start() 			throw (ThreadCreateException);
	void join()  			throw (ThreadJoinException);
	void detach()			throw (ThreadDetachException);
	virtual void stop() 	{}
	bool isActive() 		{ return active; }
	pthread_t getThreadID() { return threadID; }
	virtual ~Thread() 		{ if (active) pthread_cancel(threadID); }
};

#endif
