#ifndef _cclib_h
#define _cclib_h

#if defined(__APPLE__) && (!defined(__unix))
        #define __unix
#endif

#ifdef __unix
#define XEXPORT  
#elif defined(_WIN32)
#define XEXPORT  __declspec(dllexport)
#endif

/** i think this file should contain the interfaces and funtions that are the fundation of popogame platform but independent of the businiess logicA. 
 *
 */

#ifdef __cplusplus
extern "C" {
#endif

// size of the internal communication header of transmod
#define HEADLEN 12

XEXPORT int cg_attach (const char * ip, unsigned short port, int cid) ;
XEXPORT int cg_peek (int length) ;
XEXPORT void cg_exit (void) ;
XEXPORT void cg_headmode (int headmode) ;

XEXPORT int cg_read (int *event, int *wparam, int *lparam, void *buffer, int nowait) ;
XEXPORT int cg_write (int event, int wparam, int lparam, const void *buffer, int length, int isflush) ;
XEXPORT int cg_sendto (int hid, const void *buffer, int length, int mode) ;
XEXPORT int cg_bsendto (int hid, const void *buffer, int length) ;
XEXPORT int cg_groupcast (const int *hids, int count, const void *data, int size, int limit) ;
XEXPORT int cg_close (int hid, int code) ;
XEXPORT int cg_ioflush (void) ;

XEXPORT int cg_tag (int hid, int tag) ;
XEXPORT int cg_movec (int channel, int hid, const void *data, int length) ;
XEXPORT int cg_channel (int channel, const void *data, int length) ;
XEXPORT int cg_broadcast (int *channels, int count, const void *data, int length) ;
XEXPORT int cg_settimer (int timems) ;
XEXPORT int cg_sysinfo (int type, int info) ;
XEXPORT int cg_book_add (int category) ;
XEXPORT int cg_book_del (int category) ;
XEXPORT int cg_book_reset (void) ;
XEXPORT int cg_head (void) ;
XEXPORT int cg_tail (void) ;
XEXPORT int cg_next (int hid) ;
XEXPORT int cg_prev (int hid) ;
XEXPORT int cg_htag (int hid) ;
XEXPORT int cg_hidnum (void) ;
XEXPORT int cg_getchid (void) ;
XEXPORT int cg_setseed (int hid, int seed);
XEXPORT int cg_rc4_set_skey(int hid, const unsigned char *key, int keysize);
XEXPORT int cg_rc4_set_rkey(int hid, const unsigned char *key, int keysize);

XEXPORT int cg_flush (void) ;
XEXPORT int cg_bufmod (int mode) ;
XEXPORT int cg_gethd (int hid) ;
XEXPORT int cg_sethd (int hid, int hd) ;
XEXPORT int cg_nodelay (int nodelay);
XEXPORT int cg_forbid (int hid);
XEXPORT int cg_enable (int hid, int enable);
XEXPORT int cg_setmask (unsigned int mask);

typedef void (*cg_logger_t) (const char *text) ;

XEXPORT void cg_wlog (int mask, const char *argv, ...) ;
XEXPORT void cg_logsetup (cg_logger_t logger) ;
XEXPORT void cg_logmask (int mask) ;


#ifdef __cplusplus
} //extern "C"
#endif

#endif //_cclib_h




