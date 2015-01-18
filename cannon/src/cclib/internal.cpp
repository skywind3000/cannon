//=====================================================================
//
// ccmod.cpp - cc module interfaces
//
// NOTE:
// for more information, please see the readme file.
//
//=====================================================================
#include "internal.h"
#include <assert.h>


//=====================================================================
// 32BIT INTEGER DEFINITION 
//=====================================================================
#ifndef __INTEGER_32_BITS__
#define __INTEGER_32_BITS__
#if defined(_WIN64) || defined(WIN64) || defined(__amd64__) || \
	defined(__x86_64) || defined(__x86_64__) || defined(_M_IA64) || \
	defined(_M_AMD64)
	typedef unsigned int ISTDUINT32;
	typedef int ISTDINT32;
#elif defined(_WIN32) || defined(WIN32) || defined(__i386__) || \
	defined(__i386) || defined(_M_X86)
	typedef unsigned long ISTDUINT32;
	typedef long ISTDINT32;
#elif defined(__MACOS__)
	typedef UInt32 ISTDUINT32;
	typedef SInt32 ISTDINT32;
#elif defined(__APPLE__) && defined(__MACH__)
	#include <sys/types.h>
	typedef u_int32_t ISTDUINT32;
	typedef int32_t ISTDINT32;
#elif defined(__BEOS__)
	#include <sys/inttypes.h>
	typedef u_int32_t ISTDUINT32;
	typedef int32_t ISTDINT32;
#elif (defined(_MSC_VER) || defined(__BORLANDC__)) && (!defined(__MSDOS__))
	typedef unsigned __int32 ISTDUINT32;
	typedef __int32 ISTDINT32;
#elif defined(__GNUC__)
	#include <stdint.h>
	typedef uint32_t ISTDUINT32;
	typedef int32_t ISTDINT32;
#else 
	typedef unsigned long ISTDUINT32; 
	typedef long ISTDINT32;
#endif
#endif


//=====================================================================
// DETECTION WORD ORDER
//=====================================================================
#ifndef IWORDS_BIG_ENDIAN
    #ifdef _BIG_ENDIAN_
        #if _BIG_ENDIAN_
            #define IWORDS_BIG_ENDIAN 1
        #endif
    #endif
    #ifndef IWORDS_BIG_ENDIAN
        #if defined(__hppa__) || \
            defined(__m68k__) || defined(mc68000) || defined(_M_M68K) || \
            (defined(__MIPS__) && defined(__MISPEB__)) || \
            defined(__ppc__) || defined(__POWERPC__) || defined(_M_PPC) || \
            defined(__sparc__) || defined(__powerpc__) || \
            defined(__mc68000__) || defined(__s390x__) || defined(__s390__)
            #define IWORDS_BIG_ENDIAN 1
        #endif
    #endif
    #ifndef IWORDS_BIG_ENDIAN
        #define IWORDS_BIG_ENDIAN  0
    #endif
#endif


//=====================================================================
// inline definition
//=====================================================================
#ifndef INLINE
#ifdef __GNUC__
#if (__GNUC__ > 3) || ((__GNUC__ == 3) && (__GNUC_MINOR__ >= 1))
    #define INLINE         __inline__ __attribute__((always_inline))
#else
    #define INLINE         __inline__
#endif
#elif defined(_MSC_VER)
	#define INLINE __forceinline
#elif (defined(__BORLANDC__) || defined(__WATCOMC__))
    #define INLINE __inline
#else
    #define INLINE 
#endif
#endif

#ifndef inline
    #define inline INLINE
#endif


NAMESPACE_BEGIN(cclib)
//---------------------------------------------------------------------
// global definition
//---------------------------------------------------------------------

/* Typedefs that APR needs. */
typedef  unsigned char     apr_byte;
typedef  short             apr_int16;
typedef  unsigned short    apr_uint16;
typedef  ISTDINT32         apr_int32;
typedef  ISTDUINT32        apr_uint32;


static inline char *iencode16u_lsb(char *p, unsigned short w)
{
#if IWORDS_BIG_ENDIAN
	*(unsigned char*)(p + 0) = (w & 255);
	*(unsigned char*)(p + 1) = (w >> 8);
#else
	*(unsigned short*)(p) = w;
#endif
	p += 2;
	return p;
}

static inline char *idecode16u_lsb(const char *p, unsigned short *w)
{
#if IWORDS_BIG_ENDIAN
	*w = *(const unsigned char*)(p + 1);
	*w = *(const unsigned char*)(p + 0) + (*w << 8);
#else
	*w = *(const unsigned short*)p;
#endif
	p += 2;
	return (char*)p;
}


static inline char *iencode16u_msb(char *p, unsigned short w)
{
#if IWORDS_BIG_ENDIAN
	*(unsigned short*)(p) = w;
#else
	*(unsigned char*)(p + 0) = (w >> 8);
	*(unsigned char*)(p + 1) = (w & 255);
#endif
	p += 2;
	return p;
}

static inline char *idecode16u_msb(const char *p, unsigned short *w)
{
#if IWORDS_BIG_ENDIAN
	*w = *(const unsigned short*)p;
#else
	*w = *(const unsigned char*)(p + 0);
	*w = *(const unsigned char*)(p + 1) + (*w << 8);
#endif
	p += 2;
	return (char*)p;
}

static inline char *iencode32u_lsb(char *p, apr_uint32 l)
{
#if IWORDS_BIG_ENDIAN
	*(unsigned char*)(p + 0) = (unsigned char)((l >>  0) & 0xff);
	*(unsigned char*)(p + 1) = (unsigned char)((l >>  8) & 0xff);
	*(unsigned char*)(p + 2) = (unsigned char)((l >> 16) & 0xff);
	*(unsigned char*)(p + 3) = (unsigned char)((l >> 24) & 0xff);
#else
	*(apr_uint32*)p = l;
#endif
	p += 4;
	return p;
}

static inline char *idecode32u_lsb(const char *p, apr_uint32 *l)
{
#if IWORDS_BIG_ENDIAN
	*l = *(const unsigned char*)(p + 3);
	*l = *(const unsigned char*)(p + 2) + (*l << 8);
	*l = *(const unsigned char*)(p + 1) + (*l << 8);
	*l = *(const unsigned char*)(p + 0) + (*l << 8);
#else 
	*l = *(const apr_uint32*)p;
#endif
	p += 4;
	return (char*)p;
}

static inline char *iencode32u_msb(char *p, apr_uint32 l)
{
#if IWORDS_BIG_ENDIAN
	*(apr_uint32*)p = l;
#else
	*(unsigned char*)(p + 0) = (unsigned char)((l >> 24) & 0xff);
	*(unsigned char*)(p + 1) = (unsigned char)((l >> 16) & 0xff);
	*(unsigned char*)(p + 2) = (unsigned char)((l >>  8) & 0xff);
	*(unsigned char*)(p + 3) = (unsigned char)((l >>  0) & 0xff);
#endif
	p += 4;
	return p;
}

static inline char *idecode32u_msb(const char *p, apr_uint32 *l)
{
#if IWORDS_BIG_ENDIAN
	*l = *(const apr_uint32*)p;
#else 
	*l = *(const unsigned char*)(p + 0);
	*l = *(const unsigned char*)(p + 1) + (*l << 8);
	*l = *(const unsigned char*)(p + 2) + (*l << 8);
	*l = *(const unsigned char*)(p + 3) + (*l << 8);
#endif
	p += 4;
	return (char*)p;
}

/* encode 8 bits unsigned int */
static inline char *iencode8u(char *p, unsigned char c)
{
	*(unsigned char*)p++ = c;
	return p;
}

/* decode 8 bits unsigned int */
static inline char *idecode8u(const char *p, unsigned char *c)
{
	*c = *(unsigned char*)p++;
	return (char*)p;
}


//=====================================================================
// ´óÐ¡ÉèÖÃ
//=====================================================================
long ccsocket::_size_get(const void *p)
{
	unsigned char cbyte;
	apr_uint16 cshort;
	apr_uint32 cint;
	long length = 0;

	switch (_headint)
	{
	case ITMH_WORDLSB:
		idecode16u_lsb((const char*)p, &cshort);
		length = cshort;
		break;
	case ITMH_WORDMSB:
		idecode16u_msb((const char*)p, &cshort);
		length = cshort;
		break;
	case ITMH_DWORDLSB:
		idecode32u_lsb((const char*)p, &cint);
		length = cint;
		break;
	case ITMH_DWORDMSB:
		idecode32u_msb((const char*)p, &cint);
		length = cint;
		break;
	case ITMH_BYTELSB:
	case ITMH_BYTEMSB:
		idecode8u((const char*)p, &cbyte);
		length = cbyte;
		break;
	}
	return length + _headinc;
}

void ccsocket::_size_set(void *p, long size)
{
	size -= _headinc;
	switch (_headint)
	{
	case ITMH_WORDLSB:
		iencode16u_lsb((char*)p, (apr_uint16)size);
		break;
	case ITMH_WORDMSB:
		iencode16u_msb((char*)p, (apr_uint16)size);
		break;
	case ITMH_DWORDLSB:
		iencode32u_lsb((char*)p, (apr_uint32)size);
		break;
	case ITMH_DWORDMSB:
		iencode32u_msb((char*)p, (apr_uint32)size);
		break;
	case ITMH_BYTELSB:
	case ITMH_BYTEMSB:
		iencode8u((char*)p, (unsigned char)size);
		break;
	}
}


void ccsocket::_write_dword(void *ptr, unsigned long dword)
{
	if (_headmod & 1) {
		iencode32u_msb((char*)ptr, (apr_uint32)dword);
	}	else {
		iencode32u_lsb((char*)ptr, (apr_uint32)dword);
	}
}

void ccsocket::_write_word(void *ptr, unsigned short word)
{
	if (_headmod & 1) {
		iencode16u_msb((char*)ptr, word);
	}	else {
		iencode16u_lsb((char*)ptr, word);
	}
}

unsigned short ccsocket::_read_word(const void *ptr)
{
	unsigned short word;
	if (_headmod & 1) {
		idecode16u_msb((char*)ptr, &word);
	}	else {
		idecode16u_lsb((char*)ptr, &word);
	}
	return word;
}

unsigned long ccsocket::_read_dword(const void *ptr)
{
	apr_uint32 dword;
	if (_headmod & 1) {
		idecode32u_msb((char*)ptr, &dword);
	}	else {
		idecode32u_lsb((char*)ptr, &dword);
	}
	return dword;
}



//=====================================================================
// ccsocket
//=====================================================================
void ccsocket::initialize()
{
	_recvcache.data = NULL;
	_recvcache.size = 0;
	_recvcache.head = 0;
	_recvcache.tail = 0;
	_sendcache.data = NULL;
	_sendcache.size = 0;
	_sendcache.head = 0;
	_sendcache.tail = 0;
	_recvbuff = NULL;
	_sendbuff = NULL;
	_tempbuff = NULL;
	_buffsize = 0x10000;
	_headmod = 0;
	_headint = 0;
	_headinc = 0;
	_headlen = 0;
	_hdrsize = 2;
}

void ccsocket::destroy()
{
	_recvcache.data = NULL;
	_recvcache.size = 0;
	_recvcache.head = 0;
	_recvcache.tail = 0;
	_sendcache.data = NULL;
	_sendcache.size = 0;
	_sendcache.head = 0;
	_sendcache.tail = 0;
	if (_recvbuff) delete []_recvbuff;
	_recvbuff = NULL;
	_sendbuff = NULL;
	_buffsize = 0x10000;
	_headmod = 0;
	_headint = 0;
	_headinc = 0;
	_headlen = 0;
	_hdrsize = 2;
	sock.close();
}

int ccsocket::setup(int headmode, long buffsize)
{
	destroy();
	
	long newsize = buffsize + 32;
	_recvbuff = new char[(newsize) * 3];
	if (_recvbuff == NULL) return -1;

	_sendbuff = _recvbuff + newsize;
	_tempbuff = _sendbuff + newsize;

	_buffsize = buffsize;

	icache_init(&_recvcache, _recvbuff, buffsize);
	icache_init(&_sendcache, _sendbuff, buffsize);

	static int hdrsize[12] = { 2, 2, 4, 4, 1, 1, 2, 2, 4, 4, 1, 1 };
	static int headinc[12] = { 0, 0, 0, 0, 0, 0, 2, 2, 4, 4, 1, 1 };
	static int headlen[12] = { 12, 12, 14, 14, 11, 11, 12, 12, 14, 14, 11, 11 };

	if (headmode == 12) headmode = 2;
	if (headmode == 13) headmode = 2;
	if (headmode < 0 || headmode >= 12) headmode = 0;

	_headmod = headmode;
	_headint = (headmode < 6)? (headmode) : (headmode - 6);
	_headinc = headinc[headmode];
	_hdrsize = hdrsize[headmode];
	_headlen = headlen[headmode];

	return 0;
}

int ccsocket::ioflush()
{
	void * lptr = NULL ;
	//	flush the send buffer
	for ( ; ICACHE_DSIZE(&_sendcache) > 0; ) { 
		int len = icache_flat (&_sendcache, &lptr) ;
		int ret = sock.writen (lptr, len) ;

		if (ret != len) {
			fprintf(stderr, "[%s][ERROR] [CCLIB] ioflush:sock.writen error\n", 
				gettime());
		}
		assert (ret == len) ;

		if (0 > ret) return -1 ;
		icache_drop (&_sendcache, ret) ;
	}
	return 0;
}


/** flag : PEEK and NOWAIT 
 * recv length bytes (including length == 0 ) data
 * when data == NULL, we just peek whether cache has length bytes data or nor
 * return : 
 * -1 : error
 *  0 or n : the siz has been read . 0 means no complete message available
 */
#define CCSOCKET_IO_PEEK		1
#define CCSOCKET_IO_NOWAIT		2

long ccsocket::iorecv (void * data, long length, int flag)
{
	char *tmpbuf = _tempbuff;
	long tm, ret, val, peek ;

	peek = (flag & CCSOCKET_IO_PEEK) ? 1 : 0 ;
	tm = (flag & CCSOCKET_IO_NOWAIT) ? 0 : -1 ;

	for (int c = 0 ; length > 0 ; c++) {
		// check whether we have enought data in cache 
		if ( length <= ICACHE_DSIZE(&_recvcache)) {
			if (NULL == data) return length ;// just check if we has length bytes data
			if (peek) val = icache_peek (&_recvcache, data, length) ;
			else val = icache_read (&_recvcache, data, length) ;
			return val ;
		}
		// fill the cache.if nowait, just fill the cache once
		if (tm == 0 && c == 1 ) break;
		ret = sock.tryread (tmpbuf, ICACHE_FSIZE(&_recvcache), tm) ;
		if (ret < 0) return ret ;
		if (ret > 0) icache_write (&_recvcache, tmpbuf, ret) ;	
	}
	
	return 0 ;
}


/** send exact length bytes data
 * can handle the condition : data == NULL or length == 0
 */
long ccsocket::iosend (const void * data, long length, int isflush)
{
	unsigned char *lptr = (unsigned char*)data ;
	long len, left ;

	// put all data into cache 
	for (left = length ; (NULL != data) && (left > 0) ; ) {
		len = icache_write(&_sendcache, lptr, left);
		left -= len ;
		lptr += len ;

		// if all data is in cache then return because needing not to flush for demanding more available capacity  
		// else flush unitl we get enougth capacity
		if ( left == 0 ) break ; 
		if (ioflush() < 0)  // squezze the capacity
			return -1 ;
	} 

	if (isflush) {
		if (ioflush() < 0) 
			return -1 ;
	}

	return length - left ;
}

long ccsocket::peek (long length) 
{
	// wait and peek
	return iorecv (NULL, length, CCSOCKET_IO_PEEK | CCSOCKET_IO_NOWAIT) ;
}


long ccsocket::cg_recv(int *event, long *wparam, long *lparam, void *data, int nowait)
{
	char head[24];
	long len , ret ;
	
	*event = -1 ;

	if (nowait == 0) {
		ret = iorecv(head, _headlen, 0) ;
		if (0 > ret) return ret ;
		len = _size_get(head);
		ret = iorecv(data, len - _headlen, 0) ;
		if (0 > ret) return ret ;
	} else {
		ret = iorecv (head, _headlen, CCSOCKET_IO_PEEK | CCSOCKET_IO_NOWAIT) ;
		if (0 > ret) return ret ; 
		if (_headlen > ret) return 0 ; 
		len = _size_get(head);
	
		ret = iorecv (NULL, len, CCSOCKET_IO_NOWAIT) ;
		if (0 > ret) return ret ;
		if (0 == ret) return 0 ;
		
		iorecv (head, _headlen, CCSOCKET_IO_NOWAIT) ;
		iorecv (data, len - _headlen, CCSOCKET_IO_NOWAIT) ;
	}
	
	*event = _read_word(head + _hdrsize);
	*wparam = _read_dword(head + _hdrsize + 2);
	*lparam = _read_dword(head + _hdrsize + 6);
	
	return len - _headlen;
}


long ccsocket::cg_send(int event, long wparam, long lparam, const void *data, long length, int isflush)
{
	char head[24] ;
	long ret ;

	if (length > _buffsize) {
		fprintf(stderr, "[ERROR] [CCLIB] invalid length=%ld\n", length);
		assert(length <= _buffsize);
		abort();
	}

	if (_headint <= 1) {
		if (length > 0x10000) {
			fprintf(stderr, "[ERROR] [CCLIB] invalid length=%ld\n", length);
			assert(length <= 0x10000);
			abort();
		}
	}
	else if (_headint <= 3) {
		if (length > _buffsize) {
			fprintf(stderr, "[ERROR] [CCLIB] invalid length=%ld\n", length);
			assert(length <= _buffsize);
			abort();
		}
	}
	else if (_headint <= 5) {
		if (length > 256) {
			fprintf(stderr, "[ERROR] [CCLIB] invalid length=%ld\n", length);
			assert(length <= 256);
			abort();
		}
	}

	_size_set(head, _headlen + length);
	_write_word(head + _hdrsize, event);
	_write_dword(head + _hdrsize + 2, wparam);
	_write_dword(head + _hdrsize + 6, lparam);

	ret = iosend (head, _headlen, 0) ;

	if (ret != _headlen) {
		fprintf(stderr, "[ERROR] [CCLIB] iosend ret=%ld length=%ld\n", ret, length);
		assert(ret == _headlen);
		abort();
	}

	// if (0 > ret || 0 > length) return ret ;

	ret = iosend (data, length, isflush) ;
	if (ret != length) {
		fprintf(stderr, "[ERROR] [CCLIB] error ret=%ld length=%ld isflush=%d\n", ret, length, isflush);
		assert(ret == length);
	}

	// if (0 > ret) return ret ;

	return ret ;
}

long ccsocket::cg_send(int event, long wparam, long lparam, const void **ptrs, const long *sizes, int n, int isflush)
{
	long length, i;
	char head[24];
	long ret ;

	for (i = 0, length = 0; i < n; i++) {
		length += sizes[i];
	}

	if (length > _buffsize) {
		fprintf(stderr, "[ERROR] [CCLIB] invalid length=%ld\n", length);
		assert(length <= _buffsize);
		abort();
	}

	if (_headint <= 1) {
		if (length > 0x10000) {
			fprintf(stderr, "[ERROR] [CCLIB] invalid length=%ld\n", length);
			assert(length <= 0x10000);
			abort();
		}
	}
	else if (_headint <= 3) {
		if (length > _buffsize) {
			fprintf(stderr, "[ERROR] [CCLIB] invalid length=%ld\n", length);
			assert(length <= _buffsize);
			abort();
		}
	}
	else if (_headint <= 5) {
		if (length > 256) {
			fprintf(stderr, "[ERROR] [CCLIB] invalid length=%ld\n", length);
			assert(length <= 256);
			abort();
		}
	}

	_size_set(head, _headlen + length);
	_write_word(head + _hdrsize, event);
	_write_dword(head + _hdrsize + 2, wparam);
	_write_dword(head + _hdrsize + 6, lparam);

	ret = iosend (head, _headlen, 0) ;

	if (ret != _headlen) {
		fprintf(stderr, "[ERROR] [CCLIB] iosend ret=%ld length=%ld\n", ret, length);
		assert(ret == _headlen);
		abort();
	}

	// if (0 > ret || 0 > length) return ret ;
	long total = 0;

	for (i = 0; i < n; i++) {
		int flush = (i < n - 1)? 0 : isflush;
		ret = iosend(ptrs[i], sizes[i], flush);
		if (ret != sizes[i]) {
			fprintf(stderr, "[ERROR] [CCLIB] error ret=%ld length=%ld isflush=%d\n", 
				ret, length, isflush);
			assert(ret == sizes[i]);
			abort();
		}
		total += ret;
	}

	// if (0 > ret) return ret ;

	return total;
}


/* low level attach */
int ccsocket::attach(const char *ip, int port, int cid, int headmode, long bufsize)
{
	char login[20];

	if (setup(headmode, bufsize) != 0) return -1;

	_size_set(login, _hdrsize + 2);
	_write_word(login + _hdrsize, cid);

	if (0 > sock.open(ip, port)) return -2;
	if (0 > iosend(login, _hdrsize + 2, 1)) 
		return -1 ;

	return 0 ;
}

void ccsocket::detach()
{
	destroy();
}

int ccsocket::handle()
{
	return sock.fd();
}

ccsocket::ccsocket()
{
	initialize();
}

ccsocket::~ccsocket()
{
	destroy();
}

void* ccsocket::temp()
{
	return _tempbuff;
}

NAMESPACE_END(cclib)


