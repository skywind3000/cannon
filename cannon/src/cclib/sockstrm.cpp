
/*
 * file : sockstrm.cpp
 * description: encapsulate the low level socket api
 * author: zhj
 * created:2005-9
 * history:
 */

//you should play this header before others 
#include "sockstrm.h"

#if defined(__APPLE__) && (!defined(__unix))
        #define __unix
#endif

#ifdef __unix
#include <unistd.h>
#include <fcntl.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <netinet/tcp.h>
#include <sys/errno.h>
#include <sys/poll.h>
#include <signal.h>
#include <time.h>
#include <stdio.h>
#include <errno.h>
#elif defined (_WIN32)
#include <time.h>

#else 
#error Not support 
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>


/*
static void sig_pipe (int signo)
{
		fprintf (stderr, "[ERROR] [socket] catch signal:%d(SIGPIPE)\n", signo) ;
}
*/

#ifdef _MSC_VER
#pragma warning(disable:4996)
#endif

const char * gettime ()
{
		static char buf[256] ;
		time_t t = time (NULL) ;
	    struct tm * ptm = localtime (&t) ;

        strftime (buf, sizeof(buf), "%Y%m%d-%H:%M:%S", ptm) ;
		return buf ;
}


namespace tiny_socket 
{

	void sock_stream::init () 
	{ 
		_sk = -1 ;
		_flag = -1 ; 
		noblock = 0 ;
		// ::signal (SIGPIPE, sig_pipe) ;
	}
	
	sock_stream::sock_stream ()
	{
		init () ;
	}

	sock_stream::sock_stream (const char *ip, ushort port) 
	{
		init () ;
		set_addr (ip, port) ;

	}

	sock_stream::~sock_stream() { close () ; }
	
	
	void sock_stream::stamperror ()
	{
		#ifdef __unix
		// perror ("[ERROR] [socket]") ;
		fprintf (stderr, "[%s][ERROR]socket broken.errno:%d,strerror:%s\n", 
						gettime(), errno, strerror(errno)) ;
						

		#endif
	}

	int sock_stream::fd () { return _sk ; } 

	void sock_stream::fd (int fd) {  
		close () ;
		_sk = fd; 
	} 


	void sock_stream::set_addr (const char *ip, ushort port) 
	{
		_addr.sin_family = AF_INET ;
		_addr.sin_port = htons (port) ;
		//if (0 == ::inet_aton (ip.c_str(), &_addr.sin_addr)) {
		#ifdef __unix
		int r = ::inet_pton (AF_INET, ip, &_addr.sin_addr) ;
		#elif defined (_WIN32)
		_addr.sin_addr.s_addr = inet_addr (ip) ;
		int r = 1 ;
		#endif

		if (0 == r) {
			//std::cerr << "invalid ip address,the valid format: a.b.c.d" << endl ;
		} else if ( -1 == r) {
			stamperror () ;
		}

	}

	int sock_stream::open (const char *ip, ushort port) 
	{
		set_addr (ip, port) ;
		return open () ;

	}

	int sock_stream::handle ()
	{
		unsigned long revalue1 = 1;
		 _sk = ::socket (AF_INET, SOCK_STREAM, 0) ;
		if (_sk < 0) {
			stamperror () ;
			return -1 ;
		}
		setsockopt(_sk, SOL_SOCKET, SO_REUSEADDR, (char*)&revalue1, sizeof(revalue1));

		return 0 ;
	}

	int sock_stream::establish () 
	{
		int i = ::connect (_sk, (sockaddr*)&_addr, sizeof (sockaddr_in) ) ;
		if (i < 0) {
			stamperror () ;
			return -1 ;
		}
		return 0 ;
	}

	int sock_stream::open () 
	{
		if ( 0 > handle ()) {
			return -1 ;
		}
		if ( 0 > establish ()) {
			return -1 ;
		}

		return 0 ;
	}

	int sock_stream::open_timeo (const char *ip, ushort port, uint timeo) 
	{
		set_addr (ip, port) ;
		return open_timeo (timeo) ;

	}

	int sock_stream::setnonblock() 
	{
		int ret ;
		#ifdef __unix
		_flag = ::fcntl (_sk, F_GETFL) ;
		ret = ::fcntl (_sk, F_SETFL, _flag|O_NONBLOCK) ;
		#else
		unsigned long t = 1 ;
		ret = ioctlsocket (_sk, FIONBIO, &t) ; 
		#endif
		if ( ret < 0) {
			stamperror () ;
			return -1 ;
		}
		noblock = 1;
		return 0 ;
	}

	int sock_stream::open_timeo (uint timeo)
	{
		#ifdef __unix 
		int flag, sret, val, fret ;
		socklen_t len = sizeof(val) ;
		struct timeval tv ;

		tv.tv_sec = timeo ;
		tv.tv_usec = 0 ;

		if ( 0 > handle ()) {
			return -1 ;
		}

		
		flag = ::fcntl (_sk, F_GETFL) ;
		fret = ::fcntl (_sk, F_SETFL, flag|O_NONBLOCK) ;
		
		if (fret < 0) {
			stamperror () ;
			return -1 ;
		}

		if (0 == establish()) {
			goto done ; //connect successfully
		} else if (NETEINPROGRESS != NETERRNO) {
			stamperror () ;
			return -1 ;
		}
		
		fd_set fds ;
		FD_ZERO (&fds) ;
		FD_SET (_sk, &fds) ;
		if (0 > (sret = ::select (_sk + 1, &fds, &fds, NULL, &tv))) {
			stamperror () ;
			return -1 ;
		}
		if ( 0 == sret) {
			
			NETERRNO = NETETIMEDOUT ;
			stamperror () ;
			return -1 ;
		}

		
		getsockopt (_sk, SOL_SOCKET, SO_ERROR, &val, &len) ;
		
		if (0 != val) {
			NETERRNO = val ;
			stamperror () ;
			return -1 ;
		}

	done:
		if (0 > ::fcntl (_sk, F_SETFL, flag)) {
			stamperror () ;
			return -1 ;
		}
	#endif

		return 0 ;
	}

		
	void sock_stream::close () 
	{
		if (_sk >= 0) {
#ifdef __unix
			::close (_sk) ;
#else 
			closesocket (_sk) ;
#endif

		}
		_sk = -1;
	}

	// for blocking read only
	// eqivalent to recv with flag MSG_WAITALL
	int sock_stream::readn (void * buf, size_t sz) 
	{
		int eat = 0 ;
		int c = 0 ;
		char * p = (char*)buf ;

		while (sz > 0) {
			c = ::recv (_sk, p + eat, sz, 0) ;
			if (c < 0) {
				if (NETEINTR == NETERRNO) 
					continue ;
				stamperror () ;
				return c ;
			} else  if (0 == c) {
				break ;// connection is shutdown, return the data we have read
			}

			sz -= c ;
			eat += c ;
		}

		return eat ;
	}
			
	// for blocking write 	
	int sock_stream::writen (const void * buf, size_t sz) 
	{
		int eat = 0 ;
		int c = 0 ;
		char * p = (char*)buf ;
	
		while (sz > 0) {
			c = ::send (_sk, p + eat, sz, 0) ;
			if (c < 0) {
				if (NETEINTR == NETERRNO) 
					continue ;
				stamperror () ;
				return c ;
			}
			sz -= c ;
			eat += c ;
		}
		
		return eat ;
	}

	
	int sock_stream::setlinger (int s) 
	{
		struct linger lg ;
		lg.l_onoff = 1 ;
		lg.l_linger = s ;
		
		int ret = setsockopt (_sk, SOL_SOCKET, SO_LINGER, (char*)&lg, sizeof(linger))  ;
		if (ret < 0) {
			stamperror () ;
			return -1 ;
		}
		return 0 ;
	}
	//for linux only
	#ifdef __linux
	int sock_stream::setcork () 
	{
		long on = 1 ;
		int ret = setsockopt(_sk, SOL_TCP, TCP_CORK, &on, sizeof(on)); 
		if (ret < 0) {
			stamperror () ;
			return -1 ;
		}

		return 0 ;
	}
	#endif 

	int sock_stream::setrcvtimeo (int sec, int usec)
	{
		struct timeval to ;
		to.tv_sec = sec ;
		to.tv_usec = usec ;

		int ret = setsockopt (_sk, SOL_SOCKET, SO_RCVTIMEO, (char*)&to, sizeof (timeval)) ;
		if (ret < 0) {
			stamperror () ;
			return -1 ;
		}
		return 0 ;
	}

	int sock_stream::tryread(void *data, size_t sz, int timeout)
	{
		int len ;
		if (0 == sz) return 0 ;

		if (timeout == 0) {
			#ifdef __unix
			int ret ;
			pollfd pfd ;

			pfd.fd = _sk ;
			pfd.events = (POLLIN | POLLERR | POLLHUP) ;

			ret = poll (&pfd, 1, timeout) ;
			if (0 > ret) {
				if (NETERRNO == NETEINTR) 
					return 0 ;
				stamperror () ;
				return -1 ;
			}
			// poll timeout
			if (0 == ret)  return 0 ; 
			#else
			struct timeval tmx = { timeout / 1000, (timeout % 1000) * 1000 };
			int fdr[2] = { 1, _sk };
			int fde[2] = { 1, _sk };
			void *dr = fdr;
			void *de = fde;

			select (_sk + 1, (fd_set*)dr, NULL, (fd_set*)de, (timeout >= 0)? &tmx : NULL) ;
			if (!(fdr[0] || fde[0])) return 0 ;
			#endif
		}

		len = ::recv (_sk, (char*)data, sz, 0) ;

		if (len < 0) {
			if (NETERRNO == NETEINTR) 
				return 0 ;
			stamperror () ;
			return -1 ;
		} 
		if (len == 0) 
			return -2 ;

		return len ;
		
	}
		
	
	int sock_stream::trywrite(const void *data, size_t sz, int timeout)
	{
		int len ;

		if (0 == sz) return 0 ;

		#ifdef __unix
		int ret ;
		pollfd pfd ;

		pfd.fd = _sk ;
		pfd.events = (POLLOUT | POLLERR | POLLHUP) ;

		ret = poll (&pfd, 1, timeout) ;
		if (0 > ret) {
			if (NETERRNO == NETEINTR) 
				return 0 ;
			stamperror () ;
			return -1 ;
		}
		
		// poll timeout
		if (0 == ret)  return 0 ; 
		#else
		struct timeval tmx = { timeout / 1000, (timeout % 1000) * 1000 };
		int fdw[2] = { 1, _sk };
		int fde[2] = { 1, _sk };
		void *dw = fdw;
		void *de = fde;

		select (_sk + 1, NULL, (fd_set*)dw, (fd_set*)de, (timeout >= 0)? &tmx : NULL) ;
		if (!(fdw[0] || fde[0])) return 0 ;
		#endif

		len = ::send (_sk, (char*)data, sz, 0) ;
		if (len == 0) {//
			return -2 ;
		} else if (len < 0) {
			if (NETERRNO == NETEINTR) 
				return 0 ;
			stamperror () ;
			return -1 ;
		} 

		return len ;
	}
		
	int sock_stream::setnodelay (int nodelay) {
		unsigned long value = nodelay;
		int retval = -2000;
		if (_sk < 0) return -1000;
		#ifndef __AVM3__
		retval = setsockopt(_sk, (int)IPPROTO_TCP, TCP_NODELAY, 
			(char*)&value, sizeof(value));
		#endif
		return retval;
	}
}

int nselect(const int *fds, const int *events, int *revents, int count, 
	long millisec, void *workmem)
{
	int retval = 0;
	int i;

	if (workmem == NULL) {
		#ifdef __unix
		return count * sizeof(struct pollfd);
		#else
		return (count + 1) * sizeof(int) * 3;
		#endif
	}	
	else {
		#ifdef __unix
		struct pollfd *pfds = (struct pollfd*)workmem;

		for (i = 0; i < count; i++) {
			pfds[i].fd = fds[i];
			pfds[i].events = 0;
			pfds[i].revents = 0;
			if (events[i] & 1) pfds[i].events |= POLLIN;
			if (events[i] & 2) pfds[i].events |= POLLOUT;
			if (events[i] & 4) pfds[i].events |= POLLERR;
		}

		poll(pfds, count, millisec);

		for (i = 0; i < count; i++) {
			int event = pfds[i].revents;
			int revent = 0;
			if (event & POLLIN) revent |= 1;
			if (event & POLLOUT) revent |= 2;
			if (event & POLLERR) revent |= 4;
			revents[i] = revent & event;
			if (revents[i]) retval++;
		}

		#else
		struct timeval tmx = { 0, 0 };
		int *fdr = (int*)workmem;
		int *fdw = fdr + 1 + count;
		int *fde = fdw + 1 + count;
		void *dr, *dw, *de;
		int maxfd = 0;
		int j;

		fdr[0] = fdw[0] = fde[0] = 0;

		for (i = 0; i < count; i++) {
			int event = events[i];
			int fd = fds[i];
			if (event & 1) fdr[++fdr[0]] = fd;
			if (event & 2) fdw[++fdw[0]] = fd;
			if (event & 4) fde[++fde[0]] = fd;
			if (fd > maxfd) maxfd = fd;
		}

		dr = fdr[0]? fdr : NULL;
		dw = fdw[0]? fdw : NULL;
		de = fde[0]? fde : NULL;

		tmx.tv_sec = millisec / 1000;
		tmx.tv_usec = (millisec % 1000) * 1000;

		select(maxfd + 1, (fd_set*)dr, (fd_set*)dw, (fd_set*)de, 
			(millisec >= 0)? &tmx : 0);

		for (i = 0; i < count; i++) {
			int event = events[i];
			int fd = fds[i];
			int revent = 0;
			if (event & 1) {
				for (j = 0; j < fdr[0]; j++) {
					if (fdr[j + 1] == fd) { revent |= 1; break; }
				}
			}
			if (event & 2) {
				for (j = 0; j < fdw[0]; j++) {
					if (fdw[j + 1] == fd) { revent |= 2; break; }
				}
			}
			if (event & 4) {
				for (j = 0; j < fde[0]; j++) {
					if (fde[j + 1] == fd) { revent |= 4; break; }
				}
			}
			revents[i] = revent;
			if (revent) retval++;
		}
		#endif
	}

	return retval;
}



void icache_init(struct ICACHED *cache, void *buffer, long size)
{
	cache->data = (char*)buffer;
	cache->size = size;
	cache->head = 0;
	cache->tail = 0;
}

long icache_dsize(const struct ICACHED *cache)
{
	assert(cache);
	return ICACHE_DSIZE(cache);
}

long icache_fsize(const struct ICACHED *cache)
{
	assert(cache);
	return ICACHE_FSIZE(cache);
}

long icache_write(struct ICACHED *cache, const void *data, long size)
{
	char *lptr = (char*)data;
	long dsize, dfree, half;

	dsize = ICACHE_DSIZE(cache);
	dfree = (cache->size - dsize) - 1;
	if (dfree <= 0) return 0;

	size = (size < dfree)? size : dfree;
	half = (cache->size - cache->head);

	if (half >= size) {
		memcpy(cache->data + cache->head, lptr, (size_t)size);
	} else {
		memcpy(cache->data + cache->head, lptr, (size_t)labs(half));
		memcpy(cache->data, lptr + half, (size_t)(size - half));
	}
	cache->head += size;
	if (cache->head >= cache->size) cache->head -= cache->size;

	return size;
}

long icache_peek(const struct ICACHED *cache, void *data, long size)
{
	char *lptr = (char*)data;
	long dsize, half;

	dsize = ICACHE_DSIZE(cache);
	if (dsize <= 0) return 0;

	size = (size < dsize)? size : dsize;
	half = cache->size - cache->tail;

	if (half >= size) {
		memcpy(lptr, cache->data + cache->tail, (size_t)size);
	}	else {
		memcpy(lptr, cache->data + cache->tail, (size_t)labs(half));
		memcpy(lptr + half, cache->data, (size_t)(size - half));
	}

	return size;
}

long icache_read(struct ICACHED *cache, void *data, long size)
{
	long nsize;
	
	nsize = icache_peek(cache, data, size);
	if (nsize <= 0) return nsize;

	cache->tail += nsize;
	if (cache->tail >= cache->size) cache->tail -= cache->size;

	return nsize;
}

long icache_drop(struct ICACHED *cache, long size)
{
	long dsize ;

	dsize = ICACHE_DSIZE(cache);
	if (dsize <= 0) return 0;

	size = (size < dsize)? size : dsize;
	cache->tail += size;
	if (cache->tail >= cache->size) cache->tail -= cache->size;

	return size;
}

long icache_flat(const struct ICACHED *cache, void **pointer)
{
	long dsize, half;

	dsize = ICACHE_DSIZE(cache);
	if (dsize <= 0) return 0;

	half = cache->size - cache->tail;
	if (pointer) *pointer = (void*)(cache->data + cache->tail);

	return (half <= dsize)? half : dsize;
}


