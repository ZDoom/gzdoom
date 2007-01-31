ifeq (Windows_NT,$(OS))
  WIN=1
  WINCMD=1
endif
ifeq (msys,$(OSTYPE))
  WIN=1
  WINCMD=0
endif

ifeq (1,$(WIN))
  EXE = xlatcc.exe
  CFLAGS = $(LOC) -Os -Wall -fomit-frame-pointer
else
  EXE = xlatcc
  CFLAGS = -Dstricmp=strcasecmp -Dstrnicmp=strncasecmp $(LOC) -Os -Wall -fomit-frame-pointer
endif

CCDV = @../../ccdv
CC = gcc
LDFLAGS= -s

LEMON = ../lemon/lemon
LEMONFLAGS =

OBJS = gen.o xlat-parse.o

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

xlat-parse.o: xlat-parse.c xlat.h xlat-parse.h
	$(CCDV) $(CC) $(CFLAGS) -c -o $@ $<

xlat-parse.c xlat-parse.h: xlat-parse.y
	$(CCDV) $(LEMON) $(LEMONFLAGS) $<
   
gen.o: gen.c xlat.h
	$(CCDV) $(CC) $(CFLAGS) -c -o $@ $<
