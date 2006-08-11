ifeq (Windows_NT,$(OS))
  WIN=1
  WINCMD=1
endif
ifeq (msys,$(OSTYPE))
  WIN=1
  WINCMD=0
endif

ifeq (1,$(WIN))
	EXE = re2c.exe
	CXXFLAGS = $(LOC) -D_WIN32 -DNDEBUG -Os -Wall -Wno-unused
else
	EXE = re2c
	CXXFLAGS = -DHAVE_CONFIG_H -DNDEBUG -Os -Wall -Wno-unused
endif

CCDV = @../../ccdv
CXX = g++
LDFLAGS= -s

OBJS = actions.o code.o dfa.o main.o mbo_getopt.o parser.o scanner.o substr.o translate.o

all: $(EXE)

.cc.o:
	$(CCDV) $(CXX) $(CXXFLAGS) -c -o $@ $<

$(EXE): $(OBJS)
	$(CCDV) $(CXX) $(LDFLAGS) -o $(EXE) $(OBJS)

.PHONY: clean

clean:
ifeq (1,$(WINCMD))
	-del /q /f $(EXE) 2>nul
	-del /q /f *.o 2>nul
else
	rm -f $(EXE)
	rm -f *.o
endif

actions.o: actions.cc
code.o: code.cc
dfa.o: dfa.cc
main.o: main.cc
mbo_getopt.o: mbo_getopt.cc
parser.o: parser.cc
scanner.o: scanner.cc
substr.o: substr.cc
translate.o: translate.cc
