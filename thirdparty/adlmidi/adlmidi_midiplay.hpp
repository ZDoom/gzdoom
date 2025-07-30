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

#ifndef ADLMIDI_MIDIPLAY_HPP
#define ADLMIDI_MIDIPLAY_HPP

#include "oplinst.h"
#include "adlmidi_private.hpp"
#include "adlmidi_ptr.hpp"
#include "structures/pl_list.hpp"

/**
 * @brief Hooks of the internal events
 */
struct MIDIEventHooks
{
    MIDIEventHooks() :
        onNote(NULL),
        onNote_userData(NULL),
        onLoopStart(NULL),
        onLoopStart_userData(NULL),
        onLoopEnd(NULL),
        onLoopEnd_userData(NULL),
        onDebugMessage(NULL),
        onDebugMessage_userData(NULL)
    {}

    //! Note on/off hooks
    typedef void (*NoteHook)(void *userdata, int adlchn, int note, int ins, int pressure, double bend);
    NoteHook     onNote;
    void         *onNote_userData;

    // Loop start/end hooks
    ADL_LoopPointHook onLoopStart;
    void              *onLoopStart_userData;
    ADL_LoopPointHook onLoopEnd;
    void              *onLoopEnd_userData;


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
    ~MIDIplay();

    void applySetup();

    void partialReset();
    void resetMIDI();

private:
    void chipReset();
    void resetMIDIDefaults(int offset = 0);

public:
    /**********************Internal structures and classes**********************/

    /**
     * @brief Persistent settings for each MIDI channel
     */
    struct MIDIchannel
    {
        //! Default MIDI volume
        uint8_t def_volume;
        //! Default LSB of a bend sensitivity
        int     def_bendsense_lsb;
        //! Default MSB of a bend sensitivity
        int     def_bendsense_msb;

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
            //! Whether releasing and on extended life time defined by TTL
            bool    isOnExtendedLifeTime;
            //! Time-to-live until release (short percussion note fix)
            double  ttl;
            //! Patch selected
            const OplInstMeta *ains;
            enum
            {
                MaxNumPhysChans = 2,
                MaxNumPhysItemCount = MaxNumPhysChans
            };

            struct FindPredicate
            {
                explicit FindPredicate(unsigned note)
                    : note(note) {}
                bool operator()(const NoteInfo &ni) const
                    { return ni.note == note; }
                unsigned note;
            };

            /**
             * @brief Reference to currently using chip channel
             */
            struct Phys
            {
                //! Destination chip channel
                uint16_t chip_chan;
                //! ins, inde to adl[]
                OplTimbre op;
                //! Is this voice must be detunable?
                bool    pseudo4op;

                void assign(const Phys &oth)
                {
                    op = oth.op;
                    pseudo4op = oth.pseudo4op;
                }
                bool operator==(const Phys &oth) const
                {
                    return (op == oth.op) && (pseudo4op == oth.pseudo4op);
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
                {
                    if(chip_channels[i].chip_chan == chip_chan)
                        ph = &chip_channels[i];
                }

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
                assert(pos >= 0 && pos < static_cast<intptr_t>(chip_channels_count));
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
        //! Count of notes having a TTL countdown in this channel
        unsigned extended_note_count;

        //! Active notes in the channel
        pl_list<NoteInfo> activenotes;
        typedef pl_list<NoteInfo>::iterator notes_iterator;
        typedef pl_list<NoteInfo>::const_iterator const_notes_iterator;

        void clear_all_phys_users(unsigned chip_chan)
        {
            for(pl_list<NoteInfo>::iterator it = activenotes.begin(); it != activenotes.end(); )
            {
                NoteInfo::Phys *p = it->value.phys_find(chip_chan);
                if(p)
                {
                    it->value.phys_erase_at(p);
                    if(it->value.chip_channels_count == 0)
                        it = activenotes.erase(it);
                    else
                        ++it;
                }
                else
                    ++it;
            }
        }

        notes_iterator find_activenote(unsigned note)
        {
            return activenotes.find_if(NoteInfo::FindPredicate(note));
        }

        notes_iterator ensure_find_activenote(unsigned note)
        {
            notes_iterator it = find_activenote(note);
            assert(!it.is_end());
            return it;
        }

        notes_iterator find_or_create_activenote(unsigned note)
        {
            notes_iterator it = find_activenote(note);
            if(!it.is_end())
                cleanupNote(it);
            else
            {
                NoteInfo ni;
                ni.note = note;
                it = activenotes.insert(activenotes.end(), ni);
            }
            return it;
        }

        notes_iterator create_activenote(unsigned note)
        {
            NoteInfo ni;
            ni.note = note;
            notes_iterator it = activenotes.insert(activenotes.end(), ni);
            return it;
        }

        notes_iterator ensure_find_or_create_activenote(unsigned note)
        {
            notes_iterator it = find_or_create_activenote(note);
            assert(!it.is_end());
            return it;
        }

        notes_iterator ensure_create_activenote(unsigned note)
        {
            notes_iterator it = create_activenote(note);
            assert(!it.is_end());
            return it;
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
            volume  = def_volume;
            brightness = 127;
            panning = 64;

            resetAllControllers121();
        }

        /**
         * @brief Reset all MIDI controllers into initial state (CC121)
         */
        void resetAllControllers121()
        {
            bend = 0;
            bendsense_msb = def_bendsense_msb;
            bendsense_lsb = def_bendsense_lsb;
            updateBendSensitivity();
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
            portamento = 0;
            portamentoEnable = false;
            portamentoSource = -1;
            portamentoRate = HUGE_VAL;
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

        /**
         * @brief Clean up the state of the active note before removal
         */
        void cleanupNote(notes_iterator i)
        {
            NoteInfo &info = i->value;
            if(info.glideRate != HUGE_VAL)
                --gliding_note_count;
            if(info.ttl > 0)
                --extended_note_count;
        }

        MIDIchannel() :
            def_volume(100),
            def_bendsense_lsb(0),
            def_bendsense_msb(2),
            activenotes(128)
        {
            gliding_note_count = 0;
            extended_note_count = 0;
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

            struct FindPredicate
            {
                explicit FindPredicate(Location loc)
                    : loc(loc) {}
                bool operator()(const LocationData &ld) const
                    { return ld.loc == loc; }
                Location loc;
            };
        };

        //! Time left until sounding will be muted after key off
        int64_t koff_time_until_neglible_us;

        //! Recently passed instrument, improves a goodness of released but busy channel when matching
        MIDIchannel::NoteInfo::Phys recent_ins;

        pl_list<LocationData> users;
        typedef pl_list<LocationData>::iterator users_iterator;
        typedef pl_list<LocationData>::const_iterator const_users_iterator;

        users_iterator find_user(const Location &loc)
        {
            return users.find_if(LocationData::FindPredicate(loc));
        }

        users_iterator find_or_create_user(const Location &loc)
        {
            users_iterator it = find_user(loc);
            if(it.is_end() && users.size() != users.capacity())
            {
                LocationData ld;
                ld.loc = loc;
                it = users.insert(users.end(), ld);
            }
            return it;
        }

        // For channel allocation:
        AdlChannel(): koff_time_until_neglible_us(0), users(128)
        {
            std::memset(&recent_ins, 0, sizeof(MIDIchannel::NoteInfo::Phys));
        }

        AdlChannel(const AdlChannel &oth): koff_time_until_neglible_us(oth.koff_time_until_neglible_us), users(oth.users)
        {
        }

        AdlChannel &operator=(const AdlChannel &oth)
        {
            koff_time_until_neglible_us = oth.koff_time_until_neglible_us;
            users = oth.users;
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
    AdlMIDI_UPtr<MidiSequencer> m_sequencer;

    /**
     * @brief Interface between MIDI sequencer and this library
     */
    AdlMIDI_UPtr<BW_MidiRtInterface> m_sequencerInterface;

    /**
     * @brief Initialize MIDI sequencer interface
     */
    void initSequencerInterface();
#endif //ADLMIDI_DISABLE_MIDI_SEQUENCER

    struct Setup
    {
        int          emulator;
        bool         runAtPcmRate;

#ifdef ADLMIDI_ENABLE_HW_SERIAL
        bool         serial;
        std::string  serialName;
        unsigned int serialBaud;
        unsigned int serialProtocol;
#endif

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
        bool    enableAutoArpeggio;

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
    AdlMIDI_UPtr<Synth> m_synth;

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
                    MIDIchannel::notes_iterator i,
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
     * @brief If no free chip channels, try to kill at least one second voice of pseudo-4-op instruments and steal the released channel
     * @param new_chan Value of released chip channel to reuse
     * @return true if any channel was been stolen, or false when nothing happen
     */
    bool killSecondVoicesIfOverflow(int32_t &new_chan);

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
        AdlChannel::users_iterator j,
        MIDIchannel::notes_iterator i);

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
     * @param forceNow Do not delay the key-off to a later time
     */
    void noteOff(size_t midCh, uint8_t note, bool forceNow = false);

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

#endif //  ADLMIDI_MIDIPLAY_HPP
