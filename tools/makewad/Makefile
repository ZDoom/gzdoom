ifeq (Windows_NT,$(OS))
  EXE = makewad.exe
  CCDV = @../../ccdv
  CFLAGS = -Os -Wall -fomit-frame-pointer
  LDFLAGS = -L../../zlib/ -lz
else
  EXE = makewad
  CCDV = 
  CFLAGS = -Dstricmp=strcasecmp -Dstrnicmp=strncasecmp -Os -Wall -fomit-frame-pointer
  LDFLAGS = -lz
endif

CC = gcc

OBJS = makewad.o ioapi.o zip.o

all: $(EXE)

.c.o:
	$(CCDV) $(CC) $(CFLAGS) -c -o $@ $<

$(EXE): $(OBJS)
	$(CCDV) $(CC) $(LDFLAGS) -o $(EXE) $(OBJS)

.PHONY: clean

clean:
ifeq (Windows_NT,$(OS))
	-del /q /f $(EXE) 2>nul
	-del /q /f *.o 2>nul
else
	-rm -f $(EXE)
	-rm -f *.o
endif
