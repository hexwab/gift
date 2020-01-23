# Microsoft Developer Studio Project File - Name="libgiFT_dll" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

CFG=libgiFT_dll - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "libgiFT_dll.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "libgiFT_dll.mak" CFG="libgiFT_dll - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "libgiFT_dll - Win32 Release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "libgiFT_dll - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "libgiFT_dll - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "ReleaseDLL"
# PROP Intermediate_Dir "libgiFT_dll_Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "libgiFT_dll_EXPORTS" /YX /FD /c
# ADD CPP /nologo /MD /W3 /GX /O2 /I "../win32" /I "../lib" /I "../src" /I "../OpenFT" /D "NDEBUG" /D "USE_DLOPEN" /D "libgiFT_dll_EXPORTS" /D "_WINDOWS" /D "_USRDLL" /D "USE_ZLIB" /D "HAVE_CONFIG_H" /D "WIN32" /D "_MBCS" /YX /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /machine:I386
# ADD LINK32 gdi32.lib winspool.lib comdlg32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib ws2_32.lib binmode.obj oldnames.lib kernel32.lib user32.lib advapi32.lib /nologo /dll /machine:I386 /out:"ReleaseDLL/libgiFT.dll"

!ELSEIF  "$(CFG)" == "libgiFT_dll - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "libgiFT_dll___Win32_Debug"
# PROP BASE Intermediate_Dir "libgiFT_dll___Win32_Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "DebugDLL"
# PROP Intermediate_Dir "libgiFT_dll_Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "libgiFT_dll_EXPORTS" /YX /FD /GZ /c
# ADD CPP /nologo /MDd /W3 /Gm /GX /ZI /Od /I "../win32" /I "../lib" /I "../src" /I "../OpenFT" /D "_DEBUG" /D "USE_DLOPEN" /D "libgiFT_dll_EXPORTS" /D "_WINDOWS" /D "_USRDLL" /D "USE_ZLIB" /D "HAVE_CONFIG_H" /D "WIN32" /D "_MBCS" /YX /FD /GZ /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /debug /machine:I386 /pdbtype:sept
# ADD LINK32 zlibd.lib binmode.obj oldnames.lib gdi32.lib winspool.lib comdlg32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib ws2_32.lib kernel32.lib user32.lib advapi32.lib /nologo /dll /debug /machine:I386 /out:"DebugDLL/libgiFT.dll" /pdbtype:sept

!ENDIF 

# Begin Target

# Name "libgiFT_dll - Win32 Release"
# Name "libgiFT_dll - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=..\lib\conf.c
# End Source File
# Begin Source File

SOURCE=..\lib\connection.c
# End Source File
# Begin Source File

SOURCE=..\lib\dataset.c
# End Source File
# Begin Source File

SOURCE=..\lib\event.c
# End Source File
# Begin Source File

SOURCE=..\lib\file.c
# End Source File
# Begin Source File

SOURCE=..\lib\hash.c
# End Source File
# Begin Source File

SOURCE=..\lib\if_event.c
# End Source File
# Begin Source File

SOURCE=..\lib\interface.c
# End Source File
# Begin Source File

SOURCE=.\libgift_api.def
# End Source File
# Begin Source File

SOURCE=..\lib\list.c
# End Source File
# Begin Source File

SOURCE=..\lib\list_dataset.c
# End Source File
# Begin Source File

SOURCE=..\lib\list_lock.c
# End Source File
# Begin Source File

SOURCE=..\lib\list_queue.c
# End Source File
# Begin Source File

SOURCE=..\lib\log.c
# End Source File
# Begin Source File

SOURCE=..\lib\nb.c
# End Source File
# Begin Source File

SOURCE=..\lib\network.c
# End Source File
# Begin Source File

SOURCE=..\lib\parse.c
# End Source File
# Begin Source File

SOURCE=..\lib\platform.c
# End Source File
# Begin Source File

SOURCE=..\lib\protocol.c
# End Source File
# Begin Source File

SOURCE=..\lib\queue.c
# End Source File
# Begin Source File

SOURCE=..\lib\tree.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=..\lib\conf.h
# End Source File
# Begin Source File

SOURCE=..\lib\connection.h
# End Source File
# Begin Source File

SOURCE=..\lib\dataset.h
# End Source File
# Begin Source File

SOURCE=..\lib\event.h
# End Source File
# Begin Source File

SOURCE=..\lib\file.h
# End Source File
# Begin Source File

SOURCE=..\lib\gift.h
# End Source File
# Begin Source File

SOURCE=..\lib\hash.h
# End Source File
# Begin Source File

SOURCE=..\lib\if_event.h
# End Source File
# Begin Source File

SOURCE=..\lib\interface.h
# End Source File
# Begin Source File

SOURCE=..\lib\list.h
# End Source File
# Begin Source File

SOURCE=..\lib\list_dataset.h
# End Source File
# Begin Source File

SOURCE=..\lib\list_lock.h
# End Source File
# Begin Source File

SOURCE=..\lib\list_queue.h
# End Source File
# Begin Source File

SOURCE=..\lib\log.h
# End Source File
# Begin Source File

SOURCE=..\lib\nb.h
# End Source File
# Begin Source File

SOURCE=..\lib\network.h
# End Source File
# Begin Source File

SOURCE=..\lib\parse.h
# End Source File
# Begin Source File

SOURCE=..\lib\platform.h
# End Source File
# Begin Source File

SOURCE=..\lib\protocol.h
# End Source File
# Begin Source File

SOURCE=..\lib\queue.h
# End Source File
# Begin Source File

SOURCE=..\lib\tree.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# End Group
# End Target
# End Project
