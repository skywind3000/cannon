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


#ifdef APHAVE_EPOLL

#include <stdio.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <fcntl.h>

#ifndef APR_EPOLL_LIMIT
#define APR_EPOLL_LIMIT 20000
#endif

static int ape_startup(void);
static int ape_shutdown(void);
static int ape_init_pd(apolld ipd, int param);
static int ape_destroy_pd(apolld ipd);
static int ape_poll_add(apolld ipd, int fd, int mask, void *user);
static int ape_poll_del(apolld ipd, int fd);
static int ape_poll_set(apolld ipd, int fd, int mask);
static int ape_poll_wait(apolld ipd, int timeval);
static int ape_poll_event(apolld ipd, int *fd, int *event, void **user);


//---------------------------------------------------------------------
// epoll操作的描述符结构
//---------------------------------------------------------------------
typedef struct
{
	struct APOLLFV fv;
	int epfd;
	int num_fd;
	int max_fd;
	int results;
	int cur_res;
	int usr_len;
	struct epoll_event *mresult;
	struct IVECTOR vresult;
}	IPD_EPOLL;

//---------------------------------------------------------------------
// epoll设备驱动，通过驱动结构定义了一组epoll操作的接口
//---------------------------------------------------------------------
struct APOLL_DRIVER APOLL_EPOLL = {
	sizeof (IPD_EPOLL),	
	APDEVICE_EPOLL,
	100,
	"EPOLL",
	ape_startup,
	ape_shutdown,
	ape_init_pd,
	ape_destroy_pd,
	ape_poll_add,
	ape_poll_del,
	ape_poll_set,
	ape_poll_wait,
	ape_poll_event
};


#ifdef PSTRUCT
#undef PSTRUCT
#endif

#define PSTRUCT IPD_EPOLL


//---------------------------------------------------------------------
// 初始化epoll设备
//---------------------------------------------------------------------
static int ape_startup(void)
{
	int epfd = epoll_create(20);
	if (epfd < 0) return -1000 - errno;
	close(epfd);
	return 0;
}

//---------------------------------------------------------------------
// 还原epoll设备
//---------------------------------------------------------------------
static int ape_shutdown(void)
{
	return 0;
}

//---------------------------------------------------------------------
// 初始化epoll描述符
//---------------------------------------------------------------------
static int ape_init_pd(apolld ipd, int param)
{
	PSTRUCT *ps = PDESC(ipd);
	ps->epfd = epoll_create(param);
	if (ps->epfd < 0) return -1;

#ifdef FD_CLOEXEC
	fcntl(ps->epfd, F_SETFD, FD_CLOEXEC);
#endif

	iv_init(&ps->vresult, NULL);
	apr_poll_fvinit(&ps->fv, NULL);

	ps->max_fd = 0;
	ps->num_fd = 0;
	ps->usr_len = 0;
	
	if (iv_resize(&ps->vresult, 4 * sizeof(struct epoll_event))) {
		close(ps->epfd);
		ps->epfd = -1;
		return -2;
	}

	ps->mresult = (struct epoll_event*)ps->vresult.data;
	ps->max_fd = 4;

	return 0;
}

//---------------------------------------------------------------------
// 销毁epoll描述符
//---------------------------------------------------------------------
static int ape_destroy_pd(apolld ipd)
{
	PSTRUCT *ps = PDESC(ipd);
	iv_destroy(&ps->vresult);
	apr_poll_fvdestroy(&ps->fv);

	if (ps->epfd >= 0) close(ps->epfd);
	ps->epfd = -1;
	return 0;
}


//---------------------------------------------------------------------
// 给epoll对象增加一个文件描述
//---------------------------------------------------------------------
static int ape_poll_add(apolld ipd, int fd, int mask, void *user)
{
	PSTRUCT *ps = PDESC(ipd);
	int usr_nlen, i;
	struct epoll_event ee;

	if (ps->num_fd >= ps->max_fd) {
		i = (ps->max_fd <= 0)? 4 : ps->max_fd * 2;
		if (iv_resize(&ps->vresult, i * sizeof(struct epoll_event))) return -1;
		ps->mresult = (struct epoll_event*)ps->vresult.data;
		ps->max_fd = i;
	}
	if (fd >= ps->usr_len) {
		usr_nlen = fd + 128;
		apr_poll_fvresize(&ps->fv, usr_nlen);
		for (i = ps->usr_len; i < usr_nlen; i++) {
			ps->fv.fds[i].fd = -1;
			ps->fv.fds[i].user = NULL;
			ps->fv.fds[i].mask = 0;
		}
		ps->usr_len = usr_nlen;
	}
	if (ps->fv.fds[fd].fd >= 0) {
		ps->fv.fds[fd].user = user;
		ape_poll_set(ipd, fd, mask);
		return 0;
	}
	ps->fv.fds[fd].fd = fd;
	ps->fv.fds[fd].user = user;
	ps->fv.fds[fd].mask = mask;

	ee.events = 0;
	ee.data.fd = fd;

	if (mask & APOLL_IN) ee.events |= EPOLLIN;
	if (mask & APOLL_OUT) ee.events |= EPOLLOUT;
	if (mask & APOLL_ERR) ee.events |= EPOLLERR | EPOLLHUP;

	if (epoll_ctl(ps->epfd, EPOLL_CTL_ADD, fd, &ee)) {
		ps->fv.fds[fd].fd = -1;
		ps->fv.fds[fd].user = NULL;
		ps->fv.fds[fd].mask = 0;
		return -3;
	}
	ps->num_fd++;

	return 0;
}

//---------------------------------------------------------------------
// 在epoll对象中删除一个文件描述
//---------------------------------------------------------------------
static int ape_poll_del(apolld ipd, int fd)
{
	PSTRUCT *ps = PDESC(ipd);
	struct epoll_event ee;

	if (ps->num_fd <= 0) return -1;
	if (ps->fv.fds[fd].fd < 0) return -2;

	ee.events = 0;
	ee.data.fd = fd;

	epoll_ctl(ps->epfd, EPOLL_CTL_DEL, fd, &ee);
	ps->num_fd--;
	ps->fv.fds[fd].fd = -1;
	ps->fv.fds[fd].user = NULL;
	ps->fv.fds[fd].mask = 0;

	return 0;
}

//---------------------------------------------------------------------
// 设置epoll对象中某文件描述的事件
//---------------------------------------------------------------------
static int ape_poll_set(apolld ipd, int fd, int mask)
{
	PSTRUCT *ps = PDESC(ipd);
	struct epoll_event ee;
	int retval;

	ee.events = 0;
	ee.data.fd = fd;

	if (fd >= ps->usr_len) return -1;
	if (ps->fv.fds[fd].fd < 0) return -2;

	ps->fv.fds[fd].mask = 0;

	if (mask & APOLL_IN) {
		ee.events |= EPOLLIN;
		ps->fv.fds[fd].mask |= APOLL_IN;
	}
	if (mask & APOLL_OUT) {
		ee.events |= EPOLLOUT;
		ps->fv.fds[fd].mask |= APOLL_OUT;
	}
	if (mask & APOLL_ERR) {
		ee.events |= EPOLLERR | EPOLLHUP;
		ps->fv.fds[fd].mask |= APOLL_ERR;
	}

	retval = epoll_ctl(ps->epfd, EPOLL_CTL_MOD, fd, &ee);
	if (retval) return -10000 + retval;

	return 0;
}

//---------------------------------------------------------------------
// 调用epoll主函数，捕捉事件
//---------------------------------------------------------------------
static int ape_poll_wait(apolld ipd, int timeval)
{
	PSTRUCT *ps = PDESC(ipd);

	ps->results = epoll_wait(ps->epfd, ps->mresult, ps->max_fd * 2, timeval);
	ps->cur_res = 0;

	return ps->results;
}

//---------------------------------------------------------------------
// 从捕捉的事件列表中查找下一个事件
//---------------------------------------------------------------------
static int ape_poll_event(apolld ipd, int *fd, int *event, void **user)
{
	PSTRUCT *ps = PDESC(ipd);
	struct epoll_event *ee;
	int revent = 0, n;

	if (ps->cur_res >= ps->results) return -1;

	ee = &ps->mresult[ps->cur_res++];
	n = ee->data.fd;
	if (fd) *fd = n;

	if (ee->events & EPOLLIN) revent |= APOLL_IN;
	if (ee->events & EPOLLOUT) revent |= APOLL_OUT;
	if (ee->events & (EPOLLERR | EPOLLHUP)) revent |= APOLL_ERR; 

	if (ps->fv.fds[n].fd < 0) revent = 0;
	revent &= ps->fv.fds[n].mask;

	if (event) *event = revent;
	if (user) *user = ps->fv.fds[n].user;

	return 0;
}


#endif

