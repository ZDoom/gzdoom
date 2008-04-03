ifeq (Windows_NT,$(OS))
  WIN=1
  WINCMD=1
  ifeq ($(findstring msys,$(shell sh --version 2>nul)),msys)
    WINCMD=0
  endif
endif

CC = gcc
CFLAGS = -Os -Wall -fomit-frame-pointer
LDFLAGS = -s

OBJS = updaterevision.o

ifeq (1,$(WIN))
  EXE = updaterevision.exe
  OBJS += trustinfo.o
else
  EXE = updaterevision
endif

CCDV = @../../ccdv

all: $(EXE)

$(EXE): $(OBJS)
	$(CCDV) $(CC) -o $(EXE) $(OBJS) $(CFLAGS) $(LDFLAGS)

%.o : %.rc
	$(CCDV) windres -o $@ -i $<

.PHONY: clean

clean:
ifeq (1,$(WINCMD))
	-del /q /f $(EXE) 2>nul
	-del /q /f *.o 2>nul
else
	-rm -f $(EXE)
	-rm -f *.o
endif
