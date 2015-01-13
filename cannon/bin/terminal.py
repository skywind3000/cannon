#! /usr/bin/env python
# -*- coding: utf-8 -*-
#======================================================================
#
# terminal.py - terminal
#
# NOTE:
# for more information, please see the readme file.
#
#======================================================================
import sys, time, os
import Tkinter

from Tkinter import YES, NO, X, Y, BOTH, LEFT, TOP, BOTTOM, RIGHT, SUNKEN


#----------------------------------------------------------------------
# Initialized & Compatible Python2.6 (ttk module from lib/ttkmod.py)
#----------------------------------------------------------------------
CHARSET = 'utf-8'

#----------------------------------------------------------------------
# Initialized & Compatible Python2.6 (ttk module from lib/ttkmod.py)
#----------------------------------------------------------------------
APPHOME = os.path.abspath(os.path.split(sys.argv[0])[0])
sys.path.append(os.path.abspath(os.path.join(APPHOME, '../lib')))

if sys.platform[:3] == 'win':
	TMP = os.environ.get('TMP', os.environ.get('TEMP', APPHOME))
	DATAFILE = os.path.join(TMP, '.terminal')
else:
	HOME = os.environ.get('HOME', APPHOME)
	DATAFILE = os.path.join(HOME, '.terminal')

# compatible Tkinter module from python2.6 to python2.7
if not 'XView' in Tkinter.__dict__:
	class XView:
		def xview(self, *args):
			res = self.tk.call(self._w, 'xview', *args)
			if not args:
				return self._getdoubles(res)
		def xview_moveto(self, fraction):
			self.tk.call(self._w, 'xview', 'moveto', fraction)
		def xview_scroll(self, number, what):
			self.tk.call(self._w, 'xview', 'scroll', number, what)
	class YView:
		def yview(self, *args):
			res = self.tk.call(self._w, 'yview', *args)
			if not args: return self._getdoubles(res)
		def yview_moveto(self, fraction):
			self.tk.call(self._w, 'yview', 'moveto', fraction)
		def yview_scroll(self, number, what):
			self.tk.call(self._w, 'yview', 'scroll', number, what)
	Tkinter.XView = XView
	Tkinter.YView = YView

import netstream

try:
	import ttk
except:
	try:
		import ttkmod as ttk
	except:
		raise Exception('can not import ttk, Tkinter.TkVersion must >= 8.5')


#----------------------------------------------------------------------
# ColorTranslate
#----------------------------------------------------------------------
ColorTranslate = {
	'#cccccc': '#333333',
	'#a0dca0': '#70ac70',
	'#c0c0c0': '#808080',
	'#ffffff': '#000000',
	'yellow':  '#ff00ff',
}


#----------------------------------------------------------------------
# Terminal
#----------------------------------------------------------------------
class Terminal (ttk.Frame):

	def __init__ (self, parent = None, **kw):
		ttk.Frame.__init__(self, parent, **kw)
		self.root = parent
		self.make_widgets()
		self.make_hotkeys()
		self.uistate = 0
		self.on_init()
		self.html = []
		self.root.after(100, self.__update_timer)
		self.mask = 0
	
	def make_widgets (self):
		self.status = ttk.Frame(self)
		self.status.pack(side = BOTTOM, expand = NO, fill = X, padx = 1, pady = 1)
		self.bottom = ttk.Frame(self)
		self.bottom.pack(side = BOTTOM, expand = NO, fill = X, padx = 1, pady = 1)
		self.client = ttk.Frame(self)
		self.client.pack(side = BOTTOM, expand = YES, fill = BOTH)
		self.top = ttk.Frame(self)
		self.top.pack(side = BOTTOM, expand = NO, fill = X)
		self.make_text(self.client)
		self.make_bottom(self.bottom)
		self.make_status(self.status)
		return 0
	
	def make_text (self, parent):
		import ScrolledText
		self.text = ScrolledText.ScrolledText(parent, relief = SUNKEN)
		self.text.pack(expand = YES, fill = BOTH)
		self.text.config(font = ('courier', 10, 'normal'))
		if sys.platform[:3] == 'win':
			self.text.config(font = ('fixedsys', 12, 'normal'))
		elif sys.platform == 'darwin':
			self.text.config(font = ('Monaco', 13, 'normal'))
		self.text.config(width = 80, height = 25)
		self.text.config(background = 'black')
		self.text.config(foreground = 'white')
		self.text.config(relief = SUNKEN, cursor = 'arrow')
		self.tags = {}
		#self.text.config(state = 'disabled')
	
	def make_bottom (self, parent):
		left = ttk.Frame(parent)
		right = ttk.Frame(parent, width = 100)
		right.pack(side = RIGHT, expand = NO, fill = X)
		left.pack(side = RIGHT, expand = YES, fill = X)
		self.entry = ttk.Entry(left)
		self.entry.pack(side = LEFT, expand = YES, fill = BOTH, padx = 2, pady = 2)
		self.entry.config(cursor = 'xterm')
		self.entry.bind('<Return>', self.on_send)
		self.entry.bind('<Up>', self.on_history_up)
		self.entry.bind('<Down>', self.on_history_down)
		w1, w2, w3, w4 = 8, 6, 20, 8
		if sys.platform[:3] != 'win':
			w1, w2, w3, w4 = 5, 6, 15, 5
		self.btn1 = ttk.Button(left, text = "SEND", width = w1, command = self.on_send)
		self.btn1.pack(side = LEFT, expand = NO)
		frame = ttk.Label(left)
		frame.pack(side = LEFT, padx = 10, pady = 0)
		self.cbox = ttk.Combobox(left, state = 'readonly')
		self.cbox.pack(side = LEFT, padx = 2, pady = 2)
		self.cbox['values'] = ('BIN', 'TEXT', 'PICKLE', 'MSGPACK')
		self.cbox.config(width = w2)
		self.cbox.set('BIN')
		self.remote = ttk.Combobox(right, width = w3)
		self.remote.pack(side = LEFT, padx = 8, pady = 2)
		self.remote.bind('<Return>', self.__remote_enter)
		self.btn2 = ttk.Button(right, text = 'GO !!', width = w4, command = self.__on_go)
		self.btn2.pack(side = RIGHT, padx = 2, pady = 2)
		#self.btn2.focus_set()
		return 0
	
	def make_status (self, parent):
		label = ttk.Frame(parent)
		label.pack(expand = YES, fill = X, side = LEFT)
		panel1 = ttk.Label(label, relief = SUNKEN, text = '123423')
		panel1.config(width = 16)
		panel1.pack(side = LEFT)
		panel2 = ttk.Label(label, relief = SUNKEN)
		panel2.pack(side = LEFT, expand = YES, fill = X)
		#panel3 = ttk.Label(panel2)
		#panel3.pack(side = LEFT, fill = X)
		self.__status1 = panel1
		self.__status2 = panel2
		self.set_status(0, 'DISCONNECTED')

	def make_menu (self, win):
		top = Tkinter.Menu(win)
		win.config(menu = top)

		m_file = Tkinter.Menu(top, tearoff = 0)
		m_file.add_command(label = 'Connect', command = self.on_connect, underline = 0)
		m_file.add_command(label = 'Disconnect', command = self.on_disconnect, underline = 0)
		m_file.add_separator()
		m_file.add_command(label = 'Export', command = self.on_export, underline = 0)
		m_file.add_separator()
		m_file.add_command(label = 'Exit', command = self.__on_exit, underline = 1)

		top.add_cascade(label = 'Terminal', menu = m_file, underline = 0)

		m_edit = Tkinter.Menu(top, tearoff = 0)
		m_edit.add_command(label = 'Copy Text to Clipboard', command = self.on_copy, underline = 0)
		m_edit.add_command(label = 'Copy Html to Clipboard', command = self.on_copy_html, underline = 5)
		m_edit.add_separator()
		m_edit.add_command(label = 'Clear', command = self.on_clear, underline = 4)
		#m_edit.add_command(label = 'Send', command = self.on_send, underline = 0)
		
		top.add_cascade(label = 'Edit', menu = m_edit, underline = 0)

		m_help = Tkinter.Menu(top, tearoff = 0)
		m_help.add_command(label = 'Content', command = self.on_help_content, underline = 0)
		m_help.add_command(label = 'Header', command = self.on_help_header, underline = 0)
		m_help.add_command(label = 'Syntax', command = self.on_help_syntax, underline = 0)
		m_help.add_separator()
		m_help.add_command(label = 'About', command = self.on_help_about, underline = 0)
		
		top.add_cascade(label = 'Help', menu = m_help, underline = 0)

		self.menu_file = m_file
		m_file.entryconfig(1, state = 'disabled')
	
	def make_hotkeys (self):
		self.root.bind('<Control-w>', self.on_hotkey_switch)
		self.root.bind('<Control-Enter>', self.on_hotkey_focus)
		self.root.bind('<Control-F5>', self.on_hotkey_c0)
		self.root.bind('<Control-F1>', self.on_hotkey_c1)
		self.root.bind('<Control-F2>', self.on_hotkey_c2)
		self.root.bind('<Control-F3>', self.on_hotkey_c3)
		self.root.bind('<Control-F4>', self.on_hotkey_c4)
	
	def set_status (self, id, text):
		if id == 0:
			self.__status1.config(text = text)
		else:
			self.__status2.config(text = text)
		return 0
	
	def switch (self, connect = False):
		if connect:
			self.menu_file.entryconfig(0, state = 'disabled')
			self.menu_file.entryconfig(1, state = 'normal')
			self.btn2.config(text = 'CLOSE')
			self.remote.config(state = 'disable')
			text = self.remote.get().strip('\r\n\t ')
			self.root.title('CTerminal - ' + text)
			self.uistate = 1
		else:
			self.menu_file.entryconfig(0, state = 'normal')
			self.menu_file.entryconfig(1, state = 'disable')
			self.btn2.config(text = 'GO !!')
			self.remote.config(state = 'normal')
			self.root.title('CTerminal')
			self.uistate = 0
		return 0
	
	def __update_timer (self, **kw):
		self.on_timer(**kw)
		self.root.after(1, self.__update_timer)
		return 0

	def __remote_enter (self, *argv, **kw):
		if not self.uistate:
			self.on_connect(**kw)
		return 0
	
	def append (self, text, color = None, html = True):
		self.text.config(state = 'normal')
		if not color:
			self.text.insert(Tkinter.END, text)
		else:
			if not color in self.tags:
				self.text.tag_config(color, foreground = color)
				self.tags[color] = 1
			self.text.insert(Tkinter.END, text, color)
		if self.text.vbar.get()[1] >= 0.98:
			self.text.yview(Tkinter.END)
		self.text.config(state = 'disable')
		if html:
			self.__html_append(text, color)
		return 0
	
	def movend (self):
		self.text.yview(Tkinter.END)
	
	def mode (self):
		text = self.cbox.get()
		return text
	
	def entry_set (self, text):
		self.entry.delete('0', Tkinter.END)
		self.entry.insert(Tkinter.END, text)
	
	def __on_exit (self, *args, **kw):
		self.on_exit()
		self.root.quit(**kw)
		return 0
	
	def __on_go (self, *args, **kw):
		if not self.uistate:
			self.on_connect(**kw)
		else:
			self.on_disconnect(**kw)
		return 0

	def on_timer (self, *args, **kw):
		return 0

	def on_clear (self, *args, **kw):
		self.text.config(state = 'normal')
		self.text.delete('1.0', Tkinter.END)
		self.text.config(state = 'disable')
		return 0
	
	def output (self, text, html = True):
		size = len(text)
		import StringIO
		sio = StringIO.StringIO()
		pos = 0
		css = '#cccccc'
		while pos < size:
			p1 = text.find('&', pos)
			if p1 < 0:
				self.append(text[pos:], css, html)
				break
			if text[p1:p1 + 2] == '&&':
				self.append(text[pos:p1], css, html)
				self.append('&', css, html)
				pos = p1 + 2
			else:
				p2 = text.find(';', p1)
				if p2 < 0:
					self.append(text[pos:p1 + 2], css)
					pos = p1 + 2
				else:
					self.append(text[pos:p1], css, html)
					css = text[p1 + 1:p2].strip('\r\n\t ')
					pos = p2 + 1
		return 0
	
	def __html_append (self, text, color):
		import cgi
		head = time.strftime('%H:%M:%S', time.localtime())
		data = cgi.escape(text).split('\n')
		index = 0
		if not color:
			color = '#000000'
		color = color.lower()
		color = ColorTranslate.get(color, color)
		for line in data:
			line = line.strip('\n').replace(' ', '&nbsp;')
			text = '<font color=%s size=2>%s</font>'%(color, line)
			if len(self.html) == 0:
				self.html.append((head, text))
			else:
				ts, dt = self.html[-1]
				self.html[-1] = (head, dt + text)
			if index < len(data) - 1:
				self.html.append((head, ''))
			index += 1
		if len(self.html) > 20000:
			self.html = self.html[-20000:]
		#self.generate_html('output2.html')
		return 0
	
	def __generate_html (self):
		import cStringIO
		sio = cStringIO.StringIO()
		sio.write('<html>\n<title>Casuald Debug Terminal</title>\n')
		sio.write('<body>\n<font size=2 face=Courier>\n')
		sio.write('<table width=800>\n')
		head = ''
		for dt, text in self.html:
			sio.write('  <tr>')
			if dt != head:
				head = dt
				sio.write('<td width=100 ><font color=#cccccc face=Courier size=2>')
				sio.write('%s</font></td> '%head)
			else:
				sio.write('<td width=100><font size=2>&nbsp;</font></td> ')
			sio.write('<td><b>%s &nbsp;</b></td>'%text)
			sio.write(' </tr>\n')
		sio.write('</table>\n</font>\n</body>\n</html>\n\n')
		return sio.getvalue()
	
	def generate_html (self, name):
		value = self.__generate_html()
		fp = open(name, 'w')
		fp.write(value)
		fp.close()
		fp = None
		return 0
	
	def on_copy (self, *args, **kw):
		root = self.root
		text = self.text.get('1.0', Tkinter.END)
		root.clipboard_clear()
		root.clipboard_append(text)
		return 0
	
	def on_copy_html (self, *args, **kw):
		root = self.root
		text = self.__generate_html()
		root.clipboard_clear()
		root.clipboard_append(text)
		return 0
	
	def on_export (self, *args, **kw):
		import tkFileDialog
		format = [ ('HTML', '*.html') ]
		dialog = tkFileDialog.asksaveasfilename
		fn = dialog(parent = self, filetypes = format, title = "Save the output as...")
		if not fn:
			return 0
		if fn[-5:].lower() != '.html':
			fn += '.html'
		self.generate_html(fn)
		self.set_status(1, 'Exported: ' + fn)
		return 0
	
	def on_hotkey_switch (self, *args, **kw):
		if self.cbox.get() == 'BIN':
			self.cbox.set('TEXT')
		elif self.cbox.get() == 'TEXT':
			self.cbox.set('PICKLE')
		elif self.cbox.get() == 'MSGPACK':
			self.cbox.set('MSGPACK')
		else:
			self.cbox.set('BIN')
		self.mask = 0
		return 0

	def on_hotkey_c0 (self, *args, **kw):
		self.mask = 0
		self.set_status(1, 'MASK 0')
		return 0

	def on_hotkey_c1 (self, *args, **kw):
		self.mask = 1
		self.set_status(1, 'MASK 1')
		return 0

	def on_hotkey_c2 (self, *args, **kw):
		self.mask = 2
		self.set_status(1, 'MASK 2')
		return 0

	def on_hotkey_c3 (self, *args, **kw):
		self.mask = 3
		self.set_status(1, 'MASK 3')
		return 0

	def on_hotkey_c4 (self, *args, **kw):
		self.mask = 4
		self.set_status(1, 'MASK 4')
		return 0
	
	def on_hotkey_focus (self, *args, **kw):
		return 0

	def on_connect (self, *args, **kw):
		return 0
	
	def on_disconnect (self, *args, **kw):
		return 0
	
	def on_exit (self, *args, **kw):
		return 0
	
	def on_send (self, *args, **kw):
		self.append('hahahah' + str(time.time()) + '\n')
		return 0
	
	def on_history_up (self, *args, **kw):
		return 0
	
	def on_history_down (self, *args, **kw):
		return 0
	
	def on_help_content (self, *args, **kw):
		return 0

	def on_help_header (self, *args, **kw):
		return 0
	
	def on_help_syntax (self, *args, **kw):
		return 0
	
	def on_help_about (self, *args, **kw):
		return 0
	
	def on_init (self):
		return 0


#----------------------------------------------------------------------
# INPUT
#----------------------------------------------------------------------
def input_parse (text):
	import tokenize, StringIO, struct
	if type(text) == type(u''):
		text = text.encode(CHARSET)
	g = tokenize.generate_tokens(StringIO.StringIO(text).readline)
	p = []
	z = ''
	for toknum, tokval, _, _, _ in g:
		if toknum == tokenize.OP:
			if tokval in (',', ':', '/'):
				p.append((toknum, tokval))
				z = ''
			elif tokval == '-':
				z = '-'
		elif toknum == tokenize.NUMBER:
			p.append((toknum, z + tokval))
			z = ''
		elif toknum in (tokenize.NAME, tokenize.STRING):
			p.append((toknum, tokval))
			z = ''
	mask = 0
	if len(p) == 0:
		return mask, ''
	if len(p) > 2:
		if p[1] == (tokenize.OP, ':'):
			if p[0][0] != tokenize.NUMBER:
				return 'error mask'
			try: 
				mask = eval(p[0][1])
			except:
				return 'error mask format'
			p = p[2:]
	for i in xrange(len(p)):
		if p[i] == (tokenize.OP, ':') and i != 1:
			return 'error colon'
	t = []
	while len(p) > 0:
		i = 0
		while i < len(p):
			if p[i] == (tokenize.OP, ','):
				break
			i += 1
		t.append(p[:i])
		p = p[i + 1:]
	bytes = []
	for n in t:
		if len(n) == 0:
			continue
		if n[0][0] == tokenize.NUMBER:
			fmt = '=B'
			try:
				x = eval(n[0][1])
			except:
				return 'error convert "%s" to number'%n[0][1]
			y = x & 0xff
			if len(n) >= 3 and n[1] == (tokenize.OP, '/'):
				mode = n[2][1].lower()
				if mode == 'b':
					fmt = '=B'
					y = x & 0xff
					if x > 255:
						return 'integer is too large to pack into byte'
				elif mode in ('w', 'iw', 'h', 'ih'):
					fmt = '<H'
					y = x & 0xffff
					if x > 65535:
						return 'integer is too large to pack into word'
				elif mode in ('mw', 'bw', 'mh', 'bh'):
					fmt = '>H'
					y = x & 0xffff
					if x > 65535:
						return 'integer is too large to pack into word'
				elif mode in ('d', 'id', 'i', 'ii'):
					fmt = '<I'
					y = x & 0xffffffff
				elif mode in ('md', 'bd', 'mi', 'bi'):
					fmt = '>I'
					y = x & 0xffffffff
				else:
					return 'error integer format "%s"'%n[2][1]
			bytes.append(struct.pack(fmt, y))
		elif len(n) == 1 and n[0][0] == tokenize.STRING:
			text = eval(n[0][1])
			if type(text) == type(u''):
				text = text.encode(CHARSET)
			bytes.append(text)
		elif len(n) == 2 and n[1][0] == tokenize.STRING:
			text = n[1][1]
			data = eval(text)
			if type(data) == type(u''):
				data = data.encode(CHARSET)
			size = len(data)
			mode = n[0][1].lower()
			head = ''
			if mode in ('b', 's'):
				if size > 255:
					return 'string is too long to pack size into byte'
				head = struct.pack('<B', size & 0xff)
			elif mode in ('w', 'h', 'iw', 'ih', '#'):
				if size > 65535:
					return 'string is too long to pack size into word'
				head = struct.pack('<H', size & 0xffff)
			elif mode in ('bw', 'bh', 'mh', 'mw'):
				if size > 65535:
					return 'string is too long to pack size into word'
				head = struct.pack('>H', size & 0xffff)
			elif mode in ('i', 'd', 'ii', 'id'):
				head = struct.pack('<I', size)
			elif mode in ('bi', 'bd', 'mi', 'md'):
				head = struct.pack('>I', size)
			else:
				return 'error string size format %s'%n[0][1]
			text = head + data
			bytes.append(text)
		elif len(n) >= 1 and n[0][0] == tokenize.NAME:
			pass
	return mask, ''.join(bytes)


#----------------------------------------------------------------------
# convert_binary
#----------------------------------------------------------------------
def convert_binary(data, char = False):
	content = ''
	charset = ''
	lines = []
	for i in xrange(len(data)):
		ascii = ord(data[i])
		if i % 16 == 0: content += '%04X  '%i
		content += '%02X'%ascii
		if i < len(data) - 1:
			content += ((i & 15) == 7) and '-' or ' '
		else:
			content += ' '
		if (ascii >= 0x20) and (ascii < 0x7f): charset += data[i]
		else: charset += '.'
		if (i % 16 == 15): 
			lines.append(content + ' ' + charset)
			content, charset = '', ''
	if len(content) < 56: content += ' ' * (54 - len(content))
	lines.append(content + ' ' + charset)
	limit = char and 100 or 54
	text = []
	for n in lines: 
		text.append(n[:limit])
	return '\n'.join(text)


#----------------------------------------------------------------------
# COLORS
#----------------------------------------------------------------------
COLOR_ERROR = 'yellow'
COLOR_STDERR = 'red'
COLOR_SEND = '#a0dca0'
COLOR_RECV = '#c0c0c0'


#----------------------------------------------------------------------
# output redirector
#----------------------------------------------------------------------
class output_handler:
	def __init__(self, writer):
		self.writer = writer
		self.content = ''
	def flush(self):
		pass
	def write(self, s):
		if isinstance(s, unicode):
			s = s.encode(CHARSET)
		self.writer(s)
	def writelines(self, l):
		map(self.write, l)


#----------------------------------------------------------------------
# MainWindow
#----------------------------------------------------------------------
class MainWindow (Terminal):
	
	def __init__ (self, parent, **kw):
		Terminal.__init__(self, parent, **kw)
		self.make_menu(parent)
		self.pack(expand = YES, fill = BOTH)
		self.sock = None
		self.state = 0
		self.history = []
		self.initialize()
	
	def initialize (self):
		values, history, mode = [], [], 'BIN'
		if os.path.exists(DATAFILE):
			text = open(DATAFILE, 'rb').read()
			import pickle
			try:
				values, history, mode = pickle.loads(text)
				pass
			except:
				pass
			self.remote['values'] = values
		if len(values) > 0:
			self.remote.set(values[0])
		else:
			self.remote.set('enter ip:port here')
		self.values = values
		self.history = history
		self.cbox.set(mode)
		import sys
		self.stderr = sys.stderr
		sys.stderr = output_handler(self.__stderr)
		self.last_mode = mode
		self.remote.select_range('0', Tkinter.END)
		self.remote.focus_set()
		return 0
	
	def save (self):
		sys.stderr = self.stderr
		fp = open(DATAFILE, 'wb')
		import pickle
		data = [self.values, self.history, self.last_mode]
		data = pickle.dumps(data, 1)
		fp.write(data)
		fp.close()
		fp = None
		return 0

	def __stderr (self, text):
		self.append(text, COLOR_STDERR)

	def on_timer (self, **kw):
		if not self.sock:
			if self.uistate:
				self.switch(False)
			if self.state != 0:
				self.net_closed()
			self.state = 0
			return 0
		self.sock.process()
		if self.sock.state == 0:
			if self.state != 0:
				self.net_closed()
		elif self.sock.state == 1:
			if self.state != 1:
				self.net_connecting()
		elif self.sock.state == 2:
			if self.state != 2:
				self.net_connected()
		self.state = self.sock.state
		if self.state == 2:
			self.update()
		return 0

	def net_closed (self):
		self.set_status(0, 'DISCONNECTED')
		self.switch(False)

	def net_connecting (self):
		self.set_status(0, 'CONNECTING')
		self.switch(True)

	def net_connected (self):
		self.set_status(0, 'CONNECTED')
		self.switch(True)
		self.entry.focus_set()

	def update (self):
		while 1:
			data = self.sock.recv()
			if not data: 
				break
			self.on_recv(data)
		return 0
	
	def address_error (self, what):
		txt = 'error target address "%s", use ip:port or ip:port:format\n'
		self.append(txt%what, COLOR_ERROR)
		return 0
	
	def on_connect (self, **kw):
		self.mask = 0
		ipv6 = False
		bracket = False
		text = self.remote.get().strip('\r\n\t ')
		addr = ''

		for ch in text:
			if not bracket:
				if ch == '[': bracket = True
			else:
				ipv6 = True
				if ch == ']': bracket = False
				elif ch == ':': ch = '.'
			addr += ch

		if not ':' in addr:
			return self.address_error(text)

		part = addr.split(':')

		if not len(part) in (2, 3):
			return self.address_error(text)
		port = 0
		try: 
			port = int(part[1])
		except: 
			return self.address_error(text)
		if port == 0:
			self.append('port can not be zero\n', COLOR_ERROR)
			return 0
		head = -1
		
		if len(part) == 3:
			try:
				head = int(part[2])
				if head < 0 or head > 13:
					self.append('format must >= 0 and <= 13\n', COLOR_ERROR)
					return 0
			except:
				pass
			names = { 'WORDLSB': 0, 'WORDMSB':1, 'DWORDLSB':2, 'DWORDMSB':3,
				'BYTELSB': 4, 'BYTEMSB': 5, 'EWORDLSB': 6, 'EWORDMSB': 7,
				'EDWORDLSB': 8, 'EDWORDMSB': 9, 'EBYTELSB': 10,
				'EBYTEMSB': 11, 'DWORDMASK': 12, 'RAWDATA': 13, 'RAW': 13 }
			if head < 0 and part[2].upper() in names:
				head = names.get(part[2].upper(), -1)
			if head < 0:
				self.append('format "%s" invalid\n'%part[2], COLOR_ERROR)
				return 0

		ips = part[0].split('.')

		if not ipv6:
			if len(ips) != 4:
				return self.address_error(text)

			try:
				n = [ int(x) for x in ips ]
				for x in n:
					if x < 0 or x > 255:
						self.append('ip address "%s" invalid\n'%part[0], COLOR_ERROR)
						return 0
			except:
				self.append('ip address "%s" invalid\n'%part[0], COLOR_ERROR)
				return 0

		if self.sock:
			self.sock.close()
			self.sock = None
		
		ip = part[0].replace('[', ' ').replace(']', ' ').strip('\r\n\t ')

		if ipv6: ip = ip.replace('.', ':')
		self.sock = netstream.netstream(head)

		try:
			self.sock.connect(ip, port, head)
		except Exception, e:
			self.append('ip address "%s" invalid: %s\n'%(text, str(e)), COLOR_ERROR)
			self.sock.close()
			self.sock = None
			return 0

		self.net_connecting()
		self.state = 1
		addrlist = [ text ]
		for n in self.values:
			if n != text:
				addrlist.append(n)
		self.values = addrlist[:10]
		self.remote['values'] = self.values
		self.remote.set(text)
		
		return 0
	
	def on_disconnect (self, **kw):
		if self.sock:
			self.sock.close()
		self.sock = None
		self.net_closed()
		self.state = 0
		return 0

	def on_init (self):
		self.text.config(state = 'disable')
		return 0

	def on_exit (self, **kw):
		if self.sock:
			self.sock.close()
			self.sock = None
		return 0
	
	def on_send (self, *args, **kw):
		if self.state != 2:
			return 0
		text = self.entry.get()
		mode = self.mode()
		if mode == 'BIN':
			try:
				data = input_parse(text)
			except:
				#self.append('error input format !!\n', COLOR_ERROR)
				self.set_status(1, 'INPUT ERROR')
				return 0
			if type(data) == type(''):
				self.append(data + '\n', COLOR_ERROR)
				return 0
			mask, output = data
		elif mode == 'TEXT':
			mask, output = self.mask, text
		elif mode == 'MSGPACK':
			mask = self.mask
			try:
				output = ''
				hr = eval(text)
				import umsgpack
				output = umsgpack.dumps(hr)
			except:
				self.set_status(1, 'SYNTAX ERROR')
				return 0
		else:
			mask = self.mask
			try:
				output = ''
				hr = eval(text)
				import pickle
				output = pickle.dumps(hr)
			except:
				#self.append('input syntax error !!\n', COLOR_ERROR)
				self.set_status(1, 'SYNTAX ERROR')
				return 0
		if len(output) == 0:
			self.set_status(1, 'EMPTY')
			return 0
		if type(output) == type(u''):
			output = output.encode(CHARSET)
		self.sock.send(output, mask)
		self.entry_set('')
		data = [ '%02X'%ord(ch) for ch in output ]
		self.set_status(1, 'SEND: %s'%(' '.join(data)))
		self.append(text + '\n', COLOR_SEND)
		if len(self.history) == 0:
			self.history.append(text)
		elif self.history[-1] != text:
			self.history.append(text)
		if len(self.history) > 100:
			self.history = self.history[-100:]
		self.last_mode = mode
		self.entry.focus_set()
		return 0
	
	def on_recv (self, data):
		mode = self.mode()
		if mode == 'BIN':
			text = convert_binary(data, True)
		elif mode == 'TEXT':
			text = data
		elif mode == 'MSGPACK':
			try:
				import umsgpack
				hr = umsgpack.loads(data)
				text = repr(hr)
			except Exception, ex:
				import traceback
				text = "recv msgpack err: %s, %s\n" % (ex, traceback.format_exc())
				text += "rdata:%r\n" % data
				text += convert_binary(data, True)
		else:
			try:
				import pickle
				hr = pickle.loads(data)
				text = repr(hr)
			except:
				text = convert_binary(data, True)
		self.append(text + '\n', COLOR_RECV)
		return 0

	def on_history_up (self, *args, **kw):
		if len(self.history) == 0:
			return 0
		text = self.history[-1]
		self.history = [text] + self.history[0:-1]
		self.entry_set(text)
		self.entry.select_range('0', Tkinter.END)
		return 0
	
	def on_history_down (self, *args, **kw):
		if len(self.history) == 0:
			return 0
		text = self.history[0]
		self.history = self.history[1:] + [text]
		self.entry_set(text)
		self.entry.select_range('0', Tkinter.END)
		return 0
	
	def on_help_content (self, *args, **kw):
		self.output(__doc__, False)
		self.movend()
		return 0

	def on_help_header (self, *args, **kw):
		self.output(__header__, False)
		self.movend()
		return 0
	
	def on_help_syntax (self, *args, **kw):
		self.output(__syntax__, False)
		self.movend()
		return 0

	def on_help_about (self, *args, **kw):
		self.output(__about__, False)
		self.movend()
		return 0

	



#----------------------------------------------------------------------
# Document
#----------------------------------------------------------------------
__doc__ = '''&#ffffff;<Manual of the Casuald Debug Terminal>
&green;
1. Address Format: &#cccccc;
   IPv4   - xxx.xxx.xxx.xxx:PORT
            xxx.xxx.xxx.xxx:PORT:HEADER
   IPv6   - [xx:xx::xx]:PORT
            [xx:xx::xx]:PORT:HEADER
   HEADER - Transmod Header Format (0-13). (more. &red;'Help->Header'&#cccccc;)
&green;
2. Data Format: &#cccccc;
   BIN    - send binary described in input box with "Binary Syntax"
   TEXT   - send the content of the input box below directly in utf-8
   PICKLE - send python object in pickle format
   MSGPACK - send python object in msgpack format
&green;
3. Binary Syntax: &#cccccc;
   Present the binary with a sequence of variables seperated by comma:
      BYTE       - a number between 0-255 directly
      LSB WORD   - num/w, num/iw
      MSB WORD   - num/mw
      LSB DWORD  - num/d, num/id
      MSB DWORD  - num/md
      STRING     - "string text", "Hello,Worl\\x64 !!"
      STRING with length  - b"..", w"..", d"..", mw"..", md".."
   use &red;'Help->Syntax'&#cccccc; for details.
-------------------------------------------------------------------------
'''

__syntax__ = '''&#cccccc;
Present the binary with a sequence of variables seperated by comma:

&#cccccc;1.&green; BYTE&#cccccc;: a number between 0-255 directly
   &#cccccc;INPUT: &yellow; 1, 2, 3, 128, 0xff
   &#cccccc;SEND:  &red; 01 02 03 80 ff

&#cccccc;2.&green; LSB WORD&#cccccc;: num/w, num/iw
   &#cccccc;INPUT: &yellow; 32767/w, 0x1000/iw, 1/w, 2, 3
   &#cccccc;SEND:  &red; FF 7F 00 10 01 00 02 03

&#cccccc;3.&green; MSB WORD&#cccccc;: num/mw
   &#cccccc;INPUT: &yellow; 32767/mw, 0x1000/mw, 1/mw, 2, 3
   &#cccccc;SEND:  &red; 7F FF 10 00 00 01 02 03

&#cccccc;4.&green; LSB DWORD&#cccccc;: num/d, num/id
   &#cccccc;INPUT: &yellow; 0x11223344/d, 0x55667788/id, 100
   &#cccccc;SEND:  &red; 44 33 22 11 88 77 66 55 64

&#cccccc;5.&green; MSB DWORD&#cccccc;: num/md
   &#cccccc;INPUT: &yellow; 0x11223344/md, 0x55667788/md, 100
   &#cccccc;SEND:  &red; 11 22 33 44 55 66 77 88 64

&#cccccc;6.&green; STRING&#cccccc;: "string text", "Hello,Worl\\x64 !!"
   &#cccccc;INPUT: &yellow; 1, 2/w ,65535/d, 0x11223344/md, "str"
   &#cccccc;SEND:  &red; 01 02 00 FF FF 00 00 11 22 33 44 73 74 72

&#cccccc;7.&green; STRING with length&#cccccc;: b"..", w"..", d"..", mw"..", md".."
   &#cccccc;INPUT: &yellow; 1, 2/w, 65535/d, 0x11223344/md, w"str"
   &#cccccc;SEND:  &red; 01 02 00 FF FF 00 00 11 22 33 44 03 00 73 74 72&#cccccc;

&#cccccc;8.&green; Input Category Mask&#cccccc;: mask: ...
   &#cccccc;INPUT: &yellow; 20: 1, 2, 3, 128, 0xff
   &#cccccc;SEND:  &red; 01 02 03 80 ff &#cccccc; (with category &red;20&#cccccc;)

-------------------------------------------------------------------------

'''

__header__ = '''
HEADER = 0  (&green;WORDLSB&#cccccc;)   LSB WORD to indicate msg size
HEADER = 1  (&green;WORDMSB&#cccccc;)   MSB WORD to indicate msg size
HEADER = 2  (&green;DWORDLSB&#cccccc;)  LSB DWORD to indicate msg size
HEADER = 3  (&green;DWORDMSB&#cccccc;)  MSB DWORD to indicate msg size
HEADER = 4  (&green;BYTELSB&#cccccc;)   LSB BYTE to indicate msg size
HEADER = 5  (&green;BYTEMSB&#cccccc;)   MSB BYTE to indicate msg size
HEADER = 6  (&green;EWORDLSB&#cccccc;)  LSB WORD to indicate msg size (exclude self)
HEADER = 7  (&green;EWORDMSB&#cccccc;)  MSB WORD to indicate msg size (exclude self)
HEADER = 8  (&green;EDWORDLSB&#cccccc;) LSB DWORD to indicate msg size (exclude self)
HEADER = 9  (&green;EDWORDMSB&#cccccc;) MSB DWORD to indicate msg size (exclude self)
HEADER = 10 (&green;EBYTELSB&#cccccc;)  LSB BYTE to indicate msg size (exclude self)
HEADER = 11 (&green;EBYTEMSB&#cccccc;)  MSB BYTE to indicate msg size (exclude self)
HEADER = 12 (&green;DWORDMASK&#cccccc;) (size:0-7, size:8-15, size:16-23, mask:0-7) 
HEADER = 13 (&green;RAWDATA&#cccccc;)   raw data format

'''

__about__ = '''&green;
  _____                   __   __  ______              _           __
 / ___/__ ____ __ _____ _/ /__/ / /_  __/__ ______ _  (_)__  ___ _/ /
/ /__/ _ `(_-</ // / _ `/ / _  /   / / / -_) __/  ' \/ / _ \/ _ `/ / 
\___/\_,_/___/\_,_/\_,_/_/\_,_/   /_/  \__/_/ /_/_/_/_/_//_/\_,_/_/  

&#ffffff;<Casuald Debug Terminal>&#cccccc;
by Skywind 2013.


'''


#----------------------------------------------------------------------
# main entry
#----------------------------------------------------------------------
def main(center = False):
	root = Tkinter.Tk()
	root.title('CTerminal')
	tm = MainWindow(root)
	tm.on_help_content()
	tm.set_status(1, "")
	if sys.platform == 'darwin':
		w, h = 680, 510
	else:
		w, h = 640, 480
	if center:
		cx = root.winfo_screenwidth()
		cy = root.winfo_screenheight()
		py = int(cy * 0.382 - (h * 0.5))
		if py < 0: py = 0
		root.geometry('%dx%d+%d+%d'%(w, h, (cx - w) / 2, py))
	else:
		root.geometry('%dx%d'%(w, h))
	root.mainloop()
	tm.save()


#----------------------------------------------------------------------
# testing case
#----------------------------------------------------------------------
if __name__ == '__main__':
	main(True)




