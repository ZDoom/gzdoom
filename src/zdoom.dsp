# Microsoft Developer Studio Project File - Name="zdoom" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
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
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
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
# ADD CPP /nologo /Gr /MD /W3 /GX /O2 /I "f:\fmod\api\inc" /I "win32" /I "fmodsound" /I "." /I "zlib-1.1.4" /I "g_shared" /I "g_doom" /I "g_raven" /I "g_heretic" /I "g_hexen" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D "USEASM" /YX /FD /I /fmod/api/inc" " /c
# SUBTRACT CPP /Fr
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /o "NUL" /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /o "NUL" /win32
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib comctl32.lib strmiids.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib wsock32.lib winmm.lib zlib-1.1.4/release/zlib.lib f:\fmod/api/lib/fmodvc.lib /nologo /subsystem:windows /pdb:none /map /machine:I386 /nodefaultlib:"libc" /nodefaultlib:"libcmt" /out:"../../zdoom.exe"
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
# ADD CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /I "f:\fmod\api\inc" /I "win32" /I "fmodsound" /I "." /I "zlib-1.1.4" /I "g_shared" /I "g_doom" /I "g_raven" /I "g_heretic" /I "g_hexen" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /FD /I /fmod/api/inc" " /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /o "NUL" /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /o "NUL" /win32
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /debug /machine:I386 /pdbtype:sept
# ADD LINK32 kernel32.lib user32.lib gdi32.lib comctl32.lib strmiids.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib ddraw.lib winmm.lib wsock32.lib zlib-1.1.4/debug/zlib.lib f:\fmod/api/lib/fmodvc.lib /nologo /subsystem:windows /debug /machine:I386 /nodefaultlib:"libcmt" /out:"../../doomdbg.exe"
# SUBTRACT LINK32 /pdb:none

!ENDIF 

# Begin Target

# Name "zdoom - Win32 Release"
# Name "zdoom - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "c;cpp"
# Begin Source File

SOURCE=.\Am_map.cpp
# End Source File
# Begin Source File

SOURCE=.\autostart.cpp
# End Source File
# Begin Source File

SOURCE=.\autozend.cpp
# End Source File
# Begin Source File

SOURCE=.\b_bot.cpp
# End Source File
# Begin Source File

SOURCE=.\B_func.cpp
# End Source File
# Begin Source File

SOURCE=.\B_game.cpp
# End Source File
# Begin Source File

SOURCE=.\b_move.cpp
# End Source File
# Begin Source File

SOURCE=.\B_think.cpp
# End Source File
# Begin Source File

SOURCE=.\bbannouncer.cpp
# End Source File
# Begin Source File

SOURCE=.\c_bind.cpp
# End Source File
# Begin Source File

SOURCE=.\c_cmds.cpp
# End Source File
# Begin Source File

SOURCE=.\c_console.cpp
# End Source File
# Begin Source File

SOURCE=.\c_cvars.cpp
# End Source File
# Begin Source File

SOURCE=.\c_dispatch.cpp
# End Source File
# Begin Source File

SOURCE=.\c_expr.cpp
# End Source File
# Begin Source File

SOURCE=.\cmdlib.cpp
# End Source File
# Begin Source File

SOURCE=.\colormatcher.cpp
# End Source File
# Begin Source File

SOURCE=.\configfile.cpp
# End Source File
# Begin Source File

SOURCE=.\ct_chat.cpp
# End Source File
# Begin Source File

SOURCE=.\d_dehacked.cpp
# End Source File
# Begin Source File

SOURCE=.\D_main.cpp
# End Source File
# Begin Source File

SOURCE=.\d_net.cpp
# End Source File
# Begin Source File

SOURCE=.\d_netinfo.cpp
# End Source File
# Begin Source File

SOURCE=.\d_protocol.cpp
# End Source File
# Begin Source File

SOURCE=.\decallib.cpp
# End Source File
# Begin Source File

SOURCE=.\dobject.cpp
# End Source File
# Begin Source File

SOURCE=.\doomdef.cpp
# End Source File
# Begin Source File

SOURCE=.\doomstat.cpp
# End Source File
# Begin Source File

SOURCE=.\dsectoreffect.cpp
# End Source File
# Begin Source File

SOURCE=.\dthinker.cpp
# End Source File
# Begin Source File

SOURCE=.\f_finale.cpp
# End Source File
# Begin Source File

SOURCE=.\f_wipe.cpp
# End Source File
# Begin Source File

SOURCE=.\farchive.cpp
# End Source File
# Begin Source File

SOURCE=.\g_game.cpp
# End Source File
# Begin Source File

SOURCE=.\g_level.cpp
# End Source File
# Begin Source File

SOURCE=.\gameconfigfile.cpp
# End Source File
# Begin Source File

SOURCE=.\gi.cpp
# End Source File
# Begin Source File

SOURCE=.\hu_scores.cpp
# End Source File
# Begin Source File

SOURCE=.\info.cpp
# End Source File
# Begin Source File

SOURCE=.\infodefaults.cpp
# End Source File
# Begin Source File

SOURCE=.\lumpconfigfile.cpp
# End Source File
# Begin Source File

SOURCE=.\m_alloc.cpp
# End Source File
# Begin Source File

SOURCE=.\m_argv.cpp
# End Source File
# Begin Source File

SOURCE=.\m_bbox.cpp
# End Source File
# Begin Source File

SOURCE=.\m_cheat.cpp
# End Source File
# Begin Source File

SOURCE=.\m_fixed.cpp
# End Source File
# Begin Source File

SOURCE=.\m_menu.cpp
# End Source File
# Begin Source File

SOURCE=.\m_misc.cpp
# End Source File
# Begin Source File

SOURCE=.\m_options.cpp
# End Source File
# Begin Source File

SOURCE=.\m_png.cpp
# End Source File
# Begin Source File

SOURCE=.\m_random.cpp
# End Source File
# Begin Source File

SOURCE=.\mus2midi.cpp
# End Source File
# Begin Source File

SOURCE=.\p_acs.cpp
# End Source File
# Begin Source File

SOURCE=.\p_buildmap.cpp
# End Source File
# Begin Source File

SOURCE=.\p_ceiling.cpp
# End Source File
# Begin Source File

SOURCE=.\p_doors.cpp
# End Source File
# Begin Source File

SOURCE=.\p_effect.cpp
# End Source File
# Begin Source File

SOURCE=.\p_enemy.cpp
# End Source File
# Begin Source File

SOURCE=.\p_floor.cpp
# End Source File
# Begin Source File

SOURCE=.\p_interaction.cpp
# End Source File
# Begin Source File

SOURCE=.\p_lights.cpp
# End Source File
# Begin Source File

SOURCE=.\p_lnspec.cpp
# End Source File
# Begin Source File

SOURCE=.\p_map.cpp
# End Source File
# Begin Source File

SOURCE=.\p_maputl.cpp
# End Source File
# Begin Source File

SOURCE=.\p_mobj.cpp
# End Source File
# Begin Source File

SOURCE=.\p_nodebuild.cpp
# End Source File
# Begin Source File

SOURCE=.\p_pillar.cpp
# End Source File
# Begin Source File

SOURCE=.\p_plats.cpp
# End Source File
# Begin Source File

SOURCE=.\p_pspr.cpp
# End Source File
# Begin Source File

SOURCE=.\p_saveg.cpp
# End Source File
# Begin Source File

SOURCE=.\p_sectors.cpp
# End Source File
# Begin Source File

SOURCE=.\p_setup.cpp
# End Source File
# Begin Source File

SOURCE=.\p_sight.cpp
# End Source File
# Begin Source File

SOURCE=.\p_spec.cpp
# End Source File
# Begin Source File

SOURCE=.\p_switch.cpp
# End Source File
# Begin Source File

SOURCE=.\p_teleport.cpp
# End Source File
# Begin Source File

SOURCE=.\p_terrain.cpp
# End Source File
# Begin Source File

SOURCE=.\p_things.cpp
# End Source File
# Begin Source File

SOURCE=.\p_tick.cpp
# End Source File
# Begin Source File

SOURCE=.\p_trace.cpp
# End Source File
# Begin Source File

SOURCE=.\p_user.cpp
# End Source File
# Begin Source File

SOURCE=.\p_writemap.cpp
# End Source File
# Begin Source File

SOURCE=.\p_xlat.cpp
# End Source File
# Begin Source File

SOURCE=.\po_man.cpp
# End Source File
# Begin Source File

SOURCE=.\s_advsound.cpp
# End Source File
# Begin Source File

SOURCE=.\s_playlist.cpp
# End Source File
# Begin Source File

SOURCE=.\s_sndseq.cpp
# End Source File
# Begin Source File

SOURCE=.\s_sound.cpp
# End Source File
# Begin Source File

SOURCE=.\sc_man.cpp
# End Source File
# Begin Source File

SOURCE=.\skins.cpp
# End Source File
# Begin Source File

SOURCE=.\st_stuff.cpp
# End Source File
# Begin Source File

SOURCE=.\stats.cpp
# End Source File
# Begin Source File

SOURCE=.\stringtable.cpp
# End Source File
# Begin Source File

SOURCE=.\tables.cpp
# End Source File
# Begin Source File

SOURCE=.\tempfiles.cpp
# End Source File
# Begin Source File

SOURCE=.\v_collection.cpp
# End Source File
# Begin Source File

SOURCE=.\v_draw.cpp
# End Source File
# Begin Source File

SOURCE=.\v_font.cpp
# End Source File
# Begin Source File

SOURCE=.\v_palette.cpp
# End Source File
# Begin Source File

SOURCE=.\v_pfx.cpp
# End Source File
# Begin Source File

SOURCE=.\v_text.cpp
# End Source File
# Begin Source File

SOURCE=.\v_video.cpp
# End Source File
# Begin Source File

SOURCE=.\vectors.cpp
# End Source File
# Begin Source File

SOURCE=.\w_wad.cpp
# End Source File
# Begin Source File

SOURCE=.\wi_stuff.cpp
# End Source File
# Begin Source File

SOURCE=.\z_zone.cpp
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h"
# Begin Source File

SOURCE=.\actor.h
# End Source File
# Begin Source File

SOURCE=.\am_map.h
# End Source File
# Begin Source File

SOURCE=.\announcer.h
# End Source File
# Begin Source File

SOURCE=.\autosegs.h
# End Source File
# Begin Source File

SOURCE=.\B_BOT.H
# End Source File
# Begin Source File

SOURCE=.\basicinlines.h
# End Source File
# Begin Source File

SOURCE=.\c_bind.h
# End Source File
# Begin Source File

SOURCE=.\c_console.h
# End Source File
# Begin Source File

SOURCE=.\c_cvars.h
# End Source File
# Begin Source File

SOURCE=.\c_dispatch.h
# End Source File
# Begin Source File

SOURCE=.\cmdlib.h
# End Source File
# Begin Source File

SOURCE=.\colormatcher.h
# End Source File
# Begin Source File

SOURCE=.\configfile.h
# End Source File
# Begin Source File

SOURCE=.\d_dehacked.h
# End Source File
# Begin Source File

SOURCE=.\d_dehackedactions.h
# End Source File
# Begin Source File

SOURCE=.\d_event.h
# End Source File
# Begin Source File

SOURCE=.\d_gui.h
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

SOURCE=.\d_protocol.h
# End Source File
# Begin Source File

SOURCE=.\d_textur.h
# End Source File
# Begin Source File

SOURCE=.\d_ticcmd.h
# End Source File
# Begin Source File

SOURCE=.\decallib.h
# End Source File
# Begin Source File

SOURCE=.\dobject.h
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

SOURCE=.\dsectoreffect.h
# End Source File
# Begin Source File

SOURCE=.\dthinker.h
# End Source File
# Begin Source File

SOURCE=.\errors.h
# End Source File
# Begin Source File

SOURCE=.\f_finale.h
# End Source File
# Begin Source File

SOURCE=.\f_wipe.h
# End Source File
# Begin Source File

SOURCE=.\farchive.h
# End Source File
# Begin Source File

SOURCE=.\g_game.h
# End Source File
# Begin Source File

SOURCE=.\g_level.h
# End Source File
# Begin Source File

SOURCE=.\gameconfigfile.h
# End Source File
# Begin Source File

SOURCE=.\gccinlines.h
# End Source File
# Begin Source File

SOURCE=.\gi.h
# End Source File
# Begin Source File

SOURCE=.\gstrings.h
# End Source File
# Begin Source File

SOURCE=.\hu_stuff.h
# End Source File
# Begin Source File

SOURCE=.\i_cd.h
# End Source File
# Begin Source File

SOURCE=.\i_movie.h
# End Source File
# Begin Source File

SOURCE=.\Info.h
# End Source File
# Begin Source File

SOURCE=.\infomacros.h
# End Source File
# Begin Source File

SOURCE=.\lists.h
# End Source File
# Begin Source File

SOURCE=.\lumpconfigfile.h
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

SOURCE=.\m_crc32.h
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

SOURCE=.\m_png.h
# End Source File
# Begin Source File

SOURCE=.\m_random.h
# End Source File
# Begin Source File

SOURCE=.\m_swap.h
# End Source File
# Begin Source File

SOURCE=.\mscinlines.h
# End Source File
# Begin Source File

SOURCE=.\mus2midi.h
# End Source File
# Begin Source File

SOURCE=.\p_acs.h
# End Source File
# Begin Source File

SOURCE=.\p_effect.h
# End Source File
# Begin Source File

SOURCE=.\p_enemy.h
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

SOURCE=.\p_nodebuild.h
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

SOURCE=.\p_terrain.h
# End Source File
# Begin Source File

SOURCE=.\p_tick.h
# End Source File
# Begin Source File

SOURCE=.\p_trace.h
# End Source File
# Begin Source File

SOURCE=.\s_playlist.h
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

SOURCE=.\skins.h
# End Source File
# Begin Source File

SOURCE=.\st_stuff.h
# End Source File
# Begin Source File

SOURCE=.\statnums.h
# End Source File
# Begin Source File

SOURCE=.\stats.h
# End Source File
# Begin Source File

SOURCE=.\stringenums.h
# End Source File
# Begin Source File

SOURCE=.\stringtable.h
# End Source File
# Begin Source File

SOURCE=.\Tables.h
# End Source File
# Begin Source File

SOURCE=.\tarray.h
# End Source File
# Begin Source File

SOURCE=.\tempfiles.h
# End Source File
# Begin Source File

SOURCE=.\templates.h
# End Source File
# Begin Source File

SOURCE=.\v_collection.h
# End Source File
# Begin Source File

SOURCE=.\v_font.h
# End Source File
# Begin Source File

SOURCE=.\v_palette.h
# End Source File
# Begin Source File

SOURCE=.\v_pfx.h
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

SOURCE=.\weightedlist.h
# End Source File
# Begin Source File

SOURCE=.\wi_stuff.h
# End Source File
# Begin Source File

SOURCE=.\z_zone.h
# End Source File
# End Group
# Begin Group "Assembly Files"

# PROP Default_Filter "nas; asm"
# Begin Source File

SOURCE=.\a.nas

!IF  "$(CFG)" == "zdoom - Win32 Release"

# Begin Custom Build - Assembling $(InputPath)...
IntDir=.\Release
InputPath=.\a.nas
InputName=a

"$(IntDir)/$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	nasmw -o $(IntDir)\$(InputName).obj -f win32 $(InputPath)

# End Custom Build

!ELSEIF  "$(CFG)" == "zdoom - Win32 Debug"

# Begin Custom Build - Assembling $(InputPath)...
IntDir=.\Debug
InputPath=.\a.nas
InputName=a

"$(IntDir)/$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	nasmw -o $(IntDir)\$(InputName).obj -f win32 $(InputPath)

# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\blocks.nas

!IF  "$(CFG)" == "zdoom - Win32 Release"

# Begin Custom Build - Assembling $(InputPath)...
IntDir=.\Release
InputPath=.\blocks.nas
InputName=blocks

"$(IntDir)/$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	nasmw -o $(IntDir)\$(InputName).obj -f win32 $(InputPath)

# End Custom Build

!ELSEIF  "$(CFG)" == "zdoom - Win32 Debug"

# Begin Custom Build - Assembling $(InputPath)...
IntDir=.\Debug
InputPath=.\blocks.nas
InputName=blocks

"$(IntDir)/$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	nasmw -o $(IntDir)\$(InputName).obj -f win32 $(InputPath)

# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\misc.nas

!IF  "$(CFG)" == "zdoom - Win32 Release"

# Begin Custom Build - Assembling $(InputPath)...
IntDir=.\Release
InputPath=.\misc.nas
InputName=misc

"$(IntDir)/$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	nasmw -o $(IntDir)\$(InputName).obj -f win32 $(InputPath)

# End Custom Build

!ELSEIF  "$(CFG)" == "zdoom - Win32 Debug"

# Begin Custom Build - Assembling $(InputPath)...
IntDir=.\Debug
InputPath=.\misc.nas
InputName=misc

"$(IntDir)/$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	nasmw -o $(IntDir)\$(InputName).obj -f win32 $(InputPath)

# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\tmap.nas

!IF  "$(CFG)" == "zdoom - Win32 Release"

# Begin Custom Build - Assembling $(InputPath)...
IntDir=.\Release
InputPath=.\tmap.nas
InputName=tmap

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	nasmw -o $(IntDir)\$(InputName).obj -f win32 $(InputPath)

# End Custom Build

!ELSEIF  "$(CFG)" == "zdoom - Win32 Debug"

# Begin Custom Build - Assembling $(InputPath)...
IntDir=.\Debug
InputPath=.\tmap.nas
InputName=tmap

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	nasmw -o $(IntDir)\$(InputName).obj -f win32 $(InputPath)

# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\tmap2.nas

!IF  "$(CFG)" == "zdoom - Win32 Release"

# Begin Custom Build - Assembling $(InputPath)...
IntDir=.\Release
InputPath=.\tmap2.nas
InputName=tmap2

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	nasmw -o $(IntDir)\$(InputName).obj -f win32 $(InputPath)

# End Custom Build

!ELSEIF  "$(CFG)" == "zdoom - Win32 Debug"

# Begin Custom Build - Assembling $(InputPath)...
IntDir=.\Debug
InputPath=.\tmap2.nas
InputName=tmap2

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	nasmw -o $(IntDir)\$(InputName).obj -f win32 $(InputPath)

# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\tmap3.nas

!IF  "$(CFG)" == "zdoom - Win32 Release"

# Begin Custom Build - Assembling $(InputPath)...
IntDir=.\Release
InputPath=.\tmap3.nas
InputName=tmap3

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	nasmw -o $(IntDir)\$(InputName).obj -f win32 $(InputPath)

# End Custom Build

!ELSEIF  "$(CFG)" == "zdoom - Win32 Debug"

# Begin Custom Build - Assembling $(InputPath)...
IntDir=.\Debug
InputPath=.\tmap3.nas
InputName=tmap3

"$(IntDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	nasmw -o $(IntDir)\$(InputName).obj -f win32 $(InputPath)

# End Custom Build

!ENDIF 

# End Source File
# End Group
# Begin Group "Win32 Files"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\win32\dxcrap.cpp
# End Source File
# Begin Source File

SOURCE=.\win32\hardware.cpp
# End Source File
# Begin Source File

SOURCE=.\win32\hardware.h
# End Source File
# Begin Source File

SOURCE=.\win32\helperthread.cpp
# End Source File
# Begin Source File

SOURCE=.\win32\helperthread.h
# End Source File
# Begin Source File

SOURCE=.\win32\i_cd.cpp
# End Source File
# Begin Source File

SOURCE=.\win32\i_input.cpp
# End Source File
# Begin Source File

SOURCE=.\win32\i_input.h
# End Source File
# Begin Source File

SOURCE=.\win32\i_main.cpp
# End Source File
# Begin Source File

SOURCE=.\win32\i_movie.cpp
# End Source File
# Begin Source File

SOURCE=.\win32\i_net.cpp
# End Source File
# Begin Source File

SOURCE=.\win32\i_net.h
# End Source File
# Begin Source File

SOURCE=.\win32\I_system.cpp
# End Source File
# Begin Source File

SOURCE=.\win32\I_system.h
# End Source File
# Begin Source File

SOURCE=.\win32\icon1.ico
# End Source File
# Begin Source File

SOURCE=.\win32\mid2strm.cpp
# End Source File
# Begin Source File

SOURCE=.\win32\mid2strm.h
# End Source File
# Begin Source File

SOURCE=.\win32\mus2strm.cpp
# End Source File
# Begin Source File

SOURCE=.\win32\mus2strm.h
# End Source File
# Begin Source File

SOURCE=.\win32\resource.h
# End Source File
# Begin Source File

SOURCE=.\win32\win32iface.h
# End Source File
# Begin Source File

SOURCE=.\win32\win32video.cpp
# End Source File
# Begin Source File

SOURCE=.\win32\zdoom.rc
# End Source File
# End Group
# Begin Group "Text Files"

# PROP Default_Filter "txt"
# Begin Group "Licenses"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\docs\BUILDLIC.TXT
# End Source File
# Begin Source File

SOURCE=..\docs\doomlic.txt
# End Source File
# End Group
# Begin Source File

SOURCE=..\docs\classes.txt
# End Source File
# Begin Source File

SOURCE=..\docs\colors.txt
# End Source File
# Begin Source File

SOURCE=..\docs\commands.txt
# End Source File
# Begin Source File

SOURCE=..\docs\history.txt
# End Source File
# Begin Source File

SOURCE=..\docs\notes.txt
# End Source File
# Begin Source File

SOURCE="..\docs\rh-log.txt"
# End Source File
# Begin Source File

SOURCE=..\docs\zdoom.txt
# End Source File
# End Group
# Begin Group "Shared Game"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\g_shared\a_action.cpp
# End Source File
# Begin Source File

SOURCE=.\g_shared\a_action.h
# End Source File
# Begin Source File

SOURCE=.\g_shared\a_artifacts.cpp
# End Source File
# Begin Source File

SOURCE=.\g_shared\a_artifacts.h
# End Source File
# Begin Source File

SOURCE=.\g_shared\a_bridge.cpp
# End Source File
# Begin Source File

SOURCE=.\g_shared\a_camera.cpp
# End Source File
# Begin Source File

SOURCE=.\g_shared\a_debris.cpp
# End Source File
# Begin Source File

SOURCE=.\g_shared\a_decals.cpp
# End Source File
# Begin Source File

SOURCE=.\g_shared\a_flashfader.cpp
# End Source File
# Begin Source File

SOURCE=.\g_shared\a_fountain.cpp
# End Source File
# Begin Source File

SOURCE=.\g_shared\a_lightning.cpp
# End Source File
# Begin Source File

SOURCE=.\g_shared\a_lightning.h
# End Source File
# Begin Source File

SOURCE=.\g_shared\a_movingcamera.cpp
# End Source File
# Begin Source File

SOURCE=.\g_shared\a_pickups.cpp
# End Source File
# Begin Source File

SOURCE=.\g_shared\a_pickups.h
# End Source File
# Begin Source File

SOURCE=.\g_shared\a_quake.cpp
# End Source File
# Begin Source File

SOURCE=.\g_shared\a_secrettrigger.cpp
# End Source File
# Begin Source File

SOURCE=.\g_shared\a_sectoraction.cpp
# End Source File
# Begin Source File

SOURCE=.\g_shared\a_sharedglobal.h
# End Source File
# Begin Source File

SOURCE=.\g_shared\a_sharedmisc.cpp
# End Source File
# Begin Source File

SOURCE=.\g_shared\a_skies.cpp
# End Source File
# Begin Source File

SOURCE=.\g_shared\a_spark.cpp
# End Source File
# Begin Source File

SOURCE=.\g_shared\a_splashes.cpp
# End Source File
# Begin Source File

SOURCE=.\g_shared\a_waterzone.cpp
# End Source File
# Begin Source File

SOURCE=.\g_shared\a_weapons.cpp
# End Source File
# Begin Source File

SOURCE=.\g_shared\hudmessages.cpp
# End Source File
# Begin Source File

SOURCE=.\g_shared\sbar.h
# End Source File
# Begin Source File

SOURCE=.\g_shared\shared_sbar.cpp
# End Source File
# End Group
# Begin Group "Doom Game"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\g_doom\a_arachnotron.cpp
# End Source File
# Begin Source File

SOURCE=.\g_doom\a_archvile.cpp
# End Source File
# Begin Source File

SOURCE=.\g_doom\a_bossbrain.cpp
# End Source File
# Begin Source File

SOURCE=.\g_doom\a_bruiser.cpp
# End Source File
# Begin Source File

SOURCE=.\g_doom\a_cacodemon.cpp
# End Source File
# Begin Source File

SOURCE=.\g_doom\a_cyberdemon.cpp
# End Source File
# Begin Source File

SOURCE=.\g_doom\a_demon.cpp
# End Source File
# Begin Source File

SOURCE=.\g_doom\a_doomarmor.cpp
# End Source File
# Begin Source File

SOURCE=.\g_doom\a_doomartifacts.cpp
# End Source File
# Begin Source File

SOURCE=.\g_doom\a_doomdecorations.cpp
# End Source File
# Begin Source File

SOURCE=.\g_doom\a_doomglobal.h
# End Source File
# Begin Source File

SOURCE=.\g_doom\a_doomhealth.cpp
# End Source File
# Begin Source File

SOURCE=.\g_doom\a_doomimp.cpp
# End Source File
# Begin Source File

SOURCE=.\g_doom\a_doomkeys.cpp
# End Source File
# Begin Source File

SOURCE=.\g_doom\a_doommisc.cpp
# End Source File
# Begin Source File

SOURCE=.\g_doom\a_doomplayer.cpp
# End Source File
# Begin Source File

SOURCE=.\g_doom\a_doomweaps.cpp
# End Source File
# Begin Source File

SOURCE=.\g_doom\a_fatso.cpp
# End Source File
# Begin Source File

SOURCE=.\g_doom\a_keen.cpp
# End Source File
# Begin Source File

SOURCE=.\g_doom\a_lostsoul.cpp
# End Source File
# Begin Source File

SOURCE=.\g_doom\a_painelemental.cpp
# End Source File
# Begin Source File

SOURCE=.\g_doom\a_possessed.cpp
# End Source File
# Begin Source File

SOURCE=.\g_doom\a_revenant.cpp
# End Source File
# Begin Source File

SOURCE=.\g_doom\a_scriptedmarine.cpp
# End Source File
# Begin Source File

SOURCE=.\g_doom\a_spidermaster.cpp
# End Source File
# Begin Source File

SOURCE=.\g_doom\doom_sbar.cpp
# End Source File
# End Group
# Begin Group "Raven Shared"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\g_raven\a_artiegg.cpp
# End Source File
# Begin Source File

SOURCE=.\g_raven\a_artitele.cpp
# End Source File
# Begin Source File

SOURCE=.\g_raven\a_minotaur.cpp
# End Source File
# Begin Source File

SOURCE=.\g_raven\a_ravenambient.cpp
# End Source File
# Begin Source File

SOURCE=.\g_raven\a_ravenartifacts.cpp
# End Source File
# Begin Source File

SOURCE=.\g_raven\a_ravenhealth.cpp
# End Source File
# Begin Source File

SOURCE=.\g_raven\ravenshared.h
# End Source File
# End Group
# Begin Group "Heretic Game"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\g_heretic\a_beast.cpp
# End Source File
# Begin Source File

SOURCE=.\g_heretic\a_chicken.cpp
# End Source File
# Begin Source File

SOURCE=.\g_heretic\a_clink.cpp
# End Source File
# Begin Source File

SOURCE=.\g_heretic\a_dsparil.cpp
# End Source File
# Begin Source File

SOURCE=.\g_heretic\a_hereticambience.cpp
# End Source File
# Begin Source File

SOURCE=.\g_heretic\a_hereticarmor.cpp
# End Source File
# Begin Source File

SOURCE=.\g_heretic\a_hereticartifacts.cpp
# End Source File
# Begin Source File

SOURCE=.\g_heretic\a_hereticdecorations.cpp
# End Source File
# Begin Source File

SOURCE=.\g_heretic\a_hereticglobal.h
# End Source File
# Begin Source File

SOURCE=.\g_heretic\a_hereticimp.cpp
# End Source File
# Begin Source File

SOURCE=.\g_heretic\a_heretickeys.cpp
# End Source File
# Begin Source File

SOURCE=.\g_heretic\a_hereticmisc.cpp
# End Source File
# Begin Source File

SOURCE=.\g_heretic\a_hereticplayer.cpp
# End Source File
# Begin Source File

SOURCE=.\g_heretic\a_hereticweaps.cpp
# End Source File
# Begin Source File

SOURCE=.\g_heretic\a_ironlich.cpp
# End Source File
# Begin Source File

SOURCE=.\g_heretic\a_knight.cpp
# End Source File
# Begin Source File

SOURCE=.\g_heretic\a_mummy.cpp
# End Source File
# Begin Source File

SOURCE=.\g_heretic\a_snake.cpp
# End Source File
# Begin Source File

SOURCE=.\g_hexen\a_summon.cpp
# End Source File
# Begin Source File

SOURCE=.\g_heretic\a_wizard.cpp
# End Source File
# Begin Source File

SOURCE=.\g_heretic\heretic_sbar.cpp
# End Source File
# End Group
# Begin Group "Hexen Game"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\g_hexen\a_bats.cpp
# End Source File
# Begin Source File

SOURCE=.\g_hexen\a_bishop.cpp
# End Source File
# Begin Source File

SOURCE=.\g_hexen\a_blastradius.cpp
# End Source File
# Begin Source File

SOURCE=.\g_hexen\a_boostarmor.cpp
# End Source File
# Begin Source File

SOURCE=.\g_hexen\a_boostmana.cpp
# End Source File
# Begin Source File

SOURCE=.\g_hexen\a_centaur.cpp
# End Source File
# Begin Source File

SOURCE=.\g_hexen\a_clericboss.cpp
# End Source File
# Begin Source File

SOURCE=.\g_hexen\a_clericflame.cpp
# End Source File
# Begin Source File

SOURCE=.\g_hexen\a_clericholy.cpp
# End Source File
# Begin Source File

SOURCE=.\g_hexen\a_clericmace.cpp
# End Source File
# Begin Source File

SOURCE=.\g_hexen\a_clericstaff.cpp
# End Source File
# Begin Source File

SOURCE=.\g_hexen\a_demons.cpp
# End Source File
# Begin Source File

SOURCE=.\g_hexen\a_dragon.cpp
# End Source File
# Begin Source File

SOURCE=.\g_hexen\a_ettin.cpp
# End Source File
# Begin Source File

SOURCE=.\g_hexen\a_fighteraxe.cpp
# End Source File
# Begin Source File

SOURCE=.\g_hexen\a_fighterboss.cpp
# End Source File
# Begin Source File

SOURCE=.\g_hexen\a_fighterhammer.cpp
# End Source File
# Begin Source File

SOURCE=.\g_hexen\a_firedemon.cpp
# End Source File
# Begin Source File

SOURCE=.\g_hexen\a_flame.cpp
# End Source File
# Begin Source File

SOURCE=.\g_hexen\a_flechette.cpp
# End Source File
# Begin Source File

SOURCE=.\g_hexen\a_fog.cpp
# End Source File
# Begin Source File

SOURCE=.\g_hexen\a_heresiarch.cpp
# End Source File
# Begin Source File

SOURCE=.\g_hexen\a_hexenarmor.cpp
# End Source File
# Begin Source File

SOURCE=.\g_hexen\a_hexendecorations.cpp
# End Source File
# Begin Source File

SOURCE=.\g_hexen\a_hexenfighter.cpp
# End Source File
# Begin Source File

SOURCE=.\g_hexen\a_hexenglobal.h
# End Source File
# Begin Source File

SOURCE=.\g_hexen\a_hexenkeys.cpp
# End Source File
# Begin Source File

SOURCE=.\g_hexen\a_hexenspecialdecs.cpp
# End Source File
# Begin Source File

SOURCE=.\g_hexen\a_iceguy.cpp
# End Source File
# Begin Source File

SOURCE=.\g_hexen\a_korax.cpp
# End Source File
# Begin Source File

SOURCE=.\g_hexen\a_mageboss.cpp
# End Source File
# Begin Source File

SOURCE=.\g_hexen\a_magecone.cpp
# End Source File
# Begin Source File

SOURCE=.\g_hexen\a_magelightning.cpp
# End Source File
# Begin Source File

SOURCE=.\g_hexen\a_magestaff.cpp
# End Source File
# Begin Source File

SOURCE=.\g_hexen\a_magewand.cpp
# End Source File
# Begin Source File

SOURCE=.\g_hexen\a_mana.cpp
# End Source File
# Begin Source File

SOURCE=.\g_hexen\a_puzzleitems.cpp
# End Source File
# Begin Source File

SOURCE=.\g_hexen\a_scriptprojectiles.cpp
# End Source File
# Begin Source File

SOURCE=.\g_hexen\a_serpent.cpp
# End Source File
# Begin Source File

SOURCE=.\g_hexen\a_speedboots.cpp
# End Source File
# Begin Source File

SOURCE=.\g_hexen\a_spike.cpp
# End Source File
# Begin Source File

SOURCE=.\g_hexen\a_teleportother.cpp
# End Source File
# Begin Source File

SOURCE=.\g_hexen\a_wraith.cpp
# End Source File
# End Group
# Begin Group "FMOD Audio Files"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\fmodsound\i_music.cpp
# End Source File
# Begin Source File

SOURCE=.\fmodsound\i_music.h
# End Source File
# Begin Source File

SOURCE=.\fmodsound\i_sound.cpp
# End Source File
# Begin Source File

SOURCE=.\fmodsound\i_sound.h
# End Source File
# End Group
# Begin Group "Render Core"

# PROP Default_Filter ""
# Begin Group "Render Sources"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\r_bsp.cpp
# End Source File
# Begin Source File

SOURCE=.\r_data.cpp
# End Source File
# Begin Source File

SOURCE=.\r_draw.cpp
# End Source File
# Begin Source File

SOURCE=.\r_drawt.cpp
# End Source File
# Begin Source File

SOURCE=.\r_main.cpp
# End Source File
# Begin Source File

SOURCE=.\r_plane.cpp
# End Source File
# Begin Source File

SOURCE=.\r_segs.cpp
# End Source File
# Begin Source File

SOURCE=.\r_sky.cpp
# End Source File
# Begin Source File

SOURCE=.\r_things.cpp
# End Source File
# End Group
# Begin Group "Render Headers"

# PROP Default_Filter ""
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
# End Group
# End Group
# Begin Source File

SOURCE=.\win32\cursor1.cur
# End Source File
# End Target
# End Project
