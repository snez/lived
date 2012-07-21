#include "Thread.h"
 
void* Thread::threadProc(void* param)
{
	Thread* target = reinterpret_cast<Thread*>(param);

	if (target)
	{
		target->init();
		target->run();
		target->cleanUp();
		target->active = false;		
	}
	
	pthread_exit(0);
}

void Thread::start() throw (ThreadCreateException)
{
	if (active)
		return;
	int err = pthread_create(&threadID, NULL, &threadProc, this);
	if (err)
		throw ThreadCreateException(err);
	active = true;
}

void Thread::join() throw (ThreadJoinException)		
{ 
	int err = pthread_join(threadID, NULL);
	if (err)
		throw ThreadJoinException(err);
}

void Thread::detach() throw (ThreadDetachException)
{
	int err = pthread_detach(threadID);
	if (err)
		throw ThreadDetachException(err);	
}
