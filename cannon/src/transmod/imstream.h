/**********************************************************************
 *
 * imstream.h - memory cache stream definition
 * created on Dec. 4 2004 by skywind <skywind3000@hotmail.com>
 * 
 * provide memory stream class with the following methods:
 *
 * ims_init    - initialize the memory stream with the given mem-pool
 * ims_destroy - destroy the memory stream
 * ims_write   - write some data into the memory stream
 * ims_read    - read data from the stream into the user buffer
 * ims_rptr    - get the read pointer in the current page
 *
 * the history of this file:
 * Dec. 4 2004   skywind   created including ims_* functions
 *
 * aim to provide platform independence memory stream with ansi c
 * the memory stream is used to read/write data with the memory pool
 * in fixed page size. for more information please the comment below.
 *
 **********************************************************************/

#ifndef __I_MSTREAM_H__
#define __I_MSTREAM_H__


#include "impoold.h"



/**********************************************************************
 * IMSTREAM - definition of the memory stream interface
 **********************************************************************/
struct IMSTREAM
{
	ilong page_head;	  /* head page node index in mem-pool  */
	ilong page_tail;	  /* tail page node index in mem-pool  */
	ilong page_size;	  /* page data size in the mem-pool */
	ilong page_cnt;	      /* page count in the mem-pool     */
	ilong pos_read;	      /* current reader pointer         */
	ilong pos_write;	  /* current writer pointer         */
	ilong size;		      /* real data size in the stream   */
	struct IMPOOL *pool;  /* mem-pool struct pointer        */
};


#ifdef __cplusplus
extern "C" {
#endif


/*
 * ims_init - returns zero if successful
 * initialize the memory stream with the given mem-pool
 */
int ims_init(struct IMSTREAM *stream, struct IMPOOL *pool);


/*
 * ims_destroy - always returns zero
 * destroy the memory stream and release the pages to mempool
 */
int ims_destroy(struct IMSTREAM *stream);


/*
 * ims_write - returns how many bytes you really wrote, blow zero for error 
 * write some data into the memory stream
 */
long ims_write(struct IMSTREAM *stream, const void *buf, long size);


/*
 * ims_read - returns how many bytes you really read, blow zero for error 
 * read some data from the memory stream
 */ 
long ims_read(struct IMSTREAM *stream, void *buf, long size);


/*
 * ims_rptr - returns how many bytes can read from the current page
 * get the read pointer in the current page
 */
long ims_rptr(const struct IMSTREAM *stream, void**ptr);

/*
 * ims_peek - returns how many bytes you really read, blow zero for error
 * peek some data from the memory stream
 */
long ims_peek(struct IMSTREAM *stream, void *buf, long size);

/*
 * ims_drop - returns how many bytes you really read, blow zero for error
 * drop some data from the memory stream
 */
long ims_drop(struct IMSTREAM *stream, long size);



#ifdef __cplusplus
}
#endif


#endif



