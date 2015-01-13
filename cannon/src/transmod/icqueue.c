/**********************************************************************
 *
 * icqueue.c
 *
 * NOTE: This is a C version queue operation wrapper
 * By Linwei 5/1/2005
 *
 **********************************************************************/

#include "icqueue.h"
#include <stdio.h>
#include <assert.h>


/**********************************************************************
 * init and prepare a queue struct 
 **********************************************************************/
void iv_queue(struct IVQUEUE *queue)
{
	IV_INITQUEUE(queue);
	queue->nodecnt = 0;
}


/**********************************************************************
 * init and prepare a queue node
 **********************************************************************/
void iv_qnode(struct IVQNODE *node, const void *udata)
{
	node->prev = 0;
	node->next = 0;
	node->queue = 0;
	node->udata = (char*)udata;
}


/**********************************************************************
 * add a node to the head
 **********************************************************************/
void iv_headadd(struct IVQUEUE *queue, struct IVQNODE *node)
{
	assert(queue && node);
	assert(node->queue == NULL);

	IV_HEADADD(queue, node);
	node->queue = queue;
	queue->nodecnt++;
}


/**********************************************************************
 * add a node to the tail
 **********************************************************************/
void iv_tailadd(struct IVQUEUE *queue, struct IVQNODE *node)
{
	assert(queue && node);
	assert(node->queue == NULL);

	IV_TAILADD(queue, node);
	node->queue = queue;
	queue->nodecnt++;
}


/**********************************************************************
 * insert src node before(or after) dest node
 **********************************************************************/
void iv_insert(struct IVQNODE *dest, struct IVQNODE *src, int isafter)
{
	assert(src);
	assert(src->queue == NULL);
	
	if (isafter != 0) {
		IV_INSERTA((struct IVQUEUE*)(dest->queue), dest, src);
	}	else {
		IV_INSERTB((struct IVQUEUE*)(dest->queue), dest, src);
	}

	src->queue = dest->queue;
	((struct IVQUEUE*)(dest->queue))->nodecnt++;
}


/**********************************************************************
 * remove a node from a queue
 **********************************************************************/
void iv_remove(struct IVQUEUE *queue, struct IVQNODE *node)
{
	assert(queue && node);
	assert(queue == node->queue);

	IV_REMOVE(queue, node);
	node->queue = NULL;
	queue->nodecnt--;
}


/**********************************************************************
 * pop a node from head
 **********************************************************************/
struct IVQNODE *iv_headpop(struct IVQUEUE *queue)
{
	struct IVQNODE *node = queue->head;
	if (node == NULL) return NULL;
	iv_remove(queue, node);
	return node;
}


/**********************************************************************
 * pop a node from tail
 **********************************************************************/
struct IVQNODE *iv_tailpop(struct IVQUEUE *queue)
{
	struct IVQNODE *node = queue->tail;
	if (node == NULL) return NULL;
	iv_remove(queue, node);
	return node;
}




