This is the source code for ZDoom 1.20 released on 25 November 1999.

If you want to compile this under linux, you should have gotten the
archive zdoom-1.19.tar.gz instead. It contains everything included here,
plus the files necessary for supporting Linux.

It is based on the Linux DOOM sources that were prepared by B. Krenheimer
and generously released by John Carmack shortly before Christmas, 1997. If
you wish to obtain the original Linux source distribution, you can find it
at ftp://ftp.idsoftware.com/source/doomsrc.zip. Portions of code were also
taken from various other source ports, the majority of them coming from
the BOOM Phase I source released on 27 May 1998 (credit Team TNT and Chi
Hoang). There's even stuff from Hexen (though not much) in there. Many
changes are, of course, my own, and I've tried to flag them as such with
[RH] comments blocks (although I missed a few before I started the
commenting convention).

The file rh-log.txt in the code directory lists my adventures with the
source code on a mostly day-by-day basis. This file only goes back to the
time when I released 1.11, because that's when I created this file. Most of
the other text files in the code directory are from the original Linux
source code and do not accurately reflect the state of many of the files
used now.

If you do use these sources, you should also be familiar with
docs/doomlic.txt. This is the license agreement for the Doom sources. 

To compile this source code, you also need to download several other
packages if you don't already have them. These are:

  MIDAS Digital Audio System
    http://www.s2.org/midas/

    Be sure to link with the static library and not the DLL version of
    MIDAS! I use a hack from NTDOOM that crashes with the DLL but not the
    static library.

  OpenPTC 1.0.18
    http://www.gaffer.org/ptc

  NASM (for the assembly files)
    http://www.web-sites.co.uk/nasm/

    If you don't want to use NASM, you can #define NOASM while compiling
    ZDoom, and it will use C code instead.

The included project file (doom.dsp) is for Visual C++ 6 and makes a few
assumptions about the development environment:

    MIDAS is installed at the same location in the directory hierarchy as
    the ZDoom source code. In my case, I have the source in
    d:/games/doom/code, so MIDAS is in d:/games/doom/midas. I_music.c and
    I_sound.c also look for MIDAS include files in the "../midas"
    directory.

    NASM is installed in d:/nasm.

    PTC is installed in ../openptc.

If you want to put things in different places, you'll need to adjust the
project file's settings accordingly.

This code should also compile with Watcom C 10.6 (and presumably newer
versions as well). If you use Watcom, please tell me if the following NASM
line generates object files for tmap.nas and misc.nas that are usable with
the Watcom linker:

    nasm -o file.obj -f obj -d M_TARGET_WATCOM file.nas

(The line I use is "nasm -o file.obj -f win32 file.nas" which works with
Microsoft's linker, but not Watcom's.)

The old DOS code from 1.17c is provided, although it won't compile as-is,
because OpenPTC for DOS still only supports text- and fake-mode graphics,
and I don't care enough about DOS to hook the older version of PTC in with
the new code. If someone else wants to mess with it and get it to work, be
my guest.


Randy Heit
rheit@iastate.edu