import struct
import socket

_prf = '='

# the total size for a message is H
# the following function only use for ncsmsg.playerlsinfo
# construct msgs from raw pkt
def unmarshalls (pkstr, cls):
	i, msgs, tfmt = 0, [], _prf+'I'
	ofs = struct.calcsize (tfmt)

	while len(pkstr[i:]) > 0 :
		j = i + ofs
		sz = struct.unpack (tfmt, pkstr[i:j])[0]
		o = cls ()
		o.unmarshal(pkstr[j:j+sz])
		msgs.append(o)
		i = j + sz
	return msgs

def marshalls (objs) :
	raw, tfmt = '', _prf+'I'

	for i in objs :
		m = i.marshal()
		s = struct.pack (tfmt, len(m))
		raw = raw + s + m
	return raw



def getmsgtype (msg) :
	return struct.unpack (_prf+'H', msg[0:2])[0]

def ntop (nip) :
	raw = struct.pack (_prf + 'I', nip) 
	return socket.inet_ntop (socket.AF_INET, raw)

def pton(ipstr):
	ip = socket.inet_aton(ipstr)
	return struct.unpack(_prf + 'I', ip)[0]



class header(object) :
	def __init__(self, type):
		self.type = type 
		self.hfmt = _prf + 'H'
		# you must define the bfmt in the subclass .
		self.bfmt = None 
		self.raw = ''

		self.char_for_len = 'I'
		self.offset  = struct.calcsize(_prf + self.char_for_len)
	
	def fmtctor (self, raw) :
		x = self.bfmt.count('%')
		if x == 0 :
			return self.bfmt 
		begin, elen, lst, fmt = 0, 0, [], self.bfmt
		self.offset  = struct.calcsize(_prf + self.char_for_len)
		for i in range(x) :
			end = fmt.index ('%', begin)
			#elen = elen + struct.calcsize(fmt[end:end+3]%(s))
			elen = elen + struct.calcsize(_prf + fmt[begin:end])
			#s = struct.unpack (_prf + 'I', raw[elen-4:elen])[0]
			s = struct.unpack (_prf + self.char_for_len, raw[elen - self.offset:elen])[0]
			elen = elen + s
			lst.append(s)
			begin = end + len('%ds') 
		if elen != 0 :
			return fmt%tuple(lst)

	def marshal (self):
		self.raw = struct.pack (self.hfmt, self.type)
		# self.fixbfmt()
		ofmt, self.bfmt = self.bfmt,  _prf + self.bfmt 
		self.raw = self.raw + self.imarshal()
		self.bfmt = ofmt

		return self.raw 

	def unmarshal(self, raw = None):
		if raw is not None :
			self.raw = raw 
		i = struct.calcsize(self.hfmt)
		# need not to unpack self.type, whitch is determined by class
		record = struct.unpack (self.hfmt, self.raw[0:i])
		if self.type != record[0] :
			raise TypeError('type dismatch when unmarshal.expect:%d,actual:%d'\
							%(self.type,record[0]))
		bfmt = _prf + self.fmtctor(self.raw[i:])
		record = struct.unpack (bfmt, self.raw[i:])
		self.iunmarshal (record)
		return self

	def fixbfmt (self):
		if self.bfmt[0:len(_prf)] != _prf :
			self.bfmt = _prf + self.bfmt 

class lazy_header(header):
	def __init__(self, msgtype):
		super(lazy_header, self).__init__(msgtype)
		self.bfmt = ''
		self.params_name = []
	
	def append_param(self, pname, pvalue, ptype):
		if ptype.strip() == 's':
			self.bfmt += self.char_for_len
			self.params_name.append(None)
			ptype = '%ds'

		self.bfmt += ptype
		self.params_name.append(pname)
		self.__setattr__(pname, pvalue)

	def imarshal(self):
		values = []
		tmp = []

		lastp = True
		for pname in self.params_name:
			if pname:
				v = self.__getattribute__(pname)
				if not lastp:
					values.append( len(v) )
					tmp.append( len(v) )
				values.append( v )
			lastp = pname

		return struct.pack(self.bfmt % tuple(tmp), *values)

	def iunmarshal(self, record):
		for i in range(len(record)):
			pname = self.params_name[i]
			if pname:
				self.__setattr__(pname, record[i])

	def test(self):
		print
		print self.__class__
		s = self.marshal()
		print '%10s: %r'%('oristr', s)

		for pname in self.params_name:
			if pname:
				self.__delattr__(pname)

		obj = self.unmarshal(s)
		for pname in self.params_name:
			if pname:
				print "%10s: %r" % (pname, self.__getattribute__(pname))

	def __repr__(self):
		text = '%s' % self.__class__
		try:
			text = text.split("'")[1] 
		except:
			pass

		text = '[%s]'%text

		for pname in self.params_name:
			if pname:
				text += " %s:%r" % (pname,self.__getattribute__(pname))
		return text


if __name__ == "__main__":
	MSG_TEST = 1
	class test(lazy_header):
		def __init__(self, ip=''):
			super(test, self).__init__(0)
			## (param_name, param_value, param_type)
			self.append_param("ip", ip, 's')
	s = test('aaa').marshal()
	print '%r'%s
	print '%r'%s[0:2]
	print '%r'%s[2:4]
	print '%r'%s[4:]


	t = test('tttt tttt tttt tttt')
	print 't:', t

	d = t.marshal()
	print 't.marshal():%r'% d

	tt = test().unmarshal(d)
	print "test().unmarshal(d):", tt

	print "tt.marshal(): %r" %tt.marshal()
	tt.ip = 'bbbbbbbbbbbbbb'
	print 'tt.marshal():%r'%tt.marshal()
	
