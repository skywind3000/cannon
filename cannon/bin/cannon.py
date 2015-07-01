#! /usr/bin/env python
# -*- coding: utf-8 -*-
#======================================================================
# 
# cannon.py
#
# 服务主程序，启动transmod，再根据配置文件启停连接 Channel 0的服务。
#
#======================================================================
import sys, time, struct
import os, errno, socket, mmap
import datetime, ctypes


#======================================================================
# version info
#======================================================================
VERSION = '1.24'


#======================================================================
# const definition
#======================================================================
CTMS_SERVICE	=	0	# 服务状态
CTMS_CUCOUNT	=	1	# 现在外部连接数
CTMS_CHCOUNT	=	2	# 现在频道连接数
CTMS_WTIME		=	3	# 取得服务运行时间
CTMS_STIME		=	4	# 取得服务开始时间

CTM_STOPPED		=	0	# 服务状态：停止
CTM_STARTING	=	1	# 服务状态：启动中
CTM_RUNNING		=	2	# 服务状态：服务
CTM_STOPPING	=	3	# 服务状态：停止中

CTM_OK			=	0	# 没有错误
CTM_ERROR		=	1	# 发生错误

_optdict = {  
	'HEAD':0, 'WTIME':1, 'PORTU4':2, 'PORTC4':3, 'PORTU6':4, 'PORTC6':5,
	'PORTD4':6, 'PORTD6':7, 'HTTPSKIP':8, 'DGRAM':9, 'AUTOPORT': 10,
	'MAXCU':20, 'MAXCC':21, 'TIMEU':22, 'TIMEC':23, 'LIIMT':24, 'LIMTU':25, 
	'LIMTC':26, 'ADDRC4':27, 'ADDRC6':28, 
	'DATMAX':40, 'DHCPBASE':41, 'DHCPHIGH':42, 
	'GPORTU4':60, 'GPORTC4':61, 'GPORTD4': 63,
	'GPORTU6':64, 'GPORTC6':65, 'GPORTD6': 66,
	'PLOGP':80, 'PENCP':81, 'LOGMK':82, 'UTIME':84, 'NOREUSE':85,
	'SOCKRCVO':90, 'SOCKSNDO':91, 'SOCKRCVI':92, 'SOCKSNDI':93, 
	'SOCKUDPB':94, 
	}


#----------------------------------------------------------------------
# ctransmod: 加载 transmod.so并启动网络服务
#----------------------------------------------------------------------
class ctransmod (object):
	
	def __init__ (self, path = '.'):
		self._win32 = 0
		self._dllname = 'transmod.so'
		if sys.platform[:3] == 'win':
			self._win32 = 1
			self._dllname = 'transmod.dll'
		self.libctm = None
		if not self.libctm:
			for n in ([ path ] + sys.path):
				base = os.path.abspath(n)
				self.libctm = self._loadlib(os.path.join(base, self._dllname))
				if self.libctm: 
					break
		if not self.libctm:
			raise Exception('cannot locate %s'%self._dllname)
		self._setup_interface()
		self._setup_info()
		self._document = ''
		self._version = -1
		self.logmode(2, '')

	def _loadlib (self, name):
		lib = None
		try: lib = ctypes.cdll.LoadLibrary(name)
		except: pass
		return lib
	
	def _setup_interface (self):
		self.__startup = self.libctm.ctm_startup
		self.__shutdown = self.libctm.ctm_shutdown
		self.__status = self.libctm.ctm_status
		self.__errno = self.libctm.ctm_errno
		self.__config = self.libctm.ctm_config
		self.__handle_log = self.libctm.ctm_handle_log
		self.__handle_validate = self.libctm.ctm_handle_validate
		self.__handle_logout = self.libctm.cmt_handle_logout
		self.__version = self.libctm.ctm_version
		self.__get_date = self.libctm.ctm_get_date
		self.__get_time = self.libctm.ctm_get_time
		self.__get_stat = self.libctm.ctm_get_stat
		self.__get_doc = self.libctm.ctm_get_document
		self.__msg_get = self.libctm.ctm_msg_get
		self.__msg_put = self.libctm.ctm_msg_put
		self.__startup.argtypes = []
		self.__startup.restype = ctypes.c_int
		self.__shutdown.argtypes = []
		self.__shutdown.restype = ctypes.c_int
		self.__status.argtypes = [ ctypes.c_int ]
		self.__status.restype = ctypes.c_long
		self.__errno.argtypes = []
		self.__errno.restype = ctypes.c_long
		self.__config.argtypes = [ ctypes.c_int, ctypes.c_long, ctypes.c_char_p ]
		self.__config.restype = ctypes.c_int
		self.__handle_log.restype = ctypes.c_int
		self.__handle_validate.restype = ctypes.c_int
		self.__handle_logout.argtypes = [ ctypes.c_int, ctypes.c_char_p ]
		self.__handle_logout.restype = ctypes.c_int
		self.__version.argtypes = []
		self.__version.restype = ctypes.c_int
		self.__get_date.argtypes = []
		self.__get_date.restype = ctypes.c_char_p
		self.__get_time.argtypes = []
		self.__get_time.restype = ctypes.c_char_p
		self.__get_stat.argtypes = [ ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p ]
		self.__get_stat.restype = None
		self.__get_doc.argtypes = [ ctypes.c_void_p, ctypes.c_void_p, ctypes.c_long ]
		self.__get_doc.restype = ctypes.c_long
		self.__msg_get.argtypes = [ ctypes.c_void_p, ctypes.c_long ]
		self.__msg_get.restype = ctypes.c_long
		self.__msg_put.argtypes = [ ctypes.c_char_p, ctypes.c_long ]
		self.__msg_put.restype = ctypes.c_long
		self._buffer = ctypes.create_string_buffer('\000' * 0x200000)
		return 0
	
	def _setup_info (self):
		self._build_version = self.__version()
		self._build_date = self.__get_date()
		self._build_time = self.__get_time()
		self._build_date = self._build_date.replace('  ', ' ')
		ver = '%d.%02x'%(self._build_version >> 8, self._build_version & 255)
		self._build_desc = 'Transmod %s (%s)'%(ver, self._build_date)
		return 0
	
	def _get_document (self, fetch = True):
		ver = ctypes.c_uint()
		txt = None
		if not fetch:
			hr = self.__get_doc(ctypes.byref(ver), None, 0x200000)
		else:
			hr = self.__get_doc(ctypes.byref(ver), self._buffer, 0x200000)
			txt = self._buffer[:hr]
		return ver.value, txt

	def startup (self, wait = True):
		retval = self.__startup()
		if not wait:
			return retval
		for n in xrange(100):
			if self.status(CTMS_SERVICE) != CTM_STOPPED: break
			time.sleep(0.05)
		retval = self.status(CTMS_SERVICE)
		if retval == CTM_STOPPED: 
			return -1
		elif retval == CTM_STARTING:
			for n in xrange(100):
				if self.status(CTMS_SERVICE) != CTM_STARTING: break
				time.sleep(0.05)
			retval = self.status(CTMS_SERVICE)
		if retval != CTM_RUNNING: return -2
		return 0
	
	def shutdown (self):
		return self.__shutdown()
	
	def status (self, item):
		return self.__status(item)
	
	def errno (self):
		return self.__errno()
	
	def config (self, *args, **argv):
		hr = 0
		for n in argv:
			text = ''
			key, val = n.upper(), argv[n]
			if key in ('ADDRC', 'ADDRC4', 'ADDRC6'):
				text = val
				val = 0
			if not key in _optdict:
				raise Exception('config "%s" invalid'%n)
			hr = self.__config(_optdict[key], val, text)
		return int(hr)
	
	def logmode (self, mode, text):
		self.__handle_logout(mode, text)
	
	def get_info (self):
		outer = self.status(1)
		inner = self.status(2)
		wtime = self.status(3)
		stime = self.status(4)
		stime = struct.unpack('I', struct.pack('i', stime))[0]
		wtime = struct.unpack('I', struct.pack('i', wtime))[0]
		return outer, inner, wtime, stime

	def get_stat (self):
		pkt_send = ctypes.c_longlong()
		pkt_recv = ctypes.c_longlong()
		pkt_discard = ctypes.c_longlong()
		self.__get_stat(ctypes.byref(pkt_send), ctypes.byref(pkt_recv), ctypes.byref(pkt_discard))
		retval = pkt_send.value, pkt_recv.value, pkt_discard.value
		return retval

	def get_port (self):
		ports = {}
		ports['PORTU4'] = self.config(GPORTU4 = 0)
		ports['PORTC4'] = self.config(GPORTC4 = 0)
		ports['PORTD4'] = self.config(GPORTD4 = 0)
		ports['PORTU6'] = self.config(GPORTU6 = 0)
		ports['PORTC6'] = self.config(GPORTC6 = 0)
		ports['PORTD6'] = self.config(GPORTD6 = 0)
		return ports
	
	def update_document (self, loops = 5, waitms = 0.001):
		for i in xrange(loops):
			v, t = self._get_document(False)
			if v == self._version:
				return False
			if v % 2 == 0:
				_, nt = self._get_document(True)
				nv, _ = self._get_document(False)
				if nv == v:
					self._document = nt
					self._version = v
					return True
			if loops != 0:
				time.sleep(waitms)
		return None
	
	def msg_get (self):
		hr = self.__msg_get(self._buffer, 0x200000)
		if hr < 0:
			return None
		return self._buffer[:hr]
	
	def msg_put (self, data):
		if self.__msg_put(data, len(data)) != 0:
			return False
		return True


#----------------------------------------------------------------------
# crontab
#----------------------------------------------------------------------
class crontab (object):

	def __init__ (self):
		self.daynames = {}
		self.monnames = {}
		DAYNAMES = ('sun', 'mon', 'tue', 'wed', 'thu', 'fri', 'sat')
		MONNAMES = ('jan', 'feb', 'mar', 'apr', 'may', 'jun')
		MONNAMES = MONNAMES + ('jul', 'aug', 'sep', 'oct', 'nov', 'dec')
		for x in xrange(7):
			self.daynames[DAYNAMES[x]] = x
		for x in xrange(12):
			self.monnames[MONNAMES[x]] = x + 1
	
	# check_atom('0-10/2', (0, 59), 4) -> True
	# check_atom('0-10/2', (0, 59), 5) -> False
	def check_atom (self, text, minmax, value):
		if value < minmax[0] or value > minmax[1]:
			return None
		if minmax[0] > minmax[1]:
			return None
		text = text.strip('\r\n\t ')
		if text == '*':
			return True
		if text.isdigit():
			try: x = int(text)
			except: return None
			if x < minmax[0] or x > minmax[1]:
				return None
			if value == x:
				return True
			return False
		increase = 1
		if '/' in text:
			part = text.split('/')
			if len(part) != 2:
				return None
			try: increase = int(part[1])
			except: return None
			if increase < 1:
				return None
			text = part[0]
		if text == '*':
			x, y = minmax
		elif text.isdigit():
			try: x = int(text)
			except: return None
			if x < minmax[0] or x > minmax[1]:
				return None
			y = minmax[1]
		else:
			part = text.split('-')
			if len(part) != 2:
				return None
			try:
				x = int(part[0])
				y = int(part[1])
			except:
				return None
		if x < minmax[0] or x > minmax[1]:
			return None
		if y < minmax[0] or y > minmax[1]:
			return None
		if x <= y:
			if value >= x and value <= y:
				if increase == 1 or (value - x) % increase == 0:
					return True
			return False
		else:
			if value <= y:
				if increase == 1 or (value - minmax[0]) % increase == 0:
					return True
			elif value >= x:
				if increase == 1 or (value - x) % increase == 0:
					return True
			return False
		return None

	# 解析带有月份/星期缩写，逗号的项目
	def check_token (self, text, minmax, value):
		for text in text.strip('\r\n\t ').split(','):
			text = text.lower()
			for x, y in self.daynames.iteritems():
				text = text.replace(x, str(y))
			for x, y in self.monnames.iteritems():
				text = text.replace(x, str(y))
			hr = self.check_atom(text, minmax, value)
			if hr == None:
				return None
			if hr:
				return True
		return False
	
	# 切割 crontab配置
	def split (self, text):
		text = text.strip('\r\n\t ')
		need = text[:1] == '@' and 1 or 5
		data, mode, line = [], 1, ''
		for i in xrange(len(text)):
			ch = text[i]
			if mode == 1:
				if ch.isspace():
					data.append(line)
					line = ''
					mode = 0
				else:
					line += ch
			else:
				if not ch.isspace():
					if len(data) == need:
						data.append(text[i:])
						line = ''
						break
					line = ch
					mode = 1
		if line:
			data.append(line)
		if len(data) < need:
			return None
		if len(data) == need:
			data.append('')
		return tuple(data[:need + 1])
	
	# 传入如(2013, 10, 21, 16, 35)的时间，检查 crontab是否该运行
	def check (self, text, datetuple, runtimes = 0):
		data = self.split(text)
		if not data:
			return None
		if len(data) == 2:
			entry = data[0].lower()
			if entry == '@reboot':
				return (runtimes == 0) and True or False
			if entry in ('@yearly', '@annually'):
				data = self.split('0 0 1 1 * ' + data[1])
			elif entry == '@monthly':
				data = self.split('0 0 1 * * ' + data[1])
			elif entry == '@weekly':
				data = self.split('0 0 * * 0 ' + data[1])
			elif entry == '@daily':
				data = self.split('0 0 * * * ' + data[1])
			elif entry == '@midnight':
				data = self.split('0 0 * * * ' + data[1])
			elif entry == '@hourly':
				data = self.split('0 * * * * ' + data[1])
			else:
				return None
			if data == None:
				return None
		if len(data) != 6 or len(datetuple) != 5:
			return None
		year, month, day, hour, mins = datetuple
		if type(month) == type(''):
			month = month.lower()
			for x, y in self.monnames.iteritems():
				month = month.replace(x, str(y))
			try:
				month = int(month)
			except:
				return None
		import datetime
		try:
			x = datetime.datetime(year, month, day).strftime("%w")
			weekday = int(x)
		except:
			return None
		hr = self.check_token(data[0], (0, 59), mins)
		if not hr: return hr
		hr = self.check_token(data[1], (0, 23), hour)
		if not hr: return hr
		hr = self.check_token(data[2], (0, 31), day)
		if not hr: return hr
		hr = self.check_token(data[3], (1, 12), month)
		if not hr: return hr
		hr = self.check_token(data[4], (0, 6), weekday)
		if not hr: return hr
		return True
	
	# 调用 crontab程序
	def call (self, command):
		import subprocess
		subprocess.Popen(command, shell = True)

	# 读取crontab文本，如果错误则返回行号，否则返回任务列表
	def read (self, content, times = 0):
		schedule = []
		if content[:3] == '\xef\xbb\xbf':	# remove BOM+
			content = content[3:]
		ln = 0
		for line in content.split('\n'):
			line = line.strip('\r\n\t ')
			ln += 1
			if line[:1] in ('#', ';', ''): 
				continue
			hr = self.split(line)
			if hr == None:
				return ln
			obj = {}
			obj['cron'] = line
			obj['command'] = hr[-1]
			obj['runtimes'] = times
			obj['lineno'] = ln
			schedule.append(obj)
		return schedule
	
	# 调度任务列表
	def schedule (self, schedule, workdir = '.', env = {}):
		datetuple = time.localtime(time.time())[:5]
		runlist = []
		if not schedule:
			return []
		savedir = os.getcwd()
		for obj in schedule:
			if self.check(obj['cron'], datetuple, obj['runtimes']):
				if workdir:
					os.chdir(workdir)
				command = obj['command']
				execute = command
				for k, v in env.iteritems():
					execute = execute.replace('$(%s)'%k, str(v))
				self.call(execute)
				if workdir:
					os.chdir(savedir)
				obj['runtimes'] += 1
				t = (obj['cron'], command, execute, obj['lineno'])
				runlist.append(t + (obj['runtimes'], ))
		return runlist


#----------------------------------------------------------------------
# configure: 配置文件管理和PID管理
#----------------------------------------------------------------------
class configure (object):
	
	def __init__ (self):
		self.unix = 1
		if sys.platform[:3] == 'win':
			self.unix = 0
		self._dir_base = os.path.split(os.path.abspath(__file__))[0]
		self._dir_home = os.path.abspath(os.path.join(self._dir_base, '..'))
		self._dir_bin = self._dir_base
		self._dir_channel = self.subdir('channel')
		self._dir_conf = self.subdir('conf')
		self._dir_lib = self.subdir('lib')
		self._dir_logs = self.subdir('logs')
		self._dir_tmp = self.subdir('tmp')
		self._service = {}
		self._config = {}
		self.casuald = self._dir_home
		self.GetShortPathName = None
		self.TerminateProcess = None
		self.OpenProcess = None
		self.CloseHandle = None
		self._scan_conf()
		self._init_win()
		self.ct = crontab()
		self._mm_fp = None
		self._mm_mm = None
		self._mm_version = 0
	
	def subdir (self, text):
		return os.path.abspath(os.path.join(self._dir_home, text))

	def _scan_conf (self):
		for fn in os.listdir(self._dir_conf):
			ext = os.path.splitext(fn)[-1].lower()
			if ext == '.ini':
				sname = os.path.splitext(fn)[0]
				fname = os.path.abspath(self.subdir('conf/' + fn))
				self._service[sname] = fname
		return 0
	
	def _init_win (self):
		if self.unix:
			return 0
		try:
			self.kernel32 = ctypes.windll.LoadLibrary("kernel32.dll")
			self.textdata = ctypes.create_string_buffer('\000' * 1024)
			self.GetShortPathName = self.kernel32.GetShortPathNameA
			self.TerminateProcess = self.kernel32.TerminateProcess
			self.OpenProcess = self.kernel32.OpenProcess
			self.CloseHandle = self.kernel32.CloseHandle
			args = [ ctypes.c_char_p, ctypes.c_char_p, ctypes.c_int ]
			self.GetShortPathName.argtypes = args
			self.GetShortPathName.restype = ctypes.c_uint32
			self.TerminateProcess.argtypes = [ ctypes.c_long, ctypes.c_int ]
			self.TerminateProcess.restype = ctypes.c_int
			self.OpenProcess.argtypes = [ ctypes.c_int, ctypes.c_int, ctypes.c_int ]
			self.OpenProcess.restype = ctypes.c_long
			self.CloseHandle.argtypes = [ ctypes.c_long ]
			self.CloseHandle.restype = ctypes.c_int
			self.GetProcessId = self.kernel32.GetProcessId
			self.GetProcessId.argtypes = [ ctypes.c_int ]
			self.GetProcessId.restype = ctypes.c_int
		except:
			pass
		return 0
	
	def _load_ini (self, service, config):
		if not service in self._service:
			self.error = 'can not find service %s'%service
			return -1
		import ConfigParser
		cp = ConfigParser.ConfigParser()
		ininame = self._service[service]
		try:
			content = open(ininame, 'rb').read()
		except:
			self.error = 'can not open %s'%ininame
			return -2
		if content[:3] == '\xef\xbb\xbf':	# 去掉 BOM+
			content = content[3:]
		content = '\n'.join([ n.strip('\r\n') for n in content.split('\n') ])
		import cStringIO
		sio = cStringIO.StringIO(content)
		cp.readfp(sio)
		environ = []
		for sect in cp.sections():
			for key, val in cp.items(sect):
				lowsect, lowkey = sect.lower(), key.lower()
				if lowsect == 'environ':
					environ.append((key, val))
				config.setdefault(lowsect, {})[lowkey] = val
		if not 'transmod' in config:
			self.error = 'no transmod section in %s.ini'%service
			return -3
		if not 'service' in config:
			config['service'] = {}
		if not 'service' in config:
			self.error = 'no service section in %s.ini'%service
			return -4
		if not 'portu' in config['transmod']:
			self.error = 'no user port definition in %s.ini'%service
			return -5
		if not 'portc' in config['transmod']:
			self.error = 'no channel port definition in %s.ini'%service
			return -6
		if not 'exec' in config['service']:
			config['service']['exec'] = ''
			#self.error = 'no executive definition in %s.ini'%service
			#return -7
		autoport = config['transmod'].get('autoport', '0').lower()
		autoport = (autoport in ('1', 'yes', 1, 'on', 'y', 'true')) and 1 or 0
		config['transmod']['autoport'] = str(autoport)
		config['transmod']['exec'] = config['service']['exec']
		config['transmod']['execuser'] = config['service'].get("execuser", "")
		config['transmod']['crontab'] = config['service'].get('crontab', '')
		chaddr = config['transmod'].get('chaddr', '').strip('\r\n\t ')
		if not chaddr:
			chaddr = '127.0.0.1'
		config['transmod']['chaddr'] = chaddr
		config['transmod']['policyfile'] = ''
		config['transmod']['policyport'] = ''
		if 'flash' in config:
			flash = config['flash']
			config['transmod']['policyfile'] = flash.get('policyfile', '')
			config['transmod']['policyport'] = flash.get('policyport', '')
		config['service'] = config.get('service', {})
		schedule = config['service'].get('crontab', '')
		config['service']['crontab'] = schedule
		config['service']['crontxt'] = ''
		config['transmod']['__environ__'] = environ
		if schedule:
			fn = os.path.abspath(self.subdir('conf/%s'%schedule))
			try:
				text = open(fn, 'r').read()
				config['service']['crontxt'] = text
			except:
				pass
		return 0
	
	def _load_config (self, service):
		if service in self._config:
			return 0
		config = {}
		hr = self._load_ini(service, config)
		if hr != 0:
			return hr
		self._config[service] = config
		return 0
	
	def option (self, service, name, default = ''):
		if not service in self._service:
			text = 'not find %s.ini in %s'%(service, self.subdir('conf'))
			raise IOError(text)
		retval = self._load_config(service)
		if retval != 0:
			sys.stderr.write(self.error + '\n')
			sys.exit(-1)
		config = self._config[service]
		if not name in config['transmod']:
			text = default
		else:
			text = config['transmod'][name]
		return text
	
	def pathshort (self, path):
		path = os.path.abspath(path)
		if self.unix:
			return path
		if not self.GetShortPathName:
			self.kernel32 = None
			self.textdata = None
		if not self.GetShortPathName:
			return path
		retval = self.GetShortPathName(path, self.textdata, 1024)
		shortpath = self.textdata.value
		if retval <= 0:
			return ''
		return shortpath

	def load (self, service):
		if not service in self._service:
			text = 'not find %s.ini in %s'%(service, self.subdir('conf'))
			raise IOError(text)
		remote = self.option(service, 'chaddr', '127.0.0.1')
		if remote == '0.0.0.0': remote = '127.0.0.1'
		environ = self.option(service, '__environ__', [])
		for name, val in environ:
			os.environ[name] = val
		os.environ['CHADDR'] = remote
		os.environ['PORTU'] = self.option(service, 'portu', '3000')
		os.environ['PORTC'] = self.option(service, 'portc', '3008')
		os.environ['PORTD'] = self.option(service, 'portd', '3000')
		os.environ['PORTU6'] = self.option(service, 'portu6', '-1')
		os.environ['PORTC6'] = self.option(service, 'portc6', '-1')
		os.environ['PORTD6'] = self.option(service, 'portd6', '-1')
		os.environ['HEADER'] = self.option(service, 'header', '0')
		os.environ['HTTPSKIP'] = self.option(service, 'httpskip', '0')
		os.environ['AUTOPORT'] = self.option(service, 'autoport', '0')
		os.environ['SERVICE'] = service
		os.environ['CHOME'] = self._dir_home
		os.environ['CASUALD'] = self._dir_home
		os.environ['CONFIG'] = self._service[service]
		path = self.pathshort(self.subdir('bin')) 
		if self.unix: path = path + ':'
		else: path = path + ';'
		path = path + self.pathshort(self.subdir('lib'))
		os.environ['PYTHONPATH'] = path
		return 0
	
	def __iter__ (self):
		return self._service.__iter__()
	
	def __getitem__ (self, key):
		return self._service[key]
	
	def kill (self, pid, signal = 15):
		if self.unix:
			try:
				os.kill(pid, signal)
			except:
				return -1
		else:
			if not self.OpenProcess:
				sys.stderr.write('can not terminate process %d\n'%pid)
				sys.stderr.flush()
				return -100
			handle = self.OpenProcess(1, 0, pid)
			if not handle:
				return -1
			if signal != 0:
				if self.TerminateProcess(handle, signal) == 0:
					self.CloseHandle(handle)
					return -2
			self.CloseHandle(handle)
		return 0
	
	def initdir (self, service):
		if not service in self._service:
			text = 'not find %s.ini in %s'%(service, self.subdir('conf'))
			raise IOError(text)
		if not os.path.exists(self.subdir('logs')):
			os.mkdir(self.subdir('logs'))
		#if not os.path.exists(self.subdir('tmp')):
		#	os.mkdir(self.subdir('tmp'))
		if not os.path.exists(self.subdir('logs/' + service)):
			os.mkdir(self.subdir('logs/' + service))
		#if not os.path.exists(self.subdir('tmp/' + service)):
		#	os.mkdir(self.subdir('tmp/' + service))
		return 0
	
	def pidname (self, service):
		if not service in self._service:
			text = 'not find %s.ini in %s'%(service, self.subdir('conf'))
			raise IOError(text)
		path = os.path.join(self.subdir('logs/' + service + '/cannon.pid'))
		return path
	
	def savepid (self, service):
		if not service in self._service:
			text = 'not find %s.ini in %s'%(service, self.subdir('conf'))
			raise IOError(text)
		name = self.pidname(service)
		try: fp = open(name, 'w')
		except: return -1
		fp.write(str(os.getpid()))
		fp.flush()
		fp.truncate()
		fp.close()
		return 0
	
	def erasepid (self, service):
		if not service in self._service:
			text = 'not find %s.ini in %s'%(service, self.subdir('conf'))
			raise IOError(text)
		name = self.pidname(service)
		os.remove(name)
		return 0
	
	def readpid (self, service):
		if not service in self._service:
			text = 'not find %s.ini in %s'%(service, self.subdir('conf'))
			raise IOError(text)
		name = self.pidname(service)
		try: fp = open(name, 'r')
		except: return -1
		text = fp.read()
		fp.close()
		pid = -1
		try: pid = int(text)
		except: pass
		return pid
	
	def mm_name (self, service):
		if not service in self._service:
			text = 'not find %s.ini in %s'%(service, self.subdir('conf'))
			raise IOError(text)
		return os.path.join(self.subdir('logs/' + service + '/cannon.mem'))
		
	def mm_open (self, service):
		self.mm_close()
		name = self.mm_name(service)
		try:
			self._mm_fp = open(name, 'w+b')
		except:
			self.mm_close()
			return False
		f = self._mm_fp
		f.seek(0)
		f.write(struct.pack('<IIII', 0, 0, 0, 0))
		f.write('\x00' * 240)
		f.truncate()
		size = f.tell()
		try:
			self._mm_mm = mmap.mmap(f.fileno(), size, access = mmap.ACCESS_WRITE)
		except:
			self.mm_close()
			return False
		self._mm_svc = service
		return True
	
	def mm_close (self):
		if self._mm_mm:
			try: self._mm_mm.close()
			except: pass
			self._mm_mm = None
		if self._mm_fp:
			try: self._mm_fp.close()
			except: pass
			self._mm_fp = None
		return 0
	
	def mm_resize (self, size):
		if not self._mm_mm:
			return False
		try:
			self._mm_mm.resize(size)
		except SystemError:
			self._mm_mm.close()
			self._mm_mm = None
			if not self._mm_fp:
				return False
			self._mm_fp.seek(0)
			self._mm_fp.write('\x00' * size)
			self._mm_fp.truncate()
			fn = self._mm_fp.fileno()
			try:
				self._mm_mm = mmap.mmap(fn, size, access = mmap.ACCESS_WRITE)
			except:
				self.mm_close()
				return False
		return True
	
	def mm_update (self, data, flush = False):
		if not self._mm_mm:
			return False
		mm = self._mm_mm
		size = len(data)
		if size + 256 > len(mm):
			if size < 8192:
				page = 256
				for i in xrange(32):
					if page >= size + 256: break
					page <<= 1
			else:
				page = ((size + 256 + 8192 - 1) / 8192) * 8192
			if len(mm) < page:
				self.mm_resize(page)
				mm = self._mm_mm
		if not mm:
			return False
		version = struct.unpack('<I', mm[:4])[0]
		version = (version + 1) & 0x3fffffff
		mm[:4] = struct.pack('<I', version)
		mm[256:256 + size] = data
		mm[4:8] = struct.pack('<I', len(data))
		version = (version + 1) & 0x3fffffff
		mm[:4] = struct.pack('<I', version)
		if flush:
			mm.flush()
		return True
	
	def mm_read (self, service, retry = 10, wait = 0.002):
		name = self.mm_name(service)
		try:
			fp = open(name, 'r+b')
		except:
			return None
		try:
			mm = mmap.mmap(fp.fileno(), 0, access = mmap.ACCESS_READ)
		except:
			fp.close()
			return None
		text = None
		if len(mm) < 256:
			mm.close()
			fp.close()
			return None
		for i in xrange(retry):
			version = struct.unpack('<I', mm[:4])[0]
			if version % 2 == 0:
				size = struct.unpack('<I', mm[4:8])[0]
				data = mm[256:256 + size]
				next = struct.unpack('<I', mm[:4])[0]
				if next == version:
					text = data
					break
			time.sleep(wait)
		try:
			mm.close()
			fp.close()
		except:
			pass
		return text
	
	def mm_msg_put (self, service, text, retry = 10, wait = 0.002):
		name = self.mm_name(service)
		try:
			fp = open(name, 'r+b')
		except:
			return False
		try:
			mm = mmap.mmap(fp.fileno(), 0, access = mmap.ACCESS_WRITE)
		except:
			fp.close()
			return None
		hr = False
		for i in xrange(retry):
			version = struct.unpack('<I', mm[8:12])[0]
			if version == 0:
				text = text[:240]
				size = len(text)
				mm[16:16 + size] = text
				mm[12:16] = struct.pack('<I', size)
				mm[8:12] = struct.pack('<I', 1)
				mm.flush()
				hr = True
				break
			time.sleep(wait)
		try:
			mm.close()
			fp.close()
		except:
			pass
		return hr
	
	def mm_msg_get (self):
		text = None
		if not self._mm_mm:
			return None
		mm = self._mm_mm
		version = struct.unpack('<I', mm[8:12])[0]
		if version == 0:
			return None
		size = struct.unpack('<I', mm[12:16])[0]
		text = mm[16:16 + size]
		mm[8:16] = '\x00' * 8
		return text


#----------------------------------------------------------------------
# FlashPolicy
#----------------------------------------------------------------------
class policysvr (object):

	def __init__ (self):
		self._clients = {}
		self._sock4 = None
		self._sock6 = None
		self._errd = ( errno.EINPROGRESS, errno.EALREADY, errno.EWOULDBLOCK )
	
	def startup (self, port = 8430, policy = ''):
		self.shutdown()
		self._port = port
		self._policy = policy
		binding = '0.0.0.0'
		self._sock4 = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
		self._sock4.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
		try:
			self._sock4.bind((binding, port))
		except: 
			try: self._sock4.close()
			except: pass
			self._sock4 = None
			return -1
		self._sock4.listen(10)
		self._sock4.setblocking(0)
		if 'AF_INET6' in socket.__dict__ and socket.has_ipv6:
			try:
				self._sock6 = socket.socket(socket.AF_INET6, socket.SOCK_STREAM)
				self._sock6.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
				if 'IPPROTO_IPV6' in socket.__dict__:
					self._sock6.setsockopt(socket.IPPROTO_IPV6, socket.IPV6_V6ONLY, 1)
				self._sock6.bind(('', port))
			except:
				try: self._sock6.close()
				except: pass
				self._sock6 = None
		if self._sock6:
			self._sock6.listen(10)
			self._sock6.setblocking(0)
		self._current = time.time()
		return 0

	def shutdown (self):
		if self._sock4: 
			try: self._sock4.close()
			except: pass
		if self._sock6:
			try: self._sock6.close()
			except: pass
		self._sock4 = None
		self._sock6 = None
		self._index = 0
		cidlist = [ cid for cid in self._clients ]
		for cid in cidlist:
			client = self._clients[cid]
			try: client[0].close()
			except: pass
			client[0] = None
			client[1] = ''
			client[2] = ''
			client[3] = -1
			client[4] = 0
			del self._clients[cid]
		self._clients = {}
		return 0

	def __handle_accept (self, xsock):
		if not xsock:
			return -1
		sock = None
		try: 
			sock, remote = xsock.accept()
			sock.setblocking(0)
		except: pass
		if not sock:
			return -2
		client = [ sock, '', '', 0, self._current + 5.0, remote ]
		self._clients[self._index] = client
		self._index += 1
		return 0

	def __handle_client (self, client):
		sock, rbuf, sbuf, state, timeout, remote = client
		if state == 0:
			rdata = ''
			while 1:
				text = ''
				try:
					text = sock.recv(1024)
					if not text:
						errc = 10000
						sock.close()
						return -1
				except socket.error,(code, strerror):
					if not code in self._errd:
						sock.close()
						return -2
				if text == '':
					break
				rdata += text
			rbuf += rdata
			client[1] = rbuf
			request = '<policy-file-request/>'
			if rbuf[:len(request)] == request:
				client[3] = 1
				client[2] = self._policy
				state = 1
			elif rbuf[:4].lower() == 'get ' and rbuf[-4:] == '\r\n\r\n':
				request = rbuf[4:rbuf.find('\n')].strip('\r\n')
				if request[-8:-3] == 'HTTP/':
					request = request[:-9]
				try:
					hr = self.http(request, rbuf, remote)
				except:
					import cStringIO, traceback
					sio = cStringIO.StringIO()
					traceback.print_exc(file = sio)
					err = 'INTERNAL SERVER ERROR\r\n\r\n' + sio.getvalue()
					hr = ('500 INTERNAL SERVER ERROR', 'text/plain', err)
				if not hr:
					hr = ('404 NOT FIND', 'text/plain', 'NOT FIND')
				code, mime, text = hr
				output = 'HTTP/1.0 %s\r\nConnection: Close\r\n'%code
				output += 'Content-Type: %s\r\n'%mime
				output += 'Content-Length: %d\r\n\r\n'%len(text)
				client[2] = output + text
				client[3] = 1
				state = 1
		if state == 1:
			wsize = 0
			sbuf = client[2]
			if len(sbuf) > 0:
				try:
					wsize = sock.send(sbuf)
				except socket.error,(code, strerror):
					if not code in self._errd:
						sock.close()
						return -3
				sbuf = sbuf[wsize:]
			client[2] = sbuf
			if len(sbuf) == 0:
				client[3] = 2
				state = 2
				client[4] = self._current + 0.05
		if state == 2:
			return 1
		if self._current > timeout:
			return -4
		return 0
	
	def http (self, request, head, remote):
		return None

	def process (self):
		if self._sock4 == None and self._sock6 == None:
			return 0
		self._current = time.time()
		while 1:
			if self.__handle_accept(self._sock4) != 0:
				break
		while 1:
			if self.__handle_accept(self._sock6) != 0:
				break
		cleanlist = []
		for cid in self._clients:
			client = self._clients[cid]
			if self.__handle_client(client) != 0:
				cleanlist.append(cid)
		for cid in cleanlist:
			client = self._clients[cid]
			try: client[0].close()
			except: pass
			client[0] = None
			client[1] = ''
			client[2] = ''
			client[3] = -1
			client[4] = 0
			del self._clients[cid]
		return 0


#----------------------------------------------------------------------
# service: 服务启停，进程执行
#----------------------------------------------------------------------
class cservice (object):
	
	def __init__ (self):
		self.config = configure()
		self.transmod = None
		self.unix = self.config.unix
		self.current = ''
		self.execute = ''
		self.execuser = ''
		self.daemoned = 0
		self.child_pid = -1
		self.running = 0
		self.closing = 0
		self.autoport = 0
		self.refresh_cfg = False
		self.flashpolicy = None
		self.__logfile = None
		self.__info = {}
		self.__lastsec = None
		self.__lastmin = None
		self.__crontxt = None
		self.__dirty = False
		self.__environ = {}
		self.ready = True
		self.savecfg = {}
		class XOBJ(object): pass
		self.schedule = XOBJ()
		self.schedule.crontxt = None
		self.schedule.crontab = None
		self.schedule.schedule = None
		self.schedule.runtimes = 0
		self.schedule.environ = {}
	
	def detect (self, service):
		pid = self.config.readpid(service)
		if pid < 0:
			return False
		if self.unix:
			if os.path.exists('/proc/%d'%pid):
				return True
		if self.config.kill(pid, 0) == 0:
			return True
		return False
	
	def query (self):
		line = []
		m1 = 0
		m2 = 0
		for name in self.config:
			pid = self.config.readpid(name)
			line.append((name, pid, self.detect(name)))
			if len(name) > m1: m1 = len(name)
			if len(str(pid)) > m2: m2 = len(str(pid))
		for i in xrange(len(line)):
			name, pid, status = line[i]
			name = name.ljust(m1 + 4)
			pid = str(pid).ljust(m2 + 2)
			status = status and 'running' or 'stopped'
			if status == 'running':
				print '    +', name, status, 'pid=%s'%pid
			else:
				print '    +', name, status
		return 0
	
	def daemon (self):
		self.daemoned = 0
		if not self.unix:
			return -1
		try:
			if os.fork() > 0: os._exit(0)
		except OSError, error:
			os._exit(1)
		os.setsid()
		os.umask(0)
		try:
			if os.fork() > 0: os._exit(0)
		except OSError, error:
			os._exit(1)
		fp = open('/tmp/cannon.log', 'w')
		os.dup2(2, fp.fileno())
		noclose = (0, 1, 2, fp.fileno())
		for i in xrange(100):
			try: 
				if not i in noclose: 
					os.close(i)
			except:
				pass
		self.daemoned = 1
		os.environ['DAEMON'] = '1'
		return 0
	
	def option (self, item, default = '', cfg = None):
		if not self.current:
			return default
		if not cfg:
			text = self.config.option(self.current, item, default)
		else:
			if not 'transmod' in cfg:
				text = default
			else:
				text = cfg['transmod'].get(item, default)
		if type(default) == type(0) and type(text) == type(''):
			multiply = 1
			text = text.strip('\r\n\t ')
			postfix1 = text[-1:].lower()
			postfix2 = text[-2:].lower()
			if postfix1 == 'k':
				multiply = 1024
				text = text[:-1]
			elif postfix1 == 'm': 
				multiply = 1024 * 1024
				text = text[:-1]
			elif postfix2 == 'kb':
				multiply = 1024
				text = text[:-2]
			elif postfix2 == 'mb':
				multiply = 1024 * 1024
				text = text[:-2]
			try: text = int(text.strip('\r\n\t '), 0)
			except: text = default
			if multiply > 1: 
				text *= multiply
		return text
	
	def mlog (self, *args):
		if not self.__logfile:
			if not self.current:
				return -1
			import logging
			import logging.handlers
			size = 1024 * 1024 * 5
			format = "[%(asctime)s] %(message)s"
			nm = self.config.subdir('logs/' + self.current + '/cannon.log')
			fh = logging.handlers.RotatingFileHandler(nm, mode = 'a', \
						maxBytes = size, backupCount = 4)
			lf = logging.Formatter(format, "%Y-%m-%d %H:%M:%S")
			fh.setFormatter(lf)
			logger = logging.getLogger(self.current)
			logger.setLevel(logging.INFO)
			logger.addHandler(fh)
			self.__logfile = logger
		text = ' '.join([ str(n) for n in args ])
		self.__logfile.info(text)
		if not self.daemoned:
			head = time.strftime('%Y-%m-%d %H:%M:%S', time.localtime())
			sys.stdout.write('[%s] %s\n'%(head, text))
			sys.stdout.flush()
		return 0
	
	def transmod_config (self, name, config, default):
		value = self.option(config, default)
		if self.transmod:
			param = { name : value }
			self.transmod.config(**param)
		self.savecfg[name] = value
		self.mlog('[config] %s=%s'%(name, value))
		return 0
	
	def transmod_refresh (self, newcfg, name, config, default):
		value = self.option(config, default, newcfg)
		if self.savecfg.get(name) == value:
			return 0
		if self.transmod:
			param = { name : value }
			self.transmod.config(**param)
		self.savecfg[name] = value
		self.mlog('[config] refresh %s=%s'%(name, value))
		return 0
	
	def __setup (self):
		self.transmod_config('head', 'header', 0)
		self.transmod_config('portu4', 'portu', 3000)
		self.transmod_config('portc4', 'portc', 3008)
		self.transmod_config('portu6', 'portu6', -1)
		self.transmod_config('portc6', 'portc6', -1)
		self.transmod_config('portd4', 'portd', 0)
		self.transmod_config('portd6', 'portd6', -1)
		self.transmod_config('addrc4', 'chaddr', '127.0.0.1')
		self.transmod_config('addrc6', 'chaddr6', '\x00' * 16)
		self.transmod_config('autoport', 'autoport', 0)
		self.transmod_config('maxcu', 'maxuser', 20000)
		self.transmod_config('maxcc', 'maxchannel', 100)
		self.transmod_config('timeu', 'timeoutu', 60 * 30)
		self.transmod_config('timec', 'timeoutc', 60 * 60 * 10)
		self.transmod_config('limtu', 'memlimitu', 1024)
		self.transmod_config('limtc', 'memlimitc', 10240)
		self.transmod_config('dgram', 'dgram', 0)
		self.transmod_config('logmk', 'logmask', 0xf)
		self.transmod_config('socksndo', 'sndbufu', -1)
		self.transmod_config('sockrcvo', 'rcvbufu', -1)
		self.transmod_config('socksndi', 'sndbufc', -1)
		self.transmod_config('sockrcvi', 'rcvbufc', -1)
		self.transmod_config('sockudpb', 'sockudpb', -1)
		self.transmod_config('datmax', 'datamax', 2048 * 1024)
		self.transmod_config('httpskip', 'httpskip', 0)
		execute = self.option('exec', '')
		if execute:
			execute = os.path.abspath(execute)
			if not self.unix:
				execute = execute.lower()
				extname = os.path.splitext(execute)[-1]
				if extname == '':
					realname = ''
					for ext in ('', '.exe', '.bat', '.py', '.perl', '.cmd'):
						name = execute + ext
						if os.path.exists(name): 
							realname = name
							break
					execute = realname
		if not os.path.exists(execute):
			self.mlog('[WARNING] can not find executable %s'%execute)
			#return -2
		self.execute = execute
		self.execuser = self.option('execuser','')
		prefix = self.config.subdir('logs/' + self.current + '/d')
		self.transmod.logmode(self.daemoned and 1 or 3, prefix)
		if not self.autoport:
			if self.__listen_try() != 0:
				return -3
		return 0
	
	def __refresh_ini (self):
		newcfg = {}
		hr = self.config._load_ini(self.current, newcfg)
		if hr != 0:
			self.mlog('[ERROR] %s'%self.config.error)
			self.mlog('[ERROR] refresh config error %d'%hr)
			return -1
		self.transmod_refresh(newcfg, 'maxcu', 'maxuser', 20000)
		self.transmod_refresh(newcfg, 'maxcc', 'maxchannel', 100)
		self.transmod_refresh(newcfg, 'timeu', 'timeoutu', 60 * 30)
		self.transmod_refresh(newcfg, 'timec', 'timeoutc', 60 * 60 * 10)
		self.transmod_refresh(newcfg, 'limtu', 'memlimitu', 1024)
		self.transmod_refresh(newcfg, 'limtc', 'memlimitc', 10240)
		self.transmod_refresh(newcfg, 'logmk', 'logmask', 0xf)
		self.transmod_refresh(newcfg, 'socksndo', 'sndbufu', -1)
		self.transmod_refresh(newcfg, 'sockrcvo', 'rcvbufu', -1)
		self.transmod_refresh(newcfg, 'socksndi', 'sndbufc', -1)
		self.transmod_refresh(newcfg, 'sockrcvi', 'rcvbufc', -1)
		self.transmod_refresh(newcfg, 'httpskip', 'httpskip', 0)
		ct = newcfg['service'].get('crontxt', '')
		cf = newcfg['service'].get('crontab', '')
		if self.schedule.crontab != cf or self.schedule.crontxt != ct:
			if not ct:
				self.schedule.crontab = None
				self.schedule.crontxt = None
				self.schedule.runtimes = 1
				self.schedule.schedule = None
			else:
				jobs = self.config.ct.read(ct, self.schedule.runtimes)
				if type(jobs) == type(int):
					self.schedule.crontab = None
					self.schedule.crontxt = None
					self.schedule.runtimes = 1
					self.schedule.schedule = None
					self.mlog('[crontab] error in line %d of %s'%(jobs, cf))
					return -2
				self.schedule.schedule = jobs
				self.schedule.runtimes = 1
				self.schedule.crontab = cf
				self.schedule.crontxt = ct
			env = self.schedule.environ
			env['CHOME'] = self.config._dir_home
			env['CASUALD'] = self.config._dir_home
			env['SERVICE'] = self.current
			env['HEADER'] = self.option('header', 0)
			env['CONFIG'] = self.config._service[self.current]
			env['PORTU'] = self.option('portu', 3000)
			env['PORTC'] = self.option('portc', 3008)
			env['PORTU6'] = self.option('portu6', -1)
			env['PORTC6'] = self.option('portc6', -1)
			env['HTTPSKIP'] = self.option('httpskip', 0)
		size = self.schedule.schedule and len(self.schedule.schedule) or 0
		self.mlog('[crontab] scheduled %d jobs'%size) 
		return 0
	
	def __policy (self):
		self.flashpolicy = None
		policyfile = self.option('policyfile', '')
		policyport = self.option('policyport', 0)
		if not policyport:
			self.mlog('disable policy service (policyport=0)')
			return -1
		oldname = policyfile
		policyfile = os.path.abspath(policyfile)
		self.flashpolicy = None
		policydata = ''
		try:
			policyport = int(policyport)
		except:
			self.mlog('[ERROR] policy port error: %s'%policyport)
			return -2
		self.mlog('[flash] policy port is %s'%policyport)
		if oldname and os.path.exists(policyfile):
			try: 
				policydata = open(policyfile, 'r').read()
			except:
				self.mlog('[ERROR] policy open error: %s'%policyfile)
				return -3
			self.mlog('[flash] policy file is %s'%oldname)
		if policyport:
			self.flashpolicy = policysvr()
			retval = self.flashpolicy.startup(policyport, policydata)
			if retval != 0:
				self.mlog('[ERROR] flash policy service binding error')
		return 0

	def __listen_try (self):
		s1 = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
		s2 = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
		s1.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1) 
		s2.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1) 
		address = self.option('chaddr', '127.0.0.1')
		p1 = self.option('portu', 3000)
		p2 = self.option('portc', 3008)
		try:
			s1.bind((address, p1))
			s2.bind((address, p2))
			s1.close()
			s2.close()
		except:
			self.mlog('can not listen to port %d or %d'%(p1, p2))
			return -1
		return 0
	
	def __run (self):
		execute = os.path.abspath(self.execute)
		home, name = os.path.split(execute)
		os.chdir(home)
		self.config.savepid(self.current)
		if sys.platform[:7] in ('freebsd', 'freebsd'):
			name = execute
		if self.unix and os.getuid() == 0:
			if self.execuser:
				self.mlog('change user to %s' % self.execuser)
				import pwd
				try:
					uinfo = pwd.getpwnam(self.execuser)
				except KeyError:
					self.mlog("[ERROR] can't start %s by user %s" % (execute, self.execuser))
					return -1
				
				os.setgid(uinfo[3])
				os.setuid(uinfo[2])

		if not self.execute:
			self.mlog('[WARNING] no executable file')
			self.child_pid = -1
			return 0

		if not self.unix:
			execute = name
		try: 
			pid = os.spawnle(os.P_NOWAIT, name, execute, os.environ)
		except OSError:
			self.child_pid = -1
			self.mlog("[ERROR] can't execute \"%s\""%execute)
			return -2

		if not self.config.unix:
			handle = pid
			try:
				pid = self.config.GetProcessId(handle)
				self.config.CloseHandle(pid)
			except:
				pass
		self.child_pid = pid
		return 0
	
	def __policy_server (self):
		if not self.flashpolicy:
			return 0
		try:
			self.flashpolicy.process()
		except Exception, e:
			import cStringIO
			import traceback
			sio = cStringIO.StringIO()
			traceback.print_exc(file = sio)
			text = sio.getvalue()
			sio = None
			self.mlog('[ERROR] FlashPolicy Server traceback')
			for line in text.split('\n'):
				line = line.strip('\r\n')
				self.mlog('[ERROR] ' + line)
			policy = self.flashpolicy._policy
			port = self.flashpolicy._port
			self.flashpolicy.shutdown()
			self.flashpolicy.startup(port, policy)
		return 0
	
	def __update_info (self):
		outer, inner, wtime, stime = self.transmod.get_info()
		pkt_send, pkt_recv, pkt_discard = self.transmod.get_stat()
		#print wtime, stime, outer, inner
		self.__info['outer'] = outer
		self.__info['inner'] = inner
		self.__info['wtime'] = wtime
		self.__info['stime'] = stime
		self.__info['pkt_send'] = pkt_send
		self.__info['pkt_recv'] = pkt_recv
		self.__info['pkt_discard'] = pkt_discard
		ver = self.transmod._build_version
		desc = 'Transmod %d.%02x'%(ver >> 8, ver & 255)
		desc += ', %s'%self.transmod._build_date
		desc += ', %s'%self.transmod._build_time
		desc = 'Casuald %s (%s)'%(VERSION, desc)
		desc += ' on %s'%sys.platform
		self.__info['info'] = desc
		output = self.__info['info'] + '\n'
		output += '%s\n'%self.current
		ts = long(self.__info['stime'])
		tm = time.strftime('%Y-%m-%d %H:%M:%S', time.localtime(ts))
		output += '%s %s\n'%(tm, self.__info['stime'])
		ts = time.time() - stime
		ts, xsec = ts / 60, ts % 60
		xhh, xmm = ts / 60, ts % 60
		output += '%02d:%02d:%02d\n'%(xhh, xmm, xsec)
		p1, p2, p3 = str(pkt_send), str(pkt_recv), str(pkt_discard)
		output += '(%d, %d, %s, %s, %s)'%(outer, inner, p1, p2, p3)
		self.__info['intro'] = output
		return 0
	
	def __http (self, request, head, remote):
		if request == '/intro':
			output = self.__info['intro']
			return ('200 OK', 'text/plain', output)
		elif request == '/remote':
			return ('200 OK', 'text/plain', repr(remote))
		if 0:		# 暂时禁用设置日志
			if request[:9] in ('/logmask/', '/logmask?'):
				try:
					mask = int(request[9:], 0)
					if remote[0] in ('127.0.0.1', '::1'):
						self.transmod.config(LOGMK = mask)
						return ('200 OK', 'text/plain', 'OK')
				except:
					pass
				return ('500 ERROR', 'text/plain', 'ERROR')
		if remote[0] == '127.0.0.1':		# 本地命令
			if request == '/refresh':
				self.refresh_cfg = True
				return ('200 OK', 'text/plain', 'refresh config')
			elif request == '/document':
				return ('200 OK', 'text/plain', self.transmod._document)
			elif request == '/environ':
				item = [ (k, v) for k, v in self.__environ.iteritems() ]
				item.sort()
				text = ''.join([ '%s=%s\n'%(k, v) for k, v in item ])
				return ('200 OK', 'text/plain', text)
		return None
	
	def __interval (self):
		now = datetime.datetime.now()
		# 时钟：每秒运行
		if now.second != self.__lastsec:
			self.__lastsec = now.second
			if self.refresh_cfg:
				self.refresh_cfg = False
				self.__refresh_ini()
		# 如果环境变量更新
		hr = self.transmod.update_document()
		if hr or self.__dirty:
			self.__update_shm()
			self.__dirty = False
		# 时钟：每分钟运行
		if now.minute != self.__lastmin:
			self.__lastmin = now.minute
			# 调度任务
			if self.schedule.schedule:
				ts = (now.year, now.month, now.day, now.hour, now.minute)
				bs = self.config._dir_home
				en = self.schedule.environ
				os.environ['XOUTER'] = str(self.__info['outer'])
				os.environ['XINNER'] = str(self.__info['inner'])
				os.environ['XWTIME'] = str(self.__info['wtime'])
				os.environ['XSTIME'] = str(self.__info['stime'])
				os.environ['XPKTSEND'] = str(self.__info['pkt_send'])
				os.environ['XPKTRECV'] = str(self.__info['pkt_recv'])
				os.environ['XPKTDISCARD'] = str(self.__info['pkt_discard'])
				hr = self.config.ct.schedule(self.schedule.schedule, bs, en)
				for obj in hr:
					self.mlog('[crontab] %s'%obj[2])
		return 0
	
	def __loop (self):
		self.running = 1
		index = 0
		savesec = long(time.time())
		self.__update_info()
		if self.flashpolicy:
			self.flashpolicy.http = self.__http
		while self.running:
			time.sleep(0.05)
			if self.transmod.status(CTMS_SERVICE) != CTM_RUNNING:
				break
			if self.flashpolicy:
				self.__policy_server()
			index += 1
			if (index & 3) == 0:
				self.__update_info()
				if (index & 7) == 0:
					ts = long(time.time())
					if ts != savesec:
						savesec = ts
						if self.ready:
							self.__interval()
			while self.ready:	# 更新事件
				msg = self.transmod.msg_get()
				if msg == None: break
				self.__handle_msg(msg)
			msg = self.config.mm_msg_get()
			if msg:				# 读取外部的命令
				import hashlib
				md5s, text = msg[:32].lower(), msg[32:]
				if md5s == hashlib.md5(text).hexdigest().lower():
					self.transmod.msg_put(text)
			if self.closing:
				self.terminate()
				self.running = 0
		if self.flashpolicy:
			self.flashpolicy.shutdown()
		self.mlog('transmod quit')
		return 0
	
	def __update_shm (self):
		data = []
		item = [ (k, v) for k, v in self.__environ.iteritems() ]
		item.sort()
		text = ''.join([ '%s=%s\n'%(k, v) for k, v in item ])
		data.append(text)
		data.append(self.transmod._document)
		import cPickle, hashlib
		text = cPickle.dumps(data, 1)
		md5s = hashlib.md5(text).hexdigest()
		self.config.mm_update(md5s + text, True)
		return 0

	def __handle_msg (self, msg):
		pos = msg.find(' ')
		src = msg
		if pos >= 0:
			cmd, msg = msg[:pos], msg[pos + 1:]
		else:
			cmd, msg = msg, ''
		cmd = cmd.lower()
		if cmd == 'set':
			pos = msg.find('=')
			if pos >= 0:
				k, v = msg[:pos].strip('\r\n\t '), msg[pos + 1:]
				v = v.replace('\n', '.').replace('\x00', '.').replace('\r', '.')
				self.__environ[k] = v
				self.__dirty = True
		elif cmd == 'del':
			k = msg.strip('\r\n\t ')
			if k in self.__environ:
				del self.__environ[k]
				self.__dirty = True
		else:
			self.mlog('unknow msg: %s'%repr(src))
		return 0
	
	def terminate (self):
		self.ready = False
		if self.child_pid >= 0:
			self.config.kill(self.child_pid)
			self.child_pid = -1
		time.sleep(0.15)
		if self.transmod:
			self.transmod.shutdown()
		self.running = 0
		time.sleep(0.20)
		return 0
	
	def update (self):
		if self.child_pid >= 0:
			self.config.kill(self.child_pid)
			self.child_pid = -1
		time.sleep(0.2)
		self.__run()
		return 0
	
	def sig_exit (self, signum, frame):
		if self.closing == 0:
			self.closing = 1
	
	def sig_term (self, signum, frame):
		if self.closing == 0:
			self.closing = 1
	
	def sig_usr1 (self, signum, frame):
		self.refresh_cfg = True
		return 0
	
	def sig_usr2 (self, signum, frame):
		return 0
	
	def sig_chld (self, signum, frame):
		while 1:
			try:
				pid, status = os.waitpid(-1, os.WNOHANG)
			except:
				pid = -1
			if pid < 0: break
		return 0
	
	def sig_hup (self, signum, frame):
		if self.transmod:
			if self.transmod.status(CTMS_SERVICE) == CTM_RUNNING:
				self.update()
		return 0
	
	def __envinit (self, service):
		self.config.mm_open(service)
		ts = time.strftime('%Y-%m-%d %H:%M:%S', time.localtime())
		ports = self.transmod.get_port()
		self.savecfg['portu4'] = str(ports['PORTU4'])
		self.savecfg['portc4'] = str(ports['PORTC4'])
		self.savecfg['portu6'] = str(ports['PORTU6'])
		self.savecfg['portc6'] = str(ports['PORTC6'])
		os.environ['PORTU'] = str(ports['PORTU4'])
		os.environ['PORTC'] = str(ports['PORTC4'])
		os.environ['PORTD'] = str(ports['PORTD4'])
		os.environ['PORTU6'] = str(ports['PORTU6'])
		os.environ['PORTC6'] = str(ports['PORTC6'])
		os.environ['PORTD6'] = str(ports['PORTD6'])
		if self.autoport:
			self.mlog('[auto] portu=%d'%ports['PORTU4'])
			self.mlog('[auto] portc=%d'%ports['PORTC4'])
			if ports['PORTD4'] > 0:
				self.mlog('[auto] portd=%d'%ports['PORTD4'])
			if ports['PORTU6'] > 0:
				self.mlog('[auto] portu6=%d'%ports['PORTU6'])
			if ports['PORTC6'] > 0:
				self.mlog('[auto] portc6=%d'%ports['PORTC6'])
			if ports['PORTD6'] > 0:
				self.mlog('[auto] portd6=%d'%ports['PORTD6'])
		self.__environ['casuald.up'] = ts
		self.__environ['casuald.portu'] = self.savecfg['portu4']
		self.__environ['casuald.portc'] = self.savecfg['portc4']
		self.__environ['casuald.header'] = self.savecfg['head']

	def start (self, service, console = 0):
		self.daemoned = 0
		if not service in self.config:
			print 'can not find service %s definition in %s'%(service, self.config.subdir('conf'))
			return -1

		self.current = service
		self.config.initdir(service)
		self.config.load(service)

		self.autoport = self.option('autoport', 0)

		if not self.autoport:
			if self.__listen_try() != 0:
				return -2

		if not console:
			self.daemon()

		os.chdir(self.config._dir_home)

		self.transmod = ctransmod(self.config._dir_bin)
		self.mlog('cannon start %s service ...'%service)

		if self.__setup() != 0:
			return -3
		
		self.ready = True

		if self.unix:
			import signal
			signal.signal(signal.SIGTERM, self.sig_term)
			signal.signal(signal.SIGQUIT, self.sig_exit)
			signal.signal(signal.SIGINT, self.sig_exit)
			signal.signal(signal.SIGCHLD, self.sig_chld)
			signal.signal(signal.SIGABRT, self.sig_exit)
			signal.signal(signal.SIGHUP, self.sig_hup)
			signal.signal(signal.SIGUSR1, self.sig_usr1)
		
		retval = self.transmod.startup()

		if retval != 0:
			self.mlog('[ERROR] transmod cannot startup')
			return -4

		for i in xrange(200):
			if self.transmod.status(CTMS_SERVICE) == CTM_RUNNING:
				break
			time.sleep(0.005)

		if self.transmod.status(CTMS_SERVICE) != CTM_RUNNING:
			self.mlog('[ERROR] transmod quit')
			return -5

		self.mlog('transmod startup')
		self.refresh_cfg = True

		self.__policy()
		
		self.__envinit(service)

		hr = self.__run()

		if hr != 0:
			execute = self.option('exec', '')
			self.mlog('[ERROR] run error: %s'%execute)

		self.__loop()
		
		self.config.mm_close()
		self.mlog('service %s stop'%(service))

		return 0

	def stop (self, service):
		if not service in self.config:
			print 'can not find service %s definition in %s'%(service, self.config.subdir('conf'))
			return -1
		pid = self.config.readpid(service)
		if pid < 0:
			print 'service %s is not running'%(service)
			return -2
		if self.config.kill(pid) != 0:
			print 'can not kill process (%d)'%pid
			return -3
		self.config.erasepid(service)
		return 0
	
	def restart (self, service, console = 0):
		if not service in self.config:
			print 'can not find service %s definition in %s'%(service, self.config.subdir('conf'))
			return -1
		print 'stop %s'%service
		self.stop(service)
		time.sleep(3)
		print 'start %s'%service
		self.start(service, console)
	
	def refresh (self, service):
		if not service in self.config:
			print 'can not find service %s definition in %s'%(service, self.config.subdir('conf'))
			return -1
		pid = self.config.readpid(service)
		if pid < 0:
			print 'service %s is not running'%(service)
			return -2
		if not self.config.unix:
			print 'unsupported operation in windows'
			return -3
		print 'refresh %s'%service
		import signal
		if not 'SIGUSR1' in signal.__dict__:
			return -4
		if self.config.kill(pid, signal.SIGUSR1) != 0:
			print 'can not send SIGUSR1 to process (%d)'%pid
			return -5
		return 0
	
	def dump (self, service, what):
		if not service in self.config:
			t = 'can not find service %s definition in %s'%(service, self.config.subdir('conf'))
			sys.stderr.write(t + '\n')
			return -1
		pid = self.config.readpid(service)
		if pid < 0:
			sys.stderr.write('service %s is not running\n'%(service))
			return -2
		text = self.config.mm_read(service)
		if text == None:
			sys.stderr.write('cannot read shared memory in service %s\n'%service)
			return -3
		if len(text) < 32:
			sys.stderr.write('error read information in service %s\n'%service)
			return -4
		md5s = text[:32]
		text = text[32:]
		import hashlib
		if hashlib.md5(text).hexdigest().lower() != md5s.lower():
			sys.stderr.write('error information crc checksum in service %s\n'%service)
			return -5
		try:
			import cPickle
			data = cPickle.loads(text)
		except:
			sys.stderr.write('error unpack information in service %s\n'%service)
			return -6
		try: id = int(what)
		except: id = 0
		if id < 0 or id >= len(data): 
			print ''
		else:
			print data[id]
		return 0
	
	def post (self, service, what):
		if not service in self.config:
			t = 'can not find service %s definition in %s'%(service, self.config.subdir('conf'))
			sys.stderr.write(t + '\n')
			return -1
		pid = self.config.readpid(service)
		if pid < 0:
			sys.stderr.write('service %s is not running\n'%(service))
			return -2
		import hashlib
		what = what[:208]
		md5s = hashlib.md5(what).hexdigest().lower()
		hr = self.config.mm_msg_put(service, md5s + what)
		if not hr:
			sys.stderr.write('can not write message\n')
		else:
			print 'post %d bytes to %s'%(len(what), service)
		return 0


#----------------------------------------------------------------------
# validate
#----------------------------------------------------------------------
def validate():
	path = os.path.split(os.path.abspath(sys.argv[0]))[0]
	cclib = os.path.join(path, 'cclib')
	if sys.platform[:3] == 'win': cclib += '.pyd'
	else: cclib += '.so'
	if not os.path.exists(cclib):
		return -1
	if False:
		dll = ctypes.cdll.LoadLibrary(cclib)
		validate = dll.validate
		validate.argtypes = []
		validate.restype = ctypes.c_int
		retval = validate()
		if retval != 0:
			print 'validate failed'
			sys.exit(0)
	return 0


#----------------------------------------------------------------------
# main entry
#----------------------------------------------------------------------
def info():
	ctm = ctransmod()
	ver = ctm._build_version
	desc = 'Transmod %d.%02x'%(ver >> 8, ver & 255)
	desc += ', %s'%ctm._build_date
	desc += ', %s'%ctm._build_time
	desc = 'Cannon %s (%s)'%(VERSION, desc)
	desc += ' on %s'%sys.platform
	return desc

def main():
	service = cservice()
	if len(sys.argv) <= 2:
		print info()
		if len(sys.argv) == 1:
			print 'usage: cannon.py [start|stop|restart] service [-console]'
			print '   or  cannon.py [post|dump] service [message]'
		else:
			print 'available services:'
			service.query()
		print ''
		return 0
	cmd = sys.argv[1].lower()
	name = sys.argv[2]
	console = 0
	if len(sys.argv) >= 4: 
		enable = ('console', '-c', '--c', '--console', '-console')
		if sys.argv[3].lower() in enable:
			console = 1
	if not cmd in ('start', 'stop', 'restart', 'refresh', 'dump', 'post'):
		print 'error command'
		return -1
	if cmd == 'start':
		#validate()
		service.start(name, console)
	elif cmd == 'stop':
		service.stop(name)
	elif cmd == 'restart':
		service.restart(name, console)
	elif cmd == 'refresh':
		service.refresh(name)
	elif cmd == 'dump':
		id = (len(sys.argv) >= 4) and sys.argv[3] or ''
		service.dump(name, id)
	elif cmd == 'post':
		part = ' '.join(sys.argv[3:])
		text = part.strip('\r\n\t ')
		if not text:
			print 'can not post empty message'
		else:
			service.post(name, text)
	return 0


#----------------------------------------------------------------------
# testing case
#----------------------------------------------------------------------
if __name__ == '__main__':
	def test1():
		tm = ctransmod()
		tm.option(portu = 3000, portc = 3008, logmk = 0x0f, portd=3000, head=2)
		tm.startup()
		raw_input()
	def test2():
		config = configure()
		print config.option('echoserver', 'portu')
		print config.pathshort('d:/program files/')
		config.initdir('echoserver')
		config.load('echoserver')
		print config.pidname('echoserver')
		print config.kill(os.getpid(), 0)
		raw_input()
		return 0
	def test3():
		service = cservice()
		service.query()
	def test4():
		service = cservice()
		service.start('echoserver')
		#service.stop('echoserver')
		print 'hehe'
		raw_input()
	#test4()
	main()


