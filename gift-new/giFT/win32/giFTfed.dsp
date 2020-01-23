# Microsoft Developer Studio Project File - Name="giFTfed" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Application" 0x0103

CFG=giFTfed - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "giFTfed.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "giFTfed.mak" CFG="giFTfed - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "giFTfed - Win32 Release" (based on "Win32 (x86) Application")
!MESSAGE "giFTfed - Win32 Debug" (based on "Win32 (x86) Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "giFTfed - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "ReleaseDLL"
# PROP Intermediate_Dir "giFTfed_Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /MD /W3 /GX /O2 /I "../win32" /I "../src" /I "../lib" /I "../OpenFT" /I "../ui/pixmaps" /D "NDEBUG" /D "USE_DLOPEN" /D "USE_ZLIB" /D "HAVE_CONFIG_H" /D "WIN32" /D "_MBCS" /D "_WINDOWS" /YX /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /machine:I386
# ADD LINK32 ws2_32.lib gtk.lib glib-2.0.lib gdk.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /machine:I386 /out:"ReleaseDLL/giFTfe.exe"

!ELSEIF  "$(CFG)" == "giFTfed - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "giFTfed___Win32_Debug"
# PROP BASE Intermediate_Dir "giFTfed___Win32_Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "DebugDLL"
# PROP Intermediate_Dir "giFTfed_Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /YX /FD /GZ /c
# ADD CPP /nologo /MDd /W3 /Gm /GX /ZI /Od /I "../win32" /I "../src" /I "../lib" /I "../OpenFT" /I "../ui/pixmaps" /D "_DEBUG" /D "USE_DLOPEN" /D "USE_ZLIB" /D "HAVE_CONFIG_H" /D "WIN32" /D "_MBCS" /D "_WINDOWS" /YX /FD /GZ /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /debug /machine:I386 /pdbtype:sept
# ADD LINK32 ws2_32.lib gtk.lib glib-2.0.lib gdk.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /debug /machine:I386 /out:"DebugDLL/giFTfe.exe" /pdbtype:sept

!ENDIF 

# Begin Target

# Name "giFTfed - Win32 Release"
# Name "giFTfed - Win32 Debug"
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
