//=====================================================================
//
// TML <Transmod Library>, by skywind 2004, transmod.h
//
// NOTES： 
// 网络传输库 TML<传输模块>，建立 客户/频道的通信模式，提供基于多频道
// multi-channel通信的 TCP/UDP通信机制，缓存/内存管理，超时控制等机制
// 的服务接口，以低系统占用完成即时的万人级通信任务
//
//=====================================================================


#ifndef __TRANSMOD_H__
#define __TRANSMOD_H__

#if defined(__unix__) || defined(unix) || defined(__linux)
	#ifndef __unix
		#define __unix 1
	#endif
#endif

#if defined(__APPLE__) && (!defined(__unix))
    #define __unix 1
#endif

#ifndef APR_MODULE
#ifdef __unix
#define APR_MODULE(type)  type 
#elif defined(_WIN32)
#define APR_MODULE(type)  __declspec(dllexport) type
#else
#error APR-MODULE only can be compiled under unix or win32 !!
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

//---------------------------------------------------------------------
// Service Status Definition
//---------------------------------------------------------------------

#define CTMS_SERVICE		0	// 服务状态
#define CTMS_CUCOUNT		1	// 现在外部连接数
#define CTMS_CHCOUNT		2	// 现在频道连接数
#define CTMS_WTIME			3	// 取得服务运行时间
#define CTMS_STIME			4	// 取得开始服务时间
#define CTMS_CSEND			5	// 发送数量
#define CTMS_CRECV			6	// 接受数量
#define CTMS_CDISCARD		7	// 放弃数量

#define CTM_STOPPED		0	// 服务状态：停止
#define CTM_STARTING	1	// 服务状态：启动中
#define CTM_RUNNING		2	// 服务状态：服务
#define CTM_STOPPING	3	// 服务状态：停止中

#define CTM_OK			0	// 没有错误
#define CTM_ERROR		1	// 发生错误


//---------------------------------------------------------------------
// Service Method Definition
//---------------------------------------------------------------------

// 开启服务
APR_MODULE(int) ctm_startup(void);

// 关闭服务
APR_MODULE(int) ctm_shutdown(void);

// 取得服务状态
APR_MODULE(long) ctm_status(int item);

// 取得错误代码
APR_MODULE(long) ctm_errno(void);


//---------------------------------------------------------------------
// Options Method Definition
//---------------------------------------------------------------------
#define CTMO_HEADER		0	// 头部模式
#define CTMO_WTIME		1	// 世界时间
#define CTMO_PORTU4		2	// IPv4 外部端口
#define CTMO_PORTC4		3	// IPv4 内部端口
#define CTMO_PORTU6		4	// IPv6 外部端口
#define CTMO_PORTC6		5	// IPv6 内部端口
#define CTMO_PORTD4		6	// IPv4 数据报端口
#define CTMO_PORTD6		7	// IPv6 数据报端口
#define CTMO_HTTPSKIP	8	// 跳过 HTTP头部
#define CTMO_DGRAM		9	// 数据报启动模式
#define CTMO_AUTOPORT	10	// 是否自动端口

#define CTMO_MAXCU		20	// 外部最大连接
#define CTMO_MAXCC		21	// 内部最大连接
#define CTMO_TIMEU		22	// 外部连接超时
#define CTMO_TIMEC		23	// 内部连接超时
#define CTMO_LIMIT		24	// 频道最大限制
#define CTMO_LIMTU		25	// 外部最大缓存
#define CTMO_LIMTC		26	// 内部最大缓存
#define CTMO_ADDRC4		27	// IPv4 内部绑定地址
#define CTMO_ADDRC6		28	// IPv6 内部绑定地址

#define CTMO_DATMAX		40	// 最大数据
#define CTMO_DHCPBASE	41	// 最低的分配
#define CTMO_DHCPHIGH	42	// 最高的分配
#define CTMO_PSIZE		43	// 页面大小默认4K

#define CTMO_GETPU4		60	// 读取端口
#define CTMO_GETPC4		61	// 读取端口
#define CTMO_GETPD4		62	// 读取端口
#define CTMO_GETPU6		63	// 读取端口
#define CTMO_GETPC6		64	// 读取端口
#define CTMO_GETPD6		65	// 读取端口

#define CTMO_PLOGP		80	// 设置日志打印函数指针
#define	CTMO_PENCP		81	// 设置加密函数指针
#define CTMO_LOGMK		82	// 日志报告掩码
#define CTMO_INTERVAL	83	// 启动间隔模式
#define CTMO_UTIME		84	// 客户端计时模式
#define CTMO_REUSEADDR	85	// 套接字地址重用：0自动 1启用 2禁止
#define CTMO_REUSEPORT	86	// 套接字端口重用：0自动 1启用 2禁止
#define CTMO_SOCKSNDO	90	// 外部套接字发送缓存
#define CTMO_SOCKRCVO	91	// 外部套接字接收缓存
#define CTMO_SOCKSNDI	92	// 内部套接字发送缓存
#define CTMO_SOCKRCVI	93	// 内部套接字接收缓存
#define CTMO_SOCKUDPB	94	// 数据报套接字缓存


// 设置服务参数
APR_MODULE(int) ctm_config(int item, long value, const char *text);


typedef int (*CTM_LOG_HANDLE)(const char *);
typedef int (*CTM_ENCRYPT_HANDLE)(void*, const void *, int, int, int);
typedef int (*CTM_VALIDATE_HANDLE)(const void *sockaddr);

// 设置日志函数Handle
APR_MODULE(int) ctm_handle_log(CTM_LOG_HANDLE handle);

// 设置加密函数Handle
APR_MODULE(int) ctm_handle_encrypt(CTM_ENCRYPT_HANDLE handle);

// 设置验证函数Handle
APR_MODULE(int) ctm_handle_validate(CTM_VALIDATE_HANDLE handle);

// 设置默认日志输出接口
// mode=0是关闭, 1是文件，2是标准输入，4是标准错误
APR_MODULE(int) cmt_handle_logout(int mode, const char *fn_prefix);


//---------------------------------------------------------------------
// 编译信息
//---------------------------------------------------------------------

// 取得编译版本号
APR_MODULE(int) ctm_version(void);

// 取得编译日期
APR_MODULE(const char*) ctm_get_date(void);

// 取得编译时间
APR_MODULE(const char*) ctm_get_time(void);

// 统计信息：得到发送了多少包，收到多少包，丢弃多少包（限制发送缓存模式）
// 注意：三个指针会被填充 64位整数。
APR_MODULE(void) ctm_get_stat(void *stat_send, void *stat_recv, void *stat_discard);


#ifdef __cplusplus
}
#endif

#endif



