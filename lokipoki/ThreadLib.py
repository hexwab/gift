#!/usr/bin/env python

#
# ThreadLib Library, this lib takes care of all the threading support
# Written by Bart Verwilst <verwilst@gentoo.org>
#
from qt import *
from TreeViewItem import *
import declare

class ThreadExec(QThread): ## This thread executes the actual Functions in a new thread ##
	def __init__(self, action, extra): #win, sock, syslib, widlib, action, extra):
		QThread.__init__(self)
		self.win = declare.win
		self.sock = declare.sock
		self.syslib = declare.syslib
		self.widlib = declare.widlib
		self.action = action ## What should the thread execute?
		self.mutex = QMutex()
		self.stopped = 0
		self.extra = extra
		

	def stop(self):
		self.mutex.lock()
		self.stopped = 1
		self.mutex.unlock()

	def run(self):
		self.mutex.lock()
		if self.stopped:
			self.stopped = 0
		self.mutex.unlock()

		if self.action == "Connect":
			self.sock.DoConnect()
		elif self.action == "ScanSockets":
			self.sock.ScanSockets()
		elif self.action == "UpdateStats":
			self.sock.UpdateStats()
		elif self.action == "FindgiFTexec":
			self.syslib.FindgiFTexec()
		elif self.action == "CheckRunninggiFT":
			self.syslib.CheckRunninggiFT()
		#elif self.action == "AddSearchListItem":
			#self.widlib.AddSearchListItem(self.extra[0], self.extra[1], self.extra[2], self.extra[3], 
				#						self.extra[4], self.extra[5])
