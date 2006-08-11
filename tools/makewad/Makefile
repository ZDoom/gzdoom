ifeq (Windows_NT,$(OS))
  WIN=1
  WINCMD=1
endif
ifeq (msys,$(OSTYPE))
  WIN=1
  WINCMD=0
endif

ifeq (1,$(WIN))
  EXE = makewad.exe
  CFLAGS = -Os -Wall -fomit-frame-pointer
  LDFLAGS = -s -L../../zlib/ -lz
  ZLIB = ../../zlib/libz.a
else
  EXE = makewad
  CFLAGS = -Dstricmp=strcasecmp -Dstrnicmp=strncasecmp -Os -Wall -fomit-frame-pointer
  LDFLAGS = -s -lz
  ZLIB =
endif

CCDV = @../../ccdv
CC = gcc

OBJS = makewad.o ioapi.o zip.o

all: $(EXE)

.c.o:
	$(CCDV) $(CC) $(CFLAGS) -c -o $@ $<

$(EXE): $(OBJS) $(ZLIB)
	$(CCDV) $(CC) -o $(EXE) $(OBJS) $(LDFLAGS)

ifeq (1,$(WIN))
$(ZLIB):
	make -C ../../zlib -f Makefile.mgw
endif

.PHONY: clean

clean:
ifeq (1,$(WINCMD))
	-del /q /f $(EXE) 2>nul
	-del /q /f *.o 2>nul
else
	-rm -f $(EXE)
	-rm -f *.o
endif
