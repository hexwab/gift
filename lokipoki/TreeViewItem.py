#!/usr/bin/env python

#
# TreeViewItem Library, this lib takes care of all ListView items and such
# Written by Bart Verwilst <verwilst@gentoo.org>
#
from qt import *

class TreeViewItem(QListViewItem): ## Make sure the 
	def __init__(self, parent, attrs):
		QListViewItem.__init__(self, parent)
		self.attrs = {}
		self.parent = parent
		
		for i in range(self.parent.columns()):
			self.attrs[str(self.parent.columnText(i))] = attrs[i] ## Make a dictionary of the columns and their values.
			self.setText(i, attrs[i]) ## Fill listview with its values
		
	def key(self, column, ascending):
		if str(self.parent.columnText(column)) == "#": ## if column is the "#" one...
			try:
				if len(self.attrs["#"]) == 1:
					return "000000" + self.attrs["#"]
				elif len(self.attrs["#"]) == 2:
					return "00000" + self.attrs["#"]
				elif len(self.attrs["#"]) == 3:
					return "0000" + self.attrs["#"]
				elif len(self.attrs["#"]) == 4:
					return "000" + self.attrs["#"]
			except:
				return "-"
		else:
			return self.text(column) ## Otherwise just use the original values