/**********************************************************************
 *
 * icvector.h - automatic memory managed vector array
 * created on Feb. 2 2002 by skywind <skywind3000@hotmail.com>
 *
 * provide two basic class named IALLOCATOR and IVECTOR with methods:
 * 
 * iv_init     - init a vecotr class and setup the memory allocator
 * iv_destroy  - destroy a vector class and free buffer
 * iv_resize   - resize the vector data size in bytes
 *
 * the history of this file:
 * Feb. 2 2002   skywind   created with three basic interfaces.
 *
 * aim to provide data-type independence algorithm implement in ansi c
 * for more information please see the code and comment below.
 *
 **********************************************************************/

#ifndef __I_CVECTOR_H__
#define __I_CVECTOR_H__


/**********************************************************************
 * Global information
 **********************************************************************/
#define IENOMEM        (-1)           /* error: no memory to allocate */
#define IEOTHER        (-2)           /* error: other error occured   */

#include <stddef.h>

typedef size_t isize_t;



/**********************************************************************
 * Definition of memory allocator
 **********************************************************************/
struct IALLOCATOR
{

    void *(*alloc)(struct IALLOCATOR *, isize_t);
    void (*free)(struct IALLOCATOR *, void *);
    void *udata;
    long reserved;
};

/**********************************************************************
 * Definition of IVECTOR
 **********************************************************************/
struct IVECTOR
{
    unsigned char*data;            /* data buffer      */
    isize_t length;                /* length in byte   */
    isize_t block;                 /* block length     */
    struct IALLOCATOR *allocator;  /* memory allocator */
};

#ifdef __cplusplus
extern "C" {
#endif


/**********************************************************************
 * memory allocate and free function pointer, use iv_memory to set
 **********************************************************************/
extern void *(*iv_malloc)(isize_t size);
extern void (*iv_free)(void *mem);


/**********************************************************************
 * Interface of IVECTOR definition
 **********************************************************************/

/*
 * iv_init - init a vecotr class and setup the memory allocator
 * default allocator will be used if the paramete 'allocator' is NULL
 */
void iv_init(struct IVECTOR *v, struct IALLOCATOR *allocator);


/*
 * iv_destroy - destroy a vector class and free memory it used.
 * no return value
 */
void iv_destroy(struct IVECTOR *v);


/*
 * iv_resize - resize the vector data size in bytes
 * returns zero for successful, others for error (IENOMEM/IEOTHER)
 */
int iv_resize(struct IVECTOR *v, isize_t newsize);


/*
 * iv_memory - set default memory allocator method
 * set mode to zero to setup the allocation function, non-zero for free 
 * function. paramete 'function' is the address of the given function
 */
void iv_memory(void *function, int mode);


#ifdef __cplusplus
}
#endif

#endif

