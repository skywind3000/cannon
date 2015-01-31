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

#include "aprpoll.h"
#include "aprpollh.h"

#include <stdio.h>

struct APOLL_DRIVER APOLLDRV;
struct IMPOOL apr_polld_list;


//---------------------------------------------------------------------
// 所能够提供的设备驱动
//---------------------------------------------------------------------
#ifdef APHAVE_SELECT
extern struct APOLL_DRIVER APOLL_SELECT;
#endif
#ifdef APHAVE_POLL
extern struct APOLL_DRIVER APOLL_POLL;
#endif
#ifdef APHAVE_KQUEUE
extern struct APOLL_DRIVER APOLL_KQUEUE;
#endif
#ifdef APHAVE_EPOLL
extern struct APOLL_DRIVER APOLL_EPOLL;
#endif
#ifdef APHAVE_DEVPOLL
extern struct APOLL_DRIVER APOLL_DEVPOLL;
#endif
#ifdef APHAVE_POLLSET
extern struct APOLL_DRIVER APOLL_POLLSET;
#endif
#ifdef APHAVE_RTSIG
extern struct APOLL_DRIVER APOLL_RTSIG;
#endif
#ifdef APHAVE_WINCP
extern struct APOLL_DRIVER APOLL_WINCP;
#endif

//---------------------------------------------------------------------
// 当前支持的设备驱动列表
//---------------------------------------------------------------------
static struct APOLL_DRIVER *drivers[] = {
#ifdef APHAVE_SELECT
	&APOLL_SELECT,
#endif
#ifdef APHAVE_POLL
	&APOLL_POLL,
#endif
#ifdef APHAVE_KQUEUE
	&APOLL_KQUEUE,
#endif
#ifdef APHAVE_EPOLL
	&APOLL_EPOLL,
#endif
#ifdef APHAVE_DEVPOLL
	&APOLL_DEVPOLL,
#endif
#ifdef APHAVE_POLLSET
	&APOLL_POLLSET,
#endif
#ifdef APHAVE_RTSIG
	&APOLL_RTSIG,
#endif
#ifdef APHAVE_WINCP
	&APOLL_WINCP,
#endif
	NULL
};

static int init_flag = 0;

static apr_mutex pmutex;
static apr_mutex tmutex;

//---------------------------------------------------------------------
// 初始化POLL设备，添入上面设备表中的宏定义
//---------------------------------------------------------------------
int apr_poll_install(int device)
{
	int i, bp;
	long v;

	if (init_flag) return 1;

	if (device > 0) {
		for (i = 0; drivers[i]; i++) if (drivers[i]->id == device) break;
		if (drivers[i] == NULL) return -1;
		APOLLDRV = *drivers[i];
	}	else {
		for (i = 0, bp = -1, device = -1; drivers[i]; i++) {
			if (drivers[i]->performance > bp)
				bp = drivers[i]->performance,
				device = i;
		}
		if (device == -1) return -2;
		APOLLDRV = *drivers[device];
	}

	imp_init(&apr_polld_list, APOLLDRV.pdsize + (int)sizeof(int), NULL);

	i = APOLLDRV.startup();

	if (i) {
		imp_destroy(&apr_polld_list);
		return -10 + i;
	}
	
	v = apr_mutex_init(&pmutex);
	v = apr_mutex_init(&tmutex);

	init_flag = 1;

	return (int)v;
}

//---------------------------------------------------------------------
// 还原设备
//---------------------------------------------------------------------
int apr_poll_remove(void)
{
	int retval = 0;
	if (init_flag == 0) return 0;
	retval = APOLLDRV.shutdown();
	imp_destroy(&apr_polld_list);
	retval = (int)apr_mutex_destroy(tmutex);
	retval = (int)apr_mutex_destroy(pmutex);
	init_flag = 0;

	return retval;
}

//---------------------------------------------------------------------
// 取得当前设备的名字
//---------------------------------------------------------------------
const char *apr_poll_devname(void)
{
	return APOLLDRV.name;
}

//---------------------------------------------------------------------
// 分配一个新的poll队列号
//---------------------------------------------------------------------
static apolld apoll_new(void)
{
	int retval = -1;
	char *lptr = NULL;
	retval = apr_mutex_lock(pmutex);
	retval = imp_newnode(&apr_polld_list);
	if (retval >= 0) {
		lptr = (char*)IMP_DATA(&apr_polld_list, retval);
		*(int*)((void*)lptr) = retval;
		lptr += sizeof(int);
	}
	retval = apr_mutex_unlock(pmutex);
	return lptr;
}

//---------------------------------------------------------------------
// 删除一个已有的poll队列号
//---------------------------------------------------------------------
static int apoll_del(apolld ipd)
{
	int retval = 0;
	char *lptr = NULL;
	retval = apr_mutex_lock(pmutex);
	lptr = (char*)ipd;
	lptr -= sizeof(int);
	imp_delnode(&apr_polld_list, *(int*)((void*)lptr));
	retval = apr_mutex_unlock(pmutex);
	return retval;
}


//---------------------------------------------------------------------
// 初始化一个poll队列句柄
//---------------------------------------------------------------------
int apr_poll_init(apolld *ipd, int param)
{
	apolld pd;
	int retval = 0;

	if (ipd == NULL) return -1;
	pd = apoll_new();
	if (pd == NULL) return -2; 
	retval = APOLLDRV.init_pd(pd, param);
	if (retval) {
		retval = apoll_del(pd);
		return -3;
	}
	*ipd = pd;
	return retval;
}

//---------------------------------------------------------------------
// 释放一个poll队列设备
//---------------------------------------------------------------------
int apr_poll_destroy(apolld ipd)
{
	int retval = 0;
	retval = APOLLDRV.destroy_pd(ipd);
	retval = apoll_del(ipd);
	return retval;
}

//---------------------------------------------------------------------
// 将文件描述加入poll队列
//---------------------------------------------------------------------
int apr_poll_add(apolld ipd, int fd, int mask, void *udata)
{
	return APOLLDRV.poll_add(ipd, fd, mask, udata);
}

//---------------------------------------------------------------------
// 将文件描述从poll队列删除
//---------------------------------------------------------------------
int apr_poll_del(apolld ipd, int fd)
{
	return APOLLDRV.poll_del(ipd, fd);
}

//---------------------------------------------------------------------
// 设置poll队列中某文件描述的事件触发
//---------------------------------------------------------------------
int apr_poll_set(apolld ipd, int fd, int mask)
{
	return APOLLDRV.poll_set(ipd, fd, mask);
}

//---------------------------------------------------------------------
// 等待消息，也就是做一次poll
//---------------------------------------------------------------------
int apr_poll_wait(apolld ipd, int timeval)
{
	return APOLLDRV.poll_wait(ipd, timeval);
}

//---------------------------------------------------------------------
// 从队列中取得单个事件，成功则返回0，事件队空则返回非零，此时需重wait
//---------------------------------------------------------------------
int apr_poll_event(apolld ipd, int *fd, int *event, void **udata)
{
	int retval;
	do {
		retval = APOLLDRV.poll_event(ipd, fd, event, udata);
	}	while (*event == 0 && retval == 0);
	return retval;
}


//---------------------------------------------------------------------
// 初始化描述列表
//---------------------------------------------------------------------
void apr_poll_fvinit(struct APOLLFV *fv, struct IALLOCATOR *allocator)
{
	iv_init(&fv->vec, allocator);
	fv->count = 0;
	fv->fds = NULL;
}

//---------------------------------------------------------------------
// 销毁描述列表
//---------------------------------------------------------------------
void apr_poll_fvdestroy(struct APOLLFV *fv)
{
	iv_destroy(&fv->vec);
	fv->count = 0;
	fv->fds = NULL;
}

//---------------------------------------------------------------------
// 改变描述列表大小
//---------------------------------------------------------------------
int apr_poll_fvresize(struct APOLLFV *fv, int size)
{
	int retval = 0;

	if (size <= (int)fv->count) return 0;
	retval = iv_resize(&fv->vec, (int)sizeof(struct APOLLFD) * size);
	if (retval) return -1;
	fv->fds = (struct APOLLFD*)((void*)fv->vec.data);
	fv->count = (unsigned long)size;

	return 0;
}

