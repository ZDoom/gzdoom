# Makefile for DUMB, derived from zlib/Makefile.mgw.
# Yes, I have forgone the Makefiles provided with DUMB and opted to roll a simpler one.

CMD=0
ifeq (Windows_NT,$(OS))
  CMD=1
  ifeq ($(findstring msys,$(shell sh --version 2>nul)),msys)
    CMD=0
  endif
endif

CCDV = @../ccdv

CC = gcc
CFLAGS = $(LOC) -O3 -Wall -fomit-frame-pointer

LD = $(CC)
LDFLAGS = $(LOC) -s

AR = ar
ARFLAGS = rcs

# Macro for replacing / with \ where necessary. Usage: $(call FIX,path)
FIX = $(subst /,\,$(subst /*,\\\*,$(1)))

CORE_MODULES :=            \
    core/atexit.c          \
    core/duhlen.c          \
    core/duhtag.c          \
    core/dumbfile.c        \
    core/loadduh.c         \
    core/makeduh.c         \
    core/rawsig.c          \
    core/readduh.c         \
    core/register.c        \
    core/rendduh.c         \
    core/rendsig.c         \
    core/unload.c          \
    helpers/barray.c       \
    helpers/clickrem.c     \
    helpers/memfile.c      \
    helpers/resample.c     \
    helpers/riff.c         \
    helpers/sampbuf.c      \
    helpers/silence.c      \
    helpers/stdfile.c      \
    it/filter.cpp          \
    it/itload.c            \
    it/itload2.c           \
    it/itmisc.c            \
    it/itorder.c           \
    it/itread.c            \
    it/itread2.c           \
    it/itrender.c          \
    it/itunload.c          \
    it/load669.c           \
    it/load6692.c          \
    it/loadasy.c           \
    it/loadasy2.c          \
    it/loadmod.c           \
    it/loadmod2.c          \
    it/loadmtm.c           \
    it/loadmtm2.c          \
    it/loadoldpsm.c        \
    it/loadoldpsm2.c       \
    it/loadpsm.c           \
    it/loadpsm2.c          \
    it/loadptm.c           \
    it/loadptm2.c          \
    it/loadriff.c          \
    it/loadriff2.c         \
    it/loads3m.c           \
    it/loads3m2.c          \
    it/loadstm.c           \
    it/loadstm2.c          \
    it/loadxm.c            \
    it/loadxm2.c           \
    it/ptmeffect.c         \
    it/read669.c           \
    it/read6692.c          \
    it/readam.c            \
    it/readasy.c           \
    it/readdsmf.c          \
    it/readmod.c           \
    it/readmod2.c          \
    it/readmtm.c           \
    it/readoldpsm.c        \
    it/readpsm.c           \
    it/readptm.c           \
    it/readriff.c          \
    it/reads3m.c           \
    it/reads3m2.c          \
    it/readstm.c           \
    it/readstm2.c          \
    it/readxm.c            \
    it/readxm2.c           \
    it/xmeffect.c          

LIBDIR := lib
OBJDIR := obj

WFLAGS := -Wall -W -Wwrite-strings -Wstrict-prototypes -Wmissing-declarations -DDUMB_DECLARE_DEPRECATED
WFLAGS_ALLEGRO := -Wno-missing-declarations
OFLAGS := -O2 -ffast-math -fomit-frame-pointer
DBGFLAGS := -DDEBUGMODE=1 -g3

CFLAGS_RELEASE := -Iinclude $(WFLAGS) $(OFLAGS)
CFLAGS_DEBUG := -Iinclude $(WFLAGS) $(DBGFLAGS)

LDFLAGS := -s

CORE_LIB_FILE_RELEASE := $(LIBDIR)/libdumb.a
CORE_LIB_FILE_DEBUG := $(LIBDIR)/libdumbd.a

all: $(STATICLIB)

core: $(CORE_LIB_FILE_RELEASE) $(CORE_LIB_FILE_DEBUG)

.cpp.o:
	$(CCDV) $(CC) $(CFLAGS) -c -o $@ $<

$(STATICLIB): $(OBJS)
	$(CCDV) $(AR) $(ARFLAGS) $@ $(OBJS)


.PHONY: clean

clean:
ifeq (0,$(CMD))
	rm -f $(CORE_LIB_FILE_RELEASE)
	rm -f $(CORE_LIB_FILE_DEBUG)
	rm -f $(OBJDIR)/*.o
else
	-del /q /f $(call FIX,$(CORE_LIB_FILE_RELEASE)) 2>nul
	-del /q /f $(call FIX,$(CORE_LIB_FILE_DEBUG)) 2>nul
	-del /q /f $(call FIX,$(OBJDIR)/*.o) 2>nul
endif

CORE_OBJECTS := $(addprefix $(OBJDIR)/, $(notdir $(patsubst %.c, %.o, $(CORE_MODULES))))
ALLEGRO_OBJECTS := $(addprefix $(OBJDIR)/, $(notdir $(patsubst %.c, %.o, $(ALLEGRO_MODULES))))


# Pass the current value of CFLAGS through to the commands. Or, more
# accurately, create a local copy of the current CFLAGS variable. This is
# necessary because Make doesn't expand variables in commands until they are
# executed.
$(CORE_LIB_FILE): CFLAGS := $(CFLAGS)
$(ALLEGRO_LIB_FILE): CFLAGS := $(CFLAGS)


$(OBJDIR)/%.o: src/core/%.c include/dumb.h include/internal/dumb.h
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJDIR)/%.o: src/helpers/%.c include/dumb.h
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJDIR)/resample.o: src/helpers/resample.inc

$(OBJDIR)/%.o: src/it/%.c include/dumb.h include/internal/it.h
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJDIR)/%.o: src/sigtypes/%.c include/dumb.h
	$(CC) $(CFLAGS) -c -o $@ $<

$(CORE_LIB_FILE): $(CORE_OBJECTS)
	$(AR) $(ARFLAGS) $@ $^
