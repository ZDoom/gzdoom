/*
 * libOPNMIDI is a free Software MIDI synthesizer library with OPN2 (YM2612) emulation
 *
 * MIDI parser and player (Original code from ADLMIDI): Copyright (c) 2010-2014 Joel Yliluoma <bisqwit@iki.fi>
 * OPNMIDI Library and YM2612 support:   Copyright (c) 2017-2025 Vitaly Novichkov <admin@wohlnet.ru>
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

#ifndef OPNMIDI_OPN2_HPP
#define OPNMIDI_OPN2_HPP

#include "opnbank.h"
#include "opnmidi_ptr.hpp"
#include "opnmidi_private.hpp"
#include "opnmidi_bankmap.h"
#include "chips/opn_chip_family.h"

/**
 * @brief OPN2 Chip management class
 */
class OPN2
{
    friend class OPNMIDIplay;
public:
    enum { PercussionTag = 1 << 15 };

    //! Total number of chip channels between all running emulators
    uint32_t m_numChannels;
    //! Just a padding. Reserved.
    char _padding[4];
    //! Running chip emulators
    std::vector<AdlMIDI_SPtr<OPNChipBase > > m_chips;
#ifdef OPNMIDI_MIDI2VGM
    //! Loop Start hook
    void (*m_loopStartHook)(void*);
    //! Loop Start hook data
    void *m_loopStartHookData;
    //! Loop End hook
    void (*m_loopEndHook)(void*);
    //! Loop End hook data
    void *m_loopEndHookData;
#endif
private:
    //! Cached patch data, needed by Touch()
    std::vector<OpnTimbre>    m_insCache;
    //! Cached per-channel LFO sensitivity flags
    std::vector<uint8_t>        m_regLFOSens;
    //! LFO setup registry cache
    uint8_t                     m_regLFOSetup;

public:
    /**
     * @brief MIDI bank entry
     */
    struct Bank
    {
        //! MIDI Bank instruments
        OpnInstMeta ins[128];
    };
    typedef BasicBankMap<Bank> BankMap;
    //! MIDI bank instruments data
    BankMap         m_insBanks;
    //! MIDI bank-wide setup
    OpnBankSetup    m_insBankSetup;

public:
    //! Blank instrument template
    static const OpnInstMeta m_emptyInstrument;

    //! Total number of running concurrent emulated chips
    uint32_t m_numChips;
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
        //! MIDI mode
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
        //! OPN2 native logarithmic scale
        VOLUME_NATIVE,
        //! DMX volume scale logarithmic table
        VOLUME_DMX,
        //! Apoge Sound System volume scaling model
        VOLUME_APOGEE,
        //! Windows 9x driver volume scale table
        VOLUME_9X
    } m_volumeScale;

    //! Channel allocation algorithm
    OPNMIDI_ChannelAlloc m_channelAlloc;

    //! Reserved
    bool m_lfoEnable;
    uint8_t m_lfoFrequency;

    //! Category of the channel
    /*! 1 = DAC, 0 = regular
    */
    std::vector<char> m_channelCategory;

    //! Chip family
    OPNFamily m_chipFamily;

    /**
     * @brief C.O. Constructor
     */
    OPN2();

    /**
     * @brief C.O. Destructor
     */
    ~OPN2();

    /**
     * @brief Checks are setup locked to be changed on the fly or not
     * @return true when setup on the fly is locked
     */
    bool setupLocked();

    /**
     * @brief Write data to OPN2 chip register
     * @param chip Index of emulated chip. In hardware OPN2 builds, this parameter is ignored
     * @param port Port of the chip to write
     * @param index Register address to write
     * @param value Value to write
     */
    void writeReg(size_t chip, uint8_t port, uint8_t index, uint8_t value);

    /**
     * @brief Write data to OPN2 chip register
     * @param chip Index of emulated chip. In hardware OPN2 builds, this parameter is ignored
     * @param port Port of the chip to write
     * @param index Register address to write
     * @param value Value to write
     */
    void writeRegI(size_t chip, uint8_t port, uint32_t index, uint32_t value);

    /**
     * @brief Write to soft panning control of OPN2 chip emulator
     * @param chip Index of emulated chip.
     * @param address Register of channel to write
     * @param value Value to write
     */
    void writePan(size_t chip, uint32_t index, uint32_t value);

    /**
     * @brief Off the note in specified chip channel
     * @param c Channel of chip (Emulated chip choosing by next formula: [c = ch + (chipId * 23)])
     */
    void noteOff(size_t c);

    /**
     * @brief On the note in specified chip channel with specified frequency of the tone
     * @param c Channel of chip (Emulated chip choosing by next formula: [c = ch + (chipId * 23)])
     * @param tone The tone to play (integer part - MIDI halftone, decimal part - relative bend offset)
     */
    void noteOn(size_t c, double tone);

    /**
     * @brief Change setup of instrument in specified chip channel
     * @param c Channel of chip (Emulated chip choosing by next formula: [c = ch + (chipId * 23)])
     * @param volume Volume level (from 0 to 127)
     * @param brightness CC74 Brightness level (from 0 to 127)
     */
    void touchNote(size_t c,
                   uint_fast32_t velocity,
                   uint_fast32_t channelVolume = 127,
                   uint_fast32_t channelExpression = 127,
                   uint8_t brightness = 127);

    /**
     * @brief Set the instrument into specified chip channel
     * @param c Channel of chip (Emulated chip choosing by next formula: [c = ch + (chipId * 23)])
     * @param instrument Instrument data to set into the chip channel
     */
    void setPatch(size_t c, const OpnTimbre &instrument);

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
     * @brief commit LFO enable and frequency
     */
    void commitLFOSetup();

    /**
     * @brief Set the volume scaling model
     * @param volumeModel Type of volume scale model scale
     */
    void setVolumeScaleModel(OPNMIDI_VolumeModels volumeModel);

    /**
     * @brief Get the volume scaling model
     */
    OPNMIDI_VolumeModels getVolumeScaleModel();

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
    void reset(int emulator, unsigned long PCM_RATE, OPNFamily family, void *audioTickHandler);

    /**
     * @brief Gets the family of current chips
     * @return the chip family
     */
    OPNFamily chipFamily() const;
};

/**
 * @brief Check emulator availability
 * @param emulator Emulator ID (Opn2_Emulator)
 * @return true when emulator is available
 */
extern bool opn2_isEmulatorAvailable(int emulator);

/**
 * @brief Find highest emulator
 * @return The Opn2_Emulator enum value which contains ID of highest emulator
 */
extern int opn2_getHighestEmulator();

/**
 * @brief Find lowest emulator
 * @return The Opn2_Emulator enum value which contains ID of lowest emulator
 */
extern int opn2_getLowestEmulator();

#endif // OPNMIDI_OPN2_HPP
