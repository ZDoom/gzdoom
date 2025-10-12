/*
 * libADLMIDI is a free Software MIDI synthesizer library with OPL3 emulation
 *
 * Original ADLMIDI code: Copyright (c) 2010-2014 Joel Yliluoma <bisqwit@iki.fi>
 * ADLMIDI Library API:   Copyright (c) 2015-2025 Vitaly Novichkov <admin@wohlnet.ru>
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
#define ADLMIDI_VERSION_MINOR       6
#define ADLMIDI_VERSION_PATCHLEVEL  1

#define ADLMIDI_TOSTR_I(s) #s
#define ADLMIDI_TOSTR(s) ADLMIDI_TOSTR_I(s)
#define ADLMIDI_VERSION \
        ADLMIDI_TOSTR(ADLMIDI_VERSION_MAJOR) "." \
        ADLMIDI_TOSTR(ADLMIDI_VERSION_MINOR) "." \
        ADLMIDI_TOSTR(ADLMIDI_VERSION_PATCHLEVEL)

#define ADL_CHIP_SAMPLE_RATE        49716

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

/* == Deprecated function markers == */

#if defined(_MSC_VER) /* MSVC */
#   if _MSC_VER >= 1500 /* MSVC 2008 */
                     /*! Indicates that the following function is deprecated. */
#       define ADLMIDI_DEPRECATED(message) __declspec(deprecated(message))
#   endif
#endif /* defined(_MSC_VER) */

#ifdef __clang__
#   if __has_extension(attribute_deprecated_with_message)
#       define ADLMIDI_DEPRECATED(message) __attribute__((deprecated(message)))
#   endif
#elif defined __GNUC__ /* not clang (gcc comes later since clang emulates gcc) */
#   if (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 5))
#       define ADLMIDI_DEPRECATED(message) __attribute__((deprecated(message)))
#   elif (__GNUC__ > 3 || (__GNUC__ == 3 && __GNUC_MINOR__ >= 1))
#       define ADLMIDI_DEPRECATED(message) __attribute__((__deprecated__))
#   endif /* GNUC version */
#endif /* __clang__ || __GNUC__ */

#if !defined(ADLMIDI_DEPRECATED)
#   define ADLMIDI_DEPRECATED(message)
#endif /* if !defined(ADLMIDI_DEPRECATED) */


#ifdef ADLMIDI_BUILD
#   ifndef ADLMIDI_DECLSPEC
#       if defined (_WIN32) && defined(ADLMIDI_BUILD_DLL)
#           define ADLMIDI_DECLSPEC __declspec(dllexport)
#       else
#           define ADLMIDI_DECLSPEC
#       endif
#   endif
#else
#   define ADLMIDI_DECLSPEC
#endif


/**
 * @brief Volume scaling models
 */
enum ADLMIDI_VolumeModels
{
    /*! Automatical choice by the specific bank */
    ADLMIDI_VolumeModel_AUTO = 0,
    /*! Linearized scaling model, most standard */
    ADLMIDI_VolumeModel_Generic = 1,
    /*! Native OPL3's logarithmic volume scale */
    ADLMIDI_VolumeModel_NativeOPL3 = 2,
    /*! Native OPL3's logarithmic volume scale. Alias. */
    ADLMIDI_VolumeModel_CMF = ADLMIDI_VolumeModel_NativeOPL3,
    /*! Logarithmic volume scale, using volume map table. Used in DMX. */
    ADLMIDI_VolumeModel_DMX = 3,
    /*! Logarithmic volume scale, used in Apogee Sound System. */
    ADLMIDI_VolumeModel_APOGEE = 4,
    /*! Aproximated and shorted volume map table (SB16 driver). Similar to general, but has less granularity. */
    ADLMIDI_VolumeModel_9X = 5,
    /*! DMX model with a fixed bug of AM voices */
    ADLMIDI_VolumeModel_DMX_Fixed = 6,
    /*! Apogee model with a fixed bug of AM voices*/
    ADLMIDI_VolumeModel_APOGEE_Fixed = 7,
    /*! Audio Interface Library volume scaling model */
    ADLMIDI_VolumeModel_AIL = 8,
    /*! Aproximated and shorted volume map table (Generic FM driver). Similar to general, but has less granularity. */
    ADLMIDI_VolumeModel_9X_GENERIC_FM = 9,
    /*! HMI Sound Operating System volume scaling model */
    ADLMIDI_VolumeModel_HMI = 10,
    /*! HMI Sound Operating System volume scaling model, older variant with bugs */
    ADLMIDI_VolumeModel_HMI_OLD = 11,
    /*! Count of available volume model modes */
    ADLMIDI_VolumeModel_Count
};

/*!
 * \brief Algorithms of channel allocation for new notes
 */
enum ADLMIDI_ChannelAlloc
{
    /*! Automatical choise of the method according to the volume model and internal preferrences */
    ADLMIDI_ChanAlloc_AUTO = -1,
    /*! Take only channels that has expired sounding delay */
    ADLMIDI_ChanAlloc_OffDelay,
    /*! Take any first released channel with the same instrument */
    ADLMIDI_ChanAlloc_SameInst,
    /*! Take any first released channel */
    ADLMIDI_ChanAlloc_AnyReleased,
    /*! Count of available channel allocation modes */
    ADLMIDI_ChanAlloc_Count
};

/**
 * @brief Sound output format
 */
enum ADLMIDI_SampleType
{
    /*! signed PCM 16-bit */
    ADLMIDI_SampleType_S16 = 0,
    /*! signed PCM 8-bit */
    ADLMIDI_SampleType_S8,
    /*! float 32-bit */
    ADLMIDI_SampleType_F32,
    /*! float 64-bit */
    ADLMIDI_SampleType_F64,
    /*! signed PCM 24-bit */
    ADLMIDI_SampleType_S24,
    /*! signed PCM 32-bit */
    ADLMIDI_SampleType_S32,
    /*! unsigned PCM 8-bit */
    ADLMIDI_SampleType_U8,
    /*! unsigned PCM 16-bit */
    ADLMIDI_SampleType_U16,
    /*! unsigned PCM 24-bit */
    ADLMIDI_SampleType_U24,
    /*! unsigned PCM 32-bit */
    ADLMIDI_SampleType_U32,
    /*! Count of available sample format types */
    ADLMIDI_SampleType_Count
};

/**
 * @brief Sound output format context
 */
struct ADLMIDI_AudioFormat
{
    /*! type of sample */
    enum ADLMIDI_SampleType type;
    /*! size in bytes of the storage type */
    unsigned containerSize;
    /*! distance in bytes between consecutive samples */
    unsigned sampleOffset;
};

/**
 * @brief Instance of the library
 */
struct ADL_MIDIPlayer
{
    /*! Private context descriptor */
    void *adl_midiPlayer;
};

/* DEPRECATED */
#define adl_setNumCards adl_setNumChips

/**
 * @brief Sets number of emulated chips (from 1 to 100). Emulation of multiple chips extends polyphony limits
 * @param device Instance of the library
 * @param numChips Count of virtual chips to emulate
 * @return 0 on success, <0 when any error has occurred
 */
extern ADLMIDI_DECLSPEC int adl_setNumChips(struct ADL_MIDIPlayer *device, int numChips);

/**
 * @brief Get current number of emulated chips
 * @param device Instance of the library
 * @return Count of working chip emulators
 */
extern ADLMIDI_DECLSPEC int adl_getNumChips(struct ADL_MIDIPlayer *device);

/**
 * @brief Get obtained number of emulated chips
 * @param device Instance of the library
 * @return Count of working chip emulators
 */
extern ADLMIDI_DECLSPEC int adl_getNumChipsObtained(struct ADL_MIDIPlayer *device);

/**
 * @brief Sets a number of the patches bank from 0 to N banks.
 *
 * Is recommended to call adl_reset() to apply changes to already-loaded file player or real-time.
 *
 * @param device Instance of the library
 * @param bank Number of embedded bank
 * @return 0 on success, <0 when any error has occurred
 */
extern ADLMIDI_DECLSPEC int adl_setBank(struct ADL_MIDIPlayer *device, int bank);

/**
 * @brief Returns total number of available banks
 * @return Total number of available embedded banks
 */
extern ADLMIDI_DECLSPEC int adl_getBanksCount();

/**
 * @brief Returns pointer to array of names of every bank
 * @return Array of strings containing the name of every embedded bank
 */
extern ADLMIDI_DECLSPEC const char *const *adl_getBankNames();

/**
 * @brief Reference to dynamic bank
 */
typedef struct ADL_Bank
{
    void *pointer[3];
} ADL_Bank;

/**
 * @brief Identifier of dynamic bank
 */
typedef struct ADL_BankId
{
    /*! 0 if bank is melodic set, or 1 if bank is a percussion set */
    ADL_UInt8 percussive;
    /*! Assign to MSB bank number */
    ADL_UInt8 msb;
    /*! Assign to LSB bank number */
    ADL_UInt8 lsb;
} ADL_BankId;

/**
 * @brief Flags for dynamic bank access
 */
enum ADL_BankAccessFlags
{
    /*! create bank, allocating memory as needed */
    ADLMIDI_Bank_Create = 1,
    /*! create bank, never allocating memory */
    ADLMIDI_Bank_CreateRt = 1|2
};


/* ======== Instrument structures ======== */

/**
 * @brief Version of the instrument data format
 */
enum
{
    ADLMIDI_InstrumentVersion = 0
};

/**
 * @brief Instrument flags
 */
typedef enum ADL_InstrumentFlags
{
    /*! Is two-operator single-voice instrument (no flags) */
    ADLMIDI_Ins_2op        = 0x00,
    /*! Is true four-operator instrument */
    ADLMIDI_Ins_4op        = 0x01,
    /*! Is pseudo four-operator (two 2-operator voices) instrument */
    ADLMIDI_Ins_Pseudo4op  = 0x02,
    /*! Is a blank instrument entry */
    ADLMIDI_Ins_IsBlank    = 0x04,

    /*! RythmMode flags mask */
    ADLMIDI_Ins_RhythmModeMask = 0x38,

    /*! Mask of the flags range */
    ADLMIDI_Ins_ALL_MASK   = 0x07
} ADL_InstrumentFlags;

/**
 * @brief Rhythm-mode drum type
 */
typedef enum ADL_RhythmMode
{
    /*! RythmMode: BassDrum */
    ADLMIDI_RM_BassDrum  = 0x08,
    /*! RythmMode: Snare */
    ADLMIDI_RM_Snare     = 0x10,
    /*! RythmMode: TomTom */
    ADLMIDI_RM_TomTom    = 0x18,
    /*! RythmMode: Cymbal */
    ADLMIDI_RM_Cymbal    = 0x20,
    /*! RythmMode: HiHat */
    ADLMIDI_RM_HiHat     = 0x28
} ADL_RhythmMode;


/**
 * @brief Operator structure, part of Instrument structure
 */
typedef struct ADL_Operator
{
    /*! AM/Vib/Env/Ksr/FMult characteristics */
    ADL_UInt8 avekf_20;
    /*! Key Scale Level / Total level register data */
    ADL_UInt8 ksl_l_40;
    /*! Attack / Decay */
    ADL_UInt8 atdec_60;
    /*! Systain and Release register data */
    ADL_UInt8 susrel_80;
    /*! Wave form */
    ADL_UInt8 waveform_E0;
} ADL_Operator;

/**
 * @brief Instrument structure
 */
typedef struct ADL_Instrument
{
    /*! Version of the instrument object */
    int version;
    /*! MIDI note key (half-tone) offset for an instrument (or a first voice in pseudo-4-op mode) */
    ADL_SInt16 note_offset1;
    /*! MIDI note key (half-tone) offset for a second voice in pseudo-4-op mode */
    ADL_SInt16 note_offset2;
    /*! MIDI note velocity offset (taken from Apogee TMB format) */
    ADL_SInt8  midi_velocity_offset;
    /*! Second voice detune level (taken from DMX OP2) */
    ADL_SInt8  second_voice_detune;
    /*! Percussion MIDI base tone number at which this drum will be played */
    ADL_UInt8 percussion_key_number;
    /**
     * @var inst_flags
     * @brief Instrument flags
     *
     * Enums: #ADL_InstrumentFlags and #ADL_RhythmMode
     *
     * Bitwise flags bit map:
     * ```
     * [0EEEDCBA]
     *  A) 0x00 - 2-operator mode
     *  B) 0x01 - 4-operator mode
     *  C) 0x02 - pseudo-4-operator (two 2-operator voices) mode
     *  D) 0x04 - is 'blank' instrument (instrument which has no sound)
     *  E) 0x38 - Reserved for rhythm-mode percussion type number (three bits number)
     *     -> 0x00 - Melodic or Generic drum (rhythm-mode is disabled)
     *     -> 0x08 - is Bass drum
     *     -> 0x10 - is Snare
     *     -> 0x18 - is Tom-tom
     *     -> 0x20 - is Cymbal
     *     -> 0x28 - is Hi-hat
     *  0) Reserved / Unused
     * ```
     */
    ADL_UInt8 inst_flags;
    /*! Feedback&Connection register for first and second operators */
    ADL_UInt8 fb_conn1_C0;
    /*! Feedback&Connection register for third and fourth operators */
    ADL_UInt8 fb_conn2_C0;
    /*! Operators register data */
    ADL_Operator operators[4];
    /*! Millisecond delay of sounding while key is on */
    ADL_UInt16 delay_on_ms;
    /*! Millisecond delay of sounding after key off */
    ADL_UInt16 delay_off_ms;
} ADL_Instrument;




/* ======== Setup ======== */

/**
 * @brief Preallocates a minimum number of bank slots. Returns the actual capacity
 * @param device Instance of the library
 * @param banks Count of bank slots to pre-allocate.
 * @return actual capacity of reserved bank slots.
 */
extern ADLMIDI_DECLSPEC int adl_reserveBanks(struct ADL_MIDIPlayer *device, unsigned banks);
/**
 * @brief Gets the bank designated by the identifier, optionally creating if it does not exist
 * @param device Instance of the library
 * @param id Identifier of dynamic bank
 * @param flags Flags for dynamic bank access (ADL_BankAccessFlags)
 * @param bank Reference to dynamic bank
 * @return 0 on success, <0 when any error has occurred
 */
extern ADLMIDI_DECLSPEC int adl_getBank(struct ADL_MIDIPlayer *device, const ADL_BankId *id, int flags, ADL_Bank *bank);
/**
 * @brief Gets the identifier of a bank
 * @param device Instance of the library
 * @param bank Reference to dynamic bank.
 * @param id Identifier of dynamic bank
 * @return 0 on success, <0 when any error has occurred
 */
extern ADLMIDI_DECLSPEC int adl_getBankId(struct ADL_MIDIPlayer *device, const ADL_Bank *bank, ADL_BankId *id);
/**
 * @brief Removes a bank
 * @param device Instance of the library
 * @param bank Reference to dynamic bank
 * @return 0 on success, <0 when any error has occurred
 */
extern ADLMIDI_DECLSPEC int adl_removeBank(struct ADL_MIDIPlayer *device, ADL_Bank *bank);
/**
 * @brief Gets the first bank
 * @param device Instance of the library
 * @param bank Reference to dynamic bank
 * @return 0 on success, <0 when any error has occurred
 */
extern ADLMIDI_DECLSPEC int adl_getFirstBank(struct ADL_MIDIPlayer *device, ADL_Bank *bank);
/**
 * @brief Iterates to the next bank
 * @param device Instance of the library
 * @param bank Reference to dynamic bank
 * @return 0 on success, <0 when any error has occurred or end has been reached.
 */
extern ADLMIDI_DECLSPEC int adl_getNextBank(struct ADL_MIDIPlayer *device, ADL_Bank *bank);
/**
 * @brief Gets the nth intrument in the bank [0..127]
 * @param device Instance of the library
 * @param bank Reference to dynamic bank
 * @param index Index of the instrument
 * @param ins Instrument entry
 * @return 0 on success, <0 when any error has occurred
 */
extern ADLMIDI_DECLSPEC int adl_getInstrument(struct ADL_MIDIPlayer *device, const ADL_Bank *bank, unsigned index, ADL_Instrument *ins);
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
extern ADLMIDI_DECLSPEC int adl_setInstrument(struct ADL_MIDIPlayer *device, ADL_Bank *bank, unsigned index, const ADL_Instrument *ins);
/**
 * @brief Loads the melodic or percussive part of the nth embedded bank
 * @param device Instance of the library
 * @param bank Reference to dynamic bank
 * @param num Number of embedded bank to load into the current bank array
 * @return 0 on success, <0 when any error has occurred
 */
extern ADLMIDI_DECLSPEC int adl_loadEmbeddedBank(struct ADL_MIDIPlayer *device, ADL_Bank *bank, int num);



/**
 * @brief Sets number of 4-operator channels between all chips
 *
 * By default, it is automatically re-calculating every bank change.
 * If you want to specify custom number of four operator channels,
 * please call this function after bank change (adl_setBank() or adl_openBank()),
 * otherwise, value will be overwritten by auto-calculated.
 * If the count is specified as -1, an auto-calculated amount is used instead.
 *
 * @param device Instance of the library
 * @param ops4 Count of four-op channels to allocate between all emulating chips
 * @return 0 on success, <0 when any error has occurred
 */
extern ADLMIDI_DECLSPEC int adl_setNumFourOpsChn(struct ADL_MIDIPlayer *device, int ops4);

/**
 * @brief Get current total count of 4-operator channels between all chips
 * @param device Instance of the library
 * @return 0 on success, <-1 when any error has occurred, but, -1 - "auto"
 */
extern ADLMIDI_DECLSPEC int adl_getNumFourOpsChn(struct ADL_MIDIPlayer *device);

/**
 * @brief Get obtained total count of 4-operator channels between all chips
 * @param device Instance of the library
 * @return 0 on success, <0 when any error has occurred
 */
extern ADLMIDI_DECLSPEC int adl_getNumFourOpsChnObtained(struct ADL_MIDIPlayer *device);

/**
 * @brief Override Enable(1) or Disable(0) AdLib percussion mode. -1 - use bank default AdLib percussion mode
 *
 * [DEPRECATED] This function is no more useful and now it makes nothing. Kept for ABI compatibility. Rhythm mode can be set by the bank data only.
 *
 * This function forces rhythm-mode on any bank. The result will work glitchy.
 *
 * @param device Instance of the library
 * @param percmod 0 - disabled, 1 - enabled
 */
ADLMIDI_DEPRECATED("This function is no more useful and now it makes nothing. Kept for ABI compatibility. Rhythm mode can be set by the bank data only.")
extern ADLMIDI_DECLSPEC void adl_setPercMode(struct ADL_MIDIPlayer *device, int percmod);

/**
 * @brief Override Enable(1) or Disable(0) deep vibrato state. -1 - use bank default vibrato state
 * @param device Instance of the library
 * @param hvibro 0 - disabled, 1 - enabled
 */
extern ADLMIDI_DECLSPEC void adl_setHVibrato(struct ADL_MIDIPlayer *device, int hvibro);

/**
 * @brief Get the deep vibrato state.
 * @param device Instance of the library
 * @return deep vibrato state on success, <0 when any error has occurred
 */
extern ADLMIDI_DECLSPEC int adl_getHVibrato(struct ADL_MIDIPlayer *device);

/**
 * @brief Override Enable(1) or Disable(0) deep tremolo state. -1 - use bank default tremolo state
 * @param device Instance of the library
 * @param htremo 0 - disabled, 1 - enabled
 */
extern ADLMIDI_DECLSPEC void adl_setHTremolo(struct ADL_MIDIPlayer *device, int htremo);

/**
 * @brief Get the deep tremolo state.
 * @param device Instance of the library
 * @return deep tremolo state on success, <0 when any error has occurred
 */
extern ADLMIDI_DECLSPEC int adl_getHTremolo(struct ADL_MIDIPlayer *device);

/**
 * @brief Override Enable(1) or Disable(0) scaling of modulators by volumes. -1 - use bank default scaling of modulator volumes
 * @param device Instance of the library
 * @param smod 0 - disabled, 1 - enabled
 *
 * When this feature is enabled, all the modulators will scale together with the volume
 * that will cuase an effect of sound smoothing. However, it's better to use the CC74
 * to explicitly control the volume of modulators when it's needed, otherwise using
 * of this feature causes a lot of problems. It's possible that this feature will be
 * removed eventually and this function will become useless.
 *
 * IMPORTANT: If you develop music player or plugin, or integrate this to somewhere,
 * suggested to don't add binding to this feature and keep it always disabled.
 *
 * Details: https://github.com/Wohlstand/libOPNMIDI/issues/125
 */
extern ADLMIDI_DECLSPEC void adl_setScaleModulators(struct ADL_MIDIPlayer *device, int smod);

/**
 * @brief Enable(1) or Disable(0) full-range brightness (MIDI CC74 used in XG music to filter result sounding) scaling
 *
 * By default, brightness affects sound between 0 and 64.
 * When this option is enabled, the brightness will use full range from 0 up to 127.
 *
 * @param device Instance of the library
 * @param fr_brightness 0 - disabled, 1 - enabled
 */
extern ADLMIDI_DECLSPEC void adl_setFullRangeBrightness(struct ADL_MIDIPlayer *device, int fr_brightness);

/**
 * @brief Enable(1) or Disable(0) the automatical arpeggio system
  *
 * @param device Instance of the library
 * @param aaEn 0 - disabled, 1 - enabled
 */
extern ADLMIDI_DECLSPEC void adl_setAutoArpeggio(struct ADL_MIDIPlayer *device, int aaEn);

/**
 * @brief Get the state of the automatical arpeggio system enable state
 * @param device Instalce of the library
 * @return 0 - disabled, 1 - enabled
 */
extern ADLMIDI_DECLSPEC int adl_getAutoArpeggio(struct ADL_MIDIPlayer *device);

/**
 * @brief Enable or disable built-in loop (built-in loop supports 'loopStart' and 'loopEnd' tags to loop specific part)
 * @param device Instance of the library
 * @param loopEn 0 - disabled, 1 - enabled
 */
extern ADLMIDI_DECLSPEC void adl_setLoopEnabled(struct ADL_MIDIPlayer *device, int loopEn);

/**
 * @brief Set how many times loop will be played
 *
 * Note: The song will be played once if loop has been disabled with no matter which value of loop count was set
 *
 * @param device Instance of the library
 * @param loopCount Number of loops or -1 to loop infinitely
 */
extern ADLMIDI_DECLSPEC void adl_setLoopCount(struct ADL_MIDIPlayer *device, int loopCount);

/**
 * @brief Make song immediately stop on reaching a loop end point
 * @param device Instance of the library
 * @param loopHooksOnly 0 - disabled, 1 - enabled
 */
extern ADLMIDI_DECLSPEC void adl_setLoopHooksOnly(struct ADL_MIDIPlayer *device, int loopHooksOnly);

/**
 * @brief Enable or disable soft panning with chip emulators
 * @param device Instance of the library
 * @param softPanEn 0 - disabled, 1 - enabled
 */
extern ADLMIDI_DECLSPEC void adl_setSoftPanEnabled(struct ADL_MIDIPlayer *device, int softPanEn);

/**
 * @brief [DEPRECATED] Enable or disable Logarithmic volume changer
 *
 * This function is deprecated. Suggested replacement: `adl_setVolumeRangeModel` with `ADLMIDI_VolumeModel_NativeOPL3` volume model value;
 */
ADLMIDI_DEPRECATED("Use `adl_setVolumeRangeModel(device, ADLMIDI_VolumeModel_NativeOPL3)` instead")
extern ADLMIDI_DECLSPEC void adl_setLogarithmicVolumes(struct ADL_MIDIPlayer *device, int logvol);

/**
 * @brief Set different volume range model
 * @param device Instance of the library
 * @param volumeModel Volume model type (#ADLMIDI_VolumeModels)
 */
extern ADLMIDI_DECLSPEC void adl_setVolumeRangeModel(struct ADL_MIDIPlayer *device, int volumeModel);

/**
 * @brief Get the volume range model
 * @param device Instance of the library
 * @return volume model on success, <0 when any error has occurred
 */
extern ADLMIDI_DECLSPEC int adl_getVolumeRangeModel(struct ADL_MIDIPlayer *device);

/**
 * @brief Set the channel allocation mode
 * @param device Instance of the library
 * @param chanalloc Channel allocation mode (#ADLMIDI_ChannelAlloc)
 */
extern ADLMIDI_DECLSPEC void adl_setChannelAllocMode(struct ADL_MIDIPlayer *device, int chanalloc);

/**
 * @brief Get the current channel allocation mode
 * @param device Instance of the library
 * @return Channel allocation mode (#ADLMIDI_ChannelAlloc)
 */
extern ADLMIDI_DECLSPEC int adl_getChannelAllocMode(struct ADL_MIDIPlayer *device);

/**
 * @brief Load WOPL bank file from File System
 *
 * Is recommended to call adl_reset() to apply changes to already-loaded file player or real-time.
 *
 * @param device Instance of the library
 * @param filePath Absolute or relative path to the WOPL bank file. UTF8 encoding is required, even on Windows.
 * @return 0 on success, <0 when any error has occurred
 */
extern ADLMIDI_DECLSPEC int adl_openBankFile(struct ADL_MIDIPlayer *device, const char *filePath);

/**
 * @brief Load WOPL bank file from memory data
 *
 * Is recommended to call adl_reset() to apply changes to already-loaded file player or real-time.
 *
 * @param device Instance of the library
 * @param mem Pointer to memory block where is raw data of WOPL bank file is stored
 * @param size Size of given memory block
 * @return 0 on success, <0 when any error has occurred
 */
extern ADLMIDI_DECLSPEC int adl_openBankData(struct ADL_MIDIPlayer *device, const void *mem, unsigned long size);


/**
 * @brief [DEPRECATED] Dummy function
 *
 * This function is deprecated. Suggested replacement: `adl_chipEmulatorName`
 *
 * @return A string that contains a notice to use `adl_chipEmulatorName` instead of this function.
 */
ADLMIDI_DEPRECATED("Use `adl_chipEmulatorName(device)` instead")
extern ADLMIDI_DECLSPEC const char *adl_emulatorName();

/**
 * @brief Returns chip emulator name string
 * @param device Instance of the library
 * @return Understandable name of current OPL3 emulator
 */
extern ADLMIDI_DECLSPEC const char *adl_chipEmulatorName(struct ADL_MIDIPlayer *device);

/**
 * @brief List of available OPL3 emulators
 */
enum ADL_Emulator
{
    /*! Nuked OPL3 v. 1.8 */
    ADLMIDI_EMU_NUKED = 0,
    /*! Nuked OPL3 v. 1.7.4 */
    ADLMIDI_EMU_NUKED_174,
    /*! DosBox */
    ADLMIDI_EMU_DOSBOX,
    /*! Opal */
    ADLMIDI_EMU_OPAL,
    /*! Java */
    ADLMIDI_EMU_JAVA,
    /*! ESFMu */
    ADLMIDI_EMU_ESFMu,
    /*! MAME OPL2 */
    ADLMIDI_EMU_MAME_OPL2,
    /*! YMFM OPL2 */
    ADLMIDI_EMU_YMFM_OPL2,
    /*! YMFM OPL3 */
    ADLMIDI_EMU_YMFM_OPL3,
    /*! Nuked OPL2 LLE*/
    ADLMIDI_EMU_NUKED_OPL2_LLE,
    /*! Nuked OPL3 LLE*/
    ADLMIDI_EMU_NUKED_OPL3_LLE,
    /*! Count instrument on the level */
    ADLMIDI_EMU_end
};

/**
 * @brief Switch the emulation core
 * @param device Instance of the library
 * @param emulator Type of emulator (#ADL_Emulator)
 * @return 0 on success, <0 when any error has occurred
 */
extern ADLMIDI_DECLSPEC int adl_switchEmulator(struct ADL_MIDIPlayer *device, int emulator);

/**
 * @brief Library version context
 */
typedef struct {
    ADL_UInt16 major;
    ADL_UInt16 minor;
    ADL_UInt16 patch;
} ADL_Version;

/**
 * @brief Run emulator with PCM rate to reduce CPU usage on slow devices.
 *
 * May decrease sounding accuracy on some chip emulators.
 *
 * @param device Instance of the library
 * @param enabled 0 - disabled, 1 - enabled
 * @return 0 on success, <0 when any error has occurred
 */
extern ADLMIDI_DECLSPEC int adl_setRunAtPcmRate(struct ADL_MIDIPlayer *device, int enabled);

/**
 * @brief The list of serial port protocols
 */
enum ADL_SerialProtocol
{
    ADLMIDI_SerialProtocol_Unknown = 0,
    ADLMIDI_SerialProtocol_ArduinoOPL2,
    ADLMIDI_SerialProtocol_NukeYktOPL3,
    ADLMIDI_SerialProtocol_RetroWaveOPL3,
    ADLMIDI_SerialProtocol_END
};

/**
 * @brief Switch the synthesizer into hardware mode using Serial port
 * @param name The name of the serial port device (it may look different on various platforms. On UNIX-like systems don't type the /dev/ prefix: only name).
 * @param baud The baud speed of the serial port
 * @param protocol The binary protocol used to communicate the device (#ADL_SerialProtocol)
 * @return 0 on success, <0 when any error has occurred
 */
extern ADLMIDI_DECLSPEC int adl_switchSerialHW(struct ADL_MIDIPlayer *device,
                                               const char *name,
                                               unsigned baud,
                                               unsigned protocol);


/**
 * \brief The list of possible chip types for DOS hardware interface
 */
enum ADL_DOS_ChipType
{
    ADLMIDI_DOS_ChipAuto = 0,
    ADLMIDI_DOS_ChipOPL2,
    ADLMIDI_DOS_ChipOPL3
};

/**
 * @brief Specify the chip type and the base address before initialization.
 * This function has no effect if library didn't built for DOS with hardware OPL mode
 * Important: Run this BEFORE creating the instance of the library.
 * @param chipType
 * @param baseAddress
 * @return 0 on success, <0 when any error has occurred
 */
extern ADLMIDI_DECLSPEC int adl_switchDOSHW(int chipType, ADL_UInt16 baseAddress);

/**
 * @brief Set 4-bit device identifier. Used by the SysEx processor.
 * @param device Instance of the library
 * @param id 4-bit device identifier
 * @return 0 on success, <0 when any error has occurred
 */
extern ADLMIDI_DECLSPEC int adl_setDeviceIdentifier(struct ADL_MIDIPlayer *device, unsigned id);

/**
 * @section Information
 */

/**
 * @brief Returns string which contains a version number
 * @return String which contains a version of the library
 */
extern ADLMIDI_DECLSPEC const char *adl_linkedLibraryVersion();

/**
 * @brief Returns structure which contains a version number of library
 * @return Library version context structure which contains version number of the library
 */
extern ADLMIDI_DECLSPEC const ADL_Version *adl_linkedVersion();


/* ======== Error Info ======== */

/**
 * @brief Returns string which contains last error message of initialization
 *
 * Don't use this function to get info on any function except of `adl_init`!
 * Use `adl_errorInfo()` to get error information while workflow
 *
 * @return String with error message related to library initialization
 */
extern ADLMIDI_DECLSPEC const char *adl_errorString();

/**
 * @brief Returns string which contains last error message on specific device
 * @param device Instance of the library
 * @return String with error message related to last function call returned non-zero value.
 */
extern ADLMIDI_DECLSPEC const char *adl_errorInfo(struct ADL_MIDIPlayer *device);



/* ======== Initialization ======== */

/**
 * @brief Initialize ADLMIDI Player device
 *
 * Tip 1: You can initialize multiple instances and run them in parallel
 * Tip 2: Library is NOT thread-safe, therefore don't use same instance in different threads or use mutexes
 * Tip 3: Changing of sample rate on the fly is not supported. Re-create the instance again.
 * Top 4: To generate output in OPL chip native sample rate, please initialize it with sample rate value as `ADL_CHIP_SAMPLE_RATE`
 *
 * @param sample_rate Output sample rate
 * @return Instance of the library. If NULL was returned, check the `adl_errorString` message for more info.
 */
extern ADLMIDI_DECLSPEC struct ADL_MIDIPlayer *adl_init(long sample_rate);

/**
 * @brief Close and delete ADLMIDI device
 * @param device Instance of the library
 */
extern ADLMIDI_DECLSPEC void adl_close(struct ADL_MIDIPlayer *device);



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
extern ADLMIDI_DECLSPEC int adl_openFile(struct ADL_MIDIPlayer *device, const char *filePath);

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
extern ADLMIDI_DECLSPEC int adl_openData(struct ADL_MIDIPlayer *device, const void *mem, unsigned long size);

/**
 * @brief Switch another song if multi-song file is playing (for example, XMI)
 *
 * Note: to set the initial song to load, you should call this function
 * BBEFORE calling `adl_openFile` or `adl_openData`.  When loaded file has more than
 * one built-in songs (Usually XMIformat), it will be started from the selected number.
 * You may call this function to switch another song.
 *
 * @param device Instance of the library
 * @param songNumber Identifier of the track to load (or -1 to mix all tracks as one song)
 * @return
 */
extern ADLMIDI_DECLSPEC void adl_selectSongNum(struct ADL_MIDIPlayer *device, int songNumber);

/**
 * @brief Retrive the number of songs in a currently opened file
 * @param device Instance of the library
 * @return Number of songs in the file. If 1 or less, means, the file has only one song inside.
 */
extern ADLMIDI_DECLSPEC int adl_getSongsCount(struct ADL_MIDIPlayer *device);

/**
 * @brief Resets MIDI player (per-channel setup) into initial state
 * @param device Instance of the library
 */
extern ADLMIDI_DECLSPEC void adl_reset(struct ADL_MIDIPlayer *device);

/**
 * @brief Get total time length of current song
 *
 * Available when library is built with built-in MIDI Sequencer support.
 *
 * @param device Instance of the library
 * @return Total song length in seconds
 */
extern ADLMIDI_DECLSPEC double adl_totalTimeLength(struct ADL_MIDIPlayer *device);

/**
 * @brief Get loop start time if presented.
 *
 * Available when library is built with built-in MIDI Sequencer support.
 *
 * @param device Instance of the library
 * @return Time position in seconds of loop start point, or -1 when file has no loop points
 */
extern ADLMIDI_DECLSPEC double adl_loopStartTime(struct ADL_MIDIPlayer *device);

/**
 * @brief Get loop endtime if presented.
 *
 * Available when library is built with built-in MIDI Sequencer support.
 *
 * @param device Instance of the library
 * @return Time position in seconds of loop end point, or -1 when file has no loop points
 */
extern ADLMIDI_DECLSPEC double adl_loopEndTime(struct ADL_MIDIPlayer *device);

/**
 * @brief Get current time position in seconds
 *
 * Available when library is built with built-in MIDI Sequencer support.
 *
 * @param device Instance of the library
 * @return Current time position in seconds
 */
extern ADLMIDI_DECLSPEC double adl_positionTell(struct ADL_MIDIPlayer *device);

/**
 * @brief Jump to absolute time position in seconds
 *
 * Available when library is built with built-in MIDI Sequencer support.
 *
 * @param device Instance of the library
 * @param seconds Destination time position in seconds to seek
 */
extern ADLMIDI_DECLSPEC void adl_positionSeek(struct ADL_MIDIPlayer *device, double seconds);

/**
 * @brief Reset MIDI track position to begin
 *
 * Available when library is built with built-in MIDI Sequencer support.
 *
 * @param device Instance of the library
 */
extern ADLMIDI_DECLSPEC void adl_positionRewind(struct ADL_MIDIPlayer *device);

/**
 * @brief Set tempo multiplier
 *
 * Available when library is built with built-in MIDI Sequencer support.
 *
 * @param device Instance of the library
 * @param tempo Tempo multiplier value: 1.0 - original tempo, >1 - play faster, <1 - play slower
 */
extern ADLMIDI_DECLSPEC void adl_setTempo(struct ADL_MIDIPlayer *device, double tempo);

/**
 * @brief Returns 1 if music position has reached end
 * @param device Instance of the library
 * @return 1 when end of sing has been reached, otherwise, 0 will be returned. <0 is returned on any error
 */
extern ADLMIDI_DECLSPEC int adl_atEnd(struct ADL_MIDIPlayer *device);

/**
 * @brief Returns the number of tracks of the current sequence
 * @param device Instance of the library
 * @return Count of tracks in the current sequence
 */
extern ADLMIDI_DECLSPEC size_t adl_trackCount(struct ADL_MIDIPlayer *device);

/**
 * @brief Track options
 */
enum ADLMIDI_TrackOptions
{
    /*! Enabled track */
    ADLMIDI_TrackOption_On   = 1,
    /*! Disabled track */
    ADLMIDI_TrackOption_Off  = 2,
    /*! Solo track */
    ADLMIDI_TrackOption_Solo = 3
};

/**
 * @brief Sets options on a track of the current sequence
 * @param device Instance of the library
 * @param trackNumber Identifier of the designated track.
 * @return 0 on success, <0 when any error has occurred
 */
extern ADLMIDI_DECLSPEC int adl_setTrackOptions(struct ADL_MIDIPlayer *device, size_t trackNumber, unsigned trackOptions);

/**
 * @brief Sets the channel of the current sequence enable state
 * @param device Instance of the library
 * @param channelNumber Number of the channel (from 0 to 15)
 * @param enabled 1 to enable and 0 to disable
 * @return 0 on success, <0 when any error has occurred
 */
extern ADLMIDI_DECLSPEC int adl_setChannelEnabled(struct ADL_MIDIPlayer *device, size_t channelNumber, int enabled);

/**
 * @brief Handler of callback trigger events
 * @param userData Pointer to user data (usually, context of something)
 * @param trigger Value of the event which triggered this callback.
 * @param track Identifier of the track which triggered this callback.
 */
typedef void (*ADL_TriggerHandler)(void *userData, unsigned trigger, size_t track);

/**
 * @brief Defines a handler for callback trigger events
 * @param device Instance of the library
 * @param handler Handler to invoke from the sequencer when triggered, or NULL.
 * @param userData Instance of the library
 * @return 0 on success, <0 when any error has occurred
 */
extern ADLMIDI_DECLSPEC int adl_setTriggerHandler(struct ADL_MIDIPlayer *device, ADL_TriggerHandler handler, void *userData);




/* ======== Meta-Tags ======== */

/**
 * @brief Returns string which contains a music title
 * @param device Instance of the library
 * @return A string that contains music title
 */
extern ADLMIDI_DECLSPEC const char *adl_metaMusicTitle(struct ADL_MIDIPlayer *device);

/**
 * @brief Returns string which contains a copyright string*
 * @param device Instance of the library
 * @return A string that contains copyright notice, otherwise NULL
 */
extern ADLMIDI_DECLSPEC const char *adl_metaMusicCopyright(struct ADL_MIDIPlayer *device);

/**
 * @brief Returns count of available track titles
 *
 * NOTE: There are CAN'T be associated with channel in any of event or note hooks
 *
 * @param device Instance of the library
 * @return Count of available MIDI tracks, otherwise NULL
 */
extern ADLMIDI_DECLSPEC size_t adl_metaTrackTitleCount(struct ADL_MIDIPlayer *device);

/**
 * @brief Get track title by index
 * @param device Instance of the library
 * @param index Index of the track to retreive the title
 * @return A string that contains track title, otherwise NULL.
 */
extern ADLMIDI_DECLSPEC const char *adl_metaTrackTitle(struct ADL_MIDIPlayer *device, size_t index);

/**
 * @brief MIDI Marker structure
 */
struct Adl_MarkerEntry
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
extern ADLMIDI_DECLSPEC size_t adl_metaMarkerCount(struct ADL_MIDIPlayer *device);

/**
 * @brief Returns the marker entry
 * @param device Instance of the library
 * @param index Index of the marker to retreive it.
 * @return MIDI Marker description structure.
 */
extern ADLMIDI_DECLSPEC struct Adl_MarkerEntry adl_metaMarker(struct ADL_MIDIPlayer *device, size_t index);




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
extern ADLMIDI_DECLSPEC int  adl_play(struct ADL_MIDIPlayer *device, int sampleCount, short *out);

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
extern ADLMIDI_DECLSPEC int  adl_playFormat(struct ADL_MIDIPlayer *device, int sampleCount, ADL_UInt8 *left, ADL_UInt8 *right, const struct ADLMIDI_AudioFormat *format);

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
extern ADLMIDI_DECLSPEC int  adl_generate(struct ADL_MIDIPlayer *device, int sampleCount, short *out);

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
extern ADLMIDI_DECLSPEC int  adl_generateFormat(struct ADL_MIDIPlayer *device, int sampleCount, ADL_UInt8 *left, ADL_UInt8 *right, const struct ADLMIDI_AudioFormat *format);

/**
 * @brief Periodic tick handler.
 *
 * Notice: The function is provided to use it with Hardware OPL3 mode or for the purpose to iterate
 * MIDI playback without of sound generation.
 *
 * DON'T USE IT TOGETHER WITH adl_play() and adl_playFormat() calls
 * as there are all using this function internally!!!
 *
 * @param device Instance of the library
 * @param seconds Previous delay. On a first moment, pass the `0.0`
 * @param granulality Minimal size of one MIDI tick in seconds.
 * @return desired number of seconds until next call. Pass this value into `seconds` field in next time
 */
extern ADLMIDI_DECLSPEC double adl_tickEvents(struct ADL_MIDIPlayer *device, double seconds, double granulality);

/**
 * @brief Periodic tick handler without iterators.
 *
 * Unlike adl_tickEvents(), it doesn't handles iterators, you need to perform
 * them naually via adl_tickIterators().
 *
 * Notice: The function is provided to use it with Hardware OPL3 mode or for the purpose to iterate
 * MIDI playback without of sound generation.
 *
 * DON'T USE IT TOGETHER WITH adl_play() and adl_playFormat() calls
 * as there are all using this function internally!!!
 *
 * @param device Instance of the library
 * @param seconds Previous delay. On a first moment, pass the `0.0`
 * @param granulality Minimal size of one MIDI tick in seconds.
 * @return desired number of seconds until next call. Pass this value into `seconds` field in next time
 */
extern ADLMIDI_DECLSPEC double adl_tickEventsOnly(struct ADL_MIDIPlayer *device, double seconds, double granulality);


/**
 * @brief Periodic tick hander for the real-time hardware output
 *
 * This function runs a single step of vibrato, auto-arpeggio, and the portamento of @seconds duration.
 *
 * When running the libADLMIDI as a real-time driver for the ral hardware, call
 * this function from the timer and specify the @seconds value with a delay of the single cycle.
 *
 * @param device Instance of the library
 * @param seconds Previous delay. On a first moment, pass the `0.0`
 */
extern ADLMIDI_DECLSPEC void adl_tickIterators(struct ADL_MIDIPlayer *device, double seconds);




/* ======== Real-Time MIDI ======== */

/**
 * @brief Force Off all notes on all channels
 * @param device Instance of the library
 */
extern ADLMIDI_DECLSPEC void adl_panic(struct ADL_MIDIPlayer *device);

/**
 * @brief Reset states of all controllers on all MIDI channels
 * @param device Instance of the library
 */
extern ADLMIDI_DECLSPEC void adl_rt_resetState(struct ADL_MIDIPlayer *device);

/**
 * @brief Turn specific MIDI note ON
 * @param device Instance of the library
 * @param channel Target MIDI channel [Between 0 and 16]
 * @param note Note number to on [Between 0 and 127]
 * @param velocity Velocity level [Between 0 and 127]
 * @return 1 when note was successfully started, 0 when note was rejected by any reason.
 */
extern ADLMIDI_DECLSPEC int adl_rt_noteOn(struct ADL_MIDIPlayer *device, ADL_UInt8 channel, ADL_UInt8 note, ADL_UInt8 velocity);

/**
 * @brief Turn specific MIDI note OFF
 * @param device Instance of the library
 * @param channel Target MIDI channel [Between 0 and 16]
 * @param note Note number to off [Between 0 and 127]
 */
extern ADLMIDI_DECLSPEC void adl_rt_noteOff(struct ADL_MIDIPlayer *device, ADL_UInt8 channel, ADL_UInt8 note);

/**
 * @brief Set note after-touch
 * @param device Instance of the library
 * @param channel Target MIDI channel [Between 0 and 16]
 * @param note Note number to affect by aftertouch event [Between 0 and 127]
 * @param atVal After-Touch value [Between 0 and 127]
 */
extern ADLMIDI_DECLSPEC void adl_rt_noteAfterTouch(struct ADL_MIDIPlayer *device, ADL_UInt8 channel, ADL_UInt8 note, ADL_UInt8 atVal);

/**
 * @brief Set channel after-touch
 * @param device Instance of the library
 * @param channel Target MIDI channel [Between 0 and 16]
 * @param atVal After-Touch level [Between 0 and 127]
 */
extern ADLMIDI_DECLSPEC void adl_rt_channelAfterTouch(struct ADL_MIDIPlayer *device, ADL_UInt8 channel, ADL_UInt8 atVal);

/**
 * @brief Apply controller change
 * @param device Instance of the library
 * @param channel Target MIDI channel [Between 0 and 16]
 * @param type Type of the controller [Between 0 and 255]
 * @param value Value of the controller event [Between 0 and 127]
 */
extern ADLMIDI_DECLSPEC void adl_rt_controllerChange(struct ADL_MIDIPlayer *device, ADL_UInt8 channel, ADL_UInt8 type, ADL_UInt8 value);

/**
 * @brief Apply patch change
 * @param device Instance of the library
 * @param channel Target MIDI channel [Between 0 and 16]
 * @param patch Patch number [Between 0 and 127]
 */
extern ADLMIDI_DECLSPEC void adl_rt_patchChange(struct ADL_MIDIPlayer *device, ADL_UInt8 channel, ADL_UInt8 patch);

/**
 * @brief Apply pitch bend change
 * @param device Instance of the library
 * @param channel Target MIDI channel [Between 0 and 16]
 * @param pitch 24-bit pitch bend value
 */
extern ADLMIDI_DECLSPEC void adl_rt_pitchBend(struct ADL_MIDIPlayer *device, ADL_UInt8 channel, ADL_UInt16 pitch);

/**
 * @brief Apply pitch bend change
 * @param device Instance of the library
 * @param channel Target MIDI channel [Between 0 and 16]
 * @param msb MSB part of 24-bit pitch bend value
 * @param lsb LSB part of 24-bit pitch bend value
 */
extern ADLMIDI_DECLSPEC void adl_rt_pitchBendML(struct ADL_MIDIPlayer *device, ADL_UInt8 channel, ADL_UInt8 msb, ADL_UInt8 lsb);

/**
 * @brief Change LSB of the bank number (Alias to CC-32 event)
 * @param device Instance of the library
 * @param channel Target MIDI channel [Between 0 and 16]
 * @param lsb LSB value of the MIDI bank number
 */
extern ADLMIDI_DECLSPEC void adl_rt_bankChangeLSB(struct ADL_MIDIPlayer *device, ADL_UInt8 channel, ADL_UInt8 lsb);

/**
 * @brief Change MSB of the bank (Alias to CC-0 event)
 * @param device Instance of the library
 * @param channel Target MIDI channel [Between 0 and 16]
 * @param msb MSB value of the MIDI bank number
 */
extern ADLMIDI_DECLSPEC void adl_rt_bankChangeMSB(struct ADL_MIDIPlayer *device, ADL_UInt8 channel, ADL_UInt8 msb);

/**
 * @brief Change bank by absolute signed value
 * @param device Instance of the library
 * @param channel Target MIDI channel [Between 0 and 16]
 * @param bank Bank number as concoctated signed 16-bit value of MSB and LSB parts.
 */

extern ADLMIDI_DECLSPEC void adl_rt_bankChange(struct ADL_MIDIPlayer *device, ADL_UInt8 channel, ADL_SInt16 bank);

/**
 * @brief Perform a system exclusive message
 * @param device Instance of the library
 * @param msg Raw SysEx message buffer (must begin with 0xF0 and end with 0xF7)
 * @param size Size of given SysEx message buffer
 * @return 1 when SysEx message was successfully processed, 0 when SysEx message was rejected by any reason
 */
extern ADLMIDI_DECLSPEC int adl_rt_systemExclusive(struct ADL_MIDIPlayer *device, const ADL_UInt8 *msg, size_t size);




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
typedef void (*ADL_RawEventHook)(void *userdata, ADL_UInt8 type, ADL_UInt8 subtype, ADL_UInt8 channel, const ADL_UInt8 *data, size_t len);

/**
 * @brief Note on/off callback
 * @param userdata Pointer to user data (usually, context of someting)
 * @param adlchn Chip channel where note was played
 * @param note Note number [between 0 and 127]
 * @param pressure Velocity level, or -1 when it's note off event
 * @param bend Pitch bend offset value
 */
typedef void (*ADL_NoteHook)(void *userdata, int adlchn, int note, int ins, int pressure, double bend);

/**
 * @brief Debug messages callback
 * @param userdata Pointer to user data (usually, context of someting)
 * @param fmt Format strign output (in context of `printf()` standard function)
 */
typedef void (*ADL_DebugMessageHook)(void *userdata, const char *fmt, ...);

/**
 * @brief Loop start/end point reach hook
 * @param userdata Pointer to user data (usually, context of someting)
 */
typedef void (*ADL_LoopPointHook)(void *userdata);

/**
 * @brief Set raw MIDI event hook
 *
 * CAUTION: Don't call any libADLMIDI API functions from off this hook directly!
 * Suggestion: Use boolean variables to mark the fact this hook got been called, and then,
 * apply your action outside of this hook, for example, in the next after audio output call.
 *
 * @param device Instance of the library
 * @param rawEventHook Pointer to the callback function which will be called on every MIDI event
 * @param userData Pointer to user data which will be passed through the callback.
 */
extern ADLMIDI_DECLSPEC void adl_setRawEventHook(struct ADL_MIDIPlayer *device, ADL_RawEventHook rawEventHook, void *userData);

/**
 * @brief Set note hook
 *
 * CAUTION: Don't call any libADLMIDI API functions from off this hook directly!
 * Suggestion: Use boolean variables to mark the fact this hook got been called, and then,
 * apply your action outside of this hook, for example, in the next after audio output call.
 *
 * @param device Instance of the library
 * @param noteHook Pointer to the callback function which will be called on every noteOn MIDI event
 * @param userData Pointer to user data which will be passed through the callback.
 */
extern ADLMIDI_DECLSPEC void adl_setNoteHook(struct ADL_MIDIPlayer *device, ADL_NoteHook noteHook, void *userData);

/**
 * @brief Set debug message hook
 *
 * CAUTION: Don't call any libADLMIDI API functions from off this hook directly!
 * Suggestion: Use boolean variables to mark the fact this hook got been called, and then,
 * apply your action outside of this hook, for example, in the next after audio output call.
 *
 * @param device Instance of the library
 * @param debugMessageHook Pointer to the callback function which will be called on every debug message
 * @param userData Pointer to user data which will be passed through the callback.
 */
extern ADLMIDI_DECLSPEC void adl_setDebugMessageHook(struct ADL_MIDIPlayer *device, ADL_DebugMessageHook debugMessageHook, void *userData);

/**
 * @brief Set the look start point hook
 *
 * CAUTION: Don't call any libADLMIDI API functions from off this hook directly!
 * Suggestion: Use boolean variables to mark the fact this hook got been called, and then,
 * apply your action outside of this hook, for example, in the next after audio output call.
 *
 * @param device Instance of the library
 * @param loopStartHook Pointer to the callback function which will be called on every loop start point passing
 * @param userData Pointer to user data which will be passed through the callback.
 */
extern ADLMIDI_DECLSPEC void adl_setLoopStartHook(struct ADL_MIDIPlayer *device, ADL_LoopPointHook loopStartHook, void *userData);

/**
 * @brief Set the look start point hook
 *
 * CAUTION: Don't call any libADLMIDI API functions from off this hook directly!
 * Suggestion: Use boolean variables to mark the fact this hook got been called, and then,
 * apply your action outside of this hook, for example, in the next after audio output call.
 *
 * If you want to switch the song after calling this hook, suggested to call the function
 * adl_setLoopHooksOnly(device, 1) to immediately stop the song on reaching the loop point
 *
 * @param device Instance of the library
 * @param loopStartHook Pointer to the callback function which will be called on every loop start point passing
 * @param userData Pointer to user data which will be passed through the callback.
 */
extern ADLMIDI_DECLSPEC void adl_setLoopEndHook(struct ADL_MIDIPlayer *device, ADL_LoopPointHook loopEndHook, void *userData);

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
 *  `+` - channel is used by two-operator voice
 *  `#` - channel is used by four-operator voice
 *  `@` - channel is used to play automatic arpeggio on chip channels overflow
 *  `r` - rhythm-mode channel note
 * ```
 *
 * The `attr` field receives the MIDI channel from which the chip channel is used.
 * To get the valid MIDI channel you will need to apply the & 0x0F mask to every value.
 */
extern ADLMIDI_DECLSPEC int adl_describeChannels(struct ADL_MIDIPlayer *device, char *text, char *attr, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* ADLMIDI_H */
