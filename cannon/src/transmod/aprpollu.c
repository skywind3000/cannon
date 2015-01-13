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


#ifdef APHAVE_DEVPOLL

#include <stdio.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/queue.h>
#include <sys/devpoll.h>
#include <poll.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>


static int apu_startup(void);
static int apu_shutdown(void);
static int apu_init_pd(apolld ipd, int param);
static int apu_destroy_pd(apolld ipd);
static int apu_poll_add(apolld ipd, int fd, int mask, void *user);
static int apu_poll_del(apolld ipd, int fd);
static int apu_poll_set(apolld ipd, int fd, int mask);
static int apu_poll_wait(apolld ipd, int timeval);
static int apu_poll_event(apolld ipd, int *fd, int *event, void **user);


//---------------------------------------------------------------------
// devpoll descriptor
//---------------------------------------------------------------------
typedef struct
{
	struct APOLLFV fv;
	int dpfd;
	int num_fd;
	int num_chg;
	int max_fd;
	int max_chg;
	int results;
	int cur_res;
	int usr_len;
	int limit;
	struct pollfd *mresult;
	struct pollfd *mchange;
	struct IVECTOR vresult;
	struct IVECTOR vchange;
}	IPD_DEVPOLL;



//---------------------------------------------------------------------
// devpoll driver，通过驱动结构定义了一组 devpoll操作的接口
//---------------------------------------------------------------------
struct APOLL_DRIVER APOLL_DEVPOLL = {
	sizeof (IPD_DEVPOLL),	
	APDEVICE_DEVPOLL,
	100,
	"DEVPOLL",
	apu_startup,
	apu_shutdown,
	apu_init_pd,
	apu_destroy_pd,
	apu_poll_add,
	apu_poll_del,
	apu_poll_set,
	apu_poll_wait,
	apu_poll_event
};


#ifdef PSTRUCT
#undef PSTRUCT
#endif

#define PSTRUCT IPD_DEVPOLL


//---------------------------------------------------------------------
// 局部函数定义
//---------------------------------------------------------------------
static int apu_grow(PSTRUCT *ps, int size_fd, int size_chg);


//---------------------------------------------------------------------
// 初始化 devpoll设备
//---------------------------------------------------------------------
static int apu_startup(void)
{
	int fd, flags = O_RDWR;
#ifdef O_CLOEXEC
	flags |= O_CLOEXEC;
#endif
	fd = open("/dev/poll", flags);
	if (fd < 0) return -1;
#if (!defined(O_CLOEXEC)) && defined(FD_CLOEXEC)
	if (fcntl(fd, F_SETFD, FD_CLOEXEC) < 0) {
		close(fd);
		return -2;
	}
#endif
	close(fd);
	return 0;
}

//---------------------------------------------------------------------
// 还原 devpoll设备
//---------------------------------------------------------------------
static int apu_shutdown(void)
{
	return 0;
}


//---------------------------------------------------------------------
// 初始化 devpoll描述符
//---------------------------------------------------------------------
static int apu_init_pd(apolld ipd, int param)
{
	PSTRUCT *ps = PDESC(ipd);
	int flags = O_RDWR;
	struct rlimit rl;

#ifdef O_CLOEXEC
	flags |= O_CLOEXEC;
#endif

	ps->dpfd = open("/dev/poll", flags);
	if (ps->dpfd < 0) return -1;

#if (!defined(O_CLOEXEC)) && defined(FD_CLOEXEC)
	if (fcntl(ps->dpfd, F_SETFD, FD_CLOEXEC) < 0) {
		close(ps->dpfd);
		ps->dpfd = -1;
		return -2;
	}
#endif

	iv_init(&ps->vresult, NULL);
	iv_init(&ps->vchange, NULL);

	apr_poll_fvinit(&ps->fv, NULL);

	ps->max_fd = 0;
	ps->num_fd = 0;
	ps->max_chg = 0;
	ps->num_chg = 0;
	ps->usr_len = 0;
	ps->limit = 32000;

	if (getrlimit(RLIMIT_NOFILE, &rl) == 0) {
		if (rl.rlim_cur != RLIM_INFINITY) {
			if (rl.rlim_cur < 32000) {
				ps->limit = rl.rlim_cur;
			}
		}
	}
	
	if (apu_grow(ps, 4, 4)) {
		apu_destroy_pd(ipd);
		return -3;
	}

	return 0;
}


//---------------------------------------------------------------------
// 销毁 devpoll描述符
//---------------------------------------------------------------------
static int apu_destroy_pd(apolld ipd)
{
	PSTRUCT *ps = PDESC(ipd);
	iv_destroy(&ps->vresult);
	iv_destroy(&ps->vchange);
	apr_poll_fvdestroy(&ps->fv);

	if (ps->dpfd >= 0) close(ps->dpfd);
	ps->dpfd = -1;
	return 0;
}


//---------------------------------------------------------------------
// 增加 devpoll事件列表长度
//---------------------------------------------------------------------
static int apu_grow(PSTRUCT *ps, int size_fd, int size_chg)
{
	int r;
	if (size_fd >= 0) {
		r = iv_resize(&ps->vresult, size_fd * sizeof(struct pollfd) * 2);
		ps->mresult = (struct pollfd*)ps->vresult.data;
		ps->max_fd = size_fd;
		if (r) return -1;
	}
	if (size_chg >= 0) {
		r = iv_resize(&ps->vchange, size_chg * sizeof(struct pollfd));
		ps->mchange = (struct pollfd*)ps->vchange.data;
		ps->max_chg= size_chg;
		if (r) return -2;
	}
	return 0;
}


//---------------------------------------------------------------------
// 提交 devpoll变更事件
//---------------------------------------------------------------------
static int apu_changes_apply(apolld ipd)
{
	PSTRUCT *ps = PDESC(ipd);
	int num = ps->num_chg;
	if (num == 0) return 0;
	if (pwrite(ps->dpfd, ps->mchange, sizeof(struct pollfd) * num, 0) < 0)
		return -1;
	ps->num_chg = 0;
	return 0;
}

//---------------------------------------------------------------------
// 增加一个 devpoll变更事件
//---------------------------------------------------------------------
static int apu_changes_push(apolld ipd, int fd, int events)
{
	PSTRUCT *ps = PDESC(ipd);
	struct pollfd *pfd;

	if (fd >= ps->usr_len) return -1;
	if (ps->fv.fds[fd].fd < 0) return -2;
	if (ps->num_chg >= ps->max_chg) {
		if (apu_grow(ps, -1, ps->max_chg * 2)) return -3;
	}

	if (ps->num_chg + 1 >= ps->limit) {
		if (apu_changes_apply(ipd) < 0) return -4;
	}

	pfd = &ps->mchange[ps->num_chg++];
	memset(pfd, 0, sizeof(struct pollfd));

	pfd->fd = fd;
	pfd->events = events;
	pfd->revents = 0;

	return 0;
}


//---------------------------------------------------------------------
// 给 devpoll对象增加一个文件描述
//---------------------------------------------------------------------
static int apu_poll_add(apolld ipd, int fd, int mask, void *user)
{
	PSTRUCT *ps = PDESC(ipd);
	int usr_nlen, i, events;

	if (ps->num_fd >= ps->max_fd) {
		if (apu_grow(ps, ps->max_fd * 2, -1)) return -1;
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
		apu_poll_set(ipd, fd, mask);
		return 0;
	}

	events = 0;
	mask = mask & (APOLL_IN | APOLL_OUT | APOLL_ERR);

	if (mask & APOLL_IN) events |= POLLIN;
	if (mask & APOLL_OUT) events |= POLLOUT;
	if (mask & APOLL_ERR) events |= POLLERR;

	ps->fv.fds[fd].fd = fd;
	ps->fv.fds[fd].user = user;
	ps->fv.fds[fd].mask = mask & (APOLL_IN | APOLL_OUT | APOLL_ERR);

	if (apu_changes_push(ipd, fd, events) < 0) {
		return -2;
	}

	ps->num_fd++;

	return 0;
}

//---------------------------------------------------------------------
// 在 devpoll对象中删除一个文件描述
//---------------------------------------------------------------------
static int apu_poll_del(apolld ipd, int fd)
{
	PSTRUCT *ps = PDESC(ipd);

	if (ps->num_fd <= 0) return -1;
	if (ps->fv.fds[fd].fd < 0) return -2;
	
	apu_changes_push(ipd, fd, POLLREMOVE);

	ps->num_fd--;
	ps->fv.fds[fd].fd = -1;
	ps->fv.fds[fd].user = NULL;
	ps->fv.fds[fd].mask = 0;

	apu_changes_apply(ipd);

	return 0;
}

//---------------------------------------------------------------------
// 设置 devpoll对象中某文件描述的事件
//---------------------------------------------------------------------
static int apu_poll_set(apolld ipd, int fd, int mask)
{
	PSTRUCT *ps = PDESC(ipd);
	int events = 0;
	int retval;
	int save;

	if (fd >= ps->usr_len) return -1;
	if (ps->fv.fds[fd].fd < 0) return -2;

	save = ps->fv.fds[fd].mask;
	mask =  mask & (APOLL_IN | APOLL_OUT | APOLL_ERR);

	if ((save & mask) != save) 
		apu_changes_push(ipd, fd, POLLREMOVE);

	ps->fv.fds[fd].mask = mask;

	if (mask & APOLL_IN) events |= POLLIN;
	if (mask & APOLL_OUT) events |= POLLOUT;
	if (mask & APOLL_ERR) events |= POLLERR;

	retval = apu_changes_push(ipd, fd, events);

	return retval;
}


//---------------------------------------------------------------------
// 调用 devpoll等待函数，捕捉事件
//---------------------------------------------------------------------
static int apu_poll_wait(apolld ipd, int timeval)
{
	PSTRUCT *ps = PDESC(ipd);
	struct dvpoll dvp;
	int retval;

	if (ps->num_chg) {
		apu_changes_apply(ipd);
	}

	dvp.dp_fds = ps->mresult;
	dvp.dp_nfds = ps->max_fd * 2;
	dvp.dp_timeout = timeval;

	if (dvp.dp_nfds > ps->limit) {
		dvp.dp_nfds = ps->limit;
	}

	retval = ioctl(ps->dpfd, DP_POLL, &dvp);

	if (retval < 0) {
		if (errno != EINTR) {
			return -1;
		}
		return 0;
	}

	ps->results = retval;
	ps->cur_res = 0;

	return ps->results;
}


//---------------------------------------------------------------------
// 从捕捉的事件列表中查找下一个事件
//---------------------------------------------------------------------
static int apu_poll_event(apolld ipd, int *fd, int *event, void **user)
{
	PSTRUCT *ps = PDESC(ipd);
	int revents, eventx = 0, n;
	struct pollfd *pfd;
	if (ps->results <= 0) return -1;
	if (ps->cur_res >= ps->results) return -2;
	pfd = &ps->mresult[ps->cur_res++];

	revents = pfd->revents;
	if (revents & POLLIN) eventx |= APOLL_IN;
	if (revents & POLLOUT)eventx |= APOLL_OUT;
	if (revents & POLLERR)eventx |= APOLL_ERR;

	n = pfd->fd;
	if (ps->fv.fds[n].fd < 0) eventx = 0;
	eventx &= ps->fv.fds[n].mask;

	if (fd) *fd = n;
	if (event) *event = eventx;
	if (user) *user = ps->fv.fds[n].user;

	return 0;
}

#endif


