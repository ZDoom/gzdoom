ifeq (Windows_NT,$(OS))
  EXE = dehsupp.exe
  CCDV = @../../ccdv
  CFLAGS = $(LOC) -Os -Wall -fomit-frame-pointer
else
  EXE = dehsupp
  CCDV =
  CFLAGS = -Dstricmp=strcasecmp -Dstrnicmp=strncasecmp -Os -Wall -fomit-frame-pointer
endif

CC = gcc

BISON = bison
BISONFLAGS = 

OBJS = parse.tab.o

all: $(EXE)

$(EXE): $(OBJS)
	$(CCDV) $(CC) -o $(EXE) $(OBJS)

.PHONY: clean

clean:
ifeq (Windows_NT,$(OS))
	-del /q /f $(EXE) 2>nul
	-del /q /f *.o 2>nul
else
	rm -f $(EXE)
	rm -f *.o
endif

parse.tab.o: parse.tab.c
	$(CCDV) $(CC) $(CFLAGS) -c -o $@ $<
	
parse.tab.c: parse.y
	$(CCDV) $(BISON) $(BISONFLAGS) -o $@ $<
