This is the source code for ZDoom 1.14 released on 14 July 1998.

It is based on the Linux DOOM sources that were prepared by B. Krenheimer
and generously released by John Carmack shortly before Christmas, 1997. If
you wish to obtain the original Linux source distribution, you can find it
at ftp://ftp.idsoftware.com/source/doomsrc.zip. Portions of code were also
taken from various other source ports, the majority of them coming from
the BOOM Phase I source released on 27 May 1998 (credit Team TNT and Chi
Hoang). Many changes are, of course, my own, and I've tried to flag them as
such with [RH] comments blocks (although I missed a few before I started
the commenting convention).

The file rh-log.txt in the code directory lists my adventures with the
source code on a mostly day-by-day basis. This file only goes back to the
time when I released 1.11, because that's when I created this file. Most of
the other text files in the code directory are from the original Linux
source code and do not accurately reflect the state of many of the files
used now.

If you do use these sources, you should also be familiar with doomlic.txt.
This is the license agreement for the Doom sources. The file COPYING
applies to the patch program and NOT the Doom source code. I'd also
appreciate a short e-mail so that I know if anyone's actually using
this. :-)

To compile this source code, you also need to download several other
packages if you don't already have them. These are:

  MIDAS Digital Audio System
    http://www.s2.org/MIDAS/

    Be sure to link with the static library and not the DLL version of
    MIDAS! I use a hack from NTDOOM that crashes with the DLL but not the
    static library.

  Prometheus Truecolor Library (PTC)
    http://www.gaffer.org/ptc

    A special note about PTC is that the version available at gaffer.org
    will not work unmodified with ZDoom. A context diff has been provided
    along with a copy of GNU patch to generate a version of PTC that works
    with ZDoom. To use it, you need to download the source code for PTC
    0.73b (available by following the Developers link from the above page).
    Extract it to some directory (in my case e:/ptc/source), then using a
    DOS window, CD to that directory and apply the ptc.diff file using the
    command line:

        patch < ptc.diff

    Then you'll need to build at least the release build of PTC. The
    batch files provided with PTC to do this will generate the library
    somewhere in ../library. In my case, this ends up being
    e:/ptc/library/win32/vc5.x.

  NASM (for the assembly files)
    http://www.cryogen.com/Nasm

    If you don't want to use NASM, you can #define NOASM while compiling
    ZDoom, and it will use C code instead.

The included project file (doom.dsp) is for Visual C++ 5.x and makes a few
assumptions about the development environment:

    MIDAS is installed at the same location in the directory hierarchy as
    the ZDoom source code. In my case, I have the source in
    f:/games/doom/code, so MIDAS is in f:/games/doom/midas. I_music.c and
    I_sound.c also look for MIDAS include files in the "../midas"
    directory.

    NASM is installed in d:/nasm.

    PTC is installed in e:/ptc.

If you want to put things in different places, you'll need to adjust the
project file accordingly.

The file doom.mak is what VC++ generated when I told it to "Export
Makefile...." I have no idea if it's of any use to anyone, but if you don't
have VC++ 5.x, it may be worthwhile to try adapting it to your compiler.

This code should also compile with Watcom C 10.6 (and presumably newer
versions as well). If you use Watcom, please tell me if the following NASM
line generates object files far tmap.nas and misc.nas that are usable with
the Watcom linker:

    nasm -o file.obj -f obj -d M_TARGET_WATCOM file.nas

(The line I use is "nasm -o file.obj -f win32 file.nas" which works with
Microsoft's linker, but not Watcom's.)



Randy Heit
rheit@usa.net