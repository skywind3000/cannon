//=====================================================================
//
// The platform independence poll wrapper, Skywind 2004
//
// HISTORY:
// Dec. 15 2004   skywind  created
// Jul. 25 2005   skywind  change poll desc. from int to apolld
// Jul. 27 2005   skywind  improve apr_poll_event method
//
// NOTE：本文件为POLL 实现内部头文件
// 异步I/O包装，使得不同的系统中都可以使用相同的I/O异步信号模型，其
// 中通用的select，但是在不同的平台下面有不同的实现，unix下实现的
// kqueue/epoll而windows下实现的完备端口，最终使得所有的平台异步I/O都
// 面向统一的接口
//
//=====================================================================

#ifndef __APR_POLLH_H__
#define __APR_POLLH_H__

#include "icvector.h"
#include "impoold.h"
#include "aprpoll.h"

#if defined(_WIN32) || defined(__unix)
#define APHAVE_SELECT
#endif
#if defined(__unix)
#define APHAVE_POLL
#endif
#if defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__) \
	|| (defined(__APPLE__) && defined(__MACH__))
#define APHAVE_KQUEUE
#endif
#if defined(linux)
#define APHAVE_EPOLL
//#define APHAVE_RTSIG
#endif
#if defined(_WIN32)
//#define APHAVE_WINCP
#endif
#if defined(sun) || defined(__sun) || defined(__sun__)
#define APHAVE_DEVPOLL
#endif

#ifdef __cplusplus
extern "C" {
#endif

//---------------------------------------------------------------------
// POLL 设备驱动
//---------------------------------------------------------------------
struct APOLL_DRIVER
{
	int pdsize;								// poll描述符大小
	int id;									// 设备id
	int performance;						// 设备效率
	const char *name;						// 设备名称
	int (*startup)(void);					// 设备初始化
	int (*shutdown)(void);					// 设备关闭
	int (*init_pd)(apolld ipd, int param);	// 初始化poll描述符
	int (*destroy_pd)(apolld ipd);			// 销毁poll描述符
	int (*poll_add)(apolld ipd, int fd, int mask, void *udata);  // 增加
	int (*poll_del)(apolld ipd, int fd);						 // 删除
	int (*poll_set)(apolld ipd, int fd, int mask);		// 设置事件
	int (*poll_wait)(apolld ipd, int timeval);			// 等待捕捉事件
	int (*poll_event)(apolld ipd, int *fd, int *event, void **udata);
};

extern struct IPOLL_DRIVER IPOLLDRV;	// 但前的设备驱动
extern struct IMPOOL  apr_polld_list;	// poll描述符结构列表

#define PSTRUCT void					// 定义基本结构体
#define PDESC(pd) ((PSTRUCT*)(pd))		// 定义结构体转换


//---------------------------------------------------------------------
// 基本的描述节点
//---------------------------------------------------------------------
struct APOLLFD	
{
	int fd;		// 文件描述符
	int mask;	// 描述事件mask
	int event;	// 产生的事件
	int index;	// 对应表格索引
	void*user;	// 用户定义数据
};

//---------------------------------------------------------------------
// 基本的描述列表
//---------------------------------------------------------------------
struct APOLLFV 
{
	struct APOLLFD *fds;	// 文件描述数组
	struct IVECTOR  vec;	// 文件描述矢量
	unsigned long count;	// 文件描述大小
};

// 初始化描述列表
void apr_poll_fvinit(struct APOLLFV *fv, struct IALLOCATOR *allocator);

// 销毁描述列表
void apr_poll_fvdestroy(struct APOLLFV *fv);

// 改变描述列表大小
int apr_poll_fvresize(struct APOLLFV *fv, int size);



#ifdef __cplusplus
}
#endif

#endif

