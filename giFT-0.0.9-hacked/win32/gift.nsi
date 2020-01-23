# $Id: gift.nsi,v 1.41 2003/06/01 03:29:08 rossta Exp $
# See README.nsis for details on use.

!ifndef GIFT_DATE
!define GIFT_DATE ""
!endif

!ifndef DEBUG
!define DEBUG	"d"
!endif

!define PACKAGE       "giFT"

!ifndef VERSION
!define VERSION       "0.10.0"
!endif

!ifndef KCEASY_VERSION
!define KCEASY_VERSION	"0.6"
!endif

!define GIFT_DESC     "${PACKAGE} ${VERSION} ${GIFT_DATE}"

!ifdef DEBUG
 !define CONFIGURATION "Debug"
 !define CONF          "_d"
 !define OUTFILE       "${PACKAGE}-${VERSION}-cvs.exe"
!else
 !define CONFIGURATION "Release"
 !define CONF          ""
 !define OUTFILE       "${PACKAGE}-${VERSION}.exe"
!endif

!ifndef CYGWIN_DIR
!define CYGWIN_DIR    "c:\cygwin\bin"
!endif

!ifndef GIFTCURS_DIR
!define GIFTCURS_DIR  "r:\src\giftcurs\src"
!endif

!ifndef KCEASY_DIR
!define KCEASY_DIR    "e:\giFT\KCeasy"
!endif

!ifndef MSVCRTD_DIR
!define MSVCRTD_DIR   "c:\winnt\system32"
!endif

!ifndef NSIS_DIR
!define NSIS_DIR      "C:\Progra~1\NSIS"
!endif

!ifndef VORBIS_DIR
!define VORBIS_DIR    "d:\usr\src\OGGVOR~1.0\bin"
!endif

!ifndef WIFT_DIR
!define WIFT_DIR      "r:\src\wift"
!endif

SetCompressor bzip2

!ifndef CLASSIC_UI
  !define MUI_PRODUCT   "${PACKAGE} ${VERSION}"
  !define MUI_VERSION   "${GIFT_DATE}"

  !include "${NSISDIR}\Contrib\Modern UI\System.nsh"

  !define MUI_WELCOMEPAGE
  !define MUI_LICENSEPAGE
  !define MUI_COMPONENTSPAGE
  !define MUI_DIRECTORYPAGE
  !define MUI_FINISHPAGE
  
#  !define MUI_FINISHPAGE_SHOWREADME "$INSTDIR\Docs\index.html"
  !define MUI_FINISHPAGE_NOREBOOTSUPPORT
  
  !define MUI_ABORTWARNING
  !define MUI_UNINSTALLER
  !define MUI_UNCONFIRMPAGE

  ;NSIS updates no system files
  !define MUI_TEXT_WELCOME_INFO "\r\n\r\nThis will install ${MUI_PRODUCT} on your computer.\r\n\r\n\r\n\r\n"

  !insertmacro MUI_LANGUAGE "English"

#  !define MUI_UI "${NSISDIR}\Contrib\UIs\modern2.exe"
  
  !insertmacro MUI_SYSTEM
  
!endif

#Name "${GIFT_DESC}"

#LicenseText "Copyright (C) 2001-2002 giFT project (http://giftproject.org/). All Rights Reserved."
LicenseData tmp\COPYING
OutFile "${OUTFILE}"
#Icon gift.ico
#ComponentText "This will install ${GIFT_DESC} on your system."
#DirText "Select a directory to install giFT in:"

InstType "Full (giFT and all giFT clients)"
InstType "Lite (giFT only)"

XPStyle On

#EnabledBitmap "${NSIS_DIR}\bitmap1.bmp"
#DisabledBitmap "${NSIS_DIR}\bitmap2.bmp"

InstallDir "$PROGRAMFILES\giFT"
InstallDirRegKey HKEY_LOCAL_MACHINE "Software\giFT\giFT" "instpath"
SetOverwrite on
ShowInstDetails show

#------------------------------------------------------------------------------------------
Section "giFT"
	SectionIn 1 2

	#First try to shut down giFTtray/giFT
	FindWindow $1 "giFTtray"
	SendMessage $1, WM_CLOSE, 0, 0
	Sleep 1000

	SetOutPath $INSTDIR\OpenFT
	File tmp\OpenFT.conf
	File ..\data\OpenFT\nodes

	SetOutPath $INSTDIR\Gnutella
	File tmp\Gnutella.conf
	File ..\data\Gnutella\gwebcaches

	SetOutPath $INSTDIR\FastTrack
	File tmp\nodes

	SetOutPath $INSTDIR\data
	File ..\data\mime.types

	SetOutPath $INSTDIR\data\OpenFT
	File ..\data\OpenFT\robots.txt

	CreateDirectory "$INSTDIR\completed"
	CreateDirectory "$INSTDIR\incoming"

	# needs to come *after* the above
	SetOutPath $INSTDIR

!ifdef DEBUG
	File ${MSVCRTD_DIR}\MSVCRTD.DLL
!endif

!ifndef USE_MSVC_BINARIES
	File giFTtray\giFTtray.exe
	File giFTsetup\giFTsetup.exe
	File ..\src\giFT.exe
#	File ${VORBIS_DIR}\vorbis${CONF}.dll
#	File ${VORBIS_DIR}\vorbisfile${CONF}.dll
#	File ${VORBIS_DIR}\ogg${CONF}.dll
!else
	File giFTtray\${CONFIGURATION}\giFTtray.exe
	File giFTsetup\${CONFIGURATION}\giFTsetup.exe
	File ${CONFIGURATION}\giFT.exe
!endif

	File tmp\gift.conf
	File tmp\README
	File tmp\COPYING
	File tmp\README
	File tmp\AUTHORS
	File tmp\NEWS
 
	WriteRegStr HKEY_LOCAL_MACHINE "Software\Microsoft\Windows\CurrentVersion\Uninstall\giFT" "DisplayName" "${GIFT_DESC}"
	WriteRegStr HKEY_LOCAL_MACHINE "Software\Microsoft\Windows\CurrentVersion\Uninstall\giFT" "UninstallString" '"$INSTDIR\ungiFT.exe"'
	WriteUninstaller "ungift.exe"

	# Registering install path, so future installs will use the same path
	WriteRegStr HKEY_LOCAL_MACHINE "Software\giFT\giFT" "instpath" $INSTDIR

	WriteRegStr HKEY_CURRENT_USER "Software\giFT\giFT" "instpath" $INSTDIR
	
	# It writes it as C:\Program Files\giFT\incoming
	# as giFTsetup will read it and translate it to
	# /C/Program Files/giFT/incoming
	# when it is first run
	# this is because $INSTDIR may not be c:\program files
	WriteINIStr $INSTDIR\gift.conf download incoming $INSTDIR\incoming
	WriteINIStr $INSTDIR\gift.conf download completed $INSTDIR\completed
SectionEnd

#------------------------------------------------------------------------------------------
Section "Add giFTtray to Startup Folder"
	SectionIn 1 2
	# WriteRegStr HKEY_CURRENT_USER "Software\Microsoft\Windows\CurrentVersion\Run" "giFTtray" '"$INSTDIR\giFTtray.exe"'
	CreateShortCut "$SMSTARTUP\giFTtray.lnk" "$INSTDIR\giFTtray.exe" "" "$INSTDIR\giFTtray.exe" 0 SW_SHOWMINIMIZED
SectionEnd

#------------------------------------------------------------------------------------------
Section "Add giFT to Start/Programs Menu"
	SectionIn 1 2
	CreateDirectory "$SMPROGRAMS\giFT"
# now that we have giFTtray, we don't want a deamon icon
#	CreateShortCut "$SMPROGRAMS\giFT\giFT daemon.lnk" "$INSTDIR\giFT.exe" "" "$INSTDIR\giFT.exe" 0

	CreateShortCut "$SMPROGRAMS\giFT\giFTtray.lnk" "$INSTDIR\giFTtray.exe" "" "$INSTDIR\giFTtray.exe" 0
	CreateShortCut "$SMPROGRAMS\giFT\giFTsetup.lnk" "$INSTDIR\giFTsetup.exe" "" "$INSTDIR\giFTsetup.exe" 0

	CreateShortCut "$SMPROGRAMS\giFT\Readme file.lnk" "$WINDIR\NOTEPAD.EXE" "$INSTDIR\README"
#	CreateShortCut "$SMPROGRAMS\giFT\Edit gift.conf.lnk" "$WINDIR\NOTEPAD.EXE" "$INSTDIR\gift.conf"
#	CreateShortCut "$SMPROGRAMS\giFT\Edit OpenFT.conf.lnk" "$WINDIR\NOTEPAD.EXE" "$INSTDIR\OpenFT\OpenFT.conf"

	WriteINIStr    "$SMPROGRAMS\giFT\giFT Home Page.url" "InternetShortcut" "URL" "http://giftproject.org/"
	WriteINIStr    "$SMPROGRAMS\giFT\giFT Project Home Page.url" "InternetShortcut" "URL" "http://sourceforge.net/projects/gift/"

	CreateShortCut "$SMPROGRAMS\giFT\Uninstall giFT.lnk" "$INSTDIR\ungiFT.exe"
SectionEnd

Section "giFTcurs (ncurses front end)"
	SectionIn 1

	SetOutPath $INSTDIR

	File "${GIFTCURS_DIR}\giFTcurs.exe"
	File "${CYGWIN_DIR}\cygiconv-2.dll"
	File "${CYGWIN_DIR}\cygintl-2.dll"
	File "${CYGWIN_DIR}\cygncurses6.dll"
	File "${CYGWIN_DIR}\cygwin1.dll"
	
	CreateDirectory "$SMPROGRAMS\giFT"
	CreateShortCut "$SMPROGRAMS\giFT\giFTcurs (ncurses front end).lnk" "$INSTDIR\giFTcurs.exe" "" "$INSTDIR\giFTcurs.exe" 0
SectionEnd

# Doesn't work any more
#Section "wiFT (Delphi front end)"
#	SectionIn 1
#	SetOutPath $INSTDIR
#	File ${WIFT_DIR}\wiFT.exe
#	CreateDirectory "$SMPROGRAMS\giFT"
#	CreateShortCut "$SMPROGRAMS\giFT\wiFT (Delphi front end).lnk" "$INSTDIR\wiFT.exe" "" "$INSTDIR\wiFT.exe" 0
#SectionEnd

Section "KCeasy ${KCEASY_VERSION} (C++ Builder front end)"
	SectionIn 1

	SetOutPath $INSTDIR
	File ${KCEASY_DIR}\*.exe
	File ${KCEASY_DIR}\*.conf
	
	CreateDirectory "$SMPROGRAMS\giFT"
	CreateShortCut "$SMPROGRAMS\giFT\KCeasy (C++ Builder front end).lnk" "$INSTDIR\KCeasy.exe" "" "$INSTDIR\KCeasy.exe" 0
SectionEnd

#------------------------------------------------------------------------------------------
#Section "Add giFT to Desktop"
#	SectionIn 1 2
#	CreateShortCut "$DESKTOP\giFTcurs (recommended).lnk" "$INSTDIR\giFTcurs.exe" "" "$INSTDIR\giFTcurs.exe" 0
#	CreateShortCut "$DESKTOP\giFT daemon.lnk" "$INSTDIR\giFT.exe" "" "$INSTDIR\giFT.exe" 0
#SectionEnd

#------------------------------------------------------------------------------------------
#Section "Add giFT to QuickLaunch Folder"
#	SectionIn 1 2
#	CreateShortCut "$QUICKLAUNCH\Start giFT daemon.lnk" "$INSTDIR\giFT.exe" "" "$INSTDIR\giFT.exe" 0 SW_SHOWMINIMIZED
#SectionEnd

#------------------------------------------------------------------------------------------
#Section "View Readme File"
#	SectionIn 1 2
#	ExecWait "$WINDIR\NOTEPAD.EXE $INSTDIR\README"
#SectionEnd

Function .onInstSuccess
#  MessageBox MB_YESNO|MB_ICONQUESTION \
#             "Setup has completed. Edit gift.conf now?" \
#             IDNO NoGift
#    ExecWait "$WINDIR\NOTEPAD.EXE $INSTDIR\gift.conf"
#

#  ExecWait "$INSTDIR\giFTsetup.exe"

  MessageBox MB_YESNO|MB_ICONQUESTION \
             "Would you like to start giFT now?" \
             IDNO NoGift
    Exec "$INSTDIR\giFTtray.exe"
  NoGift:
#  NoGiftConf:
FunctionEnd

#------------------------------------------------------------------------------------------
Section -PostInstall
	ExecWait "$INSTDIR\giFTsetup.exe"
#	MessageBox MB_OK|MB_ICONINFORMATION|MB_TOPMOST `${GIFT_DESC} has been successfully installed.`
SectionEnd

#UninstallText "This will uninstall ${GIFT_DESC}"

#------------------------------------------------------------------------------------------
# Uninstall part begins here:
Section Uninstall
	#First trying to shut down the node, the system tray Window class is called: giFTTray
	FindWindow $1 "giFTtray"
	SendMessage $1, WM_CLOSE, 0, 0
	Sleep 1000

	DeleteRegValue HKEY_LOCAL_MACHINE "Software\Microsoft\Windows\CurrentVersion\Uninstall\giFT" "UninstallString"
	DeleteRegValue HKEY_LOCAL_MACHINE "Software\Microsoft\Windows\CurrentVersion\Uninstall\giFT" "DisplayName"
	DeleteRegKey HKEY_LOCAL_MACHINE "Software\Microsoft\Windows\CurrentVersion\Uninstall\giFT"

	DeleteRegKey HKEY_LOCAL_MACHINE "Software\giFT\giFT"

	DeleteRegKey HKEY_CURRENT_USER "Software\giFT\giFT"

	Delete $INSTDIR\OpenFT\*.*
	RMDir /r $INSTDIR\OpenFT
	Delete $INSTDIR\data\OpenFT\*.*
	RMDir /r $INSTDIR\data\OpenFT
	Delete $INSTDIR\*.*
	RMDir /r $INSTDIR

	Delete "$SMPROGRAMS\giFT\*.*"
	RMDir /r "$SMPROGRAMS\giFT"

#	Delete "$DESKTOP\giFTcurs.lnk"
#	Delete "$DESKTOP\giFT daemon.lnk"

#	Delete "$QUICKLAUNCH\Start giFT.lnk"
#	Delete "$QUICKLAUNCH\Start giFT daemon.lnk"

	Delete "$SMSTARTUP\giFTtray.lnk"
SectionEnd
