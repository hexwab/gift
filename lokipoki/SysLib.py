#!/usr/bin/env python

#
# SysLib Library, this lib takes care of all the system issues
# Written by Bart Verwilst <verwilst@gentoo.org>
#

from qt import QFileDialog, QMessageBox
import sys, os
import declare

class SysLib:
	def __init__(self):
		self.win = declare.win
		self.config = declare.config
		self.sock = declare.sock

		if sys.platform[:5] == "linux":
			self.pathArray = [os.getcwd() + "/giFT", "/bin/giFT", "/usr/bin/giFT", "/usr/local/bin/giFT"]
		elif sys.platform == "win32":
			self.pathArray = [os.getcwd() + "\giFT.exe", "c:\program files\giFT\giFT.exe"]

	def FindgiFTexec(self):
		self.config.OptionList["giFTexecLoc"] = ""
		for x in self.pathArray:
			if os.access(x, os.X_OK):
				self.config.OptionList["giFTexecLoc"] = x
				break
		if len(self.config.OptionList["giFTexecLoc"]) == 0: ## Binary not found
			QMessageBox.warning(self.win, "giFT executable not found!", "LokiPoki couldn't find your 'giFT' server binary.  \nPlease press \"ok\" to select it manually.")
			filename= str(QFileDialog.getOpenFileName("giFT", "giFT", self.win, "Find giFT executable"))
			self.config.OptionList["giFTexecLoc"] = filename

	def KillAll(self):
		if self.config.OptionList["ShutdowngiFT"] == "True": ## Shutdown giFT on exit
			self.sock.DoDisconnect()
			if sys.platform[:5] == "linux":
				os.system("killall " + self.config.OptionList["giFTexecLoc"])
				os.system("killall giFT") ## Just in case the user started it manually

	def CheckRunninggiFT(self):
		self.win.chkStartgiFT.setChecked(True)
		giFTrunning = os.popen("ps xa | grep giFT | grep -v grep").read()

		if len(giFTrunning) == 0: ## It's not running
			print "NOT RUNNING"
			os.system("giFT &") ## start it then
