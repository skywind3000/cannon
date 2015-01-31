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


#ifndef __APR_SYS_H__
#define __APR_SYS_H__

#include <stddef.h>
#include <stdlib.h>


//=====================================================================
// 32BIT INTEGER DEFINITION 
//=====================================================================
#ifndef __INTEGER_32_BITS__
#define __INTEGER_32_BITS__
#if defined(__UINT32_TYPE__) && defined(__UINT32_TYPE__)
	typedef __UINT32_TYPE__ ISTDUINT32;
	typedef __INT32_TYPE__ ISTDINT32;
#elif defined(__UINT_FAST32_TYPE__) && defined(__INT_FAST32_TYPE__)
	typedef __UINT_FAST32_TYPE__ ISTDUINT32;
	typedef __INT_FAST32_TYPE__ ISTDINT32;
#elif defined(_WIN64) || defined(WIN64) || defined(__amd64__) || \
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
#elif defined(__GNUC__) && (__GNUC__ > 3)
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


//=====================================================================
// Integer Type Decleration
//=====================================================================

/* Typedefs that APR needs. */
typedef  unsigned char     apr_byte;
typedef  short             apr_int16;
typedef  unsigned short    apr_uint16;
typedef  ISTDINT32         apr_int32;
typedef  ISTDUINT32        apr_uint32;


#if !(defined(_MSC_VER) || defined(__LCC__))
typedef long long apr_int64;
typedef unsigned long long apr_uint64;
#else
typedef __int64 apr_int64;
typedef unsigned __int64 apr_uint64;
#endif

#if defined(__APPLE__) && (!defined(__unix))
	#define __unix 1
#endif

#if defined(__unix__) || defined(unix) || defined(__linux)
	#ifndef __unix
		#define __unix 1
	#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif


//=====================================================================
// Interfaces
//=====================================================================

#define APR_THREAD_FUNC
#define APR_DECLARE(type)  type 

#define APR_SUCCESS        0


// 线程级别休眠，单位为 ms
APR_DECLARE(void) apr_sleep(unsigned long time);

// 读取 1000Hz的一个时钟，七小时重复一次
APR_DECLARE(long) apr_clock(void);

// 读取自1970年1月1日，00:00以来的精确时钟
APR_DECLARE(void) apr_timeofday(long *sec, long *usec);

// 读取 1MHz的时钟
APR_DECLARE(apr_int64) apr_timex(void);

typedef void (APR_THREAD_FUNC *apr_thread_start)(void*);

// 线程的创建、退出和中断函数
APR_DECLARE(long) apr_thread_create(long* tid, const apr_thread_start tproc, const void *tattr, void *targs);
APR_DECLARE(long) apr_thread_exit(long retval);
APR_DECLARE(long) apr_thread_join(long threadid);
APR_DECLARE(long) apr_thread_detach(long threadid);
APR_DECLARE(long) apr_thread_kill(long threadid);

// 互斥类型
typedef void* apr_mutex;

#ifndef APR_MAX_MUTEX
#define APR_MAX_MUTEX 0x10000
#endif

// 互斥类型的创建、消除、锁定及解锁操作
APR_DECLARE(long)      apr_mutex_init(apr_mutex *m);
APR_DECLARE(long)      apr_mutex_lock(apr_mutex m);
APR_DECLARE(long)      apr_mutex_unlock(apr_mutex m);
APR_DECLARE(long)      apr_mutex_trylock(apr_mutex m);
APR_DECLARE(long)      apr_mutex_destroy(apr_mutex m);


#ifdef __cplusplus
}
#endif

#endif

