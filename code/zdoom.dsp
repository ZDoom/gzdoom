# Microsoft Developer Studio Project File - Name="zdoom" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 5.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Application" 0x0101

CFG=zdoom - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "zdoom.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "zdoom.mak" CFG="zdoom - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "zdoom - Win32 Release" (based on "Win32 (x86) Application")
!MESSAGE "zdoom - Win32 Debug" (based on "Win32 (x86) Application")
!MESSAGE "zdoom - Win32 Profiling" (based on "Win32 (x86) Application")
!MESSAGE 

# Begin Project
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "zdoom - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /YX /FD /c
# ADD CPP /nologo /Gr /MT /W3 /GX /O2 /I "../ptc/source" /I "win32" /I "." /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D "USEASM" /FAs /YX /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /o NUL /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /o NUL /win32
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib ..\midas\lib\win32\vcretail\midas.lib wsock32.lib ..\ptc\library\win32\vc5.x\release.lib winmm.lib /nologo /subsystem:windows /pdb:none /map /machine:I386 /nodefaultlib:"libc" /out:"../zdoom.exe"
# SUBTRACT LINK32 /verbose /profile /debug

!ELSEIF  "$(CFG)" == "zdoom - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /Zi /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /YX /FD /c
# ADD CPP /nologo /G5 /Gr /MTd /W3 /Gm /GX /Zi /Od /I "../ptc/source" /I "win32" /I "." /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /D "NOASM" /YX /FD /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /o NUL /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /o NUL /win32
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /debug /machine:I386 /pdbtype:sept
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib ddraw.lib winmm.lib ..\midas\lib\win32\vcretail\midas.lib wsock32.lib ..\ptc\library\win32\vc5.x\release.lib /nologo /subsystem:windows /debug /machine:I386 /nodefaultlib:"libcmt" /out:"../doomdbg.exe"
# SUBTRACT LINK32 /profile

!ELSEIF  "$(CFG)" == "zdoom - Win32 Profiling"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "zdoom___"
# PROP BASE Intermediate_Dir "zdoom___"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Profiling"
# PROP Intermediate_Dir "Profiling"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /G5 /MT /W3 /GX /O2 /I "../ptc/source" /I "win32" /I "." /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D "USEASM" /YX /FD /c
# ADD CPP /nologo /G5 /MT /W3 /GX /O2 /I "../ptc/source" /I "win32" /I "." /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D "USEASM" /YX /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /o NUL /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /o NUL /win32
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib ..\midas\lib\win32\vcretail\midas.lib wsock32.lib d:\games\doom\ptc\library\win32\vc5.x\release.lib winmm.lib /nologo /subsystem:windows /pdb:none /map /machine:I386 /nodefaultlib:"libc" /out:"../zdoom.exe"
# SUBTRACT BASE LINK32 /verbose /profile /debug
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib ..\midas\lib\win32\vcretail\midas.lib wsock32.lib d:\games\doom\ptc\library\win32\vc5.x\release.lib winmm.lib /nologo /subsystem:windows /profile /debug /machine:I386 /nodefaultlib:"libc" /out:"../zdoomprof.exe"
# SUBTRACT LINK32 /verbose /map

!ENDIF 

# Begin Target

# Name "zdoom - Win32 Release"
# Name "zdoom - Win32 Debug"
# Name "zdoom - Win32 Profiling"
# Begin Group "Source Files"

# PROP Default_Filter "*.c; *.cpp"
# Begin Source File

SOURCE=.\am_map.c
# End Source File
# Begin Source File

SOURCE=.\c_bind.c
# End Source File
# Begin Source File

SOURCE=.\c_cmds.c
# End Source File
# Begin Source File

SOURCE=.\c_consol.c
# End Source File
# Begin Source File

SOURCE=.\c_cvars.c

!IF  "$(CFG)" == "zdoom - Win32 Release"

# ADD CPP /FAs

!ELSEIF  "$(CFG)" == "zdoom - Win32 Debug"

!ELSEIF  "$(CFG)" == "zdoom - Win32 Profiling"

# ADD BASE CPP /FAs
# ADD CPP /FAs

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\c_dispch.c
# End Source File
# Begin Source File

SOURCE=.\c_varini.c
# End Source File
# Begin Source File

SOURCE=.\cmdlib.c

!IF  "$(CFG)" == "zdoom - Win32 Release"

# ADD CPP /FAs

!ELSEIF  "$(CFG)" == "zdoom - Win32 Debug"

!ELSEIF  "$(CFG)" == "zdoom - Win32 Profiling"

# ADD BASE CPP /FAs
# ADD CPP /FAs

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\ct_chat.c
# End Source File
# Begin Source File

SOURCE=.\d_dehack.c

!IF  "$(CFG)" == "zdoom - Win32 Release"

# ADD CPP /FAs

!ELSEIF  "$(CFG)" == "zdoom - Win32 Debug"

!ELSEIF  "$(CFG)" == "zdoom - Win32 Profiling"

# ADD BASE CPP /FAs
# ADD CPP /FAs

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\d_items.c

!IF  "$(CFG)" == "zdoom - Win32 Release"

# ADD CPP /FAs

!ELSEIF  "$(CFG)" == "zdoom - Win32 Debug"

!ELSEIF  "$(CFG)" == "zdoom - Win32 Profiling"

# ADD BASE CPP /FAs
# ADD CPP /FAs

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\d_main.c

!IF  "$(CFG)" == "zdoom - Win32 Release"

# ADD CPP /FAs

!ELSEIF  "$(CFG)" == "zdoom - Win32 Debug"

!ELSEIF  "$(CFG)" == "zdoom - Win32 Profiling"

# ADD BASE CPP /FAs
# ADD CPP /FAs

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\d_net.c

!IF  "$(CFG)" == "zdoom - Win32 Release"

# ADD CPP /FAs

!ELSEIF  "$(CFG)" == "zdoom - Win32 Debug"

!ELSEIF  "$(CFG)" == "zdoom - Win32 Profiling"

# ADD BASE CPP /FAs
# ADD CPP /FAs

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\d_netinf.c
# End Source File
# Begin Source File

SOURCE=.\d_proto.c
# End Source File
# Begin Source File

SOURCE=.\doomdef.c

!IF  "$(CFG)" == "zdoom - Win32 Release"

# ADD CPP /FAs

!ELSEIF  "$(CFG)" == "zdoom - Win32 Debug"

!ELSEIF  "$(CFG)" == "zdoom - Win32 Profiling"

# ADD BASE CPP /FAs
# ADD CPP /FAs

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\doomstat.c

!IF  "$(CFG)" == "zdoom - Win32 Release"

# ADD CPP /FAs

!ELSEIF  "$(CFG)" == "zdoom - Win32 Debug"

!ELSEIF  "$(CFG)" == "zdoom - Win32 Profiling"

# ADD BASE CPP /FAs
# ADD CPP /FAs

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\dstrings.c

!IF  "$(CFG)" == "zdoom - Win32 Release"

# ADD CPP /FAs

!ELSEIF  "$(CFG)" == "zdoom - Win32 Debug"

!ELSEIF  "$(CFG)" == "zdoom - Win32 Profiling"

# ADD BASE CPP /FAs
# ADD CPP /FAs

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\f_finale.c

!IF  "$(CFG)" == "zdoom - Win32 Release"

# ADD CPP /FAs

!ELSEIF  "$(CFG)" == "zdoom - Win32 Debug"

!ELSEIF  "$(CFG)" == "zdoom - Win32 Profiling"

# ADD BASE CPP /FAs
# ADD CPP /FAs

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\f_wipe.c

!IF  "$(CFG)" == "zdoom - Win32 Release"

# ADD CPP /FAs

!ELSEIF  "$(CFG)" == "zdoom - Win32 Debug"

!ELSEIF  "$(CFG)" == "zdoom - Win32 Profiling"

# ADD BASE CPP /FAs
# ADD CPP /FAs

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\g_game.c

!IF  "$(CFG)" == "zdoom - Win32 Release"

# ADD CPP /FAs

!ELSEIF  "$(CFG)" == "zdoom - Win32 Debug"

!ELSEIF  "$(CFG)" == "zdoom - Win32 Profiling"

# ADD BASE CPP /FAs
# ADD CPP /FAs

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\g_level.c

!IF  "$(CFG)" == "zdoom - Win32 Release"

# ADD CPP /FAs

!ELSEIF  "$(CFG)" == "zdoom - Win32 Debug"

!ELSEIF  "$(CFG)" == "zdoom - Win32 Profiling"

# ADD BASE CPP /FAs
# ADD CPP /FAs

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\info.c

!IF  "$(CFG)" == "zdoom - Win32 Release"

# ADD CPP /FAs

!ELSEIF  "$(CFG)" == "zdoom - Win32 Debug"

!ELSEIF  "$(CFG)" == "zdoom - Win32 Profiling"

# ADD BASE CPP /FAs
# ADD CPP /FAs

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\m_alloc.c

!IF  "$(CFG)" == "zdoom - Win32 Release"

# ADD CPP /FAs

!ELSEIF  "$(CFG)" == "zdoom - Win32 Debug"

!ELSEIF  "$(CFG)" == "zdoom - Win32 Profiling"

# ADD BASE CPP /FAs
# ADD CPP /FAs

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\m_argv.c

!IF  "$(CFG)" == "zdoom - Win32 Release"

# ADD CPP /FAs

!ELSEIF  "$(CFG)" == "zdoom - Win32 Debug"

!ELSEIF  "$(CFG)" == "zdoom - Win32 Profiling"

# ADD BASE CPP /FAs
# ADD CPP /FAs

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\m_bbox.c

!IF  "$(CFG)" == "zdoom - Win32 Release"

# ADD CPP /FAs

!ELSEIF  "$(CFG)" == "zdoom - Win32 Debug"

!ELSEIF  "$(CFG)" == "zdoom - Win32 Profiling"

# ADD BASE CPP /FAs
# ADD CPP /FAs

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\m_cheat.c

!IF  "$(CFG)" == "zdoom - Win32 Release"

# ADD CPP /FAs

!ELSEIF  "$(CFG)" == "zdoom - Win32 Debug"

!ELSEIF  "$(CFG)" == "zdoom - Win32 Profiling"

# ADD BASE CPP /FAs
# ADD CPP /FAs

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\m_fixed.c

!IF  "$(CFG)" == "zdoom - Win32 Release"

# ADD CPP /FAs

!ELSEIF  "$(CFG)" == "zdoom - Win32 Debug"

!ELSEIF  "$(CFG)" == "zdoom - Win32 Profiling"

# ADD BASE CPP /FAs
# ADD CPP /FAs

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\m_menu.c

!IF  "$(CFG)" == "zdoom - Win32 Release"

# ADD CPP /FAs

!ELSEIF  "$(CFG)" == "zdoom - Win32 Debug"

!ELSEIF  "$(CFG)" == "zdoom - Win32 Profiling"

# ADD BASE CPP /FAs
# ADD CPP /FAs

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\m_misc.c

!IF  "$(CFG)" == "zdoom - Win32 Release"

# ADD CPP /FAs

!ELSEIF  "$(CFG)" == "zdoom - Win32 Debug"

!ELSEIF  "$(CFG)" == "zdoom - Win32 Profiling"

# ADD BASE CPP /FAs
# ADD CPP /FAs

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\m_option.c
# End Source File
# Begin Source File

SOURCE=.\m_random.c

!IF  "$(CFG)" == "zdoom - Win32 Release"

# ADD CPP /FAs

!ELSEIF  "$(CFG)" == "zdoom - Win32 Debug"

!ELSEIF  "$(CFG)" == "zdoom - Win32 Profiling"

# ADD BASE CPP /FAs
# ADD CPP /FAs

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\m_swap.c

!IF  "$(CFG)" == "zdoom - Win32 Release"

# ADD CPP /FAs

!ELSEIF  "$(CFG)" == "zdoom - Win32 Debug"

!ELSEIF  "$(CFG)" == "zdoom - Win32 Profiling"

# ADD BASE CPP /FAs
# ADD CPP /FAs

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\minilzo.c
# End Source File
# Begin Source File

SOURCE=.\p_acs.c
# End Source File
# Begin Source File

SOURCE=.\p_ceilng.c

!IF  "$(CFG)" == "zdoom - Win32 Release"

# ADD CPP /FAs

!ELSEIF  "$(CFG)" == "zdoom - Win32 Debug"

!ELSEIF  "$(CFG)" == "zdoom - Win32 Profiling"

# ADD BASE CPP /FAs
# ADD CPP /FAs

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\p_doors.c

!IF  "$(CFG)" == "zdoom - Win32 Release"

# ADD CPP /FAs

!ELSEIF  "$(CFG)" == "zdoom - Win32 Debug"

!ELSEIF  "$(CFG)" == "zdoom - Win32 Profiling"

# ADD BASE CPP /FAs
# ADD CPP /FAs

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\p_effect.c
# End Source File
# Begin Source File

SOURCE=.\p_enemy.c

!IF  "$(CFG)" == "zdoom - Win32 Release"

# ADD CPP /FAs

!ELSEIF  "$(CFG)" == "zdoom - Win32 Debug"

!ELSEIF  "$(CFG)" == "zdoom - Win32 Profiling"

# ADD BASE CPP /FAs
# ADD CPP /FAs

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\p_floor.c

!IF  "$(CFG)" == "zdoom - Win32 Release"

# ADD CPP /FAs

!ELSEIF  "$(CFG)" == "zdoom - Win32 Debug"

!ELSEIF  "$(CFG)" == "zdoom - Win32 Profiling"

# ADD BASE CPP /FAs
# ADD CPP /FAs

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\p_inter.c

!IF  "$(CFG)" == "zdoom - Win32 Release"

# ADD CPP /FAs

!ELSEIF  "$(CFG)" == "zdoom - Win32 Debug"

!ELSEIF  "$(CFG)" == "zdoom - Win32 Profiling"

# ADD BASE CPP /FAs
# ADD CPP /FAs

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\p_lights.c

!IF  "$(CFG)" == "zdoom - Win32 Release"

# ADD CPP /FAs

!ELSEIF  "$(CFG)" == "zdoom - Win32 Debug"

!ELSEIF  "$(CFG)" == "zdoom - Win32 Profiling"

# ADD BASE CPP /FAs
# ADD CPP /FAs

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\p_lnspec.c
# End Source File
# Begin Source File

SOURCE=.\p_map.c

!IF  "$(CFG)" == "zdoom - Win32 Release"

# ADD CPP /FAs

!ELSEIF  "$(CFG)" == "zdoom - Win32 Debug"

!ELSEIF  "$(CFG)" == "zdoom - Win32 Profiling"

# ADD BASE CPP /FAs
# ADD CPP /FAs

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\p_maputl.c

!IF  "$(CFG)" == "zdoom - Win32 Release"

# ADD CPP /FAs

!ELSEIF  "$(CFG)" == "zdoom - Win32 Debug"

!ELSEIF  "$(CFG)" == "zdoom - Win32 Profiling"

# ADD BASE CPP /FAs
# ADD CPP /FAs

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\p_mobj.c

!IF  "$(CFG)" == "zdoom - Win32 Release"

# ADD CPP /FAs

!ELSEIF  "$(CFG)" == "zdoom - Win32 Debug"

!ELSEIF  "$(CFG)" == "zdoom - Win32 Profiling"

# ADD BASE CPP /FAs
# ADD CPP /FAs

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\p_pillar.c
# End Source File
# Begin Source File

SOURCE=.\p_plats.c

!IF  "$(CFG)" == "zdoom - Win32 Release"

# ADD CPP /FAs

!ELSEIF  "$(CFG)" == "zdoom - Win32 Debug"

!ELSEIF  "$(CFG)" == "zdoom - Win32 Profiling"

# ADD BASE CPP /FAs
# ADD CPP /FAs

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\p_pspr.c

!IF  "$(CFG)" == "zdoom - Win32 Release"

# ADD CPP /FAs

!ELSEIF  "$(CFG)" == "zdoom - Win32 Debug"

!ELSEIF  "$(CFG)" == "zdoom - Win32 Profiling"

# ADD BASE CPP /FAs
# ADD CPP /FAs

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\p_quake.c
# End Source File
# Begin Source File

SOURCE=.\p_saveg.c

!IF  "$(CFG)" == "zdoom - Win32 Release"

# ADD CPP /FAs

!ELSEIF  "$(CFG)" == "zdoom - Win32 Debug"

!ELSEIF  "$(CFG)" == "zdoom - Win32 Profiling"

# ADD BASE CPP /FAs
# ADD CPP /FAs

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\p_setup.c

!IF  "$(CFG)" == "zdoom - Win32 Release"

# ADD CPP /FAs

!ELSEIF  "$(CFG)" == "zdoom - Win32 Debug"

!ELSEIF  "$(CFG)" == "zdoom - Win32 Profiling"

# ADD BASE CPP /FAs
# ADD CPP /FAs

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\p_sight.c

!IF  "$(CFG)" == "zdoom - Win32 Release"

# ADD CPP /FAs

!ELSEIF  "$(CFG)" == "zdoom - Win32 Debug"

!ELSEIF  "$(CFG)" == "zdoom - Win32 Profiling"

# ADD BASE CPP /FAs
# ADD CPP /FAs

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\p_spec.c

!IF  "$(CFG)" == "zdoom - Win32 Release"

# ADD CPP /FAs

!ELSEIF  "$(CFG)" == "zdoom - Win32 Debug"

!ELSEIF  "$(CFG)" == "zdoom - Win32 Profiling"

# ADD BASE CPP /FAs
# ADD CPP /FAs

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\p_switch.c

!IF  "$(CFG)" == "zdoom - Win32 Release"

# ADD CPP /FAs

!ELSEIF  "$(CFG)" == "zdoom - Win32 Debug"

!ELSEIF  "$(CFG)" == "zdoom - Win32 Profiling"

# ADD BASE CPP /FAs
# ADD CPP /FAs

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\p_telept.c

!IF  "$(CFG)" == "zdoom - Win32 Release"

# ADD CPP /FAs

!ELSEIF  "$(CFG)" == "zdoom - Win32 Debug"

!ELSEIF  "$(CFG)" == "zdoom - Win32 Profiling"

# ADD BASE CPP /FAs
# ADD CPP /FAs

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\p_things.c
# End Source File
# Begin Source File

SOURCE=.\p_tick.c

!IF  "$(CFG)" == "zdoom - Win32 Release"

# ADD CPP /FAs

!ELSEIF  "$(CFG)" == "zdoom - Win32 Debug"

!ELSEIF  "$(CFG)" == "zdoom - Win32 Profiling"

# ADD BASE CPP /FAs
# ADD CPP /FAs

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\p_user.c

!IF  "$(CFG)" == "zdoom - Win32 Release"

# ADD CPP /FAs

!ELSEIF  "$(CFG)" == "zdoom - Win32 Debug"

!ELSEIF  "$(CFG)" == "zdoom - Win32 Profiling"

# ADD BASE CPP /FAs
# ADD CPP /FAs

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\p_xlat.c
# End Source File
# Begin Source File

SOURCE=.\Po_man.c
# End Source File
# Begin Source File

SOURCE=.\r_bsp.c

!IF  "$(CFG)" == "zdoom - Win32 Release"

# ADD CPP /FAs

!ELSEIF  "$(CFG)" == "zdoom - Win32 Debug"

!ELSEIF  "$(CFG)" == "zdoom - Win32 Profiling"

# ADD BASE CPP /FAs
# ADD CPP /FAs

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\r_data.c

!IF  "$(CFG)" == "zdoom - Win32 Release"

# ADD CPP /FAs

!ELSEIF  "$(CFG)" == "zdoom - Win32 Debug"

!ELSEIF  "$(CFG)" == "zdoom - Win32 Profiling"

# ADD BASE CPP /FAs
# ADD CPP /FAs

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\r_draw.c

!IF  "$(CFG)" == "zdoom - Win32 Release"

# ADD CPP /FAs

!ELSEIF  "$(CFG)" == "zdoom - Win32 Debug"

!ELSEIF  "$(CFG)" == "zdoom - Win32 Profiling"

# ADD BASE CPP /FAs
# ADD CPP /FAs

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\r_drawt.c
# End Source File
# Begin Source File

SOURCE=.\r_main.c

!IF  "$(CFG)" == "zdoom - Win32 Release"

# ADD CPP /FAs

!ELSEIF  "$(CFG)" == "zdoom - Win32 Debug"

!ELSEIF  "$(CFG)" == "zdoom - Win32 Profiling"

# ADD BASE CPP /FAs
# ADD CPP /FAs

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\r_plane.c

!IF  "$(CFG)" == "zdoom - Win32 Release"

# ADD CPP /FAs

!ELSEIF  "$(CFG)" == "zdoom - Win32 Debug"

!ELSEIF  "$(CFG)" == "zdoom - Win32 Profiling"

# ADD BASE CPP /FAs
# ADD CPP /FAs

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\r_segs.c

!IF  "$(CFG)" == "zdoom - Win32 Release"

# ADD CPP /FAs

!ELSEIF  "$(CFG)" == "zdoom - Win32 Debug"

!ELSEIF  "$(CFG)" == "zdoom - Win32 Profiling"

# ADD BASE CPP /FAs
# ADD CPP /FAs

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\r_sky.c

!IF  "$(CFG)" == "zdoom - Win32 Release"

# ADD CPP /FAs

!ELSEIF  "$(CFG)" == "zdoom - Win32 Debug"

!ELSEIF  "$(CFG)" == "zdoom - Win32 Profiling"

# ADD BASE CPP /FAs
# ADD CPP /FAs

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\r_things.c

!IF  "$(CFG)" == "zdoom - Win32 Release"

# ADD CPP /FAs

!ELSEIF  "$(CFG)" == "zdoom - Win32 Debug"

!ELSEIF  "$(CFG)" == "zdoom - Win32 Profiling"

# ADD BASE CPP /FAs
# ADD CPP /FAs

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\s_sndseq.c
# End Source File
# Begin Source File

SOURCE=.\s_sound.c

!IF  "$(CFG)" == "zdoom - Win32 Release"

# ADD CPP /FAs

!ELSEIF  "$(CFG)" == "zdoom - Win32 Debug"

!ELSEIF  "$(CFG)" == "zdoom - Win32 Profiling"

# ADD BASE CPP /FAs
# ADD CPP /FAs

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\sc_man.c
# End Source File
# Begin Source File

SOURCE=.\st_lib.c

!IF  "$(CFG)" == "zdoom - Win32 Release"

# ADD CPP /FAs

!ELSEIF  "$(CFG)" == "zdoom - Win32 Debug"

!ELSEIF  "$(CFG)" == "zdoom - Win32 Profiling"

# ADD BASE CPP /FAs
# ADD CPP /FAs

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\st_new.c

!IF  "$(CFG)" == "zdoom - Win32 Release"

# ADD CPP /FAs

!ELSEIF  "$(CFG)" == "zdoom - Win32 Debug"

!ELSEIF  "$(CFG)" == "zdoom - Win32 Profiling"

# ADD BASE CPP /FAs
# ADD CPP /FAs

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\st_stuff.c

!IF  "$(CFG)" == "zdoom - Win32 Release"

# ADD CPP /FAs

!ELSEIF  "$(CFG)" == "zdoom - Win32 Debug"

!ELSEIF  "$(CFG)" == "zdoom - Win32 Profiling"

# ADD BASE CPP /FAs
# ADD CPP /FAs

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\tables.c

!IF  "$(CFG)" == "zdoom - Win32 Release"

# ADD CPP /FAs

!ELSEIF  "$(CFG)" == "zdoom - Win32 Debug"

!ELSEIF  "$(CFG)" == "zdoom - Win32 Profiling"

# ADD BASE CPP /FAs
# ADD CPP /FAs

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\v_draw.c
# End Source File
# Begin Source File

SOURCE=.\v_palett.c
# End Source File
# Begin Source File

SOURCE=.\v_text.c
# End Source File
# Begin Source File

SOURCE=.\v_video.c

!IF  "$(CFG)" == "zdoom - Win32 Release"

# ADD CPP /FAs

!ELSEIF  "$(CFG)" == "zdoom - Win32 Debug"

!ELSEIF  "$(CFG)" == "zdoom - Win32 Profiling"

# ADD BASE CPP /FAs
# ADD CPP /FAs

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\vectors.c
# End Source File
# Begin Source File

SOURCE=.\w_wad.c

!IF  "$(CFG)" == "zdoom - Win32 Release"

# ADD CPP /FAs

!ELSEIF  "$(CFG)" == "zdoom - Win32 Debug"

!ELSEIF  "$(CFG)" == "zdoom - Win32 Profiling"

# ADD BASE CPP /FAs
# ADD CPP /FAs

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\wi_stuff.c

!IF  "$(CFG)" == "zdoom - Win32 Release"

# ADD CPP /FAs

!ELSEIF  "$(CFG)" == "zdoom - Win32 Debug"

!ELSEIF  "$(CFG)" == "zdoom - Win32 Profiling"

# ADD BASE CPP /FAs
# ADD CPP /FAs

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\z_zone.c

!IF  "$(CFG)" == "zdoom - Win32 Release"

# ADD CPP /FAs

!ELSEIF  "$(CFG)" == "zdoom - Win32 Debug"

!ELSEIF  "$(CFG)" == "zdoom - Win32 Profiling"

# ADD BASE CPP /FAs
# ADD CPP /FAs

!ENDIF 

# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "*.H"
# Begin Source File

SOURCE=.\am_map.h
# End Source File
# Begin Source File

SOURCE=.\c_bind.h
# End Source File
# Begin Source File

SOURCE=.\c_cmds.h
# End Source File
# Begin Source File

SOURCE=.\c_consol.h
# End Source File
# Begin Source File

SOURCE=.\c_cvars.h
# End Source File
# Begin Source File

SOURCE=.\c_dispch.h
# End Source File
# Begin Source File

SOURCE=.\cmdlib.h
# End Source File
# Begin Source File

SOURCE=.\d_dehack.h
# End Source File
# Begin Source File

SOURCE=.\d_englsh.h
# End Source File
# Begin Source File

SOURCE=.\d_event.h
# End Source File
# Begin Source File

SOURCE=.\d_french.h
# End Source File
# Begin Source File

SOURCE=.\d_items.h
# End Source File
# Begin Source File

SOURCE=.\d_main.h
# End Source File
# Begin Source File

SOURCE=.\d_net.h
# End Source File
# Begin Source File

SOURCE=.\d_netinf.h
# End Source File
# Begin Source File

SOURCE=.\d_player.h
# End Source File
# Begin Source File

SOURCE=.\d_proto.h
# End Source File
# Begin Source File

SOURCE=.\d_textur.h
# End Source File
# Begin Source File

SOURCE=.\d_think.h
# End Source File
# Begin Source File

SOURCE=.\d_ticcmd.h
# End Source File
# Begin Source File

SOURCE=.\doomdata.h
# End Source File
# Begin Source File

SOURCE=.\doomdef.h
# End Source File
# Begin Source File

SOURCE=.\doomstat.h
# End Source File
# Begin Source File

SOURCE=.\doomtype.h
# End Source File
# Begin Source File

SOURCE=.\dstrings.h
# End Source File
# Begin Source File

SOURCE=.\f_finale.h
# End Source File
# Begin Source File

SOURCE=.\f_wipe.h
# End Source File
# Begin Source File

SOURCE=.\g_game.h
# End Source File
# Begin Source File

SOURCE=.\g_level.h
# End Source File
# Begin Source File

SOURCE=.\hu_stuff.h
# End Source File
# Begin Source File

SOURCE=.\Info.h
# End Source File
# Begin Source File

SOURCE=.\lzoconf.h
# End Source File
# Begin Source File

SOURCE=.\m_alloc.h
# End Source File
# Begin Source File

SOURCE=.\m_argv.h
# End Source File
# Begin Source File

SOURCE=.\m_bbox.h
# End Source File
# Begin Source File

SOURCE=.\m_cheat.h
# End Source File
# Begin Source File

SOURCE=.\m_fixed.h
# End Source File
# Begin Source File

SOURCE=.\m_menu.h
# End Source File
# Begin Source File

SOURCE=.\m_misc.h
# End Source File
# Begin Source File

SOURCE=.\m_random.h
# End Source File
# Begin Source File

SOURCE=.\m_swap.h
# End Source File
# Begin Source File

SOURCE=.\minilzo.h
# End Source File
# Begin Source File

SOURCE=.\p_acs.h
# End Source File
# Begin Source File

SOURCE=.\p_effect.h
# End Source File
# Begin Source File

SOURCE=.\p_inter.h
# End Source File
# Begin Source File

SOURCE=.\p_lnspec.h
# End Source File
# Begin Source File

SOURCE=.\p_local.h
# End Source File
# Begin Source File

SOURCE=.\p_mobj.h
# End Source File
# Begin Source File

SOURCE=.\p_pspr.h
# End Source File
# Begin Source File

SOURCE=.\p_saveg.h
# End Source File
# Begin Source File

SOURCE=.\p_setup.h
# End Source File
# Begin Source File

SOURCE=.\p_spec.h
# End Source File
# Begin Source File

SOURCE=.\p_tick.h
# End Source File
# Begin Source File

SOURCE=.\r_bsp.h
# End Source File
# Begin Source File

SOURCE=.\r_data.h
# End Source File
# Begin Source File

SOURCE=.\r_defs.h
# End Source File
# Begin Source File

SOURCE=.\r_draw.h
# End Source File
# Begin Source File

SOURCE=.\r_local.h
# End Source File
# Begin Source File

SOURCE=.\r_main.h
# End Source File
# Begin Source File

SOURCE=.\r_plane.h
# End Source File
# Begin Source File

SOURCE=.\r_segs.h
# End Source File
# Begin Source File

SOURCE=.\r_sky.h
# End Source File
# Begin Source File

SOURCE=.\r_state.h
# End Source File
# Begin Source File

SOURCE=.\r_things.h
# End Source File
# Begin Source File

SOURCE=.\s_sndseq.h
# End Source File
# Begin Source File

SOURCE=.\s_sound.h
# End Source File
# Begin Source File

SOURCE=.\sc_man.h
# End Source File
# Begin Source File

SOURCE=.\st_lib.h
# End Source File
# Begin Source File

SOURCE=.\st_stuff.h
# End Source File
# Begin Source File

SOURCE=.\Tables.h
# End Source File
# Begin Source File

SOURCE=.\v_palett.h
# End Source File
# Begin Source File

SOURCE=.\v_text.h
# End Source File
# Begin Source File

SOURCE=.\v_video.h
# End Source File
# Begin Source File

SOURCE=.\vectors.h
# End Source File
# Begin Source File

SOURCE=.\version.h
# End Source File
# Begin Source File

SOURCE=.\w_wad.h
# End Source File
# Begin Source File

SOURCE=.\wave.h
# End Source File
# Begin Source File

SOURCE=.\wi_stuff.h
# End Source File
# Begin Source File

SOURCE=.\z_zone.h
# End Source File
# End Group
# Begin Group "Assembly Files"

# PROP Default_Filter "*.nas; *.asm"
# Begin Source File

SOURCE=.\linear.nas

!IF  "$(CFG)" == "zdoom - Win32 Release"

# Begin Custom Build - Assembling...
IntDir=.\Release
InputPath=.\linear.nas
InputName=linear

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	nasmw -o $(IntDir)\$(InputName).obj -f win32 $(InputPath)

# End Custom Build

!ELSEIF  "$(CFG)" == "zdoom - Win32 Debug"

# Begin Custom Build - Assembling...
IntDir=.\Debug
InputPath=.\linear.nas
InputName=linear

"$(IntDir)/$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	nasmw -o $(IntDir)\$(InputName).obj -f win32 $(InputPath)

# End Custom Build

!ELSEIF  "$(CFG)" == "zdoom - Win32 Profiling"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\misc.nas

!IF  "$(CFG)" == "zdoom - Win32 Release"

# Begin Custom Build - Assembling...
IntDir=.\Release
InputPath=.\misc.nas
InputName=misc

"$(IntDir)/$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	nasmw -o $(IntDir)\$(InputName).obj -f win32 $(InputPath)

# End Custom Build

!ELSEIF  "$(CFG)" == "zdoom - Win32 Debug"

# Begin Custom Build - Assembling...
IntDir=.\Debug
InputPath=.\misc.nas
InputName=misc

"$(IntDir)/$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	nasmw -o $(IntDir)\$(InputName).obj -f win32 $(InputPath)

# End Custom Build

!ELSEIF  "$(CFG)" == "zdoom - Win32 Profiling"

# Begin Custom Build - Executing NASM...
IntDir=.\Profiling
InputPath=.\misc.nas
InputName=misc

"$(IntDir)/$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	d:\nasm\nasmw -o $(IntDir)\$(InputName).obj -f win32 $(InputPath)

# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\tmap.nas

!IF  "$(CFG)" == "zdoom - Win32 Release"

# Begin Custom Build - Assembling...
IntDir=.\Release
InputPath=.\tmap.nas
InputName=tmap

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	nasmw -o $(IntDir)\$(InputName).obj -f win32 $(InputPath)

# End Custom Build

!ELSEIF  "$(CFG)" == "zdoom - Win32 Debug"

# Begin Custom Build - Assembling...
IntDir=.\Debug
InputPath=.\tmap.nas
InputName=tmap

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	nasmw -o $(IntDir)\$(InputName).obj -f win32 $(InputPath)

# End Custom Build

!ELSEIF  "$(CFG)" == "zdoom - Win32 Profiling"

# Begin Custom Build - Executing NASM...
IntDir=.\Profiling
InputPath=.\tmap.nas
InputName=tmap

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	d:\nasm\nasmw -o $(IntDir)\$(InputName).obj -f win32 $(InputPath)

# End Custom Build

!ENDIF 

# End Source File
# End Group
# Begin Group "Text Files"

# PROP Default_Filter "*.txt"
# Begin Source File

SOURCE=..\commands.txt
# End Source File
# Begin Source File

SOURCE=".\docs\Rh-log.txt"
# End Source File
# End Group
# Begin Group "Win32 Files"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\win32\dxcrap.c
# End Source File
# Begin Source File

SOURCE=.\win32\I_input.c
# End Source File
# Begin Source File

SOURCE=.\win32\I_input.h
# End Source File
# Begin Source File

SOURCE=.\win32\I_main.c
# End Source File
# Begin Source File

SOURCE=.\win32\I_music.c
# End Source File
# Begin Source File

SOURCE=.\win32\I_music.h
# End Source File
# Begin Source File

SOURCE=.\win32\I_net.c
# End Source File
# Begin Source File

SOURCE=.\win32\i_net.h
# End Source File
# Begin Source File

SOURCE=.\win32\I_sound.c
# End Source File
# Begin Source File

SOURCE=.\win32\I_sound.h
# End Source File
# Begin Source File

SOURCE=.\win32\I_system.c
# End Source File
# Begin Source File

SOURCE=.\win32\I_system.h
# End Source File
# Begin Source File

SOURCE=.\win32\I_video.cpp
# End Source File
# Begin Source File

SOURCE=.\win32\I_video.h
# End Source File
# Begin Source File

SOURCE=.\win32\icon1.ico
# End Source File
# Begin Source File

SOURCE=.\win32\mid2strm.c
# End Source File
# Begin Source File

SOURCE=.\win32\mid2strm.h
# End Source File
# Begin Source File

SOURCE=.\win32\mus2strm.c
# End Source File
# Begin Source File

SOURCE=.\win32\mus2strm.h
# End Source File
# Begin Source File

SOURCE=.\win32\resource.h
# End Source File
# Begin Source File

SOURCE=.\win32\zdoom.rc
# End Source File
# End Group
# End Target
# End Project
