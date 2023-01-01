Bank was imported by a hacky way from the Tomsoft's SegaMusic program
by TommyXie (Xie Rong Chun):
- the dummy MIDI file was created that contains all 128 instruments in GM order
- the Sega emulator playable BIN file was generated
- the GYM dump was generated from the playback of that dummy instrument
- OPN2 Bank Editor was used to scan GYM file for instruments and import all of
them.

The work woth done by Jean-Pierre Cimalando:
https://github.com/Wohlstand/OPN2BankEditor/issues/44

Then, the bank was tuned by Wohlstand:
- Corrected note offsets to align octaves of all instruments
- Merged with xg.wopn to provide the set of percussions.

