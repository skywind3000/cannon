/** i think this file should contain the interfaces and funtions that are the fundation of popogame platform but independent of the businiess logicA. 
 *
 */

#include "sockstrm.h"
#include "instruction.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

using namespace tiny_socket ;

static sock_stream sock ;

#ifdef __cplusplus
extern "C" {
#endif

#define LEN ((MAX_PACKET_SIZE + 32) * 2)
//#define LEN (1 << 12) 

#if (defined(_WIN32) && defined(_MSC_VER))
#pragma comment(lib, "wsock32.lib")
#endif

///////////////////////////////
#include "cclib.h"

#define NOWAIT 0x00000001  
#define PEEK 0x00000002

static ICACHED _recvcache = {NULL, 0, 0, 0} ;
static ICACHED _sendcache = {NULL, 0, 0, 0} ;

static int itm_headmod = 0;
static int itm_headint = 0;
static int itm_headinc = 0;
static int itm_headlen = 12;
static int itm_hdrsize = 2;

static void cg_ioinit (void)
{
	static char recvbuf[LEN] ;
	static char sendbuf[LEN] ;

	icache_init (&_recvcache, recvbuf, LEN) ;
	icache_init (&_sendcache, sendbuf, LEN) ;
}


//---------------------------------------------------------------------
// ¶ÁÐ´ÄÚÈÝ
//---------------------------------------------------------------------
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
            defined(__sparc__) 
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


//=====================================================================
// Integer Type Decleration
//=====================================================================

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

#define ITMH_WORDLSB	0		// ????:2??LSB
#define ITMH_WORDMSB	1		// ????:2??MSB
#define ITMH_DWORDLSB	2		// ????:4??LSB
#define ITMH_DWORDMSB	3		// ????:4??MSB
#define ITMH_BYTELSB	4		// ????:???LSB
#define ITMH_BYTEMSB	5		// ????:???MSB
#define ITMH_EWORDLSB	6		// ????:2??LSB
#define ITMH_EWORDMSB	7		// ????:2??MSB
#define ITMH_EDWORDLSB	8		// ????:4??LSB
#define ITMH_EDWORDMSB	9		// ????:4??MSB
#define ITMH_EBYTELSB	10		// ????:???LSB
#define ITMH_EBYTEMSB	11		// ????:???MSB


static inline long itm_size_get(const void *p)
{
	unsigned char cbyte;
	apr_uint16 cshort;
	apr_uint32 cint;
	long length = 0;

	switch (itm_headint)
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
	return length + itm_headinc;
}

static inline void itm_size_set(void *p, long size)
{
	size -= itm_headinc;
	switch (itm_headint)
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



static inline void itm_write_dword(void *ptr, apr_uint32 dword)
{
	if (itm_headmod & 1) {
		iencode32u_msb((char*)ptr, dword);
	}	else {
		iencode32u_lsb((char*)ptr, dword);
	}
}

static inline void itm_write_word(void *ptr, apr_uint16 word)
{
	if (itm_headmod & 1) {
		iencode16u_msb((char*)ptr, word);
	}	else {
		iencode16u_lsb((char*)ptr, word);
	}
}

static inline unsigned short itm_read_word(const void *ptr)
{
	unsigned short word;
	if (itm_headmod & 1) {
		idecode16u_msb((char*)ptr, &word);
	}	else {
		idecode16u_lsb((char*)ptr, &word);
	}
	return word;
}

static inline apr_uint32 itm_read_dword(const void *ptr)
{
	apr_uint32 dword;
	if (itm_headmod & 1) {
		idecode32u_msb((char*)ptr, &dword);
	}	else {
		idecode32u_lsb((char*)ptr, &dword);
	}
	return dword;
}


XEXPORT int cg_ioflush (void)
{
	void * lptr = NULL ;
	//	flush the send buffer
	for ( ; ICACHE_DSIZE(&_sendcache) > 0; ) { 
		int len = icache_flat (&_sendcache, &lptr) ;
		int ret = sock.writen (lptr, len) ;

		if (ret != len) {
			fprintf(stderr, "[%s][ERROR] [CCLIB] ioflush:sock.writen error\n", gettime());
		}
		assert (ret == len) ;

		if (0 > ret) return -1 ;
		icache_drop (&_sendcache, ret) ;
	}

	return 0 ;
}

/** flag : PEEK and NOWAIT 
 * recv length bytes (including length == 0 ) data
 * when data == NULL, we just peek whether cache has length bytes data or nor
 * return : 
 * -1 : error
 *  0 or n : the siz has been read . 0 means no complete message available
 */

static int cg_iorecv (void * data, int length, int flag)
{
	char tmpbuf[0x10000] ;
	int tm, ret, val, peek ;

	peek = (flag & PEEK) ? 1 : 0 ;

	tm = (flag & NOWAIT) ? 0 : -1 ;

	for (int c = 0 ; length > 0 ; c++) {
		// check whether we have enought data in cache 
		if ( length <= ICACHE_DSIZE(&_recvcache)) {
			if (NULL == data) return length ;// just check if we has length bytes data
			if (peek) val = icache_peek (&_recvcache, data, length) ;
			else val = icache_read (&_recvcache, data, length) ;
			return val ;
		}
		// fill the cache.if nowait, just fill the cache once
		if (tm == 0 && c == 1 ) break ;
		int canread = ICACHE_FSIZE(&_recvcache);
		if (canread > 0x10000) canread = 0x10000;
		ret = sock.tryread (tmpbuf, canread, tm) ;
		if (ret < 0) return ret ;
		if (ret > 0) icache_write (&_recvcache, tmpbuf, ret) ;	
	}
	
	return 0 ;
}

/** send exact length bytes data
 * can handle the condition : data == NULL or length == 0
 */
static int cg_iosend (const void * data, int length, int isflush)
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
		if (cg_ioflush() < 0)  // squezze the capacity
			return -1 ;
	} 

	if (isflush) {
		if (cg_ioflush() < 0) 
			return -1 ;
	}

	return length - left ;
}

int cg_peek (int length) 
{
	// wait and peek
	return cg_iorecv (NULL, length, PEEK|~NOWAIT) ;
}


int cg_recv (int * event, int * wparam, int * lparam, void * data, int nowait)
{
	char head[24];
	int len , ret ;
	
	*event = -1 ;

	if (nowait == 0) {
		ret = cg_iorecv(head, itm_headlen, 0) ;
		if (0 > ret) return ret ;
		len = itm_size_get(head);
		ret = cg_iorecv(data, len - itm_headlen, 0) ;
		if (0 > ret) return ret ;
	} else {
		ret = cg_iorecv (head, itm_headlen, NOWAIT | PEEK) ;
		if (0 > ret) return ret ; 
		if (itm_headlen > ret) return 0 ; 
		len = itm_size_get(head);
	
		ret = cg_iorecv (NULL, len, NOWAIT) ;
		if (0 > ret) return ret ;
		if (0 == ret) return 0 ;
		
		cg_iorecv (head, itm_headlen, NOWAIT) ;
		cg_iorecv (data, len - itm_headlen, NOWAIT) ;
	}
	
	*event = itm_read_word(head + itm_hdrsize);
	*wparam = itm_read_dword(head + itm_hdrsize + 2);
	*lparam = itm_read_dword(head + itm_hdrsize + 6);
	
	return len - itm_headlen;
}

int cg_send(int event, int wparam, int lparam, const void * data, int length, int isflush)
{
	char head[24] ;
	int ret ;

	if (length > MAX_PACKET_SIZE) {
		fprintf(stderr, "[ERROR] [CCLIB] invalid length=%d\n", length);
		assert(length <= MAX_PACKET_SIZE);
		abort();
	}

	if (itm_headint <= 1) {
		if (length > 0x10000) 
		if (length > 0x10000) {
			fprintf(stderr, "[ERROR] [CCLIB] invalid length=%d\n", length);
			assert(length <= 0x10000);
			abort();
		}
	}
	else if (itm_headint <= 3) {
		if (length > MAX_PACKET_SIZE) {
			fprintf(stderr, "[ERROR] [CCLIB] invalid length=%d\n", length);
			assert(length <= MAX_PACKET_SIZE);
			abort();
		}
	}
	else if (itm_headint <= 5) {
		if (length > 256) {
			fprintf(stderr, "[ERROR] [CCLIB] invalid length=%d\n", length);
			assert(length <= 256);
			abort();
		}
	}

	itm_size_set(head, itm_headlen + length);
	itm_write_word(head + itm_hdrsize, event);
	itm_write_dword(head + itm_hdrsize + 2, wparam);
	itm_write_dword(head + itm_hdrsize + 6, lparam);

	ret = cg_iosend (head, itm_headlen, 0) ;

	if (ret != itm_headlen) {
		fprintf(stderr, "[ERROR] [CCLIB] iosend ret=%d length=%d\n", ret, length);
		assert(ret == itm_headlen);
		abort();
	}

	// if (0 > ret || 0 > length) return ret ;

	ret = cg_iosend (data, length, isflush) ;
	if (ret != length) {
		fprintf(stderr, "[ERROR] [CCLIB] error ret=%d length=%d isflush=%d\n", ret, length, isflush);
		assert(ret == length);
		abort();
	}

	// if (0 > ret) return ret ;

	return ret ;
}

int cg_msend(int event, int wparam, int lparam, const void **ptrs, const int *sizes, int n, int isflush)
{
	long length;
	char head[24];
	int ret, i;

	for (i = 0, length = 0; i < n; i++) {
		length += sizes[i];
	}

	if (length > MAX_PACKET_SIZE) {
		fprintf(stderr, "[ERROR] [CCLIB] invalid length=%ld\n", length);
		assert(length <= MAX_PACKET_SIZE);
		abort();
	}

	if (itm_headint <= 1) {
		if (length > 0x10000) 
		if (length > 0x10000) {
			fprintf(stderr, "[ERROR] [CCLIB] invalid length=%ld\n", length);
			assert(length <= 0x10000);
			abort();
		}
	}
	else if (itm_headint <= 3) {
		if (length > MAX_PACKET_SIZE) {
			fprintf(stderr, "[ERROR] [CCLIB] invalid length=%ld\n", length);
			assert(length <= MAX_PACKET_SIZE);
			abort();
		}
	}
	else if (itm_headint <= 5) {
		if (length > 256) {
			fprintf(stderr, "[ERROR] [CCLIB] invalid length=%ld\n", length);
			assert(length <= 256);
			abort();
		}
	}

	itm_size_set(head, itm_headlen + length);
	itm_write_word(head + itm_hdrsize, event);
	itm_write_dword(head + itm_hdrsize + 2, wparam);
	itm_write_dword(head + itm_hdrsize + 6, lparam);

	ret = cg_iosend (head, itm_headlen, 0) ;

	if (ret != itm_headlen) {
		fprintf(stderr, "[ERROR] [CCLIB] iosend ret=%d length=%ld\n", ret, length);
		assert(ret == itm_headlen);
		abort();
	}

	// if (0 > ret || 0 > length) return ret ;
	long total = 0;

	for (i = 0; i < n; i++) {
		int flush = (i < n - 1)? 0 : isflush;
		ret = cg_iosend(ptrs[i], sizes[i], flush);
		if (ret != sizes[i]) {
			fprintf(stderr, "[ERROR] [CCLIB] error ret=%d length=%ld isflush=%d\n", 
				ret, length, isflush);
			assert(ret == sizes[i]);
			abort();
		}
		total += ret;
	}

	return (int)total;
}


/* low level attach */
int cg_llattach(const char * ip, unsigned short port, int cid)
{
	char login[20];

	itm_size_set(login, itm_hdrsize + 2);
	itm_write_word(login + itm_hdrsize, cid);

	cg_ioinit() ;
	if (0 > sock.open (ip, port)) return -1 ;
	if (0 > cg_iosend (login, itm_hdrsize + 2, 1)) 
		return -1 ;

	return 0 ;
}

XEXPORT void cg_exit (void) 
{
	sock.close () ;
}

void cg_headmode (int headmode) 
{
	static int hdrsize[12] = { 2, 2, 4, 4, 1, 1, 2, 2, 4, 4, 1, 1 };
	static int headinc[12] = { 0, 0, 0, 0, 0, 0, 2, 2, 4, 4, 1, 1 };
	static int headlen[12] = { 12, 12, 14, 14, 11, 11, 12, 12, 14, 14, 11, 11 };
	if (headmode == 12) headmode = 2;
	if (headmode == 13) headmode = 2;
	if (headmode == 14) headmode = 2;
	if (headmode < 0 || headmode >= 12) headmode = 0;
	itm_headmod = headmode;
	itm_headint = (headmode < 6)? (headmode) : (headmode - 6);
	itm_headinc = headinc[headmode];
	itm_hdrsize = hdrsize[headmode];
	itm_headlen = headlen[headmode];
//	printf("headint=%d headlen=%d headinc=%d hdrsize=%d\n", itm_headint, itm_headlen, itm_headinc, itm_hdrsize);
	fflush(stdout);
}


int cg_socknodelay (int nodelay)
{
	return sock.setnodelay(nodelay);
}

extern "C" 
void cg_xor_mask (void *data, long size, unsigned char mask)
{
	unsigned char *ptr = (unsigned char*)data;
	apr_uint32 mm = mask;
	mm = (mm << 8) | (mm << 16) | (mm << 24) | mm;
	for (; size >= 4; ptr += 4, size -= 4) {
		*((apr_uint32*)ptr) ^= mm;
	}
	for (; size > 0; ptr++, size--) {
		ptr[0] ^= mask;
	}
}



#ifdef __cplusplus
} // extern "C"
#endif


 

