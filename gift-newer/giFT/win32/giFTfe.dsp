# Microsoft Developer Studio Project File - Name="giFTfe" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Application" 0x0101

CFG=giFTfe - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "giFTfe.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "giFTfe.mak" CFG="giFTfe - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "giFTfe - Win32 Release" (based on "Win32 (x86) Application")
!MESSAGE "giFTfe - Win32 Debug" (based on "Win32 (x86) Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "giFTfe - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "giftFTfe_Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /MTd /W3 /GX /O2 /I ".\pixmaps" /I "..\ui" /I "..\ui\pixmaps" /I "." /I "..\win32" /I "..\src" /I "..\OpenFT" /I "..\lib" /D "NDEBUG" /D "_CONSOLE" /D "DONT_USE_REGISTRY" /D "HAVE_CONFIG_H" /D "WIN32" /D "_MBCS" /YX /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386
# ADD LINK32 gtk.lib glib-2.0.lib gdk.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib binmode.obj wsock32.lib kernel32.lib user32.lib /nologo /version:0.10 /subsystem:console /machine:I386

!ELSEIF  "$(CFG)" == "giFTfe - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "giFTfe___Win32_Debug"
# PROP BASE Intermediate_Dir "giFTfe___Win32_Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "giftFTfe_Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /I "..\ui" /I "..\ui\pixmaps" /I "." /I "..\win32" /I "..\src" /I "..\OpenFT" /I "..\lib" /D "_DEBUG" /D "_CONSOLE" /D "DONT_USE_REGISTRY" /D "HAVE_CONFIG_H" /D "WIN32" /D "_MBCS" /FR /YX /FD /GZ /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept
# ADD LINK32 gtk.lib glib-2.0.lib gdk.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib binmode.obj wsock32.lib kernel32.lib user32.lib /nologo /version:0.10 /subsystem:console /debug /machine:I386 /pdbtype:sept
# SUBTRACT LINK32 /pdb:none

!ENDIF 

# Begin Target

# Name "giFTfe - Win32 Release"
# Name "giFTfe - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=..\ui\fe_connect.c
# End Source File
# Begin Source File

SOURCE=..\ui\fe_daemon.c
# End Source File
# Begin Source File

SOURCE=..\ui\fe_download.c
# End Source File
# Begin Source File

SOURCE=..\ui\fe_fifo.c
# End Source File
# Begin Source File

SOURCE=..\ui\fe_menu.c
# End Source File
# Begin Source File

SOURCE=..\ui\fe_obj.c
# End Source File
# Begin Source File

SOURCE=..\ui\fe_pref.c
# End Source File
# Begin Source File

SOURCE=..\ui\fe_search.c
# End Source File
# Begin Source File

SOURCE=..\ui\fe_share.c
# End Source File
# Begin Source File

SOURCE=..\ui\fe_stats.c
# End Source File
# Begin Source File

SOURCE=..\ui\fe_transfer.c
# End Source File
# Begin Source File

SOURCE=..\ui\fe_ui.c
# End Source File
# Begin Source File

SOURCE=..\ui\fe_ui_utils.c
# End Source File
# Begin Source File

SOURCE=..\ui\fe_upload.c
# End Source File
# Begin Source File

SOURCE=..\ui\fe_utils.c
# End Source File
# Begin Source File

SOURCE="..\ui\gift-fe.c"
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=..\ui\fe_connect.h
# End Source File
# Begin Source File

SOURCE=..\ui\fe_daemon.h
# End Source File
# Begin Source File

SOURCE=..\ui\fe_download.h
# End Source File
# Begin Source File

SOURCE=..\ui\fe_fifo.h
# End Source File
# Begin Source File

SOURCE="..\ui\fe_menu-if.h"
# End Source File
# Begin Source File

SOURCE=..\ui\fe_menu.h
# End Source File
# Begin Source File

SOURCE=..\ui\fe_obj.h
# End Source File
# Begin Source File

SOURCE=..\ui\fe_pref.h
# End Source File
# Begin Source File

SOURCE=..\ui\fe_search.h
# End Source File
# Begin Source File

SOURCE=..\ui\fe_share.h
# End Source File
# Begin Source File

SOURCE=..\ui\fe_stats.h
# End Source File
# Begin Source File

SOURCE=..\ui\fe_transfer.h
# End Source File
# Begin Source File

SOURCE=..\ui\fe_ui.h
# End Source File
# Begin Source File

SOURCE=..\ui\fe_ui_utils.h
# End Source File
# Begin Source File

SOURCE=..\ui\fe_upload.h
# End Source File
# Begin Source File

SOURCE=..\ui\fe_utils.h
# End Source File
# Begin Source File

SOURCE="..\ui\gift-fe.h"
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# End Group
# End Target
# End Project
