#ifndef __CCMOD_H__
#define __CCMOD_H__

#ifdef __cplusplus

#include "instruction.h"
#include <vector>
#include <list>

//=====================================================================
// DETECTION WORD ORDER
//=====================================================================
#ifndef IWORDS_BIG_ENDIAN
    #ifdef _BIG_ENDIAN_
        #if _BIG_ENDIAN_
            #define IWORDS_BIG_ENDIAN 1
        #endif
    #endif
    #ifndef IWORDS_BIG_ENDIAN
        #if defined(__hppa__) || \
            defined(__m68k__) || defined(mc68000) || defined(_M_M68K) || \
            (defined(__MIPS__) && defined(__MISPEB__)) || \
            defined(__ppc__) || defined(__POWERPC__) || defined(_M_PPC) || \
            defined(__sparc__) 
            #define IWORDS_BIG_ENDIAN 1
        #endif
    #endif
    #ifndef IWORDS_BIG_ENDIAN
        #define IWORDS_BIG_ENDIAN  0
    #endif
#endif

namespace cclib {

struct client
{
	enum { forbidden = 1 };
	int hid ;
	int tag ;
	int status ;
	int user ;
	std::list<client*>::iterator iterator ;
};

class clients
{
public:
	clients() {
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
		return _clist.size() ;
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

};

#endif

#endif



