//=====================================================================
//
// TML <Transmod Library>, by skywind 2004, itransmod.c
//
// HISTORY:
// Dec. 25 2004   skywind  created and implement tcp operation
// Aug. 19 2005   skywind  implement udp operation
// Oct. 27 2005   skywind  interface add set nodelay in SYSCD
// Nov. 25 2005   skywind  extend connection close status
// Dec. 02 2005   skywind  implement channel timer event
// Dec. 17 2005   skywind  implement ioctl event
// Apr. 27 2010   skywind  fixed: when sys-timer changed, maybe error
// Mar. 15 2011   skywind  64bit support, header size configurable
// Jun. 25 2011   skywind  implement channel subscribe
// Sep. 09 2011   skywind  new: socket buf resize, congestion ctrl.
// Nov. 30 2011   skywind  new: channel broadcasting (v2.40)
// Dec. 23 2011   skywind  new: rc4 crypt (v2.43)
// Dec. 28 2011   skywind  rc4 enchance (v2.44)
//
// NOTES： 
// 网络传输库 TML<传输模块>，建立 客户/频道的通信模式，提供基于多频道 
// multi-channel通信的 TCP/UDP传输控制，缓存/内存管理，超时控制等服务
//
//=====================================================================

#include "itransmod.h"
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <signal.h>
#include <assert.h>


//=====================================================================
// Global Variables Definition
//=====================================================================
int itm_outer_port4 = 3000;		// IPv4 对外监听端口
int itm_inner_port4 = 3008;		// IPv4 对内监听端口
int itm_dgram_port4 = 0;		// IPv4 数据报端口
int itm_outer_port6 = -1;		// IPv6 对外监听端口
int itm_inner_port6 = -1;		// IPv6 对内监听端口
int itm_dgram_port6 = -1;		// IPv6 数据报端口

int itm_outer_sock4 = -1;		// IPv4 对外监听套接字
int itm_inner_sock4 = -1;		// IPv4 对内监听套接字
int itm_dgram_sock4 = -1;		// IPv4 数据报套接字
int itm_outer_sock6 = -1;		// IPv6 对外监听套接字
int itm_inner_sock6 = -1;		// IPv6 对内监听套接字
int itm_dgram_sock6 = -1;		// IPv6 数据报套接字

int itm_outer_max = 8192;		// 对外最大连接
int itm_inner_max = 4096;		// 对内最大连接
int itm_outer_cnt = 0;			// 对外当前连接
int itm_inner_cnt = 0;			// 对内当前连接

apolld itm_polld = NULL;		// 提供给APR_POLL的事件捕捉描述字

int itm_state = 0;				// 当前状态
int itm_error = 0;				// 错误代码
int itm_psize = 4096;			// 内存页面大小
int itm_backlog = 1024;			// 最大监听的backlog
int itm_counter = 0;			// 计算HID的增量
int itm_udpmask = 0;			// 数据报丢包掩码

int itm_headmod = 0;			// 头部模式
int itm_headint = 0;			// 头部类型
int itm_headinc = 0;			// 头部增量
int itm_headlen = 12;			// 头部长度
int itm_hdrsize = 2;			// 长度的长度
int itm_headmsk = 0;			// 是否启用头部掩码

long itm_outer_time = 180;		// 外部连接生命时间：五分钟
long itm_inner_time = 3600;		// 内部连接生命时间：一小时
long itm_wtime  = 0;			// 世界时钟
long itm_datamax = 0x200000;	// 最长数据长度
long itm_limit = 0;				// 发送缓存最长数据

long itm_inner_addr4 = 0;		// 内部监听绑定的IP
long itm_logmask = 0;			// 日志掩码，0为不输出日志
char itm_msg[4097];				// 消息字符串

long itm_outer_blimit = 8192;	// 外部套接字缓存极限
long itm_inner_blimit = 65536;	// 内部频道缓存极限
long itm_dgram_blimit = 655360;	// 数据报套接字缓存
long itm_socksndi = -1;			// 内部套接字发送缓存大小
long itm_sockrcvi = -1;			// 内部套接字接受缓存大小
long itm_socksndo = -1;			// 外部套接字发送缓存大小
long itm_sockrcvo = -1;			// 外部套接字发送缓存大小

struct IVECTOR itm_hostv;		// 内部Channel列表矢量
struct IVECTOR itm_datav;		// 内部数据矢量
struct ITMD **itm_host = NULL;	// 内部Channel列表指针
char *itm_data = NULL;			// 内部数据字节指针
char *itm_crypt = NULL;			// 内部数据加密指针
long  itm_hostc= 0;				// 内部Channel数量
long  itm_datac= 0;				// 内部数据长度

struct ITMD itmd_inner4;		// IPv4 内部监听的ITMD(套接字描述)
struct ITMD itmd_outer4;		// IPv4 外部监听的ITMD(套接字描述)
struct ITMD itmd_dgram4;		// IPv4 数据报套接字的ITMD
struct ITMD itmd_inner6;		// IPv6 内部监听的ITMD(套接字描述)
struct ITMD itmd_outer6;		// IPv6 外部监听的ITMD(套接字描述)
struct ITMD itmd_dgram6;		// IPv6 数据报套接字的ITMD

struct IMSTREAM itm_dgramdat4;	// 数据报缓存
struct IMSTREAM itm_dgramdat6;	// IPv6 数据报缓存

static int itm_mode = 0;		// 系统模式
static int itm_event_count = 0;	// 事件数量

struct IMPOOL itm_fds;			// 套接字描述结构分配器
struct IMPOOL itm_mem;			// 内存页面分配器

struct IDTIMEV itm_timeu;		// 外部连接超时控制器
struct IDTIMEV itm_timec;		// 内部连接超时控制器

long *itm_wlist = NULL;			// 发送连接队列
long  itm_wsize = 0;			// 发送队列长度

apr_int64 itm_time_start = 0;		// 启动的时间
apr_int64 itm_time_current = 0;		// 当前的时间
apr_int64 itm_time_slap = 0;		// 时钟脉冲时间

apr_int64 itm_notice_slap = 0;		// 频道时钟信号定义
apr_int64 itm_notice_cycle = 0;		// 频道时钟周期
apr_int64 itm_notice_count = 0;		// 频道始终信号记时
apr_int64 itm_notice_saved = 0;		// 频道记时开始


long itm_dropped = 0;			// 丢弃的不合理数据报
long itm_utiming = 0;			// 客户端计时模式

long itm_interval = 0;			// 间隔模式

long itm_local1 = 0;			// 局部变量用作临时参数定义1
long itm_local2 = 0;			// 局部变量用作临时参数定义2

long itm_fastmode = 0;			// 是否启用写列表
long itm_httpskip = 0;			// 是否跳过类 http头部

short *itm_book[512];			// 每种事件关注的频道列表
int itm_booklen[512];			// 每种事件关注的频道数量
struct IVECTOR itm_bookv[512];	// 每种事件关注的频道列表缓存

int itm_dhcp_index = -1;		// 频道分配索引
int itm_dhcp_base = 100;		// 频道分配基址
int itm_dhcp_high = 8000;		// 频道分配限制

apr_int64 itm_stat_send = 0;		// 统计：发送了多少包
apr_int64 itm_stat_recv = 0;		// 统计：接收了多少包
apr_int64 itm_stat_discard = 0;		// 统计：放弃了多少个数据包

int itm_reuseaddr = 0;				// 地址复用：0自动，1允许 2禁止
int itm_reuseport = 0;				// 端口复用：0自动，1允许 2禁止	

char *itm_document = NULL;			// 文档变量
long itm_docsize = 0;				// 文档长度
unsigned int itm_version = 0;		// 文档版本

struct IMPOOL itm_msg_mem;			// 外部事件队列
struct IMSTREAM itm_msg_n0;			// 外部事件数据
struct IMSTREAM itm_msg_n1;			// 外部事件数据
apr_mutex itm_msg_lock = NULL;		// 外部事件锁
apr_uint32 itm_msg_cnt0 = 0;		// 外部事件计数器
apr_uint32 itm_msg_cnt1 = 0;		// 外部事件计数器


// 内部监听绑定的 IPv6地址
char itm_inner_addr6[32] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };


//=====================================================================
// Static Functions Definition
//=====================================================================
static int itm_stime = 0;

static int itm_socket_create(void);
static int itm_socket_release(void);
static int itm_timer_routine(void);

long itm_trysend(struct ITMD *itmd);	// 尝试发送 wstream
long itm_tryrecv(struct ITMD *itmd);	// 尝试接收 rstream


//=====================================================================
// Interface Functions Implement
//=====================================================================

//---------------------------------------------------------------------
// itm_startup
//---------------------------------------------------------------------
int itm_startup(void)
{
	long retval, i;

	if (itm_mode) return 0;

	// 异步I/O驱动初始化
	retval = apr_poll_install(APDEVICE_AUTO);
	if (retval) {
		itm_log(ITML_BASE, "service starting failed: poll device error %d", retval);
		return -10 + retval;
	}

	// 内存节点管理器初始化
	imp_init(&itm_fds, sizeof(struct ITMD), NULL);
	imp_init(&itm_mem, itm_psize, NULL);

	// 初始化内存增长限制
	itm_mem.grow_limit = 4096;

	// 套接字初始化
	retval = itm_socket_create();
	if (retval) {
		itm_log(ITML_BASE, "service starting failed: starting listen error %d", retval);
		return -20 + retval;
	}
	retval = apr_poll_init(&itm_polld, 0x20000);
	if (retval) {
		itm_socket_release();
		itm_log(ITML_BASE, "service starting failed: poll init error %d", retval);
		return -30 + retval;
	}
	retval = apr_poll_add(itm_polld, itm_outer_sock4, APOLL_IN, &itmd_outer4);
	if (retval) {
		itm_socket_release();
		apr_poll_destroy(itm_polld);
		itm_polld = NULL;
		itm_log(ITML_BASE, "service starting failed: poll event error %d", retval);
		return -40 + retval;
	}
	retval = apr_poll_add(itm_polld, itm_inner_sock4, APOLL_IN, &itmd_inner4);
	if (retval) {
		itm_socket_release();
		apr_poll_destroy(itm_polld);
		itm_polld = NULL;
		itm_log(ITML_BASE, "service starting failed: poll event error %d", retval);
		return -50 + retval;
	}
	retval = apr_poll_add(itm_polld, itm_dgram_sock4, 0, &itmd_dgram4);
	if (retval) {
		itm_socket_release();
		apr_poll_destroy(itm_polld);
		itm_polld = NULL;
		itm_log(ITML_BASE, "service starting failed: poll event error %d", retval);
		return -60 + retval;
	}
	
	itm_mask(&itmd_dgram4, APOLL_IN, 0);

#ifdef AF_INET
	if (itm_outer_sock6 >= 0) {
		retval = apr_poll_add(itm_polld, itm_outer_sock6, APOLL_IN, &itmd_outer6);
		if (retval) {
			itm_socket_release();
			apr_poll_destroy(itm_polld);
			itm_polld = NULL;
			itm_log(ITML_BASE, "service starting failed: poll event error %d", retval);
			return -70 + retval;
		}
	}

	if (itm_inner_sock6 >= 0) {
		retval = apr_poll_add(itm_polld, itm_inner_sock6, APOLL_IN, &itmd_inner6);
		if (retval) {
			itm_socket_release();
			apr_poll_destroy(itm_polld);
			itm_polld = NULL;
			itm_log(ITML_BASE, "service starting failed: poll event error %d", retval);
			return -80 + retval;
		}
	}

	if (itm_dgram_sock6 >= 0) {
		retval = apr_poll_add(itm_polld, itm_dgram_sock6, 0, &itmd_dgram6);
		if (retval) {
			itm_socket_release();
			apr_poll_destroy(itm_polld);
			itm_polld = NULL;
			itm_log(ITML_BASE, "service starting failed: poll event error %d", retval);
			return -90 + retval;
		}

		itm_mask(&itmd_dgram6, APOLL_IN, 0);
	}
#endif

	idt_init(&itm_timeu, itm_outer_time, NULL);
	idt_init(&itm_timec, itm_inner_time, NULL);

	iv_init(&itm_hostv, NULL);
	iv_init(&itm_datav, NULL);
	if (itm_datamax < 0x100000) itm_datamax = 0x100000;
	itm_dsize(itm_datamax + 0x10000);
	itm_wchannel(32000, NULL);

	ims_init(&itm_dgramdat4, &itm_mem);
	ims_init(&itm_dgramdat6, &itm_mem);

	itm_state = 0;
	itm_outer_cnt = 0;
	itm_inner_cnt = 0;

	itm_wtime = 0;
	itm_stime = (int)time(NULL);
	itm_time_start = apr_timex() / 1000;
	itm_time_current = itm_time_start;
	itm_time_slap = itm_time_start + ITMD_TIME_CYCLE;

	itm_notice_slap = 0;
	itm_notice_cycle = -1;
	itm_notice_count = 0;

	itm_version = 0;
	itm_docsize = 0;
	itm_document[0] = 0;

	imp_init(&itm_msg_mem, 4096, NULL);
	ims_init(&itm_msg_n0, &itm_msg_mem);
	ims_init(&itm_msg_n1, &itm_msg_mem);
	apr_mutex_init(&itm_msg_lock);
	itm_msg_cnt0 = 0;
	itm_msg_cnt1 = 0;

	itm_wsize = 0;
	
	switch (itm_headmod)
	{
	case ITMH_WORDLSB:
	case ITMH_WORDMSB:
	case ITMH_EWORDLSB:
	case ITMH_EWORDMSB:
		itm_headlen = 12;
		itm_hdrsize = 2;
		break;
	case ITMH_DWORDLSB:
	case ITMH_DWORDMSB:
	case ITMH_EDWORDLSB:
	case ITMH_EDWORDMSB:
	case ITMH_DWORDMASK:
		itm_headlen = 14;
		itm_hdrsize = 4;
		break;
	case ITMH_BYTELSB:
	case ITMH_BYTEMSB:
	case ITMH_EBYTELSB:
	case ITMH_EBYTEMSB:
		itm_headlen = 11;
		itm_hdrsize = 1;
		break;
	case ITMH_RAWDATA:
		itm_headlen = 14;
		itm_hdrsize = 4;
		break;
	case ITMH_LINESPLIT:
		itm_headlen = 14;
		itm_hdrsize = 4;
		break;
	default:
		itm_headmod = ITMH_WORDLSB;
		itm_headlen = 12;
		itm_hdrsize = 2;
		break;
	}
	
	if (itm_headmod < ITMH_EWORDLSB) {
		itm_headint = itm_headmod;
		itm_headinc = 0;
		itm_headmsk = 0;
	}	
	else if (itm_headmod < ITMH_DWORDMASK) {
		itm_headint = itm_headmod - 6;
		itm_headinc = itm_hdrsize;
		itm_headmsk = 0;
	}
	else if (itm_headmod == ITMH_DWORDMASK) {
		itm_headint = ITMH_DWORDLSB;
		itm_headinc = 0;
		itm_headmsk = 1;
	}
	else if (itm_headmod == ITMH_RAWDATA) {
		itm_headint = ITMH_DWORDLSB;
		itm_headinc = 0;
		itm_headmsk = 0;
	}
	else if (itm_headmod == ITMH_LINESPLIT) {
		itm_headint = ITMH_DWORDLSB;
		itm_headinc = 0;
		itm_headmsk = 0;
	}
	else {
		itm_headint = ITMH_DWORDLSB;
		itm_headinc = 0;
		itm_headmsk = 0;
	}

	for (i = 0; i < 512; i++) {
		itm_book[i] = NULL;
		itm_booklen[i] = 0;
		iv_init(&itm_bookv[i], NULL);
	}

	itm_mode = 1;

	itm_stat_send = 0;
	itm_stat_recv = 0;
	itm_stat_discard = 0;

	if (itm_limit < itm_inner_blimit * 2) {
		itm_limit = itm_inner_blimit * 2;
	}

	if (itm_dhcp_base < 100) itm_dhcp_base = 100;
	if (itm_dhcp_high < itm_dhcp_base) itm_dhcp_high = itm_dhcp_base;

	itm_log(ITML_BASE, "Transmod %x.%d%d (%s, %s) started ....", 
		ITMV_VERSION >> 8, (ITMV_VERSION & 0xf0) >> 4, ITMV_VERSION & 0x0f, __DATE__, __TIME__);

	return 0;
}

//---------------------------------------------------------------------
// itm_shutdown
//---------------------------------------------------------------------
int itm_shutdown(void)
{
	struct ITMD *itmd;
	int i, n, s, count = 0;
	if (itm_mode == 0) return 0;

	s = itm_logmask;
	itm_logmask = 0;
	for (i = itm_fds.node_max; i >= 0; i--) {
		n = imp_nodehead(&itm_fds);
		if (n >= 0) {
			itmd = (struct ITMD*)IMP_DATA(&itm_fds, n);
			if (itmd) { itm_event_close(itmd, 0); count++; }
		}	else break;
	}

	itm_logmask = s;
	itm_socket_release();
	ims_destroy(&itm_dgramdat4);
	ims_destroy(&itm_dgramdat6);

	itm_document = NULL;
	itm_docsize = 0;
	itm_version = 0;

	apr_poll_destroy(itm_polld);
	iv_destroy(&itm_hostv);
	iv_destroy(&itm_datav);
	itm_host = NULL;
	itm_data = NULL;
	idt_destroy(&itm_timeu);
	idt_destroy(&itm_timec);

	imp_destroy(&itm_fds);
	imp_destroy(&itm_mem);
	itm_polld = NULL;

	for (i = 0; i < 512; i++) {
		itm_book[i] = NULL;
		itm_booklen[i] = 0;
		iv_destroy(&itm_bookv[i]);
	}

	apr_mutex_lock(itm_msg_lock);
	ims_destroy(&itm_msg_n0);
	ims_destroy(&itm_msg_n1);
	imp_destroy(&itm_msg_mem);
	apr_mutex_unlock(itm_msg_lock);
	apr_mutex_destroy(itm_msg_lock);

	itm_mode = 0;

	itm_log(ITML_BASE, "service shuting down");

	return 0;
}

//---------------------------------------------------------------------
// itm_socket_create
//---------------------------------------------------------------------
static int itm_socket_create(void)
{
	unsigned long noblock1 = 1, noblock2 = 1, noblock3 = 1;
	unsigned long reuseaddr = 0;
	unsigned long reuseport = 0;
	unsigned long buffer1 = 0;
	unsigned long buffer2 = 0;
	struct sockaddr_in host_outer4;
	struct sockaddr_in host_inner4;
	struct sockaddr_in host_dgram4;
#ifdef AF_INET6
	struct sockaddr_in6 host_outer6;
	struct sockaddr_in6 host_inner6;
	struct sockaddr_in6 host_dgram6;
#endif

	itm_socket_release();

	itm_outer_sock4 = apr_socket(PF_INET, SOCK_STREAM, 0);
	if (itm_outer_sock4 < 0) return -1;
	itm_inner_sock4 = apr_socket(PF_INET, SOCK_STREAM, 0);
	if (itm_inner_sock4 < 0) {
		itm_socket_release();
		return -1;
	}
	itm_dgram_sock4 = apr_socket(PF_INET, SOCK_DGRAM, 0);
	if (itm_dgram_sock4 < 0) {
		itm_socket_release();
		return -2;
	}

	apr_enable(itm_outer_sock4, APR_CLOEXEC);
	apr_enable(itm_inner_sock4, APR_CLOEXEC);
	apr_enable(itm_dgram_sock4, APR_CLOEXEC);

	memset(&host_outer4, 0, sizeof(host_outer4));
	memset(&host_inner4, 0, sizeof(host_inner4));
	memset(&host_dgram4, 0, sizeof(host_dgram4));

	// 配置套接字监听地址
	host_outer4.sin_addr.s_addr = 0;
	host_inner4.sin_addr.s_addr = itm_inner_addr4;
	host_dgram4.sin_addr.s_addr = 0;
	host_outer4.sin_port = htons((short)itm_outer_port4);
	host_inner4.sin_port = htons((short)itm_inner_port4);
	host_dgram4.sin_port = htons((short)itm_dgram_port4);
	host_outer4.sin_family = PF_INET;
	host_inner4.sin_family = PF_INET;
	host_dgram4.sin_family = PF_INET;

	// 设置套接字参数
	apr_ioctl(itm_outer_sock4, FIONBIO, &noblock1);
	apr_ioctl(itm_inner_sock4, FIONBIO, &noblock2);
	apr_ioctl(itm_dgram_sock4, FIONBIO, &noblock3);
	
	// 地址复用
	if (itm_reuseaddr == 0) {
	#if defined(WIN32) || defined(_WIN32) || defined(WIN64) || defined(_WIN64)
		reuseaddr = 0;
	#else
		reuseaddr = 1;
	#endif
	}
	else {
		reuseaddr = (itm_reuseaddr == 1)? 1 : 0;
	}

	// 端口复用
	reuseport = (itm_reuseport == 1)? 1 : 0;

	apr_setsockopt(itm_outer_sock4, SOL_SOCKET, SO_REUSEADDR, (char*)&reuseaddr, sizeof(reuseaddr));
	apr_setsockopt(itm_inner_sock4, SOL_SOCKET, SO_REUSEADDR, (char*)&reuseaddr, sizeof(reuseaddr));
	apr_setsockopt(itm_dgram_sock4, SOL_SOCKET, SO_REUSEADDR, (char*)&reuseaddr, sizeof(reuseaddr));

#ifdef SO_REUSEPORT
	apr_setsockopt(itm_outer_sock4, SOL_SOCKET, SO_REUSEPORT, (char*)&reuseport, sizeof(reuseport));
	apr_setsockopt(itm_inner_sock4, SOL_SOCKET, SO_REUSEPORT, (char*)&reuseport, sizeof(reuseport));
	apr_setsockopt(itm_dgram_sock4, SOL_SOCKET, SO_REUSEPORT, (char*)&reuseport, sizeof(reuseport));
#endif

	buffer1 = itm_dgram_blimit;
	buffer2 = itm_dgram_blimit;

	if (itm_dgram_blimit > 0) {
		apr_setsockopt(itm_dgram_sock4, SOL_SOCKET, SO_RCVBUF, (char*)&buffer1, sizeof(buffer1));
		apr_setsockopt(itm_dgram_sock4, SOL_SOCKET, SO_SNDBUF, (char*)&buffer2, sizeof(buffer2));
	}

	// 绑定本地套接字
	if (apr_bind(itm_outer_sock4, (struct sockaddr*)&host_outer4, 0) ||
		apr_bind(itm_inner_sock4, (struct sockaddr*)&host_inner4, 0) ||
		apr_bind(itm_dgram_sock4, (struct sockaddr*)&host_dgram4, 0)) {
		itm_socket_release();
		return -3;
	}

	// 初始化 WIN32的 RESET修复
	if (apr_win32_init(itm_dgram_sock4)) {
		itm_socket_release();
		return -4;
	}

	// 数据流监听开始
	if (apr_listen(itm_outer_sock4, itm_backlog) || 
		apr_listen(itm_inner_sock4, itm_backlog)) {
		itm_socket_release();
		return -5;
	}

	apr_sockname(itm_outer_sock4, (struct sockaddr*)&host_outer4, NULL);
	apr_sockname(itm_inner_sock4, (struct sockaddr*)&host_inner4, NULL);
	apr_sockname(itm_dgram_sock4, (struct sockaddr*)&host_dgram4, NULL);

	itm_outer_port4 = htons(host_outer4.sin_port);
	itm_inner_port4 = htons(host_inner4.sin_port);
	itm_dgram_port4 = htons(host_dgram4.sin_port);

	itmd_outer4.fd = itm_outer_sock4;
	itmd_inner4.fd = itm_inner_sock4;
	itmd_dgram4.fd = itm_dgram_sock4;

	itmd_outer4.mode = ITMD_OUTER_HOST4;
	itmd_inner4.mode = ITMD_INNER_HOST4;
	itmd_dgram4.mode = ITMD_DGRAM_HOST4;
	itmd_dgram4.mask = 0;

#ifdef AF_INET6
	memset(&host_outer6, 0, sizeof(host_outer6));
	memset(&host_inner6, 0, sizeof(host_inner6));
	memset(&host_dgram6, 0, sizeof(host_dgram6));

	// 配置套接字监听地址
	memcpy(&host_inner6.sin6_addr.s6_addr, itm_inner_addr6, 16);
	host_outer6.sin6_port = htons((short)itm_outer_port6);
	host_inner6.sin6_port = htons((short)itm_inner_port6);
	host_dgram6.sin6_port = htons((short)itm_dgram_port6);
	host_outer6.sin6_family = AF_INET6;
	host_inner6.sin6_family = AF_INET6;
	host_dgram6.sin6_family = AF_INET6;

	// 如果 IPv6外部端口允许
	if (itm_outer_port6 >= 0) {
		unsigned long noblock4 = 1;
		unsigned long enable4 = 1;
		int size4 = sizeof(struct sockaddr_in6);

		itm_outer_sock6 = apr_socket(AF_INET6, SOCK_STREAM, 0);

		if (itm_outer_sock6 < 0) {
			itm_socket_release();
			return -10;
		}

	#if defined(IPPROTO_IPV6) && defined(IPV6_V6ONLY)
		apr_setsockopt(itm_outer_sock6, IPPROTO_IPV6, IPV6_V6ONLY,
			(const char*)&enable4, sizeof(enable4));
	#endif

		apr_ioctl(itm_outer_sock6, FIONBIO, &noblock4);

		apr_setsockopt(itm_outer_sock6, SOL_SOCKET, SO_REUSEADDR, 
			(char*)&reuseaddr, sizeof(reuseaddr));

	#ifdef SO_REUSEPORT
		apr_setsockopt(itm_outer_sock6, SOL_SOCKET, SO_REUSEPORT, 
			(char*)&reuseport, sizeof(reuseport));
	#endif

		apr_enable(itm_outer_sock6, APR_CLOEXEC);

		if (apr_bind(itm_outer_sock6, (struct sockaddr*)&host_outer6, 
			sizeof(host_outer6))) {
			itm_socket_release();
			return -12;
		}

		if (apr_listen(itm_outer_sock6, itm_backlog)) {
			itm_socket_release();
			return -13;
		}

		apr_sockname(itm_outer_sock6, (struct sockaddr*)&host_outer6, &size4);
		itm_outer_port6 = htons(host_outer6.sin6_port);
		enable4 = enable4;
	}

	// 如果 IPv6内部端口允许
	if (itm_inner_port6 >= 0) {
		unsigned long noblock5 = 1;
		unsigned long enable5 = 1;
		int size5 = sizeof(struct sockaddr_in6);

		itm_inner_sock6 = apr_socket(AF_INET6, SOCK_STREAM, 0);

		if (itm_inner_sock6 < 0) {
			itm_socket_release();
			return -14;
		}

	#if defined(IPPROTO_IPV6) && defined(IPV6_V6ONLY)
		apr_setsockopt(itm_inner_sock6, IPPROTO_IPV6, IPV6_V6ONLY,
			(const char*)&enable5, sizeof(enable5));
	#endif

		apr_ioctl(itm_inner_sock6, FIONBIO, &noblock5);

		apr_setsockopt(itm_inner_sock6, SOL_SOCKET, SO_REUSEADDR, 
				(char*)&reuseaddr, sizeof(reuseaddr));
	
	#ifdef SO_REUSEPORT
		apr_setsockopt(itm_inner_sock6, SOL_SOCKET, SO_REUSEPORT, 
				(char*)&reuseport, sizeof(reuseport));
	#endif

		apr_enable(itm_inner_sock6, APR_CLOEXEC);

		if (apr_bind(itm_inner_sock6, (struct sockaddr*)&host_inner6, 
			sizeof(host_inner6))) {
			itm_socket_release();
			return -15;
		}

		if (apr_listen(itm_inner_sock6, itm_backlog)) {
			itm_socket_release();
			return -16;
		}

		apr_sockname(itm_inner_sock6, (struct sockaddr*)&host_inner6, &size5);
		itm_inner_port6 = htons(host_inner6.sin6_port);
		enable5 = enable5;
	}

	// 如果 IPv6数据报端口允许
	if (itm_dgram_port6 >= 0) {
		unsigned long noblock6 = 1;
		unsigned long enable6 = 1;
		int size6 = sizeof(struct sockaddr_in6);

		itm_dgram_sock6 = apr_socket(AF_INET6, SOCK_DGRAM, 0);

		if (itm_dgram_sock6 < 0) {
			itm_socket_release();
			return -17;
		}

	#if defined(IPPROTO_IPV6) && defined(IPV6_V6ONLY)
		apr_setsockopt(itm_dgram_sock6, IPPROTO_IPV6, IPV6_V6ONLY,
			(const char*)&enable6, sizeof(enable6));
	#endif

		apr_ioctl(itm_dgram_sock6, FIONBIO, &noblock6);

		apr_setsockopt(itm_dgram_sock6, SOL_SOCKET, SO_REUSEADDR, 
			(char*)&reuseaddr, sizeof(reuseaddr));

	#ifdef SO_REUSEPORT
		apr_setsockopt(itm_dgram_sock6, SOL_SOCKET, SO_REUSEPORT, 
			(char*)&reuseport, sizeof(reuseport));
	#endif

		buffer1 = itm_dgram_blimit;
		buffer2 = itm_dgram_blimit;

		if (itm_dgram_blimit > 0) {
			apr_setsockopt(itm_dgram_sock6, SOL_SOCKET, SO_RCVBUF, 
				(char*)&buffer1, sizeof(buffer1));
			apr_setsockopt(itm_dgram_sock6, SOL_SOCKET, SO_SNDBUF,
				(char*)&buffer2, sizeof(buffer2));
		}

		apr_enable(itm_dgram_sock6, APR_CLOEXEC);

		if (apr_bind(itm_dgram_sock6, (struct sockaddr*)&host_dgram6, 
			sizeof(host_dgram6))) {
			itm_socket_release();
			return -18;
		}

		// 初始化 WIN32的 RESET修复
		if (apr_win32_init(itm_dgram_sock6)) {
			itm_socket_release();
			return -19;
		}

		apr_sockname(itm_dgram_sock6, (struct sockaddr*)&host_dgram6, &size6);
		itm_dgram_port6 = htons(host_dgram6.sin6_port);
		enable6 = enable6;
	}

	itmd_outer6.fd = itm_outer_sock6;
	itmd_inner6.fd = itm_inner_sock6;
	itmd_dgram6.fd = itm_dgram_sock6;

	itmd_outer6.mode = ITMD_OUTER_HOST6;
	itmd_inner6.mode = ITMD_INNER_HOST6;
	itmd_dgram6.mode = ITMD_DGRAM_HOST6;
	itmd_dgram6.mask = 0;

#endif

	#ifdef __unix
	signal(SIGPIPE, SIG_IGN);
	#endif
	
	reuseport = reuseport;

	return 0;
}

//---------------------------------------------------------------------
// itm_socket_release
//---------------------------------------------------------------------
static int itm_socket_release(void)
{
	if (itm_outer_sock4 >= 0) apr_close(itm_outer_sock4);
	if (itm_inner_sock4 >= 0) apr_close(itm_inner_sock4);
	if (itm_dgram_sock4 >= 0) apr_close(itm_dgram_sock4);
	itm_outer_sock4 = -1;
	itm_inner_sock4 = -1;
	itm_dgram_sock4 = -1;

	if (itm_outer_sock6 >= 0) apr_close(itm_outer_sock6);
	if (itm_inner_sock6 >= 0) apr_close(itm_inner_sock6);
	if (itm_dgram_sock6 >= 0) apr_close(itm_dgram_sock6);
	itm_outer_sock6 = -1;
	itm_inner_sock6 = -1;
	itm_dgram_sock6 = -1;

	return 0;
}

//---------------------------------------------------------------------
// 时钟例程
//---------------------------------------------------------------------
static int itm_timer_routine(void)
{
	itm_time_current = apr_timex() / 1000;

	if (itm_time_current < itm_time_slap || 
		itm_time_current > itm_time_slap + 1000L * 3600)
		itm_time_slap = itm_time_current;

	for (; itm_time_current > itm_time_slap; ) {
		itm_time_slap += ITMD_TIME_CYCLE;
		itm_timer();
	}

	if (itm_interval > 0 && itm_notice_cycle > 0) {
		if (itm_notice_slap > itm_time_current)
			apr_sleep((long)(itm_notice_slap - itm_time_current));
	}

	return 0;
}

//---------------------------------------------------------------------
// 核心事件驱动部分：
// 所有网络事件在这里将得到处理，首先利用POLL模块中的POLL方法以适合于
// 平台的设备进行网络事件捕捉，其次根据捕捉到的事件，分析并分发给对应
// 的处理函数进行相应的处理，最后进行超时控制
//---------------------------------------------------------------------
int itm_process(long timeval)
{
	int retval, fd, event, i;
	struct ITMD *itmd;
	void *userdata;

	if (itm_mode == 0) return -1;
	
	// 超时控制部分
	itm_timer_routine();

	// 网络消息捕捉
	retval = apr_poll_wait(itm_polld, timeval);

	if (retval < 0) return -2;

	// 处理网络事件
	itm_event_count = 0;
	for (i = retval * 2; i >= 0; i--) {
		if (apr_poll_event(itm_polld, &fd, &event, &userdata)) break;
		itmd = (struct ITMD*)userdata;
		itm_event_count++;

		if (itm_logmask & ITML_EVENT) {
			if (itmd == NULL) {
				itm_log(ITML_EVENT, 
					"[EVENT] fd=%d event=%d itmd=%xh", fd, event, itmd);
			}	else {
				itm_log(ITML_EVENT, 
					"[EVENT] fd=%d event=%d itmd(mode=%d hid=%XH channel=%d)", 
					fd, event, itmd->mode, itmd->hid, itmd->channel);
			}
		}
		if (itmd == NULL) {		
			itm_log(ITML_WARNING, "[WARNING] none event captured");
			continue;		
		}

		if (itmd->mode < 0) {
			itm_log(ITML_ERROR, 
				"[ERROR] capture a error event fd=%d event=%d for a closed descriptor", 
				fd, event);
			continue;
		}

		// 输入：当描述符发生输入事件
		if (event & (APOLL_IN | APOLL_ERR)) {	
			switch (itmd->mode)
			{
			case ITMD_OUTER_HOST4:
				itm_event_accept(ITMD_OUTER_HOST4);
				break;

			case ITMD_INNER_HOST4:
				itm_event_accept(ITMD_INNER_HOST4);
				break;
			
			case ITMD_OUTER_HOST6:
				itm_event_accept(ITMD_OUTER_HOST6);
				break;
			
			case ITMD_INNER_HOST6:
				itm_event_accept(ITMD_INNER_HOST6);
				break;

			case ITMD_OUTER_CLIENT:
				itm_event_recv(itmd);
				if (itmd->rstream.size >= itm_outer_blimit) {
					itm_log(ITML_INFO, 
						"buffer limited for fd=%d itmd(mode=%d hid=%XH channel=%d)", 
						fd, itmd->mode, itmd->hid, itmd->channel);
					itm_event_close(itmd, 2101);
				}
				break;

			case ITMD_INNER_CLIENT:
				itm_event_recv(itmd);
				if (itmd->rstream.size >= itm_inner_blimit) {
					itm_log(ITML_INFO, 
						"buffer limited for fd=%d itmd(mode=%d hid=%XH channel=%d)", 
						fd, itmd->mode, itmd->hid, itmd->channel);
					itm_event_close(itmd, 2101);
				}
				break;

			case ITMD_DGRAM_HOST4:
				itm_event_dgram(AF_INET);
				break;
			
			case ITMD_DGRAM_HOST6:
			#ifdef AF_INET6
				itm_event_dgram(AF_INET6);
			#endif
				break;
			}
		}

		// 如果输入处理过程关闭了该连接
		if (itmd->mode < 0) continue;	

		// 输出：当描述符发生输出事件
		if (event & APOLL_OUT) {		
			itm_event_send(itmd);
		}
	}
	
	// 处理发送列表：对有待发送数据的连接尝试发送
	for (i = 0; i < itm_wsize; i++) {
		itmd = itm_hid_itmd(itm_wlist[i]);
		if (itmd == NULL) continue;
		itmd->inwlist = 0;
		itm_event_send(itmd);
	}

	// 清空发送列表
	itm_wsize = 0;

	return 0;
}

//---------------------------------------------------------------------
// itm_timer
//---------------------------------------------------------------------
int itm_timer(void)
{
	apr_int64 notice_time, v;
	static long timesave = 0;
	static apr_int64 ticksave = 0;
	struct ITMD *itmd, *channel;
	long current, hid;
	apr_int64 ticknow;
	int timeid, i, k;

	// 处理频道时钟
	if (itm_notice_cycle > 0) {
		for (; itm_notice_slap <= itm_time_current; ) {
			itm_notice_slap += itm_notice_cycle;
			itm_notice_count++;
		}
	}	else {
		itm_notice_count = 0;
	}

	itmd = itm_rchannel(0);
	
	// 发送频道时钟信号到频道 0 以及订阅255分类的频道
	if (itm_notice_count > 0) {
		notice_time = itm_notice_slap - itm_notice_saved;
		v = notice_time - itm_notice_cycle * itm_notice_count;
		for (; itm_notice_count > 0; v += itm_notice_cycle) {
			i = (int)(v % 1000);
			itm_param_set(0, itm_headlen, ITMT_TIMER, (int)(v - i), i);
			if (itmd != NULL) {
				if (itmd->wstream.size < itm_inner_blimit) {
					itm_send(itmd, itm_data, itm_headlen);
				}
			}
			if (itm_booklen[255] > 0) {
				for (k = itm_booklen[255] - 1; k >= 0; k--) {
					int chid = itm_book[255][k];
					channel = itm_rchannel(chid);
					if (channel && chid != 0) {
						if (channel->wstream.size < itm_inner_blimit) {
							itm_send(channel, itm_data, itm_headlen);
						}
					}
				}
			}
			itm_notice_count--;
		}
	}

	// 计算十分之一秒的事件
	ticknow = itm_time_current / 100;

	// 如果没有到十分之一秒就退出
	if (ticknow == ticksave) return 0;

	// 查询是否有消息
	while (itm_msg_cnt0 > 0) {
		long hr;
		hr = itm_msg_get(0, itm_data + itm_headlen, itm_datamax);
		if (hr < 0) break;
		if (itmd != NULL) {
			itm_param_set(0, itm_headlen + hr, ITMT_SYSCD, ITMS_MESSAGE, 0);
			if (itmd->wstream.size < itm_inner_blimit) {
				itm_send(itmd, itm_data, itm_headlen + hr);
			}
		}
	}

	// 计算以秒为单位的运行时间
	current = (long)time(NULL);

	// 如果没有到一秒钟
	if (current == timesave) return 0;
	timesave = current;

	// 世界时钟增加
	itm_wtime++;

	// 以下是当秒发生变化的时候

	itmd = itm_rchannel(0);
	if (itmd) {
		if (itmd->wstream.size < itm_inner_blimit) itm_permitr(itmd);
	}

	idt_settime(&itm_timeu, itm_wtime);
	idt_settime(&itm_timec, itm_wtime);

	// 处理超时的外部连接
	for (i = itm_timeu.pnodes.node_max * 2; i >= 0; i--) {
		if (idt_timeout(&itm_timeu, &timeid, &hid)) break;
		itmd = itm_hid_itmd(hid);
		if (itmd) {
			itm_log(ITML_INFO, "client connection timeout:");
			itmd->timeid = -1;
			itm_event_close(itmd, 2200);
		}	else {
			itm_log(ITML_ERROR, 
				"[ERROR] client timeout calculate error for timeid = %d", timeid);
		}
	}

	// 处理超时的内部连接
	for (i = itm_timec.pnodes.node_max * 2; i >= 0; i--) {
		if (idt_timeout(&itm_timec, &timeid, &hid)) break;
		itmd = itm_hid_itmd(hid);
		if (itmd) {
			itm_log(ITML_INFO, "channel connection timeout:");
			itmd->timeid = -1;
			itm_event_close(itmd, 2200);
		}	else {
			itm_log(ITML_ERROR, 
				"[ERROR] channel timeout calculate error for timeid = %d", timeid);
		}
	}

	return 0;
}

//=====================================================================
// Network Operation
//=====================================================================
char itm_zdata[ITM_BUFSIZE];

#ifdef __unix
#define aprerrno errno
#else
#define aprerrno ((int)WSAGetLastError())
#endif

//---------------------------------------------------------------------
// itm_trysend
//---------------------------------------------------------------------
long itm_trysend(struct ITMD *itmd)
{
	struct IMSTREAM *stream = &itmd->wstream;
	long len, total = 0, ret = 3;
	int fd = itmd->fd;
	void*lptr;

	assert(stream && fd >= 0);
	if (stream->size == 0) return 0;
	for (ret = 1; ret > 0; ) {
		itm_error = 0;
		len = ims_rptr(stream, &lptr); 
		if (len == 0) break;
		ret = send(fd, (const char*)lptr, len, 0);
		if (ret == 0) break;
		else if (ret < 0) {
			itm_error = aprerrno;
			ret = (itm_error == IEAGAIN || itm_error == 0)? 0 : -1;
		}
		if (ret <= 0) break;
		total += ret;
		ims_drop(stream, ret);
	}

	return (ret < 0)? ret : total;
}


//---------------------------------------------------------------------
// itm_tryrecv
//---------------------------------------------------------------------
long itm_tryrecv(struct ITMD *itmd)
{
	struct IMSTREAM *stream = &itmd->rstream;
	long total = 0, ret = 3, val = 0;
	int fd = itmd->fd;

	assert(stream && fd >= 0);

	for (ret = 1; ret > 0; ) {
		itm_error = 0;
		ret = recv(fd, itm_zdata, ITM_BUFSIZE, 0);
		if (ret == 0) ret = -1;
		else if (ret < 0) {
			itm_error = aprerrno;
			ret = (itm_error == IEAGAIN || itm_error == 0)? 0 : -1;
		}
		if (ret <= 0) break;
		
		#ifndef IDISABLE_RC4
		// 判断是否加密
		if (itmd->rc4_recv_x >= 0 && itmd->rc4_recv_y >= 0) {
			itm_rc4_crypt(itmd->rc4_recv_box, &itmd->rc4_recv_x, &itmd->rc4_recv_y,
				(const unsigned char*)itm_zdata, (unsigned char*)itm_zdata, ret);
		}
		#endif

		if (itm_headmod != ITMH_LINESPLIT || itmd->mode != ITMD_OUTER_CLIENT) {
			val = ims_write(stream, itm_zdata, ret);
			assert(val == ret);
		}	else {
			struct IMSTREAM *lines = &itmd->lstream;
			long start = 0, pos = 0;
			char head[4];
			for (start = 0, pos = 0; pos < ret; pos++) {
				if (itm_zdata[pos] == '\n') {
					long x = pos - start + 1;
					long y = lines->size;
					iencode32u_lsb(head, x + y + 4);
					ims_write(stream, head, 4);
					while (lines->size > 0) {
						long csize;
						void *ptr;
						csize = ims_rptr(lines, &ptr);
						ims_write(stream, ptr, csize);
						ims_drop(lines, csize);
					}
					ims_write(stream, &itm_zdata[start], x);
					start = pos + 1;
				}
			}
			if (pos > start) {
				ims_write(lines, &itm_zdata[start], pos - start);
			}
			val = ret;
		}

		total += val;
		if (itm_fastmode != 0 && ret < ITM_BUFSIZE) break;
	}
	return (ret < 0)? ret : total;
}


//---------------------------------------------------------------------
// itm_trysendto
//---------------------------------------------------------------------
long itm_trysendto(int af)
{
	struct ITMHUDP *head;
	struct sockaddr*remote;
	unsigned short dsize;
	unsigned short xsize;
	long total = 0;

#ifdef AF_INET6
	if (af == AF_INET6) {
		for (xsize = 1; xsize > 0; ) {
			if (itm_dgramdat6.size < 2) break;
			ims_peek(&itm_dgramdat6, &dsize, 2);
			ims_peek(&itm_dgramdat6, itm_zdata, dsize);

			remote = (struct sockaddr*)(itm_zdata + 2);
			head = (struct ITMHUDP*)(itm_zdata + ITM_ADDRSIZE6 + 2);
			xsize = dsize - ITM_ADDRSIZE6 - 2;
			xsize = apr_sendto(itm_dgram_sock6, (char*)head, xsize, 0, remote, ITM_ADDRSIZE6);
			if (xsize > 0) {
				ims_drop(&itm_dgramdat6, dsize);
				total += xsize;
			}
		}
		return total;
	}
#endif

	for (xsize = 1; xsize > 0; ) {
		if (itm_dgramdat4.size < 2) break;
		ims_peek(&itm_dgramdat4, &dsize, 2);
		ims_peek(&itm_dgramdat4, itm_zdata, dsize);

		remote = (struct sockaddr*)(itm_zdata + 2);
		head = (struct ITMHUDP*)(itm_zdata + ITM_ADDRSIZE4 + 2);
		xsize = dsize - ITM_ADDRSIZE4 - 2;
		xsize = sendto(itm_dgram_sock4, (char*)head, xsize, 0, remote, ITM_ADDRSIZE4);

		if (xsize > 0) {
			ims_drop(&itm_dgramdat4, dsize);
			total += xsize;
		}
	}

	return total;
}


//---------------------------------------------------------------------
// itm_hostw
//---------------------------------------------------------------------
int itm_wchannel(int index, struct ITMD *itmd)
{
	int retval, n, i;
	if (itm_host == NULL) itm_hostc = -1;
	if (index >= itm_hostc) {
		n = (itm_hostc >= 0)? itm_hostc : 0;
		itm_hostc = index + 1;
		retval = iv_resize(&itm_hostv, itm_hostc * sizeof(struct ITMD*));
		if (retval) return -1;
		itm_host = (struct ITMD**)itm_hostv.data;
		for (i = n; i < itm_hostc; i++) itm_host[i] = NULL;
	}
	itm_host[index] = itmd;
	return 0;
}

//---------------------------------------------------------------------
// itm_dsize
//---------------------------------------------------------------------
long itm_dsize(long length)
{
	long size = length;
	length = (size << 1) + size + 8192;
	if (length >= (long)itm_datav.length) {
		iv_resize(&itm_datav, length);
		itm_data = (char*)itm_datav.data;
	}
	itm_crypt = itm_data + size;
	itm_document = itm_crypt + size;
	return 0;
}

#ifdef _MSC_VER
#pragma warning(disable:4996)
#endif

//---------------------------------------------------------------------
// itm_epname
//---------------------------------------------------------------------
char *itm_epname4(const struct sockaddr *ep)
{
	static char text[1024];
	struct sockaddr_in *addr = NULL;
	unsigned char *bytes;
	int ipb[5], i;
	addr = (struct sockaddr_in*)ep;
	bytes = (unsigned char*)&(addr->sin_addr.s_addr);
	for (i = 0; i < 4; i++) ipb[i] = (int) bytes[i];
	ipb[4] = (int)(htons(addr->sin_port));
	sprintf(text, "%d.%d.%d.%d:%d", ipb[0], ipb[1], ipb[2], ipb[3], ipb[4]);
	return text;
}

//---------------------------------------------------------------------
// itm_epname
//---------------------------------------------------------------------
char *itm_epname6(const struct sockaddr *ep)
{
	static char text[1024] = "(null)";
#ifdef AF_INET6
	struct sockaddr_in6 *addr = NULL;
	static char desc[1024] = "";
	int port;
	addr = (struct sockaddr_in6*)ep;
	apr_ntop(AF_INET6, &addr->sin6_addr.s6_addr, desc, 1024);
	port = (int)(htons(addr->sin6_port));
	sprintf(text, "%s:%d", desc, port);
#endif
	return text;
}


//---------------------------------------------------------------------
// itm_epname
//---------------------------------------------------------------------
char *itm_epname(const struct ITMD *itmd)
{
	if (itmd->IsIPv6 == 0) {
		return itm_epname4((const struct sockaddr*)&itmd->remote4);
	}	else {
	#ifdef AF_INET6
		return itm_epname6((const struct sockaddr*)&itmd->remote6);
	#else
		static char text[64] = "(unknow ipv6 addr)";
		return text;
	#endif
	}
}


//---------------------------------------------------------------------
// itm_ntop
//---------------------------------------------------------------------
char *itm_ntop(int af, const struct sockaddr *remote)
{
#ifdef AF_INET6
	if (af == AF_INET6) 
		return itm_epname6(remote);
#endif
	return itm_epname4(remote);
}


//---------------------------------------------------------------------
// itm_dataok
//---------------------------------------------------------------------
int itm_mask(struct ITMD *itmd, int enable, int disable)
{
	if (itmd == NULL) return -1;
	if (disable & ITM_READ) itmd->mask &= ~(APOLL_IN);
	if (disable & ITM_WRITE) itmd->mask &= ~(APOLL_OUT);
	if (enable & ITM_READ) itmd->mask |= APOLL_IN;
	if (enable & ITM_WRITE) itmd->mask |= APOLL_OUT;

	return apr_poll_set(itm_polld, itmd->fd, itmd->mask);
}

//---------------------------------------------------------------------
// itm_send
//---------------------------------------------------------------------
int itm_send(struct ITMD *itmd, const void *data, long length)
{
	assert(itmd);
	assert(itmd->mode >= 0);

	// 写入流
	ims_write(&itmd->wstream, data, length); 

	if (itm_fastmode == 0) {			// 设置发送事件
		if ((itmd->mask & APOLL_OUT) == 0) 
			itm_mask(itmd, ITM_WRITE, 0); 
	}	else {							// 添加到发送列表
		itm_wlist_record(itmd->hid);
	}

	// 增加计数值
	itmd->cnt_tcpw = (itmd->cnt_tcpw + 1) & 0x7fffffff;

	return 0;
}

//---------------------------------------------------------------------
// itm_sendudp
//---------------------------------------------------------------------
int itm_sendudp(int af, struct sockaddr *remote, struct ITMHUDP *head, const void *data, long size)
{
	unsigned dsize = (unsigned short)size + ITM_ADDRSIZE4 + 2;
	char headnew[16];

#ifdef AF_INET6
	if (af == AF_INET6) {
		dsize = (unsigned short)size + ITM_ADDRSIZE6 + 2;
		if (head != NULL) dsize = dsize + 16;

		ims_write(&itm_dgramdat6, &dsize, 2);
		ims_write(&itm_dgramdat6, remote, ITM_ADDRSIZE6);

		if (head != NULL) {
			iencode32u_lsb(headnew +  0, head->order);
			iencode32u_lsb(headnew +  4, head->index);
			iencode32u_lsb(headnew +  8, (apr_uint32)head->hid);
			iencode32u_lsb(headnew + 12, (apr_uint32)head->session);
			ims_write(&itm_dgramdat6, headnew, 16);
		}

		ims_write(&itm_dgramdat6, data, size);

		if ((itmd_dgram6.mask & APOLL_OUT) == 0) itm_mask(&itmd_dgram6, ITM_WRITE, 0); 
		return 0;
	}
#endif

	if (head != NULL) dsize = dsize + 16;

	ims_write(&itm_dgramdat4, &dsize, 2);
	ims_write(&itm_dgramdat4, remote, ITM_ADDRSIZE4);

	if (head != NULL) {
		iencode32u_lsb(headnew +  0, head->order);
		iencode32u_lsb(headnew +  4, head->index);
		iencode32u_lsb(headnew +  8, (apr_uint32)head->hid);
		iencode32u_lsb(headnew + 12, (apr_uint32)head->session);
		ims_write(&itm_dgramdat4, headnew, 16);
	}

	ims_write(&itm_dgramdat4, data, size);

	if ((itmd_dgram4.mask & APOLL_OUT) == 0) itm_mask(&itmd_dgram4, ITM_WRITE, 0); 

	return 0;
}


//---------------------------------------------------------------------
// itm_sendto
//---------------------------------------------------------------------
int itm_sendto(struct ITMD *itmd, const void *data, long length)
{
	struct ITMHUDP head;

	assert(itmd);
	assert(itmd->mode == ITMD_OUTER_CLIENT);

	head.order = itmd->cnt_udpw;
	head.index = itmd->cnt_tcpw;
	head.hid = itmd->hid;
	head.session = itmd->session;

	itmd->cnt_udpw = (itmd->cnt_udpw + 1) & 0x7fffffff;

	if (itmd->IsIPv6 == 0) {
		itm_sendudp(AF_INET, (struct sockaddr*)&(itmd->dgramp4),
			&head, data, length);
	}	else {
	#ifdef AF_INET6
		itm_sendudp(AF_INET6, (struct sockaddr*)&(itmd->dgramp6), 
			&head, data, length);
	#endif
	}

	return 0;
}


//---------------------------------------------------------------------
// itm_bcheck
//---------------------------------------------------------------------
int itm_bcheck(struct ITMD *itmd)
{
	int retval = 0;
	if (itmd->mode == ITMD_INNER_CLIENT) {
		if (itmd->wstream.size >= itm_limit) {
			retval = -1;
			itm_log(ITML_WARNING, 
				"[WARNING] channel buffer limit riched %d: hid=%XH channel=%d", 
				itm_limit, itmd->hid, itmd->channel, itmd->mode);
		}
	}	else if (itmd->mode == ITMD_OUTER_CLIENT) {
		if (itmd->wstream.size >= itm_outer_blimit) {
			retval = -1;
			itm_log(ITML_WARNING, 
				"[WARNING] user buffer limit riched %d: hid=%XH channel=%d", 
				itm_outer_blimit, itmd->hid, itmd->channel, itmd->mode);
		}
		if (itmd->wstream.size > itm_limit) {
			retval = -2;
			itm_log(ITML_WARNING, 
				"[WARNING] user buffer limit riched %d: hid=%XH channel=%d", 
				itm_limit, itmd->hid, itmd->channel, itmd->mode);
		}
	}
	if (retval) itm_event_close(itmd, 2102);
	return retval;
}


//---------------------------------------------------------------------
// itm_permitr
//---------------------------------------------------------------------
int itm_permitr(struct ITMD *itmd)
{
	struct IVQNODE *wnode;
	struct ITMD *client;
	wnode = iv_headpop(&itmd->waitq);
	if (wnode == NULL) return 0;
	client = (struct ITMD*)wnode->udata;
	if (client->disable == 0) {
		itm_mask(client, ITM_READ, 0);
	}
	if (itm_logmask & ITML_DATA) {
		itm_log(ITML_DATA, "channel %d permit read hid=%XH channel=%d", 
			itmd->channel, client->hid, client->channel);
	}
	return 0;
}

//---------------------------------------------------------------------
// itm_wlist_record - 记录到发送列表
//---------------------------------------------------------------------
int itm_wlist_record(long hid)
{
	static struct IVECTOR wlist;
	static int inited = 0;
	struct ITMD *itmd;
	int retval;

	if (inited == 0) {
		iv_init(&wlist, NULL);
		retval = iv_resize(&wlist, 8);
		assert(retval == 0);
		itm_wlist = (long*)wlist.data;
		itm_wsize = 0;
		inited = 1;
	}

	if ((itm_wsize * sizeof(long)) >= (unsigned long)wlist.length) {
		retval = iv_resize(&wlist, wlist.length * 2);
		assert(retval == 0);
		itm_wlist = (long*)wlist.data;
	}

	itmd = itm_hid_itmd(hid);

	if (itmd == NULL) return -1;
	if (itmd->inwlist) return 1;

	itmd->inwlist = 1;
	itm_wlist[itm_wsize++] = hid;

	return 0;
}


//---------------------------------------------------------------------
// itm_book_add
//---------------------------------------------------------------------
int itm_book_add(int category, int channel)
{
	isize_t newsize;
	short *book;
	int booklen, i;

	if (category < 0 || category > 511) 
		return -1;

	newsize = (itm_booklen[category] + 1) * sizeof(short);

	if (newsize > itm_bookv[category].length) {
		if (iv_resize(&itm_bookv[category], newsize) != 0) {
			return -3;
		}
		itm_book[category] = (short*)itm_bookv[category].data;
	}

	book = itm_book[category];
	booklen = itm_booklen[category];

	for (i = 0; i < booklen; i++) {
		if (book[i] == channel) 
			return -4;
	}

	itm_book[category][booklen] = channel;
	itm_booklen[category]++;

	return 0;
}


//---------------------------------------------------------------------
// itm_book_del
//---------------------------------------------------------------------
int itm_book_del(int category, int channel)
{
	short *book;
	int booklen;
	int pos, i;

	if (category < 0 || category > 511);
		return -1;

	book = itm_book[category];
	booklen = itm_booklen[category];

	if (booklen <= 0) 
		return -3;

	for (i = 0, pos = -1; i < booklen; i++) {
		if (book[i] == channel) {
			pos = i;
			break;
		}
	}

	if (pos < 0) 
		return -4;

	book[pos] = book[booklen - 1];
	itm_booklen[category]--;

	return 0;
}

//---------------------------------------------------------------------
// itm_book_reset
//---------------------------------------------------------------------
int itm_book_reset(int channel)
{
	int i;

	if (channel < 0) 
		return -1;

	for (i = 0; i < 512; i++) {
		itm_book_del(i, channel);
	}

	return 0;
}

//---------------------------------------------------------------------
// itm_book_empty
//---------------------------------------------------------------------
int itm_book_empty(void)
{
	int i;
	for (i = 0; i < 512; i++) {
		itm_booklen[i] = 0;
	}
	return 0;
}


//---------------------------------------------------------------------
// RC4: 初始化
//---------------------------------------------------------------------
void itm_rc4_init(unsigned char *box, int *x, int *y, 
	const unsigned char *key, int keylen)
{
	int X, Y, i, j, k, a;
	if (keylen <= 0 || key == NULL) {
		X = -1;
		Y = -1;
	}	else {
		X = Y = 0;
		j = k = 0;
		for (i = 0; i < 256; i++) {
			box[i] = (unsigned char)i;
		}
		for (i = 0; i < 256; i++) {
			a = box[i];
			j = (unsigned char)(j + a + key[k]);
			box[i] = box[j];
			box[j] = a;
			if (++k >= keylen) k = 0;
		}
	}
	x[0] = X;
	y[0] = Y;
}

//---------------------------------------------------------------------
// RC4: 加密/解密
//---------------------------------------------------------------------
void itm_rc4_crypt(unsigned char *box, int *x, int *y,
	const unsigned char *src, unsigned char *dst, long size)
{
	int X = x[0];
	int Y = y[0];
	if (X < 0 || Y < 0) {			// 不加密的情况
		if (src != dst) {
			memmove(dst, src, size);
		}
	}
	else {							// 加密的情况
		int a, b; 
		for (; size > 0; src++, dst++, size--) {
			X = (unsigned char)(X + 1);
			a = box[X];
			Y = (unsigned char)(Y + a);
			box[X] = box[Y];
			b = box[Y];
			box[Y] = a;
			dst[0] = src[0] ^ box[(unsigned char)(a + b)];
		}
		x[0] = X;
		y[0] = Y;
	}
}


//---------------------------------------------------------------------
// message - put
//---------------------------------------------------------------------
long itm_msg_put(int id, const char *data, long size)
{
	struct IMSTREAM *stream;
	char head[8];
	long hr = 0;
	if (itm_mode == 0) return -1;
	iencode32u_lsb(head, (apr_uint32)(size + 4));
	if (size >= itm_datamax) return -1;
	apr_mutex_lock(itm_msg_lock);
	stream = (id == 0)? &itm_msg_n0 : &itm_msg_n1;
	if (stream->size >= (itm_datamax << 2)) hr = -2;
	else {
		ims_write(stream, head, 4);
		ims_write(stream, data, size);
		if (id == 0) {
			itm_msg_cnt0++;
		}	else {
			itm_msg_cnt1++;
		}
	}
	apr_mutex_unlock(itm_msg_lock);
	return hr;
}


//---------------------------------------------------------------------
// message - get
//---------------------------------------------------------------------
long itm_msg_get(int id, char *data, long maxsize) 
{
	struct IMSTREAM *stream;
	apr_uint32 size;
	char head[8];
	long hr = -1;
	if (itm_mode == 0) return -1;
	if (id == 0 && itm_msg_cnt0 == 0) return -1;
	if (id != 0 && itm_msg_cnt1 == 0) return -1;
	apr_mutex_lock(itm_msg_lock);
	stream = (id == 0)? &itm_msg_n0 : &itm_msg_n1;
	if (ims_peek(stream, head, 4) == 4) {
		idecode32u_lsb(head, &size);
		if (maxsize < 0) maxsize = 0;
		if (stream->size >= (ilong)size) {
			long length = (long)size - 4;
			long need = (length > maxsize)? maxsize : length;
			long drop = length - need;
			ims_drop(stream, 4);
			if (data) {
				ims_read(stream, data, need);
			}	else {
				ims_drop(stream, need);
			}
			if (drop > 0) {
				ims_drop(stream, drop);
			}
			hr = length;
			if (id == 0) {
				itm_msg_cnt0--;
			}	else {
				itm_msg_cnt1--;
			}
		}
	}
	apr_mutex_unlock(itm_msg_lock);
	return hr;
}


//---------------------------------------------------------------------
// utils
//---------------------------------------------------------------------
int (*itm_logv)(const char *text) = NULL;

int itm_log(int mask, const char *argv, ...)
{
	va_list argptr;	

	if ((mask & itm_logmask) == 0) return 0;

	va_start(argptr, argv);
	vsprintf(itm_msg, argv, argptr);
	va_end(argptr);	

	if (itm_logv) itm_logv(itm_msg);
	else printf("%s\n", itm_msg);

	return 0;
}

void itm_lltoa(char *dst, apr_int64 x)
{
	char *left, *right, *p, c;
	if (x == 0) {
		dst[0] = '0';
		dst[1] = 0;
		return;
	}
	if (x < 0) {
		*dst++ = '-';
		x = -x;
	}
	for (p = dst, left = dst, right = dst; x > 0; ) { 
		right = p;
		*p++ = '0' + (int)(x % 10); 
		x /= 10; 
	}
	*p++ = '\0';
	for (; left < right; left++, right--) {
		c = *left;
		*left = *right;
		*right = c;
	}
}


