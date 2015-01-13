#! /usr/bin/env python
# -*- coding: utf-8 -*-
#======================================================================
#
# flashsvr.py - socket policy server
#
# NOTE:
# for more information, please see the readme file.
#
#======================================================================
import sys
import time
import os
import socket
import errno


#----------------------------------------------------------------------
# FlashPolicy
#----------------------------------------------------------------------
class policysvr (object):

	def __init__ (self):
		self._clients = {}
		self._sock4 = None
		self._sock6 = None
		self._timeout = 10
		self._errd = [ errno.EINPROGRESS, errno.EALREADY, errno.EWOULDBLOCK ]
		if 'WSAEWOULDBLOCK' in errno.__dict__:
			self._errd.append(errno.WSAEWOULDBLOCK)
		self._errd = tuple(self._errd)
		self._history = []
		self._count = 0
		self._startts = time.time()
		self._startdt = time.strftime('%Y-%m-%d %H:%M:%S')
	
	def startup (self, port = 8430, policy = '', timeout = 10):
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
		self._timeout = timeout
		self._history = []
		self._count = 0
		self._startts = time.time()
		self._startdt = time.strftime('%Y-%m-%d %H:%M:%S')
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
		client = [ sock, '', '', 0, self._current + self._timeout, remote ]
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
				self._count += 1
				ts = time.strftime('%Y-%m-%d %H:%M:%S')
				self._history.append('[%s] %s'%(ts, str(remote)))
				if len(self._history) > 16:
					self._history = self._history[-16:]
			elif rbuf[:4].lower() == 'get ' and rbuf[-4:] == '\r\n\r\n':
				request = rbuf[4:rbuf.find('\n')].strip('\r\n')
				if request[-8:-3] == 'HTTP/':
					request = request[:-9]
				try:
					hr = self.http(request.strip('\r\n\t '), rbuf, remote)
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
		if request == '/':
			p = sys.platform
			output = 'Flash Polcy Server 1.2 (on %s)\r\n'%p
			p, t = self._startdt, (time.time() - self._startts)
			c = self._count
			output += 'running for %d seconds (since %s) and handle %d requests\r\n'%(t, p, c)
			if self._history:
				output += '\r\nlogging:\n'
				for x in self._history:
					output += x + '\r\n'
			return ('200 OK', 'text/plain', output)
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
# daemon
#----------------------------------------------------------------------
def daemon():
	if sys.platform[:3] == 'win':
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
	return 0


#----------------------------------------------------------------------
# signals
#----------------------------------------------------------------------
closing = False

def sig_exit (signum, frame):
	global closing
	closing = True

def signal_initialize():
	import signal
	signal.signal(signal.SIGTERM, sig_exit)
	signal.signal(signal.SIGINT, sig_exit)
	signal.signal(signal.SIGABRT, sig_exit)
	if 'SIGQUIT' in signal.__dict__:
		signal.signal(signal.SIGQUIT, sig_exit)
	if 'SIGPIPE' in signal.__dict__:
		signal.signal(signal.SIGPIPE, signal.SIG_IGN)
	return 0


#----------------------------------------------------------------------
# main
#----------------------------------------------------------------------
def main(argv = None):
	argv = [ n for n in (argv and argv or sys.argv) ]
	import optparse
	p = optparse.OptionParser('usage: %prog [options] to start policy server')
	p.add_option('-p', '--port', dest = 'port', help = 'flash policy server listening port')
	p.add_option('-f', '--filename', dest = 'filename', metavar='FILE', help = 'policy filename')
	p.add_option('-i', '--pid', dest = 'pid', help = 'pid file path')
	p.add_option('-t', '--timeout', dest = 'timeout', help = 'connection time out (in second)')
	p.add_option('-d', '--daemon', action = 'store_true', dest = 'daemon', help = 'run as daemon')
	options, args = p.parse_args(argv) 
	if not options.port: 
		print 'No listening port. Try --help for more information.'
		return 2
	if not options.filename:
		print 'No policy filename. Try --help for more information.'
		return 2
	try:
		port = int(options.port)
	except:
		print 'bad port number "%s"'%options.port
		return 3
	filename = options.filename
	if filename:
		if not os.path.exists(filename):
			filename = None
	if not filename:
		print 'invalid file name'
		return 4
	try:
		text = open(filename, 'rb').read()
	except:
		print 'cannot read %s'%filename
		return 5
	timeout = 10
	if options.timeout:
		try:
			timeout = int(options.timeout)
		except:
			print 'bad timeout "%s"'%options.timeout
			return 6

	svr = policysvr()
	if svr.startup (port, text, timeout) != 0:
		print 'can not listen port %d'%port
		svr.shutdown()
		return 1
	if options.daemon:
		if sys.platform[:3] == 'win':
			print 'daemon mode does support in windows'
		elif not 'fork' in os.__dict__:
			print 'can not fork myself'
		else:
			daemon()
	if options.pid:
		try:
			fp = open(options.pid, 'w')
			fp.write('%d'%os.getpid())
			fp.close()
		except:
			pass
	
	if sys.platform[:3] != 'win':
		signal_initialize()

	try:
		while not closing:
			svr.process()
			time.sleep(0.1)
	except KeyboardInterrupt:
		pass

	svr.shutdown()

	if options.pid:
		if os.path.exists(options.pid):
			os.remove(options.pid)

	return 0


#----------------------------------------------------------------------
# testing case
#----------------------------------------------------------------------
if __name__ == '__main__':
	def test1():
		#main(['--daemon'])
		#main(['--help'])
		main(['--filename=../conf/flashpolicy.xml', '--pid=fuck.pid', '--port=8430', '--timeout=5'])
		return 0
	#test1()
	#main(['--help'])
	#main(['--port=1111'])
	main()


