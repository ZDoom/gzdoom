Game_Music_Emu 0.6.2: Game Music Emulators
------------------------------------------
Game_Music_Emu is a collection of video game music file emulators that
support the following formats and systems:

AY        ZX Spectrum/Amstrad CPC
GBS       Nintendo Game Boy
GYM       Sega Genesis/Mega Drive
HES       NEC TurboGrafx-16/PC Engine
KSS       MSX Home Computer/other Z80 systems (doesn't support FM sound)
NSF/NSFE  Nintendo NES/Famicom (with VRC 6, Namco 106, and FME-7 sound)
SAP       Atari systems using POKEY sound chip
SPC       Super Nintendo/Super Famicom
VGM/VGZ   Sega Master System/Mark III, Sega Genesis/Mega Drive,BBC Micro

Features:
* C interface for use in C, C++, and other compatible languages
* High emphasis has been placed on making the library very easy to use
* One set of common functions work with all emulators the same way
* Several code examples, including music player using SDL
* Portable code for use on any system with modern or older C++ compilers
* Adjustable output sample rate using quality band-limited resampling
* Uniform access to text information fields and track timing information
* End-of-track fading and automatic look ahead silence detection
* Treble/bass and stereo echo for AY/GBS/HES/KSS/NSF/NSFE/SAP/VGM
* Tempo can be adjusted and individual voices can be muted while playing
* Can read music data from file, memory, or custom reader function/class
* Can access track information without having to load into full emulator
* M3U track listing support for multi-track formats
* Modular design allows elimination of unneeded emulators/features

This library has been used in game music players for Windows, Linux on
several architectures, Mac OS, MorphOS, Xbox, PlayStation Portable,
GP2X, and Nintendo DS.

Author : Shay Green <gblargg@gmail.com>
Website: https://bitbucket.org/mpyne/game-music-emu/wiki/Home
License: GNU Lesser General Public License (LGPL)

Note: When you will use MAME YM2612 emulator, the license of library
will be GNU General Public License (GPL) v2.0+!

Current Maintainer: Michael Pyne <mpyne@purinchu.net>

Getting Started
---------------
Build a program consisting of demo/basics.c, demo/Wave_Writer.cpp, and
all source files in gme/.

Or, if you have CMake 2.6 or later, execute at a command prompt (from the
extracted source directory):

    mkdir build
    cd build
    cmake ../         # <-- Pass any needed CMake flags here
    make              # To build the library
    cd demo
    make              # To build the demo itself

Be sure "test.nsf" is in the same directory as the demo program. Running it
should generate the recording "out.wav".

You can use "make install" to install the library. To choose where to install
the library to, use the CMake argument "-DCMAKE_INSTALL_PREFIX=/usr/local"
(and replace /usr/local with the base path you wish to use). Alternately, you
can specify the base path to install to when you run "make install" by passing
'DESTDIR=/usr/local' on the make install command line (again, replace
/usr/local as appropriate).

To build a static library instead of shared (the default), pass
-DBUILD_SHARED_LIBS=OFF to the cmake command when running cmake.

A slightly more extensive demo application is available in the player/
directory.  It requires SDL to build.

Read gme.txt for more information. Post to the discussion forum for
assistance.

Files
-----
gme.txt               General notes about the library
changes.txt           Changes made since previous releases
design.txt            Library design notes
license.txt           GNU Lesser General Public License
CMakeLists.txt        CMake build rules

test.nsf              Test file for NSF emulator
test.m3u              Test m3u playlist for features.c demo

demo/
  basics.c            Records NSF file to wave sound file
  features.c          Demonstrates many additional features
  Wave_Writer.h       WAVE sound file writer used for demo output
  Wave_Writer.cpp
  CMakeLists.txt      CMake build rules

player/               Player using the SDL multimedia library
  player.cpp          Simple music player with waveform display
  Music_Player.cpp    Stand alone player for background music
  Music_Player.h
  Audio_Scope.cpp     Audio waveform scope
  Audio_Scope.h
  CMakeLists.txt      CMake build rules

gme/
  blargg_config.h     Library configuration (modify this file as needed)

  gme.h               Library interface header file
  gme.cpp

  Ay_Emu.h            ZX Spectrum AY emulator
  Ay_Emu.cpp
  Ay_Apu.cpp
  Ay_Apu.h
  Ay_Cpu.cpp
  Ay_Cpu.h

  Gbs_Emu.h           Nintendo Game Boy GBS emulator
  Gbs_Emu.cpp
  Gb_Apu.cpp
  Gb_Apu.h
  Gb_Cpu.cpp
  Gb_Cpu.h
  gb_cpu_io.h
  Gb_Oscs.cpp
  Gb_Oscs.h

  Hes_Emu.h           TurboGrafx-16/PC Engine HES emulator
  Hes_Apu.cpp
  Hes_Apu.h
  Hes_Cpu.cpp
  Hes_Cpu.h
  hes_cpu_io.h
  Hes_Emu.cpp

  Kss_Emu.h           MSX Home Computer/other Z80 systems KSS emulator
  Kss_Emu.cpp
  Kss_Cpu.cpp
  Kss_Cpu.h
  Kss_Scc_Apu.cpp
  Kss_Scc_Apu.h
  Ay_Apu.h
  Ay_Apu.cpp
  Sms_Apu.h
  Sms_Apu.cpp
  Sms_Oscs.h

  Nsf_Emu.h           Nintendo NES NSF/NSFE emulator
  Nsf_Emu.cpp
  Nes_Apu.cpp
  Nes_Apu.h
  Nes_Cpu.cpp
  Nes_Cpu.h
  nes_cpu_io.h
  Nes_Oscs.cpp
  Nes_Oscs.h
  Nes_Fme7_Apu.cpp
  Nes_Fme7_Apu.h
  Nes_Namco_Apu.cpp
  Nes_Namco_Apu.h
  Nes_Vrc6_Apu.cpp
  Nes_Vrc6_Apu.h
  Nsfe_Emu.h          NSFE support
  Nsfe_Emu.cpp

  Spc_Emu.h           Super Nintendo SPC emulator
  Spc_Emu.cpp
  Snes_Spc.cpp
  Snes_Spc.h
  Spc_Cpu.cpp
  Spc_Cpu.h
  Spc_Dsp.cpp
  Spc_Dsp.h
  Fir_Resampler.cpp
  Fir_Resampler.h

  Sap_Emu.h           Atari SAP emulator
  Sap_Emu.cpp
  Sap_Apu.cpp
  Sap_Apu.h
  Sap_Cpu.cpp
  Sap_Cpu.h
  sap_cpu_io.h

  Vgm_Emu.h           Sega VGM emulator
  Vgm_Emu_Impl.cpp
  Vgm_Emu_Impl.h
  Vgm_Emu.cpp
  Ym2413_Emu.cpp
  Ym2413_Emu.h
  Gym_Emu.h           Sega Genesis GYM emulator
  Gym_Emu.cpp
  Sms_Apu.cpp         Common Sega emulator files
  Sms_Apu.h
  Sms_Oscs.h
  Ym2612_Emu.h
  Ym2612_GENS.cpp     GENS 2.10 YM2612 emulator (LGPLv2.1+ license)
  Ym2612_GENS.h
  Ym2612_MAME.cpp     MAME YM2612 emulator (GPLv2.0+ license)
  Ym2612_MAME.h
  Ym2612_Nuked.cpp    Nuked OPN2 emulator (LGPLv2.1+ license)
  Ym2612_Nuked.h
  Dual_Resampler.cpp
  Dual_Resampler.h
  Fir_Resampler.cpp
  Fir_Resampler.h

  M3u_Playlist.h      M3U playlist support
  M3u_Playlist.cpp

  Effects_Buffer.h    Sound buffer with stereo echo and panning
  Effects_Buffer.cpp

  blargg_common.h     Common files needed by all emulators
  blargg_endian.h
  blargg_source.h
  Blip_Buffer.cpp
  Blip_Buffer.h
  Gme_File.h
  Gme_File.cpp
  Music_Emu.h
  Music_Emu.cpp
  Classic_Emu.h
  Classic_Emu.cpp
  Multi_Buffer.h
  Multi_Buffer.cpp
  Data_Reader.h
  Data_Reader.cpp

  CMakeLists.txt      CMake build rules


Legal
-----
Game_Music_Emu library copyright (C) 2003-2009 Shay Green.
Sega Genesis YM2612 emulator copyright (C) 2002 Stephane Dallongeville.
MAME YM2612 emulator copyright (C) 2003 Jarek Burczynski, Tatsuyuki Satoh
Nuked OPN2 emulator copyright (C) 2017 Alexey Khokholov (Nuke.YKT)

--
Shay Green <gblargg@gmail.com>
