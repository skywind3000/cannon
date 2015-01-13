# -*- coding: utf-8 -*-

ITMT_NEW        =  0    # 新近外部连接：(id,tag) ip/d,port/w   <hid>
ITMT_LEAVE      =  1    # 断开外部连接：(id,tag)           <hid>
ITMT_DATA       =  2    # 外部数据到达：(id,tag) data...    <hid>
ITMT_CHANNEL    =  3    # 频道通信：(channel,tag)    <>
ITMT_CHNEW      =  4    # 频道开启：(channel,id)
ITMT_CHSTOP     =  5    # 频道断开：(channel,tag)
ITMT_SYSCD      =  6    # 系统信息：(subtype, v) data...
ITMT_TIMER      =  7    # 系统时钟：(timesec,timeusec)
ITMT_UNRDAT     = 10    # 不可靠数据包：(id,tag)
ITMT_NOOP       = 80    # 空指令：(wparam, lparam)
ITMT_BLOCK      = 99    # 没有指令

ITMC_DATA =        0    # 外部数据发送：(id,*) data...
ITMC_CLOSE =        1    # 关闭外部连接：(id,code)
ITMC_TAG =        2    # 设置TAG：(id,tag)
ITMC_CHANNEL =    3    # 组间通信：(channel,*) data...
ITMC_MOVEC =        4    # 移动外部连接：(channel,id) data...
ITMC_SYSCD =        5    # 系统控制消息：(subtype, v) data...
ITMC_BROADCAST =	6	 # 广播消息
ITMC_UNRDAT =        10    # 不可靠数据包：(id,tag)
ITMC_IOCTL =        11    # 连接控制指令：(id,flag)
ITMC_NOOP =        80    # 空指令：(*,*)

ITMS_CONNC =        0    # 请求连接数量(st,0) cu/d,cc/d
ITMS_LOGLV =        1    # 设置日志级别(st,level)
ITMS_LISTC =        2    # 返回频道信息(st,cn) d[ch,id,tag],w[t,c]
ITMS_RTIME =        3    # 系统运行时间(st,wtime)
ITMS_TMVER =        4    # 传输模块版本(st,tmver)
ITMS_REHID =        5    # 返回频道的(st,ch)
ITMS_QUITD =        6    # 请求自己退出
ITMS_TIMER =        8    # 设置频道零的时钟(st,timems)
ITMS_INTERVAL =        9    # 设置是否为间隔模式(st,isinterval)
ITMS_FASTMODE =        10    # 设置是否启用快速模式
ITMS_DISABLE = 18
ITMS_ENABLE = 19
ITMS_SETDOC = 20
ITMS_GETDOC = 21
ITMS_MESSAGE = 22

ITMS_NODELAY	=	1	# 设置禁用Nagle算法
ITMS_NOPUSH		=	2	# 禁止发送接口
ITMS_PRIORITY = 3
ITMS_TOS = 4

ITML_BASE =        0x01    # 日志代码：基本
ITML_INFO =        0x02    # 日志代码：信息
ITML_ERROR =        0x04    # 日志代码：错误
ITML_WARNING =    0x08    # 日志代码：警告
ITML_DATA =        0x10    # 日志代码：数据
ITML_CHANNEL =    0x20    # 日志代码：频道
ITML_EVENT =        0x40    # 日志代码：事件
ITML_LOST =        0x80    # 日志代码：丢包记录
