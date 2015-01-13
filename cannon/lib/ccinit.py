#! /usr/bin/env python
# -*- coding: utf-8 -*-
#======================================================================
# 
# ccinit.py
#
# 初始化服务
#
#======================================================================
import sys, time, struct, os, subprocess

try:
	import cclib		# 如果使用 Python则正常使用 cclib
	CCLIB = 'cclib'
except:
	import cport		# 如果使用 pypy等，则用 cport代替 cclib
	sys.modules['cclib'] = cport
	import cclib
	CCLIB = 'cport'


#----------------------------------------------------------------------
# 取得运行环境
#----------------------------------------------------------------------
HOME = os.path.abspath(os.path.join(os.path.split(__file__)[0], '../'))
HOME = os.path.abspath(os.environ.get('CHOME', HOME))	# CASUALD 目录
DAEMON = int(os.environ.get('DAEMON', '0'), 0)			# 是否DAEMON模式
SERVICE = os.environ.get('SERVICE', '')					# 服务名称
HEADER = int(os.environ.get('HEADER', '0'), 0)			# 头部模式

CONFIG = os.environ.get('CONFIG', '')					# 配置文件
CHADDR = os.environ.get('CHADDR', '')					# 内部监听地址
PORTC = int(os.environ.get('PORTC', '-1'))				# 内部监听端口
PORTU = int(os.environ.get('PORTU', '-1'))				# 外部监听端口
PORTD = int(os.environ.get('PORTD', '-1'))				# 外部数据报端口

LOG_PREFIX = 'm'										# 日志前缀
CHANNEL = -1											# 频道编号
ENCODING = sys.stdout.encoding							# 输出编码
STDOUT = sys.stdout										# 默认输出

environ = { 'HOME': HOME, 'DAEMON': DAEMON, 'SERVICE':SERVICE,
	'HEADER': HEADER, 'CONFIG': CONFIG, 'PORTC': PORTC, 'PORTU': PORTU,
	'PORTD': PORTD }


#----------------------------------------------------------------------
# 取得路径：相对 casuald目录
#----------------------------------------------------------------------
def pathrel (*args):
	path = os.path.join(HOME, *args)
	return os.path.abspath(path)


#----------------------------------------------------------------------
# 日志输出
#----------------------------------------------------------------------
def plog(*args):
	try:
		now = time.strftime('%Y-%m-%d %H:%M:%S')
		date = now.split(None, 1)[0].replace('-', '')
		logfile = sys.modules[__name__].__dict__.get('logfile', None)
		logtime = sys.modules[__name__].__dict__.get('logtime', '')
		if date != logtime:
			logtime = date
			if logfile: logfile.close()
			logfile = None
		if logfile == None:
			logname = 'logs/%s/%s%s.log'%(SERVICE, LOG_PREFIX, date)
			logfile = open(os.path.join(HOME, logname), 'a')
		sys.modules[__name__].__dict__['logtime'] = logtime
		sys.modules[__name__].__dict__['logfile'] = logfile
		def strlize(x):
			if isinstance(x, unicode):
				return x.encode("utf-8")
			return str(x)
		str_args = map(strlize, args)
		text = " ".join(str_args)
		logfile.write('[%s] %s\n'%(now, text))
		logfile.flush()
		if DAEMON == 0:
			text = '[%s] %s\n'%(now, text)
			#STDOUT.write(text.decode('utf-8'))
			STDOUT.write(text)
	except Exception:
		pass
	return 0


#----------------------------------------------------------------------
# 重新定义输出
#----------------------------------------------------------------------
class output_handler:
	def __init__(self, writer):
		self.writer = writer
		self.content = ''
	def flush(self):
		pass
	def write(self, s):
		if isinstance(s, unicode):
			s = s.encode("utf-8")
		self.content += s
		pos = self.content.find('\n')
		if pos < 0: return
		self.writer(self.content[:pos])
		self.content = self.content[pos + 1:]
	def writelines(self, l):
		map(self.write, l)


#----------------------------------------------------------------------
# 登录网络
#----------------------------------------------------------------------
def attach(channel = 0):
	global HOME, CHANNEL
	try:
		cfg_remote = os.environ['CHADDR']
		cfg_portc = int(os.environ['PORTC'])
		cfg_portu = int(os.environ['PORTU'])
	except:
		print 'it must be started by casuald'
		sys.exit(-1)
	text = 'startup with %s:%d channel=%d header=%d'%(cfg_remote, cfg_portc, channel, HEADER)
	plog(text)
	cclib.headmode(HEADER)
	retval = cclib.attach(cfg_remote, cfg_portc, channel)
	if retval != 0:
		plog('attach failed %d'%retval)
		print 'attach failed', retval
		sys.exit(-2)
	sys.stdout = output_handler(plog)
	sys.stderr = output_handler(plog)
	CHANNEL = cclib.getchid()
	fn = sys.modules['__main__'].__dict__.get('__file__', 'unknow')
	environ_set('channel.%d.pid'%CHANNEL, os.getpid())
	environ_set('channel.%d.name'%CHANNEL, fn)
	return retval


def parseip(data):
	if len(data) != 6:
		return ('0.0.0.0', -1)
	remote = '.'.join([ str(ord(n)) for n in data[:4] ])
	port = struct.unpack('>H', data[4:6])[0]
	return (remote, port)


#----------------------------------------------------------------------
# 输出二进制
#----------------------------------------------------------------------
def print_binary(data, char = False):
	content = ''
	charset = ''
	lines = []
	for i in xrange(len(data)):
		ascii = ord(data[i])
		if i % 16 == 0: content += '%04X  '%i
		content += '%02X'%ascii
		content += ((i & 15) == 7) and '-' or ' '
		if (ascii >= 0x20) and (ascii < 0x7f): charset += data[i]
		else: charset += '.'
		if (i % 16 == 15): 
			lines.append(content + ' ' + charset)
			content, charset = '', ''
	if len(content) < 56: content += ' ' * (54 - len(content))
	lines.append(content + ' ' + charset)
	limit = char and 100 or 54
	for n in lines: print n[:limit]
	return 0



#----------------------------------------------------------------------
# 输出调用栈
#----------------------------------------------------------------------
def print_traceback():
	import cStringIO, traceback
	sio = cStringIO.StringIO()
	traceback.print_exc(file = sio)
	for line in sio.getvalue().split('\n'):
		print line
	return 0


#----------------------------------------------------------------------
# 设置网络参数
#----------------------------------------------------------------------
from instruction import *

def ioctl(hid, mode, value):
	if value == True: value = 1
	elif value == False: value = 0
	value = value
	lparam = (value << 4) | mode
	cclib.write(ITMC_IOCTL, hid, lparam, '', 1)

def set_nodelay(hid, IsNoDelay = False):
	ioctl(hid, ITMS_NODELAY, IsNoDelay)

def set_nopush(hid, IsNoPush = False):
	ioctl(hid, ITMS_NOPUSH, IsNoPush)

def set_priority(hid, priority):
	ioctl(hid, ITMS_PRIORITY, priority)

def set_tos(hid, tos):
	ioctl(hid, ITMS_TOS, tos)

def set_enable(hid):
	cclib.sysinfo(ITMS_ENABLE, hid)

def set_disable(hid):
	cclib.sysinfo(ITMS_DISABLE, hid)

def fastmode(UseFastMode = False):
	cclib.sysinfo(ITMS_FASTMODE, UseFastMode and 1 or 0)

def doc_set(text):
	cclib.write(ITMC_SYSCD, ITMS_SETDOC, 0, text, 1)

def doc_get():
	cclib.write(ITMC_SYSCD, ITMS_GETDOC, 0, '', 1)

def casuald_post(what):
	cclib.write(ITMC_SYSCD, ITMS_MESSAGE, 0, what, 1)



#----------------------------------------------------------------------
# 环境变量
#----------------------------------------------------------------------
def environ_set(key, value):
	casuald_post('SET %s=%s'%(str(key), str(value)))


#----------------------------------------------------------------------
# 加载配置：所有的 section/item都转换为小写
#----------------------------------------------------------------------
def loadcfg (ininame):
	ininame = os.path.abspath(ininame)
	content = ''
	try:
		content = open(ininame, 'rb').read()
	except:
		return None
	if content[:3] == '\xef\xbb\xbf':	# 去掉 BOM+
		content = content[3:]

	import ConfigParser
	cp = ConfigParser.ConfigParser()
	config = {}

	import cStringIO
	sio = cStringIO.StringIO(content)

	cp.readfp(sio)
	for sect in cp.sections():
		for key, val in cp.items(sect):
			lowsect, lowkey = sect.lower(), key.lower()
			config.setdefault(lowsect, {})[lowkey] = val
	
	return config


def startchn(chname, args, env):
	if os.environ.get("_LOADER_") == "CLD":
		cmd = ["cld", chname]
	else:
		chname = chname.replace(".", os.sep)
		execfn = "%s.py" % chname
		if not os.path.isfile(execfn):
			execfn = "%s.pyc" % chname
		cmd = ["python", execfn]
	cmd.extend(args)
	return subprocess.Popen(cmd, close_fds=True, env=env)

