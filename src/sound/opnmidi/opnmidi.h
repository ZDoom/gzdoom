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
#define OPNMIDI_VERSION_MINOR       4
#define OPNMIDI_VERSION_PATCHLEVEL  0

#define OPNMIDI_TOSTR_I(s) #s
#define OPNMIDI_TOSTR(s) OPNMIDI_TOSTR_I(s)
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


/* == Deprecated function markers == */

#if defined(_MSC_VER) /* MSVC */
#   if _MSC_VER >= 1500 /* MSVC 2008 */
                     /*! Indicates that the following function is deprecated. */
#       define OPNMIDI_DEPRECATED(message) __declspec(deprecated(message))
#   endif
#endif /* defined(_MSC_VER) */

#ifdef __clang__
#   if __has_extension(attribute_deprecated_with_message)
#       define OPNMIDI_DEPRECATED(message) __attribute__((deprecated(message)))
#   endif
#elif defined __GNUC__ /* not clang (gcc comes later since clang emulates gcc) */
#   if (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 5))
#       define OPNMIDI_DEPRECATED(message) __attribute__((deprecated(message)))
#   elif (__GNUC__ > 3 || (__GNUC__ == 3 && __GNUC_MINOR__ >= 1))
#       define OPNMIDI_DEPRECATED(message) __attribute__((__deprecated__))
#   endif /* GNUC version */
#endif /* __clang__ || __GNUC__ */

#if !defined(OPNMIDI_DEPRECATED)
#   define OPNMIDI_DEPRECATED(message)
#endif /* if !defined(OPNMIDI_DEPRECATED) */


#ifdef OPNMIDI_BUILD
#   ifndef OPNMIDI_DECLSPEC
#       if defined (_WIN32) && defined(OPNMIDI_BUILD_DLL)
#           define OPNMIDI_DECLSPEC __declspec(dllexport)
#       else
#           define OPNMIDI_DECLSPEC
#       endif
#   endif
#else
#   define OPNMIDI_DECLSPEC
#endif

/**
 * @brief Volume scaling models
 */
enum OPNMIDI_VolumeModels
{
    /*! Automatical choice by the specific bank */
    OPNMIDI_VolumeModel_AUTO = 0,
    /*! Linearized scaling model, most standard */
    OPNMIDI_VolumeModel_Generic,
    /*! Native OPN2's logarithmic volume scale */
    OPNMIDI_VolumeModel_NativeOPN2,
    /*! Logarithmic volume scale, using volume map table. Used in DMX. */
    OPNMIDI_VolumeModel_DMX,
    /*! Logarithmic volume scale, used in Apogee Sound System. */
    OPNMIDI_VolumeModel_APOGEE,
    /*! Aproximated and shorted volume map table. Similar to general, but has less granularity. */
    OPNMIDI_VolumeModel_9X
};

/**
 * @brief Sound output format
 */
enum OPNMIDI_SampleType
{
    /*! signed PCM 16-bit */
    OPNMIDI_SampleType_S16 = 0,
    /*! signed PCM 8-bit */
    OPNMIDI_SampleType_S8,
    /*! float 32-bit */
    OPNMIDI_SampleType_F32,
    /*! float 64-bit */
    OPNMIDI_SampleType_F64,
    /*! signed PCM 24-bit */
    OPNMIDI_SampleType_S24,
    /*! signed PCM 32-bit */
    OPNMIDI_SampleType_S32,
    /*! unsigned PCM 8-bit */
    OPNMIDI_SampleType_U8,
    /*! unsigned PCM 16-bit */
    OPNMIDI_SampleType_U16,
    /*! unsigned PCM 24-bit */
    OPNMIDI_SampleType_U24,
    /*! unsigned PCM 32-bit */
    OPNMIDI_SampleType_U32,
    /*! Count of available sample format types */
    OPNMIDI_SampleType_Count,
};

/**
 * @brief Sound output format context
 */
struct OPNMIDI_AudioFormat
{
    /*! type of sample */
    enum OPNMIDI_SampleType type;
    /*! size in bytes of the storage type */
    unsigned containerSize;
    /*! distance in bytes between consecutive samples */
    unsigned sampleOffset;
};

/**
 * @brief Instance of the library
 */
struct OPN2_MIDIPlayer
{
    /*! Private context descriptor */
    void *opn2_midiPlayer;
};

/* DEPRECATED */
#define opn2_setNumCards opn2_setNumChips

/**
 * @brief Sets number of emulated chips (from 1 to 100). Emulation of multiple chips extends polyphony limits
 * @param device Instance of the library
 * @param numChips Count of virtual chips to emulate
 * @return 0 on success, <0 when any error has occurred
 */
extern OPNMIDI_DECLSPEC int  opn2_setNumChips(struct OPN2_MIDIPlayer *device, int numCards);

/**
 * @brief Get current number of emulated chips
 * @param device Instance of the library
 * @return Count of working chip emulators
 */
extern OPNMIDI_DECLSPEC int  opn2_getNumChips(struct OPN2_MIDIPlayer *device);

/**
 * @brief Get obtained number of emulated chips
 * @param device Instance of the library
 * @return Count of working chip emulators
 */
extern OPNMIDI_DECLSPEC int opn2_getNumChipsObtained(struct OPN2_MIDIPlayer *device);

/**
 * @brief Reference to dynamic bank
 */
typedef struct OPN2_Bank
{
    void *pointer[3];
} OPN2_Bank;

/**
 * @brief Identifier of dynamic bank
 */
typedef struct OPN2_BankId
{
    /*! 0 if bank is melodic set, or 1 if bank is a percussion set */
    OPN2_UInt8 percussive;
    /*! Assign to MSB bank number */
    OPN2_UInt8 msb;
    /*! Assign to LSB bank number */
    OPN2_UInt8 lsb;
} OPN2_BankId;

/**
 * @brief Flags for dynamic bank access
 */
enum OPN2_BankAccessFlags
{
    /*! create bank, allocating memory as needed */
    OPNMIDI_Bank_Create = 1,
    /*! create bank, never allocating memory */
    OPNMIDI_Bank_CreateRt = 1|2
};

typedef struct OPN2_Instrument OPN2_Instrument;




/* ======== Setup ======== */

#ifdef OPNMIDI_UNSTABLE_API

/**
 * @brief Preallocates a minimum number of bank slots. Returns the actual capacity
 * @param device Instance of the library
 * @param banks Count of bank slots to pre-allocate.
 * @return actual capacity of reserved bank slots.
 */
extern OPNMIDI_DECLSPEC int opn2_reserveBanks(struct OPN2_MIDIPlayer *device, unsigned banks);
/**
 * @brief Gets the bank designated by the identifier, optionally creating if it does not exist
 * @param device Instance of the library
 * @param id Identifier of dynamic bank
 * @param flags Flags for dynamic bank access (OPN2_BankAccessFlags)
 * @param bank Reference to dynamic bank
 * @return 0 on success, <0 when any error has occurred
 */
extern OPNMIDI_DECLSPEC int opn2_getBank(struct OPN2_MIDIPlayer *device, const OPN2_BankId *id, int flags, OPN2_Bank *bank);
/**
 * @brief Gets the identifier of a bank
 * @param device Instance of the library
 * @param bank Reference to dynamic bank.
 * @param id Identifier of dynamic bank
 * @return 0 on success, <0 when any error has occurred
 */
extern OPNMIDI_DECLSPEC int opn2_getBankId(struct OPN2_MIDIPlayer *device, const OPN2_Bank *bank, OPN2_BankId *id);
/**
 * @brief Removes a bank
 * @param device Instance of the library
 * @param bank Reference to dynamic bank
 * @return 0 on success, <0 when any error has occurred
 */
extern OPNMIDI_DECLSPEC int opn2_removeBank(struct OPN2_MIDIPlayer *device, OPN2_Bank *bank);
/**
 * @brief Gets the first bank
 * @param device Instance of the library
 * @param bank Reference to dynamic bank
 * @return 0 on success, <0 when any error has occurred
 */
extern OPNMIDI_DECLSPEC int opn2_getFirstBank(struct OPN2_MIDIPlayer *device, OPN2_Bank *bank);
/**
 * @brief Iterates to the next bank
 * @param device Instance of the library
 * @param bank Reference to dynamic bank
 * @return 0 on success, <0 when any error has occurred or end has been reached.
 */
extern OPNMIDI_DECLSPEC int opn2_getNextBank(struct OPN2_MIDIPlayer *device, OPN2_Bank *bank);
/**
 * @brief Gets the nth intrument in the bank [0..127]
 * @param device Instance of the library
 * @param bank Reference to dynamic bank
 * @param index Index of the instrument
 * @param ins Instrument entry
 * @return 0 on success, <0 when any error has occurred
 */
extern OPNMIDI_DECLSPEC int opn2_getInstrument(struct OPN2_MIDIPlayer *device, const OPN2_Bank *bank, unsigned index, OPN2_Instrument *ins);
/**
 * @brief Sets the nth intrument in the bank [0..127]
 * @param device Instance of the library
 * @param bank Reference to dynamic bank
 * @param index Index of the instrument
 * @param ins Instrument structure pointer
 * @return 0 on success, <0 when any error has occurred
 *
 * This function allows to override an instrument on the fly
 */
extern OPNMIDI_DECLSPEC int opn2_setInstrument(struct OPN2_MIDIPlayer *device, OPN2_Bank *bank, unsigned index, const OPN2_Instrument *ins);

#endif  /* OPNMIDI_UNSTABLE_API */



/*Override Enable(1) or Disable(0) LFO. -1 - use bank default state*/
extern OPNMIDI_DECLSPEC void opn2_setLfoEnabled(struct OPN2_MIDIPlayer *device, int lfoEnable);

/*Get the LFO state*/
extern OPNMIDI_DECLSPEC int opn2_getLfoEnabled(struct OPN2_MIDIPlayer *device);

/*Override LFO frequency. -1 - use bank default state*/
extern OPNMIDI_DECLSPEC void opn2_setLfoFrequency(struct OPN2_MIDIPlayer *device, int lfoFrequency);

/*Get the LFO frequency*/
extern OPNMIDI_DECLSPEC int opn2_getLfoFrequency(struct OPN2_MIDIPlayer *device);

/**
 * @brief Override Enable(1) or Disable(0) scaling of modulator volumes. -1 - use bank default scaling of modulator volumes
 * @param device Instance of the library
 * @param smod 0 - disabled, 1 - enabled
 */
extern OPNMIDI_DECLSPEC void opn2_setScaleModulators(struct OPN2_MIDIPlayer *device, int smod);

/**
 * @brief Enable(1) or Disable(0) full-range brightness (MIDI CC74 used in XG music to filter result sounding) scaling
 *
 * By default, brightness affects sound between 0 and 64.
 * When this option is enabled, the brightness will use full range from 0 up to 127.
 *
 * @param device Instance of the library
 * @param fr_brightness 0 - disabled, 1 - enabled
 */
extern OPNMIDI_DECLSPEC void opn2_setFullRangeBrightness(struct OPN2_MIDIPlayer *device, int fr_brightness);

/**
 * @brief Enable or disable built-in loop (built-in loop supports 'loopStart' and 'loopEnd' tags to loop specific part)
 * @param device Instance of the library
 * @param loopEn 0 - disabled, 1 - enabled
 */
extern OPNMIDI_DECLSPEC void opn2_setLoopEnabled(struct OPN2_MIDIPlayer *device, int loopEn);

/**
 * @brief Enable or disable soft panning with chip emulators
 * @param device Instance of the library
 * @param softPanEn 0 - disabled, 1 - enabled
 */
extern OPNMIDI_DECLSPEC void opn2_setSoftPanEnabled(struct OPN2_MIDIPlayer *device, int softPanEn);

/**
 * @brief [DEPRECATED] Enable or disable Logarithmic volume changer
 *
 * This function is deprecated. Suggested replacement: `opn2_setVolumeRangeModel` with `OPNMIDI_VolumeModel_NativeOPN2` volume model value;
 */
OPNMIDI_DEPRECATED("Use `opn2_setVolumeRangeModel(device, OPNMIDI_VolumeModel_NativeOPN2)` instead")
extern OPNMIDI_DECLSPEC void opn2_setLogarithmicVolumes(struct OPN2_MIDIPlayer *device, int logvol);

/**
 * @brief Set different volume range model
 * @param device Instance of the library
 * @param volumeModel Volume model type (#OPNMIDI_VolumeModels)
 */
extern OPNMIDI_DECLSPEC void opn2_setVolumeRangeModel(struct OPN2_MIDIPlayer *device, int volumeModel);

/**
 * @brief Get the volume range model
 * @param device Instance of the library
 * @return volume model on success, <0 when any error has occurred
 */
extern OPNMIDI_DECLSPEC int opn2_getVolumeRangeModel(struct OPN2_MIDIPlayer *device);

/**
 * @brief Load WOPN bank file from File System
 *
 * Is recommended to call adl_reset() to apply changes to already-loaded file player or real-time.
 *
 * @param device Instance of the library
 * @param filePath Absolute or relative path to the WOPL bank file. UTF8 encoding is required, even on Windows.
 * @return 0 on success, <0 when any error has occurred
 */
extern OPNMIDI_DECLSPEC int opn2_openBankFile(struct OPN2_MIDIPlayer *device, const char *filePath);

/**
 * @brief Load WOPN bank file from memory data
 *
 * Is recommended to call adl_reset() to apply changes to already-loaded file player or real-time.
 *
 * @param device Instance of the library
 * @param mem Pointer to memory block where is raw data of WOPL bank file is stored
 * @param size Size of given memory block
 * @return 0 on success, <0 when any error has occurred
 */
extern OPNMIDI_DECLSPEC int opn2_openBankData(struct OPN2_MIDIPlayer *device, const void *mem, long size);


/**
 * @brief [DEPRECATED] Dummy function
 *
 * This function is deprecated. Suggested replacement: `opn2_chipEmulatorName`
 *
 * @return A string that contains a notice to use `opn2_chipEmulatorName` instead of this function.
 */
OPNMIDI_DEPRECATED("Use `adl_chipEmulatorName(device)` instead")
extern OPNMIDI_DECLSPEC const char *opn2_emulatorName();

/**
 * @brief Returns chip emulator name string
 * @param device Instance of the library
 * @return Understandable name of current OPN2 emulator
 */
extern OPNMIDI_DECLSPEC const char *opn2_chipEmulatorName(struct OPN2_MIDIPlayer *device);

/**
 * @brief List of available OPN2 emulators
 */
enum Opn2_Emulator
{
    /*! Mame YM2612 */
    OPNMIDI_EMU_MAME = 0,
    /*! Nuked OPN2 */
    OPNMIDI_EMU_NUKED,
    /*! GENS */
    OPNMIDI_EMU_GENS,
    /*! Genesis Plus GX (a fork of Mame YM2612) */
    OPNMIDI_EMU_GX,
    /*! Count instrument on the level */
    OPNMIDI_EMU_end
};

/**
 * @brief Switch the emulation core
 * @param device Instance of the library
 * @param emulator Type of emulator (#Opn2_Emulator)
 * @return 0 on success, <0 when any error has occurred
 */
extern OPNMIDI_DECLSPEC int opn2_switchEmulator(struct OPN2_MIDIPlayer *device, int emulator);

/**
 * @brief Library version context
 */
typedef struct {
    OPN2_UInt16 major;
    OPN2_UInt16 minor;
    OPN2_UInt16 patch;
} OPN2_Version;

/**
 * @brief Run emulator with PCM rate to reduce CPU usage on slow devices.
 *
 * May decrease sounding accuracy on some chip emulators.
 *
 * @param device Instance of the library
 * @param enabled 0 - disabled, 1 - enabled
 * @return 0 on success, <0 when any error has occurred
 */
extern OPNMIDI_DECLSPEC int opn2_setRunAtPcmRate(struct OPN2_MIDIPlayer *device, int enabled);

/**
 * @brief Set 4-bit device identifier. Used by the SysEx processor.
 * @param device Instance of the library
 * @param id 4-bit device identifier
 * @return 0 on success, <0 when any error has occurred
 */
extern OPNMIDI_DECLSPEC int opn2_setDeviceIdentifier(struct OPN2_MIDIPlayer *device, unsigned id);


/**
 * @section Information
 */

/**
 * @brief Returns string which contains a version number
 * @return String which contains a version of the library
 */
extern OPNMIDI_DECLSPEC const char *opn2_linkedLibraryVersion();

/**
 * @brief Returns structure which contains a version number of library
 * @return Library version context structure which contains version number of the library
 */
extern OPNMIDI_DECLSPEC const OPN2_Version *opn2_linkedVersion();

/**
 * @brief Returns string which contains last error message of initialization
 *
 * Don't use this function to get info on any function except of `opn2_init`!
 * Use `opn2_errorInfo()` to get error information while workflow
 *
 * @return String with error message related to library initialization
 */
extern OPNMIDI_DECLSPEC const char *opn2_errorString();

/**
 * @brief Returns string which contains last error message on specific device
 * @param device Instance of the library
 * @return String with error message related to last function call returned non-zero value.
 */
extern OPNMIDI_DECLSPEC const char *opn2_errorInfo(struct OPN2_MIDIPlayer *device);


/* ======== Initialization ======== */

/**
 * @brief Initialize OPNMIDI Player device
 *
 * Tip 1: You can initialize multiple instances and run them in parallel
 * Tip 2: Library is NOT thread-safe, therefore don't use same instance in different threads or use mutexes
 * Tip 3: Changing of sample rate on the fly is not supported. Re-create the instance again.
 *
 * @param sample_rate Output sample rate
 * @return Instance of the library. If NULL was returned, check the `adl_errorString` message for more info.
 */
extern OPNMIDI_DECLSPEC struct OPN2_MIDIPlayer *opn2_init(long sample_rate);

/**
 * @brief Close and delete OPNMIDI device
 * @param device Instance of the library
 */
extern OPNMIDI_DECLSPEC void opn2_close(struct OPN2_MIDIPlayer *device);


/* ======== MIDI Sequencer ======== */

/**
 * @brief Load MIDI (or any other supported format) file from File System
 *
 * Available when library is built with built-in MIDI Sequencer support.
 *
 * @param device Instance of the library
 * @param filePath Absolute or relative path to the music file. UTF8 encoding is required, even on Windows.
 * @return 0 on success, <0 when any error has occurred
 */
extern OPNMIDI_DECLSPEC int opn2_openFile(struct OPN2_MIDIPlayer *device, const char *filePath);

/**
 * @brief Load MIDI (or any other supported format) file from memory data
 *
 * Available when library is built with built-in MIDI Sequencer support.
 *
 * @param device Instance of the library
 * @param mem Pointer to memory block where is raw data of music file is stored
 * @param size Size of given memory block
 * @return 0 on success, <0 when any error has occurred
 */
extern OPNMIDI_DECLSPEC int opn2_openData(struct OPN2_MIDIPlayer *device, const void *mem, unsigned long size);

/**
 * @brief Resets MIDI player (per-channel setup) into initial state
 * @param device Instance of the library
 */
extern OPNMIDI_DECLSPEC void opn2_reset(struct OPN2_MIDIPlayer *device);

/**
 * @brief Get total time length of current song
 *
 * Available when library is built with built-in MIDI Sequencer support.
 *
 * @param device Instance of the library
 * @return Total song length in seconds
 */
extern OPNMIDI_DECLSPEC double opn2_totalTimeLength(struct OPN2_MIDIPlayer *device);

/**
 * @brief Get loop start time if presented.
 *
 * Available when library is built with built-in MIDI Sequencer support.
 *
 * @param device Instance of the library
 * @return Time position in seconds of loop start point, or -1 when file has no loop points
 */
extern OPNMIDI_DECLSPEC double opn2_loopStartTime(struct OPN2_MIDIPlayer *device);

/**
 * @brief Get loop endtime if presented.
 *
 * Available when library is built with built-in MIDI Sequencer support.
 *
 * @param device Instance of the library
 * @return Time position in seconds of loop end point, or -1 when file has no loop points
 */
extern OPNMIDI_DECLSPEC double opn2_loopEndTime(struct OPN2_MIDIPlayer *device);

/**
 * @brief Get current time position in seconds
 *
 * Available when library is built with built-in MIDI Sequencer support.
 *
 * @param device Instance of the library
 * @return Current time position in seconds
 */
extern OPNMIDI_DECLSPEC double opn2_positionTell(struct OPN2_MIDIPlayer *device);

/**
 * @brief Jump to absolute time position in seconds
 *
 * Available when library is built with built-in MIDI Sequencer support.
 *
 * @param device Instance of the library
 * @param seconds Destination time position in seconds to seek
 */
extern OPNMIDI_DECLSPEC void opn2_positionSeek(struct OPN2_MIDIPlayer *device, double seconds);

/**
 * @brief Reset MIDI track position to begin
 *
 * Available when library is built with built-in MIDI Sequencer support.
 *
 * @param device Instance of the library
 */
extern OPNMIDI_DECLSPEC void opn2_positionRewind(struct OPN2_MIDIPlayer *device);

/**
 * @brief Set tempo multiplier
 *
 * Available when library is built with built-in MIDI Sequencer support.
 *
 * @param device Instance of the library
 * @param tempo Tempo multiplier value: 1.0 - original tempo, >1 - play faster, <1 - play slower
 */
extern OPNMIDI_DECLSPEC void opn2_setTempo(struct OPN2_MIDIPlayer *device, double tempo);

/**
 * @brief Returns 1 if music position has reached end
 * @param device Instance of the library
 * @return 1 when end of sing has been reached, otherwise, 0 will be returned. <0 is returned on any error
 */
extern OPNMIDI_DECLSPEC int opn2_atEnd(struct OPN2_MIDIPlayer *device);

/**
 * @brief Returns the number of tracks of the current sequence
 * @param device Instance of the library
 * @return Count of tracks in the current sequence
 */
extern OPNMIDI_DECLSPEC size_t opn2_trackCount(struct OPN2_MIDIPlayer *device);



/* ======== Meta-Tags ======== */

/**
 * @brief Returns string which contains a music title
 * @param device Instance of the library
 * @return A string that contains music title
 */
extern OPNMIDI_DECLSPEC const char *opn2_metaMusicTitle(struct OPN2_MIDIPlayer *device);

/**
 * @brief Returns string which contains a copyright string*
 * @param device Instance of the library
 * @return A string that contains copyright notice, otherwise NULL
 */
extern OPNMIDI_DECLSPEC const char *opn2_metaMusicCopyright(struct OPN2_MIDIPlayer *device);

/**
 * @brief Returns count of available track titles
 *
 * NOTE: There are CAN'T be associated with channel in any of event or note hooks
 *
 * @param device Instance of the library
 * @return Count of available MIDI tracks, otherwise NULL
 */
extern OPNMIDI_DECLSPEC size_t opn2_metaTrackTitleCount(struct OPN2_MIDIPlayer *device);

/**
 * @brief Get track title by index
 * @param device Instance of the library
 * @param index Index of the track to retreive the title
 * @return A string that contains track title, otherwise NULL.
 */
extern OPNMIDI_DECLSPEC const char *opn2_metaTrackTitle(struct OPN2_MIDIPlayer *device, size_t index);

/**
 * @brief MIDI Marker structure
 */
struct Opn2_MarkerEntry
{
    /*! MIDI Marker title */
    const char      *label;
    /*! Absolute time position of the marker in seconds */
    double          pos_time;
    /*! Absolute time position of the marker in MIDI ticks */
    unsigned long   pos_ticks;
};

/**
 * @brief Returns count of available markers
 * @param device Instance of the library
 * @return Count of available MIDI markers
 */
extern OPNMIDI_DECLSPEC size_t opn2_metaMarkerCount(struct OPN2_MIDIPlayer *device);

/**
 * @brief Returns the marker entry
 * @param device Instance of the library
 * @param index Index of the marker to retreive it.
 * @return MIDI Marker description structure.
 */
extern OPNMIDI_DECLSPEC struct Opn2_MarkerEntry opn2_metaMarker(struct OPN2_MIDIPlayer *device, size_t index);




/* ======== Audio output Generation ======== */

/**
 * @brief Generate PCM signed 16-bit stereo audio output and iterate MIDI timers
 *
 * Use this function when you are playing MIDI file loaded by `adl_openFile` or by `adl_openData`
 * with using of built-in MIDI sequencer.
 *
 * Don't use count of frames, use instead count of samples. One frame is two samples.
 * So, for example, if you want to take 10 frames, you must to request amount of 20 samples!
 *
 * Available when library is built with built-in MIDI Sequencer support.
 *
 * @param device Instance of the library
 * @param sampleCount Count of samples (not frames!)
 * @param out Pointer to output with 16-bit stereo PCM output
 * @return Count of given samples, otherwise, 0 or when catching an error while playing
 */
extern OPNMIDI_DECLSPEC int  opn2_play(struct OPN2_MIDIPlayer *device, int sampleCount, short *out);

/**
 * @brief Generate PCM stereo audio output in sample format declared by given context and iterate MIDI timers
 *
 * Use this function when you are playing MIDI file loaded by `adl_openFile` or by `adl_openData`
 * with using of built-in MIDI sequencer.
 *
 * Don't use count of frames, use instead count of samples. One frame is two samples.
 * So, for example, if you want to take 10 frames, you must to request amount of 20 samples!
 *
 * Available when library is built with built-in MIDI Sequencer support.
 *
 * @param device Instance of the library
 * @param sampleCount Count of samples (not frames!)
 * @param left Left channel buffer output (Must be casted into bytes array)
 * @param right Right channel buffer output (Must be casted into bytes array)
 * @param format Destination PCM format format context
 * @return Count of given samples, otherwise, 0 or when catching an error while playing
 */
extern OPNMIDI_DECLSPEC int  opn2_playFormat(struct OPN2_MIDIPlayer *device, int sampleCount, OPN2_UInt8 *left, OPN2_UInt8 *right, const struct OPNMIDI_AudioFormat *format);

/**
 * @brief Generate PCM signed 16-bit stereo audio output without iteration of MIDI timers
 *
 * Use this function when you are using library as Real-Time MIDI synthesizer or with
 * an external MIDI sequencer. You must to request the amount of samples which is equal
 * to the delta between of MIDI event rows. One MIDI row is a group of MIDI events
 * are having zero delta/delay between each other. When you are receiving events in
 * real time, request the minimal possible delay value.
 *
 * Don't use count of frames, use instead count of samples. One frame is two samples.
 * So, for example, if you want to take 10 frames, you must to request amount of 20 samples!
 *
 * @param device Instance of the library
 * @param sampleCount
 * @param out Pointer to output with 16-bit stereo PCM output
 * @return Count of given samples, otherwise, 0 or when catching an error while playing
 */
extern OPNMIDI_DECLSPEC int  opn2_generate(struct OPN2_MIDIPlayer *device, int sampleCount, short *out);

/**
 * @brief Generate PCM stereo audio output in sample format declared by given context without iteration of MIDI timers
 *
 * Use this function when you are using library as Real-Time MIDI synthesizer or with
 * an external MIDI sequencer. You must to request the amount of samples which is equal
 * to the delta between of MIDI event rows. One MIDI row is a group of MIDI events
 * are having zero delta/delay between each other. When you are receiving events in
 * real time, request the minimal possible delay value.
 *
 * Don't use count of frames, use instead count of samples. One frame is two samples.
 * So, for example, if you want to take 10 frames, you must to request amount of 20 samples!
 *
 * @param device Instance of the library
 * @param sampleCount
 * @param left Left channel buffer output (Must be casted into bytes array)
 * @param right Right channel buffer output (Must be casted into bytes array)
 * @param format Destination PCM format format context
 * @return Count of given samples, otherwise, 0 or when catching an error while playing
 */
extern OPNMIDI_DECLSPEC int  opn2_generateFormat(struct OPN2_MIDIPlayer *device, int sampleCount, OPN2_UInt8 *left, OPN2_UInt8 *right, const struct OPNMIDI_AudioFormat *format);

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
extern OPNMIDI_DECLSPEC double opn2_tickEvents(struct OPN2_MIDIPlayer *device, double seconds, double granuality);

/**
 * @brief Track options
 */
enum OPNMIDI_TrackOptions
{
    /*! Enabled track */
    OPNMIDI_TrackOption_On   = 1,
    /*! Disabled track */
    OPNMIDI_TrackOption_Off  = 2,
    /*! Solo track */
    OPNMIDI_TrackOption_Solo = 3,
};

/**
 * @brief Sets options on a track of the current sequence
 * @param device Instance of the library
 * @param trackNumber Identifier of the designated track.
 * @return 0 on success, <0 when any error has occurred
 */
extern OPNMIDI_DECLSPEC int opn2_setTrackOptions(struct OPN2_MIDIPlayer *device, size_t trackNumber, unsigned trackOptions);




/* ======== Real-Time MIDI ======== */

/**
 * @brief Force Off all notes on all channels
 * @param device Instance of the library
 */
extern OPNMIDI_DECLSPEC void opn2_panic(struct OPN2_MIDIPlayer *device);

/**
 * @brief Reset states of all controllers on all MIDI channels
 * @param device Instance of the library
 */
extern OPNMIDI_DECLSPEC void opn2_rt_resetState(struct OPN2_MIDIPlayer *device);

/**
 * @brief Turn specific MIDI note ON
 * @param device Instance of the library
 * @param channel Target MIDI channel [Between 0 and 16]
 * @param note Note number to on [Between 0 and 127]
 * @param velocity Velocity level [Between 0 and 127]
 * @return 1 when note was successfully started, 0 when note was rejected by any reason.
 */
extern OPNMIDI_DECLSPEC int opn2_rt_noteOn(struct OPN2_MIDIPlayer *device, OPN2_UInt8 channel, OPN2_UInt8 note, OPN2_UInt8 velocity);

/**
 * @brief Turn specific MIDI note OFF
 * @param device Instance of the library
 * @param channel Target MIDI channel [Between 0 and 16]
 * @param note Note number to off [Between 0 and 127]
 */
extern OPNMIDI_DECLSPEC void opn2_rt_noteOff(struct OPN2_MIDIPlayer *device, OPN2_UInt8 channel, OPN2_UInt8 note);

/**
 * @brief Set note after-touch
 * @param device Instance of the library
 * @param channel Target MIDI channel [Between 0 and 16]
 * @param note Note number to affect by aftertouch event [Between 0 and 127]
 * @param atVal After-Touch value [Between 0 and 127]
 */
extern OPNMIDI_DECLSPEC void opn2_rt_noteAfterTouch(struct OPN2_MIDIPlayer *device, OPN2_UInt8 channel, OPN2_UInt8 note, OPN2_UInt8 atVal);

/**
 * @brief Set channel after-touch
 * @param device Instance of the library
 * @param channel Target MIDI channel [Between 0 and 16]
 * @param atVal After-Touch level [Between 0 and 127]
 */
extern OPNMIDI_DECLSPEC void opn2_rt_channelAfterTouch(struct OPN2_MIDIPlayer *device, OPN2_UInt8 channel, OPN2_UInt8 atVal);

/**
 * @brief Apply controller change
 * @param device Instance of the library
 * @param channel Target MIDI channel [Between 0 and 16]
 * @param type Type of the controller [Between 0 and 255]
 * @param value Value of the controller event [Between 0 and 127]
 */
extern OPNMIDI_DECLSPEC void opn2_rt_controllerChange(struct OPN2_MIDIPlayer *device, OPN2_UInt8 channel, OPN2_UInt8 type, OPN2_UInt8 value);

/**
 * @brief Apply patch change
 * @param device Instance of the library
 * @param channel Target MIDI channel [Between 0 and 16]
 * @param patch Patch number [Between 0 and 127]
 */
extern OPNMIDI_DECLSPEC void opn2_rt_patchChange(struct OPN2_MIDIPlayer *device, OPN2_UInt8 channel, OPN2_UInt8 patch);

/**
 * @brief Apply pitch bend change
 * @param device Instance of the library
 * @param channel Target MIDI channel [Between 0 and 16]
 * @param pitch 24-bit pitch bend value
 */
extern OPNMIDI_DECLSPEC void opn2_rt_pitchBend(struct OPN2_MIDIPlayer *device, OPN2_UInt8 channel, OPN2_UInt16 pitch);

/**
 * @brief Apply pitch bend change
 * @param device Instance of the library
 * @param channel Target MIDI channel [Between 0 and 16]
 * @param msb MSB part of 24-bit pitch bend value
 * @param lsb LSB part of 24-bit pitch bend value
 */
extern OPNMIDI_DECLSPEC void opn2_rt_pitchBendML(struct OPN2_MIDIPlayer *device, OPN2_UInt8 channel, OPN2_UInt8 msb, OPN2_UInt8 lsb);

/**
 * @brief Change LSB of the bank number (Alias to CC-32 event)
 * @param device Instance of the library
 * @param channel Target MIDI channel [Between 0 and 16]
 * @param lsb LSB value of the MIDI bank number
 */
extern OPNMIDI_DECLSPEC void opn2_rt_bankChangeLSB(struct OPN2_MIDIPlayer *device, OPN2_UInt8 channel, OPN2_UInt8 lsb);

/**
 * @brief Change MSB of the bank (Alias to CC-0 event)
 * @param device Instance of the library
 * @param channel Target MIDI channel [Between 0 and 16]
 * @param msb MSB value of the MIDI bank number
 */
extern OPNMIDI_DECLSPEC void opn2_rt_bankChangeMSB(struct OPN2_MIDIPlayer *device, OPN2_UInt8 channel, OPN2_UInt8 msb);

/**
 * @brief Change bank by absolute signed value
 * @param device Instance of the library
 * @param channel Target MIDI channel [Between 0 and 16]
 * @param bank Bank number as concoctated signed 16-bit value of MSB and LSB parts.
 */
extern OPNMIDI_DECLSPEC void opn2_rt_bankChange(struct OPN2_MIDIPlayer *device, OPN2_UInt8 channel, OPN2_SInt16 bank);

/**
 * @brief Perform a system exclusive message
 * @param device Instance of the library
 * @param msg Raw SysEx message buffer (must begin with 0xF0 and end with 0xF7)
 * @param size Size of given SysEx message buffer
 * @return 1 when SysEx message was successfully processed, 0 when SysEx message was rejected by any reason
 */
extern OPNMIDI_DECLSPEC int opn2_rt_systemExclusive(struct OPN2_MIDIPlayer *device, const OPN2_UInt8 *msg, size_t size);

/* ======== Hooks and debugging ======== */

/**
 * @brief Raw event callback
 * @param userdata Pointer to user data (usually, context of someting)
 * @param type MIDI event type
 * @param subtype MIDI event sub-type (special events only)
 * @param channel MIDI channel
 * @param data Raw event data
 * @param len Length of event data
 */
typedef void (*OPN2_RawEventHook)(void *userdata, OPN2_UInt8 type, OPN2_UInt8 subtype, OPN2_UInt8 channel, const OPN2_UInt8 *data, size_t len);
/**
 * @brief Note on/off callback
 * @param userdata Pointer to user data (usually, context of someting)
 * @param adlchn Chip channel where note was played
 * @param note Note number [between 0 and 127]
 * @param pressure Velocity level, or -1 when it's note off event
 * @param bend Pitch bend offset value
 */
typedef void (*OPN2_NoteHook)(void *userdata, int adlchn, int note, int ins, int pressure, double bend);

/**
 * @brief Debug messages callback
 * @param userdata Pointer to user data (usually, context of someting)
 * @param fmt Format strign output (in context of `printf()` standard function)
 */
typedef void (*OPN2_DebugMessageHook)(void *userdata, const char *fmt, ...);

/**
 * @brief Set raw MIDI event hook
 * @param device Instance of the library
 * @param rawEventHook Pointer to the callback function which will be called on every MIDI event
 * @param userData Pointer to user data which will be passed through the callback.
 */
extern OPNMIDI_DECLSPEC void opn2_setRawEventHook(struct OPN2_MIDIPlayer *device, OPN2_RawEventHook rawEventHook, void *userData);

/**
 * @brief Set note hook
 * @param device Instance of the library
 * @param noteHook Pointer to the callback function which will be called on every noteOn MIDI event
 * @param userData Pointer to user data which will be passed through the callback.
 */
extern OPNMIDI_DECLSPEC void opn2_setNoteHook(struct OPN2_MIDIPlayer *device, OPN2_NoteHook noteHook, void *userData);

/**
 * @brief Set debug message hook
 * @param device Instance of the library
 * @param debugMessageHook Pointer to the callback function which will be called on every debug message
 * @param userData Pointer to user data which will be passed through the callback.
 */
extern OPNMIDI_DECLSPEC void opn2_setDebugMessageHook(struct OPN2_MIDIPlayer *device, OPN2_DebugMessageHook debugMessageHook, void *userData);


/**
 * @brief Get a textual description of the channel state. For display only.
 * @param device Instance of the library
 * @param text Destination char buffer for channel usage state. Every entry is assigned to the chip channel.
 * @param attr Destination char buffer for additional attributes like MIDI channel number that uses this chip channel.
 * @param size Size of given buffers (both text and attr are must have same size!)
 * @return 0 on success, <0 when any error has occurred
 *
 * Every character in the `text` buffer means the type of usage:
 * ```
 *  `-` - channel is unused (free)
 *  `+` - channel is used by regular voice
 *  `@` - channel is used to play automatic arpeggio on chip channels overflow
 * ```
 *
 * The `attr` field receives the MIDI channel from which the chip channel is used.
 * To get the valid MIDI channel you will need to apply the & 0x0F mask to every value.
 */
extern OPNMIDI_DECLSPEC int opn2_describeChannels(struct OPN2_MIDIPlayer *device, char *text, char *attr, size_t size);




/* ======== Instrument structures ======== */

/**
 * @brief Version of the instrument data format
 */
enum
{
    OPNMIDI_InstrumentVersion = 0
};

/**
 * @brief Instrument flags
 */
typedef enum OPN2_InstrumentFlags
{
    OPNMIDI_Ins_Pseudo8op  = 0x01, /*Reserved for future use, not implemented yet*/
    OPNMIDI_Ins_IsBlank    = 0x02
} OPN2_InstrumentFlags;

/**
 * @brief Operator structure, part of Instrument structure
 */
typedef struct OPN2_Operator
{
    /* Detune and frequency multiplication register data */
    OPN2_UInt8 dtfm_30;
    /* Total level register data */
    OPN2_UInt8 level_40;
    /* Rate scale and attack register data */
    OPN2_UInt8 rsatk_50;
    /* Amplitude modulation enable and Decay-1 register data */
    OPN2_UInt8 amdecay1_60;
    /* Decay-2 register data */
    OPN2_UInt8 decay2_70;
    /* Sustain and Release register data */
    OPN2_UInt8 susrel_80;
    /* SSG-EG register data */
    OPN2_UInt8 ssgeg_90;
} OPN2_Operator;

/**
 * @brief Instrument structure
 */
typedef struct OPN2_Instrument
{
    /*! Version of the instrument object */
    int version;
    /* MIDI note key (half-tone) offset for an instrument (or a first voice in pseudo-4-op mode) */
    OPN2_SInt16 note_offset;
    /* Reserved */
    OPN2_SInt8  midi_velocity_offset;
    /* Percussion MIDI base tone number at which this drum will be played */
    OPN2_UInt8 percussion_key_number;
    /* Instrument flags */
    OPN2_UInt8 inst_flags;
    /* Feedback and Algorithm register data */
    OPN2_UInt8 fbalg;
    /* LFO Sensitivity register data */
    OPN2_UInt8 lfosens;
    /* Operators register data */
    OPN2_Operator operators[4];
    /* Millisecond delay of sounding while key is on */
    OPN2_UInt16 delay_on_ms;
    /* Millisecond delay of sounding after key off */
    OPN2_UInt16 delay_off_ms;
} OPN2_Instrument;

#ifdef __cplusplus
}
#endif

#endif /* OPNMIDI_H */
