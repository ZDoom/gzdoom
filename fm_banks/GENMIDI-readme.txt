DMXOPL3
==================================================================
https://github.com/sneakernets/DMXOPL
==================================================================

New and improved DMX GENMIDI for Doom and sourceports, taking full advantage of
the OPL3 waveforms. This takes things up a notch in terms of timbre.

# Summary
This is a GENMIDI patch for DMX for OPL3 FM synthesis. This patch aims to remedy
the "weak" default instruments to better match the Roland Sound Canvas, most
notably the SC-55 and SC-88. Recommended minimum setup for no note-cuts is
ZDoom 2.8.1 with DOSBOX OPL3 emulator core, with 6 chips emulated.

======FAQ======
# Why OPL3 only?
OPL3 has four additional waveforms - Alternating Sine, Camel Sine, Square, and
Logarithmic Square (Sawtooth). The most interesting of these is the Square wave,
for obvious reasons! This will benefit percussion instruments immensely.

# What did you use for this project?

Hex Editors, OPL3 Bank Editor, Fraggle's Python scripts, Adlib Tracker II,
Edlib, ADLMidi. I also used the following keyboards and devices:

- Yamaha PSR-3 for basic voice ideas
- Yamaha PSS-50 for layered voice ideas
- Yamaha DX-7
- Commodore 64 Music Module
- VRC7 and related chips
- Sound Blaster 16
- Yamaha PCI cards
- Sounds from various SEGA MegaDrive games

=======WOPL FAQ=======
# What's the difference between the WOPL version and the DMX version?
The main difference is with a few instruments that had to be tweaked to work
with DMX. Most of these changes are negligible for General MIDI stuff. To hear
what I intended DMXOPL to sound like in Doom, you'll have to wait for Eternity
Engine's ADLMIDI support to be finalized.

# Any plans for GS/XG support?
Yes. This will take considerable time, and voices will be added as I come across
them in my MIDI files. You can check the progress on those through the XG and GS
branches that I will make (or have already made). As I think the GM set is solid
enough now, save for just a handful of instruments, I can start work on the
other banks.

# Will the WOPL version be usable on any other FM chips?
Depends on the chip, but more than likely the answer is no. YMF262 (and the OPL*
family for that matter) operate differently than most of the other FM chips, and
wouldn't likely transfer over to DX-7 or other instruments without considerable
work, if they would work at all.


# Is the best way to listen to this really through an emulator?
Yes, unfortunately. Unless someone makes an FM chip that can handle 128+
channels, this is likely never to change. If you're using this in your music
projects and you still want to use the real thing, I recommend recording one
track at a time and throwing the result in your favorite DAW - just be sure to
put a highpass filter set to a really low value (I recommend 5 Hz) to get rid
of the offset garbage, lest your mix splatter like crazy.

======Credits======
* Wohlstand, for the OPL3BankEditor
* Bisqwuit, for ADLMidi
* Fraggle, for Chocolate Doom and GENMIDI research, as well as the Python scripts
* SubZ3ro, for AdLib Tracker II
* Esselfortium, for the encouragement and support
* Jimmy, who will include this in the next Adventures of Square release
* The Fat Man, who created the famous 2-op and 4-op patches everyone knows
* Diode Milliampere, who made FM synth popular again
* Patchouli, for keeping my spirits up
* Graf Zahl, for continuing ZDoom as GZDoom
* Randi, for implementing the OPL Emulation in ZDoom
* Fisk, for miscellaneous feedback and MIDIs to test
* Stewboy, for the Ancient Aliens MIDIs to test
* Xaser, who said this patch passes the "Angry Scientist test"
* Minigunner, just because
* Altazimuth, who claims it'll be in Eternity someday (this is now true)
* MTrop
* Quasar, for Eternity Engine
* Glaice, for patch advice
* BlastFrog, for patch advice
* Vogons Forums, for the OPL-3 Research and tools
* AtariAge, for C64 Sound module preset banks
* NintendoAge
* John Chowning, the father of FM synthesis. I hope he can hear this someday.

## Extra Thanks to:
* Doomworld Forums and the respective IRC/DISCORD channels
* The 4th Modulator DISCORD channels
* Giest118, who installed Doom again to listen to this
* Nuke.YKT, for testing this in Raptor, and for the Nuked OPL core
* kode54, who added this to Cog and FB2K, thank you!
* Patch93, who contributed various patches
* OlPainless, who contributed various patches
* Papiezak, who contributed various of patches
* Infurnus, for support
* Leileilol, for support
* MaliceX, for support
* Kuschelmonster, for support
* Anyone who makes music with this thing!

 YMF262 forever.

