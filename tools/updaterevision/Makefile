CC = gcc
CFLAGS = -Os -Wall -fomit-frame-pointer
LDFLAGS = -s

ifeq (Windows_NT,$(OS))
  EXE = updaterevision.exe
  CCDV = @../../ccdv
else
  EXE = updaterevision
  CCDV = @../../ccdv
  CFLAGS = -Os -Wall -fomit-frame-pointer
endif

OBJS = updaterevision.o

all: $(EXE)

$(EXE): $(OBJS)
	$(CCDV) $(CC) -o $(EXE) $(OBJS) $(CFLAGS) $(LDFLAGS)

.PHONY: clean

clean:
ifeq (Windows_NT,$(OS))
	-del /q /f $(EXE) 2>nul
	-del /q /f *.o 2>nul
else
	-rm -f $(EXE)
	-rm -f *.o
endif
