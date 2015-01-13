#include "cclib.h"
#include "instruction.h"
#include "ccmod.h"
#include "sockstrm.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>



extern "C" int cg_recv (int *event, int * wparam, int * lparam, void * data, int nowait) ;
extern "C" int cg_send (int event, int wparam, int lparam, const void * data, int length, int isflush) ;
extern "C" int cg_llattach (const char * ip, unsigned short port, int cid) ;
extern "C" int cg_msend (int event, int wparam, int lparam, const void **ptrs, const int *sizes, int n, int isflush);
extern "C" int cg_socknodelay (int nodelay);

extern "C" void cg_xor_mask (void *data, long size, unsigned char mask);

static cclib::clients clients ;


static long  _buffer_mode = 1 ;		// if _buffer_mode = 1 , flush for each write
static long  _log_mask = 0;			// logmask
static long  _channel_id = -1 ;		// channel id
static unsigned char _xor_mask = 0;
static unsigned char _xor_data[MAX_PACKET_SIZE];
static void(*_log_handler) (const char *text) = NULL ;


#define LOGMASK_CONHID	0x100
#define LOGMASK_DATHID	0x200

//---------------------------------------------------------------------
// attach to the transmod
// -------------------------------------------------------------------
XEXPORT int cg_attach (const char * ip, unsigned short port, int cid) 
{
		int ret ;
		int wp, lp ;
		char buf[256] ;
		int event, nowait ;
		/* establish connection and send login request to transmod */
		if ((ret = cg_llattach (ip, port, cid)) < 0) return ret ;
		/* send a NOOP to check whether sucessful login or not */ 
		cg_write(ITMC_SYSCD, ITMS_CHID, 0, NULL, 0, 1) ;
		nowait = 0 ;
		cg_ioflush() ;
		ret = cg_read (&event, &wp, &lp, buf, nowait) ;
		if (ret < 0) return ret ;
		_channel_id = lp;
		return 0 ;
}
		

//---------------------------------------------------------------------
// cg_getchid
//---------------------------------------------------------------------
XEXPORT int cg_getchid(void)
{
	return _channel_id;
}



//---------------------------------------------------------------------
// cg_read
//---------------------------------------------------------------------
XEXPORT int cg_read(int *event, int *wparam, int *lparam, void *buffer, int nowait)
{
	        
	int ret, again, hid, aret ;
		        
	for (again = 1, ret = 0; ret >= 0 && again; ) {
		again = 0 ;
		ret = cg_recv(event, wparam, lparam, buffer, nowait) ;

		if (0 > ret) {		// connection lost
			cg_wlog(0xffff, "[cclib] channel connection lost") ;
			return ret ;
		}

		//if (ret == 0

		if (0 > *event) {	// no event 
			return ret ;
		}
		
		switch (*event)
		{
		case ITMT_NEW:
			hid = *wparam ;
			aret = clients.add(hid) ;		
			cg_wlog(LOGMASK_CONHID, "[cclib] event=newuser hid=%xh tag=%d", 
				*wparam, *lparam) ;
			break ;

		case ITMT_LEAVE:
			hid = *wparam ;
			if (clients[hid].hid < 0) {
				again = 1 ;
			}	else {
				*lparam = clients[hid].tag ;
				clients.del(hid) ;
			}
			cg_wlog(LOGMASK_CONHID, "[cclib] event=leave hid=%xh tag=%d %s", 
				hid, *lparam, (again)? "discard" : "") ;
			break ;

		case ITMT_UNRDAT:
		case ITMT_DATA:
			hid = *wparam ;
			if (clients.isforbidden(hid)) {
				again = 1 ;
			} else {
				*lparam = clients[hid].tag ;
			}
			if (_log_mask & LOGMASK_DATHID) {
				cg_wlog(LOGMASK_DATHID, "[cclib] event=data hid=%xh tag=%d size=%d %s", 
					hid, *wparam, ret, (again == 0)? "" : "discard") ;
			}
						
			break ;
		};
	}

	if (_xor_mask && event[0] == ITMT_DATA && ret > 0) {
		cg_xor_mask(buffer, ret, _xor_mask);
	}

	return ret ;
}

//---------------------------------------------------------------------
// cg_write
//---------------------------------------------------------------------
// filter out the zero size message to the outer socket . 
// we should not handle this exception here, but i have no good choise !
// if not, the transmod will append two bytes of lenght 
// into the null message and send it to outer handle. that will product a bug! 
//
XEXPORT int cg_write(int event, int wparam, int lparam, const void *buffer, int length, int isflush)
{
	int hid, dret;
	
	switch (event)
	{
	case ITMC_TAG:
		hid = wparam;
		if (clients[wparam].hid == wparam) clients[hid].tag = lparam ;
		break ;
	case ITMC_CLOSE:
		clients[wparam].status |= cclib::client::forbidden ;
		break ;
	case ITMC_MOVEC:
		if (clients[lparam].hid == lparam) {
			dret = clients.del(lparam) ;
			cg_wlog(LOGMASK_CONHID, "[cclib] action=movec hid=%xh channel=%d ok", lparam, wparam) ;
		}	else {
			cg_wlog(LOGMASK_CONHID, "[cclib] action=movec hid=%xh channel=%d norecord", lparam, wparam) ;
		}
		break ;
	}

	// skip the null message for the outer socket
	if ( (NULL == buffer || length == 0) && (event == ITMC_DATA)) {
			return 0 ;
	}

	const void *data = buffer;
	
	if (_xor_mask) {
		if (event == ITMC_DATA) {
			data = _xor_data;
			memcpy(_xor_data, buffer, length);
			cg_xor_mask(_xor_data, length, _xor_mask);
		}
		else if (event == ITMC_BROADCAST) {
			long size = wparam * 4;
			data = _xor_data;
			memcpy(_xor_data, buffer, length);
			if (size < length) {
				cg_xor_mask(_xor_data, length - size, _xor_mask);
			}
		}
	}

	return cg_send(event, wparam, lparam, data, length, isflush) ;
}

//---------------------------------------------------------------------
// cg_sendto
//---------------------------------------------------------------------
XEXPORT int cg_sendto(int hid, const void *buffer, int length, int mode)
{
	int event = (mode == 0)? ITMC_DATA : ITMC_UNRDAT ;
	return cg_write(event, hid, 0, buffer, length, _buffer_mode) ;
}

//---------------------------------------------------------------------
// cg_bsendto
// remark: only send ITMC_DATA , and buffer the data 
//---------------------------------------------------------------------
XEXPORT int cg_bsendto(int hid, const void *buffer, int length)
{
	return cg_write(ITMC_DATA, hid, 0, buffer, length, 0) ;
}

//---------------------------------------------------------------------
// cg_close
//---------------------------------------------------------------------
XEXPORT int cg_close(int hid, int code) 
{
	return cg_write(ITMC_CLOSE, hid, code, NULL, 0, _buffer_mode) ;
}

//---------------------------------------------------------------------
// cg_tag
//---------------------------------------------------------------------
XEXPORT int cg_tag(int hid, int tag)
{
	return cg_write(ITMC_TAG, hid, tag, NULL, 0, _buffer_mode) ;
}

//---------------------------------------------------------------------
// cg_movec
//---------------------------------------------------------------------
XEXPORT int cg_movec(int channel, int hid, const void *data, int length) 
{
	return cg_write(ITMC_MOVEC, channel, hid, data, length, _buffer_mode) ;
}

//---------------------------------------------------------------------
// cg_channel
//---------------------------------------------------------------------
XEXPORT int cg_channel(int channel, const void *data, int length)
{
	return cg_write(ITMC_CHANNEL, channel, 0, data, length, _buffer_mode) ;
}

//---------------------------------------------------------------------
// cg_broadcast
//---------------------------------------------------------------------
XEXPORT int cg_broadcast(int *channels, int count, const void *data, int length)
{
	for (; count > 0; count--) {
		cg_channel(*channels++, data, length) ;
	}
	return 0 ;
}

//---------------------------------------------------------------------
// cg_settimer
//---------------------------------------------------------------------
XEXPORT int cg_settimer(int timems)
{
	return cg_write(ITMC_SYSCD, ITMS_TIMER, timems, NULL, 0, _buffer_mode) ;
}

//---------------------------------------------------------------------
// cg_settimer
//---------------------------------------------------------------------
XEXPORT int cg_book_add (int category)
{
	return cg_write(ITMC_SYSCD, ITMS_BOOKADD, category, NULL, 0, _buffer_mode) ;
}

//---------------------------------------------------------------------
// cg_settimer
//---------------------------------------------------------------------
XEXPORT int cg_book_del (int category)
{
	return cg_write(ITMC_SYSCD, ITMS_BOOKDEL, category, NULL, 0, _buffer_mode) ;
}

//---------------------------------------------------------------------
// cg_settimer
//---------------------------------------------------------------------
XEXPORT int cg_book_reset (void)
{
	return cg_write(ITMC_SYSCD, ITMS_BOOKRST, 0, NULL, 0, _buffer_mode) ;
}

//---------------------------------------------------------------------
// cg_sysinfo
//---------------------------------------------------------------------
XEXPORT int cg_sysinfo(int type, int param)
{
	return cg_write(ITMC_SYSCD, type, param, NULL, 0, _buffer_mode) ;
}

//---------------------------------------------------------------------
// cg_setseed
//---------------------------------------------------------------------
XEXPORT int cg_setseed (int hid, int seed)
{
	return cg_write(ITMC_SEED, hid, (int)seed, NULL, 0, _buffer_mode);
}

//---------------------------------------------------------------------
// cg_rc4_set_skey
//---------------------------------------------------------------------
XEXPORT int cg_rc4_set_skey(int hid, const unsigned char *key, int keysize)
{
	return cg_write(ITMC_SYSCD, ITMS_RC4SKEY, hid, key, keysize, _buffer_mode);
}

//---------------------------------------------------------------------
// cg_rc4_set_rkey
//---------------------------------------------------------------------
XEXPORT int cg_rc4_set_rkey(int hid, const unsigned char *key, int keysize)
{
	return cg_write(ITMC_SYSCD, ITMS_RC4RKEY, hid, key, keysize, _buffer_mode);
}

//---------------------------------------------------------------------
// cg_head
//---------------------------------------------------------------------
XEXPORT int cg_head(void)
{
	return clients.head() ;
}

//---------------------------------------------------------------------
// cg_tail
//---------------------------------------------------------------------
XEXPORT int cg_tail(void)
{
	return clients.tail() ;
}

//---------------------------------------------------------------------
// cg_next
//---------------------------------------------------------------------
XEXPORT int cg_next(int hid)
{
	return clients.next(hid) ;
}

//---------------------------------------------------------------------
// cg_prev
//---------------------------------------------------------------------
XEXPORT int cg_prev(int hid)
{
	return clients.prev(hid) ;
}

//---------------------------------------------------------------------
// cg_htag
//---------------------------------------------------------------------
XEXPORT int cg_htag(int hid)
{
	return clients[hid].tag ;
}

//---------------------------------------------------------------------
// cg_hidnum
//---------------------------------------------------------------------
XEXPORT int cg_hidnum(void)
{
	return clients.size() ;
}

//---------------------------------------------------------------------
// cg_flush
//---------------------------------------------------------------------
XEXPORT int cg_flush(void)
{
	// return cg_write(ITMC_NOOP, 0, 0, NULL, 0, 1) ;
	 return cg_ioflush () ;
}

//---------------------------------------------------------------------
// cg_gethd
//---------------------------------------------------------------------
XEXPORT int cg_gethd(int hid)
{
	return clients[hid].user ;
}

//---------------------------------------------------------------------
// cg_sethd
//---------------------------------------------------------------------
XEXPORT int cg_sethd(int hid, int hd)
{
	clients[hid].user = hd ;
	return 0 ;
}


//---------------------------------------------------------------------
// cg_bufmod
//---------------------------------------------------------------------
XEXPORT int cg_bufmod(int mode)
{
	_buffer_mode = mode ;
	return _buffer_mode ;
}


//---------------------------------------------------------------------
// cg_forbid
//---------------------------------------------------------------------
XEXPORT int cg_forbid(int hid)
{
	clients[hid].status |= cclib::client::forbidden;
	return 0;
}

//---------------------------------------------------------------------
// cg_enable
//---------------------------------------------------------------------
XEXPORT int cg_enable (int hid, int enable)
{
	if (enable) {
		clients.add(hid);
	}	else {
		clients.del(hid);
	}
	return 0;
}


//---------------------------------------------------------------------
// cg_wlog
//---------------------------------------------------------------------
#ifdef _MSC_VER
#pragma warning(disable:4996)
#endif

XEXPORT void cg_wlog(int mask, const char *argv, ...)
{
	static char msg[4097] ;
	va_list argptr ;

	if (_log_handler == NULL) return ;
	if ((mask & _log_mask) == 0 && _log_mask != 0xffff) return ;

	va_start(argptr, argv) ;
	vsprintf(msg, argv, argptr) ;
	va_end(argptr) ;

	_log_handler(msg) ;
}

//---------------------------------------------------------------------
// cg_logsetup
//---------------------------------------------------------------------
XEXPORT void cg_logsetup (cg_logger_t logger) 
{
	_log_handler = logger ;
}

//---------------------------------------------------------------------
// cg_logsetup
//---------------------------------------------------------------------
XEXPORT void cg_logmask(int mask)
{
	_log_mask = mask ;
}


//---------------------------------------------------------------------
// cg_groupcast
//---------------------------------------------------------------------
XEXPORT int cg_groupcast(const int *hids, int count, const void *data, int size, int limit)
{
	const void *vector[2];
	int sizes[2];

	if (limit > 0) {
		limit = limit | 0x40000000;
	}	else {
		limit = 0;
	}

	if (count <= 0) {
		return -1;
	}

	if (data == NULL || size <= 0) {
		data = "";
		size = 0;
	}

	vector[0] = data;
	vector[1] = hids;
	sizes[0] = size;
	sizes[1] = count * 4;

	#if IWORDS_BIG_ENDIAN
	static unsigned char buffer[0x40000];
	unsigned char *ptr = buffer;
	for (int i = 0; i < count; i++) {
		unsigned int hid = hids[i];
		ptr[0] = hid & 0xff;
		ptr[1] = (hid >>  8) & 0xff;
		ptr[2] = (hid >> 16) & 0xff;
		ptr[3] = (hid >> 24) & 0xff;
	}
	p = ptr;
	#endif

	if (_xor_mask) {
		vector[0] = _xor_data;
		memcpy(_xor_data, data, size);
		cg_xor_mask(_xor_data, size, _xor_mask);
	}

	cg_msend(ITMC_BROADCAST, count, limit, vector, sizes, 2, 1);

	return 0;
}

XEXPORT int cg_nodelay (int nodelay)
{
	return cg_socknodelay(nodelay);
}

XEXPORT int cg_setmask (unsigned int mask)
{
	_xor_mask = (unsigned char)mask;
	return 0;
}


//---------------------------------------------------------------------
// function list
//---------------------------------------------------------------------
static const void *intermap[][2] = {
	{ "cg_attach", (void*)cg_attach },
	{ "cg_exit", (void*)cg_exit },
	{ "cg_read", (void*)cg_read },
	{ "cg_write", (void*)cg_write },
	{ "cg_sendto", (void*)cg_sendto },
	{ "cg_bsendto", (void*)cg_bsendto},
	{ "cg_close", (void*)cg_close },
	{ "cg_ioflush", (void*)cg_ioflush },
	{ "cg_tag", (void*)cg_tag },
	{ "cg_movec", (void*)cg_movec },
	{ "cg_channel", (void*)cg_channel },
	{ "cg_broadcast", (void*)cg_broadcast },
	{ "cg_settimer", (void*)cg_settimer },
	{ "cg_sysinfo", (void*)cg_sysinfo },
	{ "cg_head", (void*)cg_head },
	{ "cg_tail", (void*)cg_tail },
	{ "cg_next", (void*)cg_next },
	{ "cg_prev", (void*)cg_prev },
	{ "cg_htag", (void*)cg_htag },
	{ "cg_hidnum", (void*)cg_hidnum },
	{ "cg_flush", (void*)cg_flush },
	{ "cg_bufmod", (void*)cg_bufmod },
	{ "cg_gethd", (void*)cg_gethd },
	{ "cg_sethd", (void*)cg_sethd },
	{ "cg_wlog", (void*)cg_wlog },
	{ "cg_logsetup", (void*)cg_logsetup },
	{ "cg_logmask", (void*)cg_logmask },
	{ "cg_book_add", (void*)cg_book_add },
	{ "cg_book_del", (void*)cg_book_del },
	{ "cg_book_reset", (void*)cg_book_reset },
	{ "cg_getchid", (void*)cg_getchid },
	{ "cg_setseed", (void*)cg_setseed },
	{ "cg_forbid", (void*)cg_forbid },
	{ "cg_enable", (void*)cg_enable },
	{ "cg_groupcast", (void*)cg_groupcast },
	{ "cg_setmask", (void*)cg_setmask },
	{ "cg_rc4_set_skey", (void*)cg_rc4_set_skey },
	{ "cg_rc4_set_rkey", (void*)cg_rc4_set_rkey },
	{ NULL, NULL }
};



//---------------------------------------------------------------------
// function exporter
//---------------------------------------------------------------------
extern "C" XEXPORT const void *exporter(const char *name)
{
	int i;
	for (i = 0; intermap[i][0] != NULL; i++) 
		if (strcmp(name, (char*)intermap[i][0]) == 0) 
			return intermap[i][1];
	return NULL;
}


