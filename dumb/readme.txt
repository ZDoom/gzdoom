/*  _______         ____    __         ___    ___
 * \    _  \       \    /  \  /       \   \  /   /       '   '  '
 *  |  | \  \       |  |    ||         |   \/   |         .      .
 *  |  |  |  |      |  |    ||         ||\  /|  |
 *  |  |  |  |      |  |    ||         || \/ |  |         '  '  '
 *  |  |  |  |      |  |    ||         ||    |  |         .      .
 *  |  |_/  /        \  \__//          ||    |  |
 * /_______/ynamic    \____/niversal  /__\  /____\usic   /|  .  . ibliotheque
 *                                                      /  \
 *                                                     / .  \
 * readme.txt - General information on DUMB.          / / \  \
 *                                                   | <  /   \_
 *                                                   |  \/ /\   /
 *                                                    \_  /  > /
 *                                                      | \ / /
 *                                                      |  ' /
 *                                                       \__/
 */


********************
*** Introduction ***
********************


Thank you for downloading DUMB! You should have the following documentation:

   readme.txt    - This file
   licence.txt   - Conditions for the use of this software
   release.txt   - Release notes and changes for this and past releases
   docs/
     howto.txt   - Step-by-step instructions on adding DUMB to your project
     faq.txt     - Frequently asked questions and answers to them
     dumb.txt    - DUMB library reference
     deprec.txt  - Information about deprecated parts of the API
     ptr.txt     - Quick introduction to pointers for those who need it
     fnptr.txt   - Explanation of function pointers for those who need it
     modplug.txt - Our official position regarding ModPlug Tracker

This file will help you get DUMB set up. If you have not yet done so, please
read licence.txt and release.txt before proceeding. After you've got DUMB set
up, please refer to the files in the docs/ directory at your convenience. I
recommend you start with howto.txt.


****************
*** Features ***
****************


Here is the statutory feature list:

- Freeware

- Supports playback of IT, XM, S3M and MOD files

- Faithful to the original trackers, especially IT; if it plays your module
  wrongly, please tell me so I can fix the bug! (But please don't complain
  about differences between DUMB and ModPlug Tracker; see docs/modplug.txt)

- Accurate support for low-pass resonant filters for IT files

- Very accurate timing and pitching; completely deterministic playback

- Click removal

- Facility to embed music files in other files (e.g. Allegro datafiles)

- Three resampling quality settings: aliasing, linear interpolation and cubic
  interpolation

- Number of samples playing at once can be limited to reduce processor usage,
  but samples will come back in when other louder ones stop

- All notes will be present and correct even if you start a piece of music in
  the middle

- Fast seeking to any point before the music first loops (seeking time
  increases beyond this point)

- Audio generated can be used in any way; DUMB does not necessarily send it
  straight to a sound output system

- Makefile provided for DJGPP, MinGW, Linux, BeOS and Mac OS X; project file
  provided for MSVC 6 (please contact me if you'd like to submit or request
  support for a new platform; the code itself should port anywhere that has a
  32-bit C compiler)

- Can be used with Allegro, can be used without (if you'd like to help make
  DUMB more approachable to people who aren't using Allegro, please contact
  me)


*********************
*** What you need ***
*********************


To use DUMB, you need a 32-bit C compiler (GCC and MSVC are fine). If you
have Allegro, DUMB can integrate with its audio streams and datafiles, making
your life easier. If you do not wish to use Allegro, you will have to do some
work to get music playing back. The 'dumbplay' example program requires
Allegro.

   Allegro - http://alleg.sf.net/

Neil Walker has kindly uploaded some DUMB binaries at
http://retrospec.sgn.net/allegro/ . They may not always be up to date, so you
should try to compile it yourself first.


**********************************************
*** How to set DUMB up with DJGPP or MinGW ***
**********************************************


You should have got the .zip version. If for some reason you got the .tar.gz
version instead, you may have to convert make/config.bat to DOS text file
format. WinZip does this automatically by default. Otherwise, loading it into
MS EDIT and saving it again should do the trick. You will have to do the same
for any files you want to view in Windows Notepad. If you have problems, just
go and download the .zip instead.

Make sure you preserved the directory structure when you extracted DUMB from
the archive. Most unzipping programs will do this by default, but pkunzip
requires you to pass -d. If not, please delete DUMB and extract it again
properly.

If you are using Windows, open an MS-DOS Prompt or a Windows Command Line.
Change to the directory into which you unzipped DUMB.

Type the following:

   make

DUMB will ask you whether you wish to compile for DJGPP or MinGW. Then it
will ask you whether you want support for Allegro. (You have to have made and
installed Allegro's optimised library for this to work.) Finally, it will
compile optimised and debugging builds of DUMB, along with the example
programs. When it has finished, run the following to install the libraries:

   make install

All done! If you ever need the configuration again (e.g. if you compiled for
DJGPP before and you want to compile for MinGW now), run the following:

   make config

See the comments in the makefile for other targets.

Note: the makefile will only work properly if you have COMSPEC or ComSpec set
to point to command.com or cmd.exe. If you set it to point to a Unix-style
shell, the makefile won't work.

Please let me know if you have any trouble.

Scroll down for information on the example programs. Refer to docs/howto.txt
when you are ready to start programming with DUMB. If you use DUMB in a game,
let me know - I might decide to place a link to your game on DUMB's website!


******************************************************
*** How to set DUMB up with Microsoft Visual C++ 6 ***
******************************************************


You should have got the .zip version. If for some reason you got the .tar.gz
version instead, you may have to convert some files to DOS text file format.
WinZip does this automatically by default. Otherwise, loading such files into
MS EDIT and saving them again should do the trick. You will have to do this
for any files you want to view in Windows Notepad. If you have problems, just
go and download the .zip instead.

Make sure you preserved the directory structure when you extracted DUMB from
the archive. Most unzipping programs will do this by default, but pkunzip
requires you to pass -d. If not, please delete DUMB and extract it again
properly.

DUMB now comes with a project file for Microsoft Visual C++ 6. To add DUMB to
your project:

1. Open your project in VC++.
2. Select Project|Insert Project into Workspace...
3. Navigate to the dumb\vc6 directory, and select dumb.dsp.
4. Select Build|Set Active Configuration..., and reselect one of your
   project's configurations.
5. Select Project|Dependencies... and ensure your project is dependent on
   DUMB.
6. Select Project|Settings..., Settings for: All Configurations, C/C++ tab,
   Preprocessor category. Add the DUMB include directory to the Additional
   Include Directories box.
7. Ensure that for all the projects in the workspace (or more likely just all
   the projects in a particular dependency chain) the run-time libraries are
   the same. That's in Project|Settings, C/C++ tab, Code generation category,
   Use run-time library dropdown. The settings for Release and Debug are
   separate, so you'll have to change them one at a time. Exactly which run-
   time library you use will depend on what you need; it doesn't appear that
   DUMB has any particular requirements, so set it to whatever you're using
   now.

Good thing you only have to do all that once ...

If you have the Intel compiler installed, it will - well, should - be used to
compile DUMB. The only setting I added is /QxiM. This allows the compiler to
use PPro and MMX instructions, and so when compiling with Intel the resultant
EXE will require a Pentium II or greater. I don't think this is unreasonable.
After all, it is 2003 :)

If you don't have the Intel compiler, VC will compile DUMB as normal.

This project file and these instructions were provided by Tom Seddon (I hope
I got his name right; I had to guess it from his e-mail address!). They are
untested by me. If you have problems, check the download page at
http://dumb.sf.net/ to see if they are addressed; failing that, direct
queries to me and I'll try to figure them out.

When you are ready to start using DUMB, refer to docs/howto.txt. If you use
DUMB in a game, let me know - I might decide to place a link to your game on
DUMB's website!


********************************************************************
*** How to set DUMB up on Linux, BeOS and possibly even Mac OS X ***
********************************************************************


You should have got the .tar.gz version. If for some reason you got the .zip
version instead, you may have to use dtou on some or all of the text files.
If you have problems, just go and download the .tar.gz instead.

First, run the following command as a normal user:

   make

You will be asked whether you want Allegro support. Then, unless you are on
BeOS, you will be asked where you'd like DUMB to install its headers,
libraries and examples (which will go in the include/, lib/ and bin/
subdirectories of the prefix you specify). BeOS has fixed locations for these
files. Once you have specified these pieces of information, the optimised and
debugging builds of DUMB will be compiled, along with the examples. When it
has finished, you can install them with:

   make install

You may need to be root for this to work. It depends on the prefix you chose.

Note: the makefile will only work if COMSPEC and ComSpec are both undefined.
If either of these is defined, the makefile will try to build for a Windows
system, and will fail.

Please let me know if you have any trouble.

Information on the example programs is just below. Refer to docs/howto.txt
when you are ready to start programming with DUMB. If you use DUMB in a game,
let me know - I might decide to place a link to your game on DUMB's website!


****************************
*** The example programs ***
****************************


Two example programs are provided. On DOS and Windows, you can find them in
the examples subdirectory. On other systems they will be installed system-
wide.

dumbplay
   This program will only be built if you have Allegro. Pass it the filename
   of an IT, XM, S3M or MOD file, and it will play it. It's not a polished
   player with real-time threading or anything - so don't complain about it
   stuttering while you use other programs - but it does show DUMB's fidelity
   nicely. You can control the playback quality by editing dumb.ini, which
   must be in the current working directory. (This is a flaw for systems
   where the program is installed system-wide, but it is non-fatal.) Have a
   look at the examples/dumb.ini file for further information.

dumbout
   This program does not need Allegro. You can use it to stream an IT, XM,
   S3M or MOD file to raw PCM. This can be used as input to an encoder like
   oggenc (with appropriate command-line options), or it can be sent to a
   .pcm file which can be read by any respectable waveform editor. No .wav
   support yet, sorry. This program is also convenient for timing DUMB.
   Compare the time it takes to render a module with the module's playing
   time! dumbout doesn't try to read any configuration file; the options are
   set on the command line.


*********************************************
*** Downloading music or writing your own ***
*********************************************


If you would like to compose your own music modules, then first I must offer
a word of warning: not everyone is capable of composing music. Do not assume
you will be able to learn the art. By all means have a go; if you can learn
to play tunes on the computer keyboard, you're well on the way to being a
composer!

The best programs for the job are the trackers that pioneered the file
formats:

   Impulse Tracker - IT files - http://www.noisemusic.org/it/
   Fast Tracker II - XM files - http://www.gwinternet.com/music/ft2/
   Scream Tracker 3 - S3M files -
          http://www.united-trackers.org/resources/software/screamtracker.htm

MOD files come from the Amiga; I do not know what PC tracker to recommend for
editing these. If you know of one, let me know! In the meantime, I would
recommend using a more advanced file format. However, don't convert your
existing MODs just for the sake of it.

Note that Fast Tracker II is Shareware. It arguably offers the best
interface, but the IT file format is more powerful and better defined.
Impulse Tracker and Scream Tracker 3 are Freeware. DUMB is likely to be at
its best with IT files.

These editors are DOS programs. Users of DOS-incapable operating systems may
like to try ModPlug Tracker, but should read docs/modplug.txt before using it
for any serious work. If you use a different operating system, or if you know
of any module editors for Windows that are more faithful to the original
trackers' playback, please give me some links so I can put them here!

   ModPlug Tracker - http://www.modplug.com/

BEWARE OF WINAMP! Although it's excellent for MP3s, it is notorious for being
one of the worst module players in existence; very few modules play correctly
with it. There are plug-ins available to improve Winamp's module support, for
example WSP.

   Winamp - http://www.winamp.com/
   WSP - http://www.spytech.cz/index.php?sec=demo

Samples and instruments are the building blocks of music modules. You can
download samples at:

   http://www.tump.net/

If you would like to download module files composed by other people, check
the following sites:

   http://www.modarchive.com/
   http://www.scene.org/
   http://www.tump.net/
   http://www.homemusic.cc/main.php
   http://www.modplug.com/

Once again, if you know of more sites where samples or module files are
available for download, please let me know.

If you wish to use someone's music in your game, please respect the
composer's wishes. In general, you should ask the composer. Music that has
been placed in the Public Domain can be used by anyone for anything, but it
wouldn't do any harm to ask anyway if you know who the author is. In most
cases the author will be thrilled, so don't hesitate!

A note about converting modules from one format to another: don't do it,
unless you are a musician and are prepared to go through the file and make
sure everything sounds the way it should! The module formats are all slightly
different, and converting from one format to another will usually do some
damage.

Instead, it is recommended that you allow DUMB to interpret the original file
as it sees fit. DUMB may make mistakes (it does a lot of conversion on
loading), but future versions of DUMB will be able to rectify these mistakes.
On the other hand, if you convert the file, the damage is permanent.


***********************
*** Contact details ***
***********************


If you have trouble with DUMB, or want to contact me for any other reason, my
e-mail address is given below. However, I may be able to help more if you
come on to IRC EFnet #dumb.

IRC stands for Internet Relay Chat, and is a type of chat network. Several
such networks exist, and EFnet is a popular one. In order to connect to an
IRC network, you first need an IRC client. Here are some:

   http://www.xchat.org/
   http://www.visualirc.net/beta.php
   http://www.mirc.com/

Getting on to IRC can be a steep cliff, but it is not insurmountable, and
it's well worth it. Once you have set up the client software, you need to
connect to a server. Here is a list of EFnet servers I have had success with.
Type "/server" (without quotes), then a space, then the name of a server.

   irc.homelien.no
   irc.webgiro.se
   efnet.vuurwerk.nl
   efnet.demon.co.uk
   irc.isdnet.fr
   irc.prison.net

If these servers do not work, visit http://efnet.org/ircdb/servers.php for a
huge list of other EFnet servers to try.

Once you're connected, type the following:

   /join #dumb

A window will appear, and you can ask your question. It should be clear
what's going on from this point onwards. I am 'entheh'. Note that unlike many
other nerds I am not always at my computer, so if I don't answer your
question, don't take it personally! I will usually be able to read your
question when I come back.


******************
*** Conclusion ***
******************


This is the conclusion.


Ben Davis
entheh@users.sf.net
IRC EFnet #dumb
