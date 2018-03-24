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
#   elif defined (LIBADLMIDI_VISIBILITY)
#       define ADLMIDI_EXPORT __attribute__((visibility ("default")))
#   else
#       define ADLMIDI_EXPORT
#   endif
#endif

#ifdef _WIN32
#define NOMINMAX
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
#       define NOMINMAX //Don't override std::min and std::max
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
//#endif
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <vector> // vector
#include <deque>  // deque
#include <cmath>  // exp, log, ceil
#if defined(__WATCOMC__)
#include <math.h> // round, sqrt
#endif
#include <stdio.h>
#include <stdlib.h>
#include <limits> // numeric_limit

#ifndef _WIN32
#include <errno.h>
#endif

#include <deque>
#include <algorithm>

#ifdef _MSC_VER
#pragma warning(disable:4319)
#pragma warning(disable:4267)
#pragma warning(disable:4244)
#pragma warning(disable:4146)
#endif

#include "fraction.hpp"

#ifndef ADLMIDI_HW_OPL
#ifdef ADLMIDI_USE_DOSBOX_OPL
#include "dbopl.h"
#else
#include "nukedopl3.h"
#endif
#endif

#include "adldata.hh"
#include "adlmidi.h"    //Main API
#ifndef ADLMIDI_DISABLE_CPP_EXTRAS
#include "adlmidi.hpp"  //Extra C++ API
#endif

#define ADL_UNUSED(x) (void)x

extern std::string ADLMIDI_ErrorString;

/*
    Smart pointer for C heaps, created with malloc() call.
    FAQ: Why not std::shared_ptr? Because of Android NDK now doesn't supports it
*/
template<class PTR>
class AdlMIDI_CPtr
{
    PTR *m_p;
public:
    AdlMIDI_CPtr() : m_p(NULL) {}
    ~AdlMIDI_CPtr()
    {
        reset(NULL);
    }

    void reset(PTR *p = NULL)
    {
        if(m_p)
            free(m_p);
        m_p = p;
    }

    PTR *get()
    {
        return m_p;
    }
    PTR &operator*()
    {
        return *m_p;
    }
    PTR *operator->()
    {
        return m_p;
    }
};

class MIDIplay;
struct ADL_MIDIPlayer;
class OPL3
{
public:
    friend class MIDIplay;
    friend class AdlInstrumentTester;
    uint32_t NumChannels;
    char ____padding[4];
    ADL_MIDIPlayer *_parent;
#ifndef ADLMIDI_HW_OPL
#   ifdef ADLMIDI_USE_DOSBOX_OPL
    std::vector<DBOPL::Handler> cards;
#   else
    std::vector<_opl3_chip> cards;
#   endif
#endif
private:
    std::vector<size_t>     ins; // index to adl[], cached, needed by Touch()
    std::vector<uint8_t>    pit;  // value poked to B0, cached, needed by NoteOff)(
    std::vector<uint8_t>    regBD;

    friend int adlRefreshNumCards(ADL_MIDIPlayer *device);
    //! Meta information about every instrument
    std::vector<adlinsdata>     dynamic_metainstruments; // Replaces adlins[] when CMF file
    //! Raw instrument data ready to be sent to the chip
    std::vector<adldata>        dynamic_instruments;     // Replaces adl[]    when CMF file
    size_t                      dynamic_percussion_offset;

    typedef std::map<uint16_t, size_t> BankMap;
    BankMap dynamic_melodic_banks;
    BankMap dynamic_percussion_banks;
    const unsigned  DynamicInstrumentTag /* = 0x8000u*/,
                    DynamicMetaInstrumentTag /* = 0x4000000u*/;
    const adlinsdata    &GetAdlMetaIns(size_t n);
    size_t              GetAdlMetaNumber(size_t midiins);
    const adldata       &GetAdlIns(size_t insno);
public:
    void    setEmbeddedBank(unsigned int bank);

    //! Total number of running concurrent emulated chips
    unsigned int NumCards;
    //! Currently running embedded bank number. "~0" means usign of the custom bank.
    unsigned int AdlBank;
    //! Total number of needed four-operator channels in all running chips
    unsigned int NumFourOps;
    //! Turn global Deep Tremolo mode on
    bool HighTremoloMode;
    //! Turn global Deep Vibrato mode on
    bool HighVibratoMode;
    //! Use AdLib percussion mode
    bool AdlPercussionMode;
    //! Carriers-only are scaled by default by volume level. This flag will tell to scale modulators too.
    bool ScaleModulators;
    //! Required to play CMF files. Can be turned on by using of "CMF" volume model
    bool LogarithmicVolumes;
    // ! Required to play EA-MUS files [REPLACED WITH "m_musicMode", DEPRECATED!!!]
    //bool CartoonersVolumes;
    enum MusicMode
    {
        MODE_MIDI,
        MODE_IMF,
        MODE_CMF,
        MODE_RSXX
    } m_musicMode;
    //! Just a padding. Reserved.
    char ___padding2[3];
    //! Volume models enum
    enum VolumesScale
    {
        VOLUME_Generic,
        VOLUME_CMF,
        VOLUME_DMX,
        VOLUME_APOGEE,
        VOLUME_9X
    } m_volumeScale;

    OPL3();
    char ____padding3[8];
    std::vector<char> four_op_category; // 1 = quad-master, 2 = quad-slave, 0 = regular
    // 3 = percussion BassDrum
    // 4 = percussion Snare
    // 5 = percussion Tom
    // 6 = percussion Crash cymbal
    // 7 = percussion Hihat
    // 8 = percussion slave

    void Poke(size_t card, uint32_t index, uint32_t value);
    void PokeN(size_t card, uint16_t index, uint8_t value);

    void NoteOff(size_t c);
    void NoteOn(unsigned c, double hertz);
    void Touch_Real(unsigned c, unsigned volume, uint8_t brightness = 127);
    //void Touch(unsigned c, unsigned volume)

    void Patch(uint16_t c, size_t i);
    void Pan(unsigned c, unsigned value);
    void Silence();
    void updateFlags();
    void updateDeepFlags();
    void ChangeVolumeRangesModel(ADLMIDI_VolumeModels volumeModel);
    void Reset(unsigned long PCM_RATE);
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


class MIDIplay
{
    friend void adl_reset(struct ADL_MIDIPlayer*);
public:
    MIDIplay(unsigned long sampleRate = 22050);

    ~MIDIplay()
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
            #if !defined(_WIN32) || defined(__WATCOMC__)
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
                return std::feof(fp);
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
        uint8_t panning, vibrato, sustain;
        char ____padding[6];
        double  bend, bendsense;
        double  vibpos, vibspeed, vibdepth;
        int64_t vibdelay;
        uint8_t lastlrpn, lastmrpn;
        bool nrpn;
        uint8_t brightness;
        bool is_xg_percussion;
        struct NoteInfo
        {
            // Current pressure
            uint8_t vol;
            char ____padding[1];
            // Tone selected on noteon:
            int16_t tone;
            char ____padding2[4];
            // Patch selected on noteon; index to banks[AdlBank][]
            size_t  midiins;
            // Index to physical adlib data structure, adlins[]
            size_t  insmeta;
            struct Phys
            {
                //! ins, inde to adl[]
                size_t  insId;
                //! Is this voice must be detunable?
                bool    pseudo4op;

                bool operator==(const Phys &oth) const
                {
                    return (insId == oth.insId) && (pseudo4op == oth.pseudo4op);
                }
                bool operator!=(const Phys &oth) const
                {
                    return !operator==(oth);
                }
            };
            typedef std::map<uint16_t, Phys> PhysMap;
            // List of OPL3 channels it is currently occupying.
            std::map<uint16_t /*adlchn*/, Phys> phys;
        };
        typedef std::map<uint8_t, NoteInfo> activenotemap_t;
        typedef activenotemap_t::iterator activenoteiterator;
        char ____padding2[5];
        activenotemap_t activenotes;
        void reset()
        {
            portamento = 0;
            bank_lsb = 0;
            bank_msb = 0;
            patch = 0;
            volume  = 100;
            expression = 127;
            panning = 0x30;
            vibrato = 0;
            sustain = 0;
            bend = 0.0;
            bendsense = 2 / 8192.0;
            vibpos = 0;
            vibspeed = 2 * 3.141592653 * 5.0;
            vibdepth = 0.5 / 127;
            vibdelay = 0;
            lastlrpn = 0;
            lastmrpn = 0;
            nrpn = false;
            brightness = 127;
            is_xg_percussion = false;
        }
        MIDIchannel()
            : activenotes()
        {
            reset();
        }
    };

    // Additional information about OPL3 channels
    struct AdlChannel
    {
        // For collisions
        struct Location
        {
            uint16_t    MidCh;
            uint8_t     note;
            bool operator==(const Location &b) const
            {
                return MidCh == b.MidCh && note == b.note;
            }
            bool operator< (const Location &b) const
            {
                return MidCh < b.MidCh || (MidCh == b.MidCh && note < b.note);
            }
            char ____padding[1];
        };
        struct LocationData
        {
            bool sustained;
            char ____padding[7];
            MIDIchannel::NoteInfo::Phys ins;  // a copy of that in phys[]
            int64_t kon_time_until_neglible;
            int64_t vibdelay;
        };
        typedef std::map<Location, LocationData> users_t;
        users_t users;

        // If the channel is keyoff'd
        int64_t koff_time_until_neglible;
        // For channel allocation:
        AdlChannel(): users(), koff_time_until_neglible(0) { }
        void AddAge(int64_t ms);
    };

#ifndef ADLMIDI_DISABLE_MIDI_SEQUENCER
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
#endif//ADLMIDI_DISABLE_MIDI_SEQUENCER

    struct Setup
    {
        unsigned int AdlBank;
        unsigned int NumFourOps;
        unsigned int NumCards;
        int     HighTremoloMode;
        int     HighVibratoMode;
        int     AdlPercussionMode;
        bool    LogarithmicVolumes;
        int     VolumeModel;
        //unsigned int SkipForward;
        bool    loopingIsEnabled;
        int     ScaleModulators;

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
    bool cmf_percussion_mode;

    MIDIEventHooks hooks;

private:
    std::map<std::string, uint64_t> devices;
    std::map<uint64_t /*track*/, uint64_t /*channel begin index*/> current_device;

    //Padding to fix CLanc code model's warning
    char ____padding[7];

    std::vector<AdlChannel> ch;

#ifndef ADLMIDI_DISABLE_MIDI_SEQUENCER
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

#ifndef ADLMIDI_DISABLE_MIDI_SEQUENCER
    //! Pre-processed track data storage
    std::vector<MidiTrackQueue > trackDataNew;
#endif

    //! Missing instruments catches
    std::set<uint8_t> caugh_missing_instruments;
    //! Missing melodic banks catches
    std::set<uint16_t> caugh_missing_banks_melodic;
    //! Missing percussion banks catches
    std::set<uint16_t> caugh_missing_banks_percussion;

#ifndef ADLMIDI_DISABLE_MIDI_SEQUENCER
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
#endif//ADLMIDI_DISABLE_MIDI_SEQUENCER

public:

    const std::string &getErrorString();
    void setErrorString(const std::string &err);

#ifndef ADLMIDI_DISABLE_MIDI_SEQUENCER
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
    OPL3 opl;

    int16_t outBuf[1024];

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

#ifndef ADLMIDI_DISABLE_MIDI_SEQUENCER
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

#ifndef ADLMIDI_DISABLE_MIDI_SEQUENCER
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
#endif//ADLMIDI_DISABLE_MIDI_SEQUENCER

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
        Upd_Off    = 0x20
    };

    void NoteUpdate(uint16_t MidCh,
                    MIDIchannel::activenoteiterator i,
                    unsigned props_mask,
                    int32_t select_adlchn = -1);

#ifndef ADLMIDI_DISABLE_MIDI_SEQUENCER
    bool ProcessEventsNew(bool isSeek = false);
    void HandleEvent(size_t tk, const MidiEvent &evt, int &status);
#endif

    // Determine how good a candidate this adlchannel
    // would be for playing a note from this instrument.
    int64_t CalculateAdlChannelGoodness(unsigned c, const MIDIchannel::NoteInfo::Phys &ins, uint16_t /*MidCh*/) const;

    // A new note will be played on this channel using this instrument.
    // Kill existing notes on this channel (or don't, if we do arpeggio)
    void PrepareAdlChannelForNewNote(size_t c, const MIDIchannel::NoteInfo::Phys &ins);

    void KillOrEvacuate(
        size_t  from_channel,
        AdlChannel::users_t::iterator j,
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

extern int adlRefreshNumCards(ADL_MIDIPlayer *device);


#endif // ADLMIDI_PRIVATE_HPP
