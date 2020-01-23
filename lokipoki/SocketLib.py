#!/usr/bin/env python

#
# SocketLib Library, takes care of the socket connections
# Written by Bart Verwilst <verwilst@gentoo.org>
#

import socket, os, select, time, re, sys, string
import ParseLib
import urllib
import declare
from TreeViewItem import *

class SocketLib:
	def __init__(self):
		self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM) ## Our connection
		self.sock2 = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
		## Global Declarations ##
		self.Connected = "False" ## Are we connected or not?
		self.DataBuffer = "" ## Collect all incoming data in this for further processing
		self.win = declare.win ## inherit win to be able to work with the GUI
		self.plib = declare.plib
		self.MaxItems = declare.MaxItems
		self.NrItems = "0" ## Stop search if NrItems = MaxItems
		self.currentVersion = declare.lokiversion
		self.newestVersion = ""
		self.SearchItemURL = declare.SearchItemURL
		self.SearchItemHash = declare.SearchItemHash ## Contains the hashes of the items
		self.SearchItemUser = declare.SearchItemUser ## Contains username (ip) of user
		self.SearchItemSize = declare.SearchItemSize ## Filesize of item
		self.SearchItemSaveName = declare.SearchItemSaveName ## Name that it will be saved as
			
	def UpdateStats(self):
		
		while 1:
			try:
				if self.Connected == "True":
					time.sleep(0.25)
					self.sock.send("""STATS ;""")
					time.sleep(10)
			except:
				pass

	def DoConnect(self):
		while self.Connected != "True":
			time.sleep(0.2)
			#try:
			self.sock.connect(("127.0.0.1", 1213))
			self.sock2.connect(("127.0.0.1", 1213))
			self.sock.setblocking(0)
			self.sock.send("""ATTACH client(LokiPoki) version(0.1) ;""")
			time.sleep(1)
			self.sock.send("""STATS ;""")
			time.sleep(0.2)
			self.sock.send("""DOWNLOADS [5] ;""")
			time.sleep(0.2)
			self.Connected = "True"

	def DoDisconnect(self):
		if self.Connected == "True":
			#try:
			self.sock.send("""DETACH ;""")
			time.sleep(0.2)
			self.Connected = "False"
#			except:
	#			print "Could not connect!"


	def ScanSockets(self):
		split_re = re.compile(r'(?<!\\);')
		while 1:
			if self.Connected == "True":
			# Scans sockets if there is data to be read #
				#try:

				rd,wr,ex = select.select([self.sock],[],[],0)
				if rd: ## Data to be read?
					x = self.sock.recv(1024)
					self.DataBuffer = self.DataBuffer + x

				## Search for full commands, if found, seperate and send to parser, keep the rest
				ParsedData = re.split(split_re, self.DataBuffer, 1) 
				#print ParsedData[0]
				if len(ParsedData) == 2: ## Command found! split in [0] (command) and [1] (the rest)
					
					Result = ParseLib.parse_server_object(ParsedData[0] + ";") ## Parse it, and get tuple
					self.DataBuffer = ParsedData[1] ## new buffer, with command already removed
					self.plib.SortTags(self.sock, Result)
					
				if len(self.DataBuffer) < 1000:
					time.sleep(0.2) # Don't eat too much CPU because of the loop :o)
					
				#except: ## Oops, failure?
					#pass
			else:
				time.sleep(0.2) # Don't loop too fast if we're not connected

	def SubmitSearch(self): ## Start a search with the specified search terms
		declare.ID_List.sort()
		SearchID = str(int(declare.ID_List[-1]) + 1)
		declare.ID_List.append(SearchID)
		declare.CurrentSearchID = SearchID
		
		## Clear the whole SearchList stuff
		self.win.lstSearch.clear()
		self.plib.SItemCount = 1
		self.plib.SItemList = []
		if len(str(self.win.txtSearch.text())) > 0: ## At least if you provides search terms :o)
			
			## Get the realm
			insert1 = {}
			insert2 = {}
			if self.win.btnEverything.isChecked():
				pass
			elif self.win.btnAudio.isChecked():
				insert2['realm'] = "audio"
			elif self.win.btnVideo.isChecked():
				insert2['realm'] = "video"
				
			insert1['SEARCH'] = SearchID
			insert2['query'] = str(self.win.txtSearch.text())# 'META/bitrate': ('192', '==')}
		
			z = ParseLib.tree_insert(insert1)
			z[ParseLib.children] = ParseLib.tree_insert(insert2)
			
			#print ParseLib.parse_client_object(z)
			self.sock.send(ParseLib.parse_client_object(z))
			
			
	def StartDownload(self): ## Start a new download

		ID = str(self.win.lstSearch.currentItem().text(0)) ## ID of selected item
		print self.SearchItemURL[ID]
		
		self.sock.send("ADDSOURCE user(" + self.SearchItemUser[ID] + ") hash(" + self.SearchItemHash[ID] + 
		") size(" + self.SearchItemSize[ID] + ") url(" + self.SearchItemURL[ID] + ") save(" +
		self.SearchItemSaveName[ID] + ") ;")
		
	def BrowseContactShares(self): ## Browse the selected user's shared files.
		print "Browsing"

	def AutoUpdateLokiPoki(self): ## Check for updates, download + install
		#if sys.platform[:5] == "linux":
		#	os.system("http_proxy=\"http://proxy.pandora.be:808\" && export http_proxy")
		f = urllib.urlopen("http://lokipoki.sourceforge.net/updates/autoupdate")
		
		params = f.readlines()
		params[0] = string.strip(params[0]) ## Check-var
		params[1] = string.strip(params[1]) ## VERSION
		if params[0] == "LOKIPOKI": ## We got the page OK
			v = string.split(params[1],"=")
			self.newestVersion = string.strip(v[1])
			
			if self.currentVersion < self.newestVersion: ## Update available
				print "UPDATE AVAILABLE"
		
		
