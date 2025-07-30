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

#ifndef ADLMIDI_OPL3_HPP
#define ADLMIDI_OPL3_HPP

#include "oplinst.h"
#include "adlmidi_ptr.hpp"
#include "adlmidi_private.hpp"
#include "adlmidi_bankmap.h"

#define BEND_COEFFICIENT                172.4387

#define OPL3_CHANNELS_MELODIC_BASE      0
#define OPL3_CHANNELS_RHYTHM_BASE       18

#define NUM_OF_CHANNELS                 23
#define NUM_OF_4OP_CHANNELS             6
#define NUM_OF_2OP_CHANNELS             18
#define NUM_OF_2x2_CHANNELS             9
#define NUM_OF_OPL2_CHANNELS            9
#define NUM_OF_RM_CHANNELS              5

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
    //! Running chip emulators
    std::vector<AdlMIDI_SPtr<OPLChipBase > > m_chips;

private:
    //! Cached patch data, needed by Touch()
    std::vector<OplTimbre>    m_insCache;
    //! Value written to B0, cached, needed by NoteOff.
    /*! Contains Key on/off state, octave block and frequency number values
     */
    std::vector<uint32_t>   m_keyBlockFNumCache;
    //! Cached BD registry value (flags register: DeepTremolo, DeepVibrato, and RhythmMode)
    std::vector<uint32_t>   m_regBD;
    //! Cached C0 register value (primarily for the panning state)
    std::vector<uint8_t>    m_regC0;

#ifdef ADLMIDI_ENABLE_HW_SERIAL
    bool        m_serial;
    std::string m_serialName;
    unsigned    m_serialBaud;
    unsigned    m_serialProtocol;
#endif
    //! Does loaded emulator supports soft panning?
    bool m_softPanningSup;
    //! Current type of chip
    int  m_currentChipType;
    //! Number channels per chip
    size_t m_perChipChannels;

    /*!
     * \brief Current state of the synth (if values matched to setup, chips and arrays won't be fully re-created)
     */
    struct State
    {
        int         emulator;
        uint32_t    numChips;
        unsigned long pcm_rate;

        State()
        {
            clear();
        }

        void clear()
        {
            emulator = -2;
            numChips = 0;
            pcm_rate = 0;
        }

        bool cmp_rate(unsigned long rate)
        {
            bool ret = pcm_rate != rate;

            if(ret)
                pcm_rate = rate;

            return ret;
        }

        bool cmp(int emu, uint32_t chips)
        {
            bool ret = emu != emulator || chips != numChips;

            if(ret)
            {
                emulator = emu;
                numChips = chips;
            }

            return ret;
        }
    } m_curState;

public:
    /**
     * @brief MIDI bank entry
     */
    struct Bank
    {
        //! MIDI Bank instruments
        OplInstMeta ins[128];
    };
    typedef BasicBankMap<Bank> BankMap;
    //! MIDI bank instruments data
    BankMap         m_insBanks;
    //! MIDI bank-wide setup
    OplBankSetup    m_insBankSetup;

public:
    //! Blank instrument template
    static const OplInstMeta m_emptyInstrument;
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
    //! Master volume, controlled via SysEx (0...127)
    uint8_t m_masterVolume;

    //! Just a padding. Reserved.
    char _padding2[3];

    /**
     * @brief Music playing mode
     */
    enum MusicMode
    {
        //! MIDI mode
        MODE_MIDI,
        //! AIL XMIDI mode
        MODE_XMIDI,
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
        //! Windows 9x SB16 driver volume scale table
        VOLUME_9X,
        //! DMX model with a fixed bug of AM voices
        VOLUME_DMX_FIXED,
        //! Apogee model with a fixed bug of AM voices
        VOLUME_APOGEE_FIXED,
        //! Audio Interfaces Library volume scaling model
        VOLUME_AIL,
        //! Windows 9x Generic FM driver volume scale table
        VOLUME_9X_GENERIC_FM,
        //! HMI Sound Operating System volume scale table
        VOLUME_HMI,
        //! HMI Sound Operating System volume scale model, older variant
        VOLUME_HMI_OLD
    } m_volumeScale;

    //! Channel allocation algorithm
    ADLMIDI_ChannelAlloc m_channelAlloc;

    //! Reserved
    char _padding3[8];

    /**
     * @brief Channel categiry enumeration
     */
    enum ChanCat
    {
        //! Regular melodic/percussion channel
        ChanCat_Regular     = 0,
        //! Four-op first part
        ChanCat_4op_First  = 1,
        //! Four-op second part
        ChanCat_4op_Second   = 2,
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
        //! Rhythm-mode Secondary channel
        ChanCat_Rhythm_Secondary    = 8,
        //! Here is no channel used (OPL2 only)
        ChanCat_None = 9
    };

    //! Category of the channel
    /*! 1 = quad-first, 2 = quad-second, 0 = regular
        3 = percussion BassDrum
        4 = percussion Snare
        5 = percussion Tom
        6 = percussion Crash cymbal
        7 = percussion Hihat
        8 = percussion Secondary
    */
    std::vector<uint32_t> m_channelCategory;


    /**
     * @brief C.O. Constructor
     */
    OPL3();

    /**
     * @brief C.O. Destructor
     */
    ~OPL3();

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
     * @param c2 Second 4-op channel of chip, unused for 2op (Emulated chip choosing by next formula: [c = ch + (chipId * 23)])
     * @param tone The tone to play (integer part - MIDI halftone, decimal part - relative bend offset)
     */
    void noteOn(size_t c1, size_t c2, double tone);

    /**
     * @brief Change setup of instrument in specified chip channel
     * @param c Channel of chip (Emulated chip choosing by next formula: [c = ch + (chipId * 23)])
     * @param velocity Note velocity (from 0 to 127)
     * @param channelVolume Channel volume level (from 0 to 127)
     * @param channelExpression Channel expression level (from 0 to 127)
     * @param brightness CC74 Brightness level (from 0 to 127)
     * @param isDrum Is this a drum note? This flag is needed for some volume model algorithms
     */
    void touchNote(size_t c,
                   uint_fast32_t velocity,
                   uint_fast32_t channelVolume = 127,
                   uint_fast32_t channelExpression = 127,
                   uint_fast32_t brightness = 127,
                   bool isDrum = false);

    /**
     * @brief Set the instrument into specified chip channel
     * @param c Channel of chip (Emulated chip choosing by next formula: [c = ch + (chipId * 23)])
     * @param instrument Instrument data to set into the chip channel
     */
    void setPatch(size_t c, const OplTimbre &instrument);

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

    /**
     * @brief Clean up all running emulated chip instances
     */
    void clearChips();

    /**
     * @brief Reset chip properties and initialize them
     * @param emulator Type of chip emulator
     * @param PCM_RATE Output sample rate to generate on output
     * @param audioTickHandler PCM-accurate clock hook
     */
    void reset(int emulator, unsigned long PCM_RATE, void *audioTickHandler);

    void initChip(size_t chip);

#ifdef ADLMIDI_ENABLE_HW_SERIAL
    /**
     * @brief Reset chip properties for hardware use
     * @param emulator
     * @param PCM_RATE
     * @param audioTickHandler
     */
    void resetSerial(const std::string &serialName, unsigned int baud, unsigned int protocol);
#endif
};

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

#endif // ADLMIDI_OPL3_HPP
