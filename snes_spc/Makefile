# Makefile for snes_spc, derived from zlib/Makefile.mgw.

ifeq (Windows_NT,$(OS))
  CMD=1
endif
ifeq ($(findstring msys,$(shell sh --version 2>nul)),msys)
  CMD=0
endif

STATICLIB = libsnes_spc.a

CCDV = @../ccdv

CC = gcc
CFLAGS = $(LOC) -O3 -Wall -fomit-frame-pointer

LD = $(CC)
LDFLAGS = $(LOC) -s

AR = ar
ARFLAGS = rcs

OBJS = snes_spc/dsp.o snes_spc/SNES_SPC.o snes_spc/SNES_SPC_misc.o snes_spc/SNES_SPC_state.o \
	snes_spc/spc.o snes_spc/SPC_DSP.o snes_spc/SPC_Filter.o

all: $(STATICLIB)

.cpp.o:
	$(CCDV) $(CC) $(CFLAGS) -c -o $@ $<

$(STATICLIB): $(OBJS)
	$(CCDV) $(AR) $(ARFLAGS) $@ $(OBJS)


.PHONY: clean

clean:
ifeq (0,$(CMD))
	rm -f $(STATICLIB)
	rm -f snes_spc/*.o
else
	-del /q /f $(STATICLIB) 2>nul
	-del /q /f snes_spc\*.o 2>nul
endif

dsp.o: snes_spc/dsp.cpp snes_spc/dsp.h snes_spc/SPC_DSP.h
SNES_SPC.o: snes_spc/SNES_SPC.cpp snes_spc/SNES_SPC.h snes_spc/SPC_DSP.h
SNES_SPC_misc.o: snes_spc/SNES_SPC_misc.cpp snes_spc/SNES_SPC.h snes_spc/SPC_DSP.h
SNES_SPC_state.o: snes_spc/SNES_SPC_state.cpp snes_spc/SNES_SPC.h snes_spc/SPC_DSP.h
spc.o: snes_spc/spc.cpp snes_spc/spc.h snes_spc/SNES_SPC.h snes_spc/SPC_DSP.h snes_spc/SPC_Filter.h
SPC_DSP.o: snes_spc/SPC_DSP.cpp snes_spc/SPC_DSP.h
SPC_Filter.o: snes_spc/SPC_Filter.cpp snes_spc/SPC_Filter.h
