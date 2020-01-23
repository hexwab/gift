# Microsoft Developer Studio Project File - Name="libgiFT" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=libgiFT - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "libgiFT.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "libgiFT.mak" CFG="libgiFT - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "libgiFT - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "libgiFT - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "libgiFT - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "libgiFT_Release"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /MTd /W3 /GX /O2 /I "../win32" /I "../lib" /I "../src" /I "../OpenFT" /I "../plugin" /D "NDEBUG" /D "_LIB" /D "PLUGIN_OPENFT" /D "PLUGIN_GNUTELLA" /D "HAVE_CONFIG_H" /D "WIN32" /D "_MBCS" /D "_WINDOWS" /YX /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "libgiFT - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "libgiFT___Win32_Debug"
# PROP BASE Intermediate_Dir "libgiFT___Win32_Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "libgiFT_Debug"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /I "../win32" /I "../lib" /I "../src" /I "../OpenFT" /I "../plugin" /D "_DEBUG" /D "_LIB" /D "PLUGIN_OPENFT" /D "PLUGIN_GNUTELLA" /D "HAVE_CONFIG_H" /D "WIN32" /D "_MBCS" /D "_WINDOWS" /FR /YX /FD /GZ /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ENDIF 

# Begin Target

# Name "libgiFT - Win32 Release"
# Name "libgiFT - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=..\lib\array.c
# End Source File
# Begin Source File

SOURCE=..\lib\conf.c
# End Source File
# Begin Source File

SOURCE=..\lib\dataset.c
# End Source File
# Begin Source File

SOURCE=..\lib\event.c
# End Source File
# Begin Source File

SOURCE=..\lib\fdbuf.c
# End Source File
# Begin Source File

SOURCE=..\lib\file.c
# End Source File
# Begin Source File

SOURCE=..\lib\interface.c
# End Source File
# Begin Source File

SOURCE=..\lib\list.c
# End Source File
# Begin Source File

SOURCE=..\lib\list_lock.c
# End Source File
# Begin Source File

SOURCE=..\lib\log.c
# End Source File
# Begin Source File

SOURCE=..\lib\memory.c
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

SOURCE=..\lib\stopwatch.c
# End Source File
# Begin Source File

SOURCE=..\lib\strobj.c
# End Source File
# Begin Source File

SOURCE=..\lib\tcpc.c
# End Source File
# Begin Source File

SOURCE=..\lib\tree.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=..\lib\array.h
# End Source File
# Begin Source File

SOURCE=..\lib\conf.h
# End Source File
# Begin Source File

SOURCE=..\lib\dataset.h
# End Source File
# Begin Source File

SOURCE=..\lib\event.h
# End Source File
# Begin Source File

SOURCE=..\lib\fdbuf.h
# End Source File
# Begin Source File

SOURCE=..\lib\file.h
# End Source File
# Begin Source File

SOURCE=..\lib\gift.h
# End Source File
# Begin Source File

SOURCE=..\lib\interface.h
# End Source File
# Begin Source File

SOURCE=..\lib\list.h
# End Source File
# Begin Source File

SOURCE=..\lib\list_lock.h
# End Source File
# Begin Source File

SOURCE=..\lib\log.h
# End Source File
# Begin Source File

SOURCE=..\lib\memory.h
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

SOURCE=..\lib\stopwatch.h
# End Source File
# Begin Source File

SOURCE=..\lib\strobj.h
# End Source File
# Begin Source File

SOURCE=..\lib\tcpc.h
# End Source File
# Begin Source File

SOURCE=..\lib\tree.h
# End Source File
# End Group
# End Target
# End Project
