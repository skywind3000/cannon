/**********************************************************************
 *
 * impoold.h - node based memory pool class
 * created on Dec. 4 2004 by skywind <skywind3000@hotmail.com>
 * 
 * provide memory pool class with the following methods:
 *
 * imp_init     - initialize the memory pool with the fixed node size
 * imp_destroy  - destroy the memory pool and free the memory pages
 * imp_newnode  - allocate a new node from the pool and return its id
 * imp_delnode  - delete a allocated node 
 * imp_nodehead - get the first allocated node id in the pool
 * imp_nodenext - get the next allocated node id in the pool
 * imp_nodedata - return the node data pointer of a given node
 *
 * the history of this file:
 * Dec. 4  2004   skywind   created including imp_* functions
 * Jul. 26 2005   skywind   modified mem-page align size caculation
 *
 * aim to provide platform independence memory management with ansi c
 * the memory pool is used to allocate/free memory data nodes with the 
 * fixed size. for more information please the comment below.
 *
 **********************************************************************/

#ifndef __I_MPOOLD_H__
#define __I_MPOOLD_H__

#include "icvector.h"

#ifndef __IULONG_DEFINED
#define __IULONG_DEFINED
typedef ptrdiff_t ilong;
typedef size_t iulong;
#endif


/**********************************************************************
 * IMPOOL - memory pool class definition
 * the memory pool is used to allocate/free memory data nodes with the
 * fixed size. the memory-pages in the pool can automatic grow.
 **********************************************************************/
struct IMPOOL
{
    struct IALLOCATOR *allocator;   /* memory allocator        */

    struct IVECTOR vprev;           /* prev node link vector   */
    struct IVECTOR vnext;           /* next node link vector   */
    struct IVECTOR vnode;           /* node information data   */
    struct IVECTOR vdata;           /* node data buffer vector */
    struct IVECTOR vmode;           /* mode of allocation      */
    ilong *mprev;                   /* prev node array         */
    ilong *mnext;                   /* next node array         */
    ilong *mnode;                   /* node info array         */
    void**mdata;                    /* node data array         */
    ilong *mmode;                   /* node mode array         */
    ilong node_free;                /* number of free nodes    */
    ilong node_max;                 /* number of all nodes     */

    int node_size;                  /* node data fixed size    */
    int node_shift;                 /* node data size shift    */

    struct IVECTOR vmem;            /* mem-pages in the pool   */
    char **mmem;                    /* mem-pages array         */
    ilong mem_max;                  /* max num of memory pages */
    ilong mem_count;                /* number of mem-pages     */
	ilong grow_limit;				/* grow limit              */

    ilong list_open;                /* the entry of open-list  */
    ilong list_close;               /* the entry of close-list */
    ilong total_mem;                /* total memory size       */
};


#ifdef __cplusplus
extern "C" {
#endif


/**********************************************************************
 * IMPOOL Interface Definition:
 **********************************************************************/
#define IMP_HEAD(imp)    ((imp)->list_close) /* close-list node head */
#define IMP_NEXT(imp, i) ((imp)->mnext[i])   /* close-list next node */
#define IMP_NODE(imp, i) ((imp)->mnode[i])   /* user tag for a node  */
#define IMP_DATA(imp, i) ((imp)->mdata[i])   /* node data pointer    */



/*
 * imp_init - initialize the memory pool with the fixed node size
 * default memory allocator will be used if the third parameter is NULL.
 */
void imp_init(struct IMPOOL *imp, int nodesize, struct IALLOCATOR*allocator);


/*
 * imp_destroy - destroy the memory pool and free the memory pages
 * the last step is to release all the nodes in the pool
 */
void imp_destroy(struct IMPOOL *imp);


/*
 * imp_newnode - allocate a new node in a pool 
 * returns the id of the new node allocated, blow zero for error
 */
int imp_newnode(struct IMPOOL *imp);


/*
 * imp_delnode - delete a allocated node from the pool
 * no value returned from this function
 */
void imp_delnode(struct IMPOOL *imp, int index);


/*
 * imp_nodehead - get the first allocated node id in the pool
 * returns the id of the first allocated node (first node in close 
 * list), returns -1 if there is no node which has been allocated
 */
int imp_nodehead(const struct IMPOOL *imp);


/*
 * imp_nodenext - get the next allocated node id in the pool
 * returns the id of the next node after the given index node
 */
int imp_nodenext(const struct IMPOOL *imp, int index);


/* 
 * imp_nodedata - get the node-data pointer of a given node
 * returns the pointer to the node data memory block
 */
char *imp_nodedata(const struct IMPOOL *imp, int index);




#ifdef __cplusplus
}
#endif



#endif


