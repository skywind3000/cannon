//=====================================================================
//
// ccmod.h - cc module interfaces
//
// NOTE:
// for more information, please see the readme file.
//
//=====================================================================

#ifndef __INTERNAL_H__
#define __INTERNAL_H__

#include "sockstrm.h"
#include "instruction.h"

#ifndef NAMESPACE_BEGIN
#define NAMESPACE_BEGIN(x) namespace x {
#endif

#ifndef NAMESPACE_END
#define NAMESPACE_END(x) }
#endif

#include <vector>
#include <list>


//=====================================================================
// 头部模式
//=====================================================================
#define ITMH_WORDLSB	0		// 头部标志：2字节LSB
#define ITMH_WORDMSB	1		// 头部标志：2字节MSB
#define ITMH_DWORDLSB	2		// 头部标志：4字节LSB
#define ITMH_DWORDMSB	3		// 头部标志：4字节MSB
#define ITMH_BYTELSB	4		// 头部标志：单字节LSB
#define ITMH_BYTEMSB	5		// 头部标志：单字节MSB
#define ITMH_EWORDLSB	6		// 头部标志：2字节LSB（不包含自己）
#define ITMH_EWORDMSB	7		// 头部标志：2字节MSB（不包含自己）
#define ITMH_EDWORDLSB	8		// 头部标志：4字节LSB（不包含自己）
#define ITMH_EDWORDMSB	9		// 头部标志：4字节MSB（不包含自己）
#define ITMH_EBYTELSB	10		// 头部标志：单字节LSB（不包含自己）
#define ITMH_EBYTEMSB	11		// 头部标志：单字节MSB（不包含自己）
#define ITMH_DWORDMASK	12		// 头部标志：4字节LSB（包含自己和掩码）
#define ITMH_RAWDATA	13		// 头部标志：无头对内4字节LSB（含自己）


NAMESPACE_BEGIN(cclib)

//=====================================================================
// ccsocket
//=====================================================================
class ccsocket
{
public:
	long cg_recv(int *event, long *wparam, long *lparam, void *data, int nowait);
	long cg_send(int event, long wparam, long lparam, const void *data, long length, int isflush);
	long cg_send(int event, long wparam, long lparam, const void **ptrs, const long *sizes, int n, int isflush);

	int attach(const char *ip, int port, int cid, int headmode = 0, long bufsize = 0x10000);
	void detach();
	int handle();

	void *temp();

public:
	ccsocket();
	virtual ~ccsocket();

protected:
	void initialize();
	void destroy();

	int setup(int headmode = 0, long bufsize = 0x10000);

	void _size_set(void *p, long size);
	long _size_get(const void *p);
	void _write_dword(void *ptr, unsigned long dword);
	void _write_word(void *ptr, unsigned short word);
	unsigned long _read_dword(const void *ptr);
	unsigned short _read_word(const void *ptr);

	int ioflush();
	long iorecv(void * data, long length, int flag);
	long iosend(const void * data, long length, int isflush);
	long peek(long length);

protected:
	tiny_socket::sock_stream sock;
	ICACHED _recvcache;
	ICACHED _sendcache;
	char *_recvbuff;
	char *_sendbuff;
	char *_tempbuff;
	long _buffsize;
	int _headmod;
	int _headint;
	int _headinc;
	int _headlen;
	int _hdrsize;
};



//=====================================================================
// client
//=====================================================================
struct client
{
	enum { forbidden = 1 };
	int hid ;
	int tag ;
	int status ;
	int user ;
	std::list<client*>::iterator iterator ;
};


//=====================================================================
// clients
//=====================================================================
class clientmap
{
public:
	clientmap() {
		_dummy.hid = -1 ;
		_dummy.tag = -1 ;
	}
	client& operator[](int hid) {
		int index = (hid & 0xFFFF) ;
		if ((int)_cmap.size() <= index) return _dummy ;
		if (_cmap[index].hid != hid) return _dummy ;
		return _cmap[index] ;
	}
	int add(int hid) {
		int index = (hid & 0xFFFF) ;
		for (; (int)_cmap.size() <= index; ) {
			_cmap.push_back(_dummy) ;
		}
		if (0 <= _cmap[index].hid) return -1 ;
		_cmap[index].hid = hid ;
		_cmap[index].tag = -1 ;
		_cmap[index].status = 0 ;
		_clist.push_front(&_cmap[index]) ;
		_cmap[index].iterator = _clist.begin() ;
		
		return hid ;
	}
	int del(int hid) {
		int index = (hid & 0xFFFF) ;
		if ((int)_cmap.size() <= index) return -1 ;
		if (_cmap[index].hid != hid) return -1 ;
		_cmap[index].hid = -1 ;
		_cmap[index].tag = -1 ;
		_clist.erase(_cmap[index].iterator) ;
		return hid ;
	}
	void clear() {
		_cmap.clear() ;
		_clist.clear() ;
	}
	int isforbidden(int hid) {
		int index = (hid & 0xFFFF) ;
		if ((int)_cmap.size() <= index) return -1 ;
		if (_cmap[index].hid != hid) return -1 ;
		return (_cmap[index].status & client::forbidden) ;
	}
	int size() {
		return (int)_clist.size() ;
	}
	int head() {
		if (_clist.empty()) return -1 ;
		return (*_clist.begin())->hid ;
	}
	int tail() {
		if (_clist.empty()) return -1 ;
		std::list<client*>::iterator iterator = _clist.end() ;
		iterator-- ;
		return (*iterator)->hid ;
	}
	int next(int hid) {
		int index = (hid & 0xFFFF) ;
		if ((int)_cmap.size() <= index) return -1 ;
		if (_cmap[index].hid != hid) return -1 ;
		std::list<client*>::iterator iterator = _cmap[index].iterator ;
		iterator++ ;
		if (iterator == _clist.end()) return -1 ;
		return (*iterator)->hid ;
	}
	int prev(int hid) {
		int index = (hid & 0xFFFF) ;
		if ((int)_cmap.size() <= index) return -1 ;
		if (_cmap[index].hid != hid) return -1 ;
		std::list<client*>::iterator iterator = _cmap[index].iterator ;
		if (iterator == _clist.begin()) return -1 ;
		iterator--;
		return (*iterator)->hid ;
	}
protected:
	std::vector<client> _cmap ;
	std::list<client*> _clist ;
	client _dummy ;
};



//=====================================================================
// CCLIB
//=====================================================================
class CCLIB
{
public:
	int attach(const char *ip, int port, int cid, int headmode);
	int detach();

	int channel_id();

	int read(int *event, int *wparam, int *lparam, void *buffer, int nowait = 0);
	int write(int event, int wparam, int lparam, const void *buffer, int length, int isflush = 1);

	int cg_sendto(int hid, const void *buffer, int length, int mode = 0);
	int cg_sendbuf(int hid, const void *buffer, int length);
	int cg_sendlmt(int hid, const void *buffer, int length, int limit = -1);

	int cg_groupcast(const int *hidlist, int count, const void *buffer, int length, int limit = -1);

	int cg_close(int hid);
	int cg_tag(int hid, int tag);
	int cg_movec(int channel, int hid, const void *data, int length);
	int cg_channel(int channel, const void *data, int length);
	int cg_broadcast(int *channels, int count, const void *data, int length);
	int cg_settimer(int timems);

	int book_add(int category);
	int book_del(int category);
	int book_reset(void);

	int cg_sysinfo(int type, int param);
	int cg_head(void);
	int cg_tail(void);
	int cg_next(int hid);
	int cg_prev(int hid);

	int cg_htag(int hid);
	int cg_hidnum(void);
	int cg_flush(void);
	int cg_gethd(int hid);
	int cg_sethd(int hid, int hd);
	int cg_bufmod(int mode);


protected:
protected:
	int _buffer_mode;
	int _log_mask;
	int _filter;
	int _head_mode;
	int _channel_id;
	ccsocket _netio;
	clientmap _clients;
};


NAMESPACE_END(cclib)



#endif


