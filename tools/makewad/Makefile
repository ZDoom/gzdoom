ifeq (Windows_NT,$(OS))
  EXE = makewad.exe
  CCDV = @../../ccdv
  CFLAGS = -Os -Wall -fomit-frame-pointer
  LDFLAGS = -L../../zlib/ -lz
  ZLIB = ../../zlib/libz.a
else
  EXE = makewad
  CCDV = 
  CFLAGS = -Dstricmp=strcasecmp -Dstrnicmp=strncasecmp -Os -Wall -fomit-frame-pointer
  LDFLAGS = -lz
  ZLIB =
endif

CC = gcc

OBJS = makewad.o ioapi.o zip.o

all: $(EXE)

.c.o:
	$(CCDV) $(CC) $(CFLAGS) -c -o $@ $<

$(EXE): $(OBJS) $(ZLIB)
	$(CCDV) $(CC) -o $(EXE) $(OBJS) $(LDFLAGS)

ifeq (Windows_NT,$(OS))
$(ZLIB):
	make -C ../../zlib -f Makefile.mgw
endif

.PHONY: clean

clean:
ifeq (Windows_NT,$(OS))
	-del /q /f $(EXE) 2>nul
	-del /q /f *.o 2>nul
else
	-rm -f $(EXE)
	-rm -f *.o
endif
