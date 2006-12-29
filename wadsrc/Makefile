# NMAKE doesn't know how to handle executables that use forward slashes in
# their paths, but it can handle forward slashes in dependencies. Heh.
# The wadsrc project file redefines MAKEWAD using backslashes instead. It
# also defines NOLOGO, so you don't need to see NMAKE's banner.
#
# Summary: If you use Visual C++, the provided project file is set up to
# build the wad correctly with nmake. If you use GCC, then "make" is
# sufficient to build the wad.

RM=rm -f
SLASH=/
MAKEWAD=..$(SLASH)tools$(SLASH)makewad$(SLASH)makewad
XLATCC=..$(SLASH)tools$(SLASH)xlatcc$(SLASH)xlatcc
DEHSUPP=..$(SLASH)tools$(SLASH)dehsupp$(SLASH)dehsupp

makethewad: wadmake Makefile2
	$(MAKE) $(NOLOGO) XLATCC=$(XLATCC) DEHSUPP=$(DEHSUPP) -f Makefile2

wadmake: zdoom.lst
	$(MAKEWAD) -make wadmake zdoom.lst

clean:
	$(RM) wadmake zdoom.pk3 xlat$(SLASH)*.x dehsupp.lmp

# This target is for Visual C++'s Rebuild All command
nrebuild: clean
	$(MAKE) NOLOGO=$(NOLOGO) SLASH=$(SLASH)
	copy zdoom.pk3 ..\..

# Copy the wad to my testing area
andcopy: makethewad
	copy zdoom.pk3 ..\..
