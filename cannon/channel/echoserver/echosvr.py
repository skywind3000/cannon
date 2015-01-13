import sys, time
import os
import ccinit
import cclib					
from instruction import *

ccinit.attach()	       # attach to transmod
ccinit.fastmode(False)
cclib.settimer(1000)   # set timer event (1000ms interval)
cclib.logmask(0x1f)

cclib.write(ITMC_SYSCD, ITMS_LOGLV, 0x1f, '', 1)

while 1:
	# receive event from transmod
	event, wparam, lparam, data = cclib.read()
	if event < 0: break  # transmod disconnected

	if event == ITMT_DATA:	# receive data sent by remote user
		cclib.send(wparam, data)
		ccinit.plog('recv: %s'%repr(data))
	elif event == ITMT_NEW: # remote user connected to transmod 
		remote = ccinit.parseip(data)
		ccinit.set_nodelay(wparam, True)
		ccinit.plog('new user hid=%d from %s'%(wparam, remote))
	elif event == ITMT_LEAVE: # remote user disconnect from transmod
		ccinit.plog('user disconnect hid=%d'%wparam)
	elif event == ITMT_TIMER: # timer event
		print 'TIMER'


