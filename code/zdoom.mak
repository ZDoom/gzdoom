# Microsoft Developer Studio Generated NMAKE File, Based on zdoom.dsp
!IF "$(CFG)" == ""
CFG=zdoom - Win32 Debug
!MESSAGE No configuration specified. Defaulting to zdoom - Win32 Debug.
!ENDIF 

!IF "$(CFG)" != "zdoom - Win32 Release" && "$(CFG)" != "zdoom - Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
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
!ERROR An invalid configuration is specified.
!ENDIF 

!IF "$(OS)" == "Windows_NT"
NULL=
!ELSE 
NULL=nul
!ENDIF 

!IF  "$(CFG)" == "zdoom - Win32 Release"

OUTDIR=.\Release
INTDIR=.\Release

ALL : "..\zdoom.exe"


CLEAN :
	-@erase "$(INTDIR)\Am_map.obj"
	-@erase "$(INTDIR)\b_bot.obj"
	-@erase "$(INTDIR)\B_func.obj"
	-@erase "$(INTDIR)\B_game.obj"
	-@erase "$(INTDIR)\b_move.obj"
	-@erase "$(INTDIR)\B_think.obj"
	-@erase "$(INTDIR)\c_bind.obj"
	-@erase "$(INTDIR)\c_cmds.obj"
	-@erase "$(INTDIR)\c_console.obj"
	-@erase "$(INTDIR)\c_cvars.obj"
	-@erase "$(INTDIR)\c_dispatch.obj"
	-@erase "$(INTDIR)\cmdlib.obj"
	-@erase "$(INTDIR)\ct_chat.obj"
	-@erase "$(INTDIR)\d_dehacked.obj"
	-@erase "$(INTDIR)\d_items.obj"
	-@erase "$(INTDIR)\D_main.obj"
	-@erase "$(INTDIR)\d_net.obj"
	-@erase "$(INTDIR)\d_netinfo.obj"
	-@erase "$(INTDIR)\d_protocol.obj"
	-@erase "$(INTDIR)\dobject.obj"
	-@erase "$(INTDIR)\doomdef.obj"
	-@erase "$(INTDIR)\doomstat.obj"
	-@erase "$(INTDIR)\dsectoreffect.obj"
	-@erase "$(INTDIR)\dstrings.obj"
	-@erase "$(INTDIR)\dthinker.obj"
	-@erase "$(INTDIR)\dxcrap.obj"
	-@erase "$(INTDIR)\f_finale.obj"
	-@erase "$(INTDIR)\f_wipe.obj"
	-@erase "$(INTDIR)\farchive.obj"
	-@erase "$(INTDIR)\g_game.obj"
	-@erase "$(INTDIR)\g_level.obj"
	-@erase "$(INTDIR)\gi.obj"
	-@erase "$(INTDIR)\i_input.obj"
	-@erase "$(INTDIR)\I_main.obj"
	-@erase "$(INTDIR)\i_music.obj"
	-@erase "$(INTDIR)\i_net.obj"
	-@erase "$(INTDIR)\i_sound.obj"
	-@erase "$(INTDIR)\I_system.obj"
	-@erase "$(INTDIR)\I_video.obj"
	-@erase "$(INTDIR)\info.obj"
	-@erase "$(INTDIR)\m_alloc.obj"
	-@erase "$(INTDIR)\m_argv.obj"
	-@erase "$(INTDIR)\m_bbox.obj"
	-@erase "$(INTDIR)\m_cheat.obj"
	-@erase "$(INTDIR)\m_fixed.obj"
	-@erase "$(INTDIR)\m_menu.obj"
	-@erase "$(INTDIR)\m_misc.obj"
	-@erase "$(INTDIR)\m_options.obj"
	-@erase "$(INTDIR)\m_random.obj"
	-@erase "$(INTDIR)\mid2strm.obj"
	-@erase "$(INTDIR)\minilzo.obj"
	-@erase "$(INTDIR)\mus2strm.obj"
	-@erase "$(INTDIR)\p_acs.obj"
	-@erase "$(INTDIR)\p_ceiling.obj"
	-@erase "$(INTDIR)\p_doors.obj"
	-@erase "$(INTDIR)\p_effect.obj"
	-@erase "$(INTDIR)\p_enemy.obj"
	-@erase "$(INTDIR)\p_floor.obj"
	-@erase "$(INTDIR)\p_interaction.obj"
	-@erase "$(INTDIR)\p_lights.obj"
	-@erase "$(INTDIR)\p_lnspec.obj"
	-@erase "$(INTDIR)\p_map.obj"
	-@erase "$(INTDIR)\p_maputl.obj"
	-@erase "$(INTDIR)\p_mobj.obj"
	-@erase "$(INTDIR)\p_pillar.obj"
	-@erase "$(INTDIR)\p_plats.obj"
	-@erase "$(INTDIR)\p_pspr.obj"
	-@erase "$(INTDIR)\p_quake.obj"
	-@erase "$(INTDIR)\p_saveg.obj"
	-@erase "$(INTDIR)\p_setup.obj"
	-@erase "$(INTDIR)\p_sight.obj"
	-@erase "$(INTDIR)\p_spec.obj"
	-@erase "$(INTDIR)\p_switch.obj"
	-@erase "$(INTDIR)\p_teleport.obj"
	-@erase "$(INTDIR)\p_things.obj"
	-@erase "$(INTDIR)\p_tick.obj"
	-@erase "$(INTDIR)\p_user.obj"
	-@erase "$(INTDIR)\p_xlat.obj"
	-@erase "$(INTDIR)\po_man.obj"
	-@erase "$(INTDIR)\r_bsp.obj"
	-@erase "$(INTDIR)\r_data.obj"
	-@erase "$(INTDIR)\r_draw.obj"
	-@erase "$(INTDIR)\r_drawt.obj"
	-@erase "$(INTDIR)\r_main.obj"
	-@erase "$(INTDIR)\r_plane.obj"
	-@erase "$(INTDIR)\r_segs.obj"
	-@erase "$(INTDIR)\r_sky.obj"
	-@erase "$(INTDIR)\r_things.obj"
	-@erase "$(INTDIR)\s_sndseq.obj"
	-@erase "$(INTDIR)\s_sound.obj"
	-@erase "$(INTDIR)\sc_man.obj"
	-@erase "$(INTDIR)\st_lib.obj"
	-@erase "$(INTDIR)\st_new.obj"
	-@erase "$(INTDIR)\st_stuff.obj"
	-@erase "$(INTDIR)\stats.obj"
	-@erase "$(INTDIR)\tables.obj"
	-@erase "$(INTDIR)\v_draw.obj"
	-@erase "$(INTDIR)\v_palette.obj"
	-@erase "$(INTDIR)\v_text.obj"
	-@erase "$(INTDIR)\v_video.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(INTDIR)\vectors.obj"
	-@erase "$(INTDIR)\w_wad.obj"
	-@erase "$(INTDIR)\wi_stuff.obj"
	-@erase "$(INTDIR)\z_zone.obj"
	-@erase "$(INTDIR)\zdoom.res"
	-@erase "$(OUTDIR)\zdoom.map"
	-@erase "..\zdoom.exe"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /Gr /MD /W3 /GX /O2 /I "../openptc/source" /I "win32" /I "." /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D "USEASM" /FAs /Fa"$(INTDIR)\\" /Fp"$(INTDIR)\zdoom.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

.c{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.c{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

MTL=midl.exe
MTL_PROJ=/nologo /D "NDEBUG" /mktyplib203 /o "NUL" /win32 
RSC=rc.exe
RSC_PROJ=/l 0x409 /fo"$(INTDIR)\zdoom.res" /d "NDEBUG" 
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\zdoom.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib ..\midas\lib\win32\vcretail\midas.lib wsock32.lib ../openptc/library/visual/ptc.lib winmm.lib /nologo /subsystem:windows /pdb:none /map:"$(INTDIR)\zdoom.map" /machine:I386 /nodefaultlib:"libc" /nodefaultlib:"libcmt" /out:"../zdoom.exe" 
LINK32_OBJS= \
	"$(INTDIR)\Am_map.obj" \
	"$(INTDIR)\b_bot.obj" \
	"$(INTDIR)\B_func.obj" \
	"$(INTDIR)\B_game.obj" \
	"$(INTDIR)\b_move.obj" \
	"$(INTDIR)\B_think.obj" \
	"$(INTDIR)\c_bind.obj" \
	"$(INTDIR)\c_cmds.obj" \
	"$(INTDIR)\c_console.obj" \
	"$(INTDIR)\c_cvars.obj" \
	"$(INTDIR)\c_dispatch.obj" \
	"$(INTDIR)\cmdlib.obj" \
	"$(INTDIR)\ct_chat.obj" \
	"$(INTDIR)\d_dehacked.obj" \
	"$(INTDIR)\d_items.obj" \
	"$(INTDIR)\D_main.obj" \
	"$(INTDIR)\d_net.obj" \
	"$(INTDIR)\d_netinfo.obj" \
	"$(INTDIR)\d_protocol.obj" \
	"$(INTDIR)\dobject.obj" \
	"$(INTDIR)\doomdef.obj" \
	"$(INTDIR)\doomstat.obj" \
	"$(INTDIR)\dsectoreffect.obj" \
	"$(INTDIR)\dstrings.obj" \
	"$(INTDIR)\dthinker.obj" \
	"$(INTDIR)\f_finale.obj" \
	"$(INTDIR)\f_wipe.obj" \
	"$(INTDIR)\farchive.obj" \
	"$(INTDIR)\g_game.obj" \
	"$(INTDIR)\g_level.obj" \
	"$(INTDIR)\gi.obj" \
	"$(INTDIR)\info.obj" \
	"$(INTDIR)\m_alloc.obj" \
	"$(INTDIR)\m_argv.obj" \
	"$(INTDIR)\m_bbox.obj" \
	"$(INTDIR)\m_cheat.obj" \
	"$(INTDIR)\m_fixed.obj" \
	"$(INTDIR)\m_menu.obj" \
	"$(INTDIR)\m_misc.obj" \
	"$(INTDIR)\m_options.obj" \
	"$(INTDIR)\m_random.obj" \
	"$(INTDIR)\minilzo.obj" \
	"$(INTDIR)\p_acs.obj" \
	"$(INTDIR)\p_ceiling.obj" \
	"$(INTDIR)\p_doors.obj" \
	"$(INTDIR)\p_effect.obj" \
	"$(INTDIR)\p_enemy.obj" \
	"$(INTDIR)\p_floor.obj" \
	"$(INTDIR)\p_interaction.obj" \
	"$(INTDIR)\p_lights.obj" \
	"$(INTDIR)\p_lnspec.obj" \
	"$(INTDIR)\p_map.obj" \
	"$(INTDIR)\p_maputl.obj" \
	"$(INTDIR)\p_mobj.obj" \
	"$(INTDIR)\p_pillar.obj" \
	"$(INTDIR)\p_plats.obj" \
	"$(INTDIR)\p_pspr.obj" \
	"$(INTDIR)\p_quake.obj" \
	"$(INTDIR)\p_saveg.obj" \
	"$(INTDIR)\p_setup.obj" \
	"$(INTDIR)\p_sight.obj" \
	"$(INTDIR)\p_spec.obj" \
	"$(INTDIR)\p_switch.obj" \
	"$(INTDIR)\p_teleport.obj" \
	"$(INTDIR)\p_things.obj" \
	"$(INTDIR)\p_tick.obj" \
	"$(INTDIR)\p_user.obj" \
	"$(INTDIR)\p_xlat.obj" \
	"$(INTDIR)\po_man.obj" \
	"$(INTDIR)\r_bsp.obj" \
	"$(INTDIR)\r_data.obj" \
	"$(INTDIR)\r_draw.obj" \
	"$(INTDIR)\r_drawt.obj" \
	"$(INTDIR)\r_main.obj" \
	"$(INTDIR)\r_plane.obj" \
	"$(INTDIR)\r_segs.obj" \
	"$(INTDIR)\r_sky.obj" \
	"$(INTDIR)\r_things.obj" \
	"$(INTDIR)\s_sndseq.obj" \
	"$(INTDIR)\s_sound.obj" \
	"$(INTDIR)\sc_man.obj" \
	"$(INTDIR)\st_lib.obj" \
	"$(INTDIR)\st_new.obj" \
	"$(INTDIR)\st_stuff.obj" \
	"$(INTDIR)\stats.obj" \
	"$(INTDIR)\tables.obj" \
	"$(INTDIR)\v_draw.obj" \
	"$(INTDIR)\v_palette.obj" \
	"$(INTDIR)\v_text.obj" \
	"$(INTDIR)\v_video.obj" \
	"$(INTDIR)\vectors.obj" \
	"$(INTDIR)\w_wad.obj" \
	"$(INTDIR)\wi_stuff.obj" \
	"$(INTDIR)\z_zone.obj" \
	"$(INTDIR)\dxcrap.obj" \
	"$(INTDIR)\i_input.obj" \
	"$(INTDIR)\I_main.obj" \
	"$(INTDIR)\i_music.obj" \
	"$(INTDIR)\i_net.obj" \
	"$(INTDIR)\i_sound.obj" \
	"$(INTDIR)\I_system.obj" \
	"$(INTDIR)\I_video.obj" \
	"$(INTDIR)\mid2strm.obj" \
	"$(INTDIR)\mus2strm.obj" \
	"$(INTDIR)\zdoom.res" \
	"$(INTDIR)\linear.obj" \
	"$(INTDIR)\misc.obj" \
	"$(INTDIR)\tmap.obj"

"..\zdoom.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ELSEIF  "$(CFG)" == "zdoom - Win32 Debug"

OUTDIR=.\Debug
INTDIR=.\Debug

ALL : "..\doomdbg.exe"


CLEAN :
	-@erase "$(INTDIR)\Am_map.obj"
	-@erase "$(INTDIR)\b_bot.obj"
	-@erase "$(INTDIR)\B_func.obj"
	-@erase "$(INTDIR)\B_game.obj"
	-@erase "$(INTDIR)\b_move.obj"
	-@erase "$(INTDIR)\B_think.obj"
	-@erase "$(INTDIR)\c_bind.obj"
	-@erase "$(INTDIR)\c_cmds.obj"
	-@erase "$(INTDIR)\c_console.obj"
	-@erase "$(INTDIR)\c_cvars.obj"
	-@erase "$(INTDIR)\c_dispatch.obj"
	-@erase "$(INTDIR)\cmdlib.obj"
	-@erase "$(INTDIR)\ct_chat.obj"
	-@erase "$(INTDIR)\d_dehacked.obj"
	-@erase "$(INTDIR)\d_items.obj"
	-@erase "$(INTDIR)\D_main.obj"
	-@erase "$(INTDIR)\d_net.obj"
	-@erase "$(INTDIR)\d_netinfo.obj"
	-@erase "$(INTDIR)\d_protocol.obj"
	-@erase "$(INTDIR)\dobject.obj"
	-@erase "$(INTDIR)\doomdef.obj"
	-@erase "$(INTDIR)\doomstat.obj"
	-@erase "$(INTDIR)\dsectoreffect.obj"
	-@erase "$(INTDIR)\dstrings.obj"
	-@erase "$(INTDIR)\dthinker.obj"
	-@erase "$(INTDIR)\dxcrap.obj"
	-@erase "$(INTDIR)\f_finale.obj"
	-@erase "$(INTDIR)\f_wipe.obj"
	-@erase "$(INTDIR)\farchive.obj"
	-@erase "$(INTDIR)\g_game.obj"
	-@erase "$(INTDIR)\g_level.obj"
	-@erase "$(INTDIR)\gi.obj"
	-@erase "$(INTDIR)\i_input.obj"
	-@erase "$(INTDIR)\I_main.obj"
	-@erase "$(INTDIR)\i_music.obj"
	-@erase "$(INTDIR)\i_net.obj"
	-@erase "$(INTDIR)\i_sound.obj"
	-@erase "$(INTDIR)\I_system.obj"
	-@erase "$(INTDIR)\I_video.obj"
	-@erase "$(INTDIR)\info.obj"
	-@erase "$(INTDIR)\m_alloc.obj"
	-@erase "$(INTDIR)\m_argv.obj"
	-@erase "$(INTDIR)\m_bbox.obj"
	-@erase "$(INTDIR)\m_cheat.obj"
	-@erase "$(INTDIR)\m_fixed.obj"
	-@erase "$(INTDIR)\m_menu.obj"
	-@erase "$(INTDIR)\m_misc.obj"
	-@erase "$(INTDIR)\m_options.obj"
	-@erase "$(INTDIR)\m_random.obj"
	-@erase "$(INTDIR)\mid2strm.obj"
	-@erase "$(INTDIR)\minilzo.obj"
	-@erase "$(INTDIR)\mus2strm.obj"
	-@erase "$(INTDIR)\p_acs.obj"
	-@erase "$(INTDIR)\p_ceiling.obj"
	-@erase "$(INTDIR)\p_doors.obj"
	-@erase "$(INTDIR)\p_effect.obj"
	-@erase "$(INTDIR)\p_enemy.obj"
	-@erase "$(INTDIR)\p_floor.obj"
	-@erase "$(INTDIR)\p_interaction.obj"
	-@erase "$(INTDIR)\p_lights.obj"
	-@erase "$(INTDIR)\p_lnspec.obj"
	-@erase "$(INTDIR)\p_map.obj"
	-@erase "$(INTDIR)\p_maputl.obj"
	-@erase "$(INTDIR)\p_mobj.obj"
	-@erase "$(INTDIR)\p_pillar.obj"
	-@erase "$(INTDIR)\p_plats.obj"
	-@erase "$(INTDIR)\p_pspr.obj"
	-@erase "$(INTDIR)\p_quake.obj"
	-@erase "$(INTDIR)\p_saveg.obj"
	-@erase "$(INTDIR)\p_setup.obj"
	-@erase "$(INTDIR)\p_sight.obj"
	-@erase "$(INTDIR)\p_spec.obj"
	-@erase "$(INTDIR)\p_switch.obj"
	-@erase "$(INTDIR)\p_teleport.obj"
	-@erase "$(INTDIR)\p_things.obj"
	-@erase "$(INTDIR)\p_tick.obj"
	-@erase "$(INTDIR)\p_user.obj"
	-@erase "$(INTDIR)\p_xlat.obj"
	-@erase "$(INTDIR)\po_man.obj"
	-@erase "$(INTDIR)\r_bsp.obj"
	-@erase "$(INTDIR)\r_data.obj"
	-@erase "$(INTDIR)\r_draw.obj"
	-@erase "$(INTDIR)\r_drawt.obj"
	-@erase "$(INTDIR)\r_main.obj"
	-@erase "$(INTDIR)\r_plane.obj"
	-@erase "$(INTDIR)\r_segs.obj"
	-@erase "$(INTDIR)\r_sky.obj"
	-@erase "$(INTDIR)\r_things.obj"
	-@erase "$(INTDIR)\s_sndseq.obj"
	-@erase "$(INTDIR)\s_sound.obj"
	-@erase "$(INTDIR)\sc_man.obj"
	-@erase "$(INTDIR)\st_lib.obj"
	-@erase "$(INTDIR)\st_new.obj"
	-@erase "$(INTDIR)\st_stuff.obj"
	-@erase "$(INTDIR)\stats.obj"
	-@erase "$(INTDIR)\tables.obj"
	-@erase "$(INTDIR)\v_draw.obj"
	-@erase "$(INTDIR)\v_palette.obj"
	-@erase "$(INTDIR)\v_text.obj"
	-@erase "$(INTDIR)\v_video.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(INTDIR)\vc60.pdb"
	-@erase "$(INTDIR)\vectors.obj"
	-@erase "$(INTDIR)\w_wad.obj"
	-@erase "$(INTDIR)\wi_stuff.obj"
	-@erase "$(INTDIR)\z_zone.obj"
	-@erase "$(INTDIR)\zdoom.res"
	-@erase "$(OUTDIR)\doomdbg.pdb"
	-@erase "..\doomdbg.exe"
	-@erase "..\doomdbg.ilk"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /G5 /Gr /MTd /W3 /Gm /GX /ZI /Od /I "../openptc/source" /I "win32" /I "." /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /D "NOASM" /Fp"$(INTDIR)\zdoom.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

.c{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.c{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

MTL=midl.exe
MTL_PROJ=/nologo /D "_DEBUG" /mktyplib203 /o "NUL" /win32 
RSC=rc.exe
RSC_PROJ=/l 0x409 /fo"$(INTDIR)\zdoom.res" /d "_DEBUG" 
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\zdoom.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib ddraw.lib winmm.lib ..\midas\lib\win32\vcretail\midas.lib wsock32.lib ../openptc/library/visual/ptcdebug.lib /nologo /subsystem:windows /incremental:yes /pdb:"$(OUTDIR)\doomdbg.pdb" /debug /machine:I386 /nodefaultlib:"libcmt" /out:"../doomdbg.exe" 
LINK32_OBJS= \
	"$(INTDIR)\Am_map.obj" \
	"$(INTDIR)\b_bot.obj" \
	"$(INTDIR)\B_func.obj" \
	"$(INTDIR)\B_game.obj" \
	"$(INTDIR)\b_move.obj" \
	"$(INTDIR)\B_think.obj" \
	"$(INTDIR)\c_bind.obj" \
	"$(INTDIR)\c_cmds.obj" \
	"$(INTDIR)\c_console.obj" \
	"$(INTDIR)\c_cvars.obj" \
	"$(INTDIR)\c_dispatch.obj" \
	"$(INTDIR)\cmdlib.obj" \
	"$(INTDIR)\ct_chat.obj" \
	"$(INTDIR)\d_dehacked.obj" \
	"$(INTDIR)\d_items.obj" \
	"$(INTDIR)\D_main.obj" \
	"$(INTDIR)\d_net.obj" \
	"$(INTDIR)\d_netinfo.obj" \
	"$(INTDIR)\d_protocol.obj" \
	"$(INTDIR)\dobject.obj" \
	"$(INTDIR)\doomdef.obj" \
	"$(INTDIR)\doomstat.obj" \
	"$(INTDIR)\dsectoreffect.obj" \
	"$(INTDIR)\dstrings.obj" \
	"$(INTDIR)\dthinker.obj" \
	"$(INTDIR)\f_finale.obj" \
	"$(INTDIR)\f_wipe.obj" \
	"$(INTDIR)\farchive.obj" \
	"$(INTDIR)\g_game.obj" \
	"$(INTDIR)\g_level.obj" \
	"$(INTDIR)\gi.obj" \
	"$(INTDIR)\info.obj" \
	"$(INTDIR)\m_alloc.obj" \
	"$(INTDIR)\m_argv.obj" \
	"$(INTDIR)\m_bbox.obj" \
	"$(INTDIR)\m_cheat.obj" \
	"$(INTDIR)\m_fixed.obj" \
	"$(INTDIR)\m_menu.obj" \
	"$(INTDIR)\m_misc.obj" \
	"$(INTDIR)\m_options.obj" \
	"$(INTDIR)\m_random.obj" \
	"$(INTDIR)\minilzo.obj" \
	"$(INTDIR)\p_acs.obj" \
	"$(INTDIR)\p_ceiling.obj" \
	"$(INTDIR)\p_doors.obj" \
	"$(INTDIR)\p_effect.obj" \
	"$(INTDIR)\p_enemy.obj" \
	"$(INTDIR)\p_floor.obj" \
	"$(INTDIR)\p_interaction.obj" \
	"$(INTDIR)\p_lights.obj" \
	"$(INTDIR)\p_lnspec.obj" \
	"$(INTDIR)\p_map.obj" \
	"$(INTDIR)\p_maputl.obj" \
	"$(INTDIR)\p_mobj.obj" \
	"$(INTDIR)\p_pillar.obj" \
	"$(INTDIR)\p_plats.obj" \
	"$(INTDIR)\p_pspr.obj" \
	"$(INTDIR)\p_quake.obj" \
	"$(INTDIR)\p_saveg.obj" \
	"$(INTDIR)\p_setup.obj" \
	"$(INTDIR)\p_sight.obj" \
	"$(INTDIR)\p_spec.obj" \
	"$(INTDIR)\p_switch.obj" \
	"$(INTDIR)\p_teleport.obj" \
	"$(INTDIR)\p_things.obj" \
	"$(INTDIR)\p_tick.obj" \
	"$(INTDIR)\p_user.obj" \
	"$(INTDIR)\p_xlat.obj" \
	"$(INTDIR)\po_man.obj" \
	"$(INTDIR)\r_bsp.obj" \
	"$(INTDIR)\r_data.obj" \
	"$(INTDIR)\r_draw.obj" \
	"$(INTDIR)\r_drawt.obj" \
	"$(INTDIR)\r_main.obj" \
	"$(INTDIR)\r_plane.obj" \
	"$(INTDIR)\r_segs.obj" \
	"$(INTDIR)\r_sky.obj" \
	"$(INTDIR)\r_things.obj" \
	"$(INTDIR)\s_sndseq.obj" \
	"$(INTDIR)\s_sound.obj" \
	"$(INTDIR)\sc_man.obj" \
	"$(INTDIR)\st_lib.obj" \
	"$(INTDIR)\st_new.obj" \
	"$(INTDIR)\st_stuff.obj" \
	"$(INTDIR)\stats.obj" \
	"$(INTDIR)\tables.obj" \
	"$(INTDIR)\v_draw.obj" \
	"$(INTDIR)\v_palette.obj" \
	"$(INTDIR)\v_text.obj" \
	"$(INTDIR)\v_video.obj" \
	"$(INTDIR)\vectors.obj" \
	"$(INTDIR)\w_wad.obj" \
	"$(INTDIR)\wi_stuff.obj" \
	"$(INTDIR)\z_zone.obj" \
	"$(INTDIR)\dxcrap.obj" \
	"$(INTDIR)\i_input.obj" \
	"$(INTDIR)\I_main.obj" \
	"$(INTDIR)\i_music.obj" \
	"$(INTDIR)\i_net.obj" \
	"$(INTDIR)\i_sound.obj" \
	"$(INTDIR)\I_system.obj" \
	"$(INTDIR)\I_video.obj" \
	"$(INTDIR)\mid2strm.obj" \
	"$(INTDIR)\mus2strm.obj" \
	"$(INTDIR)\zdoom.res" \
	"$(INTDIR)\linear.obj" \
	"$(INTDIR)\misc.obj" \
	"$(INTDIR)\tmap.obj"

"..\doomdbg.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ENDIF 


!IF "$(NO_EXTERNAL_DEPS)" != "1"
!IF EXISTS("zdoom.dep")
!INCLUDE "zdoom.dep"
!ELSE 
!MESSAGE Warning: cannot find "zdoom.dep"
!ENDIF 
!ENDIF 


!IF "$(CFG)" == "zdoom - Win32 Release" || "$(CFG)" == "zdoom - Win32 Debug"
SOURCE=.\Am_map.cpp

"$(INTDIR)\Am_map.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\b_bot.cpp

"$(INTDIR)\b_bot.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\B_func.cpp

"$(INTDIR)\B_func.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\B_game.cpp

"$(INTDIR)\B_game.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\b_move.cpp

"$(INTDIR)\b_move.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\B_think.cpp

"$(INTDIR)\B_think.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\c_bind.cpp

"$(INTDIR)\c_bind.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\c_cmds.cpp

"$(INTDIR)\c_cmds.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\c_console.cpp

"$(INTDIR)\c_console.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\c_cvars.cpp

"$(INTDIR)\c_cvars.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\c_dispatch.cpp

"$(INTDIR)\c_dispatch.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\cmdlib.cpp

"$(INTDIR)\cmdlib.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\ct_chat.cpp

"$(INTDIR)\ct_chat.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\d_dehacked.cpp

"$(INTDIR)\d_dehacked.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\d_items.cpp

"$(INTDIR)\d_items.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\D_main.cpp

"$(INTDIR)\D_main.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\d_net.cpp

"$(INTDIR)\d_net.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\d_netinfo.cpp

"$(INTDIR)\d_netinfo.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\d_protocol.cpp

"$(INTDIR)\d_protocol.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\dobject.cpp

"$(INTDIR)\dobject.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\doomdef.cpp

"$(INTDIR)\doomdef.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\doomstat.cpp

"$(INTDIR)\doomstat.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\dsectoreffect.cpp

"$(INTDIR)\dsectoreffect.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\dstrings.cpp

"$(INTDIR)\dstrings.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\dthinker.cpp

"$(INTDIR)\dthinker.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\f_finale.cpp

"$(INTDIR)\f_finale.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\f_wipe.cpp

"$(INTDIR)\f_wipe.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\farchive.cpp

"$(INTDIR)\farchive.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\g_game.cpp

"$(INTDIR)\g_game.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\g_level.cpp

"$(INTDIR)\g_level.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\gi.cpp

"$(INTDIR)\gi.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\info.cpp

"$(INTDIR)\info.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\m_alloc.cpp

"$(INTDIR)\m_alloc.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\m_argv.cpp

"$(INTDIR)\m_argv.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\m_bbox.cpp

"$(INTDIR)\m_bbox.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\m_cheat.cpp

"$(INTDIR)\m_cheat.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\m_fixed.cpp

"$(INTDIR)\m_fixed.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\m_menu.cpp

"$(INTDIR)\m_menu.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\m_misc.cpp

"$(INTDIR)\m_misc.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\m_options.cpp

"$(INTDIR)\m_options.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\m_random.cpp

"$(INTDIR)\m_random.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\minilzo.cpp

"$(INTDIR)\minilzo.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\p_acs.cpp

"$(INTDIR)\p_acs.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\p_ceiling.cpp

"$(INTDIR)\p_ceiling.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\p_doors.cpp

"$(INTDIR)\p_doors.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\p_effect.cpp

"$(INTDIR)\p_effect.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\p_enemy.cpp

"$(INTDIR)\p_enemy.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\p_floor.cpp

"$(INTDIR)\p_floor.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\p_interaction.cpp

"$(INTDIR)\p_interaction.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\p_lights.cpp

"$(INTDIR)\p_lights.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\p_lnspec.cpp

"$(INTDIR)\p_lnspec.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\p_map.cpp

"$(INTDIR)\p_map.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\p_maputl.cpp

"$(INTDIR)\p_maputl.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\p_mobj.cpp

"$(INTDIR)\p_mobj.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\p_pillar.cpp

"$(INTDIR)\p_pillar.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\p_plats.cpp

"$(INTDIR)\p_plats.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\p_pspr.cpp

"$(INTDIR)\p_pspr.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\p_quake.cpp

"$(INTDIR)\p_quake.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\p_saveg.cpp

"$(INTDIR)\p_saveg.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\p_setup.cpp

"$(INTDIR)\p_setup.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\p_sight.cpp

"$(INTDIR)\p_sight.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\p_spec.cpp

"$(INTDIR)\p_spec.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\p_switch.cpp

"$(INTDIR)\p_switch.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\p_teleport.cpp

"$(INTDIR)\p_teleport.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\p_things.cpp

"$(INTDIR)\p_things.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\p_tick.cpp

"$(INTDIR)\p_tick.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\p_user.cpp

"$(INTDIR)\p_user.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\p_xlat.cpp

"$(INTDIR)\p_xlat.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\po_man.cpp

"$(INTDIR)\po_man.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\r_bsp.cpp

"$(INTDIR)\r_bsp.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\r_data.cpp

"$(INTDIR)\r_data.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\r_draw.cpp

"$(INTDIR)\r_draw.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\r_drawt.cpp

"$(INTDIR)\r_drawt.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\r_main.cpp

"$(INTDIR)\r_main.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\r_plane.cpp

"$(INTDIR)\r_plane.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\r_segs.cpp

"$(INTDIR)\r_segs.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\r_sky.cpp

"$(INTDIR)\r_sky.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\r_things.cpp

"$(INTDIR)\r_things.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\s_sndseq.cpp

"$(INTDIR)\s_sndseq.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\s_sound.cpp

"$(INTDIR)\s_sound.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\sc_man.cpp

"$(INTDIR)\sc_man.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\st_lib.cpp

"$(INTDIR)\st_lib.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\st_new.cpp

"$(INTDIR)\st_new.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\st_stuff.cpp

"$(INTDIR)\st_stuff.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\stats.cpp

"$(INTDIR)\stats.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\tables.cpp

"$(INTDIR)\tables.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\v_draw.cpp

"$(INTDIR)\v_draw.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\v_palette.cpp

"$(INTDIR)\v_palette.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\v_text.cpp

"$(INTDIR)\v_text.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\v_video.cpp

"$(INTDIR)\v_video.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\vectors.cpp

"$(INTDIR)\vectors.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\w_wad.cpp

"$(INTDIR)\w_wad.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\wi_stuff.cpp

"$(INTDIR)\wi_stuff.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\z_zone.cpp

"$(INTDIR)\z_zone.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\linear.nas

!IF  "$(CFG)" == "zdoom - Win32 Release"

IntDir=.\Release
InputPath=.\linear.nas
InputName=linear

"$(INTDIR)\linear.obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	<<tempfile.bat 
	@echo off 
	nasmw -o $(IntDir)\$(InputName).obj -f win32 $(InputPath)
<< 
	

!ELSEIF  "$(CFG)" == "zdoom - Win32 Debug"

IntDir=.\Debug
InputPath=.\linear.nas
InputName=linear

"$(INTDIR)\linear.obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	<<tempfile.bat 
	@echo off 
	nasmw -o $(IntDir)\$(InputName).obj -f win32 $(InputPath)
<< 
	

!ENDIF 

SOURCE=.\misc.nas

!IF  "$(CFG)" == "zdoom - Win32 Release"

IntDir=.\Release
InputPath=.\misc.nas
InputName=misc

"$(INTDIR)\misc.obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	<<tempfile.bat 
	@echo off 
	nasmw -o $(IntDir)\$(InputName).obj -f win32 $(InputPath)
<< 
	

!ELSEIF  "$(CFG)" == "zdoom - Win32 Debug"

IntDir=.\Debug
InputPath=.\misc.nas
InputName=misc

"$(INTDIR)\misc.obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	<<tempfile.bat 
	@echo off 
	nasmw -o $(IntDir)\$(InputName).obj -f win32 $(InputPath)
<< 
	

!ENDIF 

SOURCE=.\tmap.nas

!IF  "$(CFG)" == "zdoom - Win32 Release"

IntDir=.\Release
InputPath=.\tmap.nas
InputName=tmap

"$(INTDIR)\tmap.obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	<<tempfile.bat 
	@echo off 
	nasmw -o $(IntDir)\$(InputName).obj -f win32 $(InputPath)
<< 
	

!ELSEIF  "$(CFG)" == "zdoom - Win32 Debug"

IntDir=.\Debug
InputPath=.\tmap.nas
InputName=tmap

"$(INTDIR)\tmap.obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	<<tempfile.bat 
	@echo off 
	nasmw -o $(IntDir)\$(InputName).obj -f win32 $(InputPath)
<< 
	

!ENDIF 

SOURCE=.\win32\dxcrap.cpp

"$(INTDIR)\dxcrap.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=.\win32\i_input.cpp

"$(INTDIR)\i_input.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=.\win32\I_main.cpp

"$(INTDIR)\I_main.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=.\win32\i_music.cpp

"$(INTDIR)\i_music.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=.\win32\i_net.cpp

"$(INTDIR)\i_net.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=.\win32\i_sound.cpp

"$(INTDIR)\i_sound.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=.\win32\I_system.cpp

"$(INTDIR)\I_system.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=.\win32\I_video.cpp

"$(INTDIR)\I_video.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=.\win32\mid2strm.cpp

"$(INTDIR)\mid2strm.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=.\win32\mus2strm.cpp

"$(INTDIR)\mus2strm.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=.\win32\zdoom.rc

!IF  "$(CFG)" == "zdoom - Win32 Release"


"$(INTDIR)\zdoom.res" : $(SOURCE) "$(INTDIR)"
	$(RSC) /l 0x409 /fo"$(INTDIR)\zdoom.res" /i "win32" /d "NDEBUG" $(SOURCE)


!ELSEIF  "$(CFG)" == "zdoom - Win32 Debug"


"$(INTDIR)\zdoom.res" : $(SOURCE) "$(INTDIR)"
	$(RSC) /l 0x409 /fo"$(INTDIR)\zdoom.res" /i "win32" /d "_DEBUG" $(SOURCE)


!ENDIF 


!ENDIF 

