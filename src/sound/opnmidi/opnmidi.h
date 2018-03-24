/*
 * libOPNMIDI is a free MIDI to WAV conversion library with OPN2 (YM2612) emulation
 *
 * MIDI parser and player (Original code from ADLMIDI): Copyright (c) 2010-2014 Joel Yliluoma <bisqwit@iki.fi>
 * OPNMIDI Library and YM2612 support:   Copyright (c) 2017-2018 Vitaly Novichkov <admin@wohlnet.ru>
 *
 * Library is based on the ADLMIDI, a MIDI player for Linux and Windows with OPL3 emulation:
 * http://iki.fi/bisqwit/source/adlmidi.html
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef OPNMIDI_H
#define OPNMIDI_H

#ifdef __cplusplus
extern "C" {
#endif

#define OPNMIDI_VERSION_MAJOR       1
#define OPNMIDI_VERSION_MINOR       1
#define OPNMIDI_VERSION_PATCHLEVEL  1

#define OPNMIDI_TOSTR(s) #s
#define OPNMIDI_VERSION \
        OPNMIDI_TOSTR(OPNMIDI_VERSION_MAJOR) "." \
        OPNMIDI_TOSTR(OPNMIDI_VERSION_MINOR) "." \
        OPNMIDI_TOSTR(OPNMIDI_VERSION_PATCHLEVEL)

#include <stddef.h>

#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L)
#include <stdint.h>
typedef uint8_t         OPN2_UInt8;
typedef uint16_t        OPN2_UInt16;
typedef int8_t          OPN2_SInt8;
typedef int16_t         OPN2_SInt16;
#else
typedef unsigned char   OPN2_UInt8;
typedef unsigned short  OPN2_UInt16;
typedef char            OPN2_SInt8;
typedef short           OPN2_SInt16;
#endif

enum OPNMIDI_VolumeModels
{
    OPNMIDI_VolumeModel_AUTO = 0,
    OPNMIDI_VolumeModel_Generic,
    OPNMIDI_VolumeModel_CMF,
    OPNMIDI_VolumeModel_DMX,
    OPNMIDI_VolumeModel_APOGEE,
    OPNMIDI_VolumeModel_9X
};

struct OPN2_MIDIPlayer
{
    void *opn2_midiPlayer;
};

/* DEPRECATED */
#define opn2_setNumCards opn2_setNumChips

/* Sets number of emulated sound cards (from 1 to 100). Emulation of multiple sound cards exchanges polyphony limits*/
extern int  opn2_setNumChips(struct OPN2_MIDIPlayer *device, int numCards);

/* Get current number of emulated chips */
extern int  opn2_getNumChips(struct OPN2_MIDIPlayer *device);

/*Enable or disable Enables scaling of modulator volumes*/
extern void opn2_setScaleModulators(struct OPN2_MIDIPlayer *device, int smod);

/*Enable or disable built-in loop (built-in loop supports 'loopStart' and 'loopEnd' tags to loop specific part)*/
extern void opn2_setLoopEnabled(struct OPN2_MIDIPlayer *device, int loopEn);

/*Enable or disable Logariphmic volume changer (-1 sets default per bank, 0 disable, 1 enable) */
extern void opn2_setLogarithmicVolumes(struct OPN2_MIDIPlayer *device, int logvol);

/*Set different volume range model */
extern void opn2_setVolumeRangeModel(struct OPN2_MIDIPlayer *device, int volumeModel);

/*Load WOPN bank file from File System. Is recommended to call adl_reset() to apply changes to already-loaded file player or real-time.*/
extern int opn2_openBankFile(struct OPN2_MIDIPlayer *device, const char *filePath);

/*Load WOPN bank file from memory data*/
extern int opn2_openBankData(struct OPN2_MIDIPlayer *device, const void *mem, long size);


/*Returns chip emulator name string*/
extern const char *opn2_emulatorName();

typedef struct {
    OPN2_UInt16 major;
    OPN2_UInt16 minor;
    OPN2_UInt16 patch;
} OPN2_Version;

/*Returns string which contains a version number*/
extern const char *opn2_linkedLibraryVersion();

/*Returns structure which contains a version number of library */
extern const OPN2_Version *opn2_linkedVersion();

/*Returns string which contains last error message*/
extern const char *opn2_errorString();

/*Returns string which contains last error message on specific device*/
extern const char *opn2_errorInfo(struct OPN2_MIDIPlayer *device);

/*Initialize ADLMIDI Player device*/
extern struct OPN2_MIDIPlayer *opn2_init(long sample_rate);

/*Load MIDI file from File System*/
extern int opn2_openFile(struct OPN2_MIDIPlayer *device, const char *filePath);

/*Load MIDI file from memory data*/
extern int opn2_openData(struct OPN2_MIDIPlayer *device, const void *mem, unsigned long size);

/*Resets MIDI player*/
extern void opn2_reset(struct OPN2_MIDIPlayer *device);

/*Get total time length of current song*/
extern double opn2_totalTimeLength(struct OPN2_MIDIPlayer *device);

/*Get loop start time if presented. -1 means MIDI file has no loop points */
extern double opn2_loopStartTime(struct OPN2_MIDIPlayer *device);

/*Get loop end time if presented. -1 means MIDI file has no loop points */
extern double opn2_loopEndTime(struct OPN2_MIDIPlayer *device);

/*Get current time position in seconds*/
extern double opn2_positionTell(struct OPN2_MIDIPlayer *device);

/*Jump to absolute time position in seconds*/
extern void opn2_positionSeek(struct OPN2_MIDIPlayer *device, double seconds);

/*Reset MIDI track position to begin */
extern void opn2_positionRewind(struct OPN2_MIDIPlayer *device);

/*Set tempo multiplier: 1.0 - original tempo, >1 - play faster, <1 - play slower */
extern void opn2_setTempo(struct OPN2_MIDIPlayer *device, double tempo);

/*Close and delete OPNMIDI device*/
extern void opn2_close(struct OPN2_MIDIPlayer *device);



/**META**/

/*Returns string which contains a music title*/
extern const char *opn2_metaMusicTitle(struct OPN2_MIDIPlayer *device);

/*Returns string which contains a copyright string*/
extern const char *opn2_metaMusicCopyright(struct OPN2_MIDIPlayer *device);

/*Returns count of available track titles: NOTE: there are CAN'T be associated with channel in any of event or note hooks */
extern size_t opn2_metaTrackTitleCount(struct OPN2_MIDIPlayer *device);

/*Get track title by index*/
extern const char *opn2_metaTrackTitle(struct OPN2_MIDIPlayer *device, size_t index);

struct Opn2_MarkerEntry
{
    const char      *label;
    double          pos_time;
    unsigned long   pos_ticks;
};

/*Returns count of available markers*/
extern size_t opn2_metaMarkerCount(struct OPN2_MIDIPlayer *device);

/*Returns the marker entry*/
extern struct Opn2_MarkerEntry opn2_metaMarker(struct OPN2_MIDIPlayer *device, size_t index);




/*Take a sample buffer and iterate MIDI timers */
extern int  opn2_play(struct OPN2_MIDIPlayer *device, int sampleCount, short out[]);

/*Generate audio output from chip emulators without iteration of MIDI timers.*/
extern int  opn2_generate(struct OPN2_MIDIPlayer *device, int sampleCount, short *out);

/**
 * @brief Periodic tick handler.
 * @param device
 * @param seconds seconds since last call
 * @param granularity don't expect intervals smaller than this, in seconds
 * @return desired number of seconds until next call
 *
 * Use it for Hardware OPL3 mode or when you want to process events differently from opn2_play() function.
 * DON'T USE IT TOGETHER WITH opn2_play()!!!
 */
extern double opn2_tickEvents(struct OPN2_MIDIPlayer *device, double seconds, double granuality);

/*Returns 1 if music position has reached end*/
extern int opn2_atEnd(struct OPN2_MIDIPlayer *device);

/**RealTime**/

/*Force Off all notes on all channels*/
extern void opn2_panic(struct OPN2_MIDIPlayer *device);

/*Reset states of all controllers on all MIDI channels*/
extern void opn2_rt_resetState(struct OPN2_MIDIPlayer *device);

/*Turn specific MIDI note ON*/
extern int opn2_rt_noteOn(struct OPN2_MIDIPlayer *device, OPN2_UInt8 channel, OPN2_UInt8 note, OPN2_UInt8 velocity);

/*Turn specific MIDI note OFF*/
extern void opn2_rt_noteOff(struct OPN2_MIDIPlayer *device, OPN2_UInt8 channel, OPN2_UInt8 note);

/*Set note after-touch*/
extern void opn2_rt_noteAfterTouch(struct OPN2_MIDIPlayer *device, OPN2_UInt8 channel, OPN2_UInt8 note, OPN2_UInt8 atVal);
/*Set channel after-touch*/
extern void opn2_rt_channelAfterTouch(struct OPN2_MIDIPlayer *device, OPN2_UInt8 channel, OPN2_UInt8 atVal);

/*Apply controller change*/
extern void opn2_rt_controllerChange(struct OPN2_MIDIPlayer *device, OPN2_UInt8 channel, OPN2_UInt8 type, OPN2_UInt8 value);

/*Apply patch change*/
extern void opn2_rt_patchChange(struct OPN2_MIDIPlayer *device, OPN2_UInt8 channel, OPN2_UInt8 patch);

/*Apply pitch bend change*/
extern void opn2_rt_pitchBend(struct OPN2_MIDIPlayer *device, OPN2_UInt8 channel, OPN2_UInt16 pitch);
/*Apply pitch bend change*/
extern void opn2_rt_pitchBendML(struct OPN2_MIDIPlayer *device, OPN2_UInt8 channel, OPN2_UInt8 msb, OPN2_UInt8 lsb);

/*Change LSB of the bank*/
extern void opn2_rt_bankChangeLSB(struct OPN2_MIDIPlayer *device, OPN2_UInt8 channel, OPN2_UInt8 lsb);
/*Change MSB of the bank*/
extern void opn2_rt_bankChangeMSB(struct OPN2_MIDIPlayer *device, OPN2_UInt8 channel, OPN2_UInt8 msb);
/*Change bank by absolute signed value*/
extern void opn2_rt_bankChange(struct OPN2_MIDIPlayer *device, OPN2_UInt8 channel, OPN2_SInt16 bank);


/**Hooks**/

typedef void (*OPN2_RawEventHook)(void *userdata, OPN2_UInt8 type, OPN2_UInt8 subtype, OPN2_UInt8 channel, const OPN2_UInt8 *data, size_t len);
typedef void (*OPN2_NoteHook)(void *userdata, int adlchn, int note, int ins, int pressure, double bend);
typedef void (*OPN2_DebugMessageHook)(void *userdata, const char *fmt, ...);

/* Set raw MIDI event hook */
extern void opn2_setRawEventHook(struct OPN2_MIDIPlayer *device, OPN2_RawEventHook rawEventHook, void *userData);

/* Set note hook */
extern void opn2_setNoteHook(struct OPN2_MIDIPlayer *device, OPN2_NoteHook noteHook, void *userData);

/* Set debug message hook */
extern void opn2_setDebugMessageHook(struct OPN2_MIDIPlayer *device, OPN2_DebugMessageHook debugMessageHook, void *userData);

#ifdef __cplusplus
}
#endif

#endif /* OPNMIDI_H */
