/**********************************************************************
 *
 * imstream.c - memory cache stream definition
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

#include "impoold.h"
#include "imstream.h"

#include <stdio.h>
#include <string.h>
#include <assert.h>


/*
 * ims_init - returns zero if successful
 * initialize the memory stream with the given mem-pool
 */
int ims_init(struct IMSTREAM *stream, struct IMPOOL *pool)
{
	ilong page;

	assert(stream && pool);

	stream->pool = NULL;
	page = imp_newnode(pool);

	if (page < 0) return -1;
	stream->pool = pool;
	
	IMP_NODE(pool, page) = -1;
	stream->page_head = page;
	stream->page_tail = page;
	stream->page_cnt = 1;
	stream->pos_read = 0;
	stream->pos_write = 0;
	stream->size = 0;
	stream->page_size = pool->node_size;

	return 0;
}


/*
 * ims_destroy - always returns zero
 * destroy the memory stream and release the pages to mempool
 */
int ims_destroy(struct IMSTREAM *stream)
{
	struct IMPOOL *pool;
	ilong i, n;
	assert(stream);
	assert(stream->pool);
	pool = stream->pool;
	for (i = stream->page_tail; i >= 0; i = n) {
		n = IMP_NODE(pool, i);
		imp_delnode(pool, i);
	}
	stream->page_head = -1;
	stream->page_tail = -1;
	stream->page_cnt = 0;
	stream->page_size = 0;
	stream->pos_read = 0;
	stream->pos_write = 0;
	stream->pool = NULL;
	stream->size = 0;
	return 0;
}


/*
 * ims_write - returns how many bytes you really wrote, blow zero for error 
 * write some data into the memory stream
 */
long ims_write(struct IMSTREAM *stream, const void *buf, long size)
{
	ilong total = 0, canwrite, towrite, p;
	char *lptr = (char*)buf;
	char *mem;
	struct IMPOOL *pool;

	assert(stream);
	assert(stream->pool);
	pool = stream->pool;

	if (size == 0) return 0;

	for (; size > 0; total += towrite, size -= towrite, lptr += towrite) {
		assert(stream->pos_write <= stream->page_size);

		// write in a page
		canwrite = stream->page_size - stream->pos_write;
		towrite = (canwrite < size)? canwrite : size;
		mem = (char*)IMP_DATA(pool, stream->page_head);
		memcpy(mem + stream->pos_write, lptr, (size_t)towrite);
		stream->size += towrite;
		stream->pos_write += towrite;

		// whether to allocate new page
		if (stream->pos_write >= stream->page_size) {
			p = imp_newnode(pool);
			assert(p >= 0);
			if (p < 0) {
				break;
			}
			IMP_NODE(pool, stream->page_head) = p;
			IMP_NODE(pool, p) = -1;
			stream->page_head = p;
			stream->pos_write = 0;
			stream->page_cnt++;
		}
	}

	return (long)total;
}


/*
 * ims_read - returns how many bytes you really read, blow zero for error 
 * read some data from the memory stream
 */ 
long ims_read(struct IMSTREAM *stream, void *buf, long size)
{
	ilong total = 0, canread, toread, page_tail, pos_read, p, r;
	ilong mpeek = (size >= 0)? 0 : 1; /* whether to use peek */
	struct IMPOOL *pool;
	char *lptr;
	char *mem;

	assert(stream);
	assert(stream->pool);

	pool = stream->pool;
	page_tail = stream->page_tail;
	pos_read = stream->pos_read;

	size = (size > 0)? size : (-size);
	lptr = (char*)buf;
	r = 0;

	if (size == 0) return 0;

	if (buf != NULL) {
		for (; size > 0; total += toread, size -= toread, lptr += toread) {
			assert(pos_read <= stream->page_size);

			// read in a page
			canread = (page_tail == stream->page_head)? 
						stream->pos_write - pos_read : 
						stream->page_size - pos_read;
			toread = (canread < size)? canread : size;
			mem = (char*)IMP_DATA(pool, page_tail);
			if (toread == 0) break;
			memcpy(lptr, mem + pos_read, (size_t)toread);
			pos_read += toread;
			if (mpeek == 0) stream->size -= toread;

			// free pages;
			if (pos_read >= stream->page_size) {
				p = IMP_NODE(pool, page_tail);
				if (mpeek == 0) {
					imp_delnode(pool, page_tail);
					stream->page_cnt--;
				}
				page_tail = p;
				pos_read = 0;
			}
		}
	}	else if (mpeek == 0) {
		for (; size > 0; total += toread, size -= toread, lptr += toread) {
			assert(pos_read <= stream->page_size);
			canread = (page_tail == stream->page_head)? 
						stream->pos_write - pos_read : 
						stream->page_size - pos_read;
			toread = (canread < size)? canread : size;
			if (toread == 0) break;
			pos_read += toread;
			stream->size -= toread;
			if (pos_read >= stream->page_size) {
				p = IMP_NODE(pool, page_tail);
				imp_delnode(pool, page_tail);
				stream->page_cnt--;
				page_tail = p;
				pos_read = 0;
			}
		}		
	}
	if (mpeek == 0) {
		stream->pos_read = pos_read;
		stream->page_tail = page_tail;
	}
	mpeek = r;

	return (long)total;
}


/*
 * ims_rptr - returns how many bytes can read from the current page
 * get the read pointer in the current page
 */
long ims_rptr(const struct IMSTREAM *stream, void**ptr)
{
	ilong canread = 0;

	assert(stream);
	assert(stream->pool);

	if (stream->size == 0) return 0;

	canread = (stream->page_tail == stream->page_head)? 
						stream->pos_write - stream->pos_read : 
						stream->page_size - stream->pos_read;

	if (ptr) *ptr = (char*)IMP_DATA(stream->pool, stream->page_tail) + 
					stream->pos_read;

	return (long)canread;
}


/*
 * ims_peek - returns how many bytes you really read, blow zero for error
 * peek some data from the memory stream
 */
long ims_peek(struct IMSTREAM *stream, void *buf, long size)
{
	return ims_read(stream, buf, -size);
}


/*
 * ims_drop - returns how many bytes you really read, blow zero for error
 * drop some data from the memory stream
 */
long ims_drop(struct IMSTREAM *stream, long size)
{
	return ims_read(stream, NULL, size);
}



