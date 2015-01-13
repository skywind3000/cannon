//======================================================================
//
// idtimeout.h
//
// NOTE: This is a timeout controller implement with ANSI C
// By Linwei 12/4/2004
//
//======================================================================

#ifndef __I_DTIMEOUT_H__
#define __I_DTIMEOUT_H__

#include "impoold.h"


struct IDTIMEN				// 计时器节点
{
	struct IDTIMEN *next;	// 下一个节点
	struct IDTIMEN *prev;	// 上一个节点
	long time;				// 设定的时间
	long data;				// 用户数据
	long node;				// 节点记录
};

struct IDTIMEV				// 时间控制器
{
	struct IMPOOL pnodes;	// 节点分配器
	struct IDTIMEN *head;	// 队列头部节点
	struct IDTIMEN *tail;	// 队列尾部节点
	long timeout;			// 时间设置
	long wtime;				// 当前时间
};

// 初始化超时控制器
void idt_init(struct IDTIMEV *idtime, long timeout, struct IALLOCATOR *allocator);

// 销毁超时控制器
void idt_destroy(struct IDTIMEV *idtime);

// 设置当前时间
int idt_settime(struct IDTIMEV *idtime, long time);

// 新建计时器
int idt_newtime(struct IDTIMEV *idtime, long data);

// 移出当前计时器
int idt_remove(struct IDTIMEV *idtime, int n);

// 激活当前计时器
int idt_active(struct IDTIMEV *idtime, int n);

// 获得当前完成的计时器
int idt_timeout(struct IDTIMEV *idtime, int *nodev, long *datav);


#endif

