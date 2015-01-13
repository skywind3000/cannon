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

#ifdef APHAVE_KQUEUE
#include <sys/event.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>

static int apk_startup(void);
static int apk_shutdown(void);
static int apk_init_pd(apolld ipd, int param);
static int apk_destroy_pd(apolld ipd);
static int apk_poll_add(apolld ipd, int fd, int mask, void *user);
static int apk_poll_del(apolld ipd, int fd);
static int apk_poll_set(apolld ipd, int fd, int mask);
static int apk_poll_wait(apolld ipd, int timeval);
static int apk_poll_event(apolld ipd, int *fd, int *event, void **user);


//---------------------------------------------------------------------
// kqueue操作的描述符结构
//---------------------------------------------------------------------
typedef struct
{
	struct APOLLFV fv;
	int kqueue;
	int num_fd;
	int num_chg;
	int max_fd;
	int max_chg;
	int results;
	int cur_res;
	int usr_len;
	struct kevent *mchange;
	struct kevent *mresult;
	long   stimeval;
	struct timespec stimespec;
	struct IVECTOR vchange;
	struct IVECTOR vresult;
}	IPD_KQUEUE;

//---------------------------------------------------------------------
// kqueue设备驱动，通过驱动结构定义了一组kqueue操作的接口
//---------------------------------------------------------------------
struct APOLL_DRIVER APOLL_KQUEUE = {
	sizeof (IPD_KQUEUE),	
	APDEVICE_KQUEUE,
	100,
	"KQUEUE",
	apk_startup,
	apk_shutdown,
	apk_init_pd,
	apk_destroy_pd,
	apk_poll_add,
	apk_poll_del,
	apk_poll_set,
	apk_poll_wait,
	apk_poll_event
};


#ifdef PSTRUCT
#undef PSTRUCT
#endif

#define PSTRUCT IPD_KQUEUE


//---------------------------------------------------------------------
// 初始化kqueue设备
//---------------------------------------------------------------------
static int apk_startup(void)
{
	return 0;
}

//---------------------------------------------------------------------
// 还原kqueue设备
//---------------------------------------------------------------------
static int apk_shutdown(void)
{
	return 0;
}

//---------------------------------------------------------------------
// 增加kqueue的事件列表长度
//---------------------------------------------------------------------
static int apk_grow(PSTRUCT *ps, int size_fd, int size_chg);

//---------------------------------------------------------------------
// 初始化kqueue描述符
//---------------------------------------------------------------------
static int apk_init_pd(apolld ipd, int param)
{
	PSTRUCT *ps = PDESC(ipd);
	ps->kqueue = kqueue();
	if (ps->kqueue < 0) return -1;

#ifdef FD_CLOEXEC
	fcntl(ps->kqueue, F_SETFD, FD_CLOEXEC);
#endif

	iv_init(&ps->vchange, NULL);
	iv_init(&ps->vresult, NULL);
	apr_poll_fvinit(&ps->fv, NULL);

	ps->max_fd = 0;
	ps->max_chg= 0;
	ps->num_fd = 0;
	ps->num_chg= 0;
	ps->usr_len = 0;
	ps->stimeval = -1;
	param = param;

	if (apk_grow(ps, 4, 4)) {
		apk_destroy_pd(ipd);
		return -3;
	}

	return 0;
}

//---------------------------------------------------------------------
// 销毁kqueue描述符
//---------------------------------------------------------------------
static int apk_destroy_pd(apolld ipd)
{
	PSTRUCT *ps = PDESC(ipd);

	iv_destroy(&ps->vchange);
	iv_destroy(&ps->vresult);
	apr_poll_fvdestroy(&ps->fv);

	if (ps->kqueue >= 0) close(ps->kqueue);
	ps->kqueue = -1;

	return 0;
}

//---------------------------------------------------------------------
// 增加kqueue事件列表长度
//---------------------------------------------------------------------
static int apk_grow(PSTRUCT *ps, int size_fd, int size_chg)
{
	int r;
	if (size_fd >= 0) {
		r = iv_resize(&ps->vresult, size_fd * sizeof(struct kevent) * 2);
		ps->mresult = (struct kevent*)ps->vresult.data;
		ps->max_fd = size_fd;
		if (r) return -1;
	}
	if (size_chg >= 0) {
		r = iv_resize(&ps->vchange, size_chg * sizeof(struct kevent));
		ps->mchange = (struct kevent*)ps->vchange.data;
		ps->max_chg= size_chg;
		if (r) return -2;
	}
	return 0;
}

//---------------------------------------------------------------------
// 增加一个kqueue变更事件
//---------------------------------------------------------------------
static int apk_poll_kevent(apolld ipd, int fd, int filter, int flag)
{
	PSTRUCT *ps = PDESC(ipd);
	struct kevent *ke;

	if (fd >= ps->usr_len) return -1;
	if (ps->fv.fds[fd].fd < 0) return -2;
	if (ps->num_chg >= ps->max_chg)
		if (apk_grow(ps, -1, ps->max_chg * 2)) return -3;

	ke = &ps->mchange[ps->num_chg++];
	memset(ke, 0, sizeof(struct kevent));
	ke->ident = fd;
	ke->filter = filter;
	ke->flags = flag;

	return 0;
}

//---------------------------------------------------------------------
// 给kqueue增加一个文件描述
//---------------------------------------------------------------------
static int apk_poll_add(apolld ipd, int fd, int mask, void *user)
{
	PSTRUCT *ps = PDESC(ipd);
	int usr_nlen, i, flag;

	if (ps->num_fd >= ps->max_fd) {
		if (apk_grow(ps, ps->max_fd * 2, -1)) return -1;
	}

	if (fd + 1 >= ps->usr_len) {
		usr_nlen = fd + 128;
		apr_poll_fvresize(&ps->fv, usr_nlen);
		for (i = ps->usr_len; i < usr_nlen; i++) {
			ps->fv.fds[i].fd = -1;
			ps->fv.fds[i].mask = 0;
			ps->fv.fds[i].user = NULL;
		}
		ps->usr_len = usr_nlen;
	}

	if (ps->fv.fds[fd].fd >= 0) {
		ps->fv.fds[fd].user = user;
		apk_poll_set(ipd, fd, mask);
		return 0;
	}

	ps->fv.fds[fd].fd = fd;
	ps->fv.fds[fd].user = user;
	ps->fv.fds[fd].mask = mask;

	flag = (mask & APOLL_IN)? EV_ENABLE : EV_DISABLE;
	if (apk_poll_kevent(ipd, fd, EVFILT_READ, EV_ADD | flag)) {
		ps->fv.fds[fd].fd = -1;
		ps->fv.fds[fd].user = NULL;
		ps->fv.fds[fd].mask = 0;
		return -3;
	}
	flag = (mask & APOLL_OUT)? EV_ENABLE : EV_DISABLE;
	if (apk_poll_kevent(ipd, fd, EVFILT_WRITE, EV_ADD | flag)) {
		ps->fv.fds[fd].fd = -1;
		ps->fv.fds[fd].user = NULL;
		ps->fv.fds[fd].mask = 0;
		return -4;
	}
	ps->num_fd++;

	return 0;
}

//---------------------------------------------------------------------
// 在kqueue中删除一个文件描述
//---------------------------------------------------------------------
static int apk_poll_del(apolld ipd, int fd)
{
	PSTRUCT *ps = PDESC(ipd);

	if (ps->num_fd <= 0) return -1;
	if (fd >= ps->usr_len) return -2;
	if (ps->fv.fds[fd].fd < 0) return -3;

	if (apk_poll_kevent(ipd, fd, EVFILT_READ, EV_DELETE | EV_DISABLE)) return -4;
	if (apk_poll_kevent(ipd, fd, EVFILT_WRITE, EV_DELETE| EV_DISABLE)) return -5;

	ps->num_fd--;
	kevent(ps->kqueue, ps->mchange, ps->num_chg, NULL, 0, 0);
	ps->num_chg = 0;
	ps->fv.fds[fd].fd = -1;
	ps->fv.fds[fd].user = 0;
	ps->fv.fds[fd].mask = 0;

	return 0;
}

//---------------------------------------------------------------------
// 设置kqueue中某文件描述的事件
//---------------------------------------------------------------------
static int apk_poll_set(apolld ipd, int fd, int mask)
{
	PSTRUCT *ps = PDESC(ipd);

	if (fd >= ps->usr_len) return -3;
	if (ps->fv.fds[fd].fd < 0) return -4;

	if (mask & APOLL_IN) {
		if (apk_poll_kevent(ipd, fd, EVFILT_READ, EV_ENABLE)) return -1;
	}	else {
		if (apk_poll_kevent(ipd, fd, EVFILT_READ, EV_DISABLE)) return -2;
	}
	if (mask & APOLL_OUT) {
		if (apk_poll_kevent(ipd, fd, EVFILT_WRITE, EV_ENABLE)) return -1;
	}	else {
		if (apk_poll_kevent(ipd, fd, EVFILT_WRITE, EV_DISABLE)) return -2;
	}
	ps->fv.fds[fd].mask = mask;

	return 0;
}

//---------------------------------------------------------------------
// 调用kqueue主函数，捕捉事件
//---------------------------------------------------------------------
static int apk_poll_wait(apolld ipd, int timeval)
{
	PSTRUCT *ps = PDESC(ipd);
	struct timespec tm;

	if (timeval != ps->stimeval) {
		ps->stimeval = timeval;
		ps->stimespec.tv_sec = timeval / 1000;
		ps->stimespec.tv_nsec = (timeval % 1000) * 1000000;	
	}
	tm = ps->stimespec;

	ps->results = kevent(ps->kqueue, ps->mchange, ps->num_chg, ps->mresult,
		ps->max_fd * 2, (timeval >= 0) ? &tm : (struct timespec *) 0);
	ps->cur_res = 0;
	ps->num_chg = 0;

	return ps->results;
}

//---------------------------------------------------------------------
// 从捕捉的事件列表中查找下一个事件
//---------------------------------------------------------------------
static int apk_poll_event(apolld ipd, int *fd, int *event, void **user)
{
	PSTRUCT *ps = PDESC(ipd);
	struct kevent *ke;
	int revent = 0, n;
	if (ps->cur_res >= ps->results) return -1;

	ke = &ps->mresult[ps->cur_res++];
	n = ke->ident;

	if (ke->filter == EVFILT_READ) revent = APOLL_IN;
	else if (ke->filter == EVFILT_WRITE)revent = APOLL_OUT;
	else revent = APOLL_ERR;
	if ((ke->flags & EV_ERROR)) revent = APOLL_ERR;

	if (ps->fv.fds[n].fd < 0) revent = 0;
	revent &= ps->fv.fds[n].mask;

	if (fd) *fd = n;
	if (event) *event = revent;
	if (user) *user = ps->fv.fds[n].user;

	return 0;
}


#endif

