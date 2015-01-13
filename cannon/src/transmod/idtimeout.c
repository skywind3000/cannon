//======================================================================
//
// idtimeout.h
//
// NOTE: This is a timeout controller implement with ANSI C
// By Linwei 12/4/2004
//
//======================================================================

#include "idtimeout.h"

#include <stdio.h>
#include <assert.h>

//---------------------------------------------------------------------
// 静态方法声明
//---------------------------------------------------------------------
static int idt_node_add(struct IDTIMEV *idtime, int n);
static int idt_node_remove(struct IDTIMEV *idtime, int n);


//---------------------------------------------------------------------
// 初始化超时控制器
//---------------------------------------------------------------------
void idt_init(struct IDTIMEV *idtime, long timeout, struct IALLOCATOR *allocator)
{
	assert(idtime);
	imp_init(&idtime->pnodes, sizeof(struct IDTIMEN), allocator);
	idtime->head = NULL;
	idtime->tail = NULL;
	idtime->timeout = timeout;
	idtime->wtime = 0;
}

//---------------------------------------------------------------------
// 销毁超时控制器
//---------------------------------------------------------------------
void idt_destroy(struct IDTIMEV *idtime)
{
	assert(idtime);
	imp_destroy(&idtime->pnodes);
	idtime->head = NULL;
	idtime->tail = NULL;
	idtime->timeout = 1;
	idtime->wtime = 0;
}



//---------------------------------------------------------------------
// 设置当前时间
//---------------------------------------------------------------------
int idt_settime(struct IDTIMEV *idtime, long time)
{
	assert(idtime);
	if (time < idtime->wtime) return -1;
	idtime->wtime = time;
	return 0;
}

//---------------------------------------------------------------------
// 将计时器增加到超时队列头
//---------------------------------------------------------------------
static int idt_node_add(struct IDTIMEV *idtime, int n)
{
	struct IDTIMEN *node;

	if (n < 0 || n >= idtime->pnodes.node_max) return -1;
	if (idtime->pnodes.mmode[n] == 0) return -2;
	node = (struct IDTIMEN*)IMP_DATA(&idtime->pnodes, n);
	node->prev = idtime->head;
	node->next = NULL;
	if (idtime->head) idtime->head->next = node;
	idtime->head = node;
	if (idtime->tail == NULL) idtime->tail = node;

	return 0;
}

//---------------------------------------------------------------------
// 从超时队列中移出节点
//---------------------------------------------------------------------
static int idt_node_remove(struct IDTIMEV *idtime, int n)
{
	struct IDTIMEN *node;

	if (n < 0 || n >= idtime->pnodes.node_max) return -1;
	if (idtime->pnodes.mmode[n] == 0) return -2;
	node = (struct IDTIMEN*)IMP_DATA(&idtime->pnodes, n);
	if (node->prev) node->prev->next = node->next;
	else idtime->tail = node->next;
	if (node->next) node->next->prev = node->prev;
	else idtime->head = node->prev;

	return 0;
}

//---------------------------------------------------------------------
// 新建计时器
//---------------------------------------------------------------------
int idt_newtime(struct IDTIMEV *idtime, long data)
{
	struct IDTIMEN *node;
	int n, r;

	assert(idtime);
	n = imp_newnode(&idtime->pnodes);
	if (n < 0) return -1;
	node = (struct IDTIMEN*)IMP_DATA(&idtime->pnodes, n);
	node->data = data;
	node->node = n;
	node->time = idtime->wtime + idtime->timeout;
	if (node->time < idtime->wtime) node->time = 0x7fffffff;
	r = idt_node_add(idtime, n);
	if (r != 0) return -100 + r;
	
	return n;
}

//---------------------------------------------------------------------
// 移出当前计时器
//---------------------------------------------------------------------
int idt_remove(struct IDTIMEV *idtime, int n)
{
	struct IDTIMEN *node;
	assert(idtime != NULL);
	if (idt_node_remove(idtime, n)) return -1;
	node = (struct IDTIMEN*)IMP_DATA(&idtime->pnodes, n);
	imp_delnode(&idtime->pnodes, node->node);
	return 0;
}

//---------------------------------------------------------------------
// 激活当前计时器
//---------------------------------------------------------------------
int idt_active(struct IDTIMEV *idtime, int n)
{
	struct IDTIMEN *node;
	int retval;
	
	assert(idtime != NULL);
	retval = idt_node_remove(idtime, n);
	if (retval) return -10 + retval;
	node = (struct IDTIMEN*)IMP_DATA(&idtime->pnodes, n);
	node->time = idtime->wtime + idtime->timeout;
	if (node->time < idtime->wtime) node->time = 0x7fffffff;
	retval = idt_node_add(idtime, n);
	if (retval) return -100 + retval;
	
	return 0;
}

//---------------------------------------------------------------------
// 获得当前完成的计时器
//---------------------------------------------------------------------
int idt_timeout(struct IDTIMEV *idtime, int *nodev, long *datav)
{
	struct IDTIMEN *node;

	assert(idtime != NULL);
	if (idtime->tail == NULL) return -1;
	if (idtime->wtime < idtime->tail->time) return -1;
	node = idtime->tail;
	idt_node_remove(idtime, node->node);
	if (nodev) *nodev = node->node;
	if (datav) *datav = node->data;
	imp_delnode(&idtime->pnodes, node->node);

	return 0;
}

