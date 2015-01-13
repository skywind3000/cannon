//=====================================================================
//
// The common interface of socket for Unix and Windows
// Unix/Windows 标准 socket编程通用接口
//
// HISTORY:
// Nov. 15 2004   skywind  created this file
// Dec. 17 2005   skywind  support apr_*able, apr_poll
//
// NOTE：
// 提供使 Unix或者 Windows相同的 socket编程接口主要有许多地方不一样，
// 这里提供相同的访问方式，起名apr_sock意再模仿apache的aprlib
//
//=====================================================================

#include "aprsock.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <assert.h>

#ifdef __unix
#include <netdb.h>
#include <sched.h>
#include <pthread.h>
#include <dlfcn.h>
#include <unistd.h>
#include <netinet/in.h>

#ifndef __AVM3__
#include <poll.h>
#include <netinet/tcp.h>
#include <fcntl.h>
#endif

#if defined(__sun)
#include <sys/filio.h>
#endif

#elif defined(_WIN32) || defined(WIN32) || defined(_WIN64) || defined(WIN64)
#if ((!defined(_M_PPC)) && (!defined(_M_PPC_BE)) && (!defined(_XBOX)))
#include <mmsystem.h>
#include <mswsock.h>
#include <process.h>
#include <stddef.h>
#ifdef _MSC_VER
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma warning(disable:4312)
#pragma warning(disable:4996)
#endif
#else
#include <process.h>
#pragma comment(lib, "xnet.lib")
#endif
#endif

//---------------------------------------------------------------------
// 设置内部宏定义
//---------------------------------------------------------------------
#if defined(TCP_CORK) && !defined(TCP_NOPUSH)
#define TCP_NOPUSH TCP_CORK
#endif

#ifdef __unix
typedef socklen_t DSOCKLEN_T;
#else
typedef int DSOCKLEN_T;
#endif



//=====================================================================
// 基本函数接口声明
//=====================================================================

//---------------------------------------------------------------------
// 开始网络
//---------------------------------------------------------------------
int apr_netstart(void)
{
	#if defined(_WIN32) || defined(WIN32)
	static int inited = 0;
	WSADATA WSAData;
	int retval = 0;

	#ifdef _XBOX
    XNetStartupParams xnsp;
	#endif

	if (inited == 0) {
		#ifdef _XBOX
		memset(&xnsp, 0, sizeof(xnsp));
		xnsp.cfgSizeOfStruct = sizeof(XNetStartupParams);
		xnsp.cfgFlags = XNET_STARTUP_BYPASS_SECURITY;
		XNetStartup(&xnsp);
		#endif

		retval = WSAStartup(0x202, &WSAData);
		if (WSAData.wVersion != 0x202) {
			WSACleanup();
			fprintf(stderr, "WSAStartup failed !!\n");
			fflush(stderr);
			return -1;
		}

		inited = 1;
	}
	#endif
	return 0;
}

//---------------------------------------------------------------------
// 结束网络
//---------------------------------------------------------------------
int apr_netclose(void)
{
	int retval = 0;
	#ifdef _WIN32
	retval = (int)WSACleanup();
	#endif
	return retval;
}

//---------------------------------------------------------------------
// 初始化套接字
//---------------------------------------------------------------------
int apr_socket(int family, int type, int protocol)
{
	return (int)socket(family, type, protocol);
}

//---------------------------------------------------------------------
// 关闭套接字
//---------------------------------------------------------------------
int apr_close(int sock)
{
	int retval = 0;
	if (sock < 0) return 0;
	#ifdef __unix
	retval = close(sock);
	#else
	retval = closesocket((SOCKET)sock);
	#endif
	return retval;
}

//---------------------------------------------------------------------
// 连接目标地址
//---------------------------------------------------------------------
int apr_connect(int sock, const struct sockaddr *addr, int addrlen)
{
	DSOCKLEN_T len = sizeof(struct sockaddr);
#ifdef _WIN32
	unsigned char remote[32];
	if (addrlen == 24) {
		memset(remote, 0, 32);
		memcpy(remote, addr, 24);
		addrlen = 28;
		addr = (const struct sockaddr *)remote;
	}
#endif
	if (addrlen > 0) len = (DSOCKLEN_T)addrlen;
	return connect(sock, addr, len);
}

//---------------------------------------------------------------------
// 停止套接字
//---------------------------------------------------------------------
int apr_shutdown(int sock, int mode)
{
	return shutdown(sock, mode);
}

//---------------------------------------------------------------------
// 绑定端口
//---------------------------------------------------------------------
int apr_bind(int sock, const struct sockaddr *addr, int addrlen)
{
	DSOCKLEN_T len = sizeof(struct sockaddr);
	int hr;
#ifdef _WIN32
	unsigned char remote[32];
	if (addrlen == 24) {
		memset(remote, 0, 32);
		memcpy(remote, addr, 24);
		addrlen = 28;
		addr = (const struct sockaddr *)remote;
	}
#endif
	if (addrlen > 0) len = (DSOCKLEN_T)addrlen;
	hr = bind(sock, addr, len);
	return hr;
}

//---------------------------------------------------------------------
// 监听端口
//---------------------------------------------------------------------
int apr_listen(int sock, int count)
{
	return listen(sock, count);
}

//---------------------------------------------------------------------
// 接收连接
//---------------------------------------------------------------------
int apr_accept(int sock, struct sockaddr *addr, int *addrlen)
{
	DSOCKLEN_T len = sizeof(struct sockaddr);
	struct sockaddr *target = addr;
	int hr;
#ifdef _WIN32
	unsigned char remote[32];
#endif
	if (addrlen) {
		len = (addrlen[0] > 0)? (DSOCKLEN_T)addrlen[0] : len;
	}
#ifdef _WIN32
	if (len == 24) {
		target = (struct sockaddr *)remote;
		len = 28;
	}
#endif
	hr = (int)accept(sock, target, &len);
#ifdef _WIN32
	if (target != addr) {
		memcpy(addr, remote, 24);
		len = 24;
	}
#endif
	if (addrlen) addrlen[0] = (int)len;
	return hr;
}

//---------------------------------------------------------------------
// 返回上一个错误
//---------------------------------------------------------------------
int apr_errno(void)
{
	int retval;
	#ifdef __unix
	retval = errno;
	#else
	retval = (int)WSAGetLastError();
	#endif
	return retval;
}

//---------------------------------------------------------------------
// 发送数据
//---------------------------------------------------------------------
int apr_send(int sock, const void *buf, long size, int mode)
{
	return send(sock, (char*)buf, size, mode);
}

//---------------------------------------------------------------------
// 接收数据
//---------------------------------------------------------------------
int apr_recv(int sock, void *buf, long size, int mode)
{
	return recv(sock, (char*)buf, size, mode);
}

//---------------------------------------------------------------------
// 非连接套接字发送
//---------------------------------------------------------------------
int apr_sendto(int sock, const void *buf, long size, int mode, const struct sockaddr *addr, int addrlen)
{
	DSOCKLEN_T len = sizeof(struct sockaddr);
#ifdef _WIN32
	unsigned char remote[32];
	if (addrlen == 24) {
		memset(remote, 0, 32);
		memcpy(remote, addr, 24);
		addrlen = 28;
		addr = (const struct sockaddr *)remote;
	}
#endif
	if (addrlen > 0) len = addrlen;
	return sendto(sock, (char*)buf, size, mode, addr, len);
}

//---------------------------------------------------------------------
// 非连接套接字接收
//---------------------------------------------------------------------
int apr_recvfrom(int sock, void *buf, long size, int mode, struct sockaddr *addr, int *addrlen)
{
	DSOCKLEN_T len = sizeof(struct sockaddr);
	struct sockaddr *target = addr;
	int hr;
#ifdef _WIN32
	unsigned char remote[32];
#endif
	if (addrlen) {
		len = (addrlen[0] > 0)? (DSOCKLEN_T)addrlen[0] : len;
	}
#ifdef _WIN32
	if (len == 24) {
		target = (struct sockaddr *)remote;
		len = 28;
	}
#endif
	hr = (int)recvfrom(sock, (char*)buf, size, mode, target, &len);
#ifdef _WIN32
	if (target != addr) {
		memcpy(addr, remote, 24);
		len = 24;
	}
#endif
	if (addrlen) addrlen[0] = (int)len;
	return hr;
}

//---------------------------------------------------------------------
// 调用ioctlsocket，设置输出输入参数
//---------------------------------------------------------------------
int apr_ioctl(int sock, unsigned long cmd, unsigned long *argp)
{
	int retval;
	#ifdef __unix
	retval = ioctl(sock, cmd, argp);
	#else
	retval = ioctlsocket((SOCKET)sock, (long)cmd, argp);
	#endif
	return retval;	
}

//---------------------------------------------------------------------
// 设置套接字参数
//---------------------------------------------------------------------
int apr_setsockopt(int sock, int level, int optname, const char *optval, int optlen)
{
	DSOCKLEN_T len = optlen;
	return setsockopt(sock, level, optname, optval, len);
}

//---------------------------------------------------------------------
// 读取套接字参数
//---------------------------------------------------------------------
int apr_getsockopt(int sock, int level, int optname, char *optval, int *optlen)
{
	DSOCKLEN_T len = (optlen)? *optlen : 0;
	int retval;
	retval = getsockopt(sock, level, optname, optval, &len);
	if (optlen) *optlen = len;
	return retval;
}

//---------------------------------------------------------------------
// 取得套接字地址
//---------------------------------------------------------------------
int apr_sockname(int sock, struct sockaddr *addr, int *addrlen)
{
	DSOCKLEN_T len = sizeof(struct sockaddr);
	struct sockaddr *target = addr;
	int hr;
#ifdef _WIN32
	unsigned char remote[32];
#endif
	if (addrlen) {
		len = (addrlen[0] > 0)? (DSOCKLEN_T)addrlen[0] : len;
	}
#ifdef _WIN32
	if (len == 24) {
		target = (struct sockaddr *)remote;
		len = 28;
	}
#endif
	hr = (int)getsockname(sock, target, &len);
#ifdef _WIN32
	if (target != addr) {
		memcpy(addr, remote, 24);
		len = 24;
	}
#endif
	if (addrlen) addrlen[0] = (int)len;
	return hr;
}

//---------------------------------------------------------------------
// 取得套接字所连接地址
//---------------------------------------------------------------------
int apr_peername(int sock, struct sockaddr *addr, int *addrlen)
{
	DSOCKLEN_T len = sizeof(struct sockaddr);
	struct sockaddr *target = addr;
	int hr;
#ifdef _WIN32
	unsigned char remote[32];
#endif
	if (addrlen) {
		len = (addrlen[0] > 0)? (DSOCKLEN_T)addrlen[0] : len;
	}
#ifdef _WIN32
	if (len == 24) {
		target = (struct sockaddr *)remote;
		len = 28;
	}
#endif
	hr = (int)getpeername(sock, target, &len);
#ifdef _WIN32
	if (target != addr) {
		memcpy(addr, remote, 24);
		len = 24;
	}
#endif
	if (addrlen) addrlen[0] = (int)len;
	return hr;
}



//=====================================================================
// 功能函数接口声明
//=====================================================================

//---------------------------------------------------------------------
// 允许功能选项：APR_NOBLOCK / APR_NODELAY等
//---------------------------------------------------------------------
int apr_enable(int fd, int mode)
{
	long value = 1;
	long retval = 0;

	switch (mode)
	{
	case APR_NOBLOCK:
		retval = apr_ioctl(fd, (int)FIONBIO, (unsigned long*)(void*)&value);
		break;
	case APR_REUSEADDR:
		retval = apr_setsockopt(fd, (int)SOL_SOCKET, SO_REUSEADDR, (char*)&value, sizeof(value));
		break;
	case APR_NODELAY:
		retval = apr_setsockopt(fd, (int)IPPROTO_TCP, TCP_NODELAY, (char*)&value, sizeof(value));
		break;
	case APR_NOPUSH:
		#ifdef TCP_NOPUSH
		retval = apr_setsockopt(fd, (int)IPPROTO_TCP, TCP_NOPUSH, (char*)&value, sizeof(value));
		#else
		retval = -1000;
		#endif
		break;
	case APR_CLOEXEC:
		#ifdef FD_CLOEXEC
		value = fcntl(fd, F_GETFD);
		retval = fcntl(fd, F_SETFD, FD_CLOEXEC | value);
		#else
		retval = -1000;
		#endif
		break;
	}

	return retval;
}

//---------------------------------------------------------------------
// 禁止功能选项：APR_NOBLOCK / APR_NODELAY等
//---------------------------------------------------------------------
int apr_disable(int fd, int mode)
{
	long value = 0;
	long retval = 0;

	switch (mode)
	{
	case APR_NOBLOCK:
		retval = apr_ioctl(fd, (int)FIONBIO, (unsigned long*)&value);
		break;
	case APR_REUSEADDR:
		retval = apr_setsockopt(fd, (int)SOL_SOCKET, SO_REUSEADDR, (char*)&value, sizeof(value));
		break;
	case APR_NODELAY:
		retval = apr_setsockopt(fd, (int)IPPROTO_TCP, TCP_NODELAY, (char*)&value, sizeof(value));
		break;
	case APR_NOPUSH:
		#ifdef TCP_NOPUSH
		retval = apr_setsockopt(fd, (int)IPPROTO_TCP, TCP_NOPUSH, (char*)&value, sizeof(value));
		#else
		retval = -1000;
		#endif
		break;
	case APR_CLOEXEC:
		#ifdef FD_CLOEXEC
		value = fcntl(fd, F_GETFD);
		value &= ~FD_CLOEXEC;
		retval = fcntl(fd, F_SETFD, value);
		#else
		retval = -1000;
		#endif
		break;
	}
	return retval;
}

//---------------------------------------------------------------------
// 捕捉套接字的事件：APR_ESEND / APR_ERECV / APR_ERROR 
//---------------------------------------------------------------------
int apr_pollfd(int sock, int event, long millsec)
{
	int retval = 0;

	#ifdef __unix
	struct pollfd pfd = { sock, 0, 0 };
	int POLLERC = POLLERR | POLLHUP | POLLNVAL;

	pfd.events |= (event & APR_ERECV)? POLLIN : 0;
	pfd.events |= (event & APR_ESEND)? POLLOUT : 0;
	pfd.events |= (event & APR_ERROR)? POLLERC : 0;

	poll(&pfd, 1, millsec);

	if ((event & APR_ERECV) && (pfd.revents & POLLIN)) retval |= APR_ERECV;
	if ((event & APR_ESEND) && (pfd.revents & POLLOUT)) retval |= APR_ESEND;
	if ((event & APR_ERROR) && (pfd.revents & POLLERC)) retval |= APR_ERROR;

	#else
	struct timeval tmx = { 0, 0 };
	union { void *ptr; fd_set *fds; } p[3];
	int fdr[2], fdw[2], fde[2];

	tmx.tv_sec = millsec / 1000;
	tmx.tv_usec = (millsec % 1000) * 1000;
	fdr[0] = fdw[0] = fde[0] = 1;
	fdr[1] = fdw[1] = fde[1] = sock;

	p[0].ptr = (event & APR_ERECV)? fdr : NULL;
	p[1].ptr = (event & APR_ESEND)? fdw : NULL;
	p[2].ptr = (event & APR_ESEND)? fde : NULL;

	retval = select( sock + 1, p[0].fds, p[1].fds, p[2].fds, 
					(millsec >= 0)? &tmx : 0);
	retval = 0;

	if ((event & APR_ERECV) && fdr[0]) retval |= APR_ERECV;
	if ((event & APR_ESEND) && fdw[0]) retval |= APR_ESEND;
	if ((event & APR_ERROR) && fde[0]) retval |= APR_ERROR;
	#endif

	return retval;
}

//---------------------------------------------------------------------
// 尽可能的发送数据
//---------------------------------------------------------------------
int apr_sendall(int sock, const void *buf, long size)
{
	unsigned char *lptr = (unsigned char*)buf;
	int total = 0, retval = 0, c;

	for (; size > 0; lptr += retval, size -= (long)retval) {
		retval = apr_send(sock, lptr, size, 0);
		if (retval == 0) {
			retval = -1;
			break;
		}
		if (retval == -1) {
			c = apr_errno();
			if (c != IEAGAIN) {
				retval = -1000 - c;
				break;
			}
			retval = 0;
			break;
		}
		total += retval;
	}

	return (retval < 0)? retval : total;
}

//---------------------------------------------------------------------
// 尽可能的接收数据
//---------------------------------------------------------------------
int apr_recvall(int sock, void *buf, long size)
{
	unsigned char *lptr = (unsigned char*)buf;
	int total = 0, retval = 0, c;

	for (; size > 0; lptr += retval, size -= (long)retval) {
		retval = apr_recv(sock, lptr, size, 0);
		if (retval == 0) {
			retval = -1;
			break;
		}
		if (retval == -1) {
			c = apr_errno();
			if (c != IEAGAIN) {
				retval = -1000 - c;
				break;
			}
			retval = 0;
			break;
		}
		total += retval;
	}

	return (retval < 0)? retval : total;
}




//---------------------------------------------------------------------
// WIN32 UDP初始化
//---------------------------------------------------------------------
int apr_win32_init(int sock)
{
	if (sock < 0) return -1;

#if (defined(WIN32) || defined(_WIN32) || defined(_WIN64) || defined(WIN64))
	#define _SIO_UDP_CONNRESET_ _WSAIOW(IOC_VENDOR, 12)
	{
		DWORD dwBytesReturned = 0;
		BOOL bNewBehavior = FALSE;
		DWORD status;
		/* disable  new behavior using
		   IOCTL: SIO_UDP_CONNRESET */

		status = WSAIoctl(sock, _SIO_UDP_CONNRESET_,
					&bNewBehavior, sizeof(bNewBehavior),  
					NULL, 0, &dwBytesReturned, 
					NULL, NULL);

		if (SOCKET_ERROR == (int)status) {
			DWORD err = WSAGetLastError();
			if (err == WSAEWOULDBLOCK) {
				/* nothing to doreturn(FALSE); */
			} else {
				closesocket(sock);
				return -2;
			}
		}
	}
#endif
	return 0;
}


#ifdef _MSC_VER
#pragma warning(disable:4996)
#endif

//---------------------------------------------------------------------
// 将错误码转换成对应字符串
//---------------------------------------------------------------------
char *apr_errstr(int errnum, char *msg, int size)
{
	static char buffer[1025];
	char *lptr = (msg == NULL)? buffer : msg;
	long length = (msg == NULL)? 1024 : size;
#ifdef __unix
	#ifdef __CYGWIN__
	strncpy(lptr, strerror(errnum), length);
	#else
	strerror_r(errnum, lptr, length);
	#endif
#else
	LPVOID lpMessageBuf;
	fd_set fds;
	FD_ZERO(&fds);
	FD_CLR(0, &fds);
	size = (long)FormatMessage( 
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, errnum, MAKELANGID(LANG_ENGLISH, SUBLANG_DEFAULT), (LPTSTR) &lpMessageBuf,
		0, NULL);
	strncpy(lptr, (char*)lpMessageBuf, length);
	LocalFree(lpMessageBuf);
#endif
	return lptr;
}



//---------------------------------------------------------------------
// IPV4/IPV6 地址帮助
//---------------------------------------------------------------------
#ifndef APR_IN6ADDRSZ
#define	APR_IN6ADDRSZ	16
#endif

#ifndef APR_INT16SZ
#define	APR_INT16SZ		2
#endif

#ifndef APR_INADDRSZ
#define	APR_INADDRSZ	4
#endif

/* convert presentation format to network format */
static int apr_pton4(const char *src, unsigned char *dst)
{
	unsigned int val;
	unsigned int digit;
	int base, n;
	unsigned char c;
	unsigned int parts[4];
	register unsigned int *pp = parts;
	int pton = 1;
	c = *src;
	for (;;) {
		if (!isdigit(c)) return -1;
		val = 0; base = 10;
		if (c == '0') {
			c = *++src;
			if (c == 'x' || c == 'X') base = 16, c = *++src;
			else if (isdigit(c) && c != '9') base = 8;
		}
		if (pton && base != 10) return -1;
		for (;;) {
			if (isdigit(c)) {
				digit = c - '0';
				if (digit >= (unsigned int)base) break;
				val = (val * base) + digit;
				c = *++src;
			}	else if (base == 16 && isxdigit(c)) {
				digit = c + 10 - (islower(c) ? 'a' : 'A');
				if (digit >= 16) break;
				val = (val << 4) | digit;
				c = *++src;
			}	else {
				break;
			}
		}
		if (c == '.') {
			if (pp >= parts + 3) return -1;
			*pp++ = val;
			c = *++src;
		}	else {
			break;
		}
	}

	if (c != '\0' && !isspace(c)) return -1;

	n = pp - parts + 1;
	if (pton && n != 4) return -1;

	switch (n) {
	case 0: return -1;	
	case 1:	break;
	case 2:	
		if (parts[0] > 0xff || val > 0xffffff) return -1;
		val |= parts[0] << 24;
		break;
	case 3:	
		if ((parts[0] | parts[1]) > 0xff || val > 0xffff) return -1;
		val |= (parts[0] << 24) | (parts[1] << 16);
		break;
	case 4:	
		if ((parts[0] | parts[1] | parts[2] | val) > 0xff) return -1;
		val |= (parts[0] << 24) | (parts[1] << 16) | (parts[2] << 8);
		break;
	}
	if (dst) {
		val = htonl(val);
		memcpy(dst, &val, APR_INADDRSZ);
	}
	return 0;
}

/* convert presentation format to network format */
static int apr_pton6(const char *src, unsigned char *dst)
{
	static const char xdigits_l[] = "0123456789abcdef";
	static const char xdigits_u[] = "0123456789ABCDEF";
	unsigned char tmp[APR_IN6ADDRSZ], *tp, *endp, *colonp;
	const char *xdigits, *curtok;
	unsigned int val;
	int ch, saw_xdigit;

	memset((tp = tmp), '\0', APR_IN6ADDRSZ);
	endp = tp + APR_IN6ADDRSZ;
	colonp = NULL;
	if (*src == ':')
		if (*++src != ':')
			return -1;
	curtok = src;
	saw_xdigit = 0;
	val = 0;
	while ((ch = *src++) != '\0') {
		const char *pch;
		if ((pch = strchr((xdigits = xdigits_l), ch)) == NULL)
			pch = strchr((xdigits = xdigits_u), ch);
		if (pch != NULL) {
			val <<= 4;
			val |= (pch - xdigits);
			if (val > 0xffff) return -1;
			saw_xdigit = 1;
			continue;
		}
		if (ch == ':') {
			curtok = src;
			if (!saw_xdigit) {
				if (colonp) return -1;
				colonp = tp;
				continue;
			} 
			else if (*src == '\0') {
				return -1;
			}
			if (tp + APR_INT16SZ > endp) return -1;
			*tp++ = (unsigned char) (val >> 8) & 0xff;
			*tp++ = (unsigned char) val & 0xff;
			saw_xdigit = 0;
			val = 0;
			continue;
		}
		if (ch == '.' && ((tp + APR_INADDRSZ) <= endp) &&
		    apr_pton4(curtok, tp) > 0) {
			tp += APR_INADDRSZ;
			saw_xdigit = 0;
			break;	
		}
		return -1;
	}
	if (saw_xdigit) {
		if (tp + APR_INT16SZ > endp) return -1;
		*tp++ = (unsigned char) (val >> 8) & 0xff;
		*tp++ = (unsigned char) val & 0xff;
	}
	if (colonp != NULL) {
		const int n = tp - colonp;
		int i;
		if (tp == endp) return -1;
		for (i = 1; i <= n; i++) {
			endp[- i] = colonp[n - i];
			colonp[n - i] = 0;
		}
		tp = endp;
	}
	if (tp != endp) return -1;
	memcpy(dst, tmp, APR_IN6ADDRSZ);
	return 0;
}

/* convert presentation format to network format */
static const char *
apr_ntop4(const unsigned char *src, char *dst, size_t size)
{
	char tmp[64];
	size_t len;
	len = sprintf(tmp, "%u.%u.%u.%u", src[0], src[1], src[2], src[3]);
	if (len >= size) {
		errno = ENOSPC;
		return NULL;
	}
	memcpy(dst, tmp, len + 1);
	return dst;
}

/* convert presentation format to network format */
static const char *
apr_ntop6(const unsigned char *src, char *dst, size_t size)
{
	char tmp[64], *tp;
	struct { int base, len; } best, cur;
	unsigned int words[APR_IN6ADDRSZ / APR_INT16SZ];
	int i, inc;

	memset(words, '\0', sizeof(words));
	best.base = best.len = 0;
	cur.base = cur.len = 0;

	for (i = 0; i < APR_IN6ADDRSZ; i++)
		words[i / 2] |= (src[i] << ((1 - (i % 2)) << 3));

	best.base = -1;
	cur.base = -1;

	for (i = 0; i < (APR_IN6ADDRSZ / APR_INT16SZ); i++) {
		if (words[i] == 0) {
			if (cur.base == -1) cur.base = i, cur.len = 1;
			else cur.len++;
		} 
		else {
			if (cur.base != -1) {
				if (best.base == -1 || cur.len > best.len) best = cur;
				cur.base = -1;
			}
		}
	}
	if (cur.base != -1) {
		if (best.base == -1 || cur.len > best.len)
			best = cur;
	}
	if (best.base != -1 && best.len < 2)
		best.base = -1;

	tp = tmp;
	for (i = 0; i < (APR_IN6ADDRSZ / APR_INT16SZ); i++) {
		if (best.base != -1 && i >= best.base &&
			i < (best.base + best.len)) {
			if (i == best.base)
				*tp++ = ':';
			continue;
		}

		if (i != 0) *tp++ = ':';
		if (i == 6 && best.base == 0 &&
			(best.len == 6 || (best.len == 5 && words[5] == 0xffff))) {
			if (!apr_ntop4(src+12, tp, sizeof(tmp) - (tp - tmp)))
				return NULL;
			tp += strlen(tp);
			break;
		}
		inc = sprintf(tp, "%x", words[i]);
		tp += inc;
	}

	if (best.base != -1 && (best.base + best.len) == 
		(APR_IN6ADDRSZ / APR_INT16SZ)) 
		*tp++ = ':';

	*tp++ = '\0';

	if ((size_t)(tp - tmp) > size) {
		errno = ENOSPC;
		return NULL;
	}
	memcpy(dst, tmp, tp - tmp);
	return dst;
}


//---------------------------------------------------------------------
// 转换表示格式到网络格式 
// 返回 0表示成功，支持 AF_INET/AF_INET6 
//---------------------------------------------------------------------
int apr_pton(int af, const char *src, void *dst)
{
	switch (af) {
	case AF_INET:
		return apr_pton4(src, (unsigned char*)dst);
#if AF_INET6
	case AF_INET6:
		return apr_pton6(src, (unsigned char*)dst);
#endif
	default:
		if (af == -6) {
			return apr_pton6(src, (unsigned char*)dst);
		}
		errno = EAFNOSUPPORT;
		return -1;
	}
}


//---------------------------------------------------------------------
// 转换网络格式到表示格式 
// 支持 AF_INET/AF_INET6 
//---------------------------------------------------------------------
const char *apr_ntop(int af, const void *src, char *dst, size_t size)
{
	switch (af) {
	case AF_INET:
		return apr_ntop4((const unsigned char*)src, dst, size);
	#ifdef AF_INET6
	case AF_INET6:
		return apr_ntop6((const unsigned char*)src, dst, size);
	#endif
	default:
		if (af == -6) {
			return apr_ntop6((const unsigned char*)src, dst, size);
		}
		errno = EAFNOSUPPORT;
		return NULL;
	}
}



