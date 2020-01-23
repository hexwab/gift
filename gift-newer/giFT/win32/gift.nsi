# $Id: gift.nsi,v 1.4 2002/04/30 13:56:51 rossta Exp $
# See README.nsis for details on use.

!define GIFT_MAJOR 0
!define GIFT_MINOR 10
!define GIFT_MICRO 0
!define GIFT_DATE "Mon Apr 29 10:15:17 2002 UTC"
!define GIFT_DESC "giFT ${GIFT_MAJOR}.${GIFT_MINOR}.${GIFT_MICRO} ${GIFT_DATE}"
Name "${GIFT_DESC}"

LicenseText "Copyright (C) 2001-2002 giFT project (http://giftproject.org/).  All Rights Reserved"
LicenseData tmp\COPYING
OutFile giFT${GIFT_MAJOR}${GIFT_MINOR}${GIFT_MICRO}.exe
#Icon gift.ico
ComponentText "This will install ${GIFT_DESC} on your system."
DirText "Select a directory to install giFT in:"

InstType "Full (giFT daemon and all giFT clients)"
InstType "Lite (giFT daemon only)"

EnabledBitmap bitmap1.bmp
DisabledBitmap bitmap2.bmp

InstallDir "$PROGRAMFILES\giFT"
InstallDirRegKey HKEY_LOCAL_MACHINE "Software\giFT\giFT" "instpath"
SetOverwrite on
ShowInstDetails show

#------------------------------------------------------------------------------------------
Section "giFT daemon"
	SectionIn 12

	#First trying to shut down the node, the system tray Window class is called: TrayIcongiFTClass
	#FindWindow "close" "TrayIcongiFTClass" ""

	SetOutPath $INSTDIR
	File Debug\giFT.exe

#	File ..\src\giFT.exe
#	File ..\OpenFT\OpenFT.dll

	File tmp\gift.conf
	File tmp\README
	File tmp\COPYING
	File tmp\README
	File tmp\AUTHORS
	File tmp\NEWS

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
        
	WriteRegStr HKEY_LOCAL_MACHINE "Software\Microsoft\Windows\CurrentVersion\Uninstall\giFT" "DisplayName" "${GIFT_DESC}"
	WriteRegStr HKEY_LOCAL_MACHINE "Software\Microsoft\Windows\CurrentVersion\Uninstall\giFT" "UninstallString" '"$INSTDIR\ungiFT.exe"'
	WriteUninstaller "ungift.exe"

	# Registering install path, so future installs will use the same path
	WriteRegStr HKEY_LOCAL_MACHINE "Software\giFT\giFT" "instpath" $INSTDIR

	WriteRegStr HKEY_CURRENT_USER "Software\giFT\giFT" "instpath" $INSTDIR
SectionEnd

#------------------------------------------------------------------------------------------
Section "Add giFT to Program Files Menu"
	SectionIn 12
	CreateDirectory "$SMPROGRAMS\giFT"
	CreateShortCut "$SMPROGRAMS\giFT\giFT daemon.lnk" "$INSTDIR\giFT.exe" "" "$INSTDIR\giFT.exe" 0

	CreateShortCut "$SMPROGRAMS\giFT\Readme file.lnk" "$WINDIR\NOTEPAD.EXE" "$INSTDIR\README"
	CreateShortCut "$SMPROGRAMS\giFT\Edit gift.conf.lnk" "$WINDIR\NOTEPAD.EXE" "$INSTDIR\gift.conf"
	CreateShortCut "$SMPROGRAMS\giFT\Edit OpenFT.conf.lnk" "$WINDIR\NOTEPAD.EXE" "$INSTDIR\OpenFT\OpenFT.conf"
	CreateShortCut "$SMPROGRAMS\giFT\Edit ui.conf.lnk" "$WINDIR\NOTEPAD.EXE" "$INSTDIR\ui\ui.conf"

	WriteINIStr    "$SMPROGRAMS\giFT\giFT Home Page.url" "InternetShortcut" "URL" "http://giftproject.org/"
	WriteINIStr    "$SMPROGRAMS\giFT\giFT Project Home Page.url" "InternetShortcut" "URL" "http://sourceforge.net/projects/gift/"

	CreateShortCut "$SMPROGRAMS\giFT\Uninstall giFT.lnk" "$INSTDIR\ungiFT.exe"
SectionEnd

Section "giFTcurs (ncurses front end)"
	SectionIn 1

	SetOutPath $INSTDIR

	File d:\home\ross\src\giftcurs\src\giftcurs.exe
	File c:\cygwin\bin\cygintl-1.dll
	File c:\cygwin\bin\cygncurses6.dll
	File c:\cygwin\bin\cygwin1.dll
	
	CreateDirectory "$SMPROGRAMS\giFT"
	CreateShortCut "$SMPROGRAMS\giFT\giFTcurs (ncurses front end).lnk" "$INSTDIR\giFTcurs.exe" "" "$INSTDIR\giFTcurs.exe" 0
SectionEnd

Section "giFT-fe (GTK front end)"
	SectionIn 1

	SetOutPath $INSTDIR

	File Debug\giFTfe.exe
#	File ..\ui\giFTfe.exe
	File d:\gtk\lib\libgtk-0.dll
	File d:\gtk\lib\libgdk-0.dll
	File d:\gtk\lib\libglib-2.0-0.dll
	File d:\gtk\lib\libintl-1.dll
	File d:\gtk\lib\iconv.dll
	File d:\gtk\lib\libgmodule-2.0-0.dll
	
	CreateDirectory "$SMPROGRAMS\giFT"
	CreateShortCut "$SMPROGRAMS\giFT\giFT-fe (GTK front end).lnk" "$INSTDIR\giFTfe.exe" "" "$INSTDIR\giFTfe.exe" 0
SectionEnd

Section "wiFT (Delphi front end)"
	SectionIn 1

	SetOutPath $INSTDIR
	File s:\wift\wiFT.exe
	
	CreateDirectory "$SMPROGRAMS\giFT"
	CreateShortCut "$SMPROGRAMS\giFT\wiFT (Delphi front end).lnk" "$INSTDIR\wiFT.exe" "" "$INSTDIR\wiFT.exe" 0
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
Section "Add giFT daemon to Startup Folder"
	SectionIn 12
	# WriteRegStr HKEY_CURRENT_USER "Software\Microsoft\Windows\CurrentVersion\Run" "giFT daemon" '"$INSTDIR\giFT.exe"'
	CreateShortCut "$SMSTARTUP\giFT daemon.lnk" "$INSTDIR\giFT.exe" "" "$INSTDIR\giFT.exe" 0 SW_SHOWMINIMIZED
SectionEnd

#------------------------------------------------------------------------------------------
#Section "View Readme File"
#	SectionIn 12
#	ExecWait "$WINDIR\NOTEPAD.EXE $INSTDIR\README"
#SectionEnd

Function .onInstSuccess
  MessageBox MB_YESNO|MB_ICONQUESTION \
             "Setup has completed. Edit gift.conf now?" \
             IDNO NoGiftConf
    ExecWait "$WINDIR\NOTEPAD.EXE $INSTDIR\gift.conf"
  NoGiftConf:
FunctionEnd

#------------------------------------------------------------------------------------------
Section -PostInstall
	MessageBox MB_OK|MB_ICONINFORMATION|MB_TOPMOST `${GIFT_DESC} has been successfully installed.`
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
#	Delete "$DESKTOP\giFT-FE (Highly unstable-use giFTcurs instead).lnk"
#	Delete "$DESKTOP\giFT daemon.lnk"

#	Delete "$QUICKLAUNCH\Start giFT.lnk"
#	Delete "$QUICKLAUNCH\Start giFT daemon.lnk"

	Delete "$SMSTARTUP\giFT daemon.lnk"
SectionEnd
