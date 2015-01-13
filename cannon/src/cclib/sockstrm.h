/*
 * file : sockstrm.h
 * description: encapsulate the low level socket api
 * author: zhj
 * created:2005-9
 * history:
 */


#if defined(__APPLE__) && (!defined(__unix))
        #define __unix
#endif

#ifdef __unix
#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/param.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define NETERRNO		errno
#define NETEAGAIN		EAGAIN
#define	NETETIMEDOUT	ETIMEDOUT
#define NETEINPROGRESS	EINPROGRESS
#define NETEINTR			EINTR

#elif defined (_WIN32)
#include <windows.h>
#include <winsock.h>
#define NETERRNO		GetLastError()
#define NETEAGAIN		WSAEAGAIN
#define	NETETIMEDOUT	WSAETIMEDOUT
#define NETEINPROGRESS	WSAEINPROGRESS
#define NETEINTR			WSAEINTR
#ifndef ushort
#define ushort	unsigned short
#endif
#ifndef ulong
#define ulong	unsigned long
#endif
#ifndef uint
#define uint	unsigned int
#endif
#endif

#ifndef __cplusplus
#error This file can only be compiled in C++ mode !!
#endif

#include <string>

#ifndef _sockstrm_h
#define _sockstrm_h

//#include "buffer.h"

const char* gettime () ;
int nselect(const int *fds, const int *events, int *revents, int count, 
	long millisec, void *workmem);


namespace tiny_socket {
	using namespace std ;
//	using namespace buffer ;

/**********************************************************************
 * sock_stream: socket stream implementation
 **********************************************************************/
class sock_stream
{
	public:
		sock_stream (const char * ip, ushort port) ;
		sock_stream () ;
		~sock_stream () ;
		int open (const char * ip, ushort port) ;
		int open () ;
		int open_timeo (uint timeo) ;
		int open_timeo (const char *ip, ushort port, uint timeo) ;
		void close () ;
		int fd () ;
		void fd (int fd) ;
		// readn only for bloking io 
		int readn (void * buf, size_t sz) ;
		// writen only for bloking io 
		int writen (const void * buf, size_t sz) ;
		int tryread (void * buf, size_t sz, int timeout) ;
		int trywrite (const void * buf, size_t sz, int timeout) ;
		//int setsockopt (int sk, int level, int optname, const void * optval, socklen_t optlen) ;
		//int getsockopt (int sk, int level, int optname, void * optval, socklen_t * optlen) ;
		int setlinger (int s) ;
		int setrcvtimeo (int sec, int usec) ;
		int setnonblock () ;
		int setnodelay (int nodelay);
		int error () ;
		//for linux only 
		#ifdef __linux
		int setcork () ;
		#endif 

	private:
		void set_addr (const char *ip, ushort port) ;
		int handle () ;
		int establish () ;
		void stamperror () ;
		void init () ;

	private:
		int _sk ;
		struct sockaddr_in _addr ;
		int noblock ;
		int _flag ;
} ;
}

#ifndef MAX_PACKET_SIZE
#define MAX_PACKET_SIZE  0x1600000
#endif


/**********************************************************************
 * ICACHED: The struct definition of the cache descriptor
 **********************************************************************/
struct ICACHED			/* ci-buffer type */
{
	char*data;			/* memory address */
	long size;			/* total mem-size */
	long head;			/* write pointer. */
	long tail;			/* read pointer.. */
};


/**********************************************************************
 * Macros Definition
 **********************************************************************/
#define ICACHE_DSIZE(c) (((c)->head >= (c)->tail)?  \
                         ((c)->head - (c)->tail):   \
                         (((c)->size - (c)->tail) + (c)->head))
#define ICACHE_FSIZE(c) (((c)->size - ICACHE_DSIZE(c)) - 1)


/**********************************************************************
 * Functions: Basic Operation on a cache
 **********************************************************************/
void icache_init(struct ICACHED *cache, void *buffer, long size);
long icache_dsize(const struct ICACHED *cache);
long icache_fsize(const struct ICACHED *cache);

long icache_write(struct ICACHED *cache, const void *data, long size);
long icache_read(struct ICACHED *cache, void *data, long size);

long icache_peek(const struct ICACHED *cache, void *data, long size);
long icache_drop(struct ICACHED *cache, long size);
void icache_clear(struct ICACHED *cached);
long icache_flat(const struct ICACHED *cache, void **pointer);


#endif // _sockstrm_h


