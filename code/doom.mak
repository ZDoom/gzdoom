# Microsoft Developer Studio Generated NMAKE File, Based on doom.dsp
!IF "$(CFG)" == ""
CFG=doom - Win32 Debug
!MESSAGE No configuration specified. Defaulting to doom - Win32 Debug.
!ENDIF 

!IF "$(CFG)" != "doom - Win32 Release" && "$(CFG)" != "doom - Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "doom.mak" CFG="doom - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "doom - Win32 Release" (based on "Win32 (x86) Application")
!MESSAGE "doom - Win32 Debug" (based on "Win32 (x86) Application")
!MESSAGE 
!ERROR An invalid configuration is specified.
!ENDIF 

!IF "$(OS)" == "Windows_NT"
NULL=
!ELSE 
NULL=nul
!ENDIF 

!IF  "$(CFG)" == "doom - Win32 Release"

OUTDIR=.\Release
INTDIR=.\Release

!IF "$(RECURSE)" == "0" 

ALL : "..\zdoom.exe"

!ELSE 

ALL : "..\zdoom.exe"

!ENDIF 

CLEAN :
	-@erase "$(INTDIR)\am_map.obj"
	-@erase "$(INTDIR)\c_bind.obj"
	-@erase "$(INTDIR)\c_cmds.obj"
	-@erase "$(INTDIR)\c_consol.obj"
	-@erase "$(INTDIR)\c_cvars.obj"
	-@erase "$(INTDIR)\c_dispch.obj"
	-@erase "$(INTDIR)\c_varini.obj"
	-@erase "$(INTDIR)\cmdlib.obj"
	-@erase "$(INTDIR)\d_dehack.obj"
	-@erase "$(INTDIR)\d_items.obj"
	-@erase "$(INTDIR)\d_main.obj"
	-@erase "$(INTDIR)\d_net.obj"
	-@erase "$(INTDIR)\d_netinf.obj"
	-@erase "$(INTDIR)\d_proto.obj"
	-@erase "$(INTDIR)\doom.res"
	-@erase "$(INTDIR)\doomdef.obj"
	-@erase "$(INTDIR)\doomstat.obj"
	-@erase "$(INTDIR)\dstrings.obj"
	-@erase "$(INTDIR)\dxcrap.obj"
	-@erase "$(INTDIR)\f_finale.obj"
	-@erase "$(INTDIR)\f_wipe.obj"
	-@erase "$(INTDIR)\g_game.obj"
	-@erase "$(INTDIR)\g_level.obj"
	-@erase "$(INTDIR)\hu_lib.obj"
	-@erase "$(INTDIR)\hu_stuff.obj"
	-@erase "$(INTDIR)\i_input.obj"
	-@erase "$(INTDIR)\i_main.obj"
	-@erase "$(INTDIR)\i_music.obj"
	-@erase "$(INTDIR)\i_net.obj"
	-@erase "$(INTDIR)\i_sound.obj"
	-@erase "$(INTDIR)\i_system.obj"
	-@erase "$(INTDIR)\i_video.obj"
	-@erase "$(INTDIR)\info.obj"
	-@erase "$(INTDIR)\m_alloc.obj"
	-@erase "$(INTDIR)\m_argv.obj"
	-@erase "$(INTDIR)\m_bbox.obj"
	-@erase "$(INTDIR)\m_cheat.obj"
	-@erase "$(INTDIR)\m_fixed.obj"
	-@erase "$(INTDIR)\m_menu.obj"
	-@erase "$(INTDIR)\m_misc.obj"
	-@erase "$(INTDIR)\m_option.obj"
	-@erase "$(INTDIR)\m_random.obj"
	-@erase "$(INTDIR)\m_swap.obj"
	-@erase "$(INTDIR)\p_ceilng.obj"
	-@erase "$(INTDIR)\p_doors.obj"
	-@erase "$(INTDIR)\p_enemy.obj"
	-@erase "$(INTDIR)\p_floor.obj"
	-@erase "$(INTDIR)\p_inter.obj"
	-@erase "$(INTDIR)\p_lights.obj"
	-@erase "$(INTDIR)\p_map.obj"
	-@erase "$(INTDIR)\p_maputl.obj"
	-@erase "$(INTDIR)\p_mobj.obj"
	-@erase "$(INTDIR)\p_plats.obj"
	-@erase "$(INTDIR)\p_pspr.obj"
	-@erase "$(INTDIR)\p_saveg.obj"
	-@erase "$(INTDIR)\p_setup.obj"
	-@erase "$(INTDIR)\p_sight.obj"
	-@erase "$(INTDIR)\p_spec.obj"
	-@erase "$(INTDIR)\p_switch.obj"
	-@erase "$(INTDIR)\p_telept.obj"
	-@erase "$(INTDIR)\p_tick.obj"
	-@erase "$(INTDIR)\p_user.obj"
	-@erase "$(INTDIR)\Qmus2mid.obj"
	-@erase "$(INTDIR)\r_bsp.obj"
	-@erase "$(INTDIR)\r_data.obj"
	-@erase "$(INTDIR)\r_draw.obj"
	-@erase "$(INTDIR)\r_main.obj"
	-@erase "$(INTDIR)\r_plane.obj"
	-@erase "$(INTDIR)\r_segs.obj"
	-@erase "$(INTDIR)\r_sky.obj"
	-@erase "$(INTDIR)\r_things.obj"
	-@erase "$(INTDIR)\s_sound.obj"
	-@erase "$(INTDIR)\sounds.obj"
	-@erase "$(INTDIR)\st_lib.obj"
	-@erase "$(INTDIR)\st_new.obj"
	-@erase "$(INTDIR)\st_stuff.obj"
	-@erase "$(INTDIR)\tables.obj"
	-@erase "$(INTDIR)\v_draw.obj"
	-@erase "$(INTDIR)\v_palett.obj"
	-@erase "$(INTDIR)\v_text.obj"
	-@erase "$(INTDIR)\v_video.obj"
	-@erase "$(INTDIR)\vc50.idb"
	-@erase "$(INTDIR)\vectors.obj"
	-@erase "$(INTDIR)\w_wad.obj"
	-@erase "$(INTDIR)\wi_stuff.obj"
	-@erase "$(INTDIR)\z_zone.obj"
	-@erase "$(OUTDIR)\zdoom.map"
	-@erase "..\zdoom.exe"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /G5 /MT /W3 /GX /O2 /I "e:\ptc\source" /D "WIN32" /D "NDEBUG"\
 /D "_WINDOWS" /Fp"$(INTDIR)\doom.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD\
 /c 
CPP_OBJS=.\Release/
CPP_SBRS=.

.c{$(CPP_OBJS)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(CPP_OBJS)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(CPP_OBJS)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.c{$(CPP_SBRS)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(CPP_SBRS)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(CPP_SBRS)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

MTL=midl.exe
MTL_PROJ=/nologo /D "NDEBUG" /mktyplib203 /o NUL /win32 
RSC=rc.exe
RSC_PROJ=/l 0x409 /fo"$(INTDIR)\doom.res" /d "NDEBUG" 
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\doom.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib\
 advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib\
 odbccp32.lib f:\games\doom\midas\lib\win32\vcretail\midas.lib wsock32.lib\
 E:\ptc\library\win32\vc5.x\release.lib winmm.lib /nologo /subsystem:windows\
 /pdb:none /map:"$(INTDIR)\zdoom.map" /machine:I386 /nodefaultlib:"libc"\
 /out:"../zdoom.exe" 
LINK32_OBJS= \
	"$(INTDIR)\am_map.obj" \
	"$(INTDIR)\c_bind.obj" \
	"$(INTDIR)\c_cmds.obj" \
	"$(INTDIR)\c_consol.obj" \
	"$(INTDIR)\c_cvars.obj" \
	"$(INTDIR)\c_dispch.obj" \
	"$(INTDIR)\c_varini.obj" \
	"$(INTDIR)\cmdlib.obj" \
	"$(INTDIR)\d_dehack.obj" \
	"$(INTDIR)\d_items.obj" \
	"$(INTDIR)\d_main.obj" \
	"$(INTDIR)\d_net.obj" \
	"$(INTDIR)\d_netinf.obj" \
	"$(INTDIR)\d_proto.obj" \
	"$(INTDIR)\doom.res" \
	"$(INTDIR)\doomdef.obj" \
	"$(INTDIR)\doomstat.obj" \
	"$(INTDIR)\dstrings.obj" \
	"$(INTDIR)\dxcrap.obj" \
	"$(INTDIR)\f_finale.obj" \
	"$(INTDIR)\f_wipe.obj" \
	"$(INTDIR)\g_game.obj" \
	"$(INTDIR)\g_level.obj" \
	"$(INTDIR)\hu_lib.obj" \
	"$(INTDIR)\hu_stuff.obj" \
	"$(INTDIR)\i_input.obj" \
	"$(INTDIR)\i_main.obj" \
	"$(INTDIR)\i_music.obj" \
	"$(INTDIR)\i_net.obj" \
	"$(INTDIR)\i_sound.obj" \
	"$(INTDIR)\i_system.obj" \
	"$(INTDIR)\i_video.obj" \
	"$(INTDIR)\info.obj" \
	"$(INTDIR)\m_alloc.obj" \
	"$(INTDIR)\m_argv.obj" \
	"$(INTDIR)\m_bbox.obj" \
	"$(INTDIR)\m_cheat.obj" \
	"$(INTDIR)\m_fixed.obj" \
	"$(INTDIR)\m_menu.obj" \
	"$(INTDIR)\m_misc.obj" \
	"$(INTDIR)\m_option.obj" \
	"$(INTDIR)\m_random.obj" \
	"$(INTDIR)\m_swap.obj" \
	"$(INTDIR)\misc.obj" \
	"$(INTDIR)\p_ceilng.obj" \
	"$(INTDIR)\p_doors.obj" \
	"$(INTDIR)\p_enemy.obj" \
	"$(INTDIR)\p_floor.obj" \
	"$(INTDIR)\p_inter.obj" \
	"$(INTDIR)\p_lights.obj" \
	"$(INTDIR)\p_map.obj" \
	"$(INTDIR)\p_maputl.obj" \
	"$(INTDIR)\p_mobj.obj" \
	"$(INTDIR)\p_plats.obj" \
	"$(INTDIR)\p_pspr.obj" \
	"$(INTDIR)\p_saveg.obj" \
	"$(INTDIR)\p_setup.obj" \
	"$(INTDIR)\p_sight.obj" \
	"$(INTDIR)\p_spec.obj" \
	"$(INTDIR)\p_switch.obj" \
	"$(INTDIR)\p_telept.obj" \
	"$(INTDIR)\p_tick.obj" \
	"$(INTDIR)\p_user.obj" \
	"$(INTDIR)\Qmus2mid.obj" \
	"$(INTDIR)\r_bsp.obj" \
	"$(INTDIR)\r_data.obj" \
	"$(INTDIR)\r_draw.obj" \
	"$(INTDIR)\r_main.obj" \
	"$(INTDIR)\r_plane.obj" \
	"$(INTDIR)\r_segs.obj" \
	"$(INTDIR)\r_sky.obj" \
	"$(INTDIR)\r_things.obj" \
	"$(INTDIR)\s_sound.obj" \
	"$(INTDIR)\sounds.obj" \
	"$(INTDIR)\st_lib.obj" \
	"$(INTDIR)\st_new.obj" \
	"$(INTDIR)\st_stuff.obj" \
	"$(INTDIR)\tables.obj" \
	"$(INTDIR)\tmap.obj" \
	"$(INTDIR)\v_draw.obj" \
	"$(INTDIR)\v_palett.obj" \
	"$(INTDIR)\v_text.obj" \
	"$(INTDIR)\v_video.obj" \
	"$(INTDIR)\vectors.obj" \
	"$(INTDIR)\w_wad.obj" \
	"$(INTDIR)\wi_stuff.obj" \
	"$(INTDIR)\z_zone.obj"

"..\zdoom.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ELSEIF  "$(CFG)" == "doom - Win32 Debug"

OUTDIR=.\Debug
INTDIR=.\Debug

!IF "$(RECURSE)" == "0" 

ALL : "..\doomdbg.exe"

!ELSE 

ALL : "..\doomdbg.exe"

!ENDIF 

CLEAN :
	-@erase "$(INTDIR)\am_map.obj"
	-@erase "$(INTDIR)\c_bind.obj"
	-@erase "$(INTDIR)\c_cmds.obj"
	-@erase "$(INTDIR)\c_consol.obj"
	-@erase "$(INTDIR)\c_cvars.obj"
	-@erase "$(INTDIR)\c_dispch.obj"
	-@erase "$(INTDIR)\c_varini.obj"
	-@erase "$(INTDIR)\cmdlib.obj"
	-@erase "$(INTDIR)\d_dehack.obj"
	-@erase "$(INTDIR)\d_items.obj"
	-@erase "$(INTDIR)\d_main.obj"
	-@erase "$(INTDIR)\d_net.obj"
	-@erase "$(INTDIR)\d_netinf.obj"
	-@erase "$(INTDIR)\d_proto.obj"
	-@erase "$(INTDIR)\doom.res"
	-@erase "$(INTDIR)\doomdef.obj"
	-@erase "$(INTDIR)\doomstat.obj"
	-@erase "$(INTDIR)\dstrings.obj"
	-@erase "$(INTDIR)\dxcrap.obj"
	-@erase "$(INTDIR)\f_finale.obj"
	-@erase "$(INTDIR)\f_wipe.obj"
	-@erase "$(INTDIR)\g_game.obj"
	-@erase "$(INTDIR)\g_level.obj"
	-@erase "$(INTDIR)\hu_lib.obj"
	-@erase "$(INTDIR)\hu_stuff.obj"
	-@erase "$(INTDIR)\i_input.obj"
	-@erase "$(INTDIR)\i_main.obj"
	-@erase "$(INTDIR)\i_music.obj"
	-@erase "$(INTDIR)\i_net.obj"
	-@erase "$(INTDIR)\i_sound.obj"
	-@erase "$(INTDIR)\i_system.obj"
	-@erase "$(INTDIR)\i_video.obj"
	-@erase "$(INTDIR)\info.obj"
	-@erase "$(INTDIR)\m_alloc.obj"
	-@erase "$(INTDIR)\m_argv.obj"
	-@erase "$(INTDIR)\m_bbox.obj"
	-@erase "$(INTDIR)\m_cheat.obj"
	-@erase "$(INTDIR)\m_fixed.obj"
	-@erase "$(INTDIR)\m_menu.obj"
	-@erase "$(INTDIR)\m_misc.obj"
	-@erase "$(INTDIR)\m_option.obj"
	-@erase "$(INTDIR)\m_random.obj"
	-@erase "$(INTDIR)\m_swap.obj"
	-@erase "$(INTDIR)\p_ceilng.obj"
	-@erase "$(INTDIR)\p_doors.obj"
	-@erase "$(INTDIR)\p_enemy.obj"
	-@erase "$(INTDIR)\p_floor.obj"
	-@erase "$(INTDIR)\p_inter.obj"
	-@erase "$(INTDIR)\p_lights.obj"
	-@erase "$(INTDIR)\p_map.obj"
	-@erase "$(INTDIR)\p_maputl.obj"
	-@erase "$(INTDIR)\p_mobj.obj"
	-@erase "$(INTDIR)\p_plats.obj"
	-@erase "$(INTDIR)\p_pspr.obj"
	-@erase "$(INTDIR)\p_saveg.obj"
	-@erase "$(INTDIR)\p_setup.obj"
	-@erase "$(INTDIR)\p_sight.obj"
	-@erase "$(INTDIR)\p_spec.obj"
	-@erase "$(INTDIR)\p_switch.obj"
	-@erase "$(INTDIR)\p_telept.obj"
	-@erase "$(INTDIR)\p_tick.obj"
	-@erase "$(INTDIR)\p_user.obj"
	-@erase "$(INTDIR)\Qmus2mid.obj"
	-@erase "$(INTDIR)\r_bsp.obj"
	-@erase "$(INTDIR)\r_data.obj"
	-@erase "$(INTDIR)\r_draw.obj"
	-@erase "$(INTDIR)\r_main.obj"
	-@erase "$(INTDIR)\r_plane.obj"
	-@erase "$(INTDIR)\r_segs.obj"
	-@erase "$(INTDIR)\r_sky.obj"
	-@erase "$(INTDIR)\r_things.obj"
	-@erase "$(INTDIR)\s_sound.obj"
	-@erase "$(INTDIR)\sounds.obj"
	-@erase "$(INTDIR)\st_lib.obj"
	-@erase "$(INTDIR)\st_new.obj"
	-@erase "$(INTDIR)\st_stuff.obj"
	-@erase "$(INTDIR)\tables.obj"
	-@erase "$(INTDIR)\v_draw.obj"
	-@erase "$(INTDIR)\v_palett.obj"
	-@erase "$(INTDIR)\v_text.obj"
	-@erase "$(INTDIR)\v_video.obj"
	-@erase "$(INTDIR)\vc50.idb"
	-@erase "$(INTDIR)\vc50.pdb"
	-@erase "$(INTDIR)\vectors.obj"
	-@erase "$(INTDIR)\w_wad.obj"
	-@erase "$(INTDIR)\wi_stuff.obj"
	-@erase "$(INTDIR)\z_zone.obj"
	-@erase "$(OUTDIR)\doomdbg.pdb"
	-@erase "..\doomdbg.exe"
	-@erase "..\doomdbg.ilk"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /G5 /MTd /W3 /Gm /GX /Zi /Od /I "e:\ptc\source" /D "WIN32" /D\
 "_DEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 
CPP_OBJS=.\Debug/
CPP_SBRS=.

.c{$(CPP_OBJS)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(CPP_OBJS)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(CPP_OBJS)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.c{$(CPP_SBRS)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(CPP_SBRS)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(CPP_SBRS)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

MTL=midl.exe
MTL_PROJ=/nologo /D "_DEBUG" /mktyplib203 /o NUL /win32 
RSC=rc.exe
RSC_PROJ=/l 0x409 /fo"$(INTDIR)\doom.res" /d "_DEBUG" 
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\doom.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib\
 advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib\
 odbccp32.lib ddraw.lib winmm.lib dinput.lib dxguid.lib\
 f:\games\doom\midas\lib\win32\vcretail\midas.lib wsock32.lib\
 e:\ptc\library\win32\vc5.x\debug.lib /nologo /subsystem:windows\
 /incremental:yes /pdb:"$(OUTDIR)\doomdbg.pdb" /debug /machine:I386\
 /nodefaultlib:"libcmt" /out:"../doomdbg.exe" 
LINK32_OBJS= \
	"$(INTDIR)\am_map.obj" \
	"$(INTDIR)\c_bind.obj" \
	"$(INTDIR)\c_cmds.obj" \
	"$(INTDIR)\c_consol.obj" \
	"$(INTDIR)\c_cvars.obj" \
	"$(INTDIR)\c_dispch.obj" \
	"$(INTDIR)\c_varini.obj" \
	"$(INTDIR)\cmdlib.obj" \
	"$(INTDIR)\d_dehack.obj" \
	"$(INTDIR)\d_items.obj" \
	"$(INTDIR)\d_main.obj" \
	"$(INTDIR)\d_net.obj" \
	"$(INTDIR)\d_netinf.obj" \
	"$(INTDIR)\d_proto.obj" \
	"$(INTDIR)\doom.res" \
	"$(INTDIR)\doomdef.obj" \
	"$(INTDIR)\doomstat.obj" \
	"$(INTDIR)\dstrings.obj" \
	"$(INTDIR)\dxcrap.obj" \
	"$(INTDIR)\f_finale.obj" \
	"$(INTDIR)\f_wipe.obj" \
	"$(INTDIR)\g_game.obj" \
	"$(INTDIR)\g_level.obj" \
	"$(INTDIR)\hu_lib.obj" \
	"$(INTDIR)\hu_stuff.obj" \
	"$(INTDIR)\i_input.obj" \
	"$(INTDIR)\i_main.obj" \
	"$(INTDIR)\i_music.obj" \
	"$(INTDIR)\i_net.obj" \
	"$(INTDIR)\i_sound.obj" \
	"$(INTDIR)\i_system.obj" \
	"$(INTDIR)\i_video.obj" \
	"$(INTDIR)\info.obj" \
	"$(INTDIR)\m_alloc.obj" \
	"$(INTDIR)\m_argv.obj" \
	"$(INTDIR)\m_bbox.obj" \
	"$(INTDIR)\m_cheat.obj" \
	"$(INTDIR)\m_fixed.obj" \
	"$(INTDIR)\m_menu.obj" \
	"$(INTDIR)\m_misc.obj" \
	"$(INTDIR)\m_option.obj" \
	"$(INTDIR)\m_random.obj" \
	"$(INTDIR)\m_swap.obj" \
	"$(INTDIR)\misc.obj" \
	"$(INTDIR)\p_ceilng.obj" \
	"$(INTDIR)\p_doors.obj" \
	"$(INTDIR)\p_enemy.obj" \
	"$(INTDIR)\p_floor.obj" \
	"$(INTDIR)\p_inter.obj" \
	"$(INTDIR)\p_lights.obj" \
	"$(INTDIR)\p_map.obj" \
	"$(INTDIR)\p_maputl.obj" \
	"$(INTDIR)\p_mobj.obj" \
	"$(INTDIR)\p_plats.obj" \
	"$(INTDIR)\p_pspr.obj" \
	"$(INTDIR)\p_saveg.obj" \
	"$(INTDIR)\p_setup.obj" \
	"$(INTDIR)\p_sight.obj" \
	"$(INTDIR)\p_spec.obj" \
	"$(INTDIR)\p_switch.obj" \
	"$(INTDIR)\p_telept.obj" \
	"$(INTDIR)\p_tick.obj" \
	"$(INTDIR)\p_user.obj" \
	"$(INTDIR)\Qmus2mid.obj" \
	"$(INTDIR)\r_bsp.obj" \
	"$(INTDIR)\r_data.obj" \
	"$(INTDIR)\r_draw.obj" \
	"$(INTDIR)\r_main.obj" \
	"$(INTDIR)\r_plane.obj" \
	"$(INTDIR)\r_segs.obj" \
	"$(INTDIR)\r_sky.obj" \
	"$(INTDIR)\r_things.obj" \
	"$(INTDIR)\s_sound.obj" \
	"$(INTDIR)\sounds.obj" \
	"$(INTDIR)\st_lib.obj" \
	"$(INTDIR)\st_new.obj" \
	"$(INTDIR)\st_stuff.obj" \
	"$(INTDIR)\tables.obj" \
	"$(INTDIR)\tmap.obj" \
	"$(INTDIR)\v_draw.obj" \
	"$(INTDIR)\v_palett.obj" \
	"$(INTDIR)\v_text.obj" \
	"$(INTDIR)\v_video.obj" \
	"$(INTDIR)\vectors.obj" \
	"$(INTDIR)\w_wad.obj" \
	"$(INTDIR)\wi_stuff.obj" \
	"$(INTDIR)\z_zone.obj"

"..\doomdbg.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ENDIF 


!IF "$(CFG)" == "doom - Win32 Release" || "$(CFG)" == "doom - Win32 Debug"
SOURCE=.\am_map.c

!IF  "$(CFG)" == "doom - Win32 Release"

DEP_CPP_AM_MA=\
	".\am_map.h"\
	".\c_cvars.h"\
	".\d_event.h"\
	".\d_items.h"\
	".\d_net.h"\
	".\d_netinf.h"\
	".\d_player.h"\
	".\d_proto.h"\
	".\d_think.h"\
	".\d_ticcmd.h"\
	".\doomdata.h"\
	".\doomdef.h"\
	".\doomstat.h"\
	".\doomtype.h"\
	".\dstrings.h"\
	".\g_level.h"\
	".\i_system.h"\
	".\Info.h"\
	".\m_cheat.h"\
	".\m_fixed.h"\
	".\p_local.h"\
	".\p_mobj.h"\
	".\p_pspr.h"\
	".\p_spec.h"\
	".\r_bsp.h"\
	".\r_data.h"\
	".\r_defs.h"\
	".\r_draw.h"\
	".\r_local.h"\
	".\r_main.h"\
	".\r_plane.h"\
	".\r_segs.h"\
	".\r_state.h"\
	".\r_things.h"\
	".\st_stuff.h"\
	".\Tables.h"\
	".\v_palett.h"\
	".\v_text.h"\
	".\v_video.h"\
	".\w_wad.h"\
	".\z_zone.h"\
	
CPP_SWITCHES=/nologo /G5 /MT /W3 /GX /O2 /I "e:\ptc\source" /D "WIN32" /D\
 "NDEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\am_map.obj" : $(SOURCE) $(DEP_CPP_AM_MA) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "doom - Win32 Debug"

DEP_CPP_AM_MA=\
	".\am_map.h"\
	".\c_cvars.h"\
	".\d_event.h"\
	".\d_items.h"\
	".\d_net.h"\
	".\d_netinf.h"\
	".\d_player.h"\
	".\d_proto.h"\
	".\d_think.h"\
	".\d_ticcmd.h"\
	".\doomdata.h"\
	".\doomdef.h"\
	".\doomstat.h"\
	".\doomtype.h"\
	".\dstrings.h"\
	".\g_level.h"\
	".\i_system.h"\
	".\Info.h"\
	".\m_cheat.h"\
	".\m_fixed.h"\
	".\p_local.h"\
	".\p_mobj.h"\
	".\p_pspr.h"\
	".\p_spec.h"\
	".\r_bsp.h"\
	".\r_data.h"\
	".\r_defs.h"\
	".\r_draw.h"\
	".\r_local.h"\
	".\r_main.h"\
	".\r_plane.h"\
	".\r_segs.h"\
	".\r_state.h"\
	".\r_things.h"\
	".\st_stuff.h"\
	".\Tables.h"\
	".\v_palett.h"\
	".\v_text.h"\
	".\v_video.h"\
	".\w_wad.h"\
	".\z_zone.h"\
	
CPP_SWITCHES=/nologo /G5 /MTd /W3 /Gm /GX /Zi /Od /I "e:\ptc\source" /D "WIN32"\
 /D "_DEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\am_map.obj" : $(SOURCE) $(DEP_CPP_AM_MA) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=.\c_bind.c

!IF  "$(CFG)" == "doom - Win32 Release"

DEP_CPP_C_BIN=\
	".\c_bind.h"\
	".\c_dispch.h"\
	".\cmdlib.h"\
	".\doomdef.h"\
	".\doomtype.h"\
	

"$(INTDIR)\c_bind.obj" : $(SOURCE) $(DEP_CPP_C_BIN) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "doom - Win32 Debug"

DEP_CPP_C_BIN=\
	".\c_bind.h"\
	".\c_dispch.h"\
	".\cmdlib.h"\
	".\doomdef.h"\
	".\doomtype.h"\
	

"$(INTDIR)\c_bind.obj" : $(SOURCE) $(DEP_CPP_C_BIN) "$(INTDIR)"


!ENDIF 

SOURCE=.\c_cmds.c

!IF  "$(CFG)" == "doom - Win32 Release"

DEP_CPP_C_CMD=\
	".\c_cmds.h"\
	".\c_consol.h"\
	".\c_cvars.h"\
	".\c_dispch.h"\
	".\cmdlib.h"\
	".\d_englsh.h"\
	".\d_event.h"\
	".\d_items.h"\
	".\d_net.h"\
	".\d_netinf.h"\
	".\d_player.h"\
	".\d_proto.h"\
	".\d_think.h"\
	".\d_ticcmd.h"\
	".\doomdata.h"\
	".\doomdef.h"\
	".\doomstat.h"\
	".\doomtype.h"\
	".\g_game.h"\
	".\g_level.h"\
	".\i_system.h"\
	".\Info.h"\
	".\m_fixed.h"\
	".\p_inter.h"\
	".\p_mobj.h"\
	".\p_pspr.h"\
	".\s_sound.h"\
	".\sounds.h"\
	".\Tables.h"\
	".\w_wad.h"\
	".\z_zone.h"\
	

"$(INTDIR)\c_cmds.obj" : $(SOURCE) $(DEP_CPP_C_CMD) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "doom - Win32 Debug"

DEP_CPP_C_CMD=\
	".\c_cmds.h"\
	".\c_consol.h"\
	".\c_cvars.h"\
	".\c_dispch.h"\
	".\cmdlib.h"\
	".\d_englsh.h"\
	".\d_event.h"\
	".\d_items.h"\
	".\d_net.h"\
	".\d_netinf.h"\
	".\d_player.h"\
	".\d_proto.h"\
	".\d_think.h"\
	".\d_ticcmd.h"\
	".\doomdata.h"\
	".\doomdef.h"\
	".\doomstat.h"\
	".\doomtype.h"\
	".\g_game.h"\
	".\g_level.h"\
	".\i_system.h"\
	".\Info.h"\
	".\m_fixed.h"\
	".\p_inter.h"\
	".\p_mobj.h"\
	".\p_pspr.h"\
	".\s_sound.h"\
	".\sounds.h"\
	".\Tables.h"\
	".\w_wad.h"\
	".\z_zone.h"\
	

"$(INTDIR)\c_cmds.obj" : $(SOURCE) $(DEP_CPP_C_CMD) "$(INTDIR)"


!ENDIF 

SOURCE=.\c_consol.c

!IF  "$(CFG)" == "doom - Win32 Release"

DEP_CPP_C_CON=\
	".\c_consol.h"\
	".\c_cvars.h"\
	".\c_dispch.h"\
	".\cmdlib.h"\
	".\d_event.h"\
	".\d_items.h"\
	".\d_net.h"\
	".\d_netinf.h"\
	".\d_player.h"\
	".\d_proto.h"\
	".\d_think.h"\
	".\d_ticcmd.h"\
	".\doomdata.h"\
	".\doomdef.h"\
	".\doomstat.h"\
	".\doomtype.h"\
	".\dstrings.h"\
	".\g_level.h"\
	".\hu_stuff.h"\
	".\i_input.h"\
	".\i_system.h"\
	".\Info.h"\
	".\m_alloc.h"\
	".\m_fixed.h"\
	".\m_swap.h"\
	".\p_mobj.h"\
	".\p_pspr.h"\
	".\r_data.h"\
	".\r_defs.h"\
	".\r_draw.h"\
	".\r_main.h"\
	".\r_state.h"\
	".\st_stuff.h"\
	".\Tables.h"\
	".\v_palett.h"\
	".\v_text.h"\
	".\v_video.h"\
	".\w_wad.h"\
	".\z_zone.h"\
	

"$(INTDIR)\c_consol.obj" : $(SOURCE) $(DEP_CPP_C_CON) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "doom - Win32 Debug"

DEP_CPP_C_CON=\
	".\c_consol.h"\
	".\c_cvars.h"\
	".\c_dispch.h"\
	".\cmdlib.h"\
	".\d_event.h"\
	".\d_items.h"\
	".\d_net.h"\
	".\d_netinf.h"\
	".\d_player.h"\
	".\d_proto.h"\
	".\d_think.h"\
	".\d_ticcmd.h"\
	".\doomdata.h"\
	".\doomdef.h"\
	".\doomstat.h"\
	".\doomtype.h"\
	".\dstrings.h"\
	".\g_level.h"\
	".\hu_stuff.h"\
	".\i_input.h"\
	".\i_system.h"\
	".\Info.h"\
	".\m_alloc.h"\
	".\m_fixed.h"\
	".\m_swap.h"\
	".\p_mobj.h"\
	".\p_pspr.h"\
	".\r_data.h"\
	".\r_defs.h"\
	".\r_draw.h"\
	".\r_main.h"\
	".\r_state.h"\
	".\st_stuff.h"\
	".\Tables.h"\
	".\v_palett.h"\
	".\v_text.h"\
	".\v_video.h"\
	".\w_wad.h"\
	".\z_zone.h"\
	

"$(INTDIR)\c_consol.obj" : $(SOURCE) $(DEP_CPP_C_CON) "$(INTDIR)"


!ENDIF 

SOURCE=.\c_cvars.c

!IF  "$(CFG)" == "doom - Win32 Release"

DEP_CPP_C_CVA=\
	".\c_consol.h"\
	".\c_cvars.h"\
	".\cmdlib.h"\
	".\d_event.h"\
	".\d_items.h"\
	".\d_net.h"\
	".\d_netinf.h"\
	".\d_player.h"\
	".\d_proto.h"\
	".\d_think.h"\
	".\d_ticcmd.h"\
	".\doomdata.h"\
	".\doomdef.h"\
	".\doomstat.h"\
	".\doomtype.h"\
	".\dstrings.h"\
	".\g_level.h"\
	".\i_system.h"\
	".\Info.h"\
	".\m_alloc.h"\
	".\m_fixed.h"\
	".\p_mobj.h"\
	".\p_pspr.h"\
	".\Tables.h"\
	
CPP_SWITCHES=/nologo /G5 /MT /W3 /GX /O2 /I "e:\ptc\source" /D "WIN32" /D\
 "NDEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\c_cvars.obj" : $(SOURCE) $(DEP_CPP_C_CVA) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "doom - Win32 Debug"

DEP_CPP_C_CVA=\
	".\c_consol.h"\
	".\c_cvars.h"\
	".\cmdlib.h"\
	".\d_event.h"\
	".\d_items.h"\
	".\d_net.h"\
	".\d_netinf.h"\
	".\d_player.h"\
	".\d_proto.h"\
	".\d_think.h"\
	".\d_ticcmd.h"\
	".\doomdata.h"\
	".\doomdef.h"\
	".\doomstat.h"\
	".\doomtype.h"\
	".\dstrings.h"\
	".\g_level.h"\
	".\i_system.h"\
	".\Info.h"\
	".\m_alloc.h"\
	".\m_fixed.h"\
	".\p_mobj.h"\
	".\p_pspr.h"\
	".\Tables.h"\
	
CPP_SWITCHES=/nologo /G5 /MTd /W3 /Gm /GX /Zi /Od /I "e:\ptc\source" /D "WIN32"\
 /D "_DEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\c_cvars.obj" : $(SOURCE) $(DEP_CPP_C_CVA) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=.\c_dispch.c

!IF  "$(CFG)" == "doom - Win32 Release"

DEP_CPP_C_DIS=\
	".\c_cmds.h"\
	".\c_consol.h"\
	".\c_cvars.h"\
	".\c_dispch.h"\
	".\cmdlib.h"\
	".\d_event.h"\
	".\d_items.h"\
	".\d_net.h"\
	".\d_netinf.h"\
	".\d_player.h"\
	".\d_proto.h"\
	".\d_think.h"\
	".\d_ticcmd.h"\
	".\doomdata.h"\
	".\doomdef.h"\
	".\doomstat.h"\
	".\doomtype.h"\
	".\g_level.h"\
	".\Info.h"\
	".\m_alloc.h"\
	".\m_argv.h"\
	".\m_fixed.h"\
	".\p_mobj.h"\
	".\p_pspr.h"\
	".\Tables.h"\
	

"$(INTDIR)\c_dispch.obj" : $(SOURCE) $(DEP_CPP_C_DIS) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "doom - Win32 Debug"

DEP_CPP_C_DIS=\
	".\c_cmds.h"\
	".\c_consol.h"\
	".\c_cvars.h"\
	".\c_dispch.h"\
	".\cmdlib.h"\
	".\d_event.h"\
	".\d_items.h"\
	".\d_net.h"\
	".\d_netinf.h"\
	".\d_player.h"\
	".\d_proto.h"\
	".\d_think.h"\
	".\d_ticcmd.h"\
	".\doomdata.h"\
	".\doomdef.h"\
	".\doomstat.h"\
	".\doomtype.h"\
	".\g_level.h"\
	".\Info.h"\
	".\m_alloc.h"\
	".\m_argv.h"\
	".\m_fixed.h"\
	".\p_mobj.h"\
	".\p_pspr.h"\
	".\Tables.h"\
	

"$(INTDIR)\c_dispch.obj" : $(SOURCE) $(DEP_CPP_C_DIS) "$(INTDIR)"


!ENDIF 

SOURCE=.\c_varini.c

!IF  "$(CFG)" == "doom - Win32 Release"

DEP_CPP_C_VAR=\
	".\c_cmds.h"\
	".\c_cvars.h"\
	".\cmdlib.h"\
	".\d_items.h"\
	".\d_netinf.h"\
	".\d_player.h"\
	".\d_proto.h"\
	".\d_think.h"\
	".\d_ticcmd.h"\
	".\doomdata.h"\
	".\doomdef.h"\
	".\doomtype.h"\
	".\dstrings.h"\
	".\Info.h"\
	".\m_fixed.h"\
	".\p_mobj.h"\
	".\p_pspr.h"\
	".\Tables.h"\
	

"$(INTDIR)\c_varini.obj" : $(SOURCE) $(DEP_CPP_C_VAR) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "doom - Win32 Debug"

DEP_CPP_C_VAR=\
	".\c_cmds.h"\
	".\c_cvars.h"\
	".\cmdlib.h"\
	".\d_items.h"\
	".\d_netinf.h"\
	".\d_player.h"\
	".\d_proto.h"\
	".\d_think.h"\
	".\d_ticcmd.h"\
	".\doomdata.h"\
	".\doomdef.h"\
	".\doomtype.h"\
	".\dstrings.h"\
	".\Info.h"\
	".\m_fixed.h"\
	".\p_mobj.h"\
	".\p_pspr.h"\
	".\Tables.h"\
	

"$(INTDIR)\c_varini.obj" : $(SOURCE) $(DEP_CPP_C_VAR) "$(INTDIR)"


!ENDIF 

SOURCE=.\cmdlib.c

!IF  "$(CFG)" == "doom - Win32 Release"

DEP_CPP_CMDLI=\
	".\cmdlib.h"\
	".\d_event.h"\
	".\d_proto.h"\
	".\d_ticcmd.h"\
	".\doomdef.h"\
	".\doomtype.h"\
	".\i_system.h"\
	".\m_alloc.h"\
	".\m_fixed.h"\
	
CPP_SWITCHES=/nologo /G5 /MT /W3 /GX /O2 /I "e:\ptc\source" /D "WIN32" /D\
 "NDEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\cmdlib.obj" : $(SOURCE) $(DEP_CPP_CMDLI) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "doom - Win32 Debug"

DEP_CPP_CMDLI=\
	".\cmdlib.h"\
	".\d_event.h"\
	".\d_proto.h"\
	".\d_ticcmd.h"\
	".\doomdef.h"\
	".\doomtype.h"\
	".\i_system.h"\
	".\m_alloc.h"\
	".\m_fixed.h"\
	
CPP_SWITCHES=/nologo /G5 /MTd /W3 /Gm /GX /Zi /Od /I "e:\ptc\source" /D "WIN32"\
 /D "_DEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\cmdlib.obj" : $(SOURCE) $(DEP_CPP_CMDLI) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=.\d_dehack.c

!IF  "$(CFG)" == "doom - Win32 Release"

DEP_CPP_D_DEH=\
	".\c_cvars.h"\
	".\cmdlib.h"\
	".\d_items.h"\
	".\d_net.h"\
	".\d_netinf.h"\
	".\d_player.h"\
	".\d_proto.h"\
	".\d_think.h"\
	".\d_ticcmd.h"\
	".\doomdata.h"\
	".\doomdef.h"\
	".\doomstat.h"\
	".\doomtype.h"\
	".\dstrings.h"\
	".\g_level.h"\
	".\Info.h"\
	".\m_cheat.h"\
	".\m_fixed.h"\
	".\m_misc.h"\
	".\p_mobj.h"\
	".\p_pspr.h"\
	".\sounds.h"\
	".\Tables.h"\
	".\w_wad.h"\
	
CPP_SWITCHES=/nologo /G5 /MT /W3 /GX /O2 /I "e:\ptc\source" /D "WIN32" /D\
 "NDEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\d_dehack.obj" : $(SOURCE) $(DEP_CPP_D_DEH) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "doom - Win32 Debug"

DEP_CPP_D_DEH=\
	".\c_cvars.h"\
	".\cmdlib.h"\
	".\d_items.h"\
	".\d_net.h"\
	".\d_netinf.h"\
	".\d_player.h"\
	".\d_proto.h"\
	".\d_think.h"\
	".\d_ticcmd.h"\
	".\doomdata.h"\
	".\doomdef.h"\
	".\doomstat.h"\
	".\doomtype.h"\
	".\dstrings.h"\
	".\g_level.h"\
	".\Info.h"\
	".\m_cheat.h"\
	".\m_fixed.h"\
	".\m_misc.h"\
	".\p_mobj.h"\
	".\p_pspr.h"\
	".\sounds.h"\
	".\Tables.h"\
	".\w_wad.h"\
	
CPP_SWITCHES=/nologo /G5 /MTd /W3 /Gm /GX /Zi /Od /I "e:\ptc\source" /D "WIN32"\
 /D "_DEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\d_dehack.obj" : $(SOURCE) $(DEP_CPP_D_DEH) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=.\d_items.c

!IF  "$(CFG)" == "doom - Win32 Release"

DEP_CPP_D_ITE=\
	".\d_englsh.h"\
	".\d_items.h"\
	".\d_think.h"\
	".\doomdef.h"\
	".\doomtype.h"\
	".\Info.h"\
	
CPP_SWITCHES=/nologo /G5 /MT /W3 /GX /O2 /I "e:\ptc\source" /D "WIN32" /D\
 "NDEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\d_items.obj" : $(SOURCE) $(DEP_CPP_D_ITE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "doom - Win32 Debug"

DEP_CPP_D_ITE=\
	".\d_englsh.h"\
	".\d_items.h"\
	".\d_think.h"\
	".\doomdef.h"\
	".\doomtype.h"\
	".\Info.h"\
	
CPP_SWITCHES=/nologo /G5 /MTd /W3 /Gm /GX /Zi /Od /I "e:\ptc\source" /D "WIN32"\
 /D "_DEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\d_items.obj" : $(SOURCE) $(DEP_CPP_D_ITE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=.\d_main.c

!IF  "$(CFG)" == "doom - Win32 Release"

DEP_CPP_D_MAI=\
	".\am_map.h"\
	".\c_cmds.h"\
	".\c_consol.h"\
	".\c_cvars.h"\
	".\c_dispch.h"\
	".\cmdlib.h"\
	".\d_dehack.h"\
	".\d_event.h"\
	".\d_items.h"\
	".\d_main.h"\
	".\d_net.h"\
	".\d_netinf.h"\
	".\d_player.h"\
	".\d_proto.h"\
	".\d_think.h"\
	".\d_ticcmd.h"\
	".\doomdata.h"\
	".\doomdef.h"\
	".\doomstat.h"\
	".\doomtype.h"\
	".\dstrings.h"\
	".\f_finale.h"\
	".\f_wipe.h"\
	".\g_game.h"\
	".\g_level.h"\
	".\hu_stuff.h"\
	".\i_sound.h"\
	".\i_system.h"\
	".\i_video.h"\
	".\Info.h"\
	".\m_alloc.h"\
	".\m_argv.h"\
	".\m_fixed.h"\
	".\m_menu.h"\
	".\m_misc.h"\
	".\p_mobj.h"\
	".\p_pspr.h"\
	".\p_setup.h"\
	".\r_bsp.h"\
	".\r_data.h"\
	".\r_defs.h"\
	".\r_draw.h"\
	".\r_local.h"\
	".\r_main.h"\
	".\r_plane.h"\
	".\r_segs.h"\
	".\r_sky.h"\
	".\r_state.h"\
	".\r_things.h"\
	".\s_sound.h"\
	".\sounds.h"\
	".\st_stuff.h"\
	".\Tables.h"\
	".\v_palett.h"\
	".\v_video.h"\
	".\w_wad.h"\
	".\wi_stuff.h"\
	".\z_zone.h"\
	
CPP_SWITCHES=/nologo /G5 /MT /W3 /GX /O2 /I "e:\ptc\source" /D "WIN32" /D\
 "NDEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\d_main.obj" : $(SOURCE) $(DEP_CPP_D_MAI) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "doom - Win32 Debug"

DEP_CPP_D_MAI=\
	".\am_map.h"\
	".\c_cmds.h"\
	".\c_consol.h"\
	".\c_cvars.h"\
	".\c_dispch.h"\
	".\cmdlib.h"\
	".\d_dehack.h"\
	".\d_event.h"\
	".\d_items.h"\
	".\d_main.h"\
	".\d_net.h"\
	".\d_netinf.h"\
	".\d_player.h"\
	".\d_proto.h"\
	".\d_think.h"\
	".\d_ticcmd.h"\
	".\doomdata.h"\
	".\doomdef.h"\
	".\doomstat.h"\
	".\doomtype.h"\
	".\dstrings.h"\
	".\f_finale.h"\
	".\f_wipe.h"\
	".\g_game.h"\
	".\g_level.h"\
	".\hu_stuff.h"\
	".\i_sound.h"\
	".\i_system.h"\
	".\i_video.h"\
	".\Info.h"\
	".\m_alloc.h"\
	".\m_argv.h"\
	".\m_fixed.h"\
	".\m_menu.h"\
	".\m_misc.h"\
	".\p_mobj.h"\
	".\p_pspr.h"\
	".\p_setup.h"\
	".\r_bsp.h"\
	".\r_data.h"\
	".\r_defs.h"\
	".\r_draw.h"\
	".\r_local.h"\
	".\r_main.h"\
	".\r_plane.h"\
	".\r_segs.h"\
	".\r_sky.h"\
	".\r_state.h"\
	".\r_things.h"\
	".\s_sound.h"\
	".\sounds.h"\
	".\st_stuff.h"\
	".\Tables.h"\
	".\v_palett.h"\
	".\v_video.h"\
	".\w_wad.h"\
	".\wi_stuff.h"\
	".\z_zone.h"\
	
CPP_SWITCHES=/nologo /G5 /MTd /W3 /Gm /GX /Zi /Od /I "e:\ptc\source" /D "WIN32"\
 /D "_DEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\d_main.obj" : $(SOURCE) $(DEP_CPP_D_MAI) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=.\d_net.c

!IF  "$(CFG)" == "doom - Win32 Release"

DEP_CPP_D_NET=\
	".\c_consol.h"\
	".\c_cvars.h"\
	".\cmdlib.h"\
	".\d_event.h"\
	".\d_items.h"\
	".\d_net.h"\
	".\d_netinf.h"\
	".\d_player.h"\
	".\d_proto.h"\
	".\d_think.h"\
	".\d_ticcmd.h"\
	".\doomdata.h"\
	".\doomdef.h"\
	".\doomstat.h"\
	".\doomtype.h"\
	".\g_game.h"\
	".\g_level.h"\
	".\i_net.h"\
	".\i_system.h"\
	".\i_video.h"\
	".\Info.h"\
	".\m_alloc.h"\
	".\m_cheat.h"\
	".\m_fixed.h"\
	".\m_menu.h"\
	".\p_mobj.h"\
	".\p_pspr.h"\
	".\r_data.h"\
	".\r_defs.h"\
	".\r_state.h"\
	".\s_sound.h"\
	".\sounds.h"\
	".\Tables.h"\
	".\v_palett.h"\
	".\v_video.h"\
	
CPP_SWITCHES=/nologo /G5 /MT /W3 /GX /O2 /I "e:\ptc\source" /D "WIN32" /D\
 "NDEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\d_net.obj" : $(SOURCE) $(DEP_CPP_D_NET) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "doom - Win32 Debug"

DEP_CPP_D_NET=\
	".\c_consol.h"\
	".\c_cvars.h"\
	".\cmdlib.h"\
	".\d_event.h"\
	".\d_items.h"\
	".\d_net.h"\
	".\d_netinf.h"\
	".\d_player.h"\
	".\d_proto.h"\
	".\d_think.h"\
	".\d_ticcmd.h"\
	".\doomdata.h"\
	".\doomdef.h"\
	".\doomstat.h"\
	".\doomtype.h"\
	".\g_game.h"\
	".\g_level.h"\
	".\i_net.h"\
	".\i_system.h"\
	".\i_video.h"\
	".\Info.h"\
	".\m_alloc.h"\
	".\m_cheat.h"\
	".\m_fixed.h"\
	".\m_menu.h"\
	".\p_mobj.h"\
	".\p_pspr.h"\
	".\r_data.h"\
	".\r_defs.h"\
	".\r_state.h"\
	".\s_sound.h"\
	".\sounds.h"\
	".\Tables.h"\
	".\v_palett.h"\
	".\v_video.h"\
	
CPP_SWITCHES=/nologo /G5 /MTd /W3 /Gm /GX /Zi /Od /I "e:\ptc\source" /D "WIN32"\
 /D "_DEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\d_net.obj" : $(SOURCE) $(DEP_CPP_D_NET) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=.\d_netinf.c

!IF  "$(CFG)" == "doom - Win32 Release"

DEP_CPP_D_NETI=\
	".\c_cvars.h"\
	".\d_event.h"\
	".\d_items.h"\
	".\d_net.h"\
	".\d_netinf.h"\
	".\d_player.h"\
	".\d_proto.h"\
	".\d_think.h"\
	".\d_ticcmd.h"\
	".\doomdata.h"\
	".\doomdef.h"\
	".\doomstat.h"\
	".\doomtype.h"\
	".\g_level.h"\
	".\i_system.h"\
	".\Info.h"\
	".\m_fixed.h"\
	".\p_mobj.h"\
	".\p_pspr.h"\
	".\r_data.h"\
	".\r_defs.h"\
	".\r_draw.h"\
	".\r_state.h"\
	".\Tables.h"\
	".\v_palett.h"\
	".\v_video.h"\
	

"$(INTDIR)\d_netinf.obj" : $(SOURCE) $(DEP_CPP_D_NETI) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "doom - Win32 Debug"

DEP_CPP_D_NETI=\
	".\c_cvars.h"\
	".\d_event.h"\
	".\d_items.h"\
	".\d_net.h"\
	".\d_netinf.h"\
	".\d_player.h"\
	".\d_proto.h"\
	".\d_think.h"\
	".\d_ticcmd.h"\
	".\doomdata.h"\
	".\doomdef.h"\
	".\doomstat.h"\
	".\doomtype.h"\
	".\g_level.h"\
	".\i_system.h"\
	".\Info.h"\
	".\m_fixed.h"\
	".\p_mobj.h"\
	".\p_pspr.h"\
	".\r_data.h"\
	".\r_defs.h"\
	".\r_draw.h"\
	".\r_state.h"\
	".\Tables.h"\
	".\v_palett.h"\
	".\v_video.h"\
	

"$(INTDIR)\d_netinf.obj" : $(SOURCE) $(DEP_CPP_D_NETI) "$(INTDIR)"


!ENDIF 

SOURCE=.\d_proto.c

!IF  "$(CFG)" == "doom - Win32 Release"

DEP_CPP_D_PRO=\
	".\c_cvars.h"\
	".\cmdlib.h"\
	".\d_event.h"\
	".\d_items.h"\
	".\d_net.h"\
	".\d_netinf.h"\
	".\d_player.h"\
	".\d_proto.h"\
	".\d_think.h"\
	".\d_ticcmd.h"\
	".\doomdata.h"\
	".\doomdef.h"\
	".\doomstat.h"\
	".\doomtype.h"\
	".\g_level.h"\
	".\i_system.h"\
	".\Info.h"\
	".\m_alloc.h"\
	".\m_fixed.h"\
	".\p_mobj.h"\
	".\p_pspr.h"\
	".\Tables.h"\
	".\z_zone.h"\
	

"$(INTDIR)\d_proto.obj" : $(SOURCE) $(DEP_CPP_D_PRO) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "doom - Win32 Debug"

DEP_CPP_D_PRO=\
	".\c_cvars.h"\
	".\cmdlib.h"\
	".\d_event.h"\
	".\d_items.h"\
	".\d_net.h"\
	".\d_netinf.h"\
	".\d_player.h"\
	".\d_proto.h"\
	".\d_think.h"\
	".\d_ticcmd.h"\
	".\doomdata.h"\
	".\doomdef.h"\
	".\doomstat.h"\
	".\doomtype.h"\
	".\g_level.h"\
	".\i_system.h"\
	".\Info.h"\
	".\m_alloc.h"\
	".\m_fixed.h"\
	".\p_mobj.h"\
	".\p_pspr.h"\
	".\Tables.h"\
	".\z_zone.h"\
	

"$(INTDIR)\d_proto.obj" : $(SOURCE) $(DEP_CPP_D_PRO) "$(INTDIR)"


!ENDIF 

SOURCE=.\doomdef.c
DEP_CPP_DOOMD=\
	".\doomdef.h"\
	

!IF  "$(CFG)" == "doom - Win32 Release"

CPP_SWITCHES=/nologo /G5 /MT /W3 /GX /O2 /I "e:\ptc\source" /D "WIN32" /D\
 "NDEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\doomdef.obj" : $(SOURCE) $(DEP_CPP_DOOMD) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "doom - Win32 Debug"

CPP_SWITCHES=/nologo /G5 /MTd /W3 /Gm /GX /Zi /Od /I "e:\ptc\source" /D "WIN32"\
 /D "_DEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\doomdef.obj" : $(SOURCE) $(DEP_CPP_DOOMD) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=.\doomstat.c

!IF  "$(CFG)" == "doom - Win32 Release"

DEP_CPP_DOOMS=\
	".\c_cvars.h"\
	".\d_items.h"\
	".\d_net.h"\
	".\d_netinf.h"\
	".\d_player.h"\
	".\d_proto.h"\
	".\d_think.h"\
	".\d_ticcmd.h"\
	".\doomdata.h"\
	".\doomdef.h"\
	".\doomstat.h"\
	".\doomtype.h"\
	".\g_level.h"\
	".\Info.h"\
	".\m_fixed.h"\
	".\p_mobj.h"\
	".\p_pspr.h"\
	".\Tables.h"\
	
CPP_SWITCHES=/nologo /G5 /MT /W3 /GX /O2 /I "e:\ptc\source" /D "WIN32" /D\
 "NDEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\doomstat.obj" : $(SOURCE) $(DEP_CPP_DOOMS) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "doom - Win32 Debug"

DEP_CPP_DOOMS=\
	".\c_cvars.h"\
	".\d_items.h"\
	".\d_net.h"\
	".\d_netinf.h"\
	".\d_player.h"\
	".\d_proto.h"\
	".\d_think.h"\
	".\d_ticcmd.h"\
	".\doomdata.h"\
	".\doomdef.h"\
	".\doomstat.h"\
	".\doomtype.h"\
	".\g_level.h"\
	".\Info.h"\
	".\m_fixed.h"\
	".\p_mobj.h"\
	".\p_pspr.h"\
	".\Tables.h"\
	
CPP_SWITCHES=/nologo /G5 /MTd /W3 /Gm /GX /Zi /Od /I "e:\ptc\source" /D "WIN32"\
 /D "_DEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\doomstat.obj" : $(SOURCE) $(DEP_CPP_DOOMS) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=.\dstrings.c

!IF  "$(CFG)" == "doom - Win32 Release"

DEP_CPP_DSTRI=\
	".\c_cvars.h"\
	".\cmdlib.h"\
	".\d_items.h"\
	".\d_net.h"\
	".\d_netinf.h"\
	".\d_player.h"\
	".\d_proto.h"\
	".\d_think.h"\
	".\d_ticcmd.h"\
	".\doomdata.h"\
	".\doomdef.h"\
	".\doomstat.h"\
	".\doomtype.h"\
	".\dstrings.h"\
	".\g_level.h"\
	".\Info.h"\
	".\m_argv.h"\
	".\m_fixed.h"\
	".\p_mobj.h"\
	".\p_pspr.h"\
	".\Tables.h"\
	".\w_wad.h"\
	".\z_zone.h"\
	
CPP_SWITCHES=/nologo /G5 /MT /W3 /GX /O2 /I "e:\ptc\source" /D "WIN32" /D\
 "NDEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\dstrings.obj" : $(SOURCE) $(DEP_CPP_DSTRI) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "doom - Win32 Debug"

DEP_CPP_DSTRI=\
	".\c_cvars.h"\
	".\cmdlib.h"\
	".\d_items.h"\
	".\d_net.h"\
	".\d_netinf.h"\
	".\d_player.h"\
	".\d_proto.h"\
	".\d_think.h"\
	".\d_ticcmd.h"\
	".\doomdata.h"\
	".\doomdef.h"\
	".\doomstat.h"\
	".\doomtype.h"\
	".\dstrings.h"\
	".\g_level.h"\
	".\Info.h"\
	".\m_argv.h"\
	".\m_fixed.h"\
	".\p_mobj.h"\
	".\p_pspr.h"\
	".\Tables.h"\
	".\w_wad.h"\
	".\z_zone.h"\
	
CPP_SWITCHES=/nologo /G5 /MTd /W3 /Gm /GX /Zi /Od /I "e:\ptc\source" /D "WIN32"\
 /D "_DEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\dstrings.obj" : $(SOURCE) $(DEP_CPP_DSTRI) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=.\dxcrap.c
DEP_CPP_DXCRA=\
	"e:\dxsdk\sdk\inc\dinput.h"\
	

"$(INTDIR)\dxcrap.obj" : $(SOURCE) $(DEP_CPP_DXCRA) "$(INTDIR)"


SOURCE=.\f_finale.c

!IF  "$(CFG)" == "doom - Win32 Release"

DEP_CPP_F_FIN=\
	".\c_cvars.h"\
	".\d_event.h"\
	".\d_items.h"\
	".\d_net.h"\
	".\d_netinf.h"\
	".\d_player.h"\
	".\d_proto.h"\
	".\d_think.h"\
	".\d_ticcmd.h"\
	".\doomdata.h"\
	".\doomdef.h"\
	".\doomstat.h"\
	".\doomtype.h"\
	".\dstrings.h"\
	".\g_level.h"\
	".\hu_stuff.h"\
	".\i_system.h"\
	".\Info.h"\
	".\m_fixed.h"\
	".\m_swap.h"\
	".\p_mobj.h"\
	".\p_pspr.h"\
	".\r_data.h"\
	".\r_defs.h"\
	".\r_state.h"\
	".\s_sound.h"\
	".\sounds.h"\
	".\Tables.h"\
	".\v_palett.h"\
	".\v_video.h"\
	".\w_wad.h"\
	".\z_zone.h"\
	
CPP_SWITCHES=/nologo /G5 /MT /W3 /GX /O2 /I "e:\ptc\source" /D "WIN32" /D\
 "NDEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\f_finale.obj" : $(SOURCE) $(DEP_CPP_F_FIN) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "doom - Win32 Debug"

DEP_CPP_F_FIN=\
	".\c_cvars.h"\
	".\d_event.h"\
	".\d_items.h"\
	".\d_net.h"\
	".\d_netinf.h"\
	".\d_player.h"\
	".\d_proto.h"\
	".\d_think.h"\
	".\d_ticcmd.h"\
	".\doomdata.h"\
	".\doomdef.h"\
	".\doomstat.h"\
	".\doomtype.h"\
	".\dstrings.h"\
	".\g_level.h"\
	".\hu_stuff.h"\
	".\i_system.h"\
	".\Info.h"\
	".\m_fixed.h"\
	".\m_swap.h"\
	".\p_mobj.h"\
	".\p_pspr.h"\
	".\r_data.h"\
	".\r_defs.h"\
	".\r_state.h"\
	".\s_sound.h"\
	".\sounds.h"\
	".\Tables.h"\
	".\v_palett.h"\
	".\v_video.h"\
	".\w_wad.h"\
	".\z_zone.h"\
	
CPP_SWITCHES=/nologo /G5 /MTd /W3 /Gm /GX /Zi /Od /I "e:\ptc\source" /D "WIN32"\
 /D "_DEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\f_finale.obj" : $(SOURCE) $(DEP_CPP_F_FIN) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=.\f_wipe.c

!IF  "$(CFG)" == "doom - Win32 Release"

DEP_CPP_F_WIP=\
	".\c_cvars.h"\
	".\d_items.h"\
	".\d_netinf.h"\
	".\d_player.h"\
	".\d_proto.h"\
	".\d_think.h"\
	".\d_ticcmd.h"\
	".\doomdata.h"\
	".\doomdef.h"\
	".\doomtype.h"\
	".\f_wipe.h"\
	".\i_video.h"\
	".\Info.h"\
	".\m_alloc.h"\
	".\m_fixed.h"\
	".\m_random.h"\
	".\p_mobj.h"\
	".\p_pspr.h"\
	".\r_data.h"\
	".\r_defs.h"\
	".\r_state.h"\
	".\Tables.h"\
	".\v_palett.h"\
	".\v_video.h"\
	".\z_zone.h"\
	
CPP_SWITCHES=/nologo /G5 /MT /W3 /GX /O2 /I "e:\ptc\source" /D "WIN32" /D\
 "NDEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\f_wipe.obj" : $(SOURCE) $(DEP_CPP_F_WIP) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "doom - Win32 Debug"

DEP_CPP_F_WIP=\
	".\c_cvars.h"\
	".\d_items.h"\
	".\d_netinf.h"\
	".\d_player.h"\
	".\d_proto.h"\
	".\d_think.h"\
	".\d_ticcmd.h"\
	".\doomdata.h"\
	".\doomdef.h"\
	".\doomtype.h"\
	".\f_wipe.h"\
	".\i_video.h"\
	".\Info.h"\
	".\m_alloc.h"\
	".\m_fixed.h"\
	".\m_random.h"\
	".\p_mobj.h"\
	".\p_pspr.h"\
	".\r_data.h"\
	".\r_defs.h"\
	".\r_state.h"\
	".\Tables.h"\
	".\v_palett.h"\
	".\v_video.h"\
	".\z_zone.h"\
	
CPP_SWITCHES=/nologo /G5 /MTd /W3 /Gm /GX /Zi /Od /I "e:\ptc\source" /D "WIN32"\
 /D "_DEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\f_wipe.obj" : $(SOURCE) $(DEP_CPP_F_WIP) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=.\g_game.c

!IF  "$(CFG)" == "doom - Win32 Release"

DEP_CPP_G_GAM=\
	".\am_map.h"\
	".\c_bind.h"\
	".\c_consol.h"\
	".\c_cvars.h"\
	".\c_dispch.h"\
	".\cmdlib.h"\
	".\d_event.h"\
	".\d_items.h"\
	".\d_main.h"\
	".\d_net.h"\
	".\d_netinf.h"\
	".\d_player.h"\
	".\d_proto.h"\
	".\d_think.h"\
	".\d_ticcmd.h"\
	".\doomdata.h"\
	".\doomdef.h"\
	".\doomstat.h"\
	".\doomtype.h"\
	".\dstrings.h"\
	".\f_finale.h"\
	".\g_game.h"\
	".\g_level.h"\
	".\hu_stuff.h"\
	".\i_system.h"\
	".\Info.h"\
	".\m_argv.h"\
	".\m_fixed.h"\
	".\m_menu.h"\
	".\m_misc.h"\
	".\m_random.h"\
	".\p_local.h"\
	".\p_mobj.h"\
	".\p_pspr.h"\
	".\p_saveg.h"\
	".\p_setup.h"\
	".\p_spec.h"\
	".\p_tick.h"\
	".\r_bsp.h"\
	".\r_data.h"\
	".\r_defs.h"\
	".\r_draw.h"\
	".\r_local.h"\
	".\r_main.h"\
	".\r_plane.h"\
	".\r_segs.h"\
	".\r_sky.h"\
	".\r_state.h"\
	".\r_things.h"\
	".\s_sound.h"\
	".\sounds.h"\
	".\st_stuff.h"\
	".\Tables.h"\
	".\v_palett.h"\
	".\v_video.h"\
	".\w_wad.h"\
	".\wi_stuff.h"\
	".\z_zone.h"\
	
CPP_SWITCHES=/nologo /G5 /MT /W3 /GX /O2 /I "e:\ptc\source" /D "WIN32" /D\
 "NDEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\g_game.obj" : $(SOURCE) $(DEP_CPP_G_GAM) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "doom - Win32 Debug"

DEP_CPP_G_GAM=\
	".\am_map.h"\
	".\c_bind.h"\
	".\c_consol.h"\
	".\c_cvars.h"\
	".\c_dispch.h"\
	".\cmdlib.h"\
	".\d_event.h"\
	".\d_items.h"\
	".\d_main.h"\
	".\d_net.h"\
	".\d_netinf.h"\
	".\d_player.h"\
	".\d_proto.h"\
	".\d_think.h"\
	".\d_ticcmd.h"\
	".\doomdata.h"\
	".\doomdef.h"\
	".\doomstat.h"\
	".\doomtype.h"\
	".\dstrings.h"\
	".\f_finale.h"\
	".\g_game.h"\
	".\g_level.h"\
	".\hu_stuff.h"\
	".\i_system.h"\
	".\Info.h"\
	".\m_argv.h"\
	".\m_fixed.h"\
	".\m_menu.h"\
	".\m_misc.h"\
	".\m_random.h"\
	".\p_local.h"\
	".\p_mobj.h"\
	".\p_pspr.h"\
	".\p_saveg.h"\
	".\p_setup.h"\
	".\p_spec.h"\
	".\p_tick.h"\
	".\r_bsp.h"\
	".\r_data.h"\
	".\r_defs.h"\
	".\r_draw.h"\
	".\r_local.h"\
	".\r_main.h"\
	".\r_plane.h"\
	".\r_segs.h"\
	".\r_sky.h"\
	".\r_state.h"\
	".\r_things.h"\
	".\s_sound.h"\
	".\sounds.h"\
	".\st_stuff.h"\
	".\Tables.h"\
	".\v_palett.h"\
	".\v_video.h"\
	".\w_wad.h"\
	".\wi_stuff.h"\
	".\z_zone.h"\
	
CPP_SWITCHES=/nologo /G5 /MTd /W3 /Gm /GX /Zi /Od /I "e:\ptc\source" /D "WIN32"\
 /D "_DEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\g_game.obj" : $(SOURCE) $(DEP_CPP_G_GAM) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=.\g_level.c

!IF  "$(CFG)" == "doom - Win32 Release"

DEP_CPP_G_LEV=\
	".\am_map.h"\
	".\c_consol.h"\
	".\c_cvars.h"\
	".\c_dispch.h"\
	".\cmdlib.h"\
	".\d_event.h"\
	".\d_items.h"\
	".\d_main.h"\
	".\d_net.h"\
	".\d_netinf.h"\
	".\d_player.h"\
	".\d_proto.h"\
	".\d_think.h"\
	".\d_ticcmd.h"\
	".\doomdata.h"\
	".\doomdef.h"\
	".\doomstat.h"\
	".\doomtype.h"\
	".\dstrings.h"\
	".\f_finale.h"\
	".\g_game.h"\
	".\g_level.h"\
	".\hu_stuff.h"\
	".\i_system.h"\
	".\Info.h"\
	".\m_alloc.h"\
	".\m_fixed.h"\
	".\m_random.h"\
	".\p_local.h"\
	".\p_mobj.h"\
	".\p_pspr.h"\
	".\p_setup.h"\
	".\p_spec.h"\
	".\r_bsp.h"\
	".\r_data.h"\
	".\r_defs.h"\
	".\r_draw.h"\
	".\r_local.h"\
	".\r_main.h"\
	".\r_plane.h"\
	".\r_segs.h"\
	".\r_sky.h"\
	".\r_state.h"\
	".\r_things.h"\
	".\s_sound.h"\
	".\st_stuff.h"\
	".\Tables.h"\
	".\v_palett.h"\
	".\v_video.h"\
	".\w_wad.h"\
	".\wi_stuff.h"\
	".\z_zone.h"\
	
CPP_SWITCHES=/nologo /G5 /MT /W3 /GX /O2 /I "e:\ptc\source" /D "WIN32" /D\
 "NDEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\g_level.obj" : $(SOURCE) $(DEP_CPP_G_LEV) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "doom - Win32 Debug"

DEP_CPP_G_LEV=\
	".\am_map.h"\
	".\c_consol.h"\
	".\c_cvars.h"\
	".\c_dispch.h"\
	".\cmdlib.h"\
	".\d_event.h"\
	".\d_items.h"\
	".\d_main.h"\
	".\d_net.h"\
	".\d_netinf.h"\
	".\d_player.h"\
	".\d_proto.h"\
	".\d_think.h"\
	".\d_ticcmd.h"\
	".\doomdata.h"\
	".\doomdef.h"\
	".\doomstat.h"\
	".\doomtype.h"\
	".\dstrings.h"\
	".\f_finale.h"\
	".\g_game.h"\
	".\g_level.h"\
	".\hu_stuff.h"\
	".\i_system.h"\
	".\Info.h"\
	".\m_alloc.h"\
	".\m_fixed.h"\
	".\m_random.h"\
	".\p_local.h"\
	".\p_mobj.h"\
	".\p_pspr.h"\
	".\p_setup.h"\
	".\p_spec.h"\
	".\r_bsp.h"\
	".\r_data.h"\
	".\r_defs.h"\
	".\r_draw.h"\
	".\r_local.h"\
	".\r_main.h"\
	".\r_plane.h"\
	".\r_segs.h"\
	".\r_sky.h"\
	".\r_state.h"\
	".\r_things.h"\
	".\s_sound.h"\
	".\st_stuff.h"\
	".\Tables.h"\
	".\v_palett.h"\
	".\v_video.h"\
	".\w_wad.h"\
	".\wi_stuff.h"\
	".\z_zone.h"\
	
CPP_SWITCHES=/nologo /G5 /MTd /W3 /Gm /GX /Zi /Od /I "e:\ptc\source" /D "WIN32"\
 /D "_DEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\g_level.obj" : $(SOURCE) $(DEP_CPP_G_LEV) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=.\hu_lib.c

!IF  "$(CFG)" == "doom - Win32 Release"

DEP_CPP_HU_LI=\
	".\c_consol.h"\
	".\c_cvars.h"\
	".\cmdlib.h"\
	".\d_event.h"\
	".\d_items.h"\
	".\d_netinf.h"\
	".\d_player.h"\
	".\d_proto.h"\
	".\d_think.h"\
	".\d_ticcmd.h"\
	".\doomdata.h"\
	".\doomdef.h"\
	".\doomtype.h"\
	".\hu_lib.h"\
	".\Info.h"\
	".\m_fixed.h"\
	".\m_swap.h"\
	".\p_mobj.h"\
	".\p_pspr.h"\
	".\r_bsp.h"\
	".\r_data.h"\
	".\r_defs.h"\
	".\r_draw.h"\
	".\r_local.h"\
	".\r_main.h"\
	".\r_plane.h"\
	".\r_segs.h"\
	".\r_state.h"\
	".\r_things.h"\
	".\Tables.h"\
	".\v_palett.h"\
	".\v_video.h"\
	
CPP_SWITCHES=/nologo /G5 /MT /W3 /GX /O2 /I "e:\ptc\source" /D "WIN32" /D\
 "NDEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\hu_lib.obj" : $(SOURCE) $(DEP_CPP_HU_LI) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "doom - Win32 Debug"

DEP_CPP_HU_LI=\
	".\c_consol.h"\
	".\c_cvars.h"\
	".\cmdlib.h"\
	".\d_event.h"\
	".\d_items.h"\
	".\d_netinf.h"\
	".\d_player.h"\
	".\d_proto.h"\
	".\d_think.h"\
	".\d_ticcmd.h"\
	".\doomdata.h"\
	".\doomdef.h"\
	".\doomtype.h"\
	".\hu_lib.h"\
	".\Info.h"\
	".\m_fixed.h"\
	".\m_swap.h"\
	".\p_mobj.h"\
	".\p_pspr.h"\
	".\r_bsp.h"\
	".\r_data.h"\
	".\r_defs.h"\
	".\r_draw.h"\
	".\r_local.h"\
	".\r_main.h"\
	".\r_plane.h"\
	".\r_segs.h"\
	".\r_state.h"\
	".\r_things.h"\
	".\Tables.h"\
	".\v_palett.h"\
	".\v_video.h"\
	
CPP_SWITCHES=/nologo /G5 /MTd /W3 /Gm /GX /Zi /Od /I "e:\ptc\source" /D "WIN32"\
 /D "_DEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\hu_lib.obj" : $(SOURCE) $(DEP_CPP_HU_LI) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=.\hu_stuff.c

!IF  "$(CFG)" == "doom - Win32 Release"

DEP_CPP_HU_ST=\
	".\c_consol.h"\
	".\c_cvars.h"\
	".\c_dispch.h"\
	".\cmdlib.h"\
	".\d_event.h"\
	".\d_items.h"\
	".\d_net.h"\
	".\d_netinf.h"\
	".\d_player.h"\
	".\d_proto.h"\
	".\d_think.h"\
	".\d_ticcmd.h"\
	".\doomdata.h"\
	".\doomdef.h"\
	".\doomstat.h"\
	".\doomtype.h"\
	".\dstrings.h"\
	".\g_level.h"\
	".\hu_lib.h"\
	".\hu_stuff.h"\
	".\Info.h"\
	".\m_fixed.h"\
	".\m_swap.h"\
	".\p_mobj.h"\
	".\p_pspr.h"\
	".\r_data.h"\
	".\r_defs.h"\
	".\r_state.h"\
	".\s_sound.h"\
	".\sounds.h"\
	".\st_stuff.h"\
	".\Tables.h"\
	".\v_palett.h"\
	".\v_text.h"\
	".\v_video.h"\
	".\w_wad.h"\
	".\z_zone.h"\
	
CPP_SWITCHES=/nologo /G5 /MT /W3 /GX /O2 /I "e:\ptc\source" /D "WIN32" /D\
 "NDEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\hu_stuff.obj" : $(SOURCE) $(DEP_CPP_HU_ST) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "doom - Win32 Debug"

DEP_CPP_HU_ST=\
	".\c_consol.h"\
	".\c_cvars.h"\
	".\c_dispch.h"\
	".\cmdlib.h"\
	".\d_event.h"\
	".\d_items.h"\
	".\d_net.h"\
	".\d_netinf.h"\
	".\d_player.h"\
	".\d_proto.h"\
	".\d_think.h"\
	".\d_ticcmd.h"\
	".\doomdata.h"\
	".\doomdef.h"\
	".\doomstat.h"\
	".\doomtype.h"\
	".\dstrings.h"\
	".\g_level.h"\
	".\hu_lib.h"\
	".\hu_stuff.h"\
	".\Info.h"\
	".\m_fixed.h"\
	".\m_swap.h"\
	".\p_mobj.h"\
	".\p_pspr.h"\
	".\r_data.h"\
	".\r_defs.h"\
	".\r_state.h"\
	".\s_sound.h"\
	".\sounds.h"\
	".\st_stuff.h"\
	".\Tables.h"\
	".\v_palett.h"\
	".\v_text.h"\
	".\v_video.h"\
	".\w_wad.h"\
	".\z_zone.h"\
	
CPP_SWITCHES=/nologo /G5 /MTd /W3 /Gm /GX /Zi /Od /I "e:\ptc\source" /D "WIN32"\
 /D "_DEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\hu_stuff.obj" : $(SOURCE) $(DEP_CPP_HU_ST) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=.\i_input.c

!IF  "$(CFG)" == "doom - Win32 Release"

DEP_CPP_I_INP=\
	".\c_cvars.h"\
	".\d_event.h"\
	".\d_items.h"\
	".\d_main.h"\
	".\d_net.h"\
	".\d_netinf.h"\
	".\d_player.h"\
	".\d_proto.h"\
	".\d_think.h"\
	".\d_ticcmd.h"\
	".\doomdata.h"\
	".\doomdef.h"\
	".\doomstat.h"\
	".\doomtype.h"\
	".\g_level.h"\
	".\i_input.h"\
	".\i_music.h"\
	".\i_system.h"\
	".\i_video.h"\
	".\Info.h"\
	".\m_argv.h"\
	".\m_fixed.h"\
	".\p_mobj.h"\
	".\p_pspr.h"\
	".\r_data.h"\
	".\r_defs.h"\
	".\r_state.h"\
	".\s_sound.h"\
	".\Tables.h"\
	".\v_palett.h"\
	".\v_video.h"\
	"e:\dxsdk\sdk\inc\dinput.h"\
	
CPP_SWITCHES=/nologo /G5 /MT /W3 /GX /O2 /I "e:\ptc\source" /D "WIN32" /D\
 "NDEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\i_input.obj" : $(SOURCE) $(DEP_CPP_I_INP) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "doom - Win32 Debug"

DEP_CPP_I_INP=\
	".\c_cvars.h"\
	".\d_event.h"\
	".\d_items.h"\
	".\d_main.h"\
	".\d_net.h"\
	".\d_netinf.h"\
	".\d_player.h"\
	".\d_proto.h"\
	".\d_think.h"\
	".\d_ticcmd.h"\
	".\doomdata.h"\
	".\doomdef.h"\
	".\doomstat.h"\
	".\doomtype.h"\
	".\g_level.h"\
	".\i_input.h"\
	".\i_music.h"\
	".\i_system.h"\
	".\i_video.h"\
	".\Info.h"\
	".\m_argv.h"\
	".\m_fixed.h"\
	".\p_mobj.h"\
	".\p_pspr.h"\
	".\r_data.h"\
	".\r_defs.h"\
	".\r_state.h"\
	".\s_sound.h"\
	".\Tables.h"\
	".\v_palett.h"\
	".\v_video.h"\
	"e:\dxsdk\sdk\inc\dinput.h"\
	
CPP_SWITCHES=/nologo /G5 /MTd /W3 /Gm /GX /Zi /Od /I "e:\ptc\source" /D "WIN32"\
 /D "_DEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\i_input.obj" : $(SOURCE) $(DEP_CPP_I_INP) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=.\i_main.c

!IF  "$(CFG)" == "doom - Win32 Release"

DEP_CPP_I_MAI=\
	".\c_consol.h"\
	".\cmdlib.h"\
	".\d_event.h"\
	".\d_main.h"\
	".\d_proto.h"\
	".\d_ticcmd.h"\
	".\doomdef.h"\
	".\doomtype.h"\
	".\i_system.h"\
	".\m_argv.h"\
	".\m_fixed.h"\
	
CPP_SWITCHES=/nologo /G5 /MT /W3 /GX /O2 /I "e:\ptc\source" /D "WIN32" /D\
 "NDEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\i_main.obj" : $(SOURCE) $(DEP_CPP_I_MAI) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "doom - Win32 Debug"

DEP_CPP_I_MAI=\
	".\c_consol.h"\
	".\cmdlib.h"\
	".\d_event.h"\
	".\d_main.h"\
	".\d_proto.h"\
	".\d_ticcmd.h"\
	".\doomdef.h"\
	".\doomtype.h"\
	".\i_system.h"\
	".\m_argv.h"\
	".\m_fixed.h"\
	
CPP_SWITCHES=/nologo /G5 /MTd /W3 /Gm /GX /Zi /Od /I "e:\ptc\source" /D "WIN32"\
 /D "_DEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\i_main.obj" : $(SOURCE) $(DEP_CPP_I_MAI) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=.\i_music.c

!IF  "$(CFG)" == "doom - Win32 Release"

DEP_CPP_I_MUS=\
	"..\midas\include\midasdll.h"\
	".\c_consol.h"\
	".\c_cvars.h"\
	".\cmdlib.h"\
	".\d_event.h"\
	".\d_items.h"\
	".\d_net.h"\
	".\d_netinf.h"\
	".\d_player.h"\
	".\d_proto.h"\
	".\d_think.h"\
	".\d_ticcmd.h"\
	".\doomdata.h"\
	".\doomdef.h"\
	".\doomstat.h"\
	".\doomtype.h"\
	".\g_level.h"\
	".\i_music.h"\
	".\Info.h"\
	".\m_argv.h"\
	".\m_fixed.h"\
	".\p_mobj.h"\
	".\p_pspr.h"\
	".\Tables.h"\
	".\w_wad.h"\
	
CPP_SWITCHES=/nologo /G5 /MT /W3 /GX /O2 /I "e:\ptc\source" /D "WIN32" /D\
 "NDEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\i_music.obj" : $(SOURCE) $(DEP_CPP_I_MUS) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "doom - Win32 Debug"

DEP_CPP_I_MUS=\
	"..\midas\include\midasdll.h"\
	".\c_consol.h"\
	".\c_cvars.h"\
	".\cmdlib.h"\
	".\d_event.h"\
	".\d_items.h"\
	".\d_net.h"\
	".\d_netinf.h"\
	".\d_player.h"\
	".\d_proto.h"\
	".\d_think.h"\
	".\d_ticcmd.h"\
	".\doomdata.h"\
	".\doomdef.h"\
	".\doomstat.h"\
	".\doomtype.h"\
	".\g_level.h"\
	".\i_music.h"\
	".\Info.h"\
	".\m_argv.h"\
	".\m_fixed.h"\
	".\p_mobj.h"\
	".\p_pspr.h"\
	".\Tables.h"\
	".\w_wad.h"\
	
CPP_SWITCHES=/nologo /G5 /MTd /W3 /Gm /GX /Zi /Od /I "e:\ptc\source" /D "WIN32"\
 /D "_DEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\i_music.obj" : $(SOURCE) $(DEP_CPP_I_MUS) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=.\i_net.c

!IF  "$(CFG)" == "doom - Win32 Release"

DEP_CPP_I_NET=\
	".\c_cvars.h"\
	".\d_event.h"\
	".\d_items.h"\
	".\d_net.h"\
	".\d_netinf.h"\
	".\d_player.h"\
	".\d_proto.h"\
	".\d_think.h"\
	".\d_ticcmd.h"\
	".\doomdata.h"\
	".\doomdef.h"\
	".\doomstat.h"\
	".\doomtype.h"\
	".\g_level.h"\
	".\i_net.h"\
	".\i_system.h"\
	".\Info.h"\
	".\m_alloc.h"\
	".\m_argv.h"\
	".\m_fixed.h"\
	".\m_swap.h"\
	".\p_mobj.h"\
	".\p_pspr.h"\
	".\Tables.h"\
	
CPP_SWITCHES=/nologo /G5 /MT /W3 /GX /O2 /I "e:\ptc\source" /D "WIN32" /D\
 "NDEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\i_net.obj" : $(SOURCE) $(DEP_CPP_I_NET) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "doom - Win32 Debug"

DEP_CPP_I_NET=\
	".\c_cvars.h"\
	".\d_event.h"\
	".\d_items.h"\
	".\d_net.h"\
	".\d_netinf.h"\
	".\d_player.h"\
	".\d_proto.h"\
	".\d_think.h"\
	".\d_ticcmd.h"\
	".\doomdata.h"\
	".\doomdef.h"\
	".\doomstat.h"\
	".\doomtype.h"\
	".\g_level.h"\
	".\i_net.h"\
	".\i_system.h"\
	".\Info.h"\
	".\m_alloc.h"\
	".\m_argv.h"\
	".\m_fixed.h"\
	".\m_swap.h"\
	".\p_mobj.h"\
	".\p_pspr.h"\
	".\Tables.h"\
	
CPP_SWITCHES=/nologo /G5 /MTd /W3 /Gm /GX /Zi /Od /I "e:\ptc\source" /D "WIN32"\
 /D "_DEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\i_net.obj" : $(SOURCE) $(DEP_CPP_I_NET) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=.\i_sound.c

!IF  "$(CFG)" == "doom - Win32 Release"

DEP_CPP_I_SOU=\
	"..\midas\include\midasdll.h"\
	"..\midas\src\midas\dma.h"\
	"..\midas\src\midas\errors.h"\
	"..\midas\src\midas\file.h"\
	"..\midas\src\midas\gmplayer.h"\
	"..\midas\src\midas\lang.h"\
	"..\midas\src\midas\madpcm.h"\
	"..\midas\src\midas\mglobals.h"\
	"..\midas\src\midas\midas.h"\
	"..\midas\src\midas\midasfx.h"\
	"..\midas\src\midas\midasstr.h"\
	"..\midas\src\midas\mmem.h"\
	"..\midas\src\midas\mpoll.h"\
	"..\midas\src\midas\mtypes.h"\
	"..\midas\src\midas\mutils.h"\
	"..\midas\src\midas\rawfile.h"\
	"..\midas\src\midas\sdevice.h"\
	"..\midas\src\midas\timer.h"\
	"..\midas\src\midas\waveread.h"\
	".\c_cvars.h"\
	".\d_event.h"\
	".\d_items.h"\
	".\d_net.h"\
	".\d_netinf.h"\
	".\d_player.h"\
	".\d_proto.h"\
	".\d_think.h"\
	".\d_ticcmd.h"\
	".\doomdata.h"\
	".\doomdef.h"\
	".\doomstat.h"\
	".\doomtype.h"\
	".\g_level.h"\
	".\i_music.h"\
	".\i_sound.h"\
	".\i_system.h"\
	".\i_video.h"\
	".\Info.h"\
	".\m_argv.h"\
	".\m_fixed.h"\
	".\m_misc.h"\
	".\p_mobj.h"\
	".\p_pspr.h"\
	".\r_data.h"\
	".\r_defs.h"\
	".\r_state.h"\
	".\s_sound.h"\
	".\sounds.h"\
	".\Tables.h"\
	".\v_palett.h"\
	".\v_video.h"\
	".\w_wad.h"\
	".\z_zone.h"\
	
CPP_SWITCHES=/nologo /G5 /MT /W3 /GX /O2 /I "e:\ptc\source" /D "WIN32" /D\
 "NDEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\i_sound.obj" : $(SOURCE) $(DEP_CPP_I_SOU) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "doom - Win32 Debug"

DEP_CPP_I_SOU=\
	"..\midas\include\midasdll.h"\
	"..\midas\src\midas\dma.h"\
	"..\midas\src\midas\errors.h"\
	"..\midas\src\midas\file.h"\
	"..\midas\src\midas\gmplayer.h"\
	"..\midas\src\midas\lang.h"\
	"..\midas\src\midas\madpcm.h"\
	"..\midas\src\midas\mglobals.h"\
	"..\midas\src\midas\midas.h"\
	"..\midas\src\midas\midasfx.h"\
	"..\midas\src\midas\midasstr.h"\
	"..\midas\src\midas\mmem.h"\
	"..\midas\src\midas\mpoll.h"\
	"..\midas\src\midas\mtypes.h"\
	"..\midas\src\midas\mutils.h"\
	"..\midas\src\midas\rawfile.h"\
	"..\midas\src\midas\sdevice.h"\
	"..\midas\src\midas\timer.h"\
	"..\midas\src\midas\waveread.h"\
	".\c_cvars.h"\
	".\d_event.h"\
	".\d_items.h"\
	".\d_net.h"\
	".\d_netinf.h"\
	".\d_player.h"\
	".\d_proto.h"\
	".\d_think.h"\
	".\d_ticcmd.h"\
	".\doomdata.h"\
	".\doomdef.h"\
	".\doomstat.h"\
	".\doomtype.h"\
	".\g_level.h"\
	".\i_music.h"\
	".\i_sound.h"\
	".\i_system.h"\
	".\i_video.h"\
	".\Info.h"\
	".\m_argv.h"\
	".\m_fixed.h"\
	".\m_misc.h"\
	".\p_mobj.h"\
	".\p_pspr.h"\
	".\r_data.h"\
	".\r_defs.h"\
	".\r_state.h"\
	".\s_sound.h"\
	".\sounds.h"\
	".\Tables.h"\
	".\v_palett.h"\
	".\v_video.h"\
	".\w_wad.h"\
	".\z_zone.h"\
	
CPP_SWITCHES=/nologo /G5 /MTd /W3 /Gm /GX /Zi /Od /I "e:\ptc\source" /D "WIN32"\
 /D "_DEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\i_sound.obj" : $(SOURCE) $(DEP_CPP_I_SOU) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=.\i_system.c

!IF  "$(CFG)" == "doom - Win32 Release"

DEP_CPP_I_SYS=\
	".\c_cvars.h"\
	".\d_event.h"\
	".\d_items.h"\
	".\d_net.h"\
	".\d_netinf.h"\
	".\d_player.h"\
	".\d_proto.h"\
	".\d_think.h"\
	".\d_ticcmd.h"\
	".\doomdata.h"\
	".\doomdef.h"\
	".\doomstat.h"\
	".\doomtype.h"\
	".\g_game.h"\
	".\g_level.h"\
	".\i_input.h"\
	".\i_music.h"\
	".\i_sound.h"\
	".\i_system.h"\
	".\i_video.h"\
	".\Info.h"\
	".\m_argv.h"\
	".\m_fixed.h"\
	".\m_misc.h"\
	".\p_mobj.h"\
	".\p_pspr.h"\
	".\r_data.h"\
	".\r_defs.h"\
	".\r_state.h"\
	".\sounds.h"\
	".\Tables.h"\
	".\v_palett.h"\
	".\v_video.h"\
	
CPP_SWITCHES=/nologo /G5 /MT /W3 /GX /O2 /I "e:\ptc\source" /D "WIN32" /D\
 "NDEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\i_system.obj" : $(SOURCE) $(DEP_CPP_I_SYS) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "doom - Win32 Debug"

DEP_CPP_I_SYS=\
	".\c_cvars.h"\
	".\d_event.h"\
	".\d_items.h"\
	".\d_net.h"\
	".\d_netinf.h"\
	".\d_player.h"\
	".\d_proto.h"\
	".\d_think.h"\
	".\d_ticcmd.h"\
	".\doomdata.h"\
	".\doomdef.h"\
	".\doomstat.h"\
	".\doomtype.h"\
	".\g_game.h"\
	".\g_level.h"\
	".\i_input.h"\
	".\i_music.h"\
	".\i_sound.h"\
	".\i_system.h"\
	".\i_video.h"\
	".\Info.h"\
	".\m_argv.h"\
	".\m_fixed.h"\
	".\m_misc.h"\
	".\p_mobj.h"\
	".\p_pspr.h"\
	".\r_data.h"\
	".\r_defs.h"\
	".\r_state.h"\
	".\sounds.h"\
	".\Tables.h"\
	".\v_palett.h"\
	".\v_video.h"\
	
CPP_SWITCHES=/nologo /G5 /MTd /W3 /Gm /GX /Zi /Od /I "e:\ptc\source" /D "WIN32"\
 /D "_DEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\i_system.obj" : $(SOURCE) $(DEP_CPP_I_SYS) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=.\i_video.cpp

!IF  "$(CFG)" == "doom - Win32 Release"

DEP_CPP_I_VID=\
	".\c_consol.h"\
	".\c_cvars.h"\
	".\c_dispch.h"\
	".\cmdlib.h"\
	".\d_event.h"\
	".\d_items.h"\
	".\d_main.h"\
	".\d_net.h"\
	".\d_netinf.h"\
	".\d_player.h"\
	".\d_proto.h"\
	".\d_think.h"\
	".\d_ticcmd.h"\
	".\doomdata.h"\
	".\doomdef.h"\
	".\doomstat.h"\
	".\doomtype.h"\
	".\g_level.h"\
	".\i_input.h"\
	".\i_system.h"\
	".\i_video.h"\
	".\Info.h"\
	".\m_alloc.h"\
	".\m_argv.h"\
	".\m_fixed.h"\
	".\p_mobj.h"\
	".\p_pspr.h"\
	".\r_data.h"\
	".\r_defs.h"\
	".\r_state.h"\
	".\st_stuff.h"\
	".\Tables.h"\
	".\v_palett.h"\
	".\v_text.h"\
	".\v_video.h"\
	".\z_zone.h"\
	"e:\ptc\source\classes.h"\
	"e:\ptc\source\clear.h"\
	"e:\ptc\source\clipper.h"\
	"e:\ptc\source\code.h"\
	"e:\ptc\source\color.h"\
	"e:\ptc\source\config.h"\
	"e:\ptc\source\effects.h"\
	"e:\ptc\source\file.h"\
	"e:\ptc\source\format.h"\
	"e:\ptc\source\globals.h"\
	"e:\ptc\source\idummy.h"\
	"e:\ptc\source\iface.h"\
	"e:\ptc\source\image.h"\
	"e:\ptc\source\isoft.h"\
	"e:\ptc\source\lang.h"\
	"e:\ptc\source\list.h"\
	"e:\ptc\source\loader.h"\
	"e:\ptc\source\misc.h"\
	"e:\ptc\source\native.h"\
	"e:\ptc\source\palette.h"\
	"e:\ptc\source\ptc.h"\
	"e:\ptc\source\raster.h"\
	"e:\ptc\source\rect.h"\
	"e:\ptc\source\surface.h"\
	"e:\ptc\source\tga.h"\
	
CPP_SWITCHES=/nologo /G5 /MT /W3 /GX /O2 /I "e:\ptc\source" /D "WIN32" /D\
 "NDEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\i_video.obj" : $(SOURCE) $(DEP_CPP_I_VID) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "doom - Win32 Debug"

DEP_CPP_I_VID=\
	".\c_consol.h"\
	".\c_cvars.h"\
	".\c_dispch.h"\
	".\cmdlib.h"\
	".\d_event.h"\
	".\d_items.h"\
	".\d_main.h"\
	".\d_net.h"\
	".\d_netinf.h"\
	".\d_player.h"\
	".\d_proto.h"\
	".\d_think.h"\
	".\d_ticcmd.h"\
	".\doomdata.h"\
	".\doomdef.h"\
	".\doomstat.h"\
	".\doomtype.h"\
	".\g_level.h"\
	".\i_input.h"\
	".\i_system.h"\
	".\i_video.h"\
	".\Info.h"\
	".\m_alloc.h"\
	".\m_argv.h"\
	".\m_fixed.h"\
	".\p_mobj.h"\
	".\p_pspr.h"\
	".\r_data.h"\
	".\r_defs.h"\
	".\r_state.h"\
	".\st_stuff.h"\
	".\Tables.h"\
	".\v_palett.h"\
	".\v_text.h"\
	".\v_video.h"\
	".\z_zone.h"\
	"e:\ptc\source\classes.h"\
	"e:\ptc\source\clear.h"\
	"e:\ptc\source\clipper.h"\
	"e:\ptc\source\code.h"\
	"e:\ptc\source\color.h"\
	"e:\ptc\source\config.h"\
	"e:\ptc\source\effects.h"\
	"e:\ptc\source\file.h"\
	"e:\ptc\source\format.h"\
	"e:\ptc\source\globals.h"\
	"e:\ptc\source\idummy.h"\
	"e:\ptc\source\iface.h"\
	"e:\ptc\source\image.h"\
	"e:\ptc\source\isoft.h"\
	"e:\ptc\source\lang.h"\
	"e:\ptc\source\list.h"\
	"e:\ptc\source\loader.h"\
	"e:\ptc\source\misc.h"\
	"e:\ptc\source\native.h"\
	"e:\ptc\source\palette.h"\
	"e:\ptc\source\ptc.h"\
	"e:\ptc\source\raster.h"\
	"e:\ptc\source\rect.h"\
	"e:\ptc\source\surface.h"\
	"e:\ptc\source\tga.h"\
	
CPP_SWITCHES=/nologo /G5 /MTd /W3 /Gm /GX /Zi /Od /I "e:\ptc\source" /D "WIN32"\
 /D "_DEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\i_video.obj" : $(SOURCE) $(DEP_CPP_I_VID) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=.\info.c

!IF  "$(CFG)" == "doom - Win32 Release"

DEP_CPP_INFO_=\
	".\d_think.h"\
	".\doomdata.h"\
	".\doomdef.h"\
	".\doomtype.h"\
	".\Info.h"\
	".\m_fixed.h"\
	".\p_mobj.h"\
	".\sounds.h"\
	".\Tables.h"\
	
CPP_SWITCHES=/nologo /G5 /MT /W3 /GX /O2 /I "e:\ptc\source" /D "WIN32" /D\
 "NDEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\info.obj" : $(SOURCE) $(DEP_CPP_INFO_) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "doom - Win32 Debug"

DEP_CPP_INFO_=\
	".\d_think.h"\
	".\doomdata.h"\
	".\doomdef.h"\
	".\doomtype.h"\
	".\Info.h"\
	".\m_fixed.h"\
	".\p_mobj.h"\
	".\sounds.h"\
	".\Tables.h"\
	
CPP_SWITCHES=/nologo /G5 /MTd /W3 /Gm /GX /Zi /Od /I "e:\ptc\source" /D "WIN32"\
 /D "_DEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\info.obj" : $(SOURCE) $(DEP_CPP_INFO_) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=.\m_alloc.c

!IF  "$(CFG)" == "doom - Win32 Release"

DEP_CPP_M_ALL=\
	".\d_event.h"\
	".\d_proto.h"\
	".\d_ticcmd.h"\
	".\doomdef.h"\
	".\doomtype.h"\
	".\i_system.h"\
	".\m_alloc.h"\
	".\m_fixed.h"\
	
CPP_SWITCHES=/nologo /G5 /MT /W3 /GX /O2 /I "e:\ptc\source" /D "WIN32" /D\
 "NDEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\m_alloc.obj" : $(SOURCE) $(DEP_CPP_M_ALL) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "doom - Win32 Debug"

DEP_CPP_M_ALL=\
	".\d_event.h"\
	".\d_proto.h"\
	".\d_ticcmd.h"\
	".\doomdef.h"\
	".\doomtype.h"\
	".\i_system.h"\
	".\m_alloc.h"\
	".\m_fixed.h"\
	
CPP_SWITCHES=/nologo /G5 /MTd /W3 /Gm /GX /Zi /Od /I "e:\ptc\source" /D "WIN32"\
 /D "_DEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\m_alloc.obj" : $(SOURCE) $(DEP_CPP_M_ALL) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=.\m_argv.c

!IF  "$(CFG)" == "doom - Win32 Release"

CPP_SWITCHES=/nologo /G5 /MT /W3 /GX /O2 /I "e:\ptc\source" /D "WIN32" /D\
 "NDEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\m_argv.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "doom - Win32 Debug"

CPP_SWITCHES=/nologo /G5 /MTd /W3 /Gm /GX /Zi /Od /I "e:\ptc\source" /D "WIN32"\
 /D "_DEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\m_argv.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=.\m_bbox.c

!IF  "$(CFG)" == "doom - Win32 Release"

DEP_CPP_M_BBO=\
	".\doomtype.h"\
	".\m_bbox.h"\
	".\m_fixed.h"\
	
CPP_SWITCHES=/nologo /G5 /MT /W3 /GX /O2 /I "e:\ptc\source" /D "WIN32" /D\
 "NDEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\m_bbox.obj" : $(SOURCE) $(DEP_CPP_M_BBO) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "doom - Win32 Debug"

DEP_CPP_M_BBO=\
	".\doomtype.h"\
	".\m_bbox.h"\
	".\m_fixed.h"\
	
CPP_SWITCHES=/nologo /G5 /MTd /W3 /Gm /GX /Zi /Od /I "e:\ptc\source" /D "WIN32"\
 /D "_DEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\m_bbox.obj" : $(SOURCE) $(DEP_CPP_M_BBO) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=.\m_cheat.c

!IF  "$(CFG)" == "doom - Win32 Release"

DEP_CPP_M_CHE=\
	".\c_cvars.h"\
	".\d_items.h"\
	".\d_net.h"\
	".\d_netinf.h"\
	".\d_player.h"\
	".\d_proto.h"\
	".\d_think.h"\
	".\d_ticcmd.h"\
	".\doomdata.h"\
	".\doomdef.h"\
	".\doomstat.h"\
	".\doomtype.h"\
	".\dstrings.h"\
	".\g_level.h"\
	".\Info.h"\
	".\m_cheat.h"\
	".\m_fixed.h"\
	".\p_inter.h"\
	".\p_local.h"\
	".\p_mobj.h"\
	".\p_pspr.h"\
	".\p_spec.h"\
	".\r_bsp.h"\
	".\r_data.h"\
	".\r_defs.h"\
	".\r_draw.h"\
	".\r_local.h"\
	".\r_main.h"\
	".\r_plane.h"\
	".\r_segs.h"\
	".\r_state.h"\
	".\r_things.h"\
	".\Tables.h"\
	
CPP_SWITCHES=/nologo /G5 /MT /W3 /GX /O2 /I "e:\ptc\source" /D "WIN32" /D\
 "NDEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\m_cheat.obj" : $(SOURCE) $(DEP_CPP_M_CHE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "doom - Win32 Debug"

DEP_CPP_M_CHE=\
	".\c_cvars.h"\
	".\d_items.h"\
	".\d_net.h"\
	".\d_netinf.h"\
	".\d_player.h"\
	".\d_proto.h"\
	".\d_think.h"\
	".\d_ticcmd.h"\
	".\doomdata.h"\
	".\doomdef.h"\
	".\doomstat.h"\
	".\doomtype.h"\
	".\dstrings.h"\
	".\g_level.h"\
	".\Info.h"\
	".\m_cheat.h"\
	".\m_fixed.h"\
	".\p_inter.h"\
	".\p_local.h"\
	".\p_mobj.h"\
	".\p_pspr.h"\
	".\p_spec.h"\
	".\r_bsp.h"\
	".\r_data.h"\
	".\r_defs.h"\
	".\r_draw.h"\
	".\r_local.h"\
	".\r_main.h"\
	".\r_plane.h"\
	".\r_segs.h"\
	".\r_state.h"\
	".\r_things.h"\
	".\Tables.h"\
	
CPP_SWITCHES=/nologo /G5 /MTd /W3 /Gm /GX /Zi /Od /I "e:\ptc\source" /D "WIN32"\
 /D "_DEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\m_cheat.obj" : $(SOURCE) $(DEP_CPP_M_CHE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=.\m_fixed.c

!IF  "$(CFG)" == "doom - Win32 Release"

DEP_CPP_M_FIX=\
	".\d_event.h"\
	".\d_proto.h"\
	".\d_ticcmd.h"\
	".\doomdef.h"\
	".\doomtype.h"\
	".\i_system.h"\
	".\m_fixed.h"\
	
CPP_SWITCHES=/nologo /G5 /MT /W3 /GX /O2 /I "e:\ptc\source" /D "WIN32" /D\
 "NDEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\m_fixed.obj" : $(SOURCE) $(DEP_CPP_M_FIX) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "doom - Win32 Debug"

DEP_CPP_M_FIX=\
	".\d_event.h"\
	".\d_proto.h"\
	".\d_ticcmd.h"\
	".\doomdef.h"\
	".\doomtype.h"\
	".\i_system.h"\
	".\m_fixed.h"\
	
CPP_SWITCHES=/nologo /G5 /MTd /W3 /Gm /GX /Zi /Od /I "e:\ptc\source" /D "WIN32"\
 /D "_DEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\m_fixed.obj" : $(SOURCE) $(DEP_CPP_M_FIX) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=.\m_menu.c

!IF  "$(CFG)" == "doom - Win32 Release"

DEP_CPP_M_MEN=\
	".\c_consol.h"\
	".\c_cvars.h"\
	".\c_dispch.h"\
	".\cmdlib.h"\
	".\d_event.h"\
	".\d_items.h"\
	".\d_main.h"\
	".\d_net.h"\
	".\d_netinf.h"\
	".\d_player.h"\
	".\d_proto.h"\
	".\d_think.h"\
	".\d_ticcmd.h"\
	".\doomdata.h"\
	".\doomdef.h"\
	".\doomstat.h"\
	".\doomtype.h"\
	".\dstrings.h"\
	".\g_game.h"\
	".\g_level.h"\
	".\hu_stuff.h"\
	".\i_input.h"\
	".\i_system.h"\
	".\i_video.h"\
	".\Info.h"\
	".\m_argv.h"\
	".\m_fixed.h"\
	".\m_menu.h"\
	".\m_swap.h"\
	".\p_mobj.h"\
	".\p_pspr.h"\
	".\r_bsp.h"\
	".\r_data.h"\
	".\r_defs.h"\
	".\r_draw.h"\
	".\r_local.h"\
	".\r_main.h"\
	".\r_plane.h"\
	".\r_segs.h"\
	".\r_state.h"\
	".\r_things.h"\
	".\s_sound.h"\
	".\sounds.h"\
	".\Tables.h"\
	".\v_palett.h"\
	".\v_text.h"\
	".\v_video.h"\
	".\w_wad.h"\
	".\z_zone.h"\
	
CPP_SWITCHES=/nologo /G5 /MT /W3 /GX /O2 /I "e:\ptc\source" /D "WIN32" /D\
 "NDEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\m_menu.obj" : $(SOURCE) $(DEP_CPP_M_MEN) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "doom - Win32 Debug"

DEP_CPP_M_MEN=\
	".\c_consol.h"\
	".\c_cvars.h"\
	".\c_dispch.h"\
	".\cmdlib.h"\
	".\d_event.h"\
	".\d_items.h"\
	".\d_main.h"\
	".\d_net.h"\
	".\d_netinf.h"\
	".\d_player.h"\
	".\d_proto.h"\
	".\d_think.h"\
	".\d_ticcmd.h"\
	".\doomdata.h"\
	".\doomdef.h"\
	".\doomstat.h"\
	".\doomtype.h"\
	".\dstrings.h"\
	".\g_game.h"\
	".\g_level.h"\
	".\hu_stuff.h"\
	".\i_input.h"\
	".\i_system.h"\
	".\i_video.h"\
	".\Info.h"\
	".\m_argv.h"\
	".\m_fixed.h"\
	".\m_menu.h"\
	".\m_swap.h"\
	".\p_mobj.h"\
	".\p_pspr.h"\
	".\r_bsp.h"\
	".\r_data.h"\
	".\r_defs.h"\
	".\r_draw.h"\
	".\r_local.h"\
	".\r_main.h"\
	".\r_plane.h"\
	".\r_segs.h"\
	".\r_state.h"\
	".\r_things.h"\
	".\s_sound.h"\
	".\sounds.h"\
	".\Tables.h"\
	".\v_palett.h"\
	".\v_text.h"\
	".\v_video.h"\
	".\w_wad.h"\
	".\z_zone.h"\
	
CPP_SWITCHES=/nologo /G5 /MTd /W3 /Gm /GX /Zi /Od /I "e:\ptc\source" /D "WIN32"\
 /D "_DEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\m_menu.obj" : $(SOURCE) $(DEP_CPP_M_MEN) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=.\m_misc.c

!IF  "$(CFG)" == "doom - Win32 Release"

DEP_CPP_M_MIS=\
	".\c_bind.h"\
	".\c_cvars.h"\
	".\c_dispch.h"\
	".\cmdlib.h"\
	".\d_event.h"\
	".\d_items.h"\
	".\d_net.h"\
	".\d_netinf.h"\
	".\d_player.h"\
	".\d_proto.h"\
	".\d_think.h"\
	".\d_ticcmd.h"\
	".\doomdata.h"\
	".\doomdef.h"\
	".\doomstat.h"\
	".\doomtype.h"\
	".\dstrings.h"\
	".\g_game.h"\
	".\g_level.h"\
	".\hu_stuff.h"\
	".\i_system.h"\
	".\i_video.h"\
	".\Info.h"\
	".\m_alloc.h"\
	".\m_argv.h"\
	".\m_fixed.h"\
	".\m_misc.h"\
	".\m_swap.h"\
	".\p_mobj.h"\
	".\p_pspr.h"\
	".\r_data.h"\
	".\r_defs.h"\
	".\r_state.h"\
	".\Tables.h"\
	".\v_palett.h"\
	".\v_video.h"\
	".\w_wad.h"\
	".\z_zone.h"\
	
CPP_SWITCHES=/nologo /G5 /MT /W3 /GX /O2 /I "e:\ptc\source" /D "WIN32" /D\
 "NDEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\m_misc.obj" : $(SOURCE) $(DEP_CPP_M_MIS) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "doom - Win32 Debug"

DEP_CPP_M_MIS=\
	".\c_bind.h"\
	".\c_cvars.h"\
	".\c_dispch.h"\
	".\cmdlib.h"\
	".\d_event.h"\
	".\d_items.h"\
	".\d_net.h"\
	".\d_netinf.h"\
	".\d_player.h"\
	".\d_proto.h"\
	".\d_think.h"\
	".\d_ticcmd.h"\
	".\doomdata.h"\
	".\doomdef.h"\
	".\doomstat.h"\
	".\doomtype.h"\
	".\dstrings.h"\
	".\g_game.h"\
	".\g_level.h"\
	".\hu_stuff.h"\
	".\i_system.h"\
	".\i_video.h"\
	".\Info.h"\
	".\m_alloc.h"\
	".\m_argv.h"\
	".\m_fixed.h"\
	".\m_misc.h"\
	".\m_swap.h"\
	".\p_mobj.h"\
	".\p_pspr.h"\
	".\r_data.h"\
	".\r_defs.h"\
	".\r_state.h"\
	".\Tables.h"\
	".\v_palett.h"\
	".\v_video.h"\
	".\w_wad.h"\
	".\z_zone.h"\
	
CPP_SWITCHES=/nologo /G5 /MTd /W3 /Gm /GX /Zi /Od /I "e:\ptc\source" /D "WIN32"\
 /D "_DEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\m_misc.obj" : $(SOURCE) $(DEP_CPP_M_MIS) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=.\m_option.c

!IF  "$(CFG)" == "doom - Win32 Release"

DEP_CPP_M_OPT=\
	".\c_bind.h"\
	".\c_consol.h"\
	".\c_cvars.h"\
	".\c_dispch.h"\
	".\cmdlib.h"\
	".\d_event.h"\
	".\d_items.h"\
	".\d_main.h"\
	".\d_net.h"\
	".\d_netinf.h"\
	".\d_player.h"\
	".\d_proto.h"\
	".\d_think.h"\
	".\d_ticcmd.h"\
	".\doomdata.h"\
	".\doomdef.h"\
	".\doomstat.h"\
	".\doomtype.h"\
	".\dstrings.h"\
	".\g_game.h"\
	".\g_level.h"\
	".\hu_stuff.h"\
	".\i_system.h"\
	".\i_video.h"\
	".\Info.h"\
	".\m_alloc.h"\
	".\m_argv.h"\
	".\m_fixed.h"\
	".\m_menu.h"\
	".\m_misc.h"\
	".\m_swap.h"\
	".\p_mobj.h"\
	".\p_pspr.h"\
	".\r_bsp.h"\
	".\r_data.h"\
	".\r_defs.h"\
	".\r_draw.h"\
	".\r_local.h"\
	".\r_main.h"\
	".\r_plane.h"\
	".\r_segs.h"\
	".\r_state.h"\
	".\r_things.h"\
	".\s_sound.h"\
	".\sounds.h"\
	".\Tables.h"\
	".\v_palett.h"\
	".\v_text.h"\
	".\v_video.h"\
	".\w_wad.h"\
	".\z_zone.h"\
	

"$(INTDIR)\m_option.obj" : $(SOURCE) $(DEP_CPP_M_OPT) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "doom - Win32 Debug"

DEP_CPP_M_OPT=\
	".\c_bind.h"\
	".\c_consol.h"\
	".\c_cvars.h"\
	".\c_dispch.h"\
	".\cmdlib.h"\
	".\d_event.h"\
	".\d_items.h"\
	".\d_main.h"\
	".\d_net.h"\
	".\d_netinf.h"\
	".\d_player.h"\
	".\d_proto.h"\
	".\d_think.h"\
	".\d_ticcmd.h"\
	".\doomdata.h"\
	".\doomdef.h"\
	".\doomstat.h"\
	".\doomtype.h"\
	".\dstrings.h"\
	".\g_game.h"\
	".\g_level.h"\
	".\hu_stuff.h"\
	".\i_system.h"\
	".\i_video.h"\
	".\Info.h"\
	".\m_alloc.h"\
	".\m_argv.h"\
	".\m_fixed.h"\
	".\m_menu.h"\
	".\m_misc.h"\
	".\m_swap.h"\
	".\p_mobj.h"\
	".\p_pspr.h"\
	".\r_bsp.h"\
	".\r_data.h"\
	".\r_defs.h"\
	".\r_draw.h"\
	".\r_local.h"\
	".\r_main.h"\
	".\r_plane.h"\
	".\r_segs.h"\
	".\r_state.h"\
	".\r_things.h"\
	".\s_sound.h"\
	".\sounds.h"\
	".\Tables.h"\
	".\v_palett.h"\
	".\v_text.h"\
	".\v_video.h"\
	".\w_wad.h"\
	".\z_zone.h"\
	

"$(INTDIR)\m_option.obj" : $(SOURCE) $(DEP_CPP_M_OPT) "$(INTDIR)"


!ENDIF 

SOURCE=.\m_random.c

!IF  "$(CFG)" == "doom - Win32 Release"

DEP_CPP_M_RAN=\
	".\c_cvars.h"\
	".\d_items.h"\
	".\d_net.h"\
	".\d_netinf.h"\
	".\d_player.h"\
	".\d_proto.h"\
	".\d_think.h"\
	".\d_ticcmd.h"\
	".\doomdata.h"\
	".\doomdef.h"\
	".\doomstat.h"\
	".\doomtype.h"\
	".\g_level.h"\
	".\Info.h"\
	".\m_fixed.h"\
	".\m_random.h"\
	".\p_mobj.h"\
	".\p_pspr.h"\
	".\Tables.h"\
	
CPP_SWITCHES=/nologo /G5 /MT /W3 /GX /O2 /I "e:\ptc\source" /D "WIN32" /D\
 "NDEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\m_random.obj" : $(SOURCE) $(DEP_CPP_M_RAN) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "doom - Win32 Debug"

DEP_CPP_M_RAN=\
	".\c_cvars.h"\
	".\d_items.h"\
	".\d_net.h"\
	".\d_netinf.h"\
	".\d_player.h"\
	".\d_proto.h"\
	".\d_think.h"\
	".\d_ticcmd.h"\
	".\doomdata.h"\
	".\doomdef.h"\
	".\doomstat.h"\
	".\doomtype.h"\
	".\g_level.h"\
	".\Info.h"\
	".\m_fixed.h"\
	".\m_random.h"\
	".\p_mobj.h"\
	".\p_pspr.h"\
	".\Tables.h"\
	
CPP_SWITCHES=/nologo /G5 /MTd /W3 /Gm /GX /Zi /Od /I "e:\ptc\source" /D "WIN32"\
 /D "_DEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\m_random.obj" : $(SOURCE) $(DEP_CPP_M_RAN) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=.\m_swap.c
DEP_CPP_M_SWA=\
	".\m_swap.h"\
	

!IF  "$(CFG)" == "doom - Win32 Release"

CPP_SWITCHES=/nologo /G5 /MT /W3 /GX /O2 /I "e:\ptc\source" /D "WIN32" /D\
 "NDEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\m_swap.obj" : $(SOURCE) $(DEP_CPP_M_SWA) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "doom - Win32 Debug"

CPP_SWITCHES=/nologo /G5 /MTd /W3 /Gm /GX /Zi /Od /I "e:\ptc\source" /D "WIN32"\
 /D "_DEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\m_swap.obj" : $(SOURCE) $(DEP_CPP_M_SWA) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=.\p_ceilng.c

!IF  "$(CFG)" == "doom - Win32 Release"

DEP_CPP_P_CEI=\
	".\c_cvars.h"\
	".\d_items.h"\
	".\d_net.h"\
	".\d_netinf.h"\
	".\d_player.h"\
	".\d_proto.h"\
	".\d_think.h"\
	".\d_ticcmd.h"\
	".\doomdata.h"\
	".\doomdef.h"\
	".\doomstat.h"\
	".\doomtype.h"\
	".\g_level.h"\
	".\Info.h"\
	".\m_alloc.h"\
	".\m_fixed.h"\
	".\p_local.h"\
	".\p_mobj.h"\
	".\p_pspr.h"\
	".\p_spec.h"\
	".\r_bsp.h"\
	".\r_data.h"\
	".\r_defs.h"\
	".\r_draw.h"\
	".\r_local.h"\
	".\r_main.h"\
	".\r_plane.h"\
	".\r_segs.h"\
	".\r_state.h"\
	".\r_things.h"\
	".\s_sound.h"\
	".\sounds.h"\
	".\Tables.h"\
	".\z_zone.h"\
	
CPP_SWITCHES=/nologo /G5 /MT /W3 /GX /O2 /I "e:\ptc\source" /D "WIN32" /D\
 "NDEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\p_ceilng.obj" : $(SOURCE) $(DEP_CPP_P_CEI) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "doom - Win32 Debug"

DEP_CPP_P_CEI=\
	".\c_cvars.h"\
	".\d_items.h"\
	".\d_net.h"\
	".\d_netinf.h"\
	".\d_player.h"\
	".\d_proto.h"\
	".\d_think.h"\
	".\d_ticcmd.h"\
	".\doomdata.h"\
	".\doomdef.h"\
	".\doomstat.h"\
	".\doomtype.h"\
	".\g_level.h"\
	".\Info.h"\
	".\m_alloc.h"\
	".\m_fixed.h"\
	".\p_local.h"\
	".\p_mobj.h"\
	".\p_pspr.h"\
	".\p_spec.h"\
	".\r_bsp.h"\
	".\r_data.h"\
	".\r_defs.h"\
	".\r_draw.h"\
	".\r_local.h"\
	".\r_main.h"\
	".\r_plane.h"\
	".\r_segs.h"\
	".\r_state.h"\
	".\r_things.h"\
	".\s_sound.h"\
	".\sounds.h"\
	".\Tables.h"\
	".\z_zone.h"\
	
CPP_SWITCHES=/nologo /G5 /MTd /W3 /Gm /GX /Zi /Od /I "e:\ptc\source" /D "WIN32"\
 /D "_DEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\p_ceilng.obj" : $(SOURCE) $(DEP_CPP_P_CEI) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=.\p_doors.c

!IF  "$(CFG)" == "doom - Win32 Release"

DEP_CPP_P_DOO=\
	".\c_consol.h"\
	".\c_cvars.h"\
	".\cmdlib.h"\
	".\d_event.h"\
	".\d_items.h"\
	".\d_net.h"\
	".\d_netinf.h"\
	".\d_player.h"\
	".\d_proto.h"\
	".\d_think.h"\
	".\d_ticcmd.h"\
	".\doomdata.h"\
	".\doomdef.h"\
	".\doomstat.h"\
	".\doomtype.h"\
	".\dstrings.h"\
	".\g_level.h"\
	".\Info.h"\
	".\m_fixed.h"\
	".\p_local.h"\
	".\p_mobj.h"\
	".\p_pspr.h"\
	".\p_spec.h"\
	".\r_bsp.h"\
	".\r_data.h"\
	".\r_defs.h"\
	".\r_draw.h"\
	".\r_local.h"\
	".\r_main.h"\
	".\r_plane.h"\
	".\r_segs.h"\
	".\r_state.h"\
	".\r_things.h"\
	".\s_sound.h"\
	".\sounds.h"\
	".\Tables.h"\
	".\z_zone.h"\
	
CPP_SWITCHES=/nologo /G5 /MT /W3 /GX /O2 /I "e:\ptc\source" /D "WIN32" /D\
 "NDEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\p_doors.obj" : $(SOURCE) $(DEP_CPP_P_DOO) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "doom - Win32 Debug"

DEP_CPP_P_DOO=\
	".\c_consol.h"\
	".\c_cvars.h"\
	".\cmdlib.h"\
	".\d_event.h"\
	".\d_items.h"\
	".\d_net.h"\
	".\d_netinf.h"\
	".\d_player.h"\
	".\d_proto.h"\
	".\d_think.h"\
	".\d_ticcmd.h"\
	".\doomdata.h"\
	".\doomdef.h"\
	".\doomstat.h"\
	".\doomtype.h"\
	".\dstrings.h"\
	".\g_level.h"\
	".\Info.h"\
	".\m_fixed.h"\
	".\p_local.h"\
	".\p_mobj.h"\
	".\p_pspr.h"\
	".\p_spec.h"\
	".\r_bsp.h"\
	".\r_data.h"\
	".\r_defs.h"\
	".\r_draw.h"\
	".\r_local.h"\
	".\r_main.h"\
	".\r_plane.h"\
	".\r_segs.h"\
	".\r_state.h"\
	".\r_things.h"\
	".\s_sound.h"\
	".\sounds.h"\
	".\Tables.h"\
	".\z_zone.h"\
	
CPP_SWITCHES=/nologo /G5 /MTd /W3 /Gm /GX /Zi /Od /I "e:\ptc\source" /D "WIN32"\
 /D "_DEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\p_doors.obj" : $(SOURCE) $(DEP_CPP_P_DOO) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=.\p_enemy.c

!IF  "$(CFG)" == "doom - Win32 Release"

DEP_CPP_P_ENE=\
	".\c_cvars.h"\
	".\d_event.h"\
	".\d_items.h"\
	".\d_net.h"\
	".\d_netinf.h"\
	".\d_player.h"\
	".\d_proto.h"\
	".\d_think.h"\
	".\d_ticcmd.h"\
	".\doomdata.h"\
	".\doomdef.h"\
	".\doomstat.h"\
	".\doomtype.h"\
	".\g_game.h"\
	".\g_level.h"\
	".\i_system.h"\
	".\Info.h"\
	".\m_fixed.h"\
	".\m_random.h"\
	".\p_local.h"\
	".\p_mobj.h"\
	".\p_pspr.h"\
	".\p_spec.h"\
	".\r_bsp.h"\
	".\r_data.h"\
	".\r_defs.h"\
	".\r_draw.h"\
	".\r_local.h"\
	".\r_main.h"\
	".\r_plane.h"\
	".\r_segs.h"\
	".\r_state.h"\
	".\r_things.h"\
	".\s_sound.h"\
	".\sounds.h"\
	".\Tables.h"\
	
CPP_SWITCHES=/nologo /G5 /MT /W3 /GX /O2 /I "e:\ptc\source" /D "WIN32" /D\
 "NDEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\p_enemy.obj" : $(SOURCE) $(DEP_CPP_P_ENE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "doom - Win32 Debug"

DEP_CPP_P_ENE=\
	".\c_cvars.h"\
	".\d_event.h"\
	".\d_items.h"\
	".\d_net.h"\
	".\d_netinf.h"\
	".\d_player.h"\
	".\d_proto.h"\
	".\d_think.h"\
	".\d_ticcmd.h"\
	".\doomdata.h"\
	".\doomdef.h"\
	".\doomstat.h"\
	".\doomtype.h"\
	".\g_game.h"\
	".\g_level.h"\
	".\i_system.h"\
	".\Info.h"\
	".\m_fixed.h"\
	".\m_random.h"\
	".\p_local.h"\
	".\p_mobj.h"\
	".\p_pspr.h"\
	".\p_spec.h"\
	".\r_bsp.h"\
	".\r_data.h"\
	".\r_defs.h"\
	".\r_draw.h"\
	".\r_local.h"\
	".\r_main.h"\
	".\r_plane.h"\
	".\r_segs.h"\
	".\r_state.h"\
	".\r_things.h"\
	".\s_sound.h"\
	".\sounds.h"\
	".\Tables.h"\
	
CPP_SWITCHES=/nologo /G5 /MTd /W3 /Gm /GX /Zi /Od /I "e:\ptc\source" /D "WIN32"\
 /D "_DEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\p_enemy.obj" : $(SOURCE) $(DEP_CPP_P_ENE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=.\p_floor.c

!IF  "$(CFG)" == "doom - Win32 Release"

DEP_CPP_P_FLO=\
	".\c_cvars.h"\
	".\d_items.h"\
	".\d_net.h"\
	".\d_netinf.h"\
	".\d_player.h"\
	".\d_proto.h"\
	".\d_think.h"\
	".\d_ticcmd.h"\
	".\doomdata.h"\
	".\doomdef.h"\
	".\doomstat.h"\
	".\doomtype.h"\
	".\g_level.h"\
	".\Info.h"\
	".\m_fixed.h"\
	".\p_local.h"\
	".\p_mobj.h"\
	".\p_pspr.h"\
	".\p_spec.h"\
	".\r_bsp.h"\
	".\r_data.h"\
	".\r_defs.h"\
	".\r_draw.h"\
	".\r_local.h"\
	".\r_main.h"\
	".\r_plane.h"\
	".\r_segs.h"\
	".\r_state.h"\
	".\r_things.h"\
	".\s_sound.h"\
	".\sounds.h"\
	".\Tables.h"\
	".\z_zone.h"\
	
CPP_SWITCHES=/nologo /G5 /MT /W3 /GX /O2 /I "e:\ptc\source" /D "WIN32" /D\
 "NDEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\p_floor.obj" : $(SOURCE) $(DEP_CPP_P_FLO) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "doom - Win32 Debug"

DEP_CPP_P_FLO=\
	".\c_cvars.h"\
	".\d_items.h"\
	".\d_net.h"\
	".\d_netinf.h"\
	".\d_player.h"\
	".\d_proto.h"\
	".\d_think.h"\
	".\d_ticcmd.h"\
	".\doomdata.h"\
	".\doomdef.h"\
	".\doomstat.h"\
	".\doomtype.h"\
	".\g_level.h"\
	".\Info.h"\
	".\m_fixed.h"\
	".\p_local.h"\
	".\p_mobj.h"\
	".\p_pspr.h"\
	".\p_spec.h"\
	".\r_bsp.h"\
	".\r_data.h"\
	".\r_defs.h"\
	".\r_draw.h"\
	".\r_local.h"\
	".\r_main.h"\
	".\r_plane.h"\
	".\r_segs.h"\
	".\r_state.h"\
	".\r_things.h"\
	".\s_sound.h"\
	".\sounds.h"\
	".\Tables.h"\
	".\z_zone.h"\
	
CPP_SWITCHES=/nologo /G5 /MTd /W3 /Gm /GX /Zi /Od /I "e:\ptc\source" /D "WIN32"\
 /D "_DEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\p_floor.obj" : $(SOURCE) $(DEP_CPP_P_FLO) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=.\p_inter.c

!IF  "$(CFG)" == "doom - Win32 Release"

DEP_CPP_P_INT=\
	".\am_map.h"\
	".\c_consol.h"\
	".\c_cvars.h"\
	".\cmdlib.h"\
	".\d_event.h"\
	".\d_items.h"\
	".\d_net.h"\
	".\d_netinf.h"\
	".\d_player.h"\
	".\d_proto.h"\
	".\d_think.h"\
	".\d_ticcmd.h"\
	".\doomdata.h"\
	".\doomdef.h"\
	".\doomstat.h"\
	".\doomtype.h"\
	".\dstrings.h"\
	".\g_level.h"\
	".\i_system.h"\
	".\Info.h"\
	".\m_fixed.h"\
	".\m_random.h"\
	".\p_inter.h"\
	".\p_local.h"\
	".\p_mobj.h"\
	".\p_pspr.h"\
	".\p_spec.h"\
	".\r_bsp.h"\
	".\r_data.h"\
	".\r_defs.h"\
	".\r_draw.h"\
	".\r_local.h"\
	".\r_main.h"\
	".\r_plane.h"\
	".\r_segs.h"\
	".\r_state.h"\
	".\r_things.h"\
	".\s_sound.h"\
	".\sounds.h"\
	".\Tables.h"\
	
CPP_SWITCHES=/nologo /G5 /MT /W3 /GX /O2 /I "e:\ptc\source" /D "WIN32" /D\
 "NDEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\p_inter.obj" : $(SOURCE) $(DEP_CPP_P_INT) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "doom - Win32 Debug"

DEP_CPP_P_INT=\
	".\am_map.h"\
	".\c_consol.h"\
	".\c_cvars.h"\
	".\cmdlib.h"\
	".\d_event.h"\
	".\d_items.h"\
	".\d_net.h"\
	".\d_netinf.h"\
	".\d_player.h"\
	".\d_proto.h"\
	".\d_think.h"\
	".\d_ticcmd.h"\
	".\doomdata.h"\
	".\doomdef.h"\
	".\doomstat.h"\
	".\doomtype.h"\
	".\dstrings.h"\
	".\g_level.h"\
	".\i_system.h"\
	".\Info.h"\
	".\m_fixed.h"\
	".\m_random.h"\
	".\p_inter.h"\
	".\p_local.h"\
	".\p_mobj.h"\
	".\p_pspr.h"\
	".\p_spec.h"\
	".\r_bsp.h"\
	".\r_data.h"\
	".\r_defs.h"\
	".\r_draw.h"\
	".\r_local.h"\
	".\r_main.h"\
	".\r_plane.h"\
	".\r_segs.h"\
	".\r_state.h"\
	".\r_things.h"\
	".\s_sound.h"\
	".\sounds.h"\
	".\Tables.h"\
	
CPP_SWITCHES=/nologo /G5 /MTd /W3 /Gm /GX /Zi /Od /I "e:\ptc\source" /D "WIN32"\
 /D "_DEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\p_inter.obj" : $(SOURCE) $(DEP_CPP_P_INT) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=.\p_lights.c

!IF  "$(CFG)" == "doom - Win32 Release"

DEP_CPP_P_LIG=\
	".\c_cvars.h"\
	".\d_items.h"\
	".\d_netinf.h"\
	".\d_player.h"\
	".\d_proto.h"\
	".\d_think.h"\
	".\d_ticcmd.h"\
	".\doomdata.h"\
	".\doomdef.h"\
	".\doomtype.h"\
	".\Info.h"\
	".\m_fixed.h"\
	".\m_random.h"\
	".\p_local.h"\
	".\p_mobj.h"\
	".\p_pspr.h"\
	".\p_spec.h"\
	".\r_bsp.h"\
	".\r_data.h"\
	".\r_defs.h"\
	".\r_draw.h"\
	".\r_local.h"\
	".\r_main.h"\
	".\r_plane.h"\
	".\r_segs.h"\
	".\r_state.h"\
	".\r_things.h"\
	".\Tables.h"\
	".\z_zone.h"\
	
CPP_SWITCHES=/nologo /G5 /MT /W3 /GX /O2 /I "e:\ptc\source" /D "WIN32" /D\
 "NDEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\p_lights.obj" : $(SOURCE) $(DEP_CPP_P_LIG) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "doom - Win32 Debug"

DEP_CPP_P_LIG=\
	".\c_cvars.h"\
	".\d_items.h"\
	".\d_netinf.h"\
	".\d_player.h"\
	".\d_proto.h"\
	".\d_think.h"\
	".\d_ticcmd.h"\
	".\doomdata.h"\
	".\doomdef.h"\
	".\doomtype.h"\
	".\Info.h"\
	".\m_fixed.h"\
	".\m_random.h"\
	".\p_local.h"\
	".\p_mobj.h"\
	".\p_pspr.h"\
	".\p_spec.h"\
	".\r_bsp.h"\
	".\r_data.h"\
	".\r_defs.h"\
	".\r_draw.h"\
	".\r_local.h"\
	".\r_main.h"\
	".\r_plane.h"\
	".\r_segs.h"\
	".\r_state.h"\
	".\r_things.h"\
	".\Tables.h"\
	".\z_zone.h"\
	
CPP_SWITCHES=/nologo /G5 /MTd /W3 /Gm /GX /Zi /Od /I "e:\ptc\source" /D "WIN32"\
 /D "_DEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\p_lights.obj" : $(SOURCE) $(DEP_CPP_P_LIG) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=.\p_map.c

!IF  "$(CFG)" == "doom - Win32 Release"

DEP_CPP_P_MAP=\
	".\c_cvars.h"\
	".\d_event.h"\
	".\d_items.h"\
	".\d_net.h"\
	".\d_netinf.h"\
	".\d_player.h"\
	".\d_proto.h"\
	".\d_think.h"\
	".\d_ticcmd.h"\
	".\doomdata.h"\
	".\doomdef.h"\
	".\doomstat.h"\
	".\doomtype.h"\
	".\g_level.h"\
	".\i_system.h"\
	".\Info.h"\
	".\m_alloc.h"\
	".\m_bbox.h"\
	".\m_fixed.h"\
	".\m_random.h"\
	".\p_local.h"\
	".\p_mobj.h"\
	".\p_pspr.h"\
	".\p_spec.h"\
	".\r_bsp.h"\
	".\r_data.h"\
	".\r_defs.h"\
	".\r_draw.h"\
	".\r_local.h"\
	".\r_main.h"\
	".\r_plane.h"\
	".\r_segs.h"\
	".\r_state.h"\
	".\r_things.h"\
	".\s_sound.h"\
	".\sounds.h"\
	".\Tables.h"\
	".\vectors.h"\
	
CPP_SWITCHES=/nologo /G5 /MT /W3 /GX /O2 /I "e:\ptc\source" /D "WIN32" /D\
 "NDEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\p_map.obj" : $(SOURCE) $(DEP_CPP_P_MAP) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "doom - Win32 Debug"

DEP_CPP_P_MAP=\
	".\c_cvars.h"\
	".\d_event.h"\
	".\d_items.h"\
	".\d_net.h"\
	".\d_netinf.h"\
	".\d_player.h"\
	".\d_proto.h"\
	".\d_think.h"\
	".\d_ticcmd.h"\
	".\doomdata.h"\
	".\doomdef.h"\
	".\doomstat.h"\
	".\doomtype.h"\
	".\g_level.h"\
	".\i_system.h"\
	".\Info.h"\
	".\m_alloc.h"\
	".\m_bbox.h"\
	".\m_fixed.h"\
	".\m_random.h"\
	".\p_local.h"\
	".\p_mobj.h"\
	".\p_pspr.h"\
	".\p_spec.h"\
	".\r_bsp.h"\
	".\r_data.h"\
	".\r_defs.h"\
	".\r_draw.h"\
	".\r_local.h"\
	".\r_main.h"\
	".\r_plane.h"\
	".\r_segs.h"\
	".\r_state.h"\
	".\r_things.h"\
	".\s_sound.h"\
	".\sounds.h"\
	".\Tables.h"\
	".\vectors.h"\
	
CPP_SWITCHES=/nologo /G5 /MTd /W3 /Gm /GX /Zi /Od /I "e:\ptc\source" /D "WIN32"\
 /D "_DEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\p_map.obj" : $(SOURCE) $(DEP_CPP_P_MAP) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=.\p_maputl.c

!IF  "$(CFG)" == "doom - Win32 Release"

DEP_CPP_P_MAPU=\
	".\c_cvars.h"\
	".\d_items.h"\
	".\d_net.h"\
	".\d_netinf.h"\
	".\d_player.h"\
	".\d_proto.h"\
	".\d_think.h"\
	".\d_ticcmd.h"\
	".\doomdata.h"\
	".\doomdef.h"\
	".\doomstat.h"\
	".\doomtype.h"\
	".\g_level.h"\
	".\Info.h"\
	".\m_bbox.h"\
	".\m_fixed.h"\
	".\p_local.h"\
	".\p_mobj.h"\
	".\p_pspr.h"\
	".\p_spec.h"\
	".\r_bsp.h"\
	".\r_data.h"\
	".\r_defs.h"\
	".\r_draw.h"\
	".\r_local.h"\
	".\r_main.h"\
	".\r_plane.h"\
	".\r_segs.h"\
	".\r_state.h"\
	".\r_things.h"\
	".\Tables.h"\
	
CPP_SWITCHES=/nologo /G5 /MT /W3 /GX /O2 /I "e:\ptc\source" /D "WIN32" /D\
 "NDEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\p_maputl.obj" : $(SOURCE) $(DEP_CPP_P_MAPU) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "doom - Win32 Debug"

DEP_CPP_P_MAPU=\
	".\c_cvars.h"\
	".\d_items.h"\
	".\d_net.h"\
	".\d_netinf.h"\
	".\d_player.h"\
	".\d_proto.h"\
	".\d_think.h"\
	".\d_ticcmd.h"\
	".\doomdata.h"\
	".\doomdef.h"\
	".\doomstat.h"\
	".\doomtype.h"\
	".\g_level.h"\
	".\Info.h"\
	".\m_bbox.h"\
	".\m_fixed.h"\
	".\p_local.h"\
	".\p_mobj.h"\
	".\p_pspr.h"\
	".\p_spec.h"\
	".\r_bsp.h"\
	".\r_data.h"\
	".\r_defs.h"\
	".\r_draw.h"\
	".\r_local.h"\
	".\r_main.h"\
	".\r_plane.h"\
	".\r_segs.h"\
	".\r_state.h"\
	".\r_things.h"\
	".\Tables.h"\
	
CPP_SWITCHES=/nologo /G5 /MTd /W3 /Gm /GX /Zi /Od /I "e:\ptc\source" /D "WIN32"\
 /D "_DEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\p_maputl.obj" : $(SOURCE) $(DEP_CPP_P_MAPU) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=.\p_mobj.c

!IF  "$(CFG)" == "doom - Win32 Release"

DEP_CPP_P_MOB=\
	".\c_cvars.h"\
	".\d_event.h"\
	".\d_items.h"\
	".\d_net.h"\
	".\d_netinf.h"\
	".\d_player.h"\
	".\d_proto.h"\
	".\d_think.h"\
	".\d_ticcmd.h"\
	".\doomdata.h"\
	".\doomdef.h"\
	".\doomstat.h"\
	".\doomtype.h"\
	".\g_level.h"\
	".\hu_stuff.h"\
	".\i_system.h"\
	".\Info.h"\
	".\m_alloc.h"\
	".\m_fixed.h"\
	".\m_random.h"\
	".\p_local.h"\
	".\p_mobj.h"\
	".\p_pspr.h"\
	".\p_spec.h"\
	".\r_bsp.h"\
	".\r_data.h"\
	".\r_defs.h"\
	".\r_draw.h"\
	".\r_local.h"\
	".\r_main.h"\
	".\r_plane.h"\
	".\r_segs.h"\
	".\r_state.h"\
	".\r_things.h"\
	".\s_sound.h"\
	".\sounds.h"\
	".\st_stuff.h"\
	".\Tables.h"\
	".\v_palett.h"\
	".\v_video.h"\
	".\z_zone.h"\
	
CPP_SWITCHES=/nologo /G5 /MT /W3 /GX /O2 /I "e:\ptc\source" /D "WIN32" /D\
 "NDEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\p_mobj.obj" : $(SOURCE) $(DEP_CPP_P_MOB) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "doom - Win32 Debug"

DEP_CPP_P_MOB=\
	".\c_cvars.h"\
	".\d_event.h"\
	".\d_items.h"\
	".\d_net.h"\
	".\d_netinf.h"\
	".\d_player.h"\
	".\d_proto.h"\
	".\d_think.h"\
	".\d_ticcmd.h"\
	".\doomdata.h"\
	".\doomdef.h"\
	".\doomstat.h"\
	".\doomtype.h"\
	".\g_level.h"\
	".\hu_stuff.h"\
	".\i_system.h"\
	".\Info.h"\
	".\m_alloc.h"\
	".\m_fixed.h"\
	".\m_random.h"\
	".\p_local.h"\
	".\p_mobj.h"\
	".\p_pspr.h"\
	".\p_spec.h"\
	".\r_bsp.h"\
	".\r_data.h"\
	".\r_defs.h"\
	".\r_draw.h"\
	".\r_local.h"\
	".\r_main.h"\
	".\r_plane.h"\
	".\r_segs.h"\
	".\r_state.h"\
	".\r_things.h"\
	".\s_sound.h"\
	".\sounds.h"\
	".\st_stuff.h"\
	".\Tables.h"\
	".\v_palett.h"\
	".\v_video.h"\
	".\z_zone.h"\
	
CPP_SWITCHES=/nologo /G5 /MTd /W3 /Gm /GX /Zi /Od /I "e:\ptc\source" /D "WIN32"\
 /D "_DEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\p_mobj.obj" : $(SOURCE) $(DEP_CPP_P_MOB) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=.\p_plats.c

!IF  "$(CFG)" == "doom - Win32 Release"

DEP_CPP_P_PLA=\
	".\c_cvars.h"\
	".\d_event.h"\
	".\d_items.h"\
	".\d_net.h"\
	".\d_netinf.h"\
	".\d_player.h"\
	".\d_proto.h"\
	".\d_think.h"\
	".\d_ticcmd.h"\
	".\doomdata.h"\
	".\doomdef.h"\
	".\doomstat.h"\
	".\doomtype.h"\
	".\g_level.h"\
	".\i_system.h"\
	".\Info.h"\
	".\m_alloc.h"\
	".\m_fixed.h"\
	".\m_random.h"\
	".\p_local.h"\
	".\p_mobj.h"\
	".\p_pspr.h"\
	".\p_spec.h"\
	".\r_bsp.h"\
	".\r_data.h"\
	".\r_defs.h"\
	".\r_draw.h"\
	".\r_local.h"\
	".\r_main.h"\
	".\r_plane.h"\
	".\r_segs.h"\
	".\r_state.h"\
	".\r_things.h"\
	".\s_sound.h"\
	".\sounds.h"\
	".\Tables.h"\
	".\z_zone.h"\
	
CPP_SWITCHES=/nologo /G5 /MT /W3 /GX /O2 /I "e:\ptc\source" /D "WIN32" /D\
 "NDEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\p_plats.obj" : $(SOURCE) $(DEP_CPP_P_PLA) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "doom - Win32 Debug"

DEP_CPP_P_PLA=\
	".\c_cvars.h"\
	".\d_event.h"\
	".\d_items.h"\
	".\d_net.h"\
	".\d_netinf.h"\
	".\d_player.h"\
	".\d_proto.h"\
	".\d_think.h"\
	".\d_ticcmd.h"\
	".\doomdata.h"\
	".\doomdef.h"\
	".\doomstat.h"\
	".\doomtype.h"\
	".\g_level.h"\
	".\i_system.h"\
	".\Info.h"\
	".\m_alloc.h"\
	".\m_fixed.h"\
	".\m_random.h"\
	".\p_local.h"\
	".\p_mobj.h"\
	".\p_pspr.h"\
	".\p_spec.h"\
	".\r_bsp.h"\
	".\r_data.h"\
	".\r_defs.h"\
	".\r_draw.h"\
	".\r_local.h"\
	".\r_main.h"\
	".\r_plane.h"\
	".\r_segs.h"\
	".\r_state.h"\
	".\r_things.h"\
	".\s_sound.h"\
	".\sounds.h"\
	".\Tables.h"\
	".\z_zone.h"\
	
CPP_SWITCHES=/nologo /G5 /MTd /W3 /Gm /GX /Zi /Od /I "e:\ptc\source" /D "WIN32"\
 /D "_DEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\p_plats.obj" : $(SOURCE) $(DEP_CPP_P_PLA) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=.\p_pspr.c

!IF  "$(CFG)" == "doom - Win32 Release"

DEP_CPP_P_PSP=\
	".\c_cvars.h"\
	".\d_event.h"\
	".\d_items.h"\
	".\d_net.h"\
	".\d_netinf.h"\
	".\d_player.h"\
	".\d_proto.h"\
	".\d_think.h"\
	".\d_ticcmd.h"\
	".\doomdata.h"\
	".\doomdef.h"\
	".\doomstat.h"\
	".\doomtype.h"\
	".\g_level.h"\
	".\Info.h"\
	".\m_fixed.h"\
	".\m_random.h"\
	".\p_local.h"\
	".\p_mobj.h"\
	".\p_pspr.h"\
	".\p_spec.h"\
	".\r_bsp.h"\
	".\r_data.h"\
	".\r_defs.h"\
	".\r_draw.h"\
	".\r_local.h"\
	".\r_main.h"\
	".\r_plane.h"\
	".\r_segs.h"\
	".\r_state.h"\
	".\r_things.h"\
	".\s_sound.h"\
	".\sounds.h"\
	".\Tables.h"\
	
CPP_SWITCHES=/nologo /G5 /MT /W3 /GX /O2 /I "e:\ptc\source" /D "WIN32" /D\
 "NDEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\p_pspr.obj" : $(SOURCE) $(DEP_CPP_P_PSP) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "doom - Win32 Debug"

DEP_CPP_P_PSP=\
	".\c_cvars.h"\
	".\d_event.h"\
	".\d_items.h"\
	".\d_net.h"\
	".\d_netinf.h"\
	".\d_player.h"\
	".\d_proto.h"\
	".\d_think.h"\
	".\d_ticcmd.h"\
	".\doomdata.h"\
	".\doomdef.h"\
	".\doomstat.h"\
	".\doomtype.h"\
	".\g_level.h"\
	".\Info.h"\
	".\m_fixed.h"\
	".\m_random.h"\
	".\p_local.h"\
	".\p_mobj.h"\
	".\p_pspr.h"\
	".\p_spec.h"\
	".\r_bsp.h"\
	".\r_data.h"\
	".\r_defs.h"\
	".\r_draw.h"\
	".\r_local.h"\
	".\r_main.h"\
	".\r_plane.h"\
	".\r_segs.h"\
	".\r_state.h"\
	".\r_things.h"\
	".\s_sound.h"\
	".\sounds.h"\
	".\Tables.h"\
	
CPP_SWITCHES=/nologo /G5 /MTd /W3 /Gm /GX /Zi /Od /I "e:\ptc\source" /D "WIN32"\
 /D "_DEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\p_pspr.obj" : $(SOURCE) $(DEP_CPP_P_PSP) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=.\p_saveg.c

!IF  "$(CFG)" == "doom - Win32 Release"

DEP_CPP_P_SAV=\
	".\c_cvars.h"\
	".\d_event.h"\
	".\d_items.h"\
	".\d_net.h"\
	".\d_netinf.h"\
	".\d_player.h"\
	".\d_proto.h"\
	".\d_think.h"\
	".\d_ticcmd.h"\
	".\doomdata.h"\
	".\doomdef.h"\
	".\doomstat.h"\
	".\doomtype.h"\
	".\g_level.h"\
	".\i_system.h"\
	".\Info.h"\
	".\m_fixed.h"\
	".\m_random.h"\
	".\p_local.h"\
	".\p_mobj.h"\
	".\p_pspr.h"\
	".\p_spec.h"\
	".\r_bsp.h"\
	".\r_data.h"\
	".\r_defs.h"\
	".\r_draw.h"\
	".\r_local.h"\
	".\r_main.h"\
	".\r_plane.h"\
	".\r_segs.h"\
	".\r_state.h"\
	".\r_things.h"\
	".\Tables.h"\
	".\v_palett.h"\
	".\z_zone.h"\
	
CPP_SWITCHES=/nologo /G5 /MT /W3 /GX /O2 /I "e:\ptc\source" /D "WIN32" /D\
 "NDEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\p_saveg.obj" : $(SOURCE) $(DEP_CPP_P_SAV) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "doom - Win32 Debug"

DEP_CPP_P_SAV=\
	".\c_cvars.h"\
	".\d_event.h"\
	".\d_items.h"\
	".\d_net.h"\
	".\d_netinf.h"\
	".\d_player.h"\
	".\d_proto.h"\
	".\d_think.h"\
	".\d_ticcmd.h"\
	".\doomdata.h"\
	".\doomdef.h"\
	".\doomstat.h"\
	".\doomtype.h"\
	".\g_level.h"\
	".\i_system.h"\
	".\Info.h"\
	".\m_fixed.h"\
	".\m_random.h"\
	".\p_local.h"\
	".\p_mobj.h"\
	".\p_pspr.h"\
	".\p_spec.h"\
	".\r_bsp.h"\
	".\r_data.h"\
	".\r_defs.h"\
	".\r_draw.h"\
	".\r_local.h"\
	".\r_main.h"\
	".\r_plane.h"\
	".\r_segs.h"\
	".\r_state.h"\
	".\r_things.h"\
	".\Tables.h"\
	".\v_palett.h"\
	".\z_zone.h"\
	
CPP_SWITCHES=/nologo /G5 /MTd /W3 /Gm /GX /Zi /Od /I "e:\ptc\source" /D "WIN32"\
 /D "_DEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\p_saveg.obj" : $(SOURCE) $(DEP_CPP_P_SAV) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=.\p_setup.c

!IF  "$(CFG)" == "doom - Win32 Release"

DEP_CPP_P_SET=\
	".\c_cvars.h"\
	".\d_event.h"\
	".\d_items.h"\
	".\d_net.h"\
	".\d_netinf.h"\
	".\d_player.h"\
	".\d_proto.h"\
	".\d_think.h"\
	".\d_ticcmd.h"\
	".\doomdata.h"\
	".\doomdef.h"\
	".\doomstat.h"\
	".\doomtype.h"\
	".\g_game.h"\
	".\g_level.h"\
	".\i_system.h"\
	".\Info.h"\
	".\m_alloc.h"\
	".\m_bbox.h"\
	".\m_fixed.h"\
	".\m_swap.h"\
	".\p_local.h"\
	".\p_mobj.h"\
	".\p_pspr.h"\
	".\p_spec.h"\
	".\r_bsp.h"\
	".\r_data.h"\
	".\r_defs.h"\
	".\r_draw.h"\
	".\r_local.h"\
	".\r_main.h"\
	".\r_plane.h"\
	".\r_segs.h"\
	".\r_state.h"\
	".\r_things.h"\
	".\s_sound.h"\
	".\Tables.h"\
	".\w_wad.h"\
	".\z_zone.h"\
	
CPP_SWITCHES=/nologo /G5 /MT /W3 /GX /O2 /I "e:\ptc\source" /D "WIN32" /D\
 "NDEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\p_setup.obj" : $(SOURCE) $(DEP_CPP_P_SET) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "doom - Win32 Debug"

DEP_CPP_P_SET=\
	".\c_cvars.h"\
	".\d_event.h"\
	".\d_items.h"\
	".\d_net.h"\
	".\d_netinf.h"\
	".\d_player.h"\
	".\d_proto.h"\
	".\d_think.h"\
	".\d_ticcmd.h"\
	".\doomdata.h"\
	".\doomdef.h"\
	".\doomstat.h"\
	".\doomtype.h"\
	".\g_game.h"\
	".\g_level.h"\
	".\i_system.h"\
	".\Info.h"\
	".\m_alloc.h"\
	".\m_bbox.h"\
	".\m_fixed.h"\
	".\m_swap.h"\
	".\p_local.h"\
	".\p_mobj.h"\
	".\p_pspr.h"\
	".\p_spec.h"\
	".\r_bsp.h"\
	".\r_data.h"\
	".\r_defs.h"\
	".\r_draw.h"\
	".\r_local.h"\
	".\r_main.h"\
	".\r_plane.h"\
	".\r_segs.h"\
	".\r_state.h"\
	".\r_things.h"\
	".\s_sound.h"\
	".\Tables.h"\
	".\w_wad.h"\
	".\z_zone.h"\
	
CPP_SWITCHES=/nologo /G5 /MTd /W3 /Gm /GX /Zi /Od /I "e:\ptc\source" /D "WIN32"\
 /D "_DEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\p_setup.obj" : $(SOURCE) $(DEP_CPP_P_SET) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=.\p_sight.c

!IF  "$(CFG)" == "doom - Win32 Release"

DEP_CPP_P_SIG=\
	".\c_cvars.h"\
	".\d_event.h"\
	".\d_items.h"\
	".\d_netinf.h"\
	".\d_player.h"\
	".\d_proto.h"\
	".\d_think.h"\
	".\d_ticcmd.h"\
	".\doomdata.h"\
	".\doomdef.h"\
	".\doomtype.h"\
	".\i_system.h"\
	".\Info.h"\
	".\m_fixed.h"\
	".\m_random.h"\
	".\p_local.h"\
	".\p_mobj.h"\
	".\p_pspr.h"\
	".\p_spec.h"\
	".\r_bsp.h"\
	".\r_data.h"\
	".\r_defs.h"\
	".\r_draw.h"\
	".\r_local.h"\
	".\r_main.h"\
	".\r_plane.h"\
	".\r_segs.h"\
	".\r_state.h"\
	".\r_things.h"\
	".\Tables.h"\
	
CPP_SWITCHES=/nologo /G5 /MT /W3 /GX /O2 /I "e:\ptc\source" /D "WIN32" /D\
 "NDEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\p_sight.obj" : $(SOURCE) $(DEP_CPP_P_SIG) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "doom - Win32 Debug"

DEP_CPP_P_SIG=\
	".\c_cvars.h"\
	".\d_event.h"\
	".\d_items.h"\
	".\d_netinf.h"\
	".\d_player.h"\
	".\d_proto.h"\
	".\d_think.h"\
	".\d_ticcmd.h"\
	".\doomdata.h"\
	".\doomdef.h"\
	".\doomtype.h"\
	".\i_system.h"\
	".\Info.h"\
	".\m_fixed.h"\
	".\m_random.h"\
	".\p_local.h"\
	".\p_mobj.h"\
	".\p_pspr.h"\
	".\p_spec.h"\
	".\r_bsp.h"\
	".\r_data.h"\
	".\r_defs.h"\
	".\r_draw.h"\
	".\r_local.h"\
	".\r_main.h"\
	".\r_plane.h"\
	".\r_segs.h"\
	".\r_state.h"\
	".\r_things.h"\
	".\Tables.h"\
	
CPP_SWITCHES=/nologo /G5 /MTd /W3 /Gm /GX /Zi /Od /I "e:\ptc\source" /D "WIN32"\
 /D "_DEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\p_sight.obj" : $(SOURCE) $(DEP_CPP_P_SIG) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=.\p_spec.c

!IF  "$(CFG)" == "doom - Win32 Release"

DEP_CPP_P_SPE=\
	".\c_consol.h"\
	".\c_cvars.h"\
	".\cmdlib.h"\
	".\d_event.h"\
	".\d_items.h"\
	".\d_net.h"\
	".\d_netinf.h"\
	".\d_player.h"\
	".\d_proto.h"\
	".\d_think.h"\
	".\d_ticcmd.h"\
	".\doomdata.h"\
	".\doomdef.h"\
	".\doomstat.h"\
	".\doomtype.h"\
	".\g_game.h"\
	".\g_level.h"\
	".\i_system.h"\
	".\Info.h"\
	".\m_alloc.h"\
	".\m_argv.h"\
	".\m_fixed.h"\
	".\m_random.h"\
	".\p_local.h"\
	".\p_mobj.h"\
	".\p_pspr.h"\
	".\p_spec.h"\
	".\r_bsp.h"\
	".\r_data.h"\
	".\r_defs.h"\
	".\r_draw.h"\
	".\r_local.h"\
	".\r_main.h"\
	".\r_plane.h"\
	".\r_segs.h"\
	".\r_sky.h"\
	".\r_state.h"\
	".\r_things.h"\
	".\s_sound.h"\
	".\sounds.h"\
	".\Tables.h"\
	".\w_wad.h"\
	".\z_zone.h"\
	
CPP_SWITCHES=/nologo /G5 /MT /W3 /GX /O2 /I "e:\ptc\source" /D "WIN32" /D\
 "NDEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\p_spec.obj" : $(SOURCE) $(DEP_CPP_P_SPE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "doom - Win32 Debug"

DEP_CPP_P_SPE=\
	".\c_consol.h"\
	".\c_cvars.h"\
	".\cmdlib.h"\
	".\d_event.h"\
	".\d_items.h"\
	".\d_net.h"\
	".\d_netinf.h"\
	".\d_player.h"\
	".\d_proto.h"\
	".\d_think.h"\
	".\d_ticcmd.h"\
	".\doomdata.h"\
	".\doomdef.h"\
	".\doomstat.h"\
	".\doomtype.h"\
	".\g_game.h"\
	".\g_level.h"\
	".\i_system.h"\
	".\Info.h"\
	".\m_alloc.h"\
	".\m_argv.h"\
	".\m_fixed.h"\
	".\m_random.h"\
	".\p_local.h"\
	".\p_mobj.h"\
	".\p_pspr.h"\
	".\p_spec.h"\
	".\r_bsp.h"\
	".\r_data.h"\
	".\r_defs.h"\
	".\r_draw.h"\
	".\r_local.h"\
	".\r_main.h"\
	".\r_plane.h"\
	".\r_segs.h"\
	".\r_sky.h"\
	".\r_state.h"\
	".\r_things.h"\
	".\s_sound.h"\
	".\sounds.h"\
	".\Tables.h"\
	".\w_wad.h"\
	".\z_zone.h"\
	
CPP_SWITCHES=/nologo /G5 /MTd /W3 /Gm /GX /Zi /Od /I "e:\ptc\source" /D "WIN32"\
 /D "_DEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\p_spec.obj" : $(SOURCE) $(DEP_CPP_P_SPE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=.\p_switch.c

!IF  "$(CFG)" == "doom - Win32 Release"

DEP_CPP_P_SWI=\
	".\c_cvars.h"\
	".\d_event.h"\
	".\d_items.h"\
	".\d_net.h"\
	".\d_netinf.h"\
	".\d_player.h"\
	".\d_proto.h"\
	".\d_think.h"\
	".\d_ticcmd.h"\
	".\doomdata.h"\
	".\doomdef.h"\
	".\doomstat.h"\
	".\doomtype.h"\
	".\g_game.h"\
	".\g_level.h"\
	".\i_system.h"\
	".\Info.h"\
	".\m_fixed.h"\
	".\p_local.h"\
	".\p_mobj.h"\
	".\p_pspr.h"\
	".\p_spec.h"\
	".\r_bsp.h"\
	".\r_data.h"\
	".\r_defs.h"\
	".\r_draw.h"\
	".\r_local.h"\
	".\r_main.h"\
	".\r_plane.h"\
	".\r_segs.h"\
	".\r_state.h"\
	".\r_things.h"\
	".\s_sound.h"\
	".\sounds.h"\
	".\Tables.h"\
	
CPP_SWITCHES=/nologo /G5 /MT /W3 /GX /O2 /I "e:\ptc\source" /D "WIN32" /D\
 "NDEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\p_switch.obj" : $(SOURCE) $(DEP_CPP_P_SWI) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "doom - Win32 Debug"

DEP_CPP_P_SWI=\
	".\c_cvars.h"\
	".\d_event.h"\
	".\d_items.h"\
	".\d_net.h"\
	".\d_netinf.h"\
	".\d_player.h"\
	".\d_proto.h"\
	".\d_think.h"\
	".\d_ticcmd.h"\
	".\doomdata.h"\
	".\doomdef.h"\
	".\doomstat.h"\
	".\doomtype.h"\
	".\g_game.h"\
	".\g_level.h"\
	".\i_system.h"\
	".\Info.h"\
	".\m_fixed.h"\
	".\p_local.h"\
	".\p_mobj.h"\
	".\p_pspr.h"\
	".\p_spec.h"\
	".\r_bsp.h"\
	".\r_data.h"\
	".\r_defs.h"\
	".\r_draw.h"\
	".\r_local.h"\
	".\r_main.h"\
	".\r_plane.h"\
	".\r_segs.h"\
	".\r_state.h"\
	".\r_things.h"\
	".\s_sound.h"\
	".\sounds.h"\
	".\Tables.h"\
	
CPP_SWITCHES=/nologo /G5 /MTd /W3 /Gm /GX /Zi /Od /I "e:\ptc\source" /D "WIN32"\
 /D "_DEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\p_switch.obj" : $(SOURCE) $(DEP_CPP_P_SWI) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=.\p_telept.c

!IF  "$(CFG)" == "doom - Win32 Release"

DEP_CPP_P_TEL=\
	".\c_cvars.h"\
	".\d_items.h"\
	".\d_netinf.h"\
	".\d_player.h"\
	".\d_proto.h"\
	".\d_think.h"\
	".\d_ticcmd.h"\
	".\doomdata.h"\
	".\doomdef.h"\
	".\doomtype.h"\
	".\Info.h"\
	".\m_fixed.h"\
	".\p_local.h"\
	".\p_mobj.h"\
	".\p_pspr.h"\
	".\p_spec.h"\
	".\r_bsp.h"\
	".\r_data.h"\
	".\r_defs.h"\
	".\r_draw.h"\
	".\r_local.h"\
	".\r_main.h"\
	".\r_plane.h"\
	".\r_segs.h"\
	".\r_state.h"\
	".\r_things.h"\
	".\s_sound.h"\
	".\sounds.h"\
	".\Tables.h"\
	
CPP_SWITCHES=/nologo /G5 /MT /W3 /GX /O2 /I "e:\ptc\source" /D "WIN32" /D\
 "NDEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\p_telept.obj" : $(SOURCE) $(DEP_CPP_P_TEL) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "doom - Win32 Debug"

DEP_CPP_P_TEL=\
	".\c_cvars.h"\
	".\d_items.h"\
	".\d_netinf.h"\
	".\d_player.h"\
	".\d_proto.h"\
	".\d_think.h"\
	".\d_ticcmd.h"\
	".\doomdata.h"\
	".\doomdef.h"\
	".\doomtype.h"\
	".\Info.h"\
	".\m_fixed.h"\
	".\p_local.h"\
	".\p_mobj.h"\
	".\p_pspr.h"\
	".\p_spec.h"\
	".\r_bsp.h"\
	".\r_data.h"\
	".\r_defs.h"\
	".\r_draw.h"\
	".\r_local.h"\
	".\r_main.h"\
	".\r_plane.h"\
	".\r_segs.h"\
	".\r_state.h"\
	".\r_things.h"\
	".\s_sound.h"\
	".\sounds.h"\
	".\Tables.h"\
	
CPP_SWITCHES=/nologo /G5 /MTd /W3 /Gm /GX /Zi /Od /I "e:\ptc\source" /D "WIN32"\
 /D "_DEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\p_telept.obj" : $(SOURCE) $(DEP_CPP_P_TEL) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=.\p_tick.c

!IF  "$(CFG)" == "doom - Win32 Release"

DEP_CPP_P_TIC=\
	".\c_cvars.h"\
	".\d_items.h"\
	".\d_net.h"\
	".\d_netinf.h"\
	".\d_player.h"\
	".\d_proto.h"\
	".\d_think.h"\
	".\d_ticcmd.h"\
	".\doomdata.h"\
	".\doomdef.h"\
	".\doomstat.h"\
	".\doomtype.h"\
	".\g_level.h"\
	".\Info.h"\
	".\m_fixed.h"\
	".\p_local.h"\
	".\p_mobj.h"\
	".\p_pspr.h"\
	".\p_spec.h"\
	".\r_bsp.h"\
	".\r_data.h"\
	".\r_defs.h"\
	".\r_draw.h"\
	".\r_local.h"\
	".\r_main.h"\
	".\r_plane.h"\
	".\r_segs.h"\
	".\r_state.h"\
	".\r_things.h"\
	".\Tables.h"\
	".\z_zone.h"\
	
CPP_SWITCHES=/nologo /G5 /MT /W3 /GX /O2 /I "e:\ptc\source" /D "WIN32" /D\
 "NDEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\p_tick.obj" : $(SOURCE) $(DEP_CPP_P_TIC) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "doom - Win32 Debug"

DEP_CPP_P_TIC=\
	".\c_cvars.h"\
	".\d_items.h"\
	".\d_net.h"\
	".\d_netinf.h"\
	".\d_player.h"\
	".\d_proto.h"\
	".\d_think.h"\
	".\d_ticcmd.h"\
	".\doomdata.h"\
	".\doomdef.h"\
	".\doomstat.h"\
	".\doomtype.h"\
	".\g_level.h"\
	".\Info.h"\
	".\m_fixed.h"\
	".\p_local.h"\
	".\p_mobj.h"\
	".\p_pspr.h"\
	".\p_spec.h"\
	".\r_bsp.h"\
	".\r_data.h"\
	".\r_defs.h"\
	".\r_draw.h"\
	".\r_local.h"\
	".\r_main.h"\
	".\r_plane.h"\
	".\r_segs.h"\
	".\r_state.h"\
	".\r_things.h"\
	".\Tables.h"\
	".\z_zone.h"\
	
CPP_SWITCHES=/nologo /G5 /MTd /W3 /Gm /GX /Zi /Od /I "e:\ptc\source" /D "WIN32"\
 /D "_DEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\p_tick.obj" : $(SOURCE) $(DEP_CPP_P_TIC) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=.\p_user.c

!IF  "$(CFG)" == "doom - Win32 Release"

DEP_CPP_P_USE=\
	".\c_cvars.h"\
	".\d_event.h"\
	".\d_items.h"\
	".\d_net.h"\
	".\d_netinf.h"\
	".\d_player.h"\
	".\d_proto.h"\
	".\d_think.h"\
	".\d_ticcmd.h"\
	".\doomdata.h"\
	".\doomdef.h"\
	".\doomstat.h"\
	".\doomtype.h"\
	".\g_level.h"\
	".\Info.h"\
	".\m_fixed.h"\
	".\p_local.h"\
	".\p_mobj.h"\
	".\p_pspr.h"\
	".\p_spec.h"\
	".\r_bsp.h"\
	".\r_data.h"\
	".\r_defs.h"\
	".\r_draw.h"\
	".\r_local.h"\
	".\r_main.h"\
	".\r_plane.h"\
	".\r_segs.h"\
	".\r_state.h"\
	".\r_things.h"\
	".\Tables.h"\
	
CPP_SWITCHES=/nologo /G5 /MT /W3 /GX /O2 /I "e:\ptc\source" /D "WIN32" /D\
 "NDEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\p_user.obj" : $(SOURCE) $(DEP_CPP_P_USE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "doom - Win32 Debug"

DEP_CPP_P_USE=\
	".\c_cvars.h"\
	".\d_event.h"\
	".\d_items.h"\
	".\d_net.h"\
	".\d_netinf.h"\
	".\d_player.h"\
	".\d_proto.h"\
	".\d_think.h"\
	".\d_ticcmd.h"\
	".\doomdata.h"\
	".\doomdef.h"\
	".\doomstat.h"\
	".\doomtype.h"\
	".\g_level.h"\
	".\Info.h"\
	".\m_fixed.h"\
	".\p_local.h"\
	".\p_mobj.h"\
	".\p_pspr.h"\
	".\p_spec.h"\
	".\r_bsp.h"\
	".\r_data.h"\
	".\r_defs.h"\
	".\r_draw.h"\
	".\r_local.h"\
	".\r_main.h"\
	".\r_plane.h"\
	".\r_segs.h"\
	".\r_state.h"\
	".\r_things.h"\
	".\Tables.h"\
	
CPP_SWITCHES=/nologo /G5 /MTd /W3 /Gm /GX /Zi /Od /I "e:\ptc\source" /D "WIN32"\
 /D "_DEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\p_user.obj" : $(SOURCE) $(DEP_CPP_P_USE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=.\Qmus2mid.c
DEP_CPP_QMUS2=\
	".\m_alloc.h"\
	".\Qmus2mid.h"\
	

!IF  "$(CFG)" == "doom - Win32 Release"

CPP_SWITCHES=/nologo /G5 /MT /W3 /GX /O2 /I "e:\ptc\source" /D "WIN32" /D\
 "NDEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\Qmus2mid.obj" : $(SOURCE) $(DEP_CPP_QMUS2) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "doom - Win32 Debug"

CPP_SWITCHES=/nologo /G5 /MTd /W3 /Gm /GX /Zi /Od /I "e:\ptc\source" /D "WIN32"\
 /D "_DEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\Qmus2mid.obj" : $(SOURCE) $(DEP_CPP_QMUS2) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=.\r_bsp.c

!IF  "$(CFG)" == "doom - Win32 Release"

DEP_CPP_R_BSP=\
	".\c_cvars.h"\
	".\d_event.h"\
	".\d_items.h"\
	".\d_net.h"\
	".\d_netinf.h"\
	".\d_player.h"\
	".\d_proto.h"\
	".\d_think.h"\
	".\d_ticcmd.h"\
	".\doomdata.h"\
	".\doomdef.h"\
	".\doomstat.h"\
	".\doomtype.h"\
	".\g_level.h"\
	".\i_system.h"\
	".\Info.h"\
	".\m_alloc.h"\
	".\m_bbox.h"\
	".\m_fixed.h"\
	".\p_mobj.h"\
	".\p_pspr.h"\
	".\r_data.h"\
	".\r_defs.h"\
	".\r_main.h"\
	".\r_plane.h"\
	".\r_state.h"\
	".\r_things.h"\
	".\Tables.h"\
	
CPP_SWITCHES=/nologo /G5 /MT /W3 /GX /O2 /I "e:\ptc\source" /D "WIN32" /D\
 "NDEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\r_bsp.obj" : $(SOURCE) $(DEP_CPP_R_BSP) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "doom - Win32 Debug"

DEP_CPP_R_BSP=\
	".\c_cvars.h"\
	".\d_event.h"\
	".\d_items.h"\
	".\d_net.h"\
	".\d_netinf.h"\
	".\d_player.h"\
	".\d_proto.h"\
	".\d_think.h"\
	".\d_ticcmd.h"\
	".\doomdata.h"\
	".\doomdef.h"\
	".\doomstat.h"\
	".\doomtype.h"\
	".\g_level.h"\
	".\i_system.h"\
	".\Info.h"\
	".\m_alloc.h"\
	".\m_bbox.h"\
	".\m_fixed.h"\
	".\p_mobj.h"\
	".\p_pspr.h"\
	".\r_data.h"\
	".\r_defs.h"\
	".\r_main.h"\
	".\r_plane.h"\
	".\r_state.h"\
	".\r_things.h"\
	".\Tables.h"\
	
CPP_SWITCHES=/nologo /G5 /MTd /W3 /Gm /GX /Zi /Od /I "e:\ptc\source" /D "WIN32"\
 /D "_DEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\r_bsp.obj" : $(SOURCE) $(DEP_CPP_R_BSP) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=.\r_data.c

!IF  "$(CFG)" == "doom - Win32 Release"

DEP_CPP_R_DAT=\
	".\c_cvars.h"\
	".\d_event.h"\
	".\d_items.h"\
	".\d_net.h"\
	".\d_netinf.h"\
	".\d_player.h"\
	".\d_proto.h"\
	".\d_think.h"\
	".\d_ticcmd.h"\
	".\doomdata.h"\
	".\doomdef.h"\
	".\doomstat.h"\
	".\doomtype.h"\
	".\g_level.h"\
	".\i_system.h"\
	".\Info.h"\
	".\m_fixed.h"\
	".\m_swap.h"\
	".\p_local.h"\
	".\p_mobj.h"\
	".\p_pspr.h"\
	".\p_spec.h"\
	".\r_bsp.h"\
	".\r_data.h"\
	".\r_defs.h"\
	".\r_draw.h"\
	".\r_local.h"\
	".\r_main.h"\
	".\r_plane.h"\
	".\r_segs.h"\
	".\r_sky.h"\
	".\r_state.h"\
	".\r_things.h"\
	".\Tables.h"\
	".\v_palett.h"\
	".\v_video.h"\
	".\w_wad.h"\
	".\z_zone.h"\
	
CPP_SWITCHES=/nologo /G5 /MT /W3 /GX /O2 /I "e:\ptc\source" /D "WIN32" /D\
 "NDEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\r_data.obj" : $(SOURCE) $(DEP_CPP_R_DAT) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "doom - Win32 Debug"

DEP_CPP_R_DAT=\
	".\c_cvars.h"\
	".\d_event.h"\
	".\d_items.h"\
	".\d_net.h"\
	".\d_netinf.h"\
	".\d_player.h"\
	".\d_proto.h"\
	".\d_think.h"\
	".\d_ticcmd.h"\
	".\doomdata.h"\
	".\doomdef.h"\
	".\doomstat.h"\
	".\doomtype.h"\
	".\g_level.h"\
	".\i_system.h"\
	".\Info.h"\
	".\m_fixed.h"\
	".\m_swap.h"\
	".\p_local.h"\
	".\p_mobj.h"\
	".\p_pspr.h"\
	".\p_spec.h"\
	".\r_bsp.h"\
	".\r_data.h"\
	".\r_defs.h"\
	".\r_draw.h"\
	".\r_local.h"\
	".\r_main.h"\
	".\r_plane.h"\
	".\r_segs.h"\
	".\r_sky.h"\
	".\r_state.h"\
	".\r_things.h"\
	".\Tables.h"\
	".\v_palett.h"\
	".\v_video.h"\
	".\w_wad.h"\
	".\z_zone.h"\
	
CPP_SWITCHES=/nologo /G5 /MTd /W3 /Gm /GX /Zi /Od /I "e:\ptc\source" /D "WIN32"\
 /D "_DEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\r_data.obj" : $(SOURCE) $(DEP_CPP_R_DAT) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=.\r_draw.c

!IF  "$(CFG)" == "doom - Win32 Release"

DEP_CPP_R_DRA=\
	".\c_cvars.h"\
	".\d_event.h"\
	".\d_items.h"\
	".\d_net.h"\
	".\d_netinf.h"\
	".\d_player.h"\
	".\d_proto.h"\
	".\d_think.h"\
	".\d_ticcmd.h"\
	".\doomdata.h"\
	".\doomdef.h"\
	".\doomstat.h"\
	".\doomtype.h"\
	".\g_level.h"\
	".\i_system.h"\
	".\Info.h"\
	".\m_alloc.h"\
	".\m_fixed.h"\
	".\p_mobj.h"\
	".\p_pspr.h"\
	".\r_bsp.h"\
	".\r_data.h"\
	".\r_defs.h"\
	".\r_draw.h"\
	".\r_local.h"\
	".\r_main.h"\
	".\r_plane.h"\
	".\r_segs.h"\
	".\r_state.h"\
	".\r_things.h"\
	".\st_stuff.h"\
	".\Tables.h"\
	".\v_palett.h"\
	".\v_video.h"\
	".\w_wad.h"\
	".\z_zone.h"\
	
CPP_SWITCHES=/nologo /G5 /MT /W3 /GX /O2 /I "e:\ptc\source" /D "WIN32" /D\
 "NDEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\r_draw.obj" : $(SOURCE) $(DEP_CPP_R_DRA) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "doom - Win32 Debug"

DEP_CPP_R_DRA=\
	".\c_cvars.h"\
	".\d_event.h"\
	".\d_items.h"\
	".\d_net.h"\
	".\d_netinf.h"\
	".\d_player.h"\
	".\d_proto.h"\
	".\d_think.h"\
	".\d_ticcmd.h"\
	".\doomdata.h"\
	".\doomdef.h"\
	".\doomstat.h"\
	".\doomtype.h"\
	".\g_level.h"\
	".\i_system.h"\
	".\Info.h"\
	".\m_alloc.h"\
	".\m_fixed.h"\
	".\p_mobj.h"\
	".\p_pspr.h"\
	".\r_bsp.h"\
	".\r_data.h"\
	".\r_defs.h"\
	".\r_draw.h"\
	".\r_local.h"\
	".\r_main.h"\
	".\r_plane.h"\
	".\r_segs.h"\
	".\r_state.h"\
	".\r_things.h"\
	".\st_stuff.h"\
	".\Tables.h"\
	".\v_palett.h"\
	".\v_video.h"\
	".\w_wad.h"\
	".\z_zone.h"\
	
CPP_SWITCHES=/nologo /G5 /MTd /W3 /Gm /GX /Zi /Od /I "e:\ptc\source" /D "WIN32"\
 /D "_DEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\r_draw.obj" : $(SOURCE) $(DEP_CPP_R_DRA) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=.\r_main.c

!IF  "$(CFG)" == "doom - Win32 Release"

DEP_CPP_R_MAI=\
	".\c_cvars.h"\
	".\d_event.h"\
	".\d_items.h"\
	".\d_net.h"\
	".\d_netinf.h"\
	".\d_player.h"\
	".\d_proto.h"\
	".\d_think.h"\
	".\d_ticcmd.h"\
	".\doomdata.h"\
	".\doomdef.h"\
	".\doomtype.h"\
	".\Info.h"\
	".\m_alloc.h"\
	".\m_bbox.h"\
	".\m_fixed.h"\
	".\p_mobj.h"\
	".\p_pspr.h"\
	".\r_bsp.h"\
	".\r_data.h"\
	".\r_defs.h"\
	".\r_draw.h"\
	".\r_local.h"\
	".\r_main.h"\
	".\r_plane.h"\
	".\r_segs.h"\
	".\r_sky.h"\
	".\r_state.h"\
	".\r_things.h"\
	".\st_stuff.h"\
	".\Tables.h"\
	".\v_palett.h"\
	".\v_video.h"\
	
CPP_SWITCHES=/nologo /G5 /MT /W3 /GX /O2 /I "e:\ptc\source" /D "WIN32" /D\
 "NDEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\r_main.obj" : $(SOURCE) $(DEP_CPP_R_MAI) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "doom - Win32 Debug"

DEP_CPP_R_MAI=\
	".\c_cvars.h"\
	".\d_event.h"\
	".\d_items.h"\
	".\d_net.h"\
	".\d_netinf.h"\
	".\d_player.h"\
	".\d_proto.h"\
	".\d_think.h"\
	".\d_ticcmd.h"\
	".\doomdata.h"\
	".\doomdef.h"\
	".\doomtype.h"\
	".\Info.h"\
	".\m_alloc.h"\
	".\m_bbox.h"\
	".\m_fixed.h"\
	".\p_mobj.h"\
	".\p_pspr.h"\
	".\r_bsp.h"\
	".\r_data.h"\
	".\r_defs.h"\
	".\r_draw.h"\
	".\r_local.h"\
	".\r_main.h"\
	".\r_plane.h"\
	".\r_segs.h"\
	".\r_sky.h"\
	".\r_state.h"\
	".\r_things.h"\
	".\st_stuff.h"\
	".\Tables.h"\
	".\v_palett.h"\
	".\v_video.h"\
	
CPP_SWITCHES=/nologo /G5 /MTd /W3 /Gm /GX /Zi /Od /I "e:\ptc\source" /D "WIN32"\
 /D "_DEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\r_main.obj" : $(SOURCE) $(DEP_CPP_R_MAI) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=.\r_plane.c

!IF  "$(CFG)" == "doom - Win32 Release"

DEP_CPP_R_PLA=\
	".\c_cvars.h"\
	".\d_event.h"\
	".\d_items.h"\
	".\d_net.h"\
	".\d_netinf.h"\
	".\d_player.h"\
	".\d_proto.h"\
	".\d_think.h"\
	".\d_ticcmd.h"\
	".\doomdata.h"\
	".\doomdef.h"\
	".\doomstat.h"\
	".\doomtype.h"\
	".\g_level.h"\
	".\i_system.h"\
	".\Info.h"\
	".\m_alloc.h"\
	".\m_fixed.h"\
	".\p_mobj.h"\
	".\p_pspr.h"\
	".\r_bsp.h"\
	".\r_data.h"\
	".\r_defs.h"\
	".\r_draw.h"\
	".\r_local.h"\
	".\r_main.h"\
	".\r_plane.h"\
	".\r_segs.h"\
	".\r_sky.h"\
	".\r_state.h"\
	".\r_things.h"\
	".\Tables.h"\
	".\v_palett.h"\
	".\v_video.h"\
	".\w_wad.h"\
	".\z_zone.h"\
	
CPP_SWITCHES=/nologo /G5 /MT /W3 /GX /O2 /I "e:\ptc\source" /D "WIN32" /D\
 "NDEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\r_plane.obj" : $(SOURCE) $(DEP_CPP_R_PLA) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "doom - Win32 Debug"

DEP_CPP_R_PLA=\
	".\c_cvars.h"\
	".\d_event.h"\
	".\d_items.h"\
	".\d_net.h"\
	".\d_netinf.h"\
	".\d_player.h"\
	".\d_proto.h"\
	".\d_think.h"\
	".\d_ticcmd.h"\
	".\doomdata.h"\
	".\doomdef.h"\
	".\doomstat.h"\
	".\doomtype.h"\
	".\g_level.h"\
	".\i_system.h"\
	".\Info.h"\
	".\m_alloc.h"\
	".\m_fixed.h"\
	".\p_mobj.h"\
	".\p_pspr.h"\
	".\r_bsp.h"\
	".\r_data.h"\
	".\r_defs.h"\
	".\r_draw.h"\
	".\r_local.h"\
	".\r_main.h"\
	".\r_plane.h"\
	".\r_segs.h"\
	".\r_sky.h"\
	".\r_state.h"\
	".\r_things.h"\
	".\Tables.h"\
	".\v_palett.h"\
	".\v_video.h"\
	".\w_wad.h"\
	".\z_zone.h"\
	
CPP_SWITCHES=/nologo /G5 /MTd /W3 /Gm /GX /Zi /Od /I "e:\ptc\source" /D "WIN32"\
 /D "_DEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\r_plane.obj" : $(SOURCE) $(DEP_CPP_R_PLA) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=.\r_segs.c

!IF  "$(CFG)" == "doom - Win32 Release"

DEP_CPP_R_SEG=\
	".\c_cvars.h"\
	".\d_event.h"\
	".\d_items.h"\
	".\d_net.h"\
	".\d_netinf.h"\
	".\d_player.h"\
	".\d_proto.h"\
	".\d_think.h"\
	".\d_ticcmd.h"\
	".\doomdata.h"\
	".\doomdef.h"\
	".\doomstat.h"\
	".\doomtype.h"\
	".\g_level.h"\
	".\i_system.h"\
	".\Info.h"\
	".\m_alloc.h"\
	".\m_fixed.h"\
	".\p_mobj.h"\
	".\p_pspr.h"\
	".\r_bsp.h"\
	".\r_data.h"\
	".\r_defs.h"\
	".\r_draw.h"\
	".\r_local.h"\
	".\r_main.h"\
	".\r_plane.h"\
	".\r_segs.h"\
	".\r_sky.h"\
	".\r_state.h"\
	".\r_things.h"\
	".\Tables.h"\
	
CPP_SWITCHES=/nologo /G5 /MT /W3 /GX /O2 /I "e:\ptc\source" /D "WIN32" /D\
 "NDEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\r_segs.obj" : $(SOURCE) $(DEP_CPP_R_SEG) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "doom - Win32 Debug"

DEP_CPP_R_SEG=\
	".\c_cvars.h"\
	".\d_event.h"\
	".\d_items.h"\
	".\d_net.h"\
	".\d_netinf.h"\
	".\d_player.h"\
	".\d_proto.h"\
	".\d_think.h"\
	".\d_ticcmd.h"\
	".\doomdata.h"\
	".\doomdef.h"\
	".\doomstat.h"\
	".\doomtype.h"\
	".\g_level.h"\
	".\i_system.h"\
	".\Info.h"\
	".\m_alloc.h"\
	".\m_fixed.h"\
	".\p_mobj.h"\
	".\p_pspr.h"\
	".\r_bsp.h"\
	".\r_data.h"\
	".\r_defs.h"\
	".\r_draw.h"\
	".\r_local.h"\
	".\r_main.h"\
	".\r_plane.h"\
	".\r_segs.h"\
	".\r_sky.h"\
	".\r_state.h"\
	".\r_things.h"\
	".\Tables.h"\
	
CPP_SWITCHES=/nologo /G5 /MTd /W3 /Gm /GX /Zi /Od /I "e:\ptc\source" /D "WIN32"\
 /D "_DEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\r_segs.obj" : $(SOURCE) $(DEP_CPP_R_SEG) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=.\r_sky.c

!IF  "$(CFG)" == "doom - Win32 Release"

DEP_CPP_R_SKY=\
	".\c_cvars.h"\
	".\d_items.h"\
	".\d_netinf.h"\
	".\d_player.h"\
	".\d_proto.h"\
	".\d_think.h"\
	".\d_ticcmd.h"\
	".\doomdata.h"\
	".\doomdef.h"\
	".\doomtype.h"\
	".\Info.h"\
	".\m_fixed.h"\
	".\p_mobj.h"\
	".\p_pspr.h"\
	".\r_data.h"\
	".\r_defs.h"\
	".\r_sky.h"\
	".\r_state.h"\
	".\Tables.h"\
	
CPP_SWITCHES=/nologo /G5 /MT /W3 /GX /O2 /I "e:\ptc\source" /D "WIN32" /D\
 "NDEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\r_sky.obj" : $(SOURCE) $(DEP_CPP_R_SKY) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "doom - Win32 Debug"

DEP_CPP_R_SKY=\
	".\c_cvars.h"\
	".\d_items.h"\
	".\d_netinf.h"\
	".\d_player.h"\
	".\d_proto.h"\
	".\d_think.h"\
	".\d_ticcmd.h"\
	".\doomdata.h"\
	".\doomdef.h"\
	".\doomtype.h"\
	".\Info.h"\
	".\m_fixed.h"\
	".\p_mobj.h"\
	".\p_pspr.h"\
	".\r_data.h"\
	".\r_defs.h"\
	".\r_sky.h"\
	".\r_state.h"\
	".\Tables.h"\
	
CPP_SWITCHES=/nologo /G5 /MTd /W3 /Gm /GX /Zi /Od /I "e:\ptc\source" /D "WIN32"\
 /D "_DEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\r_sky.obj" : $(SOURCE) $(DEP_CPP_R_SKY) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=.\r_things.c

!IF  "$(CFG)" == "doom - Win32 Release"

DEP_CPP_R_THI=\
	".\c_cvars.h"\
	".\d_event.h"\
	".\d_items.h"\
	".\d_net.h"\
	".\d_netinf.h"\
	".\d_player.h"\
	".\d_proto.h"\
	".\d_think.h"\
	".\d_ticcmd.h"\
	".\doomdata.h"\
	".\doomdef.h"\
	".\doomstat.h"\
	".\doomtype.h"\
	".\g_level.h"\
	".\i_system.h"\
	".\Info.h"\
	".\m_alloc.h"\
	".\m_argv.h"\
	".\m_fixed.h"\
	".\m_swap.h"\
	".\p_mobj.h"\
	".\p_pspr.h"\
	".\r_bsp.h"\
	".\r_data.h"\
	".\r_defs.h"\
	".\r_draw.h"\
	".\r_local.h"\
	".\r_main.h"\
	".\r_plane.h"\
	".\r_segs.h"\
	".\r_state.h"\
	".\r_things.h"\
	".\Tables.h"\
	".\v_palett.h"\
	".\v_video.h"\
	".\w_wad.h"\
	".\z_zone.h"\
	
CPP_SWITCHES=/nologo /G5 /MT /W3 /GX /O2 /I "e:\ptc\source" /D "WIN32" /D\
 "NDEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\r_things.obj" : $(SOURCE) $(DEP_CPP_R_THI) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "doom - Win32 Debug"

DEP_CPP_R_THI=\
	".\c_cvars.h"\
	".\d_event.h"\
	".\d_items.h"\
	".\d_net.h"\
	".\d_netinf.h"\
	".\d_player.h"\
	".\d_proto.h"\
	".\d_think.h"\
	".\d_ticcmd.h"\
	".\doomdata.h"\
	".\doomdef.h"\
	".\doomstat.h"\
	".\doomtype.h"\
	".\g_level.h"\
	".\i_system.h"\
	".\Info.h"\
	".\m_alloc.h"\
	".\m_argv.h"\
	".\m_fixed.h"\
	".\m_swap.h"\
	".\p_mobj.h"\
	".\p_pspr.h"\
	".\r_bsp.h"\
	".\r_data.h"\
	".\r_defs.h"\
	".\r_draw.h"\
	".\r_local.h"\
	".\r_main.h"\
	".\r_plane.h"\
	".\r_segs.h"\
	".\r_state.h"\
	".\r_things.h"\
	".\Tables.h"\
	".\v_palett.h"\
	".\v_video.h"\
	".\w_wad.h"\
	".\z_zone.h"\
	
CPP_SWITCHES=/nologo /G5 /MTd /W3 /Gm /GX /Zi /Od /I "e:\ptc\source" /D "WIN32"\
 /D "_DEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\r_things.obj" : $(SOURCE) $(DEP_CPP_R_THI) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=.\s_sound.c

!IF  "$(CFG)" == "doom - Win32 Release"

DEP_CPP_S_SOU=\
	".\c_cvars.h"\
	".\cmdlib.h"\
	".\d_event.h"\
	".\d_items.h"\
	".\d_net.h"\
	".\d_netinf.h"\
	".\d_player.h"\
	".\d_proto.h"\
	".\d_think.h"\
	".\d_ticcmd.h"\
	".\doomdata.h"\
	".\doomdef.h"\
	".\doomstat.h"\
	".\doomtype.h"\
	".\g_level.h"\
	".\i_music.h"\
	".\i_sound.h"\
	".\i_system.h"\
	".\Info.h"\
	".\m_alloc.h"\
	".\m_fixed.h"\
	".\m_random.h"\
	".\p_local.h"\
	".\p_mobj.h"\
	".\p_pspr.h"\
	".\p_spec.h"\
	".\r_bsp.h"\
	".\r_data.h"\
	".\r_defs.h"\
	".\r_draw.h"\
	".\r_local.h"\
	".\r_main.h"\
	".\r_plane.h"\
	".\r_segs.h"\
	".\r_state.h"\
	".\r_things.h"\
	".\s_sound.h"\
	".\sounds.h"\
	".\Tables.h"\
	".\w_wad.h"\
	".\z_zone.h"\
	
CPP_SWITCHES=/nologo /G5 /MT /W3 /GX /O2 /I "e:\ptc\source" /D "WIN32" /D\
 "NDEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\s_sound.obj" : $(SOURCE) $(DEP_CPP_S_SOU) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "doom - Win32 Debug"

DEP_CPP_S_SOU=\
	".\c_cvars.h"\
	".\cmdlib.h"\
	".\d_event.h"\
	".\d_items.h"\
	".\d_net.h"\
	".\d_netinf.h"\
	".\d_player.h"\
	".\d_proto.h"\
	".\d_think.h"\
	".\d_ticcmd.h"\
	".\doomdata.h"\
	".\doomdef.h"\
	".\doomstat.h"\
	".\doomtype.h"\
	".\g_level.h"\
	".\i_music.h"\
	".\i_sound.h"\
	".\i_system.h"\
	".\Info.h"\
	".\m_alloc.h"\
	".\m_fixed.h"\
	".\m_random.h"\
	".\p_local.h"\
	".\p_mobj.h"\
	".\p_pspr.h"\
	".\p_spec.h"\
	".\r_bsp.h"\
	".\r_data.h"\
	".\r_defs.h"\
	".\r_draw.h"\
	".\r_local.h"\
	".\r_main.h"\
	".\r_plane.h"\
	".\r_segs.h"\
	".\r_state.h"\
	".\r_things.h"\
	".\s_sound.h"\
	".\sounds.h"\
	".\Tables.h"\
	".\w_wad.h"\
	".\z_zone.h"\
	
CPP_SWITCHES=/nologo /G5 /MTd /W3 /Gm /GX /Zi /Od /I "e:\ptc\source" /D "WIN32"\
 /D "_DEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\s_sound.obj" : $(SOURCE) $(DEP_CPP_S_SOU) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=.\sounds.c

!IF  "$(CFG)" == "doom - Win32 Release"

DEP_CPP_SOUND=\
	".\doomtype.h"\
	".\sounds.h"\
	
CPP_SWITCHES=/nologo /G5 /MT /W3 /GX /O2 /I "e:\ptc\source" /D "WIN32" /D\
 "NDEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\sounds.obj" : $(SOURCE) $(DEP_CPP_SOUND) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "doom - Win32 Debug"

DEP_CPP_SOUND=\
	".\doomtype.h"\
	".\sounds.h"\
	
CPP_SWITCHES=/nologo /G5 /MTd /W3 /Gm /GX /Zi /Od /I "e:\ptc\source" /D "WIN32"\
 /D "_DEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\sounds.obj" : $(SOURCE) $(DEP_CPP_SOUND) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=.\st_lib.c

!IF  "$(CFG)" == "doom - Win32 Release"

DEP_CPP_ST_LI=\
	".\c_cvars.h"\
	".\d_event.h"\
	".\d_items.h"\
	".\d_netinf.h"\
	".\d_player.h"\
	".\d_proto.h"\
	".\d_think.h"\
	".\d_ticcmd.h"\
	".\doomdata.h"\
	".\doomdef.h"\
	".\doomtype.h"\
	".\i_system.h"\
	".\Info.h"\
	".\m_fixed.h"\
	".\m_swap.h"\
	".\p_mobj.h"\
	".\p_pspr.h"\
	".\r_bsp.h"\
	".\r_data.h"\
	".\r_defs.h"\
	".\r_draw.h"\
	".\r_local.h"\
	".\r_main.h"\
	".\r_plane.h"\
	".\r_segs.h"\
	".\r_state.h"\
	".\r_things.h"\
	".\st_lib.h"\
	".\st_stuff.h"\
	".\Tables.h"\
	".\v_palett.h"\
	".\v_video.h"\
	".\w_wad.h"\
	".\z_zone.h"\
	
CPP_SWITCHES=/nologo /G5 /MT /W3 /GX /O2 /I "e:\ptc\source" /D "WIN32" /D\
 "NDEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\st_lib.obj" : $(SOURCE) $(DEP_CPP_ST_LI) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "doom - Win32 Debug"

DEP_CPP_ST_LI=\
	".\c_cvars.h"\
	".\d_event.h"\
	".\d_items.h"\
	".\d_netinf.h"\
	".\d_player.h"\
	".\d_proto.h"\
	".\d_think.h"\
	".\d_ticcmd.h"\
	".\doomdata.h"\
	".\doomdef.h"\
	".\doomtype.h"\
	".\i_system.h"\
	".\Info.h"\
	".\m_fixed.h"\
	".\m_swap.h"\
	".\p_mobj.h"\
	".\p_pspr.h"\
	".\r_bsp.h"\
	".\r_data.h"\
	".\r_defs.h"\
	".\r_draw.h"\
	".\r_local.h"\
	".\r_main.h"\
	".\r_plane.h"\
	".\r_segs.h"\
	".\r_state.h"\
	".\r_things.h"\
	".\st_lib.h"\
	".\st_stuff.h"\
	".\Tables.h"\
	".\v_palett.h"\
	".\v_video.h"\
	".\w_wad.h"\
	".\z_zone.h"\
	
CPP_SWITCHES=/nologo /G5 /MTd /W3 /Gm /GX /Zi /Od /I "e:\ptc\source" /D "WIN32"\
 /D "_DEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\st_lib.obj" : $(SOURCE) $(DEP_CPP_ST_LI) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=.\st_new.c

!IF  "$(CFG)" == "doom - Win32 Release"

DEP_CPP_ST_NE=\
	".\c_cvars.h"\
	".\d_event.h"\
	".\d_items.h"\
	".\d_netinf.h"\
	".\d_player.h"\
	".\d_proto.h"\
	".\d_think.h"\
	".\d_ticcmd.h"\
	".\doomdata.h"\
	".\doomdef.h"\
	".\doomtype.h"\
	".\i_system.h"\
	".\Info.h"\
	".\m_fixed.h"\
	".\m_swap.h"\
	".\p_mobj.h"\
	".\p_pspr.h"\
	".\r_data.h"\
	".\r_defs.h"\
	".\r_state.h"\
	".\Tables.h"\
	".\v_palett.h"\
	".\v_video.h"\
	".\w_wad.h"\
	".\z_zone.h"\
	
CPP_SWITCHES=/nologo /G5 /MT /W3 /GX /O2 /I "e:\ptc\source" /D "WIN32" /D\
 "NDEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\st_new.obj" : $(SOURCE) $(DEP_CPP_ST_NE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "doom - Win32 Debug"

DEP_CPP_ST_NE=\
	".\c_cvars.h"\
	".\d_event.h"\
	".\d_items.h"\
	".\d_netinf.h"\
	".\d_player.h"\
	".\d_proto.h"\
	".\d_think.h"\
	".\d_ticcmd.h"\
	".\doomdata.h"\
	".\doomdef.h"\
	".\doomtype.h"\
	".\i_system.h"\
	".\Info.h"\
	".\m_fixed.h"\
	".\m_swap.h"\
	".\p_mobj.h"\
	".\p_pspr.h"\
	".\r_data.h"\
	".\r_defs.h"\
	".\r_state.h"\
	".\Tables.h"\
	".\v_palett.h"\
	".\v_video.h"\
	".\w_wad.h"\
	".\z_zone.h"\
	
CPP_SWITCHES=/nologo /G5 /MTd /W3 /Gm /GX /Zi /Od /I "e:\ptc\source" /D "WIN32"\
 /D "_DEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\st_new.obj" : $(SOURCE) $(DEP_CPP_ST_NE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=.\st_stuff.c

!IF  "$(CFG)" == "doom - Win32 Release"

DEP_CPP_ST_ST=\
	".\am_map.h"\
	".\c_cmds.h"\
	".\c_cvars.h"\
	".\c_dispch.h"\
	".\d_event.h"\
	".\d_items.h"\
	".\d_net.h"\
	".\d_netinf.h"\
	".\d_player.h"\
	".\d_proto.h"\
	".\d_think.h"\
	".\d_ticcmd.h"\
	".\doomdata.h"\
	".\doomdef.h"\
	".\doomstat.h"\
	".\doomtype.h"\
	".\dstrings.h"\
	".\g_game.h"\
	".\g_level.h"\
	".\i_system.h"\
	".\i_video.h"\
	".\Info.h"\
	".\m_cheat.h"\
	".\m_fixed.h"\
	".\m_random.h"\
	".\p_inter.h"\
	".\p_local.h"\
	".\p_mobj.h"\
	".\p_pspr.h"\
	".\p_spec.h"\
	".\r_bsp.h"\
	".\r_data.h"\
	".\r_defs.h"\
	".\r_draw.h"\
	".\r_local.h"\
	".\r_main.h"\
	".\r_plane.h"\
	".\r_segs.h"\
	".\r_state.h"\
	".\r_things.h"\
	".\s_sound.h"\
	".\sounds.h"\
	".\st_lib.h"\
	".\st_stuff.h"\
	".\Tables.h"\
	".\v_palett.h"\
	".\v_video.h"\
	".\w_wad.h"\
	".\z_zone.h"\
	
CPP_SWITCHES=/nologo /G5 /MT /W3 /GX /O2 /I "e:\ptc\source" /D "WIN32" /D\
 "NDEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\st_stuff.obj" : $(SOURCE) $(DEP_CPP_ST_ST) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "doom - Win32 Debug"

DEP_CPP_ST_ST=\
	".\am_map.h"\
	".\c_cmds.h"\
	".\c_cvars.h"\
	".\c_dispch.h"\
	".\d_event.h"\
	".\d_items.h"\
	".\d_net.h"\
	".\d_netinf.h"\
	".\d_player.h"\
	".\d_proto.h"\
	".\d_think.h"\
	".\d_ticcmd.h"\
	".\doomdata.h"\
	".\doomdef.h"\
	".\doomstat.h"\
	".\doomtype.h"\
	".\dstrings.h"\
	".\g_game.h"\
	".\g_level.h"\
	".\i_system.h"\
	".\i_video.h"\
	".\Info.h"\
	".\m_cheat.h"\
	".\m_fixed.h"\
	".\m_random.h"\
	".\p_inter.h"\
	".\p_local.h"\
	".\p_mobj.h"\
	".\p_pspr.h"\
	".\p_spec.h"\
	".\r_bsp.h"\
	".\r_data.h"\
	".\r_defs.h"\
	".\r_draw.h"\
	".\r_local.h"\
	".\r_main.h"\
	".\r_plane.h"\
	".\r_segs.h"\
	".\r_state.h"\
	".\r_things.h"\
	".\s_sound.h"\
	".\sounds.h"\
	".\st_lib.h"\
	".\st_stuff.h"\
	".\Tables.h"\
	".\v_palett.h"\
	".\v_video.h"\
	".\w_wad.h"\
	".\z_zone.h"\
	
CPP_SWITCHES=/nologo /G5 /MTd /W3 /Gm /GX /Zi /Od /I "e:\ptc\source" /D "WIN32"\
 /D "_DEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\st_stuff.obj" : $(SOURCE) $(DEP_CPP_ST_ST) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=.\tables.c

!IF  "$(CFG)" == "doom - Win32 Release"

DEP_CPP_TABLE=\
	".\m_fixed.h"\
	".\Tables.h"\
	
CPP_SWITCHES=/nologo /G5 /MT /W3 /GX /O2 /I "e:\ptc\source" /D "WIN32" /D\
 "NDEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\tables.obj" : $(SOURCE) $(DEP_CPP_TABLE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "doom - Win32 Debug"

DEP_CPP_TABLE=\
	".\m_fixed.h"\
	".\Tables.h"\
	
CPP_SWITCHES=/nologo /G5 /MTd /W3 /Gm /GX /Zi /Od /I "e:\ptc\source" /D "WIN32"\
 /D "_DEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\tables.obj" : $(SOURCE) $(DEP_CPP_TABLE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=.\v_draw.c

!IF  "$(CFG)" == "doom - Win32 Release"

DEP_CPP_V_DRA=\
	".\c_cvars.h"\
	".\d_event.h"\
	".\d_items.h"\
	".\d_netinf.h"\
	".\d_player.h"\
	".\d_proto.h"\
	".\d_think.h"\
	".\d_ticcmd.h"\
	".\doomdata.h"\
	".\doomdef.h"\
	".\doomtype.h"\
	".\i_system.h"\
	".\i_video.h"\
	".\Info.h"\
	".\m_fixed.h"\
	".\m_swap.h"\
	".\p_mobj.h"\
	".\p_pspr.h"\
	".\r_data.h"\
	".\r_defs.h"\
	".\r_state.h"\
	".\Tables.h"\
	".\v_palett.h"\
	".\v_video.h"\
	

"$(INTDIR)\v_draw.obj" : $(SOURCE) $(DEP_CPP_V_DRA) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "doom - Win32 Debug"

DEP_CPP_V_DRA=\
	".\c_cvars.h"\
	".\d_event.h"\
	".\d_items.h"\
	".\d_netinf.h"\
	".\d_player.h"\
	".\d_proto.h"\
	".\d_think.h"\
	".\d_ticcmd.h"\
	".\doomdata.h"\
	".\doomdef.h"\
	".\doomtype.h"\
	".\i_system.h"\
	".\i_video.h"\
	".\Info.h"\
	".\m_fixed.h"\
	".\m_swap.h"\
	".\p_mobj.h"\
	".\p_pspr.h"\
	".\r_data.h"\
	".\r_defs.h"\
	".\r_state.h"\
	".\Tables.h"\
	".\v_palett.h"\
	".\v_video.h"\
	

"$(INTDIR)\v_draw.obj" : $(SOURCE) $(DEP_CPP_V_DRA) "$(INTDIR)"


!ENDIF 

SOURCE=.\v_palett.c

!IF  "$(CFG)" == "doom - Win32 Release"

DEP_CPP_V_PAL=\
	".\c_cvars.h"\
	".\c_dispch.h"\
	".\d_items.h"\
	".\d_netinf.h"\
	".\d_player.h"\
	".\d_proto.h"\
	".\d_think.h"\
	".\d_ticcmd.h"\
	".\doomdata.h"\
	".\doomdef.h"\
	".\doomtype.h"\
	".\g_level.h"\
	".\i_video.h"\
	".\Info.h"\
	".\m_alloc.h"\
	".\m_fixed.h"\
	".\p_mobj.h"\
	".\p_pspr.h"\
	".\r_data.h"\
	".\r_defs.h"\
	".\r_main.h"\
	".\r_state.h"\
	".\Tables.h"\
	".\v_palett.h"\
	".\v_video.h"\
	".\w_wad.h"\
	".\z_zone.h"\
	

"$(INTDIR)\v_palett.obj" : $(SOURCE) $(DEP_CPP_V_PAL) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "doom - Win32 Debug"

DEP_CPP_V_PAL=\
	".\c_cvars.h"\
	".\c_dispch.h"\
	".\d_items.h"\
	".\d_netinf.h"\
	".\d_player.h"\
	".\d_proto.h"\
	".\d_think.h"\
	".\d_ticcmd.h"\
	".\doomdata.h"\
	".\doomdef.h"\
	".\doomtype.h"\
	".\g_level.h"\
	".\i_video.h"\
	".\Info.h"\
	".\m_alloc.h"\
	".\m_fixed.h"\
	".\p_mobj.h"\
	".\p_pspr.h"\
	".\r_data.h"\
	".\r_defs.h"\
	".\r_main.h"\
	".\r_state.h"\
	".\Tables.h"\
	".\v_palett.h"\
	".\v_video.h"\
	".\w_wad.h"\
	".\z_zone.h"\
	

"$(INTDIR)\v_palett.obj" : $(SOURCE) $(DEP_CPP_V_PAL) "$(INTDIR)"


!ENDIF 

SOURCE=.\v_text.c

!IF  "$(CFG)" == "doom - Win32 Release"

DEP_CPP_V_TEX=\
	".\c_cvars.h"\
	".\d_event.h"\
	".\d_items.h"\
	".\d_net.h"\
	".\d_netinf.h"\
	".\d_player.h"\
	".\d_proto.h"\
	".\d_think.h"\
	".\d_ticcmd.h"\
	".\doomdata.h"\
	".\doomdef.h"\
	".\doomstat.h"\
	".\doomtype.h"\
	".\g_level.h"\
	".\hu_stuff.h"\
	".\i_system.h"\
	".\Info.h"\
	".\m_fixed.h"\
	".\m_swap.h"\
	".\p_mobj.h"\
	".\p_pspr.h"\
	".\r_data.h"\
	".\r_defs.h"\
	".\r_state.h"\
	".\Tables.h"\
	".\v_palett.h"\
	".\v_text.h"\
	".\v_video.h"\
	".\w_wad.h"\
	".\z_zone.h"\
	

"$(INTDIR)\v_text.obj" : $(SOURCE) $(DEP_CPP_V_TEX) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "doom - Win32 Debug"

DEP_CPP_V_TEX=\
	".\c_cvars.h"\
	".\d_event.h"\
	".\d_items.h"\
	".\d_net.h"\
	".\d_netinf.h"\
	".\d_player.h"\
	".\d_proto.h"\
	".\d_think.h"\
	".\d_ticcmd.h"\
	".\doomdata.h"\
	".\doomdef.h"\
	".\doomstat.h"\
	".\doomtype.h"\
	".\g_level.h"\
	".\hu_stuff.h"\
	".\i_system.h"\
	".\Info.h"\
	".\m_fixed.h"\
	".\m_swap.h"\
	".\p_mobj.h"\
	".\p_pspr.h"\
	".\r_data.h"\
	".\r_defs.h"\
	".\r_state.h"\
	".\Tables.h"\
	".\v_palett.h"\
	".\v_text.h"\
	".\v_video.h"\
	".\w_wad.h"\
	".\z_zone.h"\
	

"$(INTDIR)\v_text.obj" : $(SOURCE) $(DEP_CPP_V_TEX) "$(INTDIR)"


!ENDIF 

SOURCE=.\v_video.c

!IF  "$(CFG)" == "doom - Win32 Release"

DEP_CPP_V_VID=\
	".\c_cvars.h"\
	".\c_dispch.h"\
	".\cmdlib.h"\
	".\d_event.h"\
	".\d_items.h"\
	".\d_net.h"\
	".\d_netinf.h"\
	".\d_player.h"\
	".\d_proto.h"\
	".\d_think.h"\
	".\d_ticcmd.h"\
	".\doomdata.h"\
	".\doomdef.h"\
	".\doomstat.h"\
	".\doomtype.h"\
	".\g_level.h"\
	".\hu_stuff.h"\
	".\i_system.h"\
	".\i_video.h"\
	".\Info.h"\
	".\m_alloc.h"\
	".\m_argv.h"\
	".\m_bbox.h"\
	".\m_fixed.h"\
	".\m_swap.h"\
	".\p_mobj.h"\
	".\p_pspr.h"\
	".\r_bsp.h"\
	".\r_data.h"\
	".\r_defs.h"\
	".\r_draw.h"\
	".\r_local.h"\
	".\r_main.h"\
	".\r_plane.h"\
	".\r_segs.h"\
	".\r_state.h"\
	".\r_things.h"\
	".\Tables.h"\
	".\v_palett.h"\
	".\v_text.h"\
	".\v_video.h"\
	".\w_wad.h"\
	".\z_zone.h"\
	
CPP_SWITCHES=/nologo /G5 /MT /W3 /GX /O2 /I "e:\ptc\source" /D "WIN32" /D\
 "NDEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\v_video.obj" : $(SOURCE) $(DEP_CPP_V_VID) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "doom - Win32 Debug"

DEP_CPP_V_VID=\
	".\c_cvars.h"\
	".\c_dispch.h"\
	".\cmdlib.h"\
	".\d_event.h"\
	".\d_items.h"\
	".\d_net.h"\
	".\d_netinf.h"\
	".\d_player.h"\
	".\d_proto.h"\
	".\d_think.h"\
	".\d_ticcmd.h"\
	".\doomdata.h"\
	".\doomdef.h"\
	".\doomstat.h"\
	".\doomtype.h"\
	".\g_level.h"\
	".\hu_stuff.h"\
	".\i_system.h"\
	".\i_video.h"\
	".\Info.h"\
	".\m_alloc.h"\
	".\m_argv.h"\
	".\m_bbox.h"\
	".\m_fixed.h"\
	".\m_swap.h"\
	".\p_mobj.h"\
	".\p_pspr.h"\
	".\r_bsp.h"\
	".\r_data.h"\
	".\r_defs.h"\
	".\r_draw.h"\
	".\r_local.h"\
	".\r_main.h"\
	".\r_plane.h"\
	".\r_segs.h"\
	".\r_state.h"\
	".\r_things.h"\
	".\Tables.h"\
	".\v_palett.h"\
	".\v_text.h"\
	".\v_video.h"\
	".\w_wad.h"\
	".\z_zone.h"\
	
CPP_SWITCHES=/nologo /G5 /MTd /W3 /Gm /GX /Zi /Od /I "e:\ptc\source" /D "WIN32"\
 /D "_DEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\v_video.obj" : $(SOURCE) $(DEP_CPP_V_VID) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=.\vectors.c

!IF  "$(CFG)" == "doom - Win32 Release"

DEP_CPP_VECTO=\
	".\d_think.h"\
	".\doomdata.h"\
	".\doomdef.h"\
	".\doomtype.h"\
	".\Info.h"\
	".\m_fixed.h"\
	".\p_mobj.h"\
	".\Tables.h"\
	".\vectors.h"\
	

"$(INTDIR)\vectors.obj" : $(SOURCE) $(DEP_CPP_VECTO) "$(INTDIR)"


!ELSEIF  "$(CFG)" == "doom - Win32 Debug"

DEP_CPP_VECTO=\
	".\d_think.h"\
	".\doomdata.h"\
	".\doomdef.h"\
	".\doomtype.h"\
	".\Info.h"\
	".\m_fixed.h"\
	".\p_mobj.h"\
	".\Tables.h"\
	".\vectors.h"\
	

"$(INTDIR)\vectors.obj" : $(SOURCE) $(DEP_CPP_VECTO) "$(INTDIR)"


!ENDIF 

SOURCE=.\w_wad.c

!IF  "$(CFG)" == "doom - Win32 Release"

DEP_CPP_W_WAD=\
	".\c_cvars.h"\
	".\cmdlib.h"\
	".\d_event.h"\
	".\d_items.h"\
	".\d_net.h"\
	".\d_netinf.h"\
	".\d_player.h"\
	".\d_proto.h"\
	".\d_think.h"\
	".\d_ticcmd.h"\
	".\doomdata.h"\
	".\doomdef.h"\
	".\doomstat.h"\
	".\doomtype.h"\
	".\g_level.h"\
	".\i_system.h"\
	".\Info.h"\
	".\m_alloc.h"\
	".\m_argv.h"\
	".\m_fixed.h"\
	".\m_swap.h"\
	".\p_mobj.h"\
	".\p_pspr.h"\
	".\Tables.h"\
	".\w_wad.h"\
	".\z_zone.h"\
	
CPP_SWITCHES=/nologo /G5 /MT /W3 /GX /O2 /I "e:\ptc\source" /D "WIN32" /D\
 "NDEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\w_wad.obj" : $(SOURCE) $(DEP_CPP_W_WAD) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "doom - Win32 Debug"

DEP_CPP_W_WAD=\
	".\c_cvars.h"\
	".\cmdlib.h"\
	".\d_event.h"\
	".\d_items.h"\
	".\d_net.h"\
	".\d_netinf.h"\
	".\d_player.h"\
	".\d_proto.h"\
	".\d_think.h"\
	".\d_ticcmd.h"\
	".\doomdata.h"\
	".\doomdef.h"\
	".\doomstat.h"\
	".\doomtype.h"\
	".\g_level.h"\
	".\i_system.h"\
	".\Info.h"\
	".\m_alloc.h"\
	".\m_argv.h"\
	".\m_fixed.h"\
	".\m_swap.h"\
	".\p_mobj.h"\
	".\p_pspr.h"\
	".\Tables.h"\
	".\w_wad.h"\
	".\z_zone.h"\
	
CPP_SWITCHES=/nologo /G5 /MTd /W3 /Gm /GX /Zi /Od /I "e:\ptc\source" /D "WIN32"\
 /D "_DEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\w_wad.obj" : $(SOURCE) $(DEP_CPP_W_WAD) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=.\wi_stuff.c

!IF  "$(CFG)" == "doom - Win32 Release"

DEP_CPP_WI_ST=\
	".\c_consol.h"\
	".\c_cvars.h"\
	".\cmdlib.h"\
	".\d_event.h"\
	".\d_items.h"\
	".\d_net.h"\
	".\d_netinf.h"\
	".\d_player.h"\
	".\d_proto.h"\
	".\d_think.h"\
	".\d_ticcmd.h"\
	".\doomdata.h"\
	".\doomdef.h"\
	".\doomstat.h"\
	".\doomtype.h"\
	".\g_game.h"\
	".\g_level.h"\
	".\hu_stuff.h"\
	".\i_system.h"\
	".\Info.h"\
	".\m_fixed.h"\
	".\m_random.h"\
	".\m_swap.h"\
	".\p_mobj.h"\
	".\p_pspr.h"\
	".\r_bsp.h"\
	".\r_data.h"\
	".\r_defs.h"\
	".\r_draw.h"\
	".\r_local.h"\
	".\r_main.h"\
	".\r_plane.h"\
	".\r_segs.h"\
	".\r_state.h"\
	".\r_things.h"\
	".\s_sound.h"\
	".\sounds.h"\
	".\Tables.h"\
	".\v_palett.h"\
	".\v_video.h"\
	".\w_wad.h"\
	".\wi_stuff.h"\
	".\z_zone.h"\
	
CPP_SWITCHES=/nologo /G5 /MT /W3 /GX /O2 /I "e:\ptc\source" /D "WIN32" /D\
 "NDEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\wi_stuff.obj" : $(SOURCE) $(DEP_CPP_WI_ST) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "doom - Win32 Debug"

DEP_CPP_WI_ST=\
	".\c_consol.h"\
	".\c_cvars.h"\
	".\cmdlib.h"\
	".\d_event.h"\
	".\d_items.h"\
	".\d_net.h"\
	".\d_netinf.h"\
	".\d_player.h"\
	".\d_proto.h"\
	".\d_think.h"\
	".\d_ticcmd.h"\
	".\doomdata.h"\
	".\doomdef.h"\
	".\doomstat.h"\
	".\doomtype.h"\
	".\g_game.h"\
	".\g_level.h"\
	".\hu_stuff.h"\
	".\i_system.h"\
	".\Info.h"\
	".\m_fixed.h"\
	".\m_random.h"\
	".\m_swap.h"\
	".\p_mobj.h"\
	".\p_pspr.h"\
	".\r_bsp.h"\
	".\r_data.h"\
	".\r_defs.h"\
	".\r_draw.h"\
	".\r_local.h"\
	".\r_main.h"\
	".\r_plane.h"\
	".\r_segs.h"\
	".\r_state.h"\
	".\r_things.h"\
	".\s_sound.h"\
	".\sounds.h"\
	".\Tables.h"\
	".\v_palett.h"\
	".\v_video.h"\
	".\w_wad.h"\
	".\wi_stuff.h"\
	".\z_zone.h"\
	
CPP_SWITCHES=/nologo /G5 /MTd /W3 /Gm /GX /Zi /Od /I "e:\ptc\source" /D "WIN32"\
 /D "_DEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\wi_stuff.obj" : $(SOURCE) $(DEP_CPP_WI_ST) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=.\z_zone.c

!IF  "$(CFG)" == "doom - Win32 Release"

DEP_CPP_Z_ZON=\
	".\d_event.h"\
	".\d_proto.h"\
	".\d_ticcmd.h"\
	".\doomdef.h"\
	".\doomtype.h"\
	".\i_system.h"\
	".\m_fixed.h"\
	".\z_zone.h"\
	
CPP_SWITCHES=/nologo /G5 /MT /W3 /GX /O2 /I "e:\ptc\source" /D "WIN32" /D\
 "NDEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\z_zone.obj" : $(SOURCE) $(DEP_CPP_Z_ZON) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "doom - Win32 Debug"

DEP_CPP_Z_ZON=\
	".\d_event.h"\
	".\d_proto.h"\
	".\d_ticcmd.h"\
	".\doomdef.h"\
	".\doomtype.h"\
	".\i_system.h"\
	".\m_fixed.h"\
	".\z_zone.h"\
	
CPP_SWITCHES=/nologo /G5 /MTd /W3 /Gm /GX /Zi /Od /I "e:\ptc\source" /D "WIN32"\
 /D "_DEBUG" /D "_WINDOWS" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\doom.pch" /YX\
 /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\z_zone.obj" : $(SOURCE) $(DEP_CPP_Z_ZON) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=.\doom.rc
DEP_RSC_DOOM_=\
	".\icon1.ico"\
	

"$(INTDIR)\doom.res" : $(SOURCE) $(DEP_RSC_DOOM_) "$(INTDIR)"
	$(RSC) $(RSC_PROJ) $(SOURCE)


SOURCE=.\misc.nas

!IF  "$(CFG)" == "doom - Win32 Release"

IntDir=.\Release
InputPath=.\misc.nas
InputName=misc

"$(IntDir)/$(InputName).obj"	 : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	d:\nasm\nasm -o $(IntDir)\$(InputName).obj -f win32 $(InputPath)

!ELSEIF  "$(CFG)" == "doom - Win32 Debug"

IntDir=.\Debug
InputPath=.\misc.nas
InputName=misc

"$(IntDir)/$(InputName).obj"	 : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	d:\nasm\nasm -o $(IntDir)\$(InputName).obj -f win32 $(InputPath)

!ENDIF 

SOURCE=.\tmap.nas

!IF  "$(CFG)" == "doom - Win32 Release"

IntDir=.\Release
InputPath=.\tmap.nas
InputName=tmap

"$(IntDir)\$(InputName).obj"	 : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	d:\nasm\nasm -o $(IntDir)\$(InputName).obj -f win32 $(InputPath)

!ELSEIF  "$(CFG)" == "doom - Win32 Debug"

IntDir=.\Debug
InputPath=.\tmap.nas
InputName=tmap

"$(IntDir)\$(InputName).obj"	 : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	d:\nasm\nasm -o $(IntDir)\$(InputName).obj -f win32 $(InputPath)

!ENDIF 


!ENDIF 

