/*
 * libADLMIDI is a free MIDI to WAV conversion library with OPL3 emulation
 *
 * Original ADLMIDI code: Copyright (c) 2010-2014 Joel Yliluoma <bisqwit@iki.fi>
 * ADLMIDI Library API:   Copyright (c) 2017-2018 Vitaly Novichkov <admin@wohlnet.ru>
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
#ifndef OPNMIDI_EXPORT
#   if defined (_WIN32) && defined(ADLMIDI_BUILD_DLL)
#       define OPNMIDI_EXPORT __declspec(dllexport)
#   elif defined (LIBADLMIDI_VISIBILITY)
#       define OPNMIDI_EXPORT __attribute__((visibility ("default")))
#   else
#       define OPNMIDI_EXPORT
#   endif
#endif

#ifdef _WIN32
#   undef NO_OLDNAMES

#   ifdef _MSC_VER
#       ifdef _WIN64
typedef __int64 ssize_t;
#       else
typedef __int32 ssize_t;
#       endif
#       define NOMINMAX //Don't override std::min and std::max
#   endif
#   include <windows.h>
#endif

#ifdef USE_LEGACY_EMULATOR // Kept for a backward compatibility
#define OPNMIDI_USE_LEGACY_EMULATOR
#endif

#include <vector>
#include <list>
#include <string>
#include <map>
#include <set>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cassert>
#include <vector> // vector
#include <deque>  // deque
#include <cmath>  // exp, log, ceil
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

#include "fraction.hpp"
#include "chips/opn_chip_base.h"

#include "opnbank.h"
#include "opnmidi.h"
#include "opnmidi_ptr.hpp"
#include "opnmidi_bankmap.h"

#define ADL_UNUSED(x) (void)x

#define OPN_PANNING_LEFT    0x80
#define OPN_PANNING_RIGHT   0x40
#define OPN_PANNING_BOTH    0xC0

extern std::string OPN2MIDI_ErrorString;

/*
  Sample conversions to various formats
*/
template <class Real>
inline Real opn2_cvtReal(int32_t x)
{
    return x * ((Real)1 / INT16_MAX);
}
inline int32_t opn2_cvtS16(int32_t x)
{
    x = (x < INT16_MIN) ? INT16_MIN : x;
    x = (x > INT16_MAX) ? INT16_MAX : x;
    return x;
}
inline int32_t opn2_cvtS8(int32_t x)
{
    return opn2_cvtS16(x) / 256;
}
inline int32_t opn2_cvtS24(int32_t x)
{
    return opn2_cvtS16(x) * 256;
}
inline int32_t opn2_cvtS32(int32_t x)
{
    return opn2_cvtS16(x) * 65536;
}
inline int32_t opn2_cvtU16(int32_t x)
{
    return opn2_cvtS16(x) - INT16_MIN;
}
inline int32_t opn2_cvtU8(int32_t x)
{
    return opn2_cvtS8(x) - INT8_MIN;
}
inline int32_t opn2_cvtU24(int32_t x)
{
    enum { int24_min = -(1 << 23) };
    return opn2_cvtS24(x) - int24_min;
}
inline int32_t opn2_cvtU32(int32_t x)
{
    // unsigned operation because overflow on signed integers is undefined
    return (uint32_t)opn2_cvtS32(x) - (uint32_t)INT32_MIN;
}

class OPNMIDIplay;
class OPN2
{
public:
    friend class OPNMIDIplay;
    uint32_t NumChannels;
    char ____padding[4];
    std::vector<AdlMIDI_SPtr<OPNChipBase > > cardsOP2;
private:
    std::vector<opnInstData> ins;  // patch data, cached, needed by Touch()
    std::vector<uint8_t>    pit;  // value poked to B0, cached, needed by NoteOff)(
    std::vector<uint8_t>    regBD;
    uint8_t                 regLFO;

    void cleanInstrumentBanks();
public:
    struct Bank
    {
        opnInstMeta2 ins[128];
    };
    typedef BasicBankMap<Bank> BankMap;
    BankMap dynamic_banks;
public:
    static const opnInstMeta2 emptyInstrument;
    enum { PercussionTag = 1 << 15 };

    //! Total number of running concurrent emulated chips
    unsigned int NumCards;
    //! Carriers-only are scaled by default by volume level. This flag will tell to scale modulators too.
    bool ScaleModulators;
    //! Run emulator at PCM rate if that possible. Reduces sounding accuracy, but decreases CPU usage on lower rates.
    bool runAtPcmRate;

    char ___padding2[3];

    enum MusicMode
    {
        MODE_MIDI,
        //MODE_IMF, OPN2 chip is not able to interpret OPL's except of a creepy and ugly conversion :-P
        //MODE_CMF, CMF also is not supported :-P
        MODE_RSXX
    } m_musicMode;

    enum VolumesScale
    {
        VOLUME_Generic,
        VOLUME_CMF,
        VOLUME_DMX,
        VOLUME_APOGEE,
        VOLUME_9X
    } m_volumeScale;

    OPN2();
    ~OPN2();
    char ____padding3[8];
    std::vector<char> four_op_category; // 1 = quad-master, 2 = quad-slave, 0 = regular
    // 3 = percussion BassDrum
    // 4 = percussion Snare
    // 5 = percussion Tom
    // 6 = percussion Crash cymbal
    // 7 = percussion Hihat
    // 8 = percussion slave

    void PokeO(size_t card, uint8_t port, uint8_t index, uint8_t value);

    void NoteOff(size_t c);
    void NoteOn(unsigned c, double hertz);
    void Touch_Real(unsigned c, unsigned volume, uint8_t brightness = 127);

    void Patch(uint16_t c, const opnInstData &adli);
    void Pan(unsigned c, unsigned value);
    void Silence();
    void ChangeVolumeRangesModel(OPNMIDI_VolumeModels volumeModel);
    void ClearChips();
    void Reset(int emulator, unsigned long PCM_RATE);
};


/**
 * @brief Hooks of the internal events
 */
struct MIDIEventHooks
{
    MIDIEventHooks() :
        onEvent(NULL),
        onEvent_userData(NULL),
        onNote(NULL),
        onNote_userData(NULL),
        onDebugMessage(NULL),
        onDebugMessage_userData(NULL)
    {}
    //! Raw MIDI event hook
    typedef void (*RawEventHook)(void *userdata, uint8_t type, uint8_t subtype, uint8_t channel, const uint8_t *data, size_t len);
    RawEventHook onEvent;
    void         *onEvent_userData;

    //! Note on/off hooks
    typedef void (*NoteHook)(void *userdata, int adlchn, int note, int ins, int pressure, double bend);
    NoteHook     onNote;
    void         *onNote_userData;

    //! Library internal debug messages
    typedef void (*DebugMessageHook)(void *userdata, const char *fmt, ...);
    DebugMessageHook onDebugMessage;
    void *onDebugMessage_userData;
};


class OPNMIDIplay
{
    friend void opn2_reset(struct OPN2_MIDIPlayer*);
public:
    OPNMIDIplay(unsigned long sampleRate = 22050);

    ~OPNMIDIplay()
    {}

    void applySetup();

    /**********************Internal structures and classes**********************/

    /**
     * @brief A little class gives able to read filedata from disk and also from a memory segment
     */
    class fileReader
    {
    public:
        enum relTo
        {
            SET = 0,
            CUR = 1,
            END = 2
        };

        fileReader()
        {
            fp = NULL;
            mp = NULL;
            mp_size = 0;
            mp_tell = 0;
        }
        ~fileReader()
        {
            close();
        }

        void openFile(const char *path)
        {
            #ifndef _WIN32
            fp = std::fopen(path, "rb");
            #else
            wchar_t widePath[MAX_PATH];
            int size = MultiByteToWideChar(CP_UTF8, 0, path, (int)std::strlen(path), widePath, MAX_PATH);
            widePath[size] = '\0';
            fp = _wfopen(widePath, L"rb");
            #endif
            _fileName = path;
            mp = NULL;
            mp_size = 0;
            mp_tell = 0;
        }

        void openData(const void *mem, size_t lenght)
        {
            fp = NULL;
            mp = mem;
            mp_size = lenght;
            mp_tell = 0;
        }

        void seek(long pos, int rel_to)
        {
            if(fp)
                std::fseek(fp, pos, rel_to);
            else
            {
                switch(rel_to)
                {
                case SET:
                    mp_tell = static_cast<size_t>(pos);
                    break;

                case END:
                    mp_tell = mp_size - static_cast<size_t>(pos);
                    break;

                case CUR:
                    mp_tell = mp_tell + static_cast<size_t>(pos);
                    break;
                }

                if(mp_tell > mp_size)
                    mp_tell = mp_size;
            }
        }

        inline void seeku(uint64_t pos, int rel_to)
        {
            seek(static_cast<long>(pos), rel_to);
        }

        size_t read(void *buf, size_t num, size_t size)
        {
            if(fp)
                return std::fread(buf, num, size, fp);
            else
            {
                size_t pos = 0;
                size_t maxSize = static_cast<size_t>(size * num);

                while((pos < maxSize) && (mp_tell < mp_size))
                {
                    reinterpret_cast<unsigned char *>(buf)[pos] = reinterpret_cast<unsigned const char *>(mp)[mp_tell];
                    mp_tell++;
                    pos++;
                }

                return pos;
            }
        }

        int getc()
        {
            if(fp)
                return std::getc(fp);
            else
            {
                if(mp_tell >= mp_size) return -1;
                int x = reinterpret_cast<unsigned const char *>(mp)[mp_tell];
                mp_tell++;
                return x;
            }
        }

        size_t tell()
        {
            if(fp)
                return static_cast<size_t>(std::ftell(fp));
            else
                return mp_tell;
        }

        void close()
        {
            if(fp) std::fclose(fp);

            fp = NULL;
            mp = NULL;
            mp_size = 0;
            mp_tell = 0;
        }

        bool isValid()
        {
            return (fp) || (mp);
        }

        bool eof()
        {
            if(fp)
                return (std::feof(fp) != 0);
            else
                return mp_tell >= mp_size;
        }
        std::string _fileName;
        std::FILE   *fp;
        const void  *mp;
        size_t      mp_size;
        size_t      mp_tell;
    };

    // Persistent settings for each MIDI channel
    struct MIDIchannel
    {
        uint16_t portamento;
        uint8_t bank_lsb, bank_msb;
        uint8_t patch;
        uint8_t volume, expression;
        uint8_t panning, vibrato, aftertouch, sustain;
        //! Per note Aftertouch values
        uint8_t noteAftertouch[128];
        //! Is note aftertouch has any non-zero value
        bool    noteAfterTouchInUse;
        char ____padding[6];
        int bend;
        double bendsense;
        int bendsense_lsb, bendsense_msb;
        double  vibpos, vibspeed, vibdepth;
        int64_t vibdelay;
        uint8_t lastlrpn, lastmrpn;
        bool nrpn;
        uint8_t brightness;
        bool is_xg_percussion;
        struct NoteInfo
        {
            uint8_t note;
            bool active;
            // Current pressure
            uint8_t vol;
            // Note vibrato (a part of Note Aftertouch feature)
            uint8_t vibrato;
            char ____padding[1];
            // Tone selected on noteon:
            int16_t tone;
            char ____padding2[10];
            // Patch selected on noteon; index to banks[AdlBank][]
            size_t  midiins;
            // Patch selected
            const opnInstMeta2 *ains;
            enum
            {
                MaxNumPhysChans = 2,
                MaxNumPhysItemCount = MaxNumPhysChans,
            };
            struct Phys
            {
                //! Destination chip channel
                uint16_t chip_chan;
                //! ins, inde to adl[]
                opnInstData ains;

                void assign(const Phys &oth)
                {
                    ains = oth.ains;
                }
                bool operator==(const Phys &oth) const
                {
                    return (ains == oth.ains);
                }
                bool operator!=(const Phys &oth) const
                {
                    return !operator==(oth);
                }
            };
            // List of OPN2 channels it is currently occupying.
            Phys chip_channels[MaxNumPhysItemCount];
            //! Count of used channels.
            unsigned chip_channels_count;
            //
            Phys *phys_find(unsigned chip_chan)
            {
                Phys *ph = NULL;
                for(unsigned i = 0; i < chip_channels_count && !ph; ++i)
                    if(chip_channels[i].chip_chan == chip_chan)
                        ph = &chip_channels[i];
                return ph;
            }
            Phys *phys_find_or_create(unsigned chip_chan)
            {
                Phys *ph = phys_find(chip_chan);
                if(!ph) {
                    if(chip_channels_count < MaxNumPhysItemCount) {
                        ph = &chip_channels[chip_channels_count++];
                        ph->chip_chan = (uint16_t)chip_chan;
                    }
                }
                return ph;
            }
            Phys *phys_ensure_find_or_create(unsigned chip_chan)
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
        char ____padding2[5];
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
        void resetAllControllers()
        {
            bend = 0;
            bendsense_msb = 2;
            bendsense_lsb = 0;
            updateBendSensitivity();
            volume  = 100;
            expression = 127;
            sustain = 0;
            vibrato = 0;
            aftertouch = 0;
            std::memset(noteAftertouch, 0, 128);
            noteAfterTouchInUse = false;
            vibspeed = 2 * 3.141592653 * 5.0;
            vibdepth = 0.5 / 127;
            vibdelay = 0;
            panning = OPN_PANNING_BOTH;
            portamento = 0;
            brightness = 127;
        }
        bool hasVibrato()
        {
            return (vibrato > 0) || (aftertouch > 0) || noteAfterTouchInUse;
        }
        void updateBendSensitivity()
        {
            int cent = bendsense_msb * 128 + bendsense_lsb;
            bendsense = cent * (1.0 / (128 * 8192));
        }
        MIDIchannel()
        {
            activenotes_clear();
            reset();
        }
    };

    // Additional information about OPN channels
    struct OpnChannel
    {
        struct Location
        {
            uint16_t    MidCh;
            uint8_t     note;
            bool operator==(const Location &l) const
                { return MidCh == l.MidCh && note == l.note; }
            bool operator!=(const Location &l) const
                { return !operator==(l); }
            char ____padding[1];
        };
        struct LocationData
        {
            LocationData *prev, *next;
            Location loc;
            bool sustained;
            char ____padding[3];
            MIDIchannel::NoteInfo::Phys ins;  // a copy of that in phys[]
            //! Has fixed sustain, don't iterate "on" timeout
            bool    fixed_sustain;
            //! Timeout until note will be allowed to be killed by channel manager while it is on
            int64_t kon_time_until_neglible;
            int64_t vibdelay;
        };

        // If the channel is keyoff'd
        int64_t koff_time_until_neglible;

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
        OpnChannel(): koff_time_until_neglible(0)
        {
            users_clear();
        }

        OpnChannel(const OpnChannel &oth): koff_time_until_neglible(oth.koff_time_until_neglible)
        {
            if(oth.users_first)
            {
                users_first = NULL;
                users_assign(oth.users_first, oth.users_size);
            }
            else
                users_clear();
        }

        OpnChannel &operator=(const OpnChannel &oth)
        {
            koff_time_until_neglible = oth.koff_time_until_neglible;
            users_assign(oth.users_first, oth.users_size);
            return *this;
        }

        void AddAge(int64_t ms);
    };

#ifndef OPNMIDI_DISABLE_MIDI_SEQUENCER
    /**
     * @brief MIDI Event utility container
     */
    class MidiEvent
    {
    public:
        MidiEvent();

        enum Types
        {
            T_UNKNOWN       = 0x00,
            T_NOTEOFF       = 0x08,//size == 2
            T_NOTEON        = 0x09,//size == 2
            T_NOTETOUCH     = 0x0A,//size == 2
            T_CTRLCHANGE    = 0x0B,//size == 2
            T_PATCHCHANGE   = 0x0C,//size == 1
            T_CHANAFTTOUCH  = 0x0D,//size == 1
            T_WHEEL         = 0x0E,//size == 2

            T_SYSEX         = 0xF0,//size == len
            T_SYSCOMSPOSPTR = 0xF2,//size == 2
            T_SYSCOMSNGSEL  = 0xF3,//size == 1
            T_SYSEX2        = 0xF7,//size == len
            T_SPECIAL       = 0xFF
        };
        enum SubTypes
        {
            ST_SEQNUMBER    = 0x00,//size == 2
            ST_TEXT         = 0x01,//size == len
            ST_COPYRIGHT    = 0x02,//size == len
            ST_SQTRKTITLE   = 0x03,//size == len
            ST_INSTRTITLE   = 0x04,//size == len
            ST_LYRICS       = 0x05,//size == len
            ST_MARKER       = 0x06,//size == len
            ST_CUEPOINT     = 0x07,//size == len
            ST_DEVICESWITCH = 0x09,//size == len <CUSTOM>
            ST_MIDICHPREFIX = 0x20,//size == 1

            ST_ENDTRACK     = 0x2F,//size == 0
            ST_TEMPOCHANGE  = 0x51,//size == 3
            ST_SMPTEOFFSET  = 0x54,//size == 5
            ST_TIMESIGNATURE = 0x55, //size == 4
            ST_KEYSIGNATURE = 0x59,//size == 2
            ST_SEQUENCERSPEC = 0x7F, //size == len

            /* Non-standard, internal ADLMIDI usage only */
            ST_LOOPSTART    = 0xE1,//size == 0 <CUSTOM>
            ST_LOOPEND      = 0xE2,//size == 0 <CUSTOM>
            ST_RAWOPL       = 0xE3//size == 0 <CUSTOM>
        };
        //! Main type of event
        uint8_t type;
        //! Sub-type of the event
        uint8_t subtype;
        //! Targeted MIDI channel
        uint8_t channel;
        //! Is valid event
        uint8_t isValid;
        //! Reserved 5 bytes padding
        uint8_t __padding[4];
        //! Absolute tick position (Used for the tempo calculation only)
        uint64_t absPosition;
        //! Raw data of this event
        std::vector<uint8_t> data;
    };

    /**
     * @brief A track position event contains a chain of MIDI events until next delay value
     *
     * Created with purpose to sort events by type in the same position
     * (for example, to keep controllers always first than note on events or lower than note-off events)
     */
    class MidiTrackRow
    {
    public:
        MidiTrackRow();
        void reset();
        //! Absolute time position in seconds
        double time;
        //! Delay to next event in ticks
        uint64_t delay;
        //! Absolute position in ticks
        uint64_t absPos;
        //! Delay to next event in seconds
        double timeDelay;
        std::vector<MidiEvent> events;
        /**
         * @brief Sort events in this position
         */
        void sortEvents(bool *noteStates = NULL);
    };

    /**
     * @brief Tempo change point entry. Used in the MIDI data building function only.
     */
    struct TempoChangePoint
    {
        uint64_t absPos;
        fraction<uint64_t> tempo;
    };
    //P.S. I declared it here instead of local in-function because C++99 can't process templates with locally-declared structures

    typedef std::list<MidiTrackRow> MidiTrackQueue;

    // Information about each track
    struct PositionNew
    {
        bool began;
        char padding[7];
        double wait;
        double absTimePosition;
        struct TrackInfo
        {
            size_t ptr;
            uint64_t delay;
            int     status;
            char    padding2[4];
            MidiTrackQueue::iterator pos;
            TrackInfo(): ptr(0), delay(0), status(0) {}
        };
        std::vector<TrackInfo> track;
        PositionNew(): began(false), wait(0.0), absTimePosition(0.0), track()
        {}
    };
#endif //OPNMIDI_DISABLE_MIDI_SEQUENCER

    struct Setup
    {
        int     emulator;
        bool    runAtPcmRate;
        unsigned int OpnBank;
        unsigned int NumCards;
        unsigned int LogarithmicVolumes;
        int     VolumeModel;
        //unsigned int SkipForward;
        bool    loopingIsEnabled;
        int     ScaleModulators;
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

    struct MIDI_MarkerEntry
    {
        std::string     label;
        double          pos_time;
        uint64_t        pos_ticks;
    };

    std::vector<MIDIchannel> Ch;
    //bool cmf_percussion_mode;

    MIDIEventHooks hooks;

private:
    std::map<std::string, uint64_t> devices;
    std::map<uint64_t /*track*/, uint64_t /*channel begin index*/> current_device;

    std::vector<OpnChannel> ch;
    //! Counter of arpeggio processing
    size_t m_arpeggioCounter;

#ifndef OPNMIDI_DISABLE_MIDI_SEQUENCER
    std::vector<std::vector<uint8_t> > TrackData;

    PositionNew CurrentPositionNew, LoopBeginPositionNew, trackBeginPositionNew;

    //! Full song length in seconds
    double fullSongTimeLength;
    //! Delay after song playd before rejecting the output stream requests
    double postSongWaitDelay;

    //! Loop start time
    double loopStartTime;
    //! Loop end time
    double loopEndTime;
#endif
    //! Local error string
    std::string errorString;
    //! Local error string
    std::string errorStringOut;

#ifndef OPNMIDI_DISABLE_MIDI_SEQUENCER
    //! Pre-processed track data storage
    std::vector<MidiTrackQueue > trackDataNew;
#endif

    //! Missing instruments catches
    std::set<uint8_t> caugh_missing_instruments;
    //! Missing melodic banks catches
    std::set<uint16_t> caugh_missing_banks_melodic;
    //! Missing percussion banks catches
    std::set<uint16_t> caugh_missing_banks_percussion;

#ifndef OPNMIDI_DISABLE_MIDI_SEQUENCER
    /**
     * @brief Build MIDI track data from the raw track data storage
     * @return true if everything successfully processed, or false on any error
     */
    bool buildTrackData();

    /**
     * @brief Parse one event from raw MIDI track stream
     * @param [_inout] ptr pointer to pointer to current position on the raw data track
     * @param [_in] end address to end of raw track data, needed to validate position and size
     * @param [_inout] status status of the track processing
     * @return Parsed MIDI event entry
     */
    MidiEvent parseEvent(uint8_t **ptr, uint8_t *end, int &status);
#endif

public:

    const std::string &getErrorString();
    void setErrorString(const std::string &err);

#ifndef OPNMIDI_DISABLE_MIDI_SEQUENCER
    std::string musTitle;
    std::string musCopyright;
    std::vector<std::string> musTrackTitles;
    std::vector<MIDI_MarkerEntry> musMarkers;

    fraction<uint64_t> InvDeltaTicks, Tempo;
    //! Tempo multiplier
    double  tempoMultiplier;
    bool    atEnd,
            loopStart,
            loopEnd,
            invalidLoop; /*Loop points are invalid (loopStart after loopEnd or loopStart and loopEnd are on same place)*/
    char ____padding2[2];
#endif
    OPN2 opn;

    int32_t outBuf[1024];

    Setup m_setup;

    static uint64_t ReadBEint(const void *buffer, size_t nbytes);
    static uint64_t ReadLEint(const void *buffer, size_t nbytes);

    /**
     * @brief Standard MIDI Variable-Length numeric value parser without of validation
     * @param [_inout] ptr Pointer to memory block that contains begin of variable-length value
     * @return Unsigned integer that conains parsed variable-length value
     */
    uint64_t ReadVarLen(uint8_t **ptr);
    /**
     * @brief Secure Standard MIDI Variable-Length numeric value parser with anti-out-of-range protection
     * @param [_inout] ptr Pointer to memory block that contains begin of variable-length value, will be iterated forward
     * @param [_in end Pointer to end of memory block where variable-length value is stored (after end of track)
     * @param [_out] ok Reference to boolean which takes result of variable-length value parsing
     * @return Unsigned integer that conains parsed variable-length value
     */
    uint64_t ReadVarLenEx(uint8_t **ptr, uint8_t *end, bool &ok);

    bool LoadBank(const std::string &filename);
    bool LoadBank(const void *data, size_t size);
    bool LoadBank(fileReader &fr);

#ifndef OPNMIDI_DISABLE_MIDI_SEQUENCER
    bool LoadMIDI(const std::string &filename);
    bool LoadMIDI(const void *data, size_t size);
    bool LoadMIDI(fileReader &fr);

    /**
     * @brief Periodic tick handler.
     * @param s seconds since last call
     * @param granularity don't expect intervals smaller than this, in seconds
     * @return desired number of seconds until next call
     */
    double Tick(double s, double granularity);
#endif

    /**
     * @brief Process extra iterators like vibrato or arpeggio
     * @param s seconds since last call
     */
    void   TickIteratos(double s);

#ifndef OPNMIDI_DISABLE_MIDI_SEQUENCER
    /**
     * @brief Change current position to specified time position in seconds
     * @param seconds Absolute time position in seconds
     */
    void    seek(double seconds);

    /**
     * @brief Gives current time position in seconds
     * @return Current time position in seconds
     */
    double  tell();

    /**
     * @brief Gives time length of current song in seconds
     * @return Time length of current song in seconds
     */
    double  timeLength();

    /**
     * @brief Gives loop start time position in seconds
     * @return Loop start time position in seconds or -1 if song has no loop points
     */
    double  getLoopStart();

    /**
     * @brief Gives loop end time position in seconds
     * @return Loop end time position in seconds or -1 if song has no loop points
     */
    double  getLoopEnd();

    /**
     * @brief Return to begin of current song
     */
    void    rewind();

    /**
     * @brief Set tempo multiplier
     * @param tempo Tempo multiplier: 1.0 - original tempo. >1 - faster, <1 - slower
     */
    void    setTempo(double tempo);
#endif

    /* RealTime event triggers */
    void realTime_ResetState();

    bool realTime_NoteOn(uint8_t channel, uint8_t note, uint8_t velocity);
    void realTime_NoteOff(uint8_t channel, uint8_t note);

    void realTime_NoteAfterTouch(uint8_t channel, uint8_t note, uint8_t atVal);
    void realTime_ChannelAfterTouch(uint8_t channel, uint8_t atVal);

    void realTime_Controller(uint8_t channel, uint8_t type, uint8_t value);

    void realTime_PatchChange(uint8_t channel, uint8_t patch);

    void realTime_PitchBend(uint8_t channel, uint16_t pitch);
    void realTime_PitchBend(uint8_t channel, uint8_t msb, uint8_t lsb);

    void realTime_BankChangeLSB(uint8_t channel, uint8_t lsb);
    void realTime_BankChangeMSB(uint8_t channel, uint8_t msb);
    void realTime_BankChange(uint8_t channel, uint16_t bank);

    void realTime_panic();

private:
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

    void NoteUpdate(uint16_t MidCh,
                    MIDIchannel::activenoteiterator i,
                    unsigned props_mask,
                    int32_t select_adlchn = -1);

#ifndef OPNMIDI_DISABLE_MIDI_SEQUENCER
    bool ProcessEventsNew(bool isSeek = false);
    void HandleEvent(size_t tk, const MidiEvent &evt, int &status);
#endif

    // Determine how good a candidate this adlchannel
    // would be for playing a note from this instrument.
    int64_t CalculateAdlChannelGoodness(size_t c, const MIDIchannel::NoteInfo::Phys &ins, uint16_t /*MidCh*/) const;

    // A new note will be played on this channel using this instrument.
    // Kill existing notes on this channel (or don't, if we do arpeggio)
    void PrepareAdlChannelForNewNote(size_t c, const MIDIchannel::NoteInfo::Phys &ins);

    void KillOrEvacuate(
        size_t  from_channel,
        OpnChannel::LocationData *j,
        MIDIchannel::activenoteiterator i);
    void Panic();
    void KillSustainingNotes(int32_t MidCh = -1, int32_t this_adlchn = -1);
    void SetRPN(unsigned MidCh, unsigned value, bool MSB);
    //void UpdatePortamento(unsigned MidCh)
    void NoteUpdate_All(uint16_t MidCh, unsigned props_mask);
    void NoteOff(uint16_t MidCh, uint8_t note);
    void UpdateVibrato(double amount);
    void UpdateArpeggio(double /*amount*/);

public:
    uint64_t ChooseDevice(const std::string &name);
};

extern int opn2RefreshNumCards(OPN2_MIDIPlayer *device);


#endif // ADLMIDI_PRIVATE_HPP
