ifeq (Windows_NT,$(OS))
  WIN=1
  WINCMD=1
  ifeq ($(findstring msys,$(shell sh --version 2>nul)),msys)
    WINCMD=0
  endif
endif

ifeq (1,$(WIN))
  EXE = lemon.exe
  CFLAGS = $(LOC) -D_WIN32 -Os -Wall -Wno-implicit -fomit-frame-pointer
else
  EXE = lemon
  CFLAGS = -Os -Wall -Wno-implicit -fomit-frame-pointer
endif

CCDV = @../../ccdv
CC = gcc
LDFLAGS = -s

OBJS = lemon.o

all: $(EXE)

$(EXE): $(OBJS)
	$(CCDV) $(CC) $(LDFLAGS) -o $(EXE) $(OBJS)

%.o: %.c
	$(CCDV) $(CC) $(CFLAGS) -c -o $@ $<

.PHONY: clean

clean:
ifeq (1,$(WINCMD))
	-del /q /f $(EXE) 2>nul
	-del /q /f *.o 2>nul
else
	rm -f $(EXE)
	rm -f *.o
endif

lemon.o: lemon.c lempar.c

