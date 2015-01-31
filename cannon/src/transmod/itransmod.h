//=====================================================================
//
// TML <Transmod Library>, by skywind 2004, itransmod.h
//
// HISTORY:
// Apr. 27 2010   skywind  fixed: when sys-timer changed, maybe error
// Mar. 15 2011   skywind  64bit support, header size configurable
// Jun. 25 2011   skywind  implement channel subscribe
// Sep. 09 2011   skywind  new: socket buf resize, congestion ctrl.
// Nov. 30 2011   skywind  new: channel broadcasting (v2.40)
// Dec. 23 2011   skywind  new: rc4 crypt (v2.43)
// Dec. 28 2011   skywind  rc4 enchance (v2.44)
// Mar. 03 2012   skywind  raw data header (v2.45)
// Dec. 01 2013   skywind  solaris /dev/poll supported (v2.64)
// Apr. 01 2014   skywind  new ITMH_LINESPLIT (v2.65)
//
// NOTES： 
// 网络传输库 TML<传输模块>，建立 客户/频道的通信模式，提供基于多频道
// multi-channel通信的 TCP/UDP通信机制，缓存/内存管理，超时控制等机制
// 的服务接口，以低系统占用完成即时的万人级通信任务
// 
//=====================================================================

#ifndef __I_TRANSMOD_H__
#define __I_TRANSMOD_H__

#include "aprsys.h"			// 独立平台系统调用模块
#include "aprsock.h"		// 独立平台套接字封装模块
#include "aprpoll.h"		// 独立平台POLL封装模块

#include "icvector.h"		// 通用数据数列模块
#include "impoold.h"		// 通用内存管理模块
#include "imstream.h"		// 通用缓存管理模块
#include "idtimeout.h"		// 超时控制模块
#include "icqueue.h"		// 等待队列控制模块

#ifdef __unix
#include <netinet/tcp.h>	// 额外增加的头文件
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define ITMV_VERSION 0x268	// 传输模块版本号

//=====================================================================
// Global Variables Definition
//=====================================================================

extern int itm_outer_port4;	// IPv4 对外监听端口
extern int itm_inner_port4;	// IPv4 对内监听端口
extern int itm_dgram_port4;	// IPv4 数据报端口
extern int itm_outer_port6;	// IPv6 对外监听端口 
extern int itm_inner_port6;	// IPv6 对内监听端口
extern int itm_dgram_port6;	// IPv6 数据报端口
extern int itm_outer_sock4;	// 对外监听套接字
extern int itm_inner_sock4;	// 对内监听套接字
extern int itm_dgram_sock4;	// 数据报套接字
extern int itm_outer_sock6;	// IPv6 对外监听套接字
extern int itm_inner_sock6;	// IPv6 对内监听套接字
extern int itm_dgram_sock6;	// IPv6 数据报套接字
extern int itm_outer_max;	// 对外最大连接
extern int itm_inner_max;	// 对内最大连接
extern int itm_outer_cnt;	// 对外当前连接
extern int itm_inner_cnt;	// 对内当前连接
extern apolld itm_polld;	// 提供给APR_POLL的事件捕捉描述字

extern int itm_state;		// 当前状态
extern int itm_error;		// 错误代码
extern int itm_psize;		// 内存页面大小
extern int itm_backlog;		// 最大监听的backlog
extern int itm_counter;		// 计算HID的增量
extern int itm_udpmask;		// 数据报掩码

extern int itm_headmod;		// 头部模式
extern int itm_headint;		// 头部类型
extern int itm_headinc;		// 头部增量
extern int itm_headlen;		// 头部长度
extern int itm_headmsk;		// 头部掩码
extern int itm_hdrsize;		// 长度的长度

extern long itm_outer_time;	// 外部连接生命时间
extern long itm_inner_time;	// 内部连接生命时间
extern long itm_wtime;		// 世界时钟
extern long itm_datamax;	// 最长的数据
extern long itm_limit;		// 发送缓存超过就断开

extern long itm_logmask;		// 日志级别，0 为不输出日志
extern char itm_msg[];			// 消息字符串
extern long itm_inner_addr4;	// 内部监听绑定的IP
extern char itm_inner_addr6[];	// 内部监听绑定的 IPv6地址

extern long itm_outer_blimit;	// 外部套接字缓存极限
extern long itm_inner_blimit;	// 内部频道缓存极限
extern long itm_dgram_blimit;	// 数据报套接字缓存大小
extern long itm_socksndi;		// 内部套接字发送缓存大小
extern long itm_sockrcvi;		// 内部套接字接受缓存大小
extern long itm_socksndo;		// 外部套接字发送缓存大小
extern long itm_sockrcvo;		// 外部套接字发送缓存大小

extern long  itm_hostc;			// 内部Channel数量
extern long  itm_datac;			// 内部数据长度

extern apr_int64 itm_time_start;	// 启动的时间
extern apr_int64 itm_time_current;	// 当前的时间
extern apr_int64 itm_time_slap;		// 时钟脉冲时间

extern apr_int64 itm_notice_slap;	// 频道时钟信号定义
extern apr_int64 itm_notice_cycle;	// 频道时钟周期
extern apr_int64 itm_notice_count;	// 频道始终信号记时
extern apr_int64 itm_notice_saved;	// 频道记时开始

extern long itm_dropped;			// 丢弃的不合理数据报
extern long itm_utiming;			// 客户端计时模式

extern long itm_interval;			// 时间间隔


#define ITMD_TIME_CYCLE		10		// 基础时钟定义

extern long itm_fastmode;			// 是否启用写列表
extern long itm_httpskip;			// 是否跳过类 http头部

extern int itm_dhcp_index;			// 频道分配索引
extern int itm_dhcp_base;			// 频道分配基址
extern int itm_dhcp_high;			// 频道分配限制

extern apr_int64 itm_stat_send;			// 统计：发送了多少包
extern apr_int64 itm_stat_recv;			// 统计：接收了多少包
extern apr_int64 itm_stat_discard;		// 统计：放弃了多少个数据包

extern int itm_reuseaddr;			// 地址复用：0自动，1允许 2禁止
extern int itm_reuseport;			// 端口复用：0自动，1允许 2禁止	


//=====================================================================
// ITM Connection Description Definition
//=====================================================================
#define ITMD_OUTER_HOST4	0	// 套接字模式：IPv4 外部监听的套接字
#define ITMD_INNER_HOST4	1	// 套接字模式：IPv4 内部监听的套接字
#define ITMD_OUTER_HOST6	2	// 套接字模式：IPv6 外部监听的套接字
#define ITMD_INNER_HOST6	3	// 套接字模式：IPv6 内部监听的套接字
#define ITMD_DGRAM_HOST4	4	// 套接字模式：IPv4 数据报的套接字
#define ITMD_DGRAM_HOST6	5	// 套接字模式：IPv6 数据报的套接字
#define ITMD_OUTER_CLIENT	6	// 套接字模式：外部连接的套接字
#define ITMD_INNER_CLIENT	7	// 套接字模式：内部连接的套接字

#define ITMD_HOST_IS_OUTER(mode)	(((mode) & 1) == 0)
#define ITMD_HOST_IS_INNER(mode)	(((mode) & 1) == 1)
#define ITMD_HOST_IS_IPV4(mode)		(((mode) <= 1) || (mode) == ITMD_DGRAM_HOST4)
#define ITMD_HOST_IS_IPV6(mode)		(!ITMD_HOST_IS_IPV4(mode))

struct ITMD
{
	long mode;					// 模式：0－4
	long node;					// 在itm_fds中的节点编号
	long fd;					// 文件描述
	long hid;					// 句柄
	long tag;					// 辅助用户数据
	long mask;					// 读写mask
	long channel;				// 频道
	long timeid;				// 在时间队列的索引
	long initok;				// 是否完成设置
	long ccnum;					// 频道内的用户数量
	long session;				// 会话编号
	long touched;				// 数据报连接是否捆绑
	long dropped;				// 一共丢弃的数据包
	long inwlist;				// 是否在发送列表中
	long disable;				// 是否禁止
	struct IVQNODE wnode;		// 等待队列中的节点
	struct IVQUEUE waitq;		// 等待队列
	struct IMSTREAM lstream;	// 行分割流
	struct IMSTREAM rstream;	// 读入流
	struct IMSTREAM wstream;	// 写出流
	unsigned long cnt_tcpr;		// TCP接收计数器
	unsigned long cnt_tcpw;		// TCP发送计数器
	unsigned long cnt_udpr;		// UDP接收计数器
	unsigned long cnt_udpw;		// UDP发送计数器
	unsigned char skipped;		// 跳过 http头部否
	unsigned char history1;		// 历史字符串
	unsigned char history2;		// 历史字符串
	unsigned char history3;		// 历史字符串
	unsigned char history4;		// 历史字符串
	struct sockaddr_in remote4;		// 远程地址
	struct sockaddr_in dgramp4;		// 数据报地址
#ifdef AF_INET6
	struct sockaddr_in6 remote6;	// IPv6 远端地址
	struct sockaddr_in6 dgramp6;	// IPv6 数据报地址
#endif
	int IsIPv6;					// 是否使用 IPv6
#ifndef IDISABLE_RC4			// 判断 RC4功能是否被禁止
	int rc4_send_x;				// RC4 发送加密位置1
	int rc4_send_y;				// RC4 发送加密位置2
	int rc4_recv_x;				// RC4 接收加密位置1
	int rc4_recv_y;				// RC4 接收加密位置2
	unsigned char rc4_send_box[256];	// RC4 发送加密BOX
	unsigned char rc4_recv_box[256];	// RC4 接收加密BOX
#endif
};


//=====================================================================
// Endian Transform Definition
//=====================================================================
#ifndef ITMHTONS			// 定义字节序转换方式：默认不改变
#define ITMHTONS(x) (x)	
#endif

#ifndef ITMHTONL			// 定义字节序转换方式：默认不改变
#define ITMHTONL(x) (x)
#endif

#ifndef ITMNTOHS			// 定义字节序转换方式：默认不改变
#define ITMNTOHS(x) (x)
#endif

#ifndef ITMNTOHL			// 定义字节序转换方式：默认不改变
#define ITMNTOHL(x) (x)
#endif

#define ITMDINCD(x) ((x)=((unsigned long)((x) + 1) & 0x7fffffff))



//=====================================================================
// Main Container Definition
//=====================================================================
extern struct IMPOOL itm_fds;	// 套接字描述结构分配器
extern struct IMPOOL itm_mem;	// 内存页面分配器

extern struct IVECTOR itm_datav;	// 内部数据矢量
extern struct IVECTOR itm_hostv;	// 内部Channel列表矢量
extern struct ITMD **itm_host;		// 内部Channel列表指针
extern char *itm_data;				// 内部数据字节指针
extern char *itm_crypt;				// 内部数据加密指针

extern char *itm_document;			// 文档变量
extern long itm_docsize;			// 文档长度
extern unsigned int itm_version;	// 文档版本

extern struct ITMD itmd_inner4;		// IPv4 内部监听的ITMD(套接字描述)
extern struct ITMD itmd_outer4;		// IPv4 外部监听的ITMD(套接字描述)
extern struct ITMD itmd_dgram4;		// IPv4 数据报套接字的ITMD
extern struct ITMD itmd_inner6;		// IPv6 内部监听的ITMD(套接字描述)
extern struct ITMD itmd_outer6;		// IPv6 外部监听的ITMD(套接字描述)
extern struct ITMD itmd_dgram6;		// IPv6 数据报套接字的ITMD

extern struct IMSTREAM itm_dgramdat4;	// IPv4 数据报缓存
extern struct IMSTREAM itm_dgramdat6;	// IPv6 数据报缓存


//=====================================================================
// Public Method Definition
//=====================================================================
int itm_startup(void);	// 启动服务
int itm_shutdown(void);	// 关闭服务
int itm_process(long timeval);	// 处理一轮事件


//=====================================================================
// Timeout Control Definition
//=====================================================================
extern struct IDTIMEV itm_timeu;	// 外部连接超时控制器
extern struct IDTIMEV itm_timec;	// 内部连接超时控制器
int itm_timer(void);				// 时钟控制


//=====================================================================
// Private Method Definition
//=====================================================================
long itm_trysend(struct ITMD *itmd);	// 尝试发送 wstream
long itm_tryrecv(struct ITMD *itmd);	// 尝试接收 rstream
long itm_trysendto(int af);				// 尝试发送数据报

extern long itm_local1;		// 局部变量用作临时参数定义1
extern long itm_local2;		// 局部变量用作临时参数定义2

// 读取频道
#define itm_rchannel(x) ((x >= 0 && x < itm_hostc)? itm_host[x] : NULL)

int itm_wchannel(int index, struct ITMD *itmd);	// 设置频道
long itm_dsize(long length);					// 设置缓存数据大小
int itm_permitr(struct ITMD *itmd);				// 允许一个READ事件

#define ITM_READ	1			// 套接字事件方式：读
#define ITM_WRITE	2			// 套接字事件方式：写

#define ITM_BUFSIZE 0x10000		// 接收发送临时缓存
#define ITM_ADDRSIZE4 (sizeof(struct sockaddr))
#define ITM_ADDRSIZE6 (sizeof(struct sockaddr_in6))

extern char itm_zdata[];		// 底层数据收发缓存

extern short *itm_book[512];	// 每种事件关注的频道列表
extern int itm_booklen[512];	// 每种事件关注的频道数量

int itm_mask(struct ITMD *itmd, int enable, int disable);	// 设置事件捕捉
int itm_send(struct ITMD *itmd, const void *data, long length);	// 发送数据
int itm_bcheck(struct ITMD *itmd);				// 检查缓存

char *itm_epname4(const struct sockaddr *ep);	// 取得端点名称
char *itm_epname6(const struct sockaddr *ep);	// 取得端点名称
char *itm_epname(const struct ITMD *itmd);
char *itm_ntop(int af, const struct sockaddr *remote);

int itm_book_add(int category, int channel);	// 增加关注
int itm_book_del(int category, int channel);	// 取消关注
int itm_book_reset(int channel);				// 频道全部撤销关注
int itm_book_empty(void);						// 关注复位

// RC4 初始化及加密
void itm_rc4_init(unsigned char *box, int *x, int *y, const unsigned char *key, int keylen);
void itm_rc4_crypt(unsigned char *box, int *x, int *y, const unsigned char *src, unsigned char *dst, long size);

// 外部事件
long itm_msg_put(int id, const char *data, long size);
long itm_msg_get(int id, char *data, long maxsize);


//---------------------------------------------------------------------
// 数据报操作
//---------------------------------------------------------------------
struct ITMHUDP
{
	apr_uint32 order;
	apr_uint32 index;
	apr_int32 hid;
	apr_int32 session;
};

#define ITMU_MWORK	0x0001		// 数据报掩码：数据报系统开启
#define ITMU_MDUDP	0x0002		// 丢包掩码：udp自身数据错序
#define ITMU_MDTCP	0x0004		// 丢包掩码：tcp混合数据错序

#define ITMU_TOUCH	0x6000		// 命令：UDP初始化命令
#define ITMU_ECHO	0x6001		// 命令：回射命令
#define ITMU_MIRROR	0x6002		// 命令：返回网关
#define ITMU_DELIVER 0x6003		// 命令：转移传送
#define ITMU_FORWARD 0x6004		// 命令：转发


int itm_sendudp(int af, struct sockaddr *remote, struct ITMHUDP *head, const void *data, long len);
int itm_sendto(struct ITMD *itmd, const void *data, long length);	// 发送数据报
int itm_optitmd(struct ITMD *itmd, int flags);						// 设置连接参数

//---------------------------------------------------------------------
// 信息和频道设置
//---------------------------------------------------------------------
static inline struct ITMD* itm_hid_itmd(long hid)
{
	struct ITMD *itmd;
	long c = (hid & 0xffff);
	if (c < 0 || c >= itm_fds.node_max) return NULL;
	itmd = (struct ITMD*)IMP_DATA(&itm_fds, c);
	if (hid != itmd->hid) return NULL;
	return itmd;
}

//---------------------------------------------------------------------
// 发送列表相关操作
//---------------------------------------------------------------------
int itm_wlist_record(long hid);		// 记录到发送列表

//---------------------------------------------------------------------
// 数据加密和IP验证模块函数指针
//---------------------------------------------------------------------
typedef int (*ITM_ENCRYPT_HANDLE)(void*, const void *, int, int, int);
typedef int (*ITM_VALIDATE_HANDLE)(const void *sockaddr);
extern int (*itm_encrypt)(void *output, const void *input, int length, int fd, int mode);	
extern int (*itm_validate)(const void *sockaddr);

//---------------------------------------------------------------------
// 工具模块
//---------------------------------------------------------------------
typedef int (*ITM_LOG_HANDLE)(const char *);
extern int (*itm_logv)(const char *text);

int itm_log(int mask, const char *argv, ...);
void itm_lltoa(char *dst, apr_int64 x);


//=====================================================================
// 网络事件处理部分
//=====================================================================
#define ITMT_NEW		0	// 新近外部连接：(id,tag) ip/d,port/w
#define ITMT_LEAVE		1	// 断开外部连接：(id,tag)
#define ITMT_DATA		2	// 外部数据到达：(id,tag) data...
#define ITMT_CHANNEL	3	// 频道通信：(channel,tag)
#define ITMT_CHNEW		4	// 频道开启：(channel,id)
#define ITMT_CHSTOP		5	// 频道断开：(channel,tag)
#define ITMT_SYSCD		6	// 系统信息：(subtype, v) data...
#define ITMT_TIMER		7	// 系统时钟：(timesec,timeusec)
#define ITMT_UNRDAT		10	// 不可靠数据包：(id,tag)
#define ITMT_NOOP		80	// 空指令：(wparam, lparam)

#define ITMC_DATA		0	// 外部数据发送：(id,*) data...
#define ITMC_CLOSE		1	// 关闭外部连接：(id,code)
#define ITMC_TAG		2	// 设置TAG：(id,tag)
#define ITMC_CHANNEL	3	// 组间通信：(channel,*) data...
#define ITMC_MOVEC		4	// 移动外部连接：(channel,id) data...
#define ITMC_SYSCD		5	// 系统控制消息：(subtype, v) data...
#define ITMC_BROADCAST	6	// 广播数据：(count, limit) data, hidlist...
#define ITMC_UNRDAT		10	// 不可靠数据包：(id,tag)
#define ITMC_IOCTL		11	// 连接控制指令：(id,flag)
#define ITMC_NOOP		80	// 空指令：(*,*)

#define ITMS_CONNC		0	// 请求连接数量(st,0) cu/d,cc/d
#define ITMS_LOGLV		1	// 设置日志级别(st,level)
#define ITMS_LISTC		2	// 返回频道信息(st,cn) d[ch,id,tag],w[t,c]
#define ITMS_RTIME		3	// 系统运行时间(st,wtime)
#define ITMS_TMVER		4	// 传输模块版本(st,tmver)
#define ITMS_REHID		5	// 返回频道的(st,ch)
#define ITMS_QUITD		6	// 请求自己退出
#define ITMS_TIMER		8	// 设置频道零的时钟(st,timems)
#define ITMS_INTERVAL	9	// 设置是否为间隔模式(st,isinterval)
#define ITMS_FASTMODE	10	// 设置是否使用连接队列算法
#define ITMS_CHID		11	// 取得自己的channel编号(st, ch)
#define ITMS_BOOKADD	12	// 增加订阅
#define ITMS_BOOKDEL	13	// 取消订阅
#define ITMS_BOOKRST	14	// 清空订阅
#define ITMS_STATISTIC	15	// 统计信息
#define ITMS_RC4SKEY	16	// 设置发送KEY (st, hid) key
#define ITMS_RC4RKEY	17	// 设置接收KEY (st, hid) key
#define ITMS_DISABLE	18	// 禁止接收该用户消息
#define ITMS_ENABLE		19	// 允许接收该用户消息
#define ITMS_SETDOC		20	// 文档设置
#define ITMS_GETDOC		21	// 文档读取
#define ITMS_MESSAGE	22	// 外部控制事件 
#define ITMS_NODELAY	1	// 连接控制：设置立即发送模式
#define ITMS_NOPUSH		2	// 连接控制：设置数据流塞子
#define ITMS_PRIORITY	3	// SO_PRIORITY
#define ITMS_TOS		4	// IP_TOS

#define ITMH_WORDLSB	0		// 头部标志：2字节LSB
#define ITMH_WORDMSB	1		// 头部标志：2字节MSB
#define ITMH_DWORDLSB	2		// 头部标志：4字节LSB
#define ITMH_DWORDMSB	3		// 头部标志：4字节MSB
#define ITMH_BYTELSB	4		// 头部标志：单字节LSB
#define ITMH_BYTEMSB	5		// 头部标志：单字节MSB
#define ITMH_EWORDLSB	6		// 头部标志：2字节LSB（不包含自己）
#define ITMH_EWORDMSB	7		// 头部标志：2字节MSB（不包含自己）
#define ITMH_EDWORDLSB	8		// 头部标志：4字节LSB（不包含自己）
#define ITMH_EDWORDMSB	9		// 头部标志：4字节MSB（不包含自己）
#define ITMH_EBYTELSB	10		// 头部标志：单字节LSB（不包含自己）
#define ITMH_EBYTEMSB	11		// 头部标志：单字节MSB（不包含自己）
#define ITMH_DWORDMASK	12		// 头部标志：4字节LSB（包含自己和掩码）
#define ITMH_RAWDATA	13		// 头部标志：对外无头部，对内4字节LSB
#define ITMH_LINESPLIT	14		// 头部标志：对外无头部，按行分割，对内4字节LSB


#define ITML_BASE		0x01	// 日志代码：基本
#define ITML_INFO		0x02	// 日志代码：信息
#define ITML_ERROR		0x04	// 日志代码：错误
#define ITML_WARNING	0x08	// 日志代码：警告
#define ITML_DATA		0x10	// 日志代码：数据
#define ITML_CHANNEL	0x20	// 日志代码：频道
#define ITML_EVENT		0x40	// 日志代码：事件
#define ITML_LOST		0x80	// 日志代码：丢包记录


//---------------------------------------------------------------------
// 数据输入处理
//---------------------------------------------------------------------
int itm_data_inner(struct ITMD *itmd);					// 内部数据包到达
int itm_data_outer(struct ITMD *itmd);					// 外部数据包到达

//---------------------------------------------------------------------
// 网络事件处理
//---------------------------------------------------------------------
int itm_event_accept(int hmode);						// 事件：接收新连接
int itm_event_close(struct ITMD *itmd, int code);		// 事件：关闭连接
int itm_event_recv(struct ITMD *itmd);					// 事件：接收
int itm_event_send(struct ITMD *itmd);					// 事件：发送
int itm_event_dgram(int af);							// 事件：数据报到达

int itm_dgram_data(int af, struct sockaddr *remote, struct ITMHUDP *head, void *data, long size);
int itm_dgram_cmd(int af, struct sockaddr *remote, struct ITMHUDP *head, void *data, long size);

//---------------------------------------------------------------------
// 频道事件处理
//---------------------------------------------------------------------
int itm_on_logon(struct ITMD *itmd);
int itm_on_data(struct ITMD *itmd, long wparam, long lparam, long length);
int itm_on_close(struct ITMD *itmd, long wparam, long lparam, long length);
int itm_on_tag(struct ITMD *itmd, long wparam, long lparam, long length);
int itm_on_channel(struct ITMD *itmd, long wparam, long lparam, long length);
int itm_on_movec(struct ITMD *itmd, long wparam, long lparam, long length);
int itm_on_syscd(struct ITMD *itmd, long wparam, long lparam, long length);
int itm_on_dgram(struct ITMD *itmd, long wparam, long lparam, long length);
int itm_on_ioctl(struct ITMD *itmd, long wparam, long lparam, long length);
int itm_on_broadcast(struct ITMD *itmd, long wparam, long lparam, long length);


//---------------------------------------------------------------------
// 内置函数
//---------------------------------------------------------------------
static inline char *iencode16u_lsb(char *p, unsigned short w)
{
#if IWORDS_BIG_ENDIAN
	*(unsigned char*)(p + 0) = (w & 255);
	*(unsigned char*)(p + 1) = (w >> 8);
#else
	*(unsigned short*)(p) = w;
#endif
	p += 2;
	return p;
}

static inline char *idecode16u_lsb(const char *p, unsigned short *w)
{
#if IWORDS_BIG_ENDIAN
	*w = *(const unsigned char*)(p + 1);
	*w = *(const unsigned char*)(p + 0) + (*w << 8);
#else
	*w = *(const unsigned short*)p;
#endif
	p += 2;
	return (char*)p;
}


static inline char *iencode16u_msb(char *p, unsigned short w)
{
#if IWORDS_BIG_ENDIAN
	*(unsigned short*)(p) = w;
#else
	*(unsigned char*)(p + 0) = (w >> 8);
	*(unsigned char*)(p + 1) = (w & 255);
#endif
	p += 2;
	return p;
}

static inline char *idecode16u_msb(const char *p, unsigned short *w)
{
#if IWORDS_BIG_ENDIAN
	*w = *(const unsigned short*)p;
#else
	*w = *(const unsigned char*)(p + 0);
	*w = *(const unsigned char*)(p + 1) + (*w << 8);
#endif
	p += 2;
	return (char*)p;
}

static inline char *iencode32u_lsb(char *p, apr_uint32 l)
{
#if IWORDS_BIG_ENDIAN
	*(unsigned char*)(p + 0) = (unsigned char)((l >>  0) & 0xff);
	*(unsigned char*)(p + 1) = (unsigned char)((l >>  8) & 0xff);
	*(unsigned char*)(p + 2) = (unsigned char)((l >> 16) & 0xff);
	*(unsigned char*)(p + 3) = (unsigned char)((l >> 24) & 0xff);
#else
	*(apr_uint32*)p = l;
#endif
	p += 4;
	return p;
}

static inline char *idecode32u_lsb(const char *p, apr_uint32 *l)
{
#if IWORDS_BIG_ENDIAN
	*l = *(const unsigned char*)(p + 3);
	*l = *(const unsigned char*)(p + 2) + (*l << 8);
	*l = *(const unsigned char*)(p + 1) + (*l << 8);
	*l = *(const unsigned char*)(p + 0) + (*l << 8);
#else 
	*l = *(const apr_uint32*)p;
#endif
	p += 4;
	return (char*)p;
}

static inline char *iencode32u_msb(char *p, apr_uint32 l)
{
#if IWORDS_BIG_ENDIAN
	*(apr_uint32*)p = l;
#else
	*(unsigned char*)(p + 0) = (unsigned char)((l >> 24) & 0xff);
	*(unsigned char*)(p + 1) = (unsigned char)((l >> 16) & 0xff);
	*(unsigned char*)(p + 2) = (unsigned char)((l >>  8) & 0xff);
	*(unsigned char*)(p + 3) = (unsigned char)((l >>  0) & 0xff);
#endif
	p += 4;
	return p;
}

static inline char *idecode32u_msb(const char *p, apr_uint32 *l)
{
#if IWORDS_BIG_ENDIAN
	*l = *(const apr_uint32*)p;
#else 
	*l = *(const unsigned char*)(p + 0);
	*l = *(const unsigned char*)(p + 1) + (*l << 8);
	*l = *(const unsigned char*)(p + 2) + (*l << 8);
	*l = *(const unsigned char*)(p + 3) + (*l << 8);
#endif
	p += 4;
	return (char*)p;
}

/* encode 8 bits unsigned int */
static inline char *iencode8u(char *p, unsigned char c)
{
	*(unsigned char*)p++ = c;
	return p;
}

/* decode 8 bits unsigned int */
static inline char *idecode8u(const char *p, unsigned char *c)
{
	*c = *(unsigned char*)p++;
	return (char*)p;
}

static inline long itm_size_get(const void *p)
{
	unsigned char cbyte;
	apr_uint16 cshort;
	apr_uint32 cint;
	long length = 0;

	switch (itm_headint)
	{
	case ITMH_WORDLSB:
		idecode16u_lsb((const char*)p, &cshort);
		length = cshort;
		break;
	case ITMH_WORDMSB:
		idecode16u_msb((const char*)p, &cshort);
		length = cshort;
		break;
	case ITMH_DWORDLSB:
		idecode32u_lsb((const char*)p, &cint);
		length = cint;
		break;
	case ITMH_DWORDMSB:
		idecode32u_msb((const char*)p, &cint);
		length = cint;
		break;
	case ITMH_BYTELSB:
	case ITMH_BYTEMSB:
		idecode8u((const char*)p, &cbyte);
		length = cbyte;
		break;
	}
	if (itm_headmsk) {
		length &= 0xffffff;
	}
	return length + itm_headinc;
}

static inline void itm_size_set(void *p, long size)
{
	size -= itm_headinc;
	switch (itm_headint)
	{
	case ITMH_WORDLSB:
		iencode16u_lsb((char*)p, (apr_uint16)size);
		break;
	case ITMH_WORDMSB:
		iencode16u_msb((char*)p, (apr_uint16)size);
		break;
	case ITMH_DWORDLSB:
		iencode32u_lsb((char*)p, (apr_uint32)size);
		break;
	case ITMH_DWORDMSB:
		iencode32u_msb((char*)p, (apr_uint32)size);
		break;
	case ITMH_BYTELSB:
	case ITMH_BYTEMSB:
		iencode8u((char*)p, (unsigned char)size);
		break;
	}
}

static inline int itm_cate_get(void *p)
{
	apr_uint32 length;
	if (itm_headmsk == 0) return 0;
	idecode32u_lsb((const char*)p, &length);
	return (int)((length >> 24) & 0xff);
}

static inline int itm_param_get(const void *ptr, long *length, short *cmd, long *wparam, long *lparam)
{
	const char *lptr = (const char*)ptr;
	unsigned char cbyte;
	apr_uint16 cshort;
	apr_uint32 cint;
	int size = 12;
	long x = 0;

	switch (itm_headint)
	{
	case ITMH_WORDLSB: 
		lptr = idecode16u_lsb(lptr, &cshort); x = cshort;
		lptr = idecode16u_lsb(lptr, (apr_uint16*)cmd); 
		lptr = idecode32u_lsb(lptr, &cint); *wparam = cint;
		lptr = idecode32u_lsb(lptr, &cint); *lparam = cint;
		break;
	case ITMH_WORDMSB: 
		lptr = idecode16u_msb(lptr, &cshort); x = cshort;
		lptr = idecode16u_msb(lptr, (apr_uint16*)cmd); 
		lptr = idecode32u_msb(lptr, &cint); *wparam = cint;
		lptr = idecode32u_msb(lptr, &cint); *lparam = cint;
		break;
	case ITMH_DWORDLSB: 
		lptr = idecode32u_lsb(lptr, &cint); x = cint;
		lptr = idecode16u_lsb(lptr, (apr_uint16*)cmd); 
		lptr = idecode32u_lsb(lptr, &cint); *wparam = cint;
		lptr = idecode32u_lsb(lptr, &cint); *lparam = cint;
		size = 14;
		break;
	case ITMH_DWORDMSB: 
		lptr = idecode32u_msb(lptr, &cint); x = cint;
		lptr = idecode16u_msb(lptr, (apr_uint16*)cmd); 
		lptr = idecode32u_msb(lptr, &cint); *wparam = cint;
		lptr = idecode32u_msb(lptr, &cint); *lparam = cint;
		size = 14;
		break;
	case ITMH_BYTELSB: 
		lptr = idecode8u(lptr, &cbyte); x = cbyte;
		lptr = idecode16u_lsb(lptr, (apr_uint16*)cmd); 
		lptr = idecode32u_lsb(lptr, &cint); *wparam = cint;
		lptr = idecode32u_lsb(lptr, &cint); *lparam = cint;
		size = 11;
		break;
	case ITMH_BYTEMSB: 
		lptr = idecode8u(lptr, &cbyte); x = cbyte;
		lptr = idecode16u_msb(lptr, (apr_uint16*)cmd); 
		lptr = idecode32u_msb(lptr, &cint); *wparam = cint;
		lptr = idecode32u_msb(lptr, &cint); *lparam = cint;
		size = 11;
		break;
	}
	*length = x + itm_headinc;
	return size;
}

static inline int itm_param_set(int offset, long length, short cmd, long wparam, long lparam)
{
	char *lptr = itm_data + offset;
	int size = 12;

	length -= itm_headinc;
	switch (itm_headint)
	{
	case ITMH_WORDLSB: 
		lptr = iencode16u_lsb(lptr, (apr_uint16)length);
		lptr = iencode16u_lsb(lptr, (apr_uint16)cmd);
		lptr = iencode32u_lsb(lptr, (apr_uint32)wparam);
		lptr = iencode32u_lsb(lptr, (apr_uint32)lparam);
		break;
	case ITMH_WORDMSB:
		lptr = iencode16u_msb(lptr, (apr_uint16)length);
		lptr = iencode16u_msb(lptr, (apr_uint16)cmd);
		lptr = iencode32u_msb(lptr, (apr_uint32)wparam);
		lptr = iencode32u_msb(lptr, (apr_uint32)lparam);
		break;
	case ITMH_DWORDLSB:
		lptr = iencode32u_lsb(lptr, (apr_uint32)length);
		lptr = iencode16u_lsb(lptr, (apr_uint16)cmd);
		lptr = iencode32u_lsb(lptr, (apr_uint32)wparam);
		lptr = iencode32u_lsb(lptr, (apr_uint32)lparam);
		size = 14;
		break;
	case ITMH_DWORDMSB:
		lptr = iencode32u_msb(lptr, (apr_uint32)length);
		lptr = iencode16u_msb(lptr, (apr_uint16)cmd);
		lptr = iencode32u_msb(lptr, (apr_uint32)wparam);
		lptr = iencode32u_msb(lptr, (apr_uint32)lparam);
		size = 14;
		break;
	case ITMH_BYTELSB:
		lptr = iencode8u(lptr, (unsigned char)length);
		lptr = iencode16u_lsb(lptr, (apr_uint16)cmd);
		lptr = iencode32u_lsb(lptr, (apr_uint32)wparam);
		lptr = iencode32u_lsb(lptr, (apr_uint32)lparam);
		size = 11;
		break;
	case ITMH_BYTEMSB:
		lptr = iencode8u(lptr, (unsigned char)length);
		lptr = iencode16u_msb(lptr, (apr_uint16)cmd);
		lptr = iencode32u_msb(lptr, (apr_uint32)wparam);
		lptr = iencode32u_msb(lptr, (apr_uint32)lparam);
		size = 11;
		break;
	}
	return size;
}


static inline long itm_dataok(struct ITMD *itmd)
{
	struct IMSTREAM *stream = &(itmd->rstream);
	long len;
	char buf[4];
	void *ptr = (void*)buf;

	if (itmd->mode == ITMD_OUTER_CLIENT) {
		if (itm_headmod == ITMH_RAWDATA) {
			long size = stream->size;
			long limit = (ITM_BUFSIZE < itm_datamax)? ITM_BUFSIZE : itm_datamax;
			if (size < limit) return size;
			return limit;
		}
	}

	len = ims_peek(stream, buf, itm_hdrsize);
	if (len < itm_hdrsize) return 0;

	len = (long)(itm_size_get(ptr));

	if (len < itm_hdrsize) return -1;
	if (len > itm_datamax) return -2;
	if (stream->size < len) return 0;

	return len;
}

static inline void itm_write_dword(void *ptr, apr_uint32 dword)
{
	if (itm_headint & 1) {
		iencode32u_msb((char*)ptr, dword);
	}	else {
		iencode32u_lsb((char*)ptr, dword);
	}
}

static inline void itm_write_word(void *ptr, apr_uint16 word)
{
	if (itm_headmod & 1) {
		iencode16u_msb((char*)ptr, word);
	}	else {
		iencode16u_lsb((char*)ptr, word);
	}
}

static inline unsigned short itm_read_word(const void *ptr)
{
	unsigned short word;
	if (itm_headmod & 1) {
		idecode16u_msb((const char*)ptr, &word);
	}	else {
		idecode16u_lsb((const char*)ptr, &word);
	}
	return word;
}

static inline apr_uint32 itm_read_dword(const void *ptr)
{
	apr_uint32 dword;
	if (itm_headmod & 1) {
		idecode32u_msb((const char*)ptr, &dword);
	}	else {
		idecode32u_lsb((const char*)ptr, &dword);
	}
	return dword;
}


#ifdef __cplusplus
}
#endif


#endif



