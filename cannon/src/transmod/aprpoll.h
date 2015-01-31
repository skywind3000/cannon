//=====================================================================
//
// The platform independence poll wrapper, Skywind 2004
//
// HISTORY:
// Dec. 15 2004   skywind  created
// Jul. 25 2005   skywind  change poll desc. from int to apolld
// Jul. 27 2005   skywind  improve apr_poll_event method
//
// NOTE：
// 异步I/O包装，使得不同的系统中都可以使用相同的I/O异步信号模型，其
// 中通用的select，但是在不同的平台下面有不同的实现，unix下实现的
// kqueue/epoll而windows下实现的完备端口，最终使得所有的平台异步I/O都
// 面向统一的接口
//
//=====================================================================

#ifndef __APR_POLL_H__
#define __APR_POLL_H__

#include "aprsys.h"

// 支持的设备表
#define APDEVICE_AUTO		0	// 自动选择驱动
#define APDEVICE_SELECT		1	// 使用select驱动
#define APDEVICE_POLL		2	// 使用poll驱动
#define APDEVICE_KQUEUE		3	// 使用kqueue驱动
#define APDEVICE_EPOLL		4	// 使用epoll驱动
#define APDEVICE_DEVPOLL	5	// 使用dev/poll驱动
#define APDEVICE_POLLSET	6	// 试用pollset驱动
#define APDEVICE_WINCP		7	// 使用完成端口驱动
#define APDEVICE_RTSIG		8	// 使用rtsig驱动

#define APOLL_IN	1	// 事件：文件句柄输入事件
#define APOLL_OUT	2	// 事件：文件句柄输出事件
#define APOLL_ERR	4	// 事件：文件句柄错误事件


typedef void* apolld;

#ifdef __cplusplus
extern "C" {
#endif


// 初始化POLL设备，添入上面设备表中的宏定义
int apr_poll_install(int device);

// 还原设备
int apr_poll_remove(void);

// 取得当前设备的名字
const char *apr_poll_devname(void);

// 初始化一个poll队列句柄
int apr_poll_init(apolld *ipd, int param);

// 释放一个poll队列设备
int apr_poll_destroy(apolld ipd);

// 将文件描述加入poll队列
int apr_poll_add(apolld ipd, int fd, int mask, void *udata);

// 将文件描述从poll队列删除
int apr_poll_del(apolld ipd, int fd);

// 设置poll队列中某文件描述的事件触发
int apr_poll_set(apolld ipd, int fd, int mask);

// 等待消息，也就是做一次poll
int apr_poll_wait(apolld ipd, int timeval);

// 从队列中取得单个事件，成功则返回0，事件队空则返回非零，此时需要重新wait
int apr_poll_event(apolld ipd, int *fd, int *event, void **udata);

#ifdef __cplusplus
}
#endif


#endif

