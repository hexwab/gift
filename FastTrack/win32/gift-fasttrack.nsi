;------------------------------------------------------------------------------------------
; This file creates an installer for gift-fasttrack for use with KCeasy, giFTwin32 or
; other installations of giFT on windows. It assumes gift-fasttrack was build using the
; environment provided by giFT.
;
; This installer expects two FastTrack DLLs:
; - A FastTrack.dll compiled with RELEASE=1 for giFTwin32 and custom installs.
; - A FastTrack-Loader.dll compiled with RELEASE=1 and USE_LOADER=1 for KCeasy.
;------------------------------------------------------------------------------------------

; Compile time options
;------------------------------------------------------------------------------------------

!define VERSION "0.8.9"

!define OUTFILE "gift-fasttrack-${VERSION}.exe"

!include "MUI.nsh"

# we are supposed to be in /win32-dist
!ifndef BUILD_ROOT
!define BUILD_ROOT ".."
!endif

!ifndef DIST_DIR
!define DIST_DIR      "${BUILD_ROOT}\win32-dist"
!endif

!ifndef TMP_DIR
!define TMP_DIR       "${DIST_DIR}\tmp"
!endif


; Installer options
;------------------------------------------------------------------------------------------

Name "FastTrack plugin for giFT"
Caption "giFT-FastTrack ${VERSION} Setup"
OutFile ${OUTFILE}

#InstallDir "$PROGRAMFILES\giFT"
#InstallDirRegKey HKEY_LOCAL_MACHINE "Software\giFT\giFT" "instpath"

SetCompressor lzma
SetOverwrite on
ShowInstDetails hide

InstType "Default"

; Modern UI config
;------------------------------------------------------------------------------------------

!define MUI_COMPONENTSPAGE_SMALLDESC
!define MUI_ABORTWARNING
!define MUI_FINISHPAGE_NOREBOOTSUPPORT

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_COMPONENTS

; we use this directory for the custom case
Var CUSTOM_PLUGIN_FOLDER
!define MUI_DIRECTORYPAGE_VARIABLE $CUSTOM_PLUGIN_FOLDER
!define MUI_PAGE_CUSTOMFUNCTION_PRE onPreDirectoryPage
!define MUI_DIRECTORYPAGE_TEXT_TOP "You have selected custom directory installation on the previous page.$\r$\nPlease select the directory in which to install gift-fasttrack below."
!insertmacro MUI_PAGE_DIRECTORY

!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_LANGUAGE "English"

; Utils
;------------------------------------------------------------------------------------------

; StrStr
; input, top of stack = string to search for
;        top of stack-1 = string to search in
; output, top of stack (replaces with the portion of the string remaining)
; modifies no other variables.
;
; Usage:
;   Push "this is a long ass string"
;   Push "ass"
;   Call StrStr
;   Pop $R0
;  ($R0 at this point is "ass string")

Function StrStr
  Exch $R1 ; st=haystack,old$R1, $R1=needle
  Exch    ; st=old$R1,haystack
  Exch $R2 ; st=old$R1,old$R2, $R2=haystack
  Push $R3
  Push $R4
  Push $R5
  StrLen $R3 $R1
  StrCpy $R4 0
  ; $R1=needle
  ; $R2=haystack
  ; $R3=len(needle)
  ; $R4=cnt
  ; $R5=tmp
  loop:
    StrCpy $R5 $R2 $R3 $R4
    StrCmp $R5 $R1 done
    StrCmp $R5 "" done
    IntOp $R4 $R4 + 1
    Goto loop
  done:
  StrCpy $R1 $R2 "" $R4
  Pop $R5
  Pop $R4
  Pop $R3
  Pop $R2
  Exch $R1
FunctionEnd

; Installer Sections
;------------------------------------------------------------------------------------------

Section "gift-fasttrack for KCeasy" SEC_KCEASY
	SectionIn 1

	; get install dir from registry
	ReadRegStr $R1 HKEY_LOCAL_MACHINE "Software\KCeasy" "InstallPath"
	IfErrors 0 FoundInstallPath
	MessageBox MB_OK|MB_SETFOREGROUND|MB_ICONEXCLAMATION "Couldn't find location of KCeasy.$\r$\nNot installing plugin for it."
	Goto ExitSection

FoundInstallPath:
	StrCpy $INSTDIR $R1
	SetOutPath $INSTDIR\giFT\plugins
	File /oname=FastTrack.dll FastTrack-Loader.dll

	SetOutPath $INSTDIR\giFT\conf\FastTrack
	File ${TMP_DIR}\FastTrack.conf
	File ${DIST_DIR}\data\FastTrack\nodes
	File ${DIST_DIR}\data\FastTrack\banlist

	; update giftd.conf
	ReadINIStr $R1 $INSTDIR\giFT\conf\giftd.conf "main" "plugins"
	IfErrors ConfError

	; add ":FastTrack" to plugin line if not present
	Push $R1
	Push "FastTrack"
	Call StrStr
	Pop $R0
	StrCmp $R0 "" 0 ConfError
	WriteINIStr $INSTDIR\giFT\conf\giftd.conf "main" "plugins" "$R1:FastTrack"

ConfError:
	; get installer filename in $R0
	System::Call 'kernel32::GetModuleFileNameA(i 0, t .R0, i 1024) i r1'
	; copy the installer into 'My Shared folder' if not already there
	IfFileExists "$INSTDIR\My Shared Folder\${OUTFILE}" InstallerAlreadyShared
	CopyFiles /SILENT /FILESONLY $R0 "$INSTDIR\My Shared Folder\${OUTFILE}" 2480

InstallerAlreadyShared:
ExitSection:
SectionEnd


Section "gift-fasttrack for giFTwin32" SEC_GIFTWIN32
	SectionIn 1

	; get install dir from registry
	ReadRegStr $R1 HKEY_LOCAL_MACHINE "Software\giFT\giFT" "instpath"
	IfErrors 0 FoundInstallPath
	MessageBox MB_OK|MB_SETFOREGROUND|MB_ICONEXCLAMATION "Couldn't find location of giFTwin32.$\r$\nNot installing plugin for it."
	Goto ExitSection

FoundInstallPath:
	StrCpy $INSTDIR $R1
	SetOutPath $INSTDIR
	File /oname=FastTrack.dll FastTrack.dll

	SetOutPath $INSTDIR\FastTrack
	File ${TMP_DIR}\FastTrack.conf
	File ${DIST_DIR}\data\FastTrack\nodes
	File ${DIST_DIR}\data\FastTrack\banlist

	; update giftd.conf
	ReadINIStr $R1 $INSTDIR\giftd.conf "main" "plugins"
	IfErrors ConfError

	; add ":FastTrack" to plugin line if not present
	Push $R1
	Push "FastTrack"
	Call StrStr
	Pop $R0
	StrCmp $R0 "" 0 ConfError
	WriteINIStr $INSTDIR\giftd.conf "main" "plugins" "$R1:FastTrack"
ConfError:
ExitSection:
SectionEnd


Section /o "gift-fasttrack in custom directory" SEC_CUSTOM
	SectionIn 1

	; get install dir from registry
	StrCmp $CUSTOM_PLUGIN_FOLDER "" 0 FoundInstallPath
	MessageBox MB_OK|MB_SETFOREGROUND|MB_ICONEXCLAMATION "Custom directory empty.$\r$\nNot installing plugin in custom directory."
	Goto ExitSection

FoundInstallPath:
	StrCpy $INSTDIR $CUSTOM_PLUGIN_FOLDER
	SetOutPath $INSTDIR
	File /oname=FastTrack.dll FastTrack.dll

	SetOutPath $INSTDIR\FastTrack
	File ${TMP_DIR}\FastTrack.conf
	File ${DIST_DIR}\data\FastTrack\nodes
	File ${DIST_DIR}\data\FastTrack\banlist
ExitSection:
SectionEnd


; Event handlers
;------------------------------------------------------------------------------------------

Function .onInit
	; enable KCeasy section if KCeasy is installed
	SectionGetFlags ${SEC_KCEASY} $R0
	; bit 1 set => selected
	; bit 5 set => selection is readonly
	IntOp $R0 $R0 & 0xFFFFFFFE
	IntOp $R0 $R0 | 0x10
	ReadRegStr $R1 HKEY_LOCAL_MACHINE "Software\KCeasy" "InstallPath"
	IfErrors NoKCeasy
	IntOp $R0 $R0 & 0xFFFFFFEF
	IntOp $R0 $R0 | 0x01
NoKCeasy:
	SectionSetFlags ${SEC_KCEASY} $R0

	; enable giFTwin32 section if giFTwin32 is installed
	SectionGetFlags ${SEC_GIFTWIN32} $R0
	; bit 1 set => selected
	; bit 5 set => selection is readonly
	IntOp $R0 $R0 & 0xFFFFFFFE
	IntOp $R0 $R0 | 0x10
	ReadRegStr $R1 HKEY_LOCAL_MACHINE "Software\giFT\giFT" "instpath"
	IfErrors NoGiftWin32
	IntOp $R0 $R0 & 0xFFFFFFEF
	IntOp $R0 $R0 | 0x01
NoGiftWin32:
	SectionSetFlags ${SEC_GIFTWIN32} $R0
FunctionEnd

Function onPreDirectoryPage
	; Only show page if custom install is selected
	SectionGetFlags ${SEC_CUSTOM} $R0
	IntOp $R0 $R0 & 1   ; bit one is set when section selected
	IntCmpU $R0 1 CustomSelected
	Abort
CustomSelected:
FunctionEnd
