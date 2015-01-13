//=====================================================================
//
// The platform independence poll wrapper, Skywind 2004
//
// HISTORY:
// Dec. 15 2004   skywind  created
// Jul. 25 2005   skywind  change poll desc. from int to apolld
// Jul. 27 2005   skywind  improve apr_poll_event method
//
// 异步I/O包装，使得不同的系统中都可以使用相同的I/O异步信号模型，其
// 中通用的select，但是在不同的平台下面有不同的实现，unix下实现的
// kqueue/epoll而windows下实现的完备端口，最终使得所有的平台异步I/O都
// 面向统一的接口
//
//=====================================================================
#include "aprpoll.h"
#include "aprpollh.h"

#include "aprsock.h"

#include <stdio.h>

#ifdef APHAVE_SELECT

static int aps_startup(void);
static int aps_shutdown(void);
static int aps_init_pd(apolld ipd, int param);
static int aps_destroy_pd(apolld ipd);
static int aps_poll_add(apolld ipd, int fd, int mask, void *user);
static int aps_poll_del(apolld ipd, int fd);
static int aps_poll_set(apolld ipd, int fd, int mask);
static int aps_poll_wait(apolld ipd, int timeval);
static int aps_poll_event(apolld ipd, int *fd, int *event, void **user);

//---------------------------------------------------------------------
// select操作的描述符结构
//---------------------------------------------------------------------
typedef struct
{
	struct APOLLFV fv;
	fd_set fdr, fdw, fde;
	fd_set fdrtest,fdwtest,fdetest;
	int max_fd;
	int min_fd;
	int cur_fd;
	int cnt_fd;
	int rbits;
}	IPD_SELECT;

//---------------------------------------------------------------------
// select设备驱动，通过驱动结构定义了一组select操作的接口
//---------------------------------------------------------------------
struct APOLL_DRIVER APOLL_SELECT = {
	sizeof (IPD_SELECT),
	APDEVICE_SELECT,
	0,
	"SELECT",
	aps_startup,
	aps_shutdown,
	aps_init_pd,
	aps_destroy_pd,
	aps_poll_add,
	aps_poll_del,
	aps_poll_set,
	aps_poll_wait,
	aps_poll_event
};

#ifdef PSTRUCT
#undef PSTRUCT
#endif

#define PSTRUCT IPD_SELECT

//---------------------------------------------------------------------
// 初始化select设备
//---------------------------------------------------------------------
static int aps_startup(void)
{
	return 0;
}

//---------------------------------------------------------------------
// 还原select设备
//---------------------------------------------------------------------
static int aps_shutdown(void)
{
	return 0;
}


//---------------------------------------------------------------------
// 初始化select描述符
//---------------------------------------------------------------------
static int aps_init_pd(apolld ipd, int param)
{
	int retval = 0;
	PSTRUCT *ps = PDESC(ipd);
	ps->max_fd = 0;
	ps->min_fd = 0x7fffffff;
	ps->cur_fd = 0;
	ps->cnt_fd = 0;
	ps->rbits = 0;
	FD_ZERO(&ps->fdr);
	FD_ZERO(&ps->fdw);
	FD_ZERO(&ps->fde);
	param = param;
	apr_poll_fvinit(&ps->fv, NULL);
	
	if (apr_poll_fvresize(&ps->fv, 4)) {
		retval = aps_destroy_pd(ipd);
		return -2;
	}

	return retval;
}


//---------------------------------------------------------------------
// 销毁select描述符
//---------------------------------------------------------------------
static int aps_destroy_pd(apolld ipd)
{
	PSTRUCT *ps = PDESC(ipd);
	apr_poll_fvdestroy(&ps->fv);

	return 0;
}

//---------------------------------------------------------------------
// 给select增加一个文件描述
//---------------------------------------------------------------------
static int aps_poll_add(apolld ipd, int fd, int mask, void *user)
{
	PSTRUCT *ps = PDESC(ipd);
	int oldmax = ps->max_fd, i;

	#ifdef __unix
	if (fd >= FD_SETSIZE) return -1;
	#else
	if (ps->cnt_fd >= FD_SETSIZE) return -1;
	#endif

	if (ps->max_fd < fd) ps->max_fd = fd;
	if (ps->min_fd > fd) ps->min_fd = fd;
	if (mask & APOLL_IN) FD_SET((unsigned)fd, &ps->fdr);
	if (mask & APOLL_OUT) FD_SET((unsigned)fd, &ps->fdw);
	if (mask & APOLL_ERR) FD_SET((unsigned)fd, &ps->fde);

	if (apr_poll_fvresize(&ps->fv, ps->max_fd + 2)) return -2;

	for (i = oldmax + 1; i <= ps->max_fd; i++) {
		ps->fv.fds[i].fd = -1;
	}
	ps->fv.fds[fd].fd = fd;
	ps->fv.fds[fd].user = user;
	ps->fv.fds[fd].mask = mask;

	ps->cnt_fd++;

	return 0;
}

//---------------------------------------------------------------------
// 在select中删除一个文件描述
//---------------------------------------------------------------------
static int aps_poll_del(apolld ipd, int fd)
{
	PSTRUCT *ps = PDESC(ipd);
	int mask = 0;

	if (fd > ps->max_fd) return -1;
	mask = ps->fv.fds[fd].mask;
	if (ps->fv.fds[fd].fd < 0) return -2;

	if (mask & APOLL_IN) FD_CLR((unsigned)fd, &ps->fdr);
	if (mask & APOLL_OUT) FD_CLR((unsigned)fd, &ps->fdw);
	if (mask & APOLL_ERR) FD_CLR((unsigned)fd, &ps->fde);

	ps->fv.fds[fd].fd = -1;
	ps->fv.fds[fd].user = NULL;
	ps->fv.fds[fd].mask = 0;

	ps->cnt_fd--;

	return 0;
}

//---------------------------------------------------------------------
// 设置select中某文件描述的事件
//---------------------------------------------------------------------
static int aps_poll_set(apolld ipd, int fd, int mask)
{
	PSTRUCT *ps = PDESC(ipd);
	int omask = 0;

	if (ps->fv.fds[fd].fd < 0) return -1;
	omask = ps->fv.fds[fd].mask;

	if (omask & APOLL_IN) {
		if (!(mask & APOLL_IN)) FD_CLR((unsigned)fd, &ps->fdr);
	}	else {
		if (mask & APOLL_IN) FD_SET((unsigned)fd, &ps->fdr);
	}
	if (omask & APOLL_OUT) {
		if (!(mask & APOLL_OUT)) FD_CLR((unsigned)fd, &ps->fdw);
	}	else {
		if (mask & APOLL_OUT) FD_SET((unsigned)fd, &ps->fdw);
	}
	if (omask & APOLL_ERR) {
		if (!(mask & APOLL_ERR)) FD_CLR((unsigned)fd, &ps->fde);
	}	else {
		if (mask & APOLL_ERR) FD_SET((unsigned)fd, &ps->fde);
	}
	ps->fv.fds[fd].mask = mask;

	return 0;
}

//---------------------------------------------------------------------
// 调用select主函数，捕捉事件
//---------------------------------------------------------------------
static int aps_poll_wait(apolld ipd, int timeval)
{
	PSTRUCT *ps = PDESC(ipd);
	int nbits;
	struct timeval timeout;

	timeout.tv_sec  = timeval / 1000;
	timeout.tv_usec = (timeval % 1000) * 1000;
	ps->fdrtest = ps->fdr;
	ps->fdwtest = ps->fdw;
	ps->fdetest = ps->fde;
	nbits = select(ps->max_fd + 1, &ps->fdrtest, &ps->fdwtest, 
		&ps->fdetest, (timeval < 0)? NULL : &timeout);
	if (nbits < 0) return -1;

	ps->cur_fd = ps->min_fd - 1;
	ps->rbits = nbits;
	return (nbits == 0)? 0 : nbits;
}

//---------------------------------------------------------------------
// 从捕捉的事件列表中查找下一个事件
//---------------------------------------------------------------------
static int aps_poll_event(apolld ipd, int *fd, int *event, void **user)
{
	PSTRUCT *ps = PDESC(ipd);
	int revents, n;

	if (ps->rbits < 1) return -1;
	for (revents=0; ps->cur_fd++ < ps->max_fd; ) {
		if (FD_ISSET(ps->cur_fd, &ps->fdrtest)) revents = APOLL_IN;
		if (FD_ISSET(ps->cur_fd, &ps->fdwtest)) revents |= APOLL_OUT;
		if (FD_ISSET(ps->cur_fd, &ps->fdetest)) revents |= APOLL_ERR;
		if (revents) break;
	}

	if (!revents) return -2;

	if (revents & APOLL_IN)  ps->rbits--;
	if (revents & APOLL_OUT) ps->rbits--;
	if (revents & APOLL_ERR) ps->rbits--;

	n = ps->cur_fd;
	if (ps->fv.fds[n].fd < 0) revents = 0;
	revents &= ps->fv.fds[n].mask;

	if (fd) *fd = n;
	if (event) *event = revents;
	if (user) *user = ps->fv.fds[n].user;

	return 0;
}

#endif

