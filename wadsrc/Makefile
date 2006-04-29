# NMAKE doesn't know how to handle executables that use forward slashes in
# their paths, but it can handle forward slashes in dependencies. Heh.
# The wadsrc project file redefines MAKEWAD using backslashes instead. It
# also defines NOLOGO, so you don't need to see NMAKE's banner.
#
# Summary: If you use Visual C++, the provided project file is set up to
# build the wad correctly with nmake. If you use GCC, then "make" is
# sufficient to build the wad.

MAKEWAD=../tools/makewad/makewad
XLATCC=../tools/xlatcc/xlatcc
DEHSUPP=../tools/dehsupp/dehsupp
RM=rm -f

makethewad: wadmake Makefile2
	$(MAKE) $(NOLOGO) XLATCC=$(XLATCC) DEHSUPP=$(DEHSUPP) -f Makefile2

wadmake: zdoom.lst
	$(MAKEWAD) -make wadmake zdoom.lst

clean:
	$(RM) wadmake zdoom.pk3 xlat/*.x dehsupp.lmp

# This target is for Visual C++'s Rebuild All command
nrebuild: clean
	$(MAKE) NOLOGO=$(NOLOGO) MAKEWAD=$(MAKEWAD) XLATCC=$(XLATCC) DEHSUPP=$(DEHSUPP)
	copy zdoom.pk3 ..\..

# Copy the wad to my testing area
andcopy: makethewad
	copy zdoom.pk3 ..\..
