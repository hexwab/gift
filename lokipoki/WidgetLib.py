#!/usr/bin/env python

#
# WidgetLib Library, this lib takes care of all ListView items and such
# Written by Bart Verwilst <verwilst@gentoo.org>
#

from qt import *
from TreeViewItem import *
import declare

class WidgetLib:
	def __init__(self):
		self.win = declare.win
		self.sock = declare.sock
		self.SearchItemURL = declare.SearchItemURL
		self.SearchItemHash = declare.SearchItemHash
		self.SearchItemUser = declare.SearchItemUser
		self.SearchItemSize = declare.SearchItemSize
		
	def ShowMenu(self, button, item): ## Show the download popupbox when you rightclick an item
		if item: ## An item is selected
			if button == 2: #Rightmouse button clicked
				DownLoadPopup = QPopupMenu(self.win.lstSearch, "Popup")
				DownLoadPopup.insertItem( "&Download", self.sock.StartDownload)
				DownLoadPopup.insertItem( "&Browse Host's Shared Files", self.sock.BrowseContactShares)
				DownLoadPopup.popup(QCursor.pos()) ## Popup at the cursor's position

	def Test(self):
		MyItem = self.win.lstSearch.selectedItem()
		#MyItem.setText(1,"IK HEB GEKLIKT!")
