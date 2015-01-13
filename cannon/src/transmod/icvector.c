/**********************************************************************
 *
 * icvector.c - automatic memory managed vector array
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

#include "icvector.h"

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>


/**********************************************************************
 * allocator function pointers
 **********************************************************************/
static void*iv_dmalloc(isize_t size);  /* default malloc function */
static void iv_dfree(void *mem);       /* default free function   */

typedef void*(*iv_tmalloc)(isize_t);   /* type of malloc function */
typedef void (*iv_tfree)(void*);       /* type of free function   */


/**********************************************************************
 * memory allocate and free function pointer, use iv_memory to set
 **********************************************************************/
void *(*iv_malloc)(isize_t size) = iv_dmalloc; /* malloc pointer */
void (*iv_free)(void *mem) = iv_dfree;         /* free pointer   */

static void*iv_dmalloc(isize_t size) { return malloc((size_t)size); }
static void iv_dfree(void *mem) { if (mem) free((char*)mem); }                 

static void *iv_calloc(struct IALLOCATOR *allocator, isize_t size);
static void  iv_cfree(struct IALLOCATOR *allocator, void *mem);


/**********************************************************************
 * Interface of IVECTOR definition
 **********************************************************************/

/*
 * iv_init - init a vecotr class and setup the memory allocator
 * default allocator will be used if the paramete 'allocator' is NULL
 */
void iv_init(struct IVECTOR *v, struct IALLOCATOR *allocator)
{
    if (v == NULL) return;

    v->data = NULL;
    v->length = 0;
    v->block = 0;
    v->allocator = NULL;
    v->allocator = allocator;
}

/*
 * iv_destroy - destroy a vector class and free memory it used.
 * no return value
 */
void iv_destroy(struct IVECTOR *v)
{
    if (v == NULL) return;

    if (v->data) {
        iv_cfree(v->allocator, v->data);
    }

    v->data = NULL;
    v->length = 0;
    v->block = 0;
}

/*
 * iv_resize - resize the vector data size in bytes
 * returns zero for successful, others for error (IENOMEM/IEOTHER)
 */
int iv_resize(struct IVECTOR *v, isize_t newsize)
{
    unsigned char *lptr;
    isize_t block, min;
    unsigned long nblock;

    if (v == NULL) return IEOTHER;

    /* check if it is really need to resize */
    if (newsize > v->length && newsize <= v->block) { 
        v->length = newsize; 
        return 0; 
    }
    
    /* find equal size powered of 2 to the new size */
    for (nblock = 1; (isize_t)nblock < newsize; ) nblock <<= 1;
    block = (isize_t)nblock;

    /* check if it is really need to change block size */
    if (block == v->block) { 
        v->length = newsize; 
        return 0; 
    }

    /* reallocate the memory and copy the data */
    if (v->block == 0 || v->data == NULL) {    /* allocate new */
        v->data = (unsigned char*)iv_calloc(v->allocator, block);
        if (v->data == NULL) return IENOMEM;

        /* update data members of the vector */
        v->length = newsize;
        v->block = block;
    }    else {            /* reallocate buffer for the vector */
        lptr = (unsigned char*)iv_calloc(v->allocator, block);
        if (lptr == NULL) return IENOMEM;
        
        /* copy data and free the old memory block */
        min = (v->length <= newsize)? v->length : newsize;
        memcpy(lptr, v->data, (size_t)min);
        iv_cfree(v->allocator, v->data);

        /* update data members of the vector */
        v->data = lptr;
        v->length = newsize;
        v->block = block;
    }

    return 0;

}


/**********************************************************************
 * call memory allocator to alloc one block
 **********************************************************************/
static void *iv_calloc(struct IALLOCATOR *allocator, isize_t size)
{
    if (allocator) 
        return allocator->alloc(allocator, size);
    return iv_malloc(size);
}

/**********************************************************************
 * call memory allocator to free one block
 **********************************************************************/
static void  iv_cfree(struct IALLOCATOR *allocator, void *mem)
{
    if (allocator) {
        allocator->free(allocator, mem);
        return;
    }
    iv_free(mem);
}

/*
 * iv_memory - set default memory allocator method
 * set mode to zero to setup the allocation function, non-zero for free 
 * function. paramete 'function' is the address of the given function
 */
void iv_memory(void *function, int mode)
{
    union { void *ptr; iv_tmalloc fun; } fun1;
    union { void *ptr; iv_tfree fun; } fun2;

    fun1.ptr = function;
    fun2.ptr = function;

    switch (mode)
    {
    case 0:
        if (function == NULL) iv_malloc = (iv_tmalloc)iv_dmalloc;
        else iv_malloc = fun1.fun;
        break;
    case 1:
        if (function == NULL) iv_free = (iv_tfree)iv_dfree;
        else iv_free = fun2.fun;
        break;
    }
}

