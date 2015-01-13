#ifndef __INSTRUCTION_H__
#define __INSTRUCTION_H__

// instructions channel will receive
//
#define ITMT_NEW		0	// 新近外部连接：(id,tag) ip/d,port/w   <hid>
#define ITMT_LEAVE		1	// 断开外部连接：(id,tag)   		<hid>
#define ITMT_DATA		2	// 外部数据到达：(id,tag) data...	<hid>
		//GE_CHAT
		//GE_TIMER
		//...
#define ITMT_CHANNEL	3	// 频道通信：(channel,tag)	<>
		//CH_RESULT
		//CH_EXIT	
#define ITMT_CHNEW		4	// 频道开启：(channel,id)
#define ITMT_CHSTOP		5	// 频道断开：(channel,tag)

#define ITMT_SYSCD		6	// 系统信息：(subtype, v) data...
#define ITMT_TIMER		7	// 系统时钟：(timesec,timeusec)
#define ITMT_UNRDAT		10	// 不可靠数据包：(id,tag)
#define ITMT_NOOP		80	// 空指令：(wparam, lparam)


// instructions from channel to transmod 
//
#define ITMC_DATA		0	// 外部数据发送：(id,*) data...
#define ITMC_CLOSE		1	// 关闭外部连接：(id,code)
#define ITMC_TAG		2	// 设置TAG：(id,tag)
#define ITMC_CHANNEL	3	// 组间通信：(channel,*) data...
#define ITMC_MOVEC		4	// 移动外部连接：(channel,id) data...
#define ITMC_SYSCD		5	// 系统控制消息：(subtype, v) data...
#define ITMC_BROADCAST  6	// 广播
#define ITMC_UNRDAT		10	// 不可靠数据包：(id,tag)
#define ITMC_IOCTL		11	// 连接控制指令：(id,flag)
#define ITMC_SEED		12	// 设置加密种子
#define ITMC_NOOP		80	// 空指令：(*,*)


//the subinstrcutions for the ITMC_SYSINFO
#define ITMS_CONNC		0	// 请求连接数量(st,0) cu/d,cc/d
#define ITMS_LOGLV		1	// 设置日志级别(st,level)
#define ITMS_LISTC		2	// 返回频道信息(st,cn) d[ch,id,tag],w[t,c]
#define ITMS_RTIME		3	// 系统运行时间(st,wtime)
#define ITMS_TMVER		4	// 传输模块版本(st,tmver)
#define ITMS_REHID		5	// 返回频道的(st,ch)
#define ITMS_QUITD		6	// 请求自己退出
#define ITMS_TIMER		8	// 设置频道零的时钟(st,timems)
#define ITMS_CHID		11	// 取得自己的channel编号(st, ch)
#define ITMS_BOOKADD	12	// 增加订阅
#define ITMS_BOOKDEL	13	// 取消订阅
#define ITMS_BOOKRST	14	// 清空订阅
#define ITMS_STATISTIC	15	// 统计信息
#define ITMS_RC4SKEY	16	// 设置发送KEY (st, hid) key
#define ITMS_RC4RKEY	17	// 设置接收KEY (st, hid) key

#define ITMS_NODELAY	1	// 设置禁用Nagle算法
#define ITMS_NOPUSH		2	// 禁止发送接口

//for log
#define ITML_BASE		0x01	// 日志代码：基本
#define ITML_INFO		0x02	// 日志代码：信息
#define ITML_ERROR		0x04	// 日志代码：错误
#define ITML_WARNING	0x08	// 日志代码：警告
#define ITML_DATA		0x10	// 日志代码：数据
#define ITML_CHANNEL	0x20	// 日志代码：频道
#define ITML_EVENT		0x40	// 日志代码：事件
#define ITML_LOST		0x80	// 日志代码：丢包记录

#endif

