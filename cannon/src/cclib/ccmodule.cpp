/** i think this file should contain the interfaces and funtions that are the fundation of popogame platform but independent of the businiess logicA. 
 *
 */
#include <Python.h>
#include "cclib.h"
#include "instruction.h"
#include "sockstrm.h"

#ifdef _WIN32
#include <windows.h>
#endif

#include <list>
#include <string>

static char cc_logcmd[1024] ; 
static void cc_logger(const char *text) ;

static std::list<char*> log_list;
static char cc_logmod = 0;

static unsigned char xbuffer[MAX_PACKET_SIZE];

extern "C" int validate();
extern "C" int shutdown_cclib();


// for crash check
static int ticker = 0;
const char *ticker_func = "";
class CCLibTicker
{
public:
    CCLibTicker(const char* func_name)
    {
        ticker_func = func_name;
        ticker++;
        if(ticker > 10000000) 
            ticker = 1;
    }

    virtual ~CCLibTicker()
    {
        ticker_func = "";
        ticker++;
    }
};


/**for python module interface 
 */
static PyObject *
cc_getticker(PyObject *self, PyObject *args)
{
	return Py_BuildValue("(i,s)", ticker, ticker_func);
}

static PyObject *
cc_attach (PyObject *self, PyObject *args)
{
	char * ip ;
	int port, chl, ret ;

	#ifdef _WIN32
	struct WSAData wsa;
	WSAStartup((unsigned short)0x101, &wsa);
	#endif

	if (validate() != 0) {
		shutdown_cclib();
		return Py_BuildValue("i", -1);
	}

	if (!PyArg_ParseTuple(args, "sii", &ip, &port, &chl)) {
		return NULL;
	}
	Py_BEGIN_ALLOW_THREADS
	ret = cg_attach (ip, (unsigned short)port, chl) ;
	Py_END_ALLOW_THREADS
	return Py_BuildValue("i", ret);
}

static PyObject *
cc_headmode (PyObject *self, PyObject *args)
{
	int mode;
	if (!PyArg_ParseTuple(args, "i", &mode)) {
		return NULL;
	}
	cg_headmode(mode);
	return Py_BuildValue("i", 0);
}

static PyObject *
cc_peek (PyObject *self, PyObject *args)
{
	int length = 0, ret  = 0;


	if (!PyArg_ParseTuple(args, "i", &length)) {
		return NULL;
	}
	ret = cg_peek (length) ;
	return Py_BuildValue("i", ret);
}

static PyObject *
cc_exit (PyObject *self, PyObject *args)
{
	Py_BEGIN_ALLOW_THREADS
	cg_exit() ;
	Py_END_ALLOW_THREADS
	return Py_BuildValue("i", 0) ;	
}


static PyObject *
cc_read (PyObject *self, PyObject *args)
{
    CCLibTicker ticker("cc_read");
	char *data = (char*)xbuffer;
	int w = 0, l = 0, ret = 0;
	int e, nw = 0;
	char *cmd ;

	
	if (!PyArg_ParseTuple(args, "|i", &nw)) {
		return NULL;
	}
	log_list.clear() ;
	cc_logmod = 1 ;

	Py_BEGIN_ALLOW_THREADS
	ret = cg_read (&e, &w, &l, data, nw) ;
	Py_END_ALLOW_THREADS

	if (ret > MAX_PACKET_SIZE) {
		assert(ret <= MAX_PACKET_SIZE);
		fprintf(stderr, "FUCK SIZE\n");
		printf("size=%d\n", ret);
		abort();
	}
	
	cc_logmod = 0;
	for (; log_list.begin() != log_list.end(); ) {
		cmd = *(log_list.begin()) ;
		log_list.pop_front() ;
		PyRun_SimpleString(cmd) ;
		delete cmd ;
	}
	
	if (ret < 0) {
		return Py_BuildValue("iiis#", -1, w, l, "", 0);
	}
	else if (ret == 0 && e < 0) {
		return Py_BuildValue("iiis#", 99, w, l, "", 0);
	}

	// return format: (len of payload, event, wparam, lparam, payload)
	return Py_BuildValue("iiis#", e, w, l, data, ret);
}

static PyObject *
cc_write (PyObject *self, PyObject *args)
{
    CCLibTicker ticker("cc_write");
	int w = 0, l = 0, len = 0, ret = 0;
	int e, flush ;
	char * data ;

	if (!PyArg_ParseTuple(args, "iiis#i", &e, &w, &l, &data, &len, &flush)) {
		return NULL;
	}
	Py_BEGIN_ALLOW_THREADS
	ret = cg_write (e, w, l, data, len, flush) ;
	Py_END_ALLOW_THREADS
	return Py_BuildValue("i", ret);
}

static PyObject *
cc_sendto (PyObject *self, PyObject *args)
{
    CCLibTicker ticker("cc_sendto");
	int hid = 0, len = 0, mod = 0, c = 0;
	char * data ;

	if (!PyArg_ParseTuple(args, "is#|i", &hid, &data, &len, &mod)) {
		return NULL;
	}
	Py_BEGIN_ALLOW_THREADS
	c = cg_sendto (hid, data, len, mod) ;
	Py_END_ALLOW_THREADS
	return Py_BuildValue("i", c);
}

static PyObject *
cc_bsendto (PyObject *self, PyObject *args)
{
	int hid = 0, len = 0, c = 0;
	char * data ;

	if (!PyArg_ParseTuple(args, "is#", &hid, &data, &len)) {
		return NULL;
	}
	Py_BEGIN_ALLOW_THREADS
	c = cg_bsendto(hid, data, len) ;
	Py_END_ALLOW_THREADS
	return Py_BuildValue("i", c);
}

static PyObject *
cc_sendmulti (PyObject *self, PyObject *args)
{
	int hlen = 0, dlen = 0, data_mod = 0, buffer_mod = 0;
	char *hids;
	char *pos;
	char *data ;

	if (!PyArg_ParseTuple(args, "s#s#ii", &hids, &hlen, &data, &dlen, &data_mod, &buffer_mod)) {
		return NULL;
	}

	hlen /= sizeof(int);
	int event = (data_mod == 0)? ITMC_DATA : ITMC_UNRDAT;

	pos = hids;
	Py_BEGIN_ALLOW_THREADS
	for(; hlen>0; hlen--) {
		cg_write(event, *(int *)pos, 0, data, dlen, buffer_mod); 
		pos += sizeof(int);
	}
	Py_END_ALLOW_THREADS
	return Py_BuildValue("i", 0);
}

static PyObject *
cc_close (PyObject *self, PyObject *args)
{
	int hid = 0, code = 0, ret = 0;

	if (!PyArg_ParseTuple(args, "ii", &hid, &code)) {
		return NULL;
	}
	Py_BEGIN_ALLOW_THREADS
	ret = cg_close (hid, code) ;
	Py_END_ALLOW_THREADS
	return Py_BuildValue("i", ret);
}


static PyObject *
cc_tag (PyObject *self, PyObject *args)
{
	int hid = 0, tag = 0, ret = 0;

	if (!PyArg_ParseTuple(args, "ii", &hid, &tag)) {
		return NULL;
	}
	ret = cg_tag (hid, tag) ;
	return Py_BuildValue("i", ret);
}

static PyObject *
cc_movec (PyObject *self, PyObject *args)
{
	int channel = 0, hid = 0, size = 0, ret = 0;
	char*data ;

	if (!PyArg_ParseTuple(args, "iis#", &channel, &hid, &data, &size)) {
		return NULL;
	}
	ret = cg_movec(channel, hid, data, size) ;
	return Py_BuildValue("i", ret);
}

static PyObject *
cc_channel (PyObject *self, PyObject *args)
{
    CCLibTicker ticker("cc_channel");
	int chl = 0, len = 0, ret = 0;
	char * data ;

	if (!PyArg_ParseTuple(args, "is#", &chl, &data, &len)) {
		return NULL;
	}
	Py_BEGIN_ALLOW_THREADS
	ret = cg_channel (chl, data, len) ;
	Py_END_ALLOW_THREADS
	return Py_BuildValue("i", ret);
}

static PyObject *
cc_settimer (PyObject *self, PyObject *args)
{
	int ms = 0, ret = 0;

	if (!PyArg_ParseTuple(args, "i", &ms)) {
		return NULL;
	}
	ret = cg_settimer (ms) ;
	return Py_BuildValue("i", ret);
}

static PyObject *
cc_book_add (PyObject *self, PyObject *args)
{
	int category = 0, ret = 0;

	if (!PyArg_ParseTuple(args, "i", &category)) {
		return NULL;
	}
	ret = cg_book_add (category) ;
	return Py_BuildValue("i", ret);
}

static PyObject *
cc_book_del (PyObject *self, PyObject *args)
{
	int category = 0, ret = 0;

	if (!PyArg_ParseTuple(args, "i", &category)) {
		return NULL;
	}
	ret = cg_book_del (category) ;
	return Py_BuildValue("i", ret);
}

static PyObject *
cc_book_reset (PyObject *self, PyObject *args)
{
	int ret = 0;
	ret = cg_book_reset () ;
	return Py_BuildValue("i", ret);
}

static PyObject *
cc_rc4_skey (PyObject *self, PyObject *args)
{
	char *key = NULL;
	int ret = 0, hid, size = 0;
	if (!PyArg_ParseTuple(args, "is#", &hid, &key, &size)) {
		return NULL;
	}
	ret = cg_rc4_set_skey(hid, (unsigned char*)key, size);
	return Py_BuildValue("i", ret);
}

static PyObject *
cc_rc4_rkey (PyObject *self, PyObject *args)
{
	char *key = NULL;
	int ret = 0, hid, size = 0;
	if (!PyArg_ParseTuple(args, "is#", &hid, &key, &size)) {
		return NULL;
	}
	ret = cg_rc4_set_rkey(hid, (unsigned char*)key, size);
	return Py_BuildValue("i", ret);
}

static PyObject *
cc_setseed (PyObject *self, PyObject *args)
{
	int ret = 0, hid, seed;
	if (!PyArg_ParseTuple(args, "ii", &hid, &seed)) {
		return NULL;
	}
	ret = cg_setseed(hid, seed);
	return Py_BuildValue("i", ret);
}

static PyObject *
cc_getchid (PyObject *self, PyObject *args)
{
	int ret = 0;
	ret = cg_getchid () ;
	return Py_BuildValue("i", ret);
}

static PyObject *
cc_sysinfo (PyObject *self, PyObject *args)
{
	int type = 0, info = 0, ret = 0;

	if (!PyArg_ParseTuple(args, "ii", &type, &info)) {
		return NULL;
	}
	ret = cg_sysinfo (type, info) ;
	return Py_BuildValue("i", ret);
}

static PyObject *
cc_head (PyObject *self, PyObject *args)
{
	int ret ;

	ret = cg_head () ;
	return Py_BuildValue("i", ret);
}

static PyObject *
cc_tail (PyObject *self, PyObject *args)
{
	int ret ;

	ret = cg_tail () ;
	return Py_BuildValue("i", ret);
}

static PyObject *
cc_next (PyObject *self, PyObject *args)
{
	int hid, ret ;

	if (!PyArg_ParseTuple(args, "i", &hid)) {
		return NULL;
	}
	ret = cg_next (hid) ;
	return Py_BuildValue("i", ret);
}

static PyObject *
cc_prev (PyObject *self, PyObject *args)
{
	int hid, ret ;

	if (!PyArg_ParseTuple(args, "i", &hid)) {
		return NULL;
	}
	ret = cg_prev (hid) ;
	return Py_BuildValue("i", ret);
}

static PyObject *
cc_htag (PyObject *self, PyObject *args)
{
	int hid, ret ;

	if (!PyArg_ParseTuple(args, "i", &hid)) {
		return NULL;
	}
	ret = cg_htag (hid) ;
	return Py_BuildValue("i", ret);
}

static PyObject *
cc_hidnum (PyObject *self, PyObject *args)
{
	int ret ;

	ret = cg_hidnum () ;
	return Py_BuildValue("i", ret);
}

static PyObject *
cc_flush (PyObject *self, PyObject *args)
{
	int ret ;
	Py_BEGIN_ALLOW_THREADS
	ret = cg_flush () ;
	Py_END_ALLOW_THREADS
	return Py_BuildValue("i", ret);
}

static PyObject *
cc_bufmod (PyObject *self, PyObject *args)
{
	int mod, ret ;

	if (!PyArg_ParseTuple(args, "i", &mod)) {
		return NULL;
	}
	ret = cg_bufmod (mod) ;
	return Py_BuildValue("i", ret);
}

static PyObject *
cc_gethd (PyObject *self, PyObject *args)
{
	int hid, ret ;

	if (!PyArg_ParseTuple(args, "i", &hid)) {
		return NULL;
	}
	ret = cg_gethd (hid) ;
	return Py_BuildValue("i", ret);
}

static PyObject *
cc_sethd (PyObject *self, PyObject *args)
{
	int hid, hd, ret ;

	if (!PyArg_ParseTuple(args, "ii", &hid, &hd)) {
		return NULL;
	}
	ret = cg_sethd (hid, hd) ;
	return Py_BuildValue("i", ret);
}

static PyObject *
cc_nodelay (PyObject *self, PyObject *args)
{
	int nodelay = 0, ret;
	if (!PyArg_ParseTuple(args, "i", &nodelay)) {
		return NULL;
	}
	ret = cg_nodelay(nodelay);
	return Py_BuildValue("i", ret);
}

static PyObject *
cc_forbid (PyObject *self, PyObject *args)
{
	int hid;
	if (!PyArg_ParseTuple(args, "i", &hid)) {
		return NULL;
	}
	cg_forbid(hid);
	return Py_BuildValue("i", 0);
}

static PyObject *
cc_enable (PyObject *self, PyObject *args)
{
	int hid, e;
	if (!PyArg_ParseTuple(args, "ii", &hid, &e)) {
		return NULL;
	}
	cg_enable(hid, e);
	return Py_BuildValue("i", 0);
}


static PyObject *
cc_wlog (PyObject *self, PyObject *args)
{
	int mask ;
	char*text ;

	if (!PyArg_ParseTuple(args, "is", &mask, &text)) {
		return NULL;
	}
	Py_BEGIN_ALLOW_THREADS
	cg_wlog(mask, text) ;
	Py_END_ALLOW_THREADS
	return Py_BuildValue("i", 0);
}

static PyObject *
cc_logsetup (PyObject *self, PyObject *args)
{
	char *text ;

	if (!PyArg_ParseTuple(args, "s", &text)) {
		return NULL;
	}
	strcpy(cc_logcmd, text) ;
	cg_logsetup(text[0]? cc_logger : NULL) ;

	return Py_BuildValue("i", 0);
}

static PyObject *
cc_logmask (PyObject *self, PyObject *args)
{
	int mask ;

	if (!PyArg_ParseTuple(args, "i", &mask)) {
		return NULL;
	}

	cg_logmask(mask) ;
	return Py_BuildValue("i", 0);
}

static PyObject *
cc_setmask (PyObject *self, PyObject *args)
{
	int mask ;

	if (!PyArg_ParseTuple(args, "i", &mask)) {
		return NULL;
	}

	cg_setmask((unsigned int)mask) ;
	return Py_BuildValue("i", 0);
}

static void cc_logger(const char *text)
{
	static char command[1024] ;
	char *lptr = (char*)text ;
	int i ;

	sprintf(command, "%s(\"", cc_logcmd) ;

	for (i = strlen(command); lptr[0]; lptr++) {
		if (lptr[0] == '\r') command[i++] = '\\', command[i++] = 'r' ;
		else if (lptr[0] == '\n') command[i++] = '\\', command[i++] = 'n' ;
		else if (lptr[0] == '\\') command[i++] = '\\', command[i++] = '\\' ;
		else if (lptr[0] == '"') command[i++] = '\\', command[i++] = '"' ;
		else if (lptr[0] == '\t') command[i++] = '\\', command[i++] = 't' ;
		else command[i++] = lptr[0] ;
	}
	command[i] = 0 ;
	strcat(command, "\")") ;

	if (cc_logmod == 0) {
		PyRun_SimpleString(command) ;
	}	else {
		char *lptr = new char[strlen(command) + 10];
		strcpy(lptr, command);
		log_list.push_back(lptr) ;
	}
}

static PyObject *
cc_groupcast (PyObject *self, PyObject *args)
{
	PyObject *obj;
	char *data = NULL;
	int limit = -1;
	int len = -1;
	int count = 0;
	int *hids = (int*)xbuffer;

	if (!PyArg_ParseTuple(args, "Os#|i", &obj, &data, &len, &limit)) {
		return NULL;
	}

	PyObject *iterator = PyObject_GetIter(obj);
	PyObject *item;

	if (iterator == NULL) {
		return NULL;
	}

	while ((item = PyIter_Next(iterator)) != NULL) {
		hids[count++] = PyInt_AsLong(item);
		Py_DECREF(item);
	}

	Py_DECREF(iterator);

	if (PyErr_Occurred()) {
		return NULL;
	}
	
	if (count > 0) {
		Py_BEGIN_ALLOW_THREADS
		cg_groupcast(hids, count, data, len, limit);
		Py_END_ALLOW_THREADS
	}
	
	Py_INCREF(Py_None);
	return Py_None;
}

static void AddIntConstant(PyObject *module)
{
	PyModule_AddIntConstant(module, "ITMT_NEW", ITMT_NEW) ;
	PyModule_AddIntConstant(module, "ITMT_LEAVE", ITMT_LEAVE) ;
	PyModule_AddIntConstant(module, "ITMT_DATA", ITMT_DATA) ;
	PyModule_AddIntConstant(module, "ITMT_CHANNEL", ITMT_CHANNEL) ;
	PyModule_AddIntConstant(module, "ITMT_CHNEW", ITMT_CHNEW) ;
	PyModule_AddIntConstant(module, "ITMT_CHSTOP", ITMT_CHSTOP) ;
	PyModule_AddIntConstant(module, "ITMT_SYSCD", ITMT_SYSCD) ;
	PyModule_AddIntConstant(module, "ITMT_TIMER", ITMT_TIMER) ;
	PyModule_AddIntConstant(module, "ITMT_UNRDAT", ITMT_UNRDAT) ;
	PyModule_AddIntConstant(module, "ITMT_NOOP", ITMT_NOOP) ;
	PyModule_AddIntConstant(module, "ITMC_DATA", ITMC_DATA) ;
	PyModule_AddIntConstant(module, "ITMC_CLOSE", ITMC_CLOSE) ;
	PyModule_AddIntConstant(module, "ITMC_TAG", ITMC_TAG) ;
	PyModule_AddIntConstant(module, "ITMC_CHANNEL", ITMC_CHANNEL) ;
	PyModule_AddIntConstant(module, "ITMC_MOVEC", ITMC_MOVEC) ;
	PyModule_AddIntConstant(module, "ITMC_SYSCD", ITMC_SYSCD) ;
	PyModule_AddIntConstant(module, "ITMC_UNRDAT", ITMC_UNRDAT) ;
	PyModule_AddIntConstant(module, "ITMC_BROADCAST", ITMC_BROADCAST) ;
	PyModule_AddIntConstant(module, "ITMC_IOCTL", ITMC_IOCTL) ;
	PyModule_AddIntConstant(module, "ITMC_NOOP", ITMC_NOOP) ;
	PyModule_AddIntConstant(module, "ITMS_CONNC", ITMS_CONNC) ;
	PyModule_AddIntConstant(module, "ITMS_LOGLV", ITMS_LOGLV) ;
	PyModule_AddIntConstant(module, "ITMS_LISTC", ITMS_LISTC) ;
	PyModule_AddIntConstant(module, "ITMS_RTIME", ITMS_RTIME) ;
	PyModule_AddIntConstant(module, "ITMS_TMVER", ITMS_TMVER) ;
	PyModule_AddIntConstant(module, "ITMS_REHID", ITMS_REHID) ;
	PyModule_AddIntConstant(module, "ITMS_QUITD", ITMS_QUITD) ;
	PyModule_AddIntConstant(module, "ITMS_NODELAY", ITMS_NODELAY) ;
	PyModule_AddIntConstant(module, "ITMS_TIMER", ITMS_TIMER) ;
	PyModule_AddIntConstant(module, "ITMS_BOOKADD", ITMS_BOOKADD) ;
	PyModule_AddIntConstant(module, "ITMS_BOOKDEL", ITMS_BOOKDEL) ;
	PyModule_AddIntConstant(module, "ITMS_BOOKRST", ITMS_BOOKRST) ;
	PyModule_AddIntConstant(module, "ITMS_STATISTIC", ITMS_STATISTIC) ;
	PyModule_AddIntConstant(module, "ITMS_RC4SKEY", ITMS_RC4SKEY) ;
	PyModule_AddIntConstant(module, "ITMS_RC4RKEY", ITMS_RC4RKEY) ;
}


static PyMethodDef Methods[] = {
	//extern name, inner name, parameter passing , descrpition
	{"attach",  cc_attach, METH_VARARGS, ""},	
	{"headmode", cc_headmode, METH_VARARGS, ""},
	{"peek",  cc_peek, METH_VARARGS, ""},	
	{"exit",  cc_exit, METH_VARARGS, ""},	
	{"read",  cc_read, METH_VARARGS, ""},	
	{"write",  cc_write, METH_VARARGS, ""},	
	{"send",  cc_sendto, METH_VARARGS, ""},
	{"sendto",  cc_sendto, METH_VARARGS, ""},	
	{"bsendto",  cc_bsendto, METH_VARARGS, ""},	
	{"sendmulti", cc_sendmulti, METH_VARARGS, ""},
	{"close",  cc_close, METH_VARARGS, ""},	
	{"tag",  cc_tag, METH_VARARGS, ""},	
	{"movec",  cc_movec, METH_VARARGS, ""},	
	{"channel",  cc_channel, METH_VARARGS, ""},	
	{"settimer",  cc_settimer, METH_VARARGS, ""},	
	{"sysinfo",  cc_sysinfo, METH_VARARGS, ""},	
	{"head",  cc_head, METH_NOARGS, ""},	
	{"tail",  cc_tail, METH_NOARGS, ""},	
	{"next",  cc_next, METH_VARARGS, ""},	
	{"prev",  cc_prev, METH_VARARGS, ""},	
	{"htag",  cc_htag, METH_VARARGS, ""},	
	{"hidnum",  cc_hidnum, METH_NOARGS, ""},	
	{"flush",  cc_flush, METH_NOARGS, ""},	
	{"bufmod",  cc_bufmod, METH_VARARGS, ""},	
	{"gethd",  cc_gethd, METH_VARARGS, ""},	
	{"sethd",  cc_sethd, METH_VARARGS, ""},	
	{"wlog", cc_wlog, METH_VARARGS, "" },
	{"logsetup", cc_logsetup, METH_VARARGS, "" },
	{"setmask", cc_setmask, METH_VARARGS, "" },
	{"logmask", cc_logmask, METH_VARARGS, "" },
	{"bookadd", cc_book_add, METH_VARARGS, "" },
	{"bookdel", cc_book_del, METH_VARARGS, "" },
	{"bookrst", cc_book_reset, METH_NOARGS, "" },
	{"getchid", cc_getchid, METH_NOARGS, "" },
	{"setseed", cc_setseed, METH_VARARGS, "" },
	{"groupcast", cc_groupcast, METH_VARARGS, "" },
	{"rc4_set_rkey", cc_rc4_rkey, METH_VARARGS, "" },
	{"rc4_set_skey", cc_rc4_skey, METH_VARARGS, "" },
	{"forbid", cc_forbid, METH_VARARGS, "" },
	{"enable", cc_enable, METH_VARARGS, "" },
	{"nodelay", cc_nodelay, METH_VARARGS, "" },
    {"getticker", cc_getticker, METH_NOARGS, ""},
	
	{NULL, NULL, 0, NULL}        /* Sentinel */
};


#ifndef PYPY_VERSION
PyMODINIT_FUNC
#else
extern "C" XEXPORT void
#endif
initcclib(void)
{
	PyObject* pyModule ;
	pyModule = Py_InitModule("cclib", Methods) ;
	AddIntConstant(pyModule) ;
}






