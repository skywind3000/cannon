//=====================================================================
//
// The platform independence system call wrapper, Skywind 2004
// Unix/Windows 标准系统调用编程通用接口
//
// HISTORY:
// Nov. 15 2004   skywind  created
//
// NOTE：
// 提供使 Unix或者 Windows相同的系统调用编程接口主要有几个方面的包装，
// 第一是时钟、第二是线程、第三是互斥，用VC编译需要打开/MT开关，取名
// apr意再模仿apache的aprlib
//
//=====================================================================

#include "aprsys.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>


#define APR_MULTI_THREAD

//---------------------------------------------------------------------
// UNIX Header Definition
//---------------------------------------------------------------------
#if defined(__unix)
#include <sys/time.h>
#include <unistd.h>
#include <semaphore.h>
#include <pthread.h>

//---------------------------------------------------------------------
// WIN32 Header Definition
//---------------------------------------------------------------------
#elif defined(_WIN32)
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0500
#endif
#include <windows.h>
#include <time.h>

#if (defined(_MSC_VER) && (!defined(_MT)) && defined(APR_MULTI_THREAD))
#undef APR_MULTI_THREAD
#endif

#ifdef APR_MULTI_THREAD
#include <process.h>
#include <stddef.h>
#endif

#else
#error Unknow platform, only support unix and win32
#endif


//=====================================================================
// Timer and Threading Operation
//=====================================================================


//---------------------------------------------------------------------
// apr_sleep
//---------------------------------------------------------------------
APR_DECLARE(void) apr_sleep(unsigned long timems)
{
	#ifdef __unix 	// usleep( time * 1000 );
	usleep((timems << 10) - (timems << 4) - (timems << 3));
	#elif defined(_WIN32)
	Sleep(timems);
	#endif
}

//---------------------------------------------------------------------
// apr_clock
//---------------------------------------------------------------------
APR_DECLARE(long) apr_clock(void)
{
	#ifdef __unix
	static struct timezone tz={ 0,0 };
	struct timeval time;
	gettimeofday(&time,&tz);
	return (time.tv_sec*1000+time.tv_usec/1000);
	#elif defined(_WIN32)
	return clock();
	#endif
}

//---------------------------------------------------------------------
// apr_timeofday
//---------------------------------------------------------------------
APR_DECLARE(void) apr_timeofday(long *sec, long *usec)
{
	#if defined(__unix)
	static struct timezone tz={ 0,0 };
	struct timeval time;
	gettimeofday(&time,&tz);
	if (sec) *sec = time.tv_sec;
	if (usec) *usec = time.tv_usec;
	#elif defined(_WIN32)
	static long mode = 0, addsec = 0;
	BOOL retval;
	#ifdef _MSC_VER
	static __int64 freq = 1;
	__int64 qpc;
	#else
	static long long freq = 1;
	long long qpc;
	#endif
	if (mode == 0) {
		retval = QueryPerformanceFrequency((LARGE_INTEGER*)&freq);
		freq = (freq == 0)? 1 : freq;
		retval = QueryPerformanceCounter((LARGE_INTEGER*)&qpc);
		addsec = (long)time(NULL);
		addsec = addsec - (long)((qpc / freq) & 0x7fffffff);
		mode = 1;
	}
	retval = QueryPerformanceCounter((LARGE_INTEGER*)&qpc);
	retval = retval * 2;
	if (sec) *sec = (long)(qpc / freq) + addsec;
	if (usec) *usec = (long)((qpc % freq) * 1000000 / freq);
	#endif
}

//---------------------------------------------------------------------
// apr_timex
//---------------------------------------------------------------------
apr_int64 apr_timex(void)
{
	long sec, usec;
	apr_int64 timex;
	apr_timeofday(&sec, &usec);
	timex = sec;
	return timex * (apr_int64)1000000 + usec;
}

//---------------------------------------------------------------------
// judge if use multi threading
//---------------------------------------------------------------------
#ifdef APR_MULTI_THREAD

//---------------------------------------------------------------------
// apr_thread_create
//---------------------------------------------------------------------
APR_DECLARE(long) apr_thread_create(long* tid, const apr_thread_start tproc, const void *tattr, void *targs)
{
	#ifdef __unix
	pthread_t newthread;
	long ret = pthread_create((pthread_t*)&newthread, NULL, (void*(*)(void*)) tproc, targs);
	if (tid) *tid = (long)newthread;
	if (ret) return -1;
	#elif defined(_WIN32)
	apr_uint32 Handle;
	Handle = (apr_uint32)_beginthread((void(*)(void*))tproc, 0, targs);
	if (tid) *tid = (long)Handle;
	if (Handle == 0) return -1;
	#endif
	return APR_SUCCESS;
}

//---------------------------------------------------------------------
// apr_thread_exit
//---------------------------------------------------------------------
APR_DECLARE(long) apr_thread_exit(long retval)
{
	#ifdef __unix
	pthread_exit(NULL);
	#elif defined(_WIN32)
	_endthread();
	#endif
	return APR_SUCCESS;
}

//---------------------------------------------------------------------
// apr_thread_join
//---------------------------------------------------------------------
APR_DECLARE(long) apr_thread_join(long threadid)
{
	long status;
	#ifdef __unix
	void *lptr = &status;
	status = pthread_join((pthread_t)threadid, lptr);
	#elif defined(_WIN32)
	status = WaitForSingleObject((HANDLE)threadid, INFINITE);
	#endif
	return status;
}

//---------------------------------------------------------------------
// apr_thread_detach
//---------------------------------------------------------------------
APR_DECLARE(long) apr_thread_detach(long threadid)
{
	long status;
	#ifdef __unix
	status = pthread_detach((pthread_t)threadid);
	#elif defined(_WIN32)
	CloseHandle((HANDLE)threadid);
	status = APR_SUCCESS;
	#endif
	return status;
}

//---------------------------------------------------------------------
// apr_thread_kill
//---------------------------------------------------------------------
APR_DECLARE(long) apr_thread_kill(long threadid)
{
	#ifdef __unix
	pthread_cancel((pthread_t)threadid);
	#elif defined(_WIN32)
	TerminateThread((HANDLE)threadid, 0);
	#endif
	
	return 0;
}

#endif
// end of multi threading implement


//=====================================================================
// Mutex Definition
//=====================================================================

#ifdef __unix
#define APR_MUTEX_TYPE pthread_mutex_t
#define APR_MUTEX_INIT(p)		pthread_mutex_init(p, NULL)
#define APR_MUTEX_LOCK(p)		pthread_mutex_lock(p)
#define APR_MUTEX_UNLOCK(p)		pthread_mutex_unlock(p)
#define APR_MUTEX_TRYLOCK(p)    pthread_mutex_trylock(p)
#define APR_MUTEX_DESTROY(p)	pthread_mutex_destroy(p)
#else

#define APR_MUTEX_TYPE CRITICAL_SECTION
#define APR_MUTEX_INIT(p)		InitializeCriticalSection(p)
#define APR_MUTEX_LOCK(p)		EnterCriticalSection(p)
#define APR_MUTEX_UNLOCK(p)		LeaveCriticalSection(p)
#define APR_MUTEX_TRYLOCK(p)    ((TryEnterCriticalSection(p))? 0 : -1)
#define APR_MUTEX_DESTROY(p)    DeleteCriticalSection(p)
#endif

#define APR_MUTEX_NEXT(p) (*((char**)p))

#define APR_MUTEX_BSIZE (APR_MAX_MUTEX * sizeof(APR_MUTEX_TYPE))
// MUTEX BUFFER
static char apr_mutexs[APR_MUTEX_BSIZE];
static char*apr_mutex_start;
static APR_MUTEX_TYPE apr_mutex_atomic;

//---------------------------------------------------------------------
// INIT MUTEX BUFFER
//---------------------------------------------------------------------
static APR_DECLARE(void) apr_mutex_startup(void)
{
	static int inited = 0;
	int i;
	char *p;
	if (inited) return;
	apr_mutex_start = apr_mutexs;
	for (p = apr_mutexs, i = 0; i < APR_MAX_MUTEX; i++) {
		if (i >= APR_MAX_MUTEX - 1) 
			APR_MUTEX_NEXT(p) = NULL;
		else
			APR_MUTEX_NEXT(p) = p + sizeof(APR_MUTEX_TYPE);
		p += sizeof(APR_MUTEX_TYPE);
	}	
	inited = 1;

	APR_MUTEX_INIT(&apr_mutex_atomic);
}

//---------------------------------------------------------------------
// ALLOC MUTEX
//---------------------------------------------------------------------
static APR_DECLARE(APR_MUTEX_TYPE*) apr_mutex_new(void)
{
	APR_MUTEX_TYPE *retval = NULL;

	apr_mutex_startup();

	APR_MUTEX_LOCK(&apr_mutex_atomic);
	if (apr_mutex_start) {
		retval = (APR_MUTEX_TYPE*)((void*)apr_mutex_start);
		apr_mutex_start = APR_MUTEX_NEXT(((void*)apr_mutex_start));
	}
	APR_MUTEX_UNLOCK(&apr_mutex_atomic);

	return retval;
}

//---------------------------------------------------------------------
// FREE MUTEX
//---------------------------------------------------------------------
static APR_DECLARE(void) apr_mutex_free(char *p)
{
	char *m = p;
	apr_mutex_startup();
	
	assert((p >= apr_mutexs && p < apr_mutexs + APR_MUTEX_BSIZE));

	APR_MUTEX_LOCK(&apr_mutex_atomic);
	APR_MUTEX_NEXT(((void*)m)) = apr_mutex_start;
	apr_mutex_start = p;
	APR_MUTEX_UNLOCK(&apr_mutex_atomic);
}

//---------------------------------------------------------------------
// apr_mutex_create
//---------------------------------------------------------------------
APR_DECLARE(long)      apr_mutex_init(apr_mutex *m)
{
	APR_MUTEX_TYPE *mutex;

	if (m == NULL) return -1;
	mutex = apr_mutex_new();
	if (mutex == NULL) return -2;

	APR_MUTEX_INIT(mutex);
	*(APR_MUTEX_TYPE**)m = mutex;

	return APR_SUCCESS;
}

//---------------------------------------------------------------------
// apr_mutex_lock
//---------------------------------------------------------------------
APR_DECLARE(long)      apr_mutex_lock(apr_mutex m)
{
	if (m == NULL) return -1;
	
	APR_MUTEX_LOCK((APR_MUTEX_TYPE*)m);

	return APR_SUCCESS;
}

//---------------------------------------------------------------------
// apr_mutex_unlock
//---------------------------------------------------------------------
APR_DECLARE(long)      apr_mutex_unlock(apr_mutex m)
{
	if (m == NULL) return -1;

	APR_MUTEX_UNLOCK((APR_MUTEX_TYPE*)m);

	return APR_SUCCESS;
}

//---------------------------------------------------------------------
// apr_mutex_trylock
//---------------------------------------------------------------------
APR_DECLARE(long)      apr_mutex_trylock(apr_mutex m)
{
	int retval = 0;
	if (m == NULL) return -1;

	#if defined(__unix)
	retval = APR_MUTEX_TRYLOCK((APR_MUTEX_TYPE*)m);

	#elif defined(_WIN32)

	#ifdef _MSC_VER
	#if (_WIN32_WINNT >= 0x0400)
	retval = APR_MUTEX_TRYLOCK((APR_MUTEX_TYPE*)m);
	#else 
	retval = -1;
	#endif // #if (_WIN32_WINNT >= 0x0400)
	#else  // #ifdef _MSC_VER
	retval = APR_MUTEX_TRYLOCK((APR_MUTEX_TYPE*)m);
	#endif // #ifdef _MSC_VER

	#endif // #elif defined(_WIN32)

	return retval;
}

//---------------------------------------------------------------------
// apr_mutex_destroy
//---------------------------------------------------------------------
APR_DECLARE(long)      apr_mutex_destroy(apr_mutex m)
{
	if (m == NULL) return APR_SUCCESS;

	APR_MUTEX_DESTROY((APR_MUTEX_TYPE*)m);

	apr_mutex_free((char*)m);

	return APR_SUCCESS;
}


