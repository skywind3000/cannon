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


#ifdef APHAVE_POLLSET

#include <sys/poll.h>
#include <sys/pollset.h>
#include <errno.h>


static int apx_startup(void);
static int apx_shutdown(void);
static int apx_init_pd(apolld ipd, int param);
static int apx_destroy_pd(apolld ipd);
static int apx_poll_add(apolld ipd, int fd, int mask, void *user);
static int apx_poll_del(apolld ipd, int fd);
static int apx_poll_set(apolld ipd, int fd, int mask);
static int apx_poll_wait(apolld ipd, int timeval);
static int apx_poll_event(apolld ipd, int *fd, int *event, void **user);


//---------------------------------------------------------------------
// pollset descriptor
//---------------------------------------------------------------------
typedef struct
{
	struct APOLLFV fv;
	pollset_t ps;
	int num_fd;
	int num_chg;
	int max_fd;
	int max_chg;
	int results;
	int cur_res;
	int usr_len;
	int limit;
	struct pollfd *mresult;
	struct poll_ctl *mchange;
	struct IVECTOR vresult;
	struct IVECTOR vchange;
}	IPD_POLLSET;



//---------------------------------------------------------------------
// pollset driver，通过驱动结构定义了一组 pollset操作的接口
//---------------------------------------------------------------------
struct APOLL_DRIVER APOLL_POLLSET = {
	sizeof (IPD_POLLSET),	
	APDEVICE_POLLSET,
	100,
	"POLLSET",
	apx_startup,
	apx_shutdown,
	apx_init_pd,
	apx_destroy_pd,
	apx_poll_add,
	apx_poll_del,
	apx_poll_set,
	apx_poll_wait,
	apx_poll_event
};


#ifdef PSTRUCT
#undef PSTRUCT
#endif

#define PSTRUCT IPD_POLLSET


//---------------------------------------------------------------------
// 局部函数定义
//---------------------------------------------------------------------
static int apx_grow(PSTRUCT *ps, int size_fd, int size_chg);


//---------------------------------------------------------------------
// 初始化 pollset设备
//---------------------------------------------------------------------
static int apx_startup(void)
{
	pollset_t ps = pollset_create(-1);
	if (ps < 0) return -1;
	pollset_destroy(ps);
	return 0;
}

//---------------------------------------------------------------------
// 还原 pollset设备
//---------------------------------------------------------------------
static int apx_shutdown(void)
{
	return 0;
}


//---------------------------------------------------------------------
// 初始化 pollset描述符
//---------------------------------------------------------------------
static int apx_init_pd(apolld ipd, int param)
{
	PSTRUCT *ps = PDESC(ipd);
	struct rlimit rl;

	ps->ps = pollset_create(-1);
	if (ps->ps < 0) return -1;

	iv_init(&ps->vresult, NULL);
	iv_init(&ps->vchange, NULL);

	apr_poll_fvinit(&ps->fv, NULL);

	ps->max_fd = 0;
	ps->num_fd = 0;
	ps->max_chg = 0;
	ps->num_chg = 0;
	ps->usr_len = 0;
	ps->limit = 32000;
	
	if (apx_grow(ps, 4, 4)) {
		apx_destroy_pd(ipd);
		return -3;
	}

	param = param + 1;

	return 0;
}


//---------------------------------------------------------------------
// 销毁 pollset描述符
//---------------------------------------------------------------------
static int apx_destroy_pd(apolld ipd)
{
	PSTRUCT *ps = PDESC(ipd);
	iv_destroy(&ps->vresult);
	iv_destroy(&ps->vchange);
	apr_poll_fvdestroy(&ps->fv);

	if (ps->ps >= 0) pollset_destroy(ps->ps);
	ps->ps = -1;
	return 0;
}


//---------------------------------------------------------------------
// 增加 pollset事件列表长度
//---------------------------------------------------------------------
static int apx_grow(PSTRUCT *ps, int size_fd, int size_chg)
{
	int r;
	if (size_fd >= 0) {
		r = iv_resize(&ps->vresult, size_fd * sizeof(struct pollfd) * 2);
		ps->mresult = (struct pollfd*)ps->vresult.data;
		ps->max_fd = size_fd;
		if (r) return -1;
	}
	if (size_chg >= 0) {
		r = iv_resize(&ps->vchange, size_chg * sizeof(struct poll_ctl));
		ps->mchange = (struct poll_ctl*)ps->vchange.data;
		ps->max_chg= size_chg;
		if (r) return -2;
	}
	return 0;
}


//---------------------------------------------------------------------
// 提交 pollset变更事件
//---------------------------------------------------------------------
static int apx_changes_apply(apolld ipd)
{
	PSTRUCT *ps = PDESC(ipd);
	int num = ps->num_chg;
	if (num == 0) return 0;
	pollset_ctl(ps->ps, ps->mchange, num);
	ps->num_chg = 0;
	return 0;
}

//---------------------------------------------------------------------
// 增加一个 pollset变更事件
//---------------------------------------------------------------------
static int apx_changes_push(apolld ipd, int fd, int cmd, int events)
{
	PSTRUCT *ps = PDESC(ipd);
#if 1
	struct poll_ctl *ctl;

	if (ps->num_chg >= ps->max_chg) {
		if (apx_grow(ps, -1, ps->max_chg * 2)) return -3;
	}

	if (ps->num_chg + 1 >= ps->limit) {
		if (apx_changes_apply(ipd) < 0) return -4;
	}

	ctl = &ps->mchange[ps->num_chg++];
	memset(ctl, 0, sizeof(struct poll_ctl));

	ctl->fd = fd;
	ctl->events = events;
	ctl->cmd = cmd;
#else
	struct poll_ctl ctl;
	int hr;
	ctl.fd = fd;
	ctl.events = events;
	ctl.cmd = cmd;
	hr = pollset_ctl(ps->ps, &ctl, 1);
#endif
	//printf("pollset(fd=%d cmd=%d event=%x): %d\n", fd, cmd, events, hr);
	return 0;
}


//---------------------------------------------------------------------
// 给 pollset对象增加一个文件描述
//---------------------------------------------------------------------
static int apx_poll_add(apolld ipd, int fd, int mask, void *user)
{
	PSTRUCT *ps = PDESC(ipd);
	int usr_nlen, i, events;

	if (ps->num_fd >= ps->max_fd) {
		if (apx_grow(ps, ps->max_fd * 2, -1)) return -1;
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
		apx_poll_set(ipd, fd, mask);
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

	if (apx_changes_push(ipd, fd, PS_ADD, events) < 0) {
		return -2;
	}

	ps->num_fd++;

	return 0;
}

//---------------------------------------------------------------------
// 在 pollset对象中删除一个文件描述
//---------------------------------------------------------------------
static int apx_poll_del(apolld ipd, int fd)
{
	PSTRUCT *ps = PDESC(ipd);

	if (ps->num_fd <= 0) return -1;
	if (ps->fv.fds[fd].fd < 0) return -2;
	
	apx_changes_push(ipd, fd, PS_DELETE, 0);

	ps->num_fd--;
	ps->fv.fds[fd].fd = -1;
	ps->fv.fds[fd].user = NULL;
	ps->fv.fds[fd].mask = 0;

	apx_changes_apply(ipd);

	return 0;
}

//---------------------------------------------------------------------
// 设置 pollset对象中某文件描述的事件
//---------------------------------------------------------------------
static int apx_poll_set(apolld ipd, int fd, int mask)
{
	PSTRUCT *ps = PDESC(ipd);
	int events = 0;
	int retval = 0;
	int save;

	if (fd >= ps->usr_len) return -1;
	if (ps->fv.fds[fd].fd < 0) return -2;

	save = ps->fv.fds[fd].mask;
	mask =  mask & (APOLL_IN | APOLL_OUT | APOLL_ERR);

	ps->fv.fds[fd].mask = mask;

	if (mask & APOLL_IN) events |= POLLIN;
	if (mask & APOLL_OUT) events |= POLLOUT;
	if (mask & APOLL_ERR) events |= POLLERR;

	retval = apx_changes_push(ipd, fd, PS_DELETE, 0);
	if (events != 0) {
		retval = apx_changes_push(ipd, fd, PS_MOD, events);
	}

	return retval;
}


//---------------------------------------------------------------------
// 调用 pollset等待函数，捕捉事件
//---------------------------------------------------------------------
static int apx_poll_wait(apolld ipd, int timeval)
{
	PSTRUCT *ps = PDESC(ipd);
	int retval;

	if (ps->num_chg) {
		apx_changes_apply(ipd);
	}

	retval = pollset_poll(ps->ps, ps->mresult, ps->max_fd * 2, timeval);

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
static int apx_poll_event(apolld ipd, int *fd, int *event, void **user)
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
	if (ps->fv.fds[n].fd < 0) {
		eventx = 0;
		apx_changes_push(ipd, n, PS_DELETE, 0);
	}	else {
		eventx &= ps->fv.fds[n].mask;
		if (eventx == 0) {
			apx_changes_push(ipd, n, PS_DELETE, 0);
			if (ps->fv.fds[n].mask != 0) {
				apx_changes_push(ipd, n, PS_MOD, ps->fv.fds[n].mask);
			}
		}
	}

	if (fd) *fd = n;
	if (event) *event = eventx;
	if (user) *user = ps->fv.fds[n].user;

	return 0;
}

#endif


