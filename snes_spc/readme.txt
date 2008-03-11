snes_spc 0.9.0: SNES SPC-700 APU Emulator
-----------------------------------------
This library includes a full SPC emulator and an S-DSP emulator that can
be used on its own. Two S-DSP emulators are available: a highly accurate
one for use in a SNES emulator, and a 3x faster one for use in an SPC
music player or a resource-limited SNES emulator.

* Can be used from C and C++ code
* Full SPC-700 APU emulator with cycle accuracy in most cases
* Loads, plays, and saves SPC music files
* Can save and load exact full emulator state
* DSP voice muting, surround sound disable, and song tempo adjustment
* Uses 7% CPU average on 400 MHz Mac to play an SPC using fast DSP

The accurate DSP emulator is based on past research by others and
hundreds of hours of recent research by me. It passes over a hundred
strenuous timing and behavior validation tests that were also run on the
SNES. As far as I know, it's the first DSP emulator with cycle accuracy,
properly emulating every DSP register and memory access at the exact SPC
cycle it occurs at, whereas previous DSP emulators emulated these only
to the nearest sample (which occurs every 32 clocks).

Author : Shay Green <gblargg@gmail.com>
Website: http://www.slack.net/~ant/
Forum  : http://groups.google.com/group/blargg-sound-libs
License: GNU Lesser General Public License (LGPL)


Getting Started
---------------
Build a program consisting of demo/play_spc.c, demo/demo_util.c,
demo/wave_writer.c, and all source files in snes_spc/. Put an SPC music
file in the same directory and name it "test.spc". Running the program
should generate the recording "out.wav".

Read snes_spc.txt for more information. Post to the discussion forum for
assistance.


Files
-----
snes_spc.txt            Documentation
changes.txt             Change log
license.txt             GNU LGPL license

demo/
  play_spc.c            Records SPC file to wave sound file
  benchmark.c           Finds how fast emulator runs on your computer
  trim_spc.c            Trims silence off beginning of an SPC file
  save_state.c          Saves/loads exact emulator state to/from file
  comm.c                Communicates with SPC how SNES would
  demo_util.h           General utility functions used by demos
  demo_util.c
  wave_writer.h         WAVE sound file writer used for demo output
  wave_writer.c

fast_dsp/               Optional standalone fast DSP emulator
  SPC_DSP.h             To use with full SPC emulator, move into
  SPC_DSP.cpp           snes_spc/ and replace original files

snes_spc/               Library sources
  blargg_config.h       Configuration (modify as necessary)
  
  spc.h                 C interface to SPC emulator and sound filter
  spc.cpp
  
  SPC_Filter.h          Optional filter to make sound more authentic
  SPC_Filter.cpp
  
  SNES_SPC.h            Full SPC emulator
  SNES_SPC.cpp
  SNES_SPC_misc.cpp
  SNES_SPC_state.cpp
  SPC_CPU.h
  
  dsp.h                 C interface to DSP emulator
  dsp.cpp
  
  SPC_DSP.h             Standalone accurate DSP emulator
  SPC_DSP.cpp
  blargg_common.h
  blargg_endian.h
  blargg_source.h

-- 
Shay Green <gblargg@gmail.com>
