####################################################################
#
#  Makefile for ZDoom. This probably requires GNU Make.
#
####################################################################

CC = gcc -V3.0
CXX = g++ -V3.0
AS = nasm
RM = rm -f
RMTREE = rm -R
RMDIR = rmdir
CP = cp
LD = ld
INSTALL = install
ECHO = echo
TAR = tar
GZIP = gzip

basename = zdoom-1.23

# the OS type we are building for; should match a directory in src_dir
SYSTEM = linux

# distribution names
BINTAR = $(basename)-i586.tar.gz
SRCTAR = $(basename).tar.gz

# options common to all builds
prefix = /usr/local
lib_dir = $(prefix)/lib
bin_dir = $(prefix)/bin
doc_dir = $(prefix)/doc/zdoom
share_dir = $(prefix)/share
zdoomshare_dir = $(share_dir)/zdoom
X_libs = /usr/X11R6/lib
src_dir = src

IMPDIR = $(src_dir)/$(SYSTEM)
mode = release

# directory to hold all intermediate files
INTDIR = $(IMPDIR)/$(mode)

SRCDOC = INSTALL README
CPPFLAGS_common = -Wall -Winline -I$(src_dir) -I$(src_dir)/fmodsound -I$(IMPDIR) \
	-I$(src_dir)/g_shared -I$(src_dir)/g_doom -I$(src_dir)/g_raven \
	-I$(src_dir)/g_heretic -I$(src_dir)/g_hexen -Ifmod/api/inc
OUTFILE = zdoom

zdoom doom release all: $(INTDIR)/$(OUTFILE)

include $(IMPDIR)/files.mak

# options specific to the debug build
CPPFLAGS_debug = -g -Wp,-DRANGECHECK
LDFLAGS_debug =
ASFLAGS_debug = -g
DEFS_debug = -Wp,-DDEBUG,-DNOASM

# options specific to the release build
CPPFLAGS_release = -O2 -march=pentium -fomit-frame-pointer
LDFLAGS_release = -s
ASFLAGS_release =
DEFS_release = -Wp,-DNDEBUG,-DUSEASM

# select flags for this build
CPPFLAGS = $(CPPFLAGS_common) $(CPPFLAGS_$(mode)) \
	$(DEFS_common) $(DEFS_$(mode))
LDFLAGS = $(LDFLAGS_common) $(LDFLAGS_$(mode))
ASFLAGS = $(ASFLAGS_common) $(ASFLAGS_$(mode))

# directory to get dependency files
DEPSDIR = deps

# pull in common object and source files extracted from zdoom.dsp
include dspfiles

# object files
OBJS = $(DSPOBJS) $(SYSOBJS)

# all dependency directories
DEPSDIRS = $(DSPDEPSDIRS) $(DEPSDIR)/$(SYSTEM)

ALLSOURCES = $(DSPSOURCES) $(SOURCES) $(HWSOURCES)

DSPDEPS = $(patsubst $(src_dir)/%.cpp,$(DEPSDIR)/%.d, \
	  $(filter %.cpp %.c,$(DSPSOURCES)))
SYSDEPS = $(patsubst $(src_dir)/%.cpp,$(DEPSDIR)/%.d, \
	  $(filter %.cpp %.c,$(SYSSOURCES)))

# If the VC++ project file changes, remake our list of source files
dspfiles: $(src_dir)/zdoom.dsp util/cvdsp
	util/cvdsp $(src_dir)/zdoom.dsp > dspfiles
	
util/cvdsp: util/cvdsp.c
	$(CC) util/cvdsp.c -o util/cvdsp -s -O

debug:
	$(MAKE) mode=debug

clean:
	$(RMTREE) $(IMPDIR)/release
	$(RMTREE) $(IMPDIR)/debug
	$(RMTREE) $(DEPSDIR)

$(INTDIR)/$(OUTFILE): common sysspecific
	$(CXX) $(LDFLAGS) $(OBJS) -o $@ $(LIBS)

common: $(DSPINTDIRS) $(DSPDEPS) $(DSPOBJS)
	
sysspecific: $(INTDIR)/$(SYSTEM) $(SYSDEPS) $(SYSOBJS)

# Compile .cpp files
$(INTDIR)/%.o: $(src_dir)/%.cpp
	$(CXX) $(CPPFLAGS) -c $< -o $@

# Assemble .nas files
$(INTDIR)/%.o: $(src_dir)/%.nas
	$(AS) $(ASFLAGS) -o $@ $<

# Make intermediate directories
$(INTDIR):
	mkdir -p $@
	
$(INTDIR)/%:
	mkdir -p $@

#$(INTDIR)/%.o: $(IMPDIR)/%.nas
#	$(AS) $(ASFLAGS) -o $@ $<

#$(INTDIR)/%.o: $(IMPDIR)/%.cpp
#	$(CXX) $(CPPFLAGS) -c $< -o $@

# Create dependency files
$(DEPSDIR)/%.d: $(src_dir)/%.cpp
	mkdir -p $(DEPSDIRS)
	$(SHELL) -ec '$(CXX) -MM $(CPPFLAGS_common) $(DEFS_common) $< \
		      | sed '\''s/\($(*F)\)\.o[ :]*/$$\(INTDIR\)\/\1.o $(subst /,\/,$(@D))\/$(@F) : /g'\'' > $@; \
		      [ -s $@ ] || rm -f $@'

include $(DSPDEPS)
include $(SYSDEPS)

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
			$(basename)/$(src_dir)/fmodsound/* \
			$(basename)/$(src_dir)/g_heretic/* \
			$(basename)/$(src_dir)/g_raven/* \
			$(basename)/$(src_dir)/g_doom/* \
			$(basename)/$(src_dir)/g_hexen/* \
			$(basename)/$(src_dir)/g_shared/* \
			$(SRCDOC:%=$(basename)/%) \
		| $(GZIP) -9c > $(basename)/$(SRCTAR)'

srcbz2: srctar
	$(GZIP) -cd $(SRCTAR) | bzip2 -9c > $(basename).tar.bz2
