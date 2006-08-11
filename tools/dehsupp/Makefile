ifeq (Windows_NT,$(OS))
  WIN=1
  WINCMD=1
endif
ifeq (msys,$(OSTYPE))
  WIN=1
  WINCMD=0
endif

ifeq (1,$(WIN))
  EXE = dehsupp.exe
  CFLAGS = $(LOC) -Os -Wall -fomit-frame-pointer
else
  EXE = dehsupp
  CFLAGS = -Dstricmp=strcasecmp -Dstrnicmp=strncasecmp -Os -Wall -fomit-frame-pointer
endif

CCDV = @../../ccdv
CC = gcc
LDFLAGS = -s

RE2C = ../re2c/re2c
LEMON = ../lemon/lemon
LEMONFLAGS =
RE2CFLAGS = -s

OBJS = dehsupp.o parse.o scanner.o

all: $(EXE)

$(EXE): $(OBJS)
	$(CCDV) $(CC) $(LDFLAGS) -o $(EXE) $(OBJS)

.PHONY: clean

clean:
ifeq (1,$(WINCMD))
	-del /q /f $(EXE) 2>nul
	-del /q /f *.o 2>nul
else
	rm -f $(EXE)
	rm -f *.o
endif

%.o: %.c
	$(CCDV) $(CC) $(CFLAGS) -c -o $@ $<

parse.h parse.c: parse.y
	$(CCDV) $(LEMON) $(LEMONFLAGS) $<
	
scanner.c: scanner.re
	$(CCDV) $(RE2C) $(RE2CFLAGS) -o $@ $<

parse.tab.c: parse.y
	$(CCDV) $(BISON) $(BISONFLAGS) -o $@ $<
