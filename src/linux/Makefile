####################################################################
#
#  Makefile for ZDOOM with Linux
#
####################################################################

CXX = g++
AS = nasm
RM = rm -f
RMDIR = rmdir
CP = cp
LD = ld
INSTALL = install
ECHO = echo
TAR = tar
GZIP = gzip

basename = zdoom-1.22

# distribution names
BINTAR = $(basename)-i386.tar.gz
SRCTAR = $(basename).tar.gz

# options common to all builds
prefix = /usr/local
lib_dir = $(prefix)/lib
bin_dir = $(prefix)/bin
doc_dir = $(prefix)/doc/zdoom
share_dir = $(prefix)/share
zdoomshare_dir = $(prefix)/share/zdoom
X_libs = /usr/X11R6/lib
src_dir = src

SRCDOC = DoomLicense INSTALL README colors commands history
IMPDIR = $(src_dir)/linux
CPPFLAGS_common = -Wall -Winline -I$(src_dir) -I$(IMPDIR) -I.
KEYMAP_FILE = $(zdoomshare_dir)/xkeys
DEFS_common = -DLINUX -Dstricmp=strcasecmp -Dstrnicmp=strncasecmp \
	-DNO_STRUPR -DNO_FILELENGTH -DKEYMAP_FILE=\"$(KEYMAP_FILE)\" -DUNIX \
	-DSHARE_DIR=\"$(zdoomshare_dir)/\"
OUTFILE = zdoom
ASFLAGS_common = -f elf -DM_TARGET_LINUX
LDFLAGS_common = -rdynamic -Xlinker -rpath -Xlinker $(lib_dir)
SCPPFLAGS = -fpic -shared

# options specific to the debug build
CPPFLAGS_debug = -g -DRANGECHECK
LDFLAGS_debug =
ASFLAGS_debug = -g
DEFS_debug = -DDEBUG -DNOASM

# options specific to the release build
CPPFLAGS_release = -O2 -ffast-math -mpentium
LDFLAGS_release = -s
ASFLAGS_release =
DEFS_release = -DNDEBUG -DUSEASM

# options for the X driver
HW_X_CPPFLAGS = -DUSE_SHM -DUSE_DGA -DUSE_VIDMODE
HW_X_LDFLAGS = -L$(X_libs) -lX11 -lXext -lXxf86dga -lXxf86vm

# libraries to link with
LIBS = -lm -ldl -lpthread midas/lib/linux/gcretail/libmidas.a

# select flags for this build
mode = release
CPPFLAGS = $(CPPFLAGS_common) $(CPPFLAGS_$(mode)) \
	$(DEFS_common) $(DEFS_$(mode))
LDFLAGS = $(LDFLAGS_common) $(LDFLAGS_$(mode))
ASFLAGS = $(ASFLAGS_common) $(ASFLAGS_$(mode))

# directory to hold all intermediate files
INTDIR = $(IMPDIR)/$(mode)

# object files
OBJS = \
	$(INTDIR)/am_map.o \
	$(INTDIR)/b_bot.o \
	$(INTDIR)/b_func.o \
	$(INTDIR)/b_game.o \
	$(INTDIR)/b_move.o \
	$(INTDIR)/b_think.o \
	$(INTDIR)/c_bind.o \
	$(INTDIR)/c_cmds.o \
	$(INTDIR)/c_console.o \
	$(INTDIR)/c_cvars.o \
	$(INTDIR)/c_dispatch.o \
	$(INTDIR)/cmdlib.o \
	$(INTDIR)/ct_chat.o \
	$(INTDIR)/d_dehacked.o \
	$(INTDIR)/d_items.o \
	$(INTDIR)/d_main.o \
	$(INTDIR)/d_net.o \
	$(INTDIR)/d_netinfo.o \
	$(INTDIR)/d_protocol.o \
	$(INTDIR)/dobject.o \
	$(INTDIR)/doomdef.o \
	$(INTDIR)/doomstat.o \
	$(INTDIR)/dsectoreffect.o \
	$(INTDIR)/dstrings.o \
	$(INTDIR)/dthinker.o \
	$(INTDIR)/f_finale.o \
	$(INTDIR)/f_wipe.o \
	$(INTDIR)/farchive.o \
	$(INTDIR)/g_game.o \
	$(INTDIR)/g_level.o \
	$(INTDIR)/gi.o \
	$(INTDIR)/info.o \
	$(INTDIR)/m_alloc.o \
	$(INTDIR)/m_argv.o \
	$(INTDIR)/m_bbox.o \
	$(INTDIR)/m_cheat.o \
	$(INTDIR)/m_fixed.o \
	$(INTDIR)/m_menu.o \
	$(INTDIR)/m_misc.o \
	$(INTDIR)/m_options.o \
	$(INTDIR)/m_random.o \
	$(INTDIR)/minilzo.o \
	$(INTDIR)/p_acs.o \
	$(INTDIR)/p_ceiling.o \
	$(INTDIR)/p_doors.o \
	$(INTDIR)/p_effect.o \
	$(INTDIR)/p_enemy.o \
	$(INTDIR)/p_floor.o \
	$(INTDIR)/p_interaction.o \
	$(INTDIR)/p_lights.o \
	$(INTDIR)/p_lnspec.o \
	$(INTDIR)/p_map.o \
	$(INTDIR)/p_maputl.o \
	$(INTDIR)/p_mobj.o \
	$(INTDIR)/p_pillar.o \
	$(INTDIR)/p_plats.o \
	$(INTDIR)/p_pspr.o \
	$(INTDIR)/p_quake.o \
	$(INTDIR)/p_saveg.o \
	$(INTDIR)/p_setup.o \
	$(INTDIR)/p_sight.o \
	$(INTDIR)/p_spec.o \
	$(INTDIR)/p_switch.o \
	$(INTDIR)/p_teleport.o \
	$(INTDIR)/p_things.o \
	$(INTDIR)/p_tick.o \
	$(INTDIR)/p_user.o \
	$(INTDIR)/p_xlat.o \
	$(INTDIR)/po_man.o \
	$(INTDIR)/r_bsp.o \
	$(INTDIR)/r_data.o \
	$(INTDIR)/r_draw.o \
	$(INTDIR)/r_drawt.o \
	$(INTDIR)/r_main.o \
	$(INTDIR)/r_plane.o \
	$(INTDIR)/r_segs.o \
	$(INTDIR)/r_sky.o \
	$(INTDIR)/r_things.o \
	$(INTDIR)/s_sndseq.o \
	$(INTDIR)/s_sound.o \
	$(INTDIR)/sc_man.o \
	$(INTDIR)/st_lib.o \
	$(INTDIR)/st_new.o \
	$(INTDIR)/st_stuff.o \
	$(INTDIR)/stats.o \
	$(INTDIR)/tables.o \
	$(INTDIR)/v_draw.o \
	$(INTDIR)/v_palette.o \
	$(INTDIR)/v_text.o \
	$(INTDIR)/v_video.o \
	$(INTDIR)/vectors.o \
	$(INTDIR)/w_wad.o \
	$(INTDIR)/wi_stuff.o \
	$(INTDIR)/z_zone.o \
	$(INTDIR)/i_main.o \
	$(INTDIR)/i_music.o \
	$(INTDIR)/i_net.o \
	$(INTDIR)/i_sound.o \
	$(INTDIR)/i_system.o \
	$(INTDIR)/hardware.o \
	$(INTDIR)/linear.o \
	$(INTDIR)/misc.o \
	$(INTDIR)/tmap.o \
	$(INTDIR)/expandorama.o

SOURCES = \
	$(src_dir)/am_map.cpp \
	$(src_dir)/b_bot.cpp \
	$(src_dir)/b_func.cpp \
	$(src_dir)/b_game.cpp \
	$(src_dir)/b_move.cpp \
	$(src_dir)/b_think.cpp \
	$(src_dir)/c_bind.cpp \
	$(src_dir)/c_cmds.cpp \
	$(src_dir)/c_console.cpp \
	$(src_dir)/c_cvars.cpp \
	$(src_dir)/c_dispatch.cpp \
	$(src_dir)/cmdlib.cpp \
	$(src_dir)/ct_chat.cpp \
	$(src_dir)/d_dehacked.cpp \
	$(src_dir)/d_items.cpp \
	$(src_dir)/d_main.cpp \
	$(src_dir)/d_net.cpp \
	$(src_dir)/d_netinfo.cpp \
	$(src_dir)/d_protocol.cpp \
	$(src_dir)/dobject.cpp \
	$(src_dir)/doomdef.cpp \
	$(src_dir)/doomstat.cpp \
	$(src_dir)/dsectoreffect.cpp \
	$(src_dir)/dstrings.cpp \
	$(src_dir)/dthinker.cpp \
	$(src_dir)/f_finale.cpp \
	$(src_dir)/f_wipe.cpp \
	$(src_dir)/farchive.cpp \
	$(src_dir)/g_game.cpp \
	$(src_dir)/g_level.cpp \
	$(src_dir)/gi.cpp \
	$(src_dir)/info.cpp \
	$(src_dir)/m_alloc.cpp \
	$(src_dir)/m_argv.cpp \
	$(src_dir)/m_bbox.cpp \
	$(src_dir)/m_cheat.cpp \
	$(src_dir)/m_fixed.cpp \
	$(src_dir)/m_menu.cpp \
	$(src_dir)/m_misc.cpp \
	$(src_dir)/m_options.cpp \
	$(src_dir)/m_random.cpp \
	$(src_dir)/minilzo.cpp \
	$(src_dir)/p_acs.cpp \
	$(src_dir)/p_ceiling.cpp \
	$(src_dir)/p_doors.cpp \
	$(src_dir)/p_effect.cpp \
	$(src_dir)/p_enemy.cpp \
	$(src_dir)/p_floor.cpp \
	$(src_dir)/p_interaction.cpp \
	$(src_dir)/p_lights.cpp \
	$(src_dir)/p_lnspec.cpp \
	$(src_dir)/p_map.cpp \
	$(src_dir)/p_maputl.cpp \
	$(src_dir)/p_mobj.cpp \
	$(src_dir)/p_pillar.cpp \
	$(src_dir)/p_plats.cpp \
	$(src_dir)/p_pspr.cpp \
	$(src_dir)/p_quake.cpp \
	$(src_dir)/p_saveg.cpp \
	$(src_dir)/p_setup.cpp \
	$(src_dir)/p_sight.cpp \
	$(src_dir)/p_spec.cpp \
	$(src_dir)/p_switch.cpp \
	$(src_dir)/p_teleport.cpp \
	$(src_dir)/p_things.cpp \
	$(src_dir)/p_tick.cpp \
	$(src_dir)/p_user.cpp \
	$(src_dir)/p_xlat.cpp \
	$(src_dir)/po_man.cpp \
	$(src_dir)/r_bsp.cpp \
	$(src_dir)/r_data.cpp \
	$(src_dir)/r_draw.cpp \
	$(src_dir)/r_drawt.cpp \
	$(src_dir)/r_main.cpp \
	$(src_dir)/r_plane.cpp \
	$(src_dir)/r_segs.cpp \
	$(src_dir)/r_sky.cpp \
	$(src_dir)/r_things.cpp \
	$(src_dir)/s_sndseq.cpp \
	$(src_dir)/s_sound.cpp \
	$(src_dir)/sc_man.cpp \
	$(src_dir)/st_lib.cpp \
	$(src_dir)/st_new.cpp \
	$(src_dir)/st_stuff.cpp \
	$(src_dir)/stats.cpp \
	$(src_dir)/tables.cpp \
	$(src_dir)/v_draw.cpp \
	$(src_dir)/v_palette.cpp \
	$(src_dir)/v_text.cpp \
	$(src_dir)/v_video.cpp \
	$(src_dir)/vectors.cpp \
	$(src_dir)/w_wad.cpp \
	$(src_dir)/wi_stuff.cpp \
	$(src_dir)/z_zone.cpp \
	$(IMPDIR)/i_main.cpp \
	$(IMPDIR)/i_music.cpp \
	$(IMPDIR)/i_net.cpp \
	$(IMPDIR)/i_sound.cpp \
	$(IMPDIR)/i_system.cpp \
	$(IMPDIR)/hardware.cpp

HWSOURCES = \
	$(IMPDIR)/hw_svgalib.cpp \
	$(IMPDIR)/hw_x.cpp

ALLSOURCES = $(SOURCES) $(HWSOURCES)

zdoom doom release all: $(INTDIR)/$(OUTFILE) hwlibs

debug:
	$(MAKE) mode=debug

clean:
	$(RM) $(IMPDIR)/release/*.o
	$(RM) $(IMPDIR)/release/*.so
	$(RM) $(IMPDIR)/release/zdoom
	$(RM) $(IMPDIR)/debug/*.o
	$(RM) $(IMPDIR)/debug/*.so
	$(RM) $(IMPDIR)/debug/zdoom
	$(RM) $(IMPDIR)/*.d
	$(RM) $(src_dir)/*.d

install: $(INTDIR)/$(OUTFILE) hwlibs
	$(INSTALL) -d $(zdoomshare_dir) $(lib_dir) $(bin_dir) $(doc_dir)
	$(INSTALL) -o root -m 4755 $(INTDIR)/$(OUTFILE) $(bin_dir)
	$(INSTALL) -o root $(INTDIR)/libzdoom-x.so $(lib_dir)
	$(INSTALL) -o root $(INTDIR)/libzdoom-svgalib.so $(lib_dir)
	$(INSTALL) -o root -m 644 other/zdoom.wad $(zdoomshare_dir)
	$(INSTALL) -o root -m 644 other/bots.cfg $(zdoomshare_dir)
	$(INSTALL) -o root -m 644 other/railgun.bex $(zdoomshare_dir)
	$(INSTALL) -o root -m 644 $(SRCDOC) $(doc_dir)
	$(ECHO) >/dev/null \
	 \\\
	 \\-------------------------------------------------------------\
	 \\ Before using ZDoom, you should either copy your IWAD \
	 \\ files to $(zdoomshare_dir) or set the \
	 \\ DOOMWADDIR environment variable to point to them. \
	 \\\
	 \\ If you will be using X with a non-QWERTY keymap, you \
	 \\ should also be sure you are running X with a QWERTY \
	 \\ keymap and then run "make xkeys" so that ZDoom will be \
	 \\ able to use the same key bindings under both X and svgalib \
	 \\\
	 \\ On a RedHat 6.0 system you can do this by typing: \
	 \\    # xmodmap /usr/share/xmodmap/xmodmap.us \
	 \\    # make xkeys \
	 \\ Other distributions should be similar. \
	 \\-------------------------------------------------------------\
	 \\

uninstall:
	$(RM) $(bin_dir)/$(OUTFILE)
	$(RM) $(lib_dir)/libzdoom-x.so
	$(RM) $(lib_dir)/libzdoom-svgalib.so
	$(RM) $(zdoomshare_dir)/zdoom.wad
	$(RM) $(zdoomshare_dir)/bots.cfg
	$(RM) $(zdoomshare_dir)/railgun.bex
	$(RMDIR) $(zdoomshare_dir)
	$(RM) $(doc_dir)/*
	$(RMDIR) $(doc_dir)

xkeys:
	$(CXX) other/genkeys.c -o genkeys -lX11 -L$(X_libs) -lX11
	genkeys > .keys
	$(INSTALL) -d $(zdoomshare_dir)
	$(INSTALL) -m 644 .keys $(KEYMAP_FILE)
	$(RM) .keys
	$(RM) genkeys
	$(ECHO) >/dev/null \
	 \\\
	 \\-------------------------------------------------------------\
	 \\ Remember, if you did not use a QWERTY keymap, you will need \
	 \\ to switch to it and "make xkeys" again.                     \
	 \\-------------------------------------------------------------\
	 \\

$(INTDIR)/$(OUTFILE): $(SOURCES:.cpp=.d) $(OBJS)
	$(CXX) $(LDFLAGS) $(OBJS) -o $@ $(LIBS)

hwlibs: $(INTDIR)/libzdoom-svgalib.so $(INTDIR)/libzdoom-x.so

$(INTDIR)/libzdoom-svgalib.so: $(HWSOURCES:.cpp=.d) $(INTDIR)/hw_svgalib.o
	$(LD) -shared $(LDFLAGS_$(mode)) $(INTDIR)/hw_svgalib.o -o \
		$(INTDIR)/libzdoom-svgalib.so -lvga -lconsole -lctutils

$(INTDIR)/hw_svgalib.o: $(IMPDIR)/hw_svgalib.cpp
	$(CXX) $(CPPFLAGS) $(SCPPFLAGS) -c $< -o $@

$(INTDIR)/libzdoom-x.so: $(HWSOURCES:.cpp=.d) $(INTDIR)/hw_x.o
	$(LD) -shared $(LDFLAGS_$(mode)) $(INTDIR)/hw_x.o -o \
		$(INTDIR)/libzdoom-x.so -lHermes $(HW_X_LDFLAGS)

$(INTDIR)/hw_x.o: $(IMPDIR)/hw_x.cpp
	$(CXX) $(CPPFLAGS) $(SCPPFLAGS) $(HW_X_CPPFLAGS) -c $< -o $@

$(INTDIR)/%.o: $(src_dir)/%.cpp
	$(CXX) $(CPPFLAGS) -c $< -o $@

$(INTDIR)/%.o: $(src_dir)/%.nas
	$(AS) $(ASFLAGS) -o $@ $<

$(INTDIR)/%.o: $(IMPDIR)/%.nas
	$(AS) $(ASFLAGS) -o $@ $<

$(INTDIR)/%.o: $(IMPDIR)/%.cpp
	$(CXX) $(CPPFLAGS) -c $< -o $@

%.d: %.cpp
	$(SHELL) -ec '$(CXX) -MM $(CPPFLAGS_common) $(DEFS_common) $< \
		      | sed '\''s/\($(*F)\)\.o[ :]*/$$\(INTDIR\)\/\1.o $(subst /,\/,$(@D))\/$(@F) : /g'\'' > $@; \
		      [ -s $@ ] || rm -f $@'

include $(ALLSOURCES:.cpp=.d)

# Create a binary tarball
bintar: install
	$(TAR) -cv \
		$(bin_dir)/$(OUTFILE) \
		$(lib_dir)/libzdoom-x.so \
		$(lib_dir)/libzdoom-svgalib.so \
		$(zdoomshare_dir)/zdoom.wad \
		$(zdoomshare_dir)/xkeys \
		$(zdoomshare_dir)/bots.cfg \
		$(zdoomshare_dir)/railgun.bex \
		$(doc_dir)/* \
	| $(GZIP) -9c > $(BINTAR)

# Create a source tarball
srctar: $(ALLSOURCES) Makefile $(SRCDOC) docs/* other/*
	$(SHELL) -ec 'cd ..; \
		$(TAR) -cv --no-recursion --exclude=*.d --exclude=*~\
			$(basename)/Makefile \
			$(basename)/docs/* \
			$(basename)/other/* \
			$(basename)/$(src_dir)/* \
			$(basename)/$(src_dir)/linux/* \
			$(basename)/$(src_dir)/win32/* \
			$(basename)/$(src_dir)/osf1/* \
			$(basename)/$(src_dir)/djgpp/* \
			$(SRCDOC:%=$(basename)/%) \
		| $(GZIP) -9c > $(basename)/$(SRCTAR)'

