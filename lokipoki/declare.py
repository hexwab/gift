#!/usr/bin/env python

#
# This file holds all main instances, which are inherited over the whole app.
# Written by Bart Verwilst <verwilst@gentoo.org>
#

import sys, os, ConfigParser
from ThreadLib import *
from qt import *
from mainwindow import *
from SocketLib import *
from ParseLib import ParseLib
from ConfLib import *
from SysLib import *
from WidgetLib import *

app = QApplication(sys.argv)
lokiversion = "0.1" ## Current LokiPoki version

SearchStopped = "" ## ID of search that has been stopped (for use in ScanSockets)

CurrentSearchID = "0" ## ID of current search process

MaxItems = "100"

ID_List= [] ## Contains the used ID's
ID_List.append("1") ## Default value

SearchItemURL = {} ## ListID's + URL { 0 : "OpenFT://...", 1 : ...}
SearchItemHash = {} ## ListID's + Hash { 0 : "OpenFT://...", 1 : ...}
SearchItemUser = {} ## ListID's + User { 0 : "OpenFT://...", 1 : ...}
SearchItemSize = {} ## ListID's + fileSize { 0 : "OpenFT://...", 1 : ...}
SearchItemSaveName = {} ## Name that it will be saved as

win = MainForm()
plib = ParseLib() # win
sock = SocketLib() # win plib
config = ConfLib() # win
syslib = SysLib() # win config, sock
widlib = WidgetLib() # win sock

config.init()
