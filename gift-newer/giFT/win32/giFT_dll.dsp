# Microsoft Developer Studio Project File - Name="giFT_dll" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

CFG=giFT_dll - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "giFT_dll.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "giFT_dll.mak" CFG="giFT_dll - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "giFT_dll - Win32 Release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "giFT_dll - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "giFT_dll - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "giFT_dll_Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "GIFT_DLL_EXPORTS" /YX /FD /c
# ADD CPP /nologo /MT /W3 /GX /O2 /I "../win32" /I "../OpenFT" /D "NDEBUG" /D "USE_DLOPEN" /D "HAVE_CONFIG_H" /D "GIFT_DLL_EXPORTS" /D "WIN32" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /YX /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /machine:I386
# ADD LINK32 ws2_32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /machine:I386 /out:"Release/giFT.dll"

!ELSEIF  "$(CFG)" == "giFT_dll - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "giFT_dll___Win32_Debug"
# PROP BASE Intermediate_Dir "giFT_dll___Win32_Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "giFT_dll_Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "GIFT_DLL_EXPORTS" /YX /FD /GZ  /c
# ADD CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /I "../win32" /I "../OpenFT" /D "_DEBUG" /D "USE_DLOPEN" /D "HAVE_CONFIG_H" /D "GIFT_DLL_EXPORTS" /D "WIN32" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /YX /FD /GZ  /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /debug /machine:I386 /pdbtype:sept
# ADD LINK32 ws2_32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /debug /machine:I386 /out:"Debug/giFTd.dll" /pdbtype:sept

!ENDIF 

# Begin Target

# Name "giFT_dll - Win32 Release"
# Name "giFT_dll - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=..\src\conf.c
# End Source File
# Begin Source File

SOURCE=..\src\connection.c
# End Source File
# Begin Source File

SOURCE=..\src\daemon.c
# End Source File
# Begin Source File

SOURCE=..\src\dataset.c
# End Source File
# Begin Source File

SOURCE=..\src\download.c
# End Source File
# Begin Source File

SOURCE=..\src\event.c
# End Source File
# Begin Source File

SOURCE=..\src\file.c
# End Source File
# Begin Source File

SOURCE=.\gift_api.def
# End Source File
# Begin Source File

SOURCE=..\src\hash.c
# End Source File
# Begin Source File

SOURCE=..\src\if_event.c
# End Source File
# Begin Source File

SOURCE=..\src\interface.c
# End Source File
# Begin Source File

SOURCE=..\src\list.c
# End Source File
# Begin Source File

SOURCE=..\src\main.c
# End Source File
# Begin Source File

SOURCE=..\src\md5.c
# End Source File
# Begin Source File

SOURCE=..\src\mime.c
# End Source File
# Begin Source File

SOURCE=..\src\nb.c
# End Source File
# Begin Source File

SOURCE=..\src\network.c
# End Source File
# Begin Source File

SOURCE=..\src\parse.c
# End Source File
# Begin Source File

SOURCE=..\src\platform.c
# End Source File
# Begin Source File

SOURCE=..\src\protocol.c
# End Source File
# Begin Source File

SOURCE=..\src\queue.c
# End Source File
# Begin Source File

SOURCE=..\src\sharing.c
# End Source File
# Begin Source File

SOURCE=..\src\transfer.c
# End Source File
# Begin Source File

SOURCE=..\src\upload.c
# End Source File
# Begin Source File

SOURCE=..\src\watch.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=..\src\conf.h
# End Source File
# Begin Source File

SOURCE=..\src\connection.h
# End Source File
# Begin Source File

SOURCE=..\src\daemon.h
# End Source File
# Begin Source File

SOURCE=..\src\dataset.h
# End Source File
# Begin Source File

SOURCE=..\src\download.h
# End Source File
# Begin Source File

SOURCE=..\src\event.h
# End Source File
# Begin Source File

SOURCE=..\src\file.h
# End Source File
# Begin Source File

SOURCE=..\src\gift.h
# End Source File
# Begin Source File

SOURCE=..\src\hash.h
# End Source File
# Begin Source File

SOURCE=..\src\if_event.h
# End Source File
# Begin Source File

SOURCE=..\src\interface.h
# End Source File
# Begin Source File

SOURCE=..\src\list.h
# End Source File
# Begin Source File

SOURCE=..\src\md5.h
# End Source File
# Begin Source File

SOURCE=..\src\mime.h
# End Source File
# Begin Source File

SOURCE=..\src\nb.h
# End Source File
# Begin Source File

SOURCE=..\src\network.h
# End Source File
# Begin Source File

SOURCE=..\src\parse.h
# End Source File
# Begin Source File

SOURCE=..\src\platform.h
# End Source File
# Begin Source File

SOURCE=..\src\protocol.h
# End Source File
# Begin Source File

SOURCE=..\src\queue.h
# End Source File
# Begin Source File

SOURCE=..\src\sharing.h
# End Source File
# Begin Source File

SOURCE=..\src\transfer.h
# End Source File
# Begin Source File

SOURCE=..\src\upload.h
# End Source File
# Begin Source File

SOURCE=..\src\watch.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# End Group
# End Target
# End Project
