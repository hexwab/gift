# $Id: gift.nsi,v 1.19 2002/06/29 04:00:31 rossta Exp $
# See README.nsis for details on use.

!define GIFT_DATE "Sat Jun 29 02:40:22 2002 UTC"

!define PACKAGE       "giFT"
!define VERSION       "0.10.0"
!define OUTFILE       "${PACKAGE}-${VERSION}-cvs.exe"
!define GIFT_DESC     "${PACKAGE} ${VERSION} ${GIFT_DATE}"
!define CONFIGURATION "Debug"
!define NSIS_DIR      "C:\Program Files\NSIS"
!define CYGWIN_DIR    "c:\cygwin\bin"
!define GIFTCURS_DIR  "d:\home\ross\src\giftcurs\src"
!define GTK_DIR       "d:\gtk\lib"
!define KCEASY_DIR    "f:\giFT\KCeasy"
!define WIFT_DIR      "s:\wift"

Name "${GIFT_DESC}"

LicenseText "Copyright (C) 2001-2002 giFT project (http://giftproject.org/). All Rights Reserved."
LicenseData tmp\COPYING
OutFile "${OUTFILE}"
#Icon gift.ico
ComponentText "This will install ${GIFT_DESC} on your system."
DirText "Select a directory to install giFT in:"

InstType "Full (giFT and all giFT clients)"
InstType "Lite (giFT only)"

EnabledBitmap "${NSIS_DIR}\bitmap1.bmp"
DisabledBitmap "${NSIS_DIR}\bitmap2.bmp"

InstallDir "$PROGRAMFILES\giFT"
InstallDirRegKey HKEY_LOCAL_MACHINE "Software\giFT\giFT" "instpath"
SetOverwrite on
ShowInstDetails show

#------------------------------------------------------------------------------------------
Section "giFT"
	SectionIn 12

	#First try to shut down giFTtray/giFT
	FindWindow $1 "giFTtray"
	SendMessage $1, WM_CLOSE, 0, 0

	SetOutPath $INSTDIR\OpenFT
	File tmp\OpenFT.conf
	File ..\data\OpenFT\nodes

	SetOutPath $INSTDIR\ui
	File tmp\ui.conf

	SetOutPath $INSTDIR\data
	File ..\data\mime.types

	SetOutPath $INSTDIR\data\OpenFT
	File ..\data\OpenFT\*.css
	File ..\data\OpenFT\*.gif
	File ..\data\OpenFT\*.jpg
	File ..\data\OpenFT\robots.txt

	# needs to come *after* the above
	SetOutPath $INSTDIR

	File giFTtray\${CONFIGURATION}\giFTtray.exe

	File giFTsetup\${CONFIGURATION}\giFTsetup.exe

# if you've built the source via MSVC (gift.dsp)
	File ${CONFIGURATION}\giFT.exe

# if you've built the source via nmake /f makefile.msvc static=1
#	File ..\src\giFT.exe

# if you've built the source via nmake /f makefile.msvc
	File ..\src\giFT.exe
	File ..\src\gift.dll
	File ..\lib\libgift.dll
	File ..\OpenFT\OpenFT.dll

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
SectionEnd

#------------------------------------------------------------------------------------------
Section "Add giFTtray to Startup Folder"
	SectionIn 12
	# WriteRegStr HKEY_CURRENT_USER "Software\Microsoft\Windows\CurrentVersion\Run" "giFTtray" '"$INSTDIR\giFTtray.exe"'
	CreateShortCut "$SMSTARTUP\giFTtray.lnk" "$INSTDIR\giFTtray.exe" "" "$INSTDIR\giFTtray.exe" 0 SW_SHOWMINIMIZED
SectionEnd

#------------------------------------------------------------------------------------------
Section "Add giFT to Program Files Menu"
	SectionIn 12
	CreateDirectory "$SMPROGRAMS\giFT"
# now that we have giFTtray, we don't want a deamon icon
#	CreateShortCut "$SMPROGRAMS\giFT\giFT daemon.lnk" "$INSTDIR\giFT.exe" "" "$INSTDIR\giFT.exe" 0

	CreateShortCut "$SMPROGRAMS\giFT\giFTtray.lnk" "$INSTDIR\giFTtray.exe" "" "$INSTDIR\giFTtray.exe" 0
	CreateShortCut "$SMPROGRAMS\giFT\giFTsetup.lnk" "$INSTDIR\giFTsetup.exe" "" "$INSTDIR\giFTsetup.exe" 0

	CreateShortCut "$SMPROGRAMS\giFT\Readme file.lnk" "$WINDIR\NOTEPAD.EXE" "$INSTDIR\README"
#	CreateShortCut "$SMPROGRAMS\giFT\Edit gift.conf.lnk" "$WINDIR\NOTEPAD.EXE" "$INSTDIR\gift.conf"
#	CreateShortCut "$SMPROGRAMS\giFT\Edit OpenFT.conf.lnk" "$WINDIR\NOTEPAD.EXE" "$INSTDIR\OpenFT\OpenFT.conf"
#	CreateShortCut "$SMPROGRAMS\giFT\Edit ui.conf.lnk" "$WINDIR\NOTEPAD.EXE" "$INSTDIR\ui\ui.conf"

	WriteINIStr    "$SMPROGRAMS\giFT\giFT Home Page.url" "InternetShortcut" "URL" "http://giftproject.org/"
	WriteINIStr    "$SMPROGRAMS\giFT\giFT Project Home Page.url" "InternetShortcut" "URL" "http://sourceforge.net/projects/gift/"

	CreateShortCut "$SMPROGRAMS\giFT\Uninstall giFT.lnk" "$INSTDIR\ungiFT.exe"
SectionEnd

Section "giFTcurs (ncurses front end)"
	SectionIn 1

	SetOutPath $INSTDIR

	File "${GIFTCURS_DIR}\giFTcurs.exe"
	File "${CYGWIN_DIR}\cygintl-1.dll"
	File "${CYGWIN_DIR}\cygncurses6.dll"
	File "${CYGWIN_DIR}\cygwin1.dll"
	
	CreateDirectory "$SMPROGRAMS\giFT"
	CreateShortCut "$SMPROGRAMS\giFT\giFTcurs (ncurses front end).lnk" "$INSTDIR\giFTcurs.exe" "" "$INSTDIR\giFTcurs.exe" 0
SectionEnd

Section "giFT-fe (GTK front end)"
	SectionIn 1

	SetOutPath $INSTDIR

	File Debug\giFTfe.exe
#	File ..\ui\giFTfe.exe
	File "${GTK_DIR}\libgtk-0.dll"
	File "${GTK_DIR}\libgdk-0.dll"
	File "${GTK_DIR}\libglib-2.0-0.dll"
	File "${GTK_DIR}\libintl-1.dll"
	File "${GTK_DIR}\iconv.dll"
	File "${GTK_DIR}\libgmodule-2.0-0.dll"
	
	CreateDirectory "$SMPROGRAMS\giFT"
	CreateShortCut "$SMPROGRAMS\giFT\giFT-fe (GTK front end).lnk" "$INSTDIR\giFTfe.exe" "" "$INSTDIR\giFTfe.exe" 0
SectionEnd

Section "wiFT (Delphi front end)"
	SectionIn 1

	SetOutPath $INSTDIR
	File ${WIFT_DIR}\wiFT.exe
	
	CreateDirectory "$SMPROGRAMS\giFT"
	CreateShortCut "$SMPROGRAMS\giFT\wiFT (Delphi front end).lnk" "$INSTDIR\wiFT.exe" "" "$INSTDIR\wiFT.exe" 0
SectionEnd

Section "KCeasy (C++ Builder front end)"
	SectionIn 1

	SetOutPath $INSTDIR
	File ${KCEASY_DIR}\*.*
	
	CreateDirectory "$SMPROGRAMS\giFT"
	CreateShortCut "$SMPROGRAMS\giFT\KCeasy (C++ Builder front end).lnk" "$INSTDIR\KCeasy.exe" "" "$INSTDIR\KCeasy.exe" 0
SectionEnd

#------------------------------------------------------------------------------------------
#Section "Add giFT to Desktop"
#	SectionIn 12
#	CreateShortCut "$DESKTOP\giFTcurs (recommended).lnk" "$INSTDIR\giFTcurs.exe" "" "$INSTDIR\giFTcurs.exe" 0
#	CreateShortCut "$DESKTOP\giFT-fe.lnk" "$INSTDIR\giFTfe.exe" "" "$INSTDIR\giFTfe.exe" 0
#	CreateShortCut "$DESKTOP\giFT daemon.lnk" "$INSTDIR\giFT.exe" "" "$INSTDIR\giFT.exe" 0
#SectionEnd

#------------------------------------------------------------------------------------------
#Section "Add giFT to QuickLaunch Folder"
#	SectionIn 12
#	CreateShortCut "$QUICKLAUNCH\Start giFT.lnk" "$INSTDIR\giFTFE.exe" "" "$INSTDIR\giFTFE.exe" 0 SW_SHOWMINIMIZED
#	CreateShortCut "$QUICKLAUNCH\Start giFT daemon.lnk" "$INSTDIR\giFT.exe" "" "$INSTDIR\giFT.exe" 0 SW_SHOWMINIMIZED
#SectionEnd

#------------------------------------------------------------------------------------------
#Section "Launch giFT now"
#	SectionIn 12
#	Exec "$INSTDIR\giFT.exe"
#	Exec "$INSTDIR\giFTfe.exe"
#SectionEnd

#------------------------------------------------------------------------------------------
#Section "View Readme File"
#	SectionIn 12
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

UninstallText "This will uninstall ${GIFT_DESC}"

#------------------------------------------------------------------------------------------
# Uninstall part begins here:
Section Uninstall
	#First trying to shut down the node, the system tray Window class is called: TrayIcongiFTClass
	#FindWindow "close" "TrayIcongiFTClass" ""

	DeleteRegValue HKEY_LOCAL_MACHINE "Software\Microsoft\Windows\CurrentVersion\Uninstall\giFT" "UninstallString"
	DeleteRegValue HKEY_LOCAL_MACHINE "Software\Microsoft\Windows\CurrentVersion\Uninstall\giFT" "DisplayName"
	DeleteRegKey HKEY_LOCAL_MACHINE "Software\Microsoft\Windows\CurrentVersion\Uninstall\giFT"

	DeleteRegKey HKEY_LOCAL_MACHINE "Software\giFT\giFT"

	DeleteRegKey HKEY_CURRENT_USER "Software\giFT\giFT"

	Delete $INSTDIR\OpenFT\*.*
	RMDir /r $INSTDIR\OpenFT
	Delete $INSTDIR\ui\*.*
	RMDir /r $INSTDIR\ui
	Delete $INSTDIR\data\OpenFT\*.*
	RMDir /r $INSTDIR\data\OpenFT
	Delete $INSTDIR\*.*
	RMDir /r $INSTDIR

	Delete "$SMPROGRAMS\giFT\*.*"
	RMDir /r "$SMPROGRAMS\giFT"

#	Delete "$DESKTOP\giFTcurs.lnk"
#	Delete "$DESKTOP\giFT-FE (Highly unstable - use giFTcurs instead).lnk"
#	Delete "$DESKTOP\giFT daemon.lnk"

#	Delete "$QUICKLAUNCH\Start giFT.lnk"
#	Delete "$QUICKLAUNCH\Start giFT daemon.lnk"

	Delete "$SMSTARTUP\giFTtray.lnk"
SectionEnd
