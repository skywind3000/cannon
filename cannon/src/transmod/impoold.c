/**********************************************************************
 *
 * impoold.c - node based memory pool class
 * created on Dec. 4 2004 by skywind <skywind3000@hotmail.com>
 * 
 * provide memory pool class with the following methods:
 *
 * imp_init     - initialize the memory pool with the fixed node size
 * imp_destroy  - destroy the memory pool and free buffer
 * imp_newnode  - allocate a new node from the pool and return its id
 * imp_delnode  - delete a allocated node 
 * imp_nodehead - get the first allocated node id in the pool
 * imp_nodenext - get the next allocated node id in the pool
 * imp_nodedata - return the data pointer of a given node
 *
 * the history of this file:
 * Dec. 4  2004   skywind   created including imp_* functions
 * Jul. 26 2005   skywind   modified mem-page align size caculation
 *
 * aim to provide platform independence memory management with ansi c
 * the memory pool is used to allocate/free memory data pages with the 
 * fixed size. for more information please the comment below.
 *
 **********************************************************************/

#include "impoold.h"

#include <stdio.h>
#include <assert.h>


/**********************************************************************
 * private macro definition
 **********************************************************************/
#define IMPN_NODE(imp, i) ((imp)->mnode[i])
#define IMPN_PREV(imp, i) ((imp)->mprev[i])
#define IMPN_NEXT(imp, i) ((imp)->mnext[i])
#define IMPN_DATA(imp, i) ((imp)->mdata[i])
#define IMPN_MODE(imp, i) ((imp)->mmode[i])

#define IMPN_POWER2(n) (1 << (n))

#ifndef IMPN_ROUND
#define IMPN_ROUND   3
#endif

#define IMPN_ALIGN IMPN_POWER2(IMPN_ROUND)


/**********************************************************************
 * IMPOOL operation interface
 **********************************************************************/

/*
 * imp_init - initialize the memory pool with the fixed node size
 * default memory allocator will be used if the third parameter is NULL.
 */
void imp_init(struct IMPOOL *imp, int nodesize, struct IALLOCATOR *allocator)
{
    int newsize;
    int shift;

    assert(imp != NULL);
    imp->allocator = allocator;

    /* initialize the vectors in the pool */
    iv_init(&imp->vprev, allocator);
    iv_init(&imp->vnext, allocator);
    iv_init(&imp->vnode, allocator);
    iv_init(&imp->vdata, allocator);
    iv_init(&imp->vmem, allocator);
    iv_init(&imp->vmode, allocator);

    /* caculate the new size powerd by 2 from the nodesize */
    for (shift = 1; (1 << shift) < nodesize; ) shift++;

    /* caculate the new roundup nodesize */
    newsize = (nodesize < IMPN_ALIGN)? IMPN_ALIGN : nodesize;
    newsize = (newsize + IMPN_ALIGN - 1) & ~(IMPN_ALIGN - 1);

    /* setup the variables in the pool */
    imp->node_size = newsize;
    imp->node_shift = (int)shift;
    imp->node_free = 0;
    imp->node_max = 0;
    imp->mem_max = 0;
    imp->mem_count = 0;
    imp->list_open = -1;
    imp->list_close = -1;
    imp->total_mem = 0;
	imp->grow_limit = -1;
}


/*
 * imp_destroy - destroy the memory pool and free the memory pages
 * the last step is to release all the nodes in the pool
 */
void imp_destroy(struct IMPOOL *imp)
{
    int i;

    assert(imp != NULL);

    /* free the memory pages */
    if (imp->mem_count > 0) {
        for (i = 0; i < imp->mem_count && imp->mmem; i++) {
            if (imp->mmem[i]) {
                if (imp->allocator) {
                    imp->allocator->free(imp->allocator, imp->mmem[i]);
                }    else {
                    iv_free(imp->mmem[i]);
                }
            }
            imp->mmem[i] = NULL;
        }
        imp->mem_count = 0;
        imp->mem_max = 0;
        iv_destroy(&imp->vmem);
        imp->mmem = NULL;
    }

    /* free the nodes */
    iv_destroy(&imp->vprev);
    iv_destroy(&imp->vnext);
    iv_destroy(&imp->vnode);
    iv_destroy(&imp->vdata);
    iv_destroy(&imp->vmode);

    /* reset the data members */
    imp->mprev = NULL;
    imp->mnext = NULL;
    imp->mnode = NULL;
    imp->mdata = NULL;
    imp->mmode = NULL;

    imp->node_free = 0;
    imp->node_max = 0;
    imp->list_open = -1;
    imp->list_close= -1;
    imp->total_mem = 0;
}


/*
 * imp_node_resize - returns zero for successful others for error
 * change the size of node array vector
 */ 
static int imp_node_resize(struct IMPOOL *imp, ilong size)
{
    isize_t size1 = (isize_t)(size * (ilong)sizeof(ilong));
    isize_t size2 = (isize_t)(size * (ilong)sizeof(void*));

    if (iv_resize(&imp->vprev, size1)) return -1;
    if (iv_resize(&imp->vnext, size1)) return -2;
    if (iv_resize(&imp->vnode, size1)) return -3;
    if (iv_resize(&imp->vdata, size2)) return -5;
    if (iv_resize(&imp->vmode, size1)) return -6;

    /* reset the pointer to the current vector buffer */
    imp->mprev = (ilong*)((void*)imp->vprev.data);
    imp->mnext = (ilong*)((void*)imp->vnext.data);
    imp->mnode = (ilong*)((void*)imp->vnode.data);
    imp->mdata =(void**)((void*)imp->vdata.data);
    imp->mmode = (ilong*)((void*)imp->vmode.data);
    imp->node_max = size;

    return 0;
}


/*
 * imp_mem_add - returns zero for successful others for error
 * increase memory page in the pool
 */
static int imp_mem_add(struct IMPOOL *imp, ilong node_count, void **mem)
{
    isize_t newsize;
    char *mpage;

    /* check for increase the size of page array */
    if (imp->mem_count >= imp->mem_max) {
        newsize = (imp->mem_max <= 0)? IMPN_ALIGN : imp->mem_max * 2;
        if (iv_resize(&imp->vmem, newsize * (isize_t)sizeof(void*))) 
            return -1;
        imp->mem_max = newsize;
        imp->mmem = (char**)((void*)imp->vmem.data);
    }

    /* allocate memory for the new memory page */
    newsize = ((ilong)node_count) * imp->node_size;

    if (imp->allocator) {
        mpage = (char*)imp->allocator->alloc(imp->allocator, newsize);
    }    else {
        mpage = (char*)iv_malloc(newsize);
    }

    if (mpage == NULL) {
		return -2;
	}

    /* record the new mem-page */
    imp->mmem[imp->mem_count++] = mpage;
    imp->total_mem += newsize;

    /* save the mem-page address */
    if (mem) *mem = mpage;

    return 0;
}

/*
 * imp_node_grow - returns zero for successful others for error
 * grow node in the pool and add a memory page for the new nodes
 */ 
static int imp_node_grow(struct IMPOOL *imp)
{
    ilong size_start = imp->node_max;
    ilong size_new = 0;
    ilong retval, count, i, j;
    void *mpage;
    char *p;

	/* calculate the new node count need to increase */
	count = (imp->node_max <= IMPN_ALIGN)? IMPN_ALIGN : imp->node_max;

	if (imp->grow_limit > 0 && count > imp->grow_limit)
		count = imp->grow_limit;

	if (count < IMPN_ALIGN) count = IMPN_ALIGN;

	size_new = size_start + count;

    /* increase the memory pool */
    retval = imp_mem_add(imp, count, &mpage);
    if (retval) return -10 + retval;

    /* resize the node array size */
    retval = imp_node_resize(imp, size_new);
    if (retval) return -20 + retval;

    /* initialize each new node added just now */
    p = (char*)mpage;
    for (i = imp->node_max - 1, j = 0; j < count; i--, j++) {
        IMPN_NODE(imp, i) = 0;          /* empty the user reserved data */
        IMPN_MODE(imp, i) = 0;          /* init the node mode    */
        IMPN_DATA(imp, i) = p;          /* setup the data offset */
        IMPN_PREV(imp, i) = -1;         /* add to the open-list  */

        /* add the new node to the open-list */
        IMPN_NEXT(imp, i) = imp->list_open;
        if (imp->list_open >= 0) IMPN_PREV(imp, imp->list_open) = i;
        imp->list_open = i;
        imp->node_free++;
        p += imp->node_size;
    }

    return 0;
}


/*
 * imp_newnode - allocate a new node in a pool 
 * returns the id of the new node allocated, blow zero for error
 */
int imp_newnode(struct IMPOOL *imp)
{
    int node, next;

    assert(imp);

    /* grow the node array and allocate memory if needed */
    if (imp->list_open < 0) {
		int hr = imp_node_grow(imp);
        if (hr) {
			return -2;
		}
    }

    if (imp->list_open < 0 || imp->node_free <= 0) return -3;

    /* pick up the first node in open-list - it is the new node we need */
    node = imp->list_open;
    next = IMPN_NEXT(imp, node);
    if (next >= 0) IMPN_PREV(imp, next) = -1;
    imp->list_open = next;

    /* add it to the close list */
    IMPN_PREV(imp, node) = -1;
    IMPN_NEXT(imp, node) = imp->list_close;

    if (imp->list_close >= 0) IMPN_PREV(imp, imp->list_close) = node;
    imp->list_close = node;
    IMPN_MODE(imp, node) = 1;

    imp->node_free--;

    return node;
}


/*
 * imp_delnode - delete a allocated node from the pool
 * no value returned from this function
 */
void imp_delnode(struct IMPOOL *imp, int index)
{
    int prev, next;

    assert(imp);
    assert((index >= 0) && (index < imp->node_max));
    assert(IMPN_MODE(imp, index));

    /* remove the node from the close-list */
    next = IMPN_NEXT(imp, index);
    prev = IMPN_PREV(imp, index);

    if (next >= 0) IMPN_PREV(imp, next) = prev;
    if (prev >= 0) IMPN_NEXT(imp, prev) = next;
    else imp->list_close = next;

    /* add the node into the open-list as a new node */
    IMPN_PREV(imp, index) = -1;
    IMPN_NEXT(imp, index) = imp->list_open;

    if (imp->list_open >= 0) IMPN_PREV(imp, imp->list_open) = index;
    imp->list_open = index;

    IMPN_MODE(imp, index) = 0;
    imp->node_free++;
}


/*
 * imp_nodehead - get the first allocated node id in the pool
 * returns the id of the first allocated node (first node in close 
 * list), returns -1 if there is no node which has been allocated
 */
int imp_nodehead(const struct IMPOOL *imp)
{
    return (imp)? imp->list_close : -1;
}


/*
 * imp_nodenext - get the next allocated node id in the pool
 * returns the id of the next node after the given index node
 */
int imp_nodenext(const struct IMPOOL *imp, int index)
{
    return (imp)? IMPN_NEXT(imp, index) : -1;
}


/* 
 * imp_nodedata - get the node-data pointer of a given node
 * returns the pointer to the node data memory block
 */
char *imp_nodedata(const struct IMPOOL *imp, int index)
{
    return (char*)IMPN_DATA(imp, index);
}


