/*
 * libADLMIDI is a free MIDI to WAV conversion library with OPL3 emulation
 *
 * Original ADLMIDI code: Copyright (c) 2010-2014 Joel Yliluoma <bisqwit@iki.fi>
 * ADLMIDI Library API:   Copyright (c) 2015-2018 Vitaly Novichkov <admin@wohlnet.ru>
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

#ifndef ADLMIDI_H
#define ADLMIDI_H

#ifdef __cplusplus
extern "C" {
#endif

#define ADLMIDI_VERSION_MAJOR       1
#define ADLMIDI_VERSION_MINOR       3
#define ADLMIDI_VERSION_PATCHLEVEL  3

#define ADLMIDI_TOSTR_I(s) #s
#define ADLMIDI_TOSTR(s) ADLMIDI_TOSTR_I(s)
#define ADLMIDI_VERSION \
        ADLMIDI_TOSTR(ADLMIDI_VERSION_MAJOR) "." \
        ADLMIDI_TOSTR(ADLMIDI_VERSION_MINOR) "." \
        ADLMIDI_TOSTR(ADLMIDI_VERSION_PATCHLEVEL)


#include <stddef.h>

#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L)
#include <stdint.h>
typedef uint8_t         ADL_UInt8;
typedef uint16_t        ADL_UInt16;
typedef int8_t          ADL_SInt8;
typedef int16_t         ADL_SInt16;
#else
typedef unsigned char   ADL_UInt8;
typedef unsigned short  ADL_UInt16;
typedef char            ADL_SInt8;
typedef short           ADL_SInt16;
#endif

enum ADLMIDI_VolumeModels
{
    ADLMIDI_VolumeModel_AUTO = 0,
    ADLMIDI_VolumeModel_Generic = 1,
    ADLMIDI_VolumeModel_NativeOPL3 = 2,
    ADLMIDI_VolumeModel_CMF = ADLMIDI_VolumeModel_NativeOPL3,
    ADLMIDI_VolumeModel_DMX = 3,
    ADLMIDI_VolumeModel_APOGEE = 4,
    ADLMIDI_VolumeModel_9X = 5
};

enum ADLMIDI_SampleType
{
    ADLMIDI_SampleType_S16 = 0,  /* signed PCM 16-bit */
    ADLMIDI_SampleType_S8,       /* signed PCM 8-bit */
    ADLMIDI_SampleType_F32,      /* float 32-bit */
    ADLMIDI_SampleType_F64,      /* float 64-bit */
    ADLMIDI_SampleType_S24,      /* signed PCM 24-bit */
    ADLMIDI_SampleType_S32,      /* signed PCM 32-bit */
    ADLMIDI_SampleType_U8,       /* unsigned PCM 8-bit */
    ADLMIDI_SampleType_U16,      /* unsigned PCM 16-bit */
    ADLMIDI_SampleType_U24,      /* unsigned PCM 24-bit */
    ADLMIDI_SampleType_U32,      /* unsigned PCM 32-bit */
    ADLMIDI_SampleType_Count,
};

struct ADLMIDI_AudioFormat
{
    enum ADLMIDI_SampleType type;  /* type of sample */
    unsigned containerSize;        /* size in bytes of the storage type */
    unsigned sampleOffset;         /* distance in bytes between consecutive samples */
};

struct ADL_MIDIPlayer
{
    void *adl_midiPlayer;
};

/* DEPRECATED */
#define adl_setNumCards adl_setNumChips

/* Sets number of emulated chips (from 1 to 100). Emulation of multiple chips exchanges polyphony limits*/
extern int adl_setNumChips(struct ADL_MIDIPlayer *device, int numCards);

/* Get current number of emulated chips */
extern int adl_getNumChips(struct ADL_MIDIPlayer *device);

/* Sets a number of the patches bank from 0 to N banks. Is recommended to call adl_reset() to apply changes to already-loaded file player or real-time. */
extern int adl_setBank(struct ADL_MIDIPlayer *device, int bank);

/* Returns total number of available banks */
extern int adl_getBanksCount();

/* Returns pointer to array of names of every bank */
extern const char *const *adl_getBankNames();

/* Reference to dynamic bank */
typedef struct ADL_Bank
{
    void *pointer[3];
} ADL_Bank;

/* Identifier of dynamic bank */
typedef struct ADL_BankId
{
    ADL_UInt8 percussive, msb, lsb;
} ADL_BankId;

/* Flags for dynamic bank access */
enum ADL_BankAccessFlags
{
    ADLMIDI_Bank_Create = 1,      /* create bank, allocating memory as needed */
    ADLMIDI_Bank_CreateRt = 1|2,  /* create bank, never allocating memory */
};

typedef struct ADL_Instrument ADL_Instrument;

#if defined(ADLMIDI_UNSTABLE_API)
/* Preallocates a minimum number of bank slots. Returns the actual capacity. */
extern int adl_reserveBanks(struct ADL_MIDIPlayer *device, unsigned banks);
/* Gets the bank designated by the identifier, optionally creating if it does not exist. */
extern int adl_getBank(struct ADL_MIDIPlayer *device, const ADL_BankId *id, int flags, ADL_Bank *bank);
/* Gets the identifier of a bank. */
extern int adl_getBankId(struct ADL_MIDIPlayer *device, const ADL_Bank *bank, ADL_BankId *id);
/* Removes a bank. */
extern int adl_removeBank(struct ADL_MIDIPlayer *device, ADL_Bank *bank);
/* Gets the first bank. */
extern int adl_getFirstBank(struct ADL_MIDIPlayer *device, ADL_Bank *bank);
/* Iterates to the next bank. */
extern int adl_getNextBank(struct ADL_MIDIPlayer *device, ADL_Bank *bank);
/* Gets the nth intrument in the bank [0..127]. */
extern int adl_getInstrument(struct ADL_MIDIPlayer *device, const ADL_Bank *bank, unsigned index, ADL_Instrument *ins);
/* Sets the nth intrument in the bank [0..127]. */
extern int adl_setInstrument(struct ADL_MIDIPlayer *device, ADL_Bank *bank, unsigned index, const ADL_Instrument *ins);
#endif /* defined(ADLMIDI_UNSTABLE_API) */

/*Sets number of 4-operator channels between all chips.
  By default, it is automatically re-calculating every bank change.
  If you want to specify custom number of four operator channels,
  please call this function after bank change (adl_setBank() or adl_openBank()),
  otherwise, value will be overwritten by auto-calculated.*/
extern int adl_setNumFourOpsChn(struct ADL_MIDIPlayer *device, int ops4);

/*Get current total count of 4-operator channels between all chips*/
extern int adl_getNumFourOpsChn(struct ADL_MIDIPlayer *device);

/*Override Enable(1) or Disable(0) AdLib percussion mode. -1 - use bank default AdLib percussion mode*/
extern void adl_setPercMode(struct ADL_MIDIPlayer *device, int percmod);

/*Override Enable(1) or Disable(0) deep vibrato state. -1 - use bank default vibrato state*/
extern void adl_setHVibrato(struct ADL_MIDIPlayer *device, int hvibro);

/*Override Enable(1) or Disable(0) deep tremolo state. -1 - use bank default tremolo state*/
extern void adl_setHTremolo(struct ADL_MIDIPlayer *device, int htremo);

/*Override Enable(1) or Disable(0) scaling of modulator volumes. -1 - use bank default scaling of modulator volumes*/
extern void adl_setScaleModulators(struct ADL_MIDIPlayer *device, int smod);

/*Enable(1) or Disable(0) full-range brightness (MIDI CC74 used in XG music to filter result sounding) scaling.
    By default, brightness affects sound between 0 and 64.
    When this option is enabled, the range will use a full range from 0 up to 127.
*/
extern void adl_setFullRangeBrightness(struct ADL_MIDIPlayer *device, int fr_brightness);

/*Enable or disable built-in loop (built-in loop supports 'loopStart' and 'loopEnd' tags to loop specific part)*/
extern void adl_setLoopEnabled(struct ADL_MIDIPlayer *device, int loopEn);

/* !!!DEPRECATED!!! Enable or disable Logariphmic volume changer */
extern void adl_setLogarithmicVolumes(struct ADL_MIDIPlayer *device, int logvol);

/*Set different volume range model */
extern void adl_setVolumeRangeModel(struct ADL_MIDIPlayer *device, int volumeModel);

/*Load WOPL bank file from File System. Is recommended to call adl_reset() to apply changes to already-loaded file player or real-time.*/
extern int adl_openBankFile(struct ADL_MIDIPlayer *device, const char *filePath);

/*Load WOPL bank file from memory data*/
extern int adl_openBankData(struct ADL_MIDIPlayer *device, const void *mem, unsigned long size);


/* DEPRECATED */
extern const char *adl_emulatorName();

/*Returns chip emulator name string*/
extern const char *adl_chipEmulatorName(struct ADL_MIDIPlayer *device);

enum ADL_Emulator
{
    ADLMIDI_EMU_NUKED = 0,
    ADLMIDI_EMU_NUKED_174,
    ADLMIDI_EMU_DOSBOX,
    ADLMIDI_EMU_end
};

/* Switch the emulation core */
extern int adl_switchEmulator(struct ADL_MIDIPlayer *device, int emulator);


typedef struct {
    ADL_UInt16 major;
    ADL_UInt16 minor;
    ADL_UInt16 patch;
} ADL_Version;

/*Run emulator with PCM rate to reduce CPU usage on slow devices. May decrease sounding accuracy.*/
extern int adl_setRunAtPcmRate(struct ADL_MIDIPlayer *device, int enabled);

/*Returns string which contains a version number*/
extern const char *adl_linkedLibraryVersion();

/*Returns structure which contains a version number of library */
extern const ADL_Version *adl_linkedVersion();

/*Returns string which contains last error message of initialization*/
extern const char *adl_errorString();

/*Returns string which contains last error message on specific device*/
extern const char *adl_errorInfo(struct ADL_MIDIPlayer *device);

/*Initialize ADLMIDI Player device*/
extern struct ADL_MIDIPlayer *adl_init(long sample_rate);

/*Load MIDI file from File System*/
extern int adl_openFile(struct ADL_MIDIPlayer *device, const char *filePath);

/*Load MIDI file from memory data*/
extern int adl_openData(struct ADL_MIDIPlayer *device, const void *mem, unsigned long size);

/*Resets MIDI player*/
extern void adl_reset(struct ADL_MIDIPlayer *device);

/*Get total time length of current song*/
extern double adl_totalTimeLength(struct ADL_MIDIPlayer *device);

/*Get loop start time if presented. -1 means MIDI file has no loop points */
extern double adl_loopStartTime(struct ADL_MIDIPlayer *device);

/*Get loop end time if presented. -1 means MIDI file has no loop points */
extern double adl_loopEndTime(struct ADL_MIDIPlayer *device);

/*Get current time position in seconds*/
extern double adl_positionTell(struct ADL_MIDIPlayer *device);

/*Jump to absolute time position in seconds*/
extern void adl_positionSeek(struct ADL_MIDIPlayer *device, double seconds);

/*Reset MIDI track position to begin */
extern void adl_positionRewind(struct ADL_MIDIPlayer *device);

/*Set tempo multiplier: 1.0 - original tempo, >1 - play faster, <1 - play slower */
extern void adl_setTempo(struct ADL_MIDIPlayer *device, double tempo);

/*Close and delete ADLMIDI device*/
extern void adl_close(struct ADL_MIDIPlayer *device);



/**META**/

/*Returns string which contains a music title*/
extern const char *adl_metaMusicTitle(struct ADL_MIDIPlayer *device);

/*Returns string which contains a copyright string*/
extern const char *adl_metaMusicCopyright(struct ADL_MIDIPlayer *device);

/*Returns count of available track titles: NOTE: there are CAN'T be associated with channel in any of event or note hooks */
extern size_t adl_metaTrackTitleCount(struct ADL_MIDIPlayer *device);

/*Get track title by index*/
extern const char *adl_metaTrackTitle(struct ADL_MIDIPlayer *device, size_t index);

struct Adl_MarkerEntry
{
    const char      *label;
    double          pos_time;
    unsigned long   pos_ticks;
};

/*Returns count of available markers*/
extern size_t adl_metaMarkerCount(struct ADL_MIDIPlayer *device);

/*Returns the marker entry*/
extern struct Adl_MarkerEntry adl_metaMarker(struct ADL_MIDIPlayer *device, size_t index);


/*Take a sample buffer and iterate MIDI timers */
extern int  adl_play(struct ADL_MIDIPlayer *device, int sampleCount, short *out);

/*Take a sample buffer and iterate MIDI timers */
extern int  adl_playFormat(struct ADL_MIDIPlayer *device, int sampleCount, ADL_UInt8 *left, ADL_UInt8 *right, const struct ADLMIDI_AudioFormat *format);

/*Generate audio output from chip emulators without iteration of MIDI timers.*/
extern int  adl_generate(struct ADL_MIDIPlayer *device, int sampleCount, short *out);

/*Generate audio output from chip emulators without iteration of MIDI timers.*/
extern int  adl_generateFormat(struct ADL_MIDIPlayer *device, int sampleCount, ADL_UInt8 *left, ADL_UInt8 *right, const struct ADLMIDI_AudioFormat *format);

/**
 * @brief Periodic tick handler.
 * @param device
 * @param seconds seconds since last call
 * @param granularity don't expect intervals smaller than this, in seconds
 * @return desired number of seconds until next call
 *
 * Use it for Hardware OPL3 mode or when you want to process events differently from adl_play() function.
 * DON'T USE IT TOGETHER WITH adl_play()!!!
 */
extern double adl_tickEvents(struct ADL_MIDIPlayer *device, double seconds, double granuality);

/*Returns 1 if music position has reached end*/
extern int adl_atEnd(struct ADL_MIDIPlayer *device);

/**RealTime**/

/*Force Off all notes on all channels*/
extern void adl_panic(struct ADL_MIDIPlayer *device);

/*Reset states of all controllers on all MIDI channels*/
extern void adl_rt_resetState(struct ADL_MIDIPlayer *device);

/*Turn specific MIDI note ON*/
extern int adl_rt_noteOn(struct ADL_MIDIPlayer *device, ADL_UInt8 channel, ADL_UInt8 note, ADL_UInt8 velocity);

/*Turn specific MIDI note OFF*/
extern void adl_rt_noteOff(struct ADL_MIDIPlayer *device, ADL_UInt8 channel, ADL_UInt8 note);

/*Set note after-touch*/
extern void adl_rt_noteAfterTouch(struct ADL_MIDIPlayer *device, ADL_UInt8 channel, ADL_UInt8 note, ADL_UInt8 atVal);
/*Set channel after-touch*/
extern void adl_rt_channelAfterTouch(struct ADL_MIDIPlayer *device, ADL_UInt8 channel, ADL_UInt8 atVal);

/*Apply controller change*/
extern void adl_rt_controllerChange(struct ADL_MIDIPlayer *device, ADL_UInt8 channel, ADL_UInt8 type, ADL_UInt8 value);

/*Apply patch change*/
extern void adl_rt_patchChange(struct ADL_MIDIPlayer *device, ADL_UInt8 channel, ADL_UInt8 patch);

/*Apply pitch bend change*/
extern void adl_rt_pitchBend(struct ADL_MIDIPlayer *device, ADL_UInt8 channel, ADL_UInt16 pitch);
/*Apply pitch bend change*/
extern void adl_rt_pitchBendML(struct ADL_MIDIPlayer *device, ADL_UInt8 channel, ADL_UInt8 msb, ADL_UInt8 lsb);

/*Change LSB of the bank*/
extern void adl_rt_bankChangeLSB(struct ADL_MIDIPlayer *device, ADL_UInt8 channel, ADL_UInt8 lsb);
/*Change MSB of the bank*/
extern void adl_rt_bankChangeMSB(struct ADL_MIDIPlayer *device, ADL_UInt8 channel, ADL_UInt8 msb);
/*Change bank by absolute signed value*/
extern void adl_rt_bankChange(struct ADL_MIDIPlayer *device, ADL_UInt8 channel, ADL_SInt16 bank);


/**Hooks**/

typedef void (*ADL_RawEventHook)(void *userdata, ADL_UInt8 type, ADL_UInt8 subtype, ADL_UInt8 channel, const ADL_UInt8 *data, size_t len);
typedef void (*ADL_NoteHook)(void *userdata, int adlchn, int note, int ins, int pressure, double bend);
typedef void (*ADL_DebugMessageHook)(void *userdata, const char *fmt, ...);

/* Set raw MIDI event hook */
extern void adl_setRawEventHook(struct ADL_MIDIPlayer *device, ADL_RawEventHook rawEventHook, void *userData);

/* Set note hook */
extern void adl_setNoteHook(struct ADL_MIDIPlayer *device, ADL_NoteHook noteHook, void *userData);

/* Set debug message hook */
extern void adl_setDebugMessageHook(struct ADL_MIDIPlayer *device, ADL_DebugMessageHook debugMessageHook, void *userData);


/**Instrument structures**/

enum
{
    ADLMIDI_InstrumentVersion = 0,
};

typedef enum ADL_InstrumentFlags
{
    /* Is two-operator single-voice instrument (no flags) */
    ADLMIDI_Ins_2op        = 0x00,
    /* Is true four-operator instrument */
    ADLMIDI_Ins_4op        = 0x01,
    /* Is pseudo four-operator (two 2-operator voices) instrument */
    ADLMIDI_Ins_Pseudo4op  = 0x02,
    /* Is a blank instrument entry */
    ADLMIDI_Ins_IsBlank    = 0x04,
    /* Mask of the flags range */
    ADLMIDI_Ins_ALL_MASK   = 0x07,
} ADL_InstrumentFlags;

typedef struct ADL_Operator
{
    /* AM/Vib/Env/Ksr/FMult characteristics */
    ADL_UInt8 avekf_20;
    /* Key Scale Level / Total level register data */
    ADL_UInt8 ksl_l_40;
    /* Attack / Decay */
    ADL_UInt8 atdec_60;
    /* Systain and Release register data */
    ADL_UInt8 susrel_80;
    /* Wave form */
    ADL_UInt8 waveform_E0;
} ADL_Operator;

typedef struct ADL_Instrument
{
    /* Version of the instrument object */
    int version;
    /* MIDI note key (half-tone) offset for an instrument (or a first voice in pseudo-4-op mode) */
    ADL_SInt16 note_offset1;
    /* MIDI note key (half-tone) offset for a second voice in pseudo-4-op mode */
    ADL_SInt16 note_offset2;
    /* MIDI note velocity offset (taken from Apogee TMB format) */
    ADL_SInt8  midi_velocity_offset;
    /* Second voice detune level (taken from DMX OP2) */
    ADL_SInt8  second_voice_detune;
    /* Percussion MIDI base tone number at which this drum will be played */
    ADL_UInt8 percussion_key_number;
    /* Enum ADL_InstrumentFlags */
    ADL_UInt8 inst_flags;
    /* Feedback&Connection register for first and second operators */
    ADL_UInt8 fb_conn1_C0;
    /* Feedback&Connection register for third and fourth operators */
    ADL_UInt8 fb_conn2_C0;
    /* Operators register data */
    ADL_Operator operators[4];
    /* Millisecond delay of sounding while key is on */
    ADL_UInt16 delay_on_ms;
    /* Millisecond delay of sounding after key off */
    ADL_UInt16 delay_off_ms;
} ADL_Instrument;

#ifdef __cplusplus
}
#endif

#endif /* ADLMIDI_H */
