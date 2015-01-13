#! /usr/bin/env python
# -*- coding: utf-8 -*-
#======================================================================
# 
# ccport.py - cclib port
#
# 初始化服务
#
#======================================================================
import sys, time, struct, os
import ctypes
from ctypes import c_uint, c_int, c_char_p, c_void_p, c_int32, c_uint32
from ctypes import c_int16, c_uint16, c_int8, c_uint8, c_ushort, byref

#----------------------------------------------------------------------
# initialize
#----------------------------------------------------------------------
HOME = os.environ.get('CHOME', os.path.abspath('.'))	# CASUALD 目录
HOME = os.path.abspath(HOME)

def loadlib (fn):
	try: dl = ctypes.cdll.LoadLibrary(fn)
	except: return None
	return dl

_unix = sys.platform[:3] != 'win' and True or False

_search = [ HOME, os.path.join(HOME, 'bin'), '.', '../bin' ]
_names = [ 'cclib', 'cclib.cc', 'cclib.win', 'cclib.pyd', 'cclib.so' ]
_dllname = ''
_cclib = None

for root in _search:
	path = os.path.abspath(root)
	for fn in _names:
		nm = os.path.join(path, fn)
		dl = loadlib(nm)
		if not dl: continue
		try: test = dl.cg_attach
		except: continue
		_cclib = dl
		_dllname = nm
		break

if _cclib == None:
	print 'can not load dynamic library cclib'
	sys.exit(1)


#----------------------------------------------------------------------
# port interface
#----------------------------------------------------------------------
_cg_attach = _cclib.cg_attach
_cg_headmode = _cclib.cg_headmode
_cg_peek = _cclib.cg_peek
_cg_exit = _cclib.cg_exit

_cg_read = _cclib.cg_read
_cg_write = _cclib.cg_write
_cg_sendto = _cclib.cg_sendto
_cg_bsendto = _cclib.cg_bsendto
_cg_groupcast = _cclib.cg_groupcast
_cg_close = _cclib.cg_close
_cg_ioflush = _cclib.cg_ioflush

_cg_tag = _cclib.cg_tag
_cg_movec = _cclib.cg_movec
_cg_channel = _cclib.cg_channel
_cg_broadcast = _cclib.cg_broadcast
_cg_settimer = _cclib.cg_settimer
_cg_sysinfo = _cclib.cg_sysinfo
_cg_logmask = _cclib.cg_logmask
_cg_book_add = _cclib.cg_book_add
_cg_book_del = _cclib.cg_book_del
_cg_book_reset = _cclib.cg_book_reset

_cg_head = _cclib.cg_head
_cg_tail = _cclib.cg_tail
_cg_next = _cclib.cg_next
_cg_prev = _cclib.cg_prev
_cg_htag = _cclib.cg_htag
_cg_hidnum = _cclib.cg_hidnum
_cg_getchid = _cclib.cg_getchid

_cg_setseed = _cclib.cg_setseed
_cg_rc4_set_skey = _cclib.cg_rc4_set_skey
_cg_rc4_set_rkey = _cclib.cg_rc4_set_rkey
_cg_flush = _cclib.cg_flush
_cg_bufmod = _cclib.cg_bufmod
_cg_gethd = _cclib.cg_gethd
_cg_sethd = _cclib.cg_sethd
_cg_setmask = _cclib.cg_setmask


#----------------------------------------------------------------------
# prototypes
#----------------------------------------------------------------------
_cg_attach.argtypes = [ c_char_p, c_ushort, c_int ]
_cg_attach.restype = c_int
_cg_headmode.argtypes = [ c_int ]
_cg_headmode.restype = None
_cg_peek.argtypes = [ c_int ]
_cg_peek.restype = c_int
_cg_exit.argtypes = [ ]
_cg_exit.restype = None

_cg_read.argtypes = [ c_void_p, c_void_p, c_void_p, c_void_p, c_int ]
_cg_read.restype = c_int
_cg_write.argtypes = [ c_int, c_int, c_int, c_char_p, c_int, c_int ]
_cg_write.restype = c_int
_cg_sendto.argtypes = [ c_int, c_char_p, c_int, c_int ]
_cg_sendto.restype = c_int
_cg_bsendto.argtypes = [ c_int, c_char_p, c_int, c_int ]
_cg_bsendto.restype = c_int
_cg_groupcast.argtypes = [ c_void_p, c_int, c_char_p, c_int, c_int ]
_cg_groupcast.restype = c_int
_cg_close.argtypes = [ c_int, c_int ]
_cg_close.restype = c_int
_cg_ioflush.argtypes = []
_cg_ioflush.restype = c_int

_cg_tag.argtypes = [ c_int, c_int ]
_cg_tag.restype = c_int
_cg_movec.argtypes = [ c_int, c_int, c_char_p, c_int ]
_cg_movec.restype = c_int
_cg_channel.argtypes = [ c_int, c_char_p, c_int ]
_cg_channel.restype = c_int
_cg_broadcast.argtypes = [ c_void_p, c_int, c_char_p, c_int ]
_cg_broadcast.restype = c_int
_cg_settimer.argtypes = [ c_int ]
_cg_settimer.restype = c_int
_cg_sysinfo.argtypes = [ c_int, c_int ]
_cg_sysinfo.restype = c_int
_cg_logmask.argtypes = [ c_int ]
_cg_logmask.restype = None
_cg_book_add.argtypes = [ c_int ]
_cg_book_add.restype = c_int
_cg_book_del.argtypes = [ c_int ]
_cg_book_del.restype = c_int
_cg_book_reset.argtypes = []
_cg_book_reset.restype = c_int
_cg_head.argtypes = []
_cg_head.restype = c_int
_cg_tail.argtypes = []
_cg_tail.restype = c_int
_cg_next.argtypes = [ c_int ]
_cg_next.restype = c_int
_cg_prev.argtypes = [ c_int ]
_cg_prev.restype = c_int
_cg_htag.argtypes = [ c_int ]
_cg_htag.restype = c_int
_cg_hidnum.argtypes = []
_cg_hidnum.restype = c_int
_cg_getchid.argtypes = []
_cg_getchid.restype = c_int
_cg_setseed.argtypes = [ c_int, c_int ]
_cg_setseed.restype = c_int
_cg_rc4_set_skey.argtypes = [ c_int, c_char_p, c_int ]
_cg_rc4_set_skey.restype = c_int
_cg_rc4_set_rkey.argtypes = [ c_int, c_char_p, c_int ]
_cg_rc4_set_rkey.restype = c_int

_cg_flush.argtypes = []
_cg_flush.restype = c_int
_cg_bufmod.argtypes = [ c_int ]
_cg_bufmod.restype = c_int
_cg_gethd.argtypes = [ c_int ]
_cg_gethd.restype = c_int
_cg_sethd.argtypes = [ c_int, c_int ]
_cg_sethd.restype = c_int

_cg_setmask.argtypes = [ c_int ]
_cg_setmask.restype = c_int


#----------------------------------------------------------------------
# instruction
#----------------------------------------------------------------------
ITMT_NEW        =  0    # 新近外部连接：(id,tag) ip/d,port/w   <hid>
ITMT_LEAVE      =  1    # 断开外部连接：(id,tag)           <hid>
ITMT_DATA       =  2    # 外部数据到达：(id,tag) data...    <hid>
ITMT_CHANNEL    =  3    # 频道通信：(channel,tag)    <>
ITMT_CHNEW      =  4    # 频道开启：(channel,id)
ITMT_CHSTOP     =  5    # 频道断开：(channel,tag)
ITMT_SYSCD      =  6    # 系统信息：(subtype, v) data...
ITMT_TIMER      =  7    # 系统时钟：(timesec,timeusec)
ITMT_UNRDAT     = 10    # 不可靠数据包：(id,tag)
ITMT_NOOP       = 80    # 空指令：(wparam, lparam)
ITMT_BLOCK      = 99    # 没有指令

ITMC_DATA =        0    # 外部数据发送：(id,*) data...
ITMC_CLOSE =        1    # 关闭外部连接：(id,code)
ITMC_TAG =        2    # 设置TAG：(id,tag)
ITMC_CHANNEL =    3    # 组间通信：(channel,*) data...
ITMC_MOVEC =        4    # 移动外部连接：(channel,id) data...
ITMC_SYSCD =        5    # 系统控制消息：(subtype, v) data...
ITMC_BROADCAST =	6	 # 广播消息
ITMC_UNRDAT =        10    # 不可靠数据包：(id,tag)
ITMC_IOCTL =        11    # 连接控制指令：(id,flag)
ITMC_NOOP =        80    # 空指令：(*,*)

ITMS_CONNC =        0    # 请求连接数量(st,0) cu/d,cc/d
ITMS_LOGLV =        1    # 设置日志级别(st,level)
ITMS_LISTC =        2    # 返回频道信息(st,cn) d[ch,id,tag],w[t,c]
ITMS_RTIME =        3    # 系统运行时间(st,wtime)
ITMS_TMVER =        4    # 传输模块版本(st,tmver)
ITMS_REHID =        5    # 返回频道的(st,ch)
ITMS_QUITD =        6    # 请求自己退出
ITMS_TIMER =        8    # 设置频道零的时钟(st,timems)
ITMS_INTERVAL =        9    # 设置是否为间隔模式(st,isinterval)
ITMS_FASTMODE =        10    # 设置是否启用快速模式

ITMS_NODELAY	=	1	# 设置禁用Nagle算法
ITMS_NOPUSH		=	2	# 禁止发送接口


#----------------------------------------------------------------------
# port interface
#----------------------------------------------------------------------
def attach (ip, port, channel):
	if not _unix:
		import socket
		sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
		sock = None
	return _cg_attach(ip, port, channel)


def headmode (mode = 0):
	_cg_headmode(mode)

def peek (length):
	return _cg_peak(length)

def exit ():
	return _cg_exit()

textdata = ctypes.create_string_buffer('\000' * 0x100000)

def read (nowait = 0):
	p_event = c_int()
	p_wparam = c_int()
	p_lparam = c_int()
	hr = _cg_read(byref(p_event), byref(p_wparam), byref(p_lparam), \
		textdata, nowait)
	e = p_event.value
	w = p_wparam.value
	l = p_lparam.value
	if hr < 0:
		return -1, w, l, ''
	elif hr == 0 and e < 0:
		return 99, w, l, ''
	d = textdata[:hr]
	return e, w, l, d

def write (event, wparam, lparam, data, flush = True):
	flush = flush and 1 or 0
	return _cg_write(event, wparam, lparam, data, len(data), flush)

def sendto (hid, data, udp = False):
	udp = udp and 1 or 0
	return _cg_sendto(hid, data, len(data), udp)

def send (hid, data, udp = False):
	udp = udp and 1 or 0
	return _cg_sendto(hid, data, len(data), udp)

def bsendto (hid, data):
	return _cg_bsendto(hid, data, len(data))

def sendmulti (hids, data, datamode = 0, buffermode = 0):
	if type(hids) == type(''):
		result = []
		unpack = struct.unpack
		for i in xrange(len(hids) / 4):
			hid = unpack('I', hids[i * 4: i * 4 + 4])[0]
			result.append(hid)
		hids = result
	cmd = (datamode) and ITMC_UNRDAT or ITMC_DATA
	for hid in hids:
		_cg_write(cmd, hid, 0, data, len(data), buffermode)
	return 0

def close (hid, code = 0):
	return _cg_close(hid, code)

def tag (hid, TAG):
	return _cg_tag(hid, TAG)

def movec (channel, hid, data):
	return _cg_movec(channel, hid, data, len(data))

def channel (channelid, data):
	return _cg_channel(channelid, data, len(data))

def settimer (millisec):
	return _cg_settimer(millisec)

def sysinfo (mode, info):
	return _cg_sysinfo(mode, info)

def head ():
	return _cg_head()

def tail ():
	return _cg_tail()

def next (hid):
	return _cg_next(hid)

def prev (hid):
	return _cg_prev(hid)

def htag (hid): 
	return _cg_htag(hid)

def hidnum ():
	return _cg_hidnum()

def flush ():
	return _cg_flush()

def bufmod (mode):
	return _cg_bufmod(mode)

def gethd (hid):
	return _cg_gethd(hid)

def sethd (hid, hd):
	return _cg_sethd(hid, hd)

def logmask (mask):
	return _cg_logmask(mask)

def bookadd (category):
	return _cg_book_add(category)

def bookdel (category):
	return _cg_book_del(category)

def bookrst (category):
	return _cg_book_reset()

def getchid ():
	return _cg_getchid()

def setseed (hid, seed):
	return _cg_setseed(hid, seed)

def groupcast (hids, data, limit = -1):
	pack = struct.pack
	count = len(hids)
	hids = ''.join(pack('I', hid) for hid in hids)
	return _cg_groupcast(hids, count, data, len(data), limit)

def rc4_set_rkey (hid, key):
	return _cg_rc4_set_rkey(hid, key, len(key))

def rc4_set_skey (hid, key):
	return _cg_rc4_set_skey(hid, key, len(key))

def setmask (mask):
	return _cg_setmask(mask)


__all__ = [
	'attach', 'headmode', 'peek', 'exit', 'read', 'write', 'send', 'sendto',
	'bsendto', 'sendmulti', 'close', 'tag', 'movec', 'channel', 'settimer',
	'sysinfo', 'head', 'tail', 'next', 'prev', 'htag', 'hidnum', 'flush',
	'bufmod', 'gethd', 'sethd', 'bookadd', 'bookdel', 'bookrst', 'getchid',
	'setseed', 'groupcast', 'rc4_set_skey', 'rc4_set_rkey', 'setmask',
]

#----------------------------------------------------------------------
# testing case
#----------------------------------------------------------------------
if __name__ == '__main__':
	headmode(0)


