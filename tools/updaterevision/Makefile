ifeq (Windows_NT,$(OS))
  WIN=1
  WINCMD=1
endif
ifeq (msys,$(OSTYPE))
  WIN=1
  WINCMD=0
endif

CC = gcc
CFLAGS = -Os -Wall -fomit-frame-pointer
LDFLAGS = -s

ifeq (1,$(WIN))
  EXE = updaterevision.exe
else
  EXE = updaterevision
endif

CCDV = @../../ccdv

OBJS = updaterevision.o

all: $(EXE)

$(EXE): $(OBJS)
	$(CCDV) $(CC) -o $(EXE) $(OBJS) $(CFLAGS) $(LDFLAGS)

.PHONY: clean

clean:
ifeq (1,$(WINCMD))
	-del /q /f $(EXE) 2>nul
	-del /q /f *.o 2>nul
else
	-rm -f $(EXE)
	-rm -f *.o
endif
