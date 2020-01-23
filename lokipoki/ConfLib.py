#!/usr/bin/env python

#
# ConfLib Library, takes care of Configuration Issues
# Written by Bart Verwilst <verwilst@gentoo.org>
#

import os
import string
import declare 

class ConfLib:
	def __init__(self):
		self.win = declare.win
		self.configFile=os.path.join(os.environ["HOME"],".lokipoki_rc") ## Config file used by LokiPoki
		self.OptionList = {}
		self.OptionList["StartgiFT"] = "False" ## autostart giFT server on startup
		self.OptionList["WinMaximized"] = "True" ## Start maximized
		self.OptionList["Width"] = "700" ## Set screenwidth
		self.OptionList["Height"] = "500" ## Set screenheight
		self.OptionList["ShutdowngiFT"] = "False" ## Shutdown giFT when LokiPoki closes?
		self.OptionList["giFTexecLoc"] = "/usr/bin/giFT" ## Default Location of giFT binary

		if os.access(self.configFile, os.F_OK + os.R_OK) == 0:
			self.f = open(self.configFile, "w")
			self.f.write("StartgiFT=\"False\"\n")
			self.f.write("WinMaximized=\"False\"\n")
			self.f.write("Width=\"700\"\n")
			self.f.write("Height=\"500\"\n")
			self.f.write("ShutdowngiFT=\"False\"\n")
			self.f.write("giFTexecLoc=\"/usr/bin/giFT\"\n")
			self.f.close()


	def init(self):
	## Initialize all variables

		for x in open(self.configFile):
			x = string.split(x, "=")
			self.OptionList[x[0]] = string.strip(x[1].replace("\"",""))
	    

	def SaveAll(self):
		if self.win.chkStartgiFT.isChecked():
			self.OptionList["StartgiFT"] = "True"
		else:
			self.OptionList["StartgiFT"] = "False"

		if self.win.chkShutdowngiFT.isChecked():
			self.OptionList["ShutdowngiFT"] = "True"
		else:
			self.OptionList["ShutdowngiFT"] = "False"

		if self.win.isMaximized():
			self.OptionList["WinMaximized"] = "True"
		else:
			self.OptionList["WinMaximized"] = "False"
			self.OptionList["Width"] = str(self.win.width())
			self.OptionList["Height"] = str(self.win.height())

		#print self.OptionList

		## Then write it to the config file ##
		self.f = open(self.configFile, "w")
		self.f.write("StartgiFT=\"" + self.OptionList["StartgiFT"] + "\"\n")
		self.f.write("WinMaximized=\"" + self.OptionList["WinMaximized"] + "\"\n")
		self.f.write("Width=\"" + self.OptionList["Width"] + "\"\n")
		self.f.write("Height=\"" + self.OptionList["Height"] + "\"\n")
		self.f.write("ShutdowngiFT=\"" + self.OptionList["ShutdowngiFT"] + "\"\n")
		self.f.write("giFTexecLoc=\"" + self.OptionList["giFTexecLoc"] + "\"\n")
		self.f.close()


