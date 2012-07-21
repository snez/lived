#ifndef __COMMON_H__
#define __COMMON_H__

#ifdef DEGUB
	#define DBG(x) { x }
#else
	#define DBG(x) 
#endif

#define safe_delete(x) { if (x > 0) { delete x; x = 0; } }
#define perror2(s, e) fprintf(stderr, "%s: %s\n", s, strerror(e))
#define logWrite(a) { pthread_mutex_lock(&g_logMutex); a; pthread_mutex_unlock(&g_logMutex); }

#endif
