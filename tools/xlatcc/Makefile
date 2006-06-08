ifeq (Windows_NT,$(OS))
  EXE = xlatcc.exe
  CCDV = @../../ccdv
  CFLAGS = $(LOC) -Os -Wall -fomit-frame-pointer
else
  EXE = xlatcc
  CCDV = @../../ccdv
  CFLAGS = -Dstricmp=strcasecmp -Dstrnicmp=strncasecmp $(LOC) -Os -Wall -fomit-frame-pointer
endif

CC = gcc
LDFLAGS= -s

BISON = bison
BISONFLAGS = 

OBJS = gen.o xlat-parse.tab.o

all: $(EXE)

$(EXE): $(OBJS)
	$(CCDV) $(CC) $(LDFLAGS) -o $(EXE) $(OBJS)

.PHONY: clean

clean:
ifeq (Windows_NT,$(OS))
	-del /q /f $(EXE) 2>nul
	-del /q /f *.o 2>nul
else
	rm -f $(EXE)
	rm -f *.o
endif

xlat-parse.tab.o: xlat-parse.tab.c xlat.h
	$(CCDV) $(CC) $(CFLAGS) -c -o $@ $<

xlat-parse.tab.c: xlat-parse.y
	$(CCDV) $(BISON) $(BISONFLAGS) -o $@ $<
   
gen.o: gen.c xlat.h
	$(CCDV) $(CC) $(CFLAGS) -c -o $@ $<
