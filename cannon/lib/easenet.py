#! /usr/bin/env python
# -*- coding: utf-8 -*-
#======================================================================
#
# easenet.py - easy network toolbox
#
# NOTE:
# for more information, please see the readme file.
#
#======================================================================
import sys
import time

try:
	import json
except:
	try: import simplejson as json
	except: pass

#----------------------------------------------------------------------
# OBJECT：enchanced object
#----------------------------------------------------------------------
class OBJECT (object):
	def __init__ (self, **argv):
		for x in argv: self.__dict__[x] = argv[x]
	def __getitem__ (self, x):
		return self.__dict__[x]
	def __setitem__ (self, x, y):
		self.__dict__[x] = y
	def __delitem__ (self, x):
		del self.__dict__[x]
	def __contains__ (self, x):
		return self.__dict__.__contains__(x)
	def __len__ (self):
		return self.__dict__.__len__()
	def __repr__ (self):
		line = [ '%s=%s'%(k, repr(v)) for k, v in self.__dict__.iteritems() ]
		return 'OBJECT(' + ', '.join(line) + ')'
	def __str__ (self):
		return self.__repr__()
	def __iter__ (self):
		return self.__dict__.__iter__()


#----------------------------------------------------------------------
# json serializing
#----------------------------------------------------------------------
if 'json' in sys.modules[__name__].__dict__:
	class MyEncoder(json.JSONEncoder):
		def default(self, obj):
			if not isinstance(obj, OBJECT):
				return super(MyEncoder, self).default(obj)
			return obj.__dict__

	MyDecoder = lambda obj: (type(obj) == dict) and OBJECT(**obj) or obj

	def object_dumps (obj):
		return json.dumps(obj, cls = MyEncoder)

	def object_loads (txt):
		return json.loads(txt, object_hook = MyDecoder)


#----------------------------------------------------------------------
# 取得调用栈
#----------------------------------------------------------------------
def callstack ():
	import cStringIO, traceback
	sio = cStringIO.StringIO()
	traceback.print_exc(file = sio)
	return sio.getvalue()


#----------------------------------------------------------------------
# itask 线程任务
#----------------------------------------------------------------------
class itask (object):
	def run (self):				# 线程池调用的主函数，self.__env__是环境字典
		return 0
	def done (self):			# 主线程调用，如果 run 无异常
		return 0
	def error (self, what):		# 主线程调用，如果 run 抛出异常了
		return 0
	def final (self):			# 主线程调用，结束调用，释放资源用
		return 0


#----------------------------------------------------------------------
# pooltask 工作线程池
#----------------------------------------------------------------------
import threading
import Queue

class pooltask (object):

	# 初始化，name为名称 num为需要启动多少个线程
	def __init__ (self, name = '', num = 1, slap = 0.05):
		self.name = name
		self.stderr = None
		self.__q1 = Queue.Queue(0)
		self.__q2 = Queue.Queue(0)
		self.__threads = []					# 线程池
		self.__stop_unconditional = False	# 强制停止标志，有该标志就停止
		self.__stop_empty_queue = False		# 完成停止标志，有该标志且无新任务才退出
		self.__active_thread_num = 0		# 活跃数量
		self.__thread_num = num				# 线程数量
		self.__lock_main = threading.Lock()
		self.__lock_output = threading.Lock()
		self.__cond_main = threading.Condition()

		self.__slap = slap
		self.__environ = [ OBJECT() for n in xrange(num) ]
		self.__envctor = None				# 线程环境初始化构造函数
		self.__envdtor = None				# 线程环境结束时析构函数
		
	# 内部函数：输出错误
	def __output (self, text):
		self.__lock_output.acquire()
		if self.stderr:
			self.stderr.write(text + '\n')
		self.__lock_output.release()
		return 0
	
	# 内部函数：线程主函数，从请求队列取出 task执行完 run后，塞入结果队列
	def __run (self, env):
		self.__active_thread_num += 1
		name = threading.currentThread().name
		env.NAME = name
		if self.__envctor:
			self.__envctor(env)
		while not self.__stop_unconditional:
			if self.__stop_empty_queue:
				break
			node = None
			self.__cond_main.acquire()
			try:
				if 0 == self.__q1.qsize():
					#5秒的超时设置
					self.__cond_main.wait(5)					
				else:
					node = self.__q1.get(True, self.__slap)
				
			except Queue.Empty, e:				
				pass			
			self.__cond_main.release()
			if None == node:
				continue

			if node != None:
				ts = time.time()
				hr = None
				try:
					node.task.__env__ = env
					hr = node.task.run()
					node.task.__env__ = None
					node.hr = hr
					node.ex = None
					node.ts = time.time() - ts
					node.ok = True
				except Exception, e:
					node.hr = hr
					node.ex = e
					node.ts = time.time() - ts
					node.ok = False
					self.__output('[%s] error in run(): %s'%(name, e))
					for line in callstack().split('\n'):
						self.__output(line)
				except:
					node.hr = hr
					node.ex = None
					node.ts = time.time() - ts
					node.ok = False
					self.__output('[%s] unknow error in run()'%(name))
					for line in callstack().split('\n'):
						self.__output(line)
				node.task.__env__ = None
				self.__q2.put(node)
			self.__q1.task_done()
			if node == None:
				break
		if self.__envdtor:
			self.__envdtor(env)
		self.__active_thread_num -= 1
		return 0
	
	# 内部函数：更新状态，从结果队列取出task 并执行 done, error, final
	def __update (self, limit = -1):
		count = 0
		while True:
			#为了在需要发送大量包的时候，保证其他操作能够被调用到，一次最多发送50个包
			if count > limit and limit >= 0:
				break
			
			try:
				node = self.__q2.get(False)
			except Queue.Empty, e:
				break
			if node != None:
				name = 'main'
				node.task.__error__ = node.ex
				node.task.__elapse__ = node.ts
				if node.ok:
					try: 
						node.task.done()
					except Exception, e: 
						self.__output('[main] error in done(): %s'%(e))
						for line in callstack().split('\n'):
							self.__output(line)
					except:
						self.__output('[main] unknow error in done()')
						for line in callstack().split('\n'):
							self.__output(line)
				else:
					try: 
						node.task.error(node.ex)
					except Exception, e: 
						self.__output('[main] error in error(): %s'%e)
						for line in callstack().split('\n'):
							self.__output(line)
					except:
						self.__output('[main] unknow error in error()')
						for line in callstack().split('\n'):
							self.__output(line)
				try: 
					node.task.final()
				except Exception, e: 
					self.__output('[main] error in final(): %s'%(e))
					for line in callstack().split('\n'):
						self.__output(line)
				except:
					self.__output('[main] unknow error in final()')
					for line in callstack().split('\n'):
						self.__output(line)
				count += 1
			self.__q2.task_done()
			if node == None:
				break
		return count

	# 将一个 task塞入请求队列
	def push (self, task, update = False):
		node = OBJECT(task = task)
		
		self.__cond_main.acquire()
		self.__q1.put(node)
		if update:
			#self.__update()
			pass
		self.__cond_main.notify()
		self.__cond_main.release()


		return 0

	# 更新状态：从结果队列取出task 并执行 done, error, final
	# limit为每次运行多少条主线程消息，< 0 为持续处理完所有消息 ，这里默认数值为50，主要是防止连续收发很多包导致主线程卡住
	def update (self, limit = 50):
		self.__update(limit)

	# 等待线程池处理完所有任务并结束
	def join (self, timeout = 0, discard = False):
		self.__lock_main.acquire()
		self.__stop_empty_queue = True
		if self.__threads:
			self.__stop_unconditional = False
			if discard:
				self.__stop_unconditional = True
			self.__update()
			ts = time.time()
			while self.__active_thread_num > 0:
				#唤醒全部等待中的线程
				self.__cond_main.acquire()				
				self.__cond_main.notifyAll()
				self.__cond_main.release()

				self.__update()
				time.sleep(0.01)
				if timeout > 0:
					if time.time() - ts >= timeout:
						break
			timeout = timeout * 0.2
			if timeout < 0.2:
				timeout = 0.2
			slap = timeout / self.__thread_num
			for th in self.__threads:
				th.join(slap)
			self.__update()
			self.__threads = []
		self.__lock_main.release()
		return 0
	
	# 开始线程池：envctor/envdtor分别是线程环境的初始化反初始化函数
	# 如果提供的话，每个线程开始都会调用 envctor(env)，结束前都会调用
	# envdtor(env), 其中 env是每个独立线程特有的环境变量，是一个 OBJECT
	def start (self, envctor = None, envdtor = None):
		hr = False
		self.__lock_main.acquire()
		if not self.__threads:
			self.__q1 = Queue.Queue(0)
			self.__q2 = Queue.Queue(0)
			self.__stop_unconditional = False
			self.__stop_empty_queue = False
			self.__active_thread_num = False
			self.__envctor = envctor
			self.__envdtor = envdtor
			for i in xrange(self.__thread_num):
				name = self.name + '.%d'%(i + 1)
				env = self.__environ[i]
				th = threading.Thread(None, self.__run, name, [env])
				th.setDaemon(True)
				th.start()
				self.__threads.append(th)
			hr = True
		self.__lock_main.release()
		return hr
	
	# 强制结束线程池
	def stop (self):
		self.__lock_main.acquire()
		self.__stop_empty_queue = True
		self.__stop_unconditional = True
		self.__lock_main.release()
		self.__cond_main.acquire()				
		self.__cond_main.notifyAll()
		self.__cond_main.release()
	
	# 取得运行环境
	def __getitem__ (self, id):
		return self.__environ[id]
	
	# 取得请求队列的任务数量
	def qsize (self):
		return self.__q1.qsize()
	
	#get send queue size
	def sendqsize (self):
		return self.__q2.qsize()



#----------------------------------------------------------------------
# interval 周期控制
#----------------------------------------------------------------------
class interval (object):
	def __init__ (self, period):
		self.period = period
		self.current = time.time()
		self.slap = self.current + self.period
		self.count = 0
	def check (self):
		self.current = time.time()
		if self.current < self.slap:
			return False
		if self.current - self.slap > self.period * 5:
			self.slap = self.current
		self.slap += self.period
		self.count += 1
		return True



#----------------------------------------------------------------------
# timer 时钟消息
#----------------------------------------------------------------------
class timer (object):

	def __init__ (self):
		self.current = time.time()
		self.timers = {}
		self.newid = 0
	
	def __timer_set (self, delay, repeat, fn, *args, **argv):
		if len(self.timers) >= 0x70000000:
			raise Exception('too many timers')
		if delay <= 0:
			raise Exception('timer delay must greater than zero')
		obj = OBJECT(id = self.newid, fn = fn, repeat = repeat)
		while 1:
			if not self.newid in self.timers:
				obj.id = self.newid
				break
			self.newid += 1
			if self.newid >= 0x70000000:
				self.newid = 0
		self.timers[obj.id] = obj
		self.current = time.time()
		obj.args = args
		obj.argv = argv
		obj.delay = delay
		obj.slap = self.current + obj.delay
		return obj.id
	
	def __timer_clear (self, id):
		if not id in self.timers:
			raise KeyError('not find timer id %d'%id)
		del self.timers[id]
		return 0
	
	def __timer_update (self):
		self.current = time.time()
		removal = []
		for id, obj in self.timers.items():
			while self.current >= obj.slap:
				obj.fn(*obj.args, **obj.argv)
				obj.slap += obj.delay
				if obj.repeat > 0:
					obj.repeat -= 1
					if obj.repeat == 0:
						removal.append(id)
						break
		for id in removal:
			if id in self.timers:
				del self.timers[id]
		return 0
	
	def set_timeout (self, delay, fn, *args, **argv):
		return self.__timer_set(delay, 1, fn, *args, **argv)
	
	def set_interval (self, delay, fn, *args, **argv):
		return self.__timer_set(delay, 0, fn, *args, **argv)
	
	def clear_timeout (self, id):
		return self.__timer_clear(id)
	
	def clear_interval (self, id):
		return self.__timer_clear(id)
	
	def update (self):
		return self.__timer_update()
		
	def reset (self):
		self.timers = {}


#----------------------------------------------------------------------
# 简易任务
#----------------------------------------------------------------------
class simpletask (itask):
	def __init__ (self, run, callback, *args, **argv):
		self.args = args
		self.argv = argv
		self.taskrun = run
		self.callback = callback
		self.hr = None
	def run (self):
		self.hr = self.taskrun(*self.args, **self.argv)
	def final (self):
		self.callback(self.hr)
		self.args = None
		self.argv = None
		self.taskrun = None
		self.callback = None
		self.hr = None


#----------------------------------------------------------------------
# 随机字符串
#----------------------------------------------------------------------
import random
random_chars = 'AaBbCcDdEeFfGgHhIiJjKkLlMmNnOoPpQqRrSsTtUuVvWwXxYyZz0123456'
def random_str(randomlength = 8):
	str = ''
	length = len(random_chars) - 1
	rand = random.Random().randint
	for i in range(randomlength):
		str += random_chars[rand(0, length)]
	return str

#----------------------------------------------------------------------
# 工具函数
#----------------------------------------------------------------------
import hashlib
def md5sum(text):
	return hashlib.md5(text).hexdigest()

def sha1sum(text):
	return hashlib.sha1(text).hexdigest()

def timestamp(ts = None):
	if not ts: ts = time.time()
	return time.strftime('%Y%m%d%H%M%S', time.localtime(ts))

def readts(ts):
	try: return time.mktime(time.strptime(ts, '%Y%m%d%H%M%S'))
	except: pass
	return 0

# 数字签名
def md5sign(*args):
	return md5sum(''.join([ str(x) for x in args ]))

# 时间差
def timeminus(ts1, ts2):
	t1 = readts(ts1)
	t2 = readts(ts2)
	return t1 - t2


#----------------------------------------------------------------------
# testing case
#----------------------------------------------------------------------
if __name__ == '__main__':

	class tk(itask):
		def __init__ (self, id):
			self.id = id
		def run(self):
			print '[%s] run %d'%(threading.currentThread().name, self.id)
			time.sleep(0.1)
			return -1
		def done (self):
			print '[%s] done %d'%(threading.currentThread().name, self.id)
		def error (self, e):
			print '[%s] error %d %s'%(threading.currentThread().name, self.id, e)
		def final(self):
			print '[%s] elapse %f %d'%(threading.currentThread().name, self.__elapse__, self.id)


	th = pooltask('mytask', 10)
	th.stderr = sys.stderr
	th.start()
	while 1:
		time.sleep(0.05)
		th.update()
		t = tk(int(time.time()))
		th.push(t)

	'''
	def test1():
		friendlst = OBJECT(uid=10, friend=[1,2,3,4])
		baseinfo = OBJECT(uid = 10, name = 'jack', flist = friendlst)
		print baseinfo
		txt = object_dumps(baseinfo)
		print txt
		print object_loads(txt)
		return 0
	def test2():
		q = Queue.Queue(0)
		print dir(q)
		try:
			q.ddd()
			q.get(False, 0.1)
		except Queue.Empty, e:
			print 'empty'
		except Exception, e:
			print 'fuck', e
		return 0
	def test3():
		def f():
			time.sleep(10000)
		th = threading.Thread(target = f)
		th.start()
		th.join(0.1)
		return 0
	def test4():
		class tk(itask):
			def __init__ (self, id):
				self.id = id
			def run(self):
				print '[%s] run %d'%(threading.currentThread().name, self.id)
				time.sleep(0.1)
				return -1
			def done (self):
				print '[%s] done %d'%(threading.currentThread().name, self.id)
			def error (self, e):
				print '[%s] error %d %s'%(threading.currentThread().name, self.id, e)
			def final(self):
				print '[%s] elapse %f %d'%(threading.currentThread().name, self.__elapse__, self.id)
		def bootstrap(env):
			print '[BOOT]', env
		th = pooltask('mytask', 4)
		th.stderr = sys.stderr
		th.start(bootstrap)
		for i in xrange(32):
			th.push(tk(i))
		th.join()
		th.join()
		th.join()
		return 0
	def test5():
		i = interval(1.0)
		while 1:
			time.sleep(0.001)
			if i.check():
				print time.time()
		return 0
	def test6():
		tm = timer()
		id = 0
		def fn1(name):
			print 'fn1(%s)'%name
			#tm.clear_interval(id)
			tm.set_timeout(1, fn1, 'repeat')
		id = tm.set_timeout(3, fn1, 100)
		while 1:
			time.sleep(0.001)
			tm.update()
	def test7():
		ts = timestamp()
		print ts
		print readts(ts)
		print time.time()
		print md5sign('adsf', '1234', 10)
		return 0
	test7()

	'''
