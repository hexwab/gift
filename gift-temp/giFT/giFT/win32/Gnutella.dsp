# Microsoft Developer Studio Project File - Name="Gnutella" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=Gnutella - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "Gnutella.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "Gnutella.mak" CFG="Gnutella - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "Gnutella - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "Gnutella - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "Gnutella - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Gnutella_Release"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /MTd /W3 /GX /O2 /I "../win32" /I "../lib" /I "../src" /I "../Gnutella" /I "../plugin" /D "NDEBUG" /D "_LIB" /D "PLUGIN_OPENFT" /D "PLUGIN_GNUTELLA" /D "HAVE_CONFIG_H" /D "WIN32" /D "_MBCS" /D "_WINDOWS" /YX /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "Gnutella - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Gnutella___Win32_Debug"
# PROP BASE Intermediate_Dir "Gnutella___Win32_Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Gnutella_Debug"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /I "../win32" /I "../lib" /I "../src" /I "../Gnutella" /I "../plugin" /D "_DEBUG" /D "_LIB" /D "PLUGIN_OPENFT" /D "PLUGIN_GNUTELLA" /D "HAVE_CONFIG_H" /D "WIN32" /D "_MBCS" /D "_WINDOWS" /FR /YX /FD /GZ /c
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

# Name "Gnutella - Win32 Release"
# Name "Gnutella - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter ""
# End Group
# Begin Source File

SOURCE=..\Gnutella\file_cache.c
# End Source File
# Begin Source File

SOURCE=..\Gnutella\file_cache.h
# End Source File
# Begin Source File

SOURCE=..\Gnutella\ft_http_client.c
# End Source File
# Begin Source File

SOURCE=..\Gnutella\ft_http_client.h
# End Source File
# Begin Source File

SOURCE=..\Gnutella\ft_http_server.c
# End Source File
# Begin Source File

SOURCE=..\Gnutella\ft_http_server.h
# End Source File
# Begin Source File

SOURCE=..\Gnutella\ft_xfer.c
# End Source File
# Begin Source File

SOURCE=..\Gnutella\ft_xfer.h
# End Source File
# Begin Source File

SOURCE=..\Gnutella\gt_accept.c
# End Source File
# Begin Source File

SOURCE=..\Gnutella\gt_accept.h
# End Source File
# Begin Source File

SOURCE=..\Gnutella\gt_connect.c
# End Source File
# Begin Source File

SOURCE=..\Gnutella\gt_connect.h
# End Source File
# Begin Source File

SOURCE=..\Gnutella\gt_gnutella.c
# End Source File
# Begin Source File

SOURCE=..\Gnutella\gt_gnutella.h
# End Source File
# Begin Source File

SOURCE=..\Gnutella\gt_guid.c
# End Source File
# Begin Source File

SOURCE=..\Gnutella\gt_guid.h
# End Source File
# Begin Source File

SOURCE=..\Gnutella\gt_netorg.c
# End Source File
# Begin Source File

SOURCE=..\Gnutella\gt_netorg.h
# End Source File
# Begin Source File

SOURCE=..\Gnutella\gt_node.c
# End Source File
# Begin Source File

SOURCE=..\Gnutella\gt_node.h
# End Source File
# Begin Source File

SOURCE=..\Gnutella\gt_packet.c
# End Source File
# Begin Source File

SOURCE=..\Gnutella\gt_packet.h
# End Source File
# Begin Source File

SOURCE=..\Gnutella\gt_protocol.c
# End Source File
# Begin Source File

SOURCE=..\Gnutella\gt_protocol.h
# End Source File
# Begin Source File

SOURCE=..\Gnutella\gt_query_route.c
# End Source File
# Begin Source File

SOURCE=..\Gnutella\gt_query_route.h
# End Source File
# Begin Source File

SOURCE=..\Gnutella\gt_search.c
# End Source File
# Begin Source File

SOURCE=..\Gnutella\gt_search.h
# End Source File
# Begin Source File

SOURCE=..\Gnutella\gt_search_exec.c
# End Source File
# Begin Source File

SOURCE=..\Gnutella\gt_search_exec.h
# End Source File
# Begin Source File

SOURCE=..\Gnutella\gt_share.c
# End Source File
# Begin Source File

SOURCE=..\Gnutella\gt_share.h
# End Source File
# Begin Source File

SOURCE=..\Gnutella\gt_share_file.c
# End Source File
# Begin Source File

SOURCE=..\Gnutella\gt_share_file.h
# End Source File
# Begin Source File

SOURCE=..\Gnutella\gt_stats.c
# End Source File
# Begin Source File

SOURCE=..\Gnutella\gt_stats.h
# End Source File
# Begin Source File

SOURCE=..\Gnutella\gt_utils.c
# End Source File
# Begin Source File

SOURCE=..\Gnutella\gt_utils.h
# End Source File
# Begin Source File

SOURCE=..\Gnutella\gt_web_cache.c
# End Source File
# Begin Source File

SOURCE=..\Gnutella\gt_web_cache.h
# End Source File
# Begin Source File

SOURCE=..\Gnutella\gt_xfer.c
# End Source File
# Begin Source File

SOURCE=..\Gnutella\gt_xfer.h
# End Source File
# Begin Source File

SOURCE=..\Gnutella\html.c
# End Source File
# Begin Source File

SOURCE=..\Gnutella\html.h
# End Source File
# Begin Source File

SOURCE=..\Gnutella\http.c
# End Source File
# Begin Source File

SOURCE=..\Gnutella\http.h
# End Source File
# Begin Source File

SOURCE=..\Gnutella\http_request.c
# End Source File
# Begin Source File

SOURCE=..\Gnutella\http_request.h
# End Source File
# Begin Source File

SOURCE=..\Gnutella\sha1.c
# End Source File
# Begin Source File

SOURCE=..\Gnutella\sha1.h
# End Source File
# End Target
# End Project
