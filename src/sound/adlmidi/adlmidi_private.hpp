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

#ifndef ADLMIDI_PRIVATE_HPP
#define ADLMIDI_PRIVATE_HPP

// Setup compiler defines useful for exporting required public API symbols in gme.cpp
#ifndef ADLMIDI_EXPORT
#   if defined (_WIN32) && defined(ADLMIDI_BUILD_DLL)
#       define ADLMIDI_EXPORT __declspec(dllexport)
#   elif defined (LIBADLMIDI_VISIBILITY) && defined (__GNUC__)
#       define ADLMIDI_EXPORT __attribute__((visibility ("default")))
#   else
#       define ADLMIDI_EXPORT
#   endif
#endif


#ifdef _WIN32
#define NOMINMAX 1
#endif

#if defined(_WIN32) && !defined(__WATCOMC__)
#   undef NO_OLDNAMES
#       include <stdint.h>
#   ifdef _MSC_VER
#       ifdef _WIN64
typedef __int64 ssize_t;
#       else
typedef __int32 ssize_t;
#       endif
#       define NOMINMAX 1 //Don't override std::min and std::max
#   else
#       ifdef _WIN64
typedef int64_t ssize_t;
#       else
typedef int32_t ssize_t;
#       endif
#   endif
#   include <windows.h>
#endif

#if defined(__DJGPP__) || (defined(__WATCOMC__) && (defined(__DOS__) || defined(__DOS4G__) || defined(__DOS4GNZ__)))
#define ADLMIDI_HW_OPL
#include <conio.h>
#ifdef __DJGPP__
#include <pc.h>
#include <dpmi.h>
#include <go32.h>
#include <sys/farptr.h>
#include <dos.h>
#endif

#endif

#include <vector>
#include <list>
#include <string>
//#ifdef __WATCOMC__
//#include <myset.h> //TODO: Implemnet a workaround for OpenWatcom to fix a crash while using those containers
//#include <mymap.h>
//#else
#include <map>
#include <set>
#include <new> // nothrow
//#endif
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cassert>
#include <vector> // vector
#include <deque>  // deque
#include <cmath>  // exp, log, ceil
#if defined(__WATCOMC__)
#include <math.h> // round, sqrt
#endif
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits> // numeric_limit

#ifndef _WIN32
#include <errno.h>
#endif

#include <deque>
#include <algorithm>

/*
 * Workaround for some compilers are has no those macros in their headers!
 */
#ifndef INT8_MIN
#define INT8_MIN    (-0x7f - 1)
#endif
#ifndef INT16_MIN
#define INT16_MIN   (-0x7fff - 1)
#endif
#ifndef INT32_MIN
#define INT32_MIN   (-0x7fffffff - 1)
#endif
#ifndef INT8_MAX
#define INT8_MAX    0x7f
#endif
#ifndef INT16_MAX
#define INT16_MAX   0x7fff
#endif
#ifndef INT32_MAX
#define INT32_MAX   0x7fffffff
#endif

#include "file_reader.hpp"

#ifndef ADLMIDI_DISABLE_MIDI_SEQUENCER
// Rename class to avoid ABI collisions
#define BW_MidiSequencer AdlMidiSequencer
#include "midi_sequencer.hpp"
typedef BW_MidiSequencer MidiSequencer;
#endif//ADLMIDI_DISABLE_MIDI_SEQUENCER

#ifndef ADLMIDI_HW_OPL
#include "chips/opl_chip_base.h"
#endif

#include "adldata.hh"

#define ADLMIDI_BUILD
#include "adlmidi.h"    //Main API

#ifndef ADLMIDI_DISABLE_CPP_EXTRAS
#include "adlmidi.hpp"  //Extra C++ API
#endif

#include "adlmidi_ptr.hpp"
#include "adlmidi_bankmap.h"

#define ADL_UNUSED(x) (void)x

#define OPL_PANNING_LEFT    0x10
#define OPL_PANNING_RIGHT   0x20
#define OPL_PANNING_BOTH    0x30

#ifdef ADLMIDI_HW_OPL
#define ADL_MAX_CHIPS 1
#define ADL_MAX_CHIPS_STR "1" //Why not just "#MaxCards" ? Watcom fails to pass this with "syntax error" :-P
#else
#define ADL_MAX_CHIPS 100
#define ADL_MAX_CHIPS_STR "100"
#endif

extern std::string ADLMIDI_ErrorString;

/*
  Sample conversions to various formats
*/
template <class Real>
inline Real adl_cvtReal(int32_t x)
{
    return static_cast<Real>(x) * (static_cast<Real>(1) / static_cast<Real>(INT16_MAX));
}

inline int32_t adl_cvtS16(int32_t x)
{
    x = (x < INT16_MIN) ? (INT16_MIN) : x;
    x = (x > INT16_MAX) ? (INT16_MAX) : x;
    return x;
}

inline int32_t adl_cvtS8(int32_t x)
{
    return adl_cvtS16(x) / 256;
}
inline int32_t adl_cvtS24(int32_t x)
{
    return adl_cvtS16(x) * 256;
}
inline int32_t adl_cvtS32(int32_t x)
{
    return adl_cvtS16(x) * 65536;
}
inline int32_t adl_cvtU16(int32_t x)
{
    return adl_cvtS16(x) - INT16_MIN;
}
inline int32_t adl_cvtU8(int32_t x)
{
    return (adl_cvtS16(x) / 256) - INT8_MIN;
}
inline int32_t adl_cvtU24(int32_t x)
{
    enum { int24_min = -(1 << 23) };
    return adl_cvtS24(x) - int24_min;
}
inline int32_t adl_cvtU32(int32_t x)
{
    // unsigned operation because overflow on signed integers is undefined
    return (uint32_t)adl_cvtS32(x) - (uint32_t)INT32_MIN;
}

struct ADL_MIDIPlayer;
class  MIDIplay;
/**
 * @brief OPL3 Chip management class
 */
class OPL3
{
    friend class MIDIplay;
    friend class AdlInstrumentTester;
    friend int adlCalculateFourOpChannels(MIDIplay *play, bool silent);
public:
    enum
    {
        PercussionTag = 1 << 15,
        CustomBankTag = 0xFFFFFFFF
    };

    //! Total number of chip channels between all running emulators
    uint32_t m_numChannels;
    //! Just a padding. Reserved.
    char _padding[4];
#ifndef ADLMIDI_HW_OPL
    //! Running chip emulators
    std::vector<AdlMIDI_SPtr<OPLChipBase > > m_chips;
#endif

private:
    //! Cached patch data, needed by Touch()
    std::vector<adldata>    m_insCache;
    //! Value written to B0, cached, needed by NoteOff.
    /*! Contains Key on/off state, octave block and frequency number values
     */
    std::vector<uint32_t>   m_keyBlockFNumCache;
    //! Cached BD registry value (flags register: DeepTremolo, DeepVibrato, and RhythmMode)
    std::vector<uint32_t>   m_regBD;

public:
    /**
     * @brief MIDI bank entry
     */
    struct Bank
    {
        //! MIDI Bank instruments
        adlinsdata2 ins[128];
    };
    typedef BasicBankMap<Bank> BankMap;
    //! MIDI bank instruments data
    BankMap         m_insBanks;
    //! MIDI bank-wide setup
    AdlBankSetup    m_insBankSetup;

public:
    //! Blank instrument template
    static const adlinsdata2 m_emptyInstrument;
    //! Total number of running concurrent emulated chips
    uint32_t m_numChips;
    //! Currently running embedded bank number. "CustomBankTag" means usign of the custom bank.
    uint32_t m_embeddedBank;
    //! Total number of needed four-operator channels in all running chips
    uint32_t m_numFourOps;
    //! Turn global Deep Tremolo mode on
    bool m_deepTremoloMode;
    //! Turn global Deep Vibrato mode on
    bool m_deepVibratoMode;
    //! Use Rhythm Mode percussions
    bool m_rhythmMode;
    //! Carriers-only are scaled by default by volume level. This flag will tell to scale modulators too.
    bool m_scaleModulators;
    //! Run emulator at PCM rate if that possible. Reduces sounding accuracy, but decreases CPU usage on lower rates.
    bool m_runAtPcmRate;
    //! Enable soft panning
    bool m_softPanning;

    //! Just a padding. Reserved.
    char _padding2[3];

    /**
     * @brief Music playing mode
     */
    enum MusicMode
    {
        //! MIDI mode
        MODE_MIDI,
        //! Id-Software Music mode
        MODE_IMF,
        //! Creative Music Files mode
        MODE_CMF,
        //! EA-MUS (a.k.a. RSXX) mode
        MODE_RSXX
    } m_musicMode;

    /**
     * @brief Volume models enum
     */
    enum VolumesScale
    {
        //! Generic volume model (linearization of logarithmic scale)
        VOLUME_Generic,
        //! OPL3 native logarithmic scale
        VOLUME_NATIVE,
        //! DMX volume scale logarithmic table
        VOLUME_DMX,
        //! Apoge Sound System volume scaling model
        VOLUME_APOGEE,
        //! Windows 9x driver volume scale table
        VOLUME_9X
    } m_volumeScale;

    //! Reserved
    char _padding3[8];

    /**
     * @brief Channel categiry enumeration
     */
    enum ChanCat
    {
        //! Regular melodic/percussion channel
        ChanCat_Regular     = 0,
        //! Four-op master
        ChanCat_4op_Master  = 1,
        //! Four-op slave
        ChanCat_4op_Slave   = 2,
        //! Rhythm-mode Bass drum
        ChanCat_Rhythm_Bass     = 3,
        //! Rhythm-mode Snare drum
        ChanCat_Rhythm_Snare    = 4,
        //! Rhythm-mode Tom-Tom
        ChanCat_Rhythm_Tom      = 5,
        //! Rhythm-mode Cymbal
        ChanCat_Rhythm_Cymbal   = 6,
        //! Rhythm-mode Hi-Hat
        ChanCat_Rhythm_HiHat    = 7,
        //! Rhythm-mode Slave channel
        ChanCat_Rhythm_Slave    = 8
    };

    //! Category of the channel
    /*! 1 = quad-master, 2 = quad-slave, 0 = regular
        3 = percussion BassDrum
        4 = percussion Snare
        5 = percussion Tom
        6 = percussion Crash cymbal
        7 = percussion Hihat
        8 = percussion slave
    */
    std::vector<uint32_t> m_channelCategory;


    /**
     * @brief C.O. Constructor
     */
    OPL3();

    /**
     * @brief Checks are setup locked to be changed on the fly or not
     * @return true when setup on the fly is locked
     */
    bool setupLocked();

    /**
     * @brief Choose one of embedded banks
     * @param bank ID of the bank
     */
    void setEmbeddedBank(uint32_t bank);

    /**
     * @brief Write data to OPL3 chip register
     * @param chip Index of emulated chip. In hardware OPL3 builds, this parameter is ignored
     * @param address Register address to write
     * @param value Value to write
     */
    void writeReg(size_t chip, uint16_t address, uint8_t value);

    /**
     * @brief Write data to OPL3 chip register
     * @param chip Index of emulated chip. In hardware OPL3 builds, this parameter is ignored
     * @param address Register address to write
     * @param value Value to write
     */
    void writeRegI(size_t chip, uint32_t address, uint32_t value);

    /**
     * @brief Write to soft panning control of OPL3 chip emulator
     * @param chip Index of emulated chip.
     * @param address Register of channel to write
     * @param value Value to write
     */
    void writePan(size_t chip, uint32_t address, uint32_t value);

    /**
     * @brief Off the note in specified chip channel
     * @param c Channel of chip (Emulated chip choosing by next formula: [c = ch + (chipId * 23)])
     */
    void noteOff(size_t c);

    /**
     * @brief On the note in specified chip channel with specified frequency of the tone
     * @param c1 Channel of chip [or master 4-op channel] (Emulated chip choosing by next formula: [c = ch + (chipId * 23)])
     * @param c2 Slave 4-op channel of chip, unused for 2op (Emulated chip choosing by next formula: [c = ch + (chipId * 23)])
     * @param hertz Frequency of the tone in hertzes
     */
    void noteOn(size_t c1, size_t c2, double hertz);

    /**
     * @brief Change setup of instrument in specified chip channel
     * @param c Channel of chip (Emulated chip choosing by next formula: [c = ch + (chipId * 23)])
     * @param volume Volume level (from 0 to 63)
     * @param brightness CC74 Brightness level (from 0 to 127)
     */
    void touchNote(size_t c, uint8_t volume, uint8_t brightness = 127);

    /**
     * @brief Set the instrument into specified chip channel
     * @param c Channel of chip (Emulated chip choosing by next formula: [c = ch + (chipId * 23)])
     * @param instrument Instrument data to set into the chip channel
     */
    void setPatch(size_t c, const adldata &instrument);

    /**
     * @brief Set panpot position
     * @param c Channel of chip (Emulated chip choosing by next formula: [c = ch + (chipId * 23)])
     * @param value 3-bit panpot value
     */
    void setPan(size_t c, uint8_t value);

    /**
     * @brief Shut up all chip channels
     */
    void silenceAll();

    /**
     * @brief Commit updated flag states to chip registers
     */
    void updateChannelCategories();

    /**
     * @brief commit deepTremolo and deepVibrato flags
     */
    void commitDeepFlags();

    /**
     * @brief Set the volume scaling model
     * @param volumeModel Type of volume scale model scale
     */
    void setVolumeScaleModel(ADLMIDI_VolumeModels volumeModel);

    /**
     * @brief Get the volume scaling model
     */
    ADLMIDI_VolumeModels getVolumeScaleModel();

    #ifndef ADLMIDI_HW_OPL
    /**
     * @brief Clean up all running emulated chip instances
     */
    void clearChips();
    #endif

    /**
     * @brief Reset chip properties and initialize them
     * @param emulator Type of chip emulator
     * @param PCM_RATE Output sample rate to generate on output
     * @param audioTickHandler PCM-accurate clock hook
     */
    void reset(int emulator, unsigned long PCM_RATE, void *audioTickHandler);
};


/**
 * @brief Hooks of the internal events
 */
struct MIDIEventHooks
{
    MIDIEventHooks() :
        onNote(NULL),
        onNote_userData(NULL),
        onDebugMessage(NULL),
        onDebugMessage_userData(NULL)
    {}

    //! Note on/off hooks
    typedef void (*NoteHook)(void *userdata, int adlchn, int note, int ins, int pressure, double bend);
    NoteHook     onNote;
    void         *onNote_userData;

    //! Library internal debug messages
    typedef void (*DebugMessageHook)(void *userdata, const char *fmt, ...);
    DebugMessageHook onDebugMessage;
    void *onDebugMessage_userData;
};


class MIDIplay
{
    friend void adl_reset(struct ADL_MIDIPlayer*);
public:
    explicit MIDIplay(unsigned long sampleRate = 22050);

    ~MIDIplay()
    {}

    void applySetup();

    void partialReset();
    void resetMIDI();

    /**********************Internal structures and classes**********************/

    /**
     * @brief Persistent settings for each MIDI channel
     */
    struct MIDIchannel
    {
        //! LSB Bank number
        uint8_t bank_lsb,
        //! MSB Bank number
                bank_msb;
        //! Current patch number
        uint8_t patch;
        //! Volume level
        uint8_t volume,
        //! Expression level
                expression;
        //! Panning level
        uint8_t panning,
        //! Vibrato level
                vibrato,
        //! Channel aftertouch level
                aftertouch;
        //! Portamento time
        uint16_t portamento;
        //! Is Pedal sustain active
        bool sustain;
        //! Is Soft pedal active
        bool softPedal;
        //! Is portamento enabled
        bool portamentoEnable;
        //! Source note number used by portamento
        int8_t portamentoSource;  // note number or -1
        //! Portamento rate
        double portamentoRate;
        //! Per note Aftertouch values
        uint8_t noteAftertouch[128];
        //! Is note aftertouch has any non-zero value
        bool    noteAfterTouchInUse;
        //! Reserved
        char _padding[6];
        //! Pitch bend value
        int bend;
        //! Pitch bend sensitivity
        double bendsense;
        //! Pitch bend sensitivity LSB value
        int bendsense_lsb,
        //! Pitch bend sensitivity MSB value
            bendsense_msb;
        //! Vibrato position value
        double  vibpos,
        //! Vibrato speed value
                vibspeed,
        //! Vibrato depth value
                vibdepth;
        //! Vibrato delay time
        int64_t vibdelay_us;
        //! Last LSB part of RPN value received
        uint8_t lastlrpn,
        //! Last MSB poart of RPN value received
                lastmrpn;
        //! Interpret RPN value as NRPN
        bool nrpn;
        //! Brightness level
        uint8_t brightness;

        //! Is melodic channel turned into percussion
        bool is_xg_percussion;

        /**
         * @brief Per-Note information
         */
        struct NoteInfo
        {
            //! Note number
            uint8_t note;
            //! Is note active
            bool active;
            //! Current pressure
            uint8_t vol;
            //! Note vibrato (a part of Note Aftertouch feature)
            uint8_t vibrato;
            //! Tone selected on noteon:
            int16_t noteTone;
            //! Current tone (!= noteTone if gliding note)
            double currentTone;
            //! Gliding rate
            double glideRate;
            //! Patch selected on noteon; index to bank.ins[]
            size_t  midiins;
            //! Is note the percussion instrument
            bool    isPercussion;
            //! Note that plays missing instrument. Doesn't using any chip channels
            bool    isBlank;
            //! Patch selected
            const adlinsdata2 *ains;
            enum
            {
                MaxNumPhysChans = 2,
                MaxNumPhysItemCount = MaxNumPhysChans
            };

            /**
             * @brief Reference to currently using chip channel
             */
            struct Phys
            {
                //! Destination chip channel
                uint16_t chip_chan;
                //! ins, inde to adl[]
                adldata ains;
                //! Is this voice must be detunable?
                bool    pseudo4op;

                void assign(const Phys &oth)
                {
                    ains = oth.ains;
                    pseudo4op = oth.pseudo4op;
                }
                bool operator==(const Phys &oth) const
                {
                    return (ains == oth.ains) && (pseudo4op == oth.pseudo4op);
                }
                bool operator!=(const Phys &oth) const
                {
                    return !operator==(oth);
                }
            };

            //! List of OPL3 channels it is currently occupying.
            Phys chip_channels[MaxNumPhysItemCount];
            //! Count of used channels.
            unsigned chip_channels_count;

            Phys *phys_find(unsigned chip_chan)
            {
                Phys *ph = NULL;
                for(unsigned i = 0; i < chip_channels_count && !ph; ++i)
                    if(chip_channels[i].chip_chan == chip_chan)
                        ph = &chip_channels[i];
                return ph;
            }
            Phys *phys_find_or_create(uint16_t chip_chan)
            {
                Phys *ph = phys_find(chip_chan);
                if(!ph) {
                    if(chip_channels_count < MaxNumPhysItemCount) {
                        ph = &chip_channels[chip_channels_count++];
                        ph->chip_chan = chip_chan;
                    }
                }
                return ph;
            }
            Phys *phys_ensure_find_or_create(uint16_t chip_chan)
            {
                Phys *ph = phys_find_or_create(chip_chan);
                assert(ph);
                return ph;
            }
            void phys_erase_at(const Phys *ph)
            {
                intptr_t pos = ph - chip_channels;
                assert(pos < static_cast<intptr_t>(chip_channels_count));
                for(intptr_t i = pos + 1; i < static_cast<intptr_t>(chip_channels_count); ++i)
                    chip_channels[i - 1] = chip_channels[i];
                --chip_channels_count;
            }
            void phys_erase(unsigned chip_chan)
            {
                Phys *ph = phys_find(chip_chan);
                if(ph)
                    phys_erase_at(ph);
            }
        };

        //! Reserved
        char _padding2[5];
        //! Count of gliding notes in this channel
        unsigned gliding_note_count;

        //! Active notes in the channel
        NoteInfo activenotes[128];

        struct activenoteiterator
        {
            explicit activenoteiterator(NoteInfo *info = NULL)
                : ptr(info) {}
            activenoteiterator &operator++()
            {
                if(ptr->note == 127)
                    ptr = NULL;
                else
                    for(++ptr; ptr && !ptr->active;)
                        ptr = (ptr->note == 127) ? NULL : (ptr + 1);
                return *this;
            }
            activenoteiterator operator++(int)
            {
                activenoteiterator pos = *this;
                ++*this;
                return pos;
            }
            NoteInfo &operator*() const
                { return *ptr; }
            NoteInfo *operator->() const
                { return ptr; }
            bool operator==(activenoteiterator other) const
                { return ptr == other.ptr; }
            bool operator!=(activenoteiterator other) const
                { return ptr != other.ptr; }
            operator NoteInfo *() const
                { return ptr; }
        private:
            NoteInfo *ptr;
        };

        activenoteiterator activenotes_begin()
        {
            activenoteiterator it(activenotes);
            return (it->active) ? it : ++it;
        }

        activenoteiterator activenotes_find(uint8_t note)
        {
            assert(note < 128);
            return activenoteiterator(
                activenotes[note].active ? &activenotes[note] : NULL);
        }

        activenoteiterator activenotes_ensure_find(uint8_t note)
        {
            activenoteiterator it = activenotes_find(note);
            assert(it);
            return it;
        }

        std::pair<activenoteiterator, bool> activenotes_insert(uint8_t note)
        {
            assert(note < 128);
            NoteInfo &info = activenotes[note];
            bool inserted = !info.active;
            if(inserted) info.active = true;
            return std::pair<activenoteiterator, bool>(activenoteiterator(&info), inserted);
        }

        void activenotes_erase(activenoteiterator pos)
        {
            if(pos)
                pos->active = false;
        }

        bool activenotes_empty()
        {
            return !activenotes_begin();
        }

        void activenotes_clear()
        {
            for(uint8_t i = 0; i < 128; ++i) {
                activenotes[i].note = i;
                activenotes[i].active = false;
            }
        }

        /**
         * @brief Reset channel into initial state
         */
        void reset()
        {
            resetAllControllers();
            patch = 0;
            vibpos = 0;
            bank_lsb = 0;
            bank_msb = 0;
            lastlrpn = 0;
            lastmrpn = 0;
            nrpn = false;
            is_xg_percussion = false;
        }

        /**
         * @brief Reset all MIDI controllers into initial state
         */
        void resetAllControllers()
        {
            bend = 0;
            bendsense_msb = 2;
            bendsense_lsb = 0;
            updateBendSensitivity();
            volume  = 100;
            expression = 127;
            sustain = false;
            softPedal = false;
            vibrato = 0;
            aftertouch = 0;
            std::memset(noteAftertouch, 0, 128);
            noteAfterTouchInUse = false;
            vibspeed = 2 * 3.141592653 * 5.0;
            vibdepth = 0.5 / 127;
            vibdelay_us = 0;
            panning = 64;
            portamento = 0;
            portamentoEnable = false;
            portamentoSource = -1;
            portamentoRate = HUGE_VAL;
            brightness = 127;
        }

        /**
         * @brief Has channel vibrato to process
         * @return
         */
        bool hasVibrato()
        {
            return (vibrato > 0) || (aftertouch > 0) || noteAfterTouchInUse;
        }

        /**
         * @brief Commit pitch bend sensitivity value from MSB and LSB
         */
        void updateBendSensitivity()
        {
            int cent = bendsense_msb * 128 + bendsense_lsb;
            bendsense = cent * (1.0 / (128 * 8192));
        }

        MIDIchannel()
        {
            activenotes_clear();
            gliding_note_count = 0;
            reset();
        }
    };

    /**
     * @brief Additional information about OPL3 channels
     */
    struct AdlChannel
    {
        struct Location
        {
            uint16_t    MidCh;
            uint8_t     note;
            bool operator==(const Location &l) const
                { return MidCh == l.MidCh && note == l.note; }
            bool operator!=(const Location &l) const
                { return !operator==(l); }
        };
        struct LocationData
        {
            LocationData *prev, *next;
            Location loc;
            enum {
                Sustain_None        = 0x00,
                Sustain_Pedal       = 0x01,
                Sustain_Sostenuto   = 0x02,
                Sustain_ANY         = Sustain_Pedal | Sustain_Sostenuto
            };
            uint32_t sustained;
            char _padding[6];
            MIDIchannel::NoteInfo::Phys ins;  // a copy of that in phys[]
            //! Has fixed sustain, don't iterate "on" timeout
            bool    fixed_sustain;
            //! Timeout until note will be allowed to be killed by channel manager while it is on
            int64_t kon_time_until_neglible_us;
            int64_t vibdelay_us;
        };

        //! Time left until sounding will be muted after key off
        int64_t koff_time_until_neglible_us;

        //! Recently passed instrument, improves a goodness of released but busy channel when matching
        MIDIchannel::NoteInfo::Phys recent_ins;

        enum { users_max = 128 };
        LocationData *users_first, *users_free_cells;
        LocationData users_cells[users_max];
        unsigned users_size;

        bool users_empty() const;
        LocationData *users_find(Location loc);
        LocationData *users_allocate();
        LocationData *users_find_or_create(Location loc);
        LocationData *users_insert(const LocationData &x);
        void users_erase(LocationData *user);
        void users_clear();
        void users_assign(const LocationData *users, size_t count);

        // For channel allocation:
        AdlChannel(): koff_time_until_neglible_us(0)
        {
            users_clear();
            std::memset(&recent_ins, 0, sizeof(MIDIchannel::NoteInfo::Phys));
        }

        AdlChannel(const AdlChannel &oth): koff_time_until_neglible_us(oth.koff_time_until_neglible_us)
        {
            if(oth.users_first)
            {
                users_first = NULL;
                users_assign(oth.users_first, oth.users_size);
            }
            else
                users_clear();
        }

        AdlChannel &operator=(const AdlChannel &oth)
        {
            koff_time_until_neglible_us = oth.koff_time_until_neglible_us;
            users_assign(oth.users_first, oth.users_size);
            return *this;
        }

        /**
         * @brief Increases age of active note in microseconds time
         * @param us Amount time in microseconds
         */
        void addAge(int64_t us);
    };

#ifndef ADLMIDI_DISABLE_MIDI_SEQUENCER
    /**
     * @brief MIDI files player sequencer
     */
    MidiSequencer m_sequencer;

    /**
     * @brief Interface between MIDI sequencer and this library
     */
    BW_MidiRtInterface m_sequencerInterface;

    /**
     * @brief Initialize MIDI sequencer interface
     */
    void initSequencerInterface();
#endif //ADLMIDI_DISABLE_MIDI_SEQUENCER

    struct Setup
    {
        int          emulator;
        bool         runAtPcmRate;
        unsigned int bankId;
        int          numFourOps;
        unsigned int numChips;
        int     deepTremoloMode;
        int     deepVibratoMode;
        int     rhythmMode;
        bool    logarithmicVolumes;
        int     volumeScaleModel;
        //unsigned int SkipForward;
        int     scaleModulators;
        bool    fullRangeBrightnessCC74;

        double delay;
        double carry;

        /* The lag between visual content and audio content equals */
        /* the sum of these two buffers. */
        double mindelay;
        double maxdelay;

        /* For internal usage */
        ssize_t tick_skip_samples_delay; /* Skip tick processing after samples count. */
        /* For internal usage */

        unsigned long PCM_RATE;
    };

    /**
     * @brief MIDI Marker entry
     */
    struct MIDI_MarkerEntry
    {
        //! Label of marker
        std::string     label;
        //! Absolute position in seconds
        double          pos_time;
        //! Absolute position in ticks in the track
        uint64_t        pos_ticks;
    };

    //! Available MIDI Channels
    std::vector<MIDIchannel> m_midiChannels;

    //! CMF Rhythm mode
    bool    m_cmfPercussionMode;

    //! Master volume, controlled via SysEx
    uint8_t m_masterVolume;

    //! SysEx device ID
    uint8_t m_sysExDeviceId;

    /**
     * @brief MIDI Synthesizer mode
     */
    enum SynthMode
    {
        Mode_GM  = 0x00,
        Mode_GS  = 0x01,
        Mode_XG  = 0x02,
        Mode_GM2 = 0x04
    };
    //! MIDI Synthesizer mode
    uint32_t m_synthMode;

    //! Installed function hooks
    MIDIEventHooks hooks;

private:
    //! Per-track MIDI devices map
    std::map<std::string, size_t> m_midiDevices;
    //! Current MIDI device per track
    std::map<size_t /*track*/, size_t /*channel begin index*/> m_currentMidiDevice;

    //! Padding to fix CLanc code model's warning
    char _padding[7];

    //! Chip channels map
    std::vector<AdlChannel> m_chipChannels;
    //! Counter of arpeggio processing
    size_t m_arpeggioCounter;

#if defined(ADLMIDI_AUDIO_TICK_HANDLER)
    //! Audio tick counter
    uint32_t m_audioTickCounter;
#endif

    //! Local error string
    std::string errorStringOut;

    //! Missing instruments catches
    std::set<size_t> caugh_missing_instruments;
    //! Missing melodic banks catches
    std::set<size_t> caugh_missing_banks_melodic;
    //! Missing percussion banks catches
    std::set<size_t> caugh_missing_banks_percussion;

public:

    const std::string &getErrorString();
    void setErrorString(const std::string &err);

    //! OPL3 Chip manager
    OPL3 m_synth;

    //! Generator output buffer
    int32_t m_outBuf[1024];

    //! Synthesizer setup
    Setup m_setup;

    /**
     * @brief Load custom bank from file
     * @param filename Path to bank file
     * @return true on succes
     */
    bool LoadBank(const std::string &filename);

    /**
     * @brief Load custom bank from memory block
     * @param data Pointer to memory block where raw bank file is stored
     * @param size Size of given memory block
     * @return true on succes
     */
    bool LoadBank(const void *data, size_t size);

    /**
     * @brief Load custom bank from opened FileAndMemReader class
     * @param fr Instance with opened file
     * @return true on succes
     */
    bool LoadBank(FileAndMemReader &fr);

#ifndef ADLMIDI_DISABLE_MIDI_SEQUENCER
    /**
     * @brief MIDI file loading pre-process
     * @return true on success, false on failure
     */
    bool LoadMIDI_pre();

    /**
     * @brief MIDI file loading post-process
     * @return true on success, false on failure
     */
    bool LoadMIDI_post();

    /**
     * @brief Load music file from a file
     * @param filename Path to music file
     * @return true on success, false on failure
     */

    bool LoadMIDI(const std::string &filename);

    /**
     * @brief Load music file from the memory block
     * @param data pointer to the memory block
     * @param size size of memory block
     * @return true on success, false on failure
     */
    bool LoadMIDI(const void *data, size_t size);

    /**
     * @brief Periodic tick handler.
     * @param s seconds since last call
     * @param granularity don't expect intervals smaller than this, in seconds
     * @return desired number of seconds until next call
     */
    double Tick(double s, double granularity);
#endif //ADLMIDI_DISABLE_MIDI_SEQUENCER

    /**
     * @brief Process extra iterators like vibrato or arpeggio
     * @param s seconds since last call
     */
    void   TickIterators(double s);


    /* RealTime event triggers */
    /**
     * @brief Reset state of all channels
     */
    void realTime_ResetState();

    /**
     * @brief Note On event
     * @param channel MIDI channel
     * @param note Note key (from 0 to 127)
     * @param velocity Velocity level (from 0 to 127)
     * @return true if Note On event was accepted
     */
    bool realTime_NoteOn(uint8_t channel, uint8_t note, uint8_t velocity);

    /**
     * @brief Note Off event
     * @param channel MIDI channel
     * @param note Note key (from 0 to 127)
     */
    void realTime_NoteOff(uint8_t channel, uint8_t note);

    /**
     * @brief Note aftertouch event
     * @param channel MIDI channel
     * @param note Note key (from 0 to 127)
     * @param atVal After-Touch level (from 0 to 127)
     */
    void realTime_NoteAfterTouch(uint8_t channel, uint8_t note, uint8_t atVal);

    /**
     * @brief Channel aftertouch event
     * @param channel MIDI channel
     * @param atVal After-Touch level (from 0 to 127)
     */
    void realTime_ChannelAfterTouch(uint8_t channel, uint8_t atVal);

    /**
     * @brief Controller Change event
     * @param channel MIDI channel
     * @param type Type of controller
     * @param value Value of the controller (from 0 to 127)
     */
    void realTime_Controller(uint8_t channel, uint8_t type, uint8_t value);

    /**
     * @brief Patch change
     * @param channel MIDI channel
     * @param patch Patch Number (from 0 to 127)
     */
    void realTime_PatchChange(uint8_t channel, uint8_t patch);

    /**
     * @brief Pitch bend change
     * @param channel MIDI channel
     * @param pitch Concoctated raw pitch value
     */
    void realTime_PitchBend(uint8_t channel, uint16_t pitch);

    /**
     * @brief Pitch bend change
     * @param channel MIDI channel
     * @param msb MSB of pitch value
     * @param lsb LSB of pitch value
     */
    void realTime_PitchBend(uint8_t channel, uint8_t msb, uint8_t lsb);

    /**
     * @brief LSB Bank Change CC
     * @param channel MIDI channel
     * @param lsb LSB value of bank number
     */
    void realTime_BankChangeLSB(uint8_t channel, uint8_t lsb);

    /**
     * @brief MSB Bank Change CC
     * @param channel MIDI channel
     * @param msb MSB value of bank number
     */
    void realTime_BankChangeMSB(uint8_t channel, uint8_t msb);

    /**
     * @brief Bank Change (united value)
     * @param channel MIDI channel
     * @param bank Bank number value
     */
    void realTime_BankChange(uint8_t channel, uint16_t bank);

    /**
     * @brief Sets the Device identifier
     * @param id 7-bit Device identifier
     */
    void setDeviceId(uint8_t id);

    /**
     * @brief System Exclusive message
     * @param msg Raw SysEx Message
     * @param size Length of SysEx message
     * @return true if message was passed successfully. False on any errors
     */
    bool realTime_SysEx(const uint8_t *msg, size_t size);

    /**
     * @brief Turn off all notes and mute the sound of releasing notes
     */
    void realTime_panic();

    /**
     * @brief Device switch (to extend 16-channels limit of MIDI standard)
     * @param track MIDI track index
     * @param data Device name
     * @param length Length of device name string
     */
    void realTime_deviceSwitch(size_t track, const char *data, size_t length);

    /**
     * @brief Currently selected device index
     * @param track MIDI track index
     * @return Multiple 16 value
     */
    size_t realTime_currentDevice(size_t track);

    /**
     * @brief Send raw OPL chip command
     * @param reg OPL Register
     * @param value Value to write
     */
    void realTime_rawOPL(uint8_t reg, uint8_t value);

#if defined(ADLMIDI_AUDIO_TICK_HANDLER)
    // Audio rate tick handler
    void AudioTick(uint32_t chipId, uint32_t rate);
#endif

private:
    /**
     * @brief Hardware manufacturer (Used for SysEx)
     */
    enum
    {
        Manufacturer_Roland               = 0x41,
        Manufacturer_Yamaha               = 0x43,
        Manufacturer_UniversalNonRealtime = 0x7E,
        Manufacturer_UniversalRealtime    = 0x7F
    };

    /**
     * @brief Roland Mode (Used for SysEx)
     */
    enum
    {
        RolandMode_Request = 0x11,
        RolandMode_Send    = 0x12
    };

    /**
     * @brief Device model (Used for SysEx)
     */
    enum
    {
        RolandModel_GS   = 0x42,
        RolandModel_SC55 = 0x45,
        YamahaModel_XG   = 0x4C
    };

    /**
     * @brief Process generic SysEx events
     * @param dev Device ID
     * @param realtime Is real-time event
     * @param data Raw SysEx data
     * @param size Size of given SysEx data
     * @return true when event was successfully handled
     */
    bool doUniversalSysEx(unsigned dev, bool realtime, const uint8_t *data, size_t size);

    /**
     * @brief Process events specific to Roland devices
     * @param dev Device ID
     * @param data Raw SysEx data
     * @param size Size of given SysEx data
     * @return true when event was successfully handled
     */
    bool doRolandSysEx(unsigned dev, const uint8_t *data, size_t size);

    /**
     * @brief Process events specific to Yamaha devices
     * @param dev Device ID
     * @param data Raw SysEx data
     * @param size Size of given SysEx data
     * @return true when event was successfully handled
     */
    bool doYamahaSysEx(unsigned dev, const uint8_t *data, size_t size);

private:
    /**
     * @brief Note Update properties
     */
    enum
    {
        Upd_Patch  = 0x1,
        Upd_Pan    = 0x2,
        Upd_Volume = 0x4,
        Upd_Pitch  = 0x8,
        Upd_All    = Upd_Pan + Upd_Volume + Upd_Pitch,
        Upd_Off    = 0x20,
        Upd_Mute   = 0x40,
        Upd_OffMute = Upd_Off + Upd_Mute
    };

    /**
     * @brief Update active note
     * @param MidCh MIDI Channel where note is processing
     * @param i Iterator that points to active note in the MIDI channel
     * @param props_mask Properties to update
     * @param select_adlchn Specify chip channel, or -1 - all chip channels used by the note
     */
    void noteUpdate(size_t midCh,
                    MIDIchannel::activenoteiterator i,
                    unsigned props_mask,
                    int32_t select_adlchn = -1);

    /**
     * @brief Update all notes in specified MIDI channel
     * @param midCh MIDI channel to update all notes in it
     * @param props_mask Properties to update
     */
    void noteUpdateAll(size_t midCh, unsigned props_mask);

    /**
     * @brief Determine how good a candidate this adlchannel would be for playing a note from this instrument.
     * @param c Wanted chip channel
     * @param ins Instrument wanted to be used in this channel
     * @return Calculated coodness points
     */
    int64_t calculateChipChannelGoodness(size_t c, const MIDIchannel::NoteInfo::Phys &ins) const;

    /**
     * @brief A new note will be played on this channel using this instrument.
     * @param c Wanted chip channel
     * @param ins Instrument wanted to be used in this channel
     * Kill existing notes on this channel (or don't, if we do arpeggio)
     */
    void prepareChipChannelForNewNote(size_t c, const MIDIchannel::NoteInfo::Phys &ins);

    /**
     * @brief Kills note that uses wanted channel. When arpeggio is possible, note is evaluating to another channel
     * @param from_channel Wanted chip channel
     * @param j Chip channel instance
     * @param i MIDI Channel active note instance
     */
    void killOrEvacuate(
        size_t  from_channel,
        AdlChannel::LocationData *j,
        MIDIchannel::activenoteiterator i);

    /**
     * @brief Off all notes and silence sound
     */
    void panic();

    /**
     * @brief Kill note, sustaining by pedal or sostenuto
     * @param MidCh MIDI channel, -1 - all MIDI channels
     * @param this_adlchn Chip channel, -1 - all chip channels
     * @param sustain_type Type of systain to process
     */
    void killSustainingNotes(int32_t midCh = -1,
                             int32_t this_adlchn = -1,
                             uint32_t sustain_type = AdlChannel::LocationData::Sustain_ANY);
    /**
     * @brief Find active notes and mark them as sostenuto-sustained
     * @param MidCh MIDI channel, -1 - all MIDI channels
     */
    void markSostenutoNotes(int32_t midCh = -1);

    /**
     * @brief Set RPN event value
     * @param MidCh MIDI channel
     * @param value 1 byte part of RPN value
     * @param MSB is MSB or LSB part of value
     */
    void setRPN(size_t midCh, unsigned value, bool MSB);

    /**
     * @brief Update portamento setup in MIDI channel
     * @param midCh MIDI channel where portamento needed to be updated
     */
    void updatePortamento(size_t midCh);

    /**
     * @brief Off the note
     * @param midCh MIDI channel
     * @param note Note to off
     */
    void noteOff(size_t midCh, uint8_t note);

    /**
     * @brief Update processing of vibrato to amount of seconds
     * @param amount Amount value in seconds
     */
    void updateVibrato(double amount);

    /**
     * @brief Update auto-arpeggio
     * @param amount Amount value in seconds [UNUSED]
     */
    void updateArpeggio(double /*amount*/);

    /**
     * @brief Update Portamento gliding to amount of seconds
     * @param amount Amount value in seconds
     */
    void updateGlide(double amount);

public:
    /**
     * @brief Checks was device name used or not
     * @param name Name of MIDI device
     * @return Offset of the MIDI Channels, multiple to 16
     */
    size_t chooseDevice(const std::string &name);

    /**
     * @brief Gets a textual description of the state of chip channels
     * @param text character pointer for text
     * @param attr character pointer for text attributes
     * @param size number of characters available to write
     */
    void describeChannels(char *text, char *attr, size_t size);
};

// I think, this is useless inside of Library
/*
struct FourChars
{
    char ret[4];

    FourChars(const char *s)
    {
        for(unsigned c = 0; c < 4; ++c)
            ret[c] = s[c];
    }
    FourChars(unsigned w) // Little-endian
    {
        for(unsigned c = 0; c < 4; ++c)
            ret[c] = static_cast<int8_t>((w >>(c * 8)) & 0xFF);
    }
};
*/

#if defined(ADLMIDI_AUDIO_TICK_HANDLER)
extern void adl_audioTickHandler(void *instance, uint32_t chipId, uint32_t rate);
#endif

/**
 * @brief Automatically calculate and enable necessary count of 4-op channels on emulated chips
 * @param device Library context
 * @param silent Don't re-count channel categories
 * @return Always 0
 */
extern int adlCalculateFourOpChannels(MIDIplay *play, bool silent = false);

/**
 * @brief Check emulator availability
 * @param emulator Emulator ID (ADL_Emulator)
 * @return true when emulator is available
 */
extern bool adl_isEmulatorAvailable(int emulator);

/**
 * @brief Find highest emulator
 * @return The ADL_Emulator enum value which contains ID of highest emulator
 */
extern int adl_getHighestEmulator();

/**
 * @brief Find lowest emulator
 * @return The ADL_Emulator enum value which contains ID of lowest emulator
 */
extern int adl_getLowestEmulator();

#endif // ADLMIDI_PRIVATE_HPP
