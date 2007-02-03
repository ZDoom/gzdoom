ifeq (Windows_NT,$(OS))
  WIN=1
  WINCMD=1
endif
ifeq (msys,$(OSTYPE))
  WIN=1
  WINCMD=0
endif

ifeq (1,$(WIN))
  EXE = fixrtext.exe
  CFLAGS = $(LOC) -D_WIN32 -Os -Wall -fomit-frame-pointer
else
# This is only useful for Win32 builds.
endif

CCDV = @../../ccdv
CC = gcc
LDFLAGS = -s

all: $(EXE)

$(EXE): fixrtext.c
	$(CCDV) $(CC) $(LDFLAGS) -o $(EXE) fixrtext.c

.PHONY: clean

clean:
ifeq (1,$(WINCMD))
	-del /q /f $(EXE) 2>nul
else
	rm -f $(EXE)
endif
