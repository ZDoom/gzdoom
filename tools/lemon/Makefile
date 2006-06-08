ifeq (Windows_NT,$(OS))
  EXE = lemon.exe
  CCDV = @../../ccdv
  CFLAGS = $(LOC) -D_WIN32 -Os -Wall -Wno-implicit -fomit-frame-pointer
else
  EXE = lemon
  CCDV = @../../ccdv
  CFLAGS = -Os -Wall -Wno-implicit -fomit-frame-pointer
endif

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
ifeq (Windows_NT,$(OS))
	-del /q /f $(EXE) 2>nul
	-del /q /f *.o 2>nul
else
	rm -f $(EXE)
	rm -f *.o
endif

lemon.o: lemon.c lempar.c

