ifeq (Windows_NT,$(OS))
	EXE = re2c.exe
	CCDV = @../../ccdv
	CXXFLAGS = $(LOC) -D_WIN32 -DNDEBUG -Os -Wall -Wno-unused
else
	EXE = re2c
	CCDV = @../../ccdv
	CXXFLAGS = -DHAVE_CONFIG_H -DNDEBUG -Os -Wall -Wno-unused
endif

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
ifeq (Windows_NT,$(OS))
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
