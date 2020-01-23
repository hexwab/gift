# Microsoft Developer Studio Project File - Name="OpenFT" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=OpenFT - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "OpenFT.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "OpenFT.mak" CFG="OpenFT - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "OpenFT - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "OpenFT - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "OpenFT - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "OpenFT_Release"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /MTd /W3 /GX /O2 /I "..\win32" /I "..\lib" /I "..\src" /I "..\OpenFT" /D "NDEBUG" /D "_LIB" /D "HAVE_CONFIG_H" /D "WIN32" /D "_MBCS" /YX /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "OpenFT - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "OpenFT___Win32_Debug"
# PROP BASE Intermediate_Dir "OpenFT___Win32_Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "OpenFT_Debug"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /I "..\win32" /I "..\lib" /I "..\src" /I "..\OpenFT" /D "_DEBUG" /D "_LIB" /D "HAVE_CONFIG_H" /D "WIN32" /D "_MBCS" /FR /YX /FD /GZ /c
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

# Name "OpenFT - Win32 Release"
# Name "OpenFT - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=..\OpenFT\ft_event.c
# End Source File
# Begin Source File

SOURCE=..\OpenFT\ft_html.c
# End Source File
# Begin Source File

SOURCE=..\OpenFT\ft_http_client.c
# End Source File
# Begin Source File

SOURCE=..\OpenFT\ft_http_server.c
# End Source File
# Begin Source File

SOURCE=..\OpenFT\ft_netorg.c
# End Source File
# Begin Source File

SOURCE=..\OpenFT\ft_node.c
# End Source File
# Begin Source File

SOURCE=..\OpenFT\ft_openft.c
# End Source File
# Begin Source File

SOURCE=..\OpenFT\ft_packet.c
# End Source File
# Begin Source File

SOURCE=..\OpenFT\ft_protocol.c
# End Source File
# Begin Source File

SOURCE=..\OpenFT\ft_search.c
# End Source File
# Begin Source File

SOURCE=..\OpenFT\ft_search_exec.c
# End Source File
# Begin Source File

SOURCE=..\OpenFT\ft_session.c
# End Source File
# Begin Source File

SOURCE=..\OpenFT\ft_share.c
# End Source File
# Begin Source File

SOURCE=..\OpenFT\ft_share_file.c
# End Source File
# Begin Source File

SOURCE=..\OpenFT\ft_shost.c
# End Source File
# Begin Source File

SOURCE=..\OpenFT\ft_stats.c
# End Source File
# Begin Source File

SOURCE=..\OpenFT\ft_stream.c
# End Source File
# Begin Source File

SOURCE=..\OpenFT\ft_utils.c
# End Source File
# Begin Source File

SOURCE=..\OpenFT\ft_version.c
# End Source File
# Begin Source File

SOURCE=..\OpenFT\ft_xfer.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=..\OpenFT\ft_event.h
# End Source File
# Begin Source File

SOURCE=..\OpenFT\ft_html.h
# End Source File
# Begin Source File

SOURCE=..\OpenFT\ft_http_client.h
# End Source File
# Begin Source File

SOURCE=..\OpenFT\ft_http_server.h
# End Source File
# Begin Source File

SOURCE=..\OpenFT\ft_netorg.h
# End Source File
# Begin Source File

SOURCE=..\OpenFT\ft_node.h
# End Source File
# Begin Source File

SOURCE=..\OpenFT\ft_openft.h
# End Source File
# Begin Source File

SOURCE=..\OpenFT\ft_packet.h
# End Source File
# Begin Source File

SOURCE=..\OpenFT\ft_protocol.h
# End Source File
# Begin Source File

SOURCE=..\OpenFT\ft_search.h
# End Source File
# Begin Source File

SOURCE=..\OpenFT\ft_search_exec.h
# End Source File
# Begin Source File

SOURCE=..\OpenFT\ft_session.h
# End Source File
# Begin Source File

SOURCE=..\OpenFT\ft_share.h
# End Source File
# Begin Source File

SOURCE=..\OpenFT\ft_share_file.h
# End Source File
# Begin Source File

SOURCE=..\OpenFT\ft_shost.h
# End Source File
# Begin Source File

SOURCE=..\OpenFT\ft_stats.h
# End Source File
# Begin Source File

SOURCE=..\OpenFT\ft_stream.h
# End Source File
# Begin Source File

SOURCE=..\OpenFT\ft_utils.h
# End Source File
# Begin Source File

SOURCE=..\OpenFT\ft_version.h
# End Source File
# Begin Source File

SOURCE=..\OpenFT\ft_xfer.h
# End Source File
# End Group
# End Target
# End Project
