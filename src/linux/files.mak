####################################################################
#
#  Linux-specific Makefile stuff.
#
####################################################################

KEYMAP_FILE = $(zdoomshare_dir)/xkeys

DEFS_common = -Wp,-Dstricmp=strcasecmp -Wp,-Dstrnicmp=strncasecmp \
	-Wp,-DNO_STRUPR -Wp,-DNO_FILELENGTH -Wp,-DKEYMAP_FILE=\"$(KEYMAP_FILE)\" \
	-Wp,-DSHARE_DIR=\"$(zdoomshare_dir)/\" -Wp,-DPLATFORM_LINUX
ASFLAGS_common = -f elf -DM_TARGET_LINUX
#LDFLAGS_common = -rdynamic -Xlinker -rpath -Xlinker $(lib_dir)
LDFLAGS_common = 
SCPPFLAGS = -fpic -shared

# options for the X driver
HW_X_CPPFLAGS = -DUSE_SHM -DUSE_DGA -DUSE_VIDMODE
HW_X_LDFLAGS = -L$(X_libs) -lX11 -lXext -lXxf86dga -lXxf86vm

# libraries to link with
#LIBS = -lm -ldl -lpthread midas/lib/linux/gcretail/libmidas.a
LIBS = -lm /home/randy/zdoom-1.23/fmod/api/libfmod-3.33.so -lz -lSDL -lpthread

SYSINT = $(INTDIR)/$(SYSTEM)

SYSOBJS = \
	$(SYSINT)/i_main.o \
	$(SYSINT)/i_net.o \
	$(SYSINT)/i_system.o \
	$(SYSINT)/i_cd.o \
	$(SYSINT)/i_input.o \
	$(SYSINT)/hardware.o \
	$(SYSINT)/sdlvideo.o \
	$(SYSINT)/expandorama.o

SYSSOURCES = \
	$(IMPDIR)/i_main.cpp \
	$(IMPDIR)/i_net.cpp \
	$(IMPDIR)/i_system.cpp \
	$(IMPDIR)/i_cd.cpp \
	$(IMPDIR)/i_input.cpp \
	$(IMPDIR)/hardware.cpp \
	$(IMPDIR)/sdlvideo.cpp \
	$(IMPDIR)/expandorama.nas

install: $(INTDIR)/$(OUTFILE)
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
