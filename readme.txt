This is the source code for ZDoom 1.12 released on 7 April 1998. If you wish to
obtain the original Linux source code, you can find it at
ftp://ftp.idsoftware.com/source/doomsrc.zip.

Visual C++ 5.0 was used to compile this program, although it shouldn't be too
big a deal to use another compiler.

To recreate my environment, extract the files in this archive into your Doom
directory, being sure to preserve the full path names of these files. You
should also use a program that supports long file names such as WinZip.

You also need the MIDAS Digital Audio System. This can be found at
http://www.s2.org/MIDAS/. I have it in a Doom/Midas directory. If you put it
somewhere else, you will need to edit the i_music.c and i_sound.c files to make
them look for the header files in the right place.

NASM is used to assemble the misc.nas and tmap.nas files. You can get it at
http://www.cryogen.com/Nasm/. The custom build settings for the *.nas files
expect NASM to be in a D:\nasm directory, so if you have it somewhere else, you
will need to modify those settings as well. If for some reason you don't want to
use the assembly routines when you compile ZDoom, you can use the -DNOASM parameter
when you compile to disable support for assembly routines. Doing so can have a big
performance hit, especially at higher resolutions, so I don't recommend it. NASM is
free, so why not use it?

The file rh-log.txt in the code directory now lists my adventures with the source
code on a mostly day-by-day basis. This file only goes back to the time when I
released 1.11, because that's when I created this file.

Randy Heit
rheit@usa.net