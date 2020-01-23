# Microsoft Developer Studio Project File - Name="giFT" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Application" 0x0101

CFG=giFT - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "giFT.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "giFT.mak" CFG="giFT - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "giFT - Win32 Release" (based on "Win32 (x86) Application")
!MESSAGE "giFT - Win32 Debug" (based on "Win32 (x86) Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "giFT - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "giFT_Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /MTd /W3 /GX /O2 /I "../win32" /I "../lib" /I "../src" /I "../OpenFT" /I "../plugin" /I "../Gnutella" /D "NDEBUG" /D "PLUGIN_OPENFT" /D "PLUGIN_GNUTELLA" /D "HAVE_CONFIG_H" /D "WIN32" /D "_MBCS" /D "_WINDOWS" /YX /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /machine:I386
# ADD LINK32 vorbis.lib vorbisfile.lib zlib.lib libdb41.lib binmode.obj oldnames.lib ws2_32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib /nologo /version:0.10 /subsystem:windows /machine:I386
# SUBTRACT LINK32 /pdb:none

!ELSEIF  "$(CFG)" == "giFT - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "giFT___Win32_Debug"
# PROP BASE Intermediate_Dir "giFT___Win32_Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "giFT_Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /I "../win32" /I "../lib" /I "../src" /I "../OpenFT" /I "../plugin" /I "../Gnutella" /D "_DEBUG" /D "PLUGIN_OPENFT" /D "PLUGIN_GNUTELLA" /D "HAVE_CONFIG_H" /D "WIN32" /D "_MBCS" /D "_WINDOWS" /FR /YX /FD /GZ /c
# SUBTRACT CPP /X
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /fo"giFT.res" /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /debug /machine:I386 /pdbtype:sept
# ADD LINK32 vorbis_d.lib vorbisfile_d.lib zlibd.lib id3libD.lib libdb41d.lib binmode.obj oldnames.lib ws2_32.lib kernel32.lib user32.lib advapi32.lib /nologo /version:0.10 /subsystem:windows /debug /machine:I386 /nodefaultlib:"msvcrtd.lib" /pdbtype:sept
# SUBTRACT LINK32 /pdb:none /nodefaultlib

!ENDIF 

# Begin Target

# Name "giFT - Win32 Release"
# Name "giFT - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=..\src\daemon.c
# End Source File
# Begin Source File

SOURCE=..\src\download.c
# End Source File
# Begin Source File

SOURCE=.\getopt.c
# End Source File
# Begin Source File

SOURCE=..\src\httpd.c
# End Source File
# Begin Source File

SOURCE=..\src\if_message.c
# End Source File
# Begin Source File

SOURCE=..\src\if_port.c
# End Source File
# Begin Source File

SOURCE=..\src\if_search.c
# End Source File
# Begin Source File

SOURCE=..\src\if_share.c
# End Source File
# Begin Source File

SOURCE=..\src\if_stats.c
# End Source File
# Begin Source File

SOURCE=..\src\if_transfer.c
# End Source File
# Begin Source File

SOURCE=..\src\main.c
# End Source File
# Begin Source File

SOURCE=..\src\meta.c
# End Source File
# Begin Source File

SOURCE=..\src\meta_avi.c
# End Source File
# Begin Source File

SOURCE=..\src\meta_image.c
# End Source File
# Begin Source File

SOURCE=..\src\meta_mp3.c
# End Source File
# Begin Source File

SOURCE=..\src\meta_ogg.c
# End Source File
# Begin Source File

SOURCE=..\src\mime.c
# End Source File
# Begin Source File

SOURCE=..\src\opt.c
# End Source File
# Begin Source File

SOURCE=..\src\plugin.c
# End Source File
# Begin Source File

SOURCE=..\src\share_cache.c
# End Source File
# Begin Source File

SOURCE=..\src\share_db.c
# End Source File
# Begin Source File

SOURCE=..\src\share_file.c
# End Source File
# Begin Source File

SOURCE=..\src\share_hash.c
# End Source File
# Begin Source File

SOURCE=..\src\transfer.c
# End Source File
# Begin Source File

SOURCE=..\src\upload.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=.\config.h
# End Source File
# Begin Source File

SOURCE=..\src\daemon.h
# End Source File
# Begin Source File

SOURCE=..\src\download.h
# End Source File
# Begin Source File

SOURCE=.\getopt.h
# End Source File
# Begin Source File

SOURCE=..\src\httpd.h
# End Source File
# Begin Source File

SOURCE=..\src\if_message.h
# End Source File
# Begin Source File

SOURCE=..\src\if_port.h
# End Source File
# Begin Source File

SOURCE=..\src\if_search.h
# End Source File
# Begin Source File

SOURCE=..\src\if_share.h
# End Source File
# Begin Source File

SOURCE=..\src\if_stats.h
# End Source File
# Begin Source File

SOURCE=..\src\if_transfer.h
# End Source File
# Begin Source File

SOURCE=..\src\main.h
# End Source File
# Begin Source File

SOURCE=..\src\meta.h
# End Source File
# Begin Source File

SOURCE=..\src\meta_avi.h
# End Source File
# Begin Source File

SOURCE=..\src\meta_image.h
# End Source File
# Begin Source File

SOURCE=..\src\meta_mp3.h
# End Source File
# Begin Source File

SOURCE=..\src\meta_ogg.h
# End Source File
# Begin Source File

SOURCE=..\src\mime.h
# End Source File
# Begin Source File

SOURCE=..\src\opt.h
# End Source File
# Begin Source File

SOURCE=..\src\plugin.h
# End Source File
# Begin Source File

SOURCE=..\src\share_cache.h
# End Source File
# Begin Source File

SOURCE=..\src\share_db.h
# End Source File
# Begin Source File

SOURCE=..\src\share_file.h
# End Source File
# Begin Source File

SOURCE=..\src\share_hash.h
# End Source File
# Begin Source File

SOURCE=..\src\transfer.h
# End Source File
# Begin Source File

SOURCE=..\src\upload.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# Begin Source File

SOURCE=.\giFT.ico
# End Source File
# End Group
# End Target
# End Project
