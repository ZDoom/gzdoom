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

#include "adlmidi_private.hpp"

// Mapping from MIDI volume level to OPL level value.

static const uint_fast32_t DMX_volume_mapping_table[128] =
{
    0,  1,  3,  5,  6,  8,  10, 11,
    13, 14, 16, 17, 19, 20, 22, 23,
    25, 26, 27, 29, 30, 32, 33, 34,
    36, 37, 39, 41, 43, 45, 47, 49,
    50, 52, 54, 55, 57, 59, 60, 61,
    63, 64, 66, 67, 68, 69, 71, 72,
    73, 74, 75, 76, 77, 79, 80, 81,
    82, 83, 84, 84, 85, 86, 87, 88,
    89, 90, 91, 92, 92, 93, 94, 95,
    96, 96, 97, 98, 99, 99, 100, 101,
    101, 102, 103, 103, 104, 105, 105, 106,
    107, 107, 108, 109, 109, 110, 110, 111,
    112, 112, 113, 113, 114, 114, 115, 115,
    116, 117, 117, 118, 118, 119, 119, 120,
    120, 121, 121, 122, 122, 123, 123, 123,
    124, 124, 125, 125, 126, 126, 127, 127,
};

static const uint_fast32_t W9X_volume_mapping_table[32] =
{
    63, 63, 40, 36, 32, 28, 23, 21,
    19, 17, 15, 14, 13, 12, 11, 10,
    9,  8,  7,  6,  5,  5,  4,  4,
    3,  3,  2,  2,  1,  1,  0,  0
};


//static const char MIDIsymbols[256+1] =
//"PPPPPPhcckmvmxbd"  // Ins  0-15
//"oooooahoGGGGGGGG"  // Ins 16-31
//"BBBBBBBBVVVVVHHM"  // Ins 32-47
//"SSSSOOOcTTTTTTTT"  // Ins 48-63
//"XXXXTTTFFFFFFFFF"  // Ins 64-79
//"LLLLLLLLpppppppp"  // Ins 80-95
//"XXXXXXXXGGGGGTSS"  // Ins 96-111
//"bbbbMMMcGXXXXXXX"  // Ins 112-127
//"????????????????"  // Prc 0-15
//"????????????????"  // Prc 16-31
//"???DDshMhhhCCCbM"  // Prc 32-47
//"CBDMMDDDMMDDDDDD"  // Prc 48-63
//"DDDDDDDDDDDDDDDD"  // Prc 64-79
//"DD??????????????"  // Prc 80-95
//"????????????????"  // Prc 96-111
//"????????????????"; // Prc 112-127

static const uint8_t PercussionMap[256] =
    "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"//GM
    "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0" // 3 = bass drum
    "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0" // 4 = snare
    "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0" // 5 = tom
    "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0" // 6 = cymbal
    "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0" // 7 = hihat
    "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
    "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
    "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"//GP0
    "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"//GP16
    //2 3 4 5 6 7 8 940 1 2 3 4 5 6 7
    "\0\0\0\3\3\0\0\7\0\5\7\5\0\5\7\5"//GP32
    //8 950 1 2 3 4 5 6 7 8 960 1 2 3
    "\5\6\5\0\6\0\5\6\0\6\0\6\5\5\5\5"//GP48
    //4 5 6 7 8 970 1 2 3 4 5 6 7 8 9
    "\5\0\0\0\0\0\7\0\0\0\0\0\0\0\0\0"//GP64
    "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
    "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
    "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0";

enum { MasterVolumeDefault = 127 };

inline bool isXgPercChannel(uint8_t msb, uint8_t lsb)
{
    return (msb == 0x7E || msb == 0x7F) && (lsb == 0);
}

void MIDIplay::AdlChannel::addAge(int64_t us)
{
    const int64_t neg = 1000 * static_cast<int64_t>(-0x1FFFFFFFll);
    if(users_empty())
    {
        koff_time_until_neglible_us = std::max(koff_time_until_neglible_us - us, neg);
        if(koff_time_until_neglible_us < 0)
            koff_time_until_neglible_us = 0;
    }
    else
    {
        koff_time_until_neglible_us = 0;
        for(LocationData *i = users_first; i; i = i->next)
        {
            if(!i->fixed_sustain)
                i->kon_time_until_neglible_us = std::max(i->kon_time_until_neglible_us - us, neg);
            i->vibdelay_us += us;
        }
    }
}

MIDIplay::MIDIplay(unsigned long sampleRate):
    m_cmfPercussionMode(false),
    m_masterVolume(MasterVolumeDefault),
    m_sysExDeviceId(0),
    m_synthMode(Mode_XG),
    m_arpeggioCounter(0)
#if defined(ADLMIDI_AUDIO_TICK_HANDLER)
    , m_audioTickCounter(0)
#endif
{
    m_midiDevices.clear();

    m_setup.emulator = adl_getLowestEmulator();
    m_setup.runAtPcmRate = false;

    m_setup.PCM_RATE   = sampleRate;
    m_setup.mindelay = 1.0 / (double)m_setup.PCM_RATE;
    m_setup.maxdelay = 512.0 / (double)m_setup.PCM_RATE;

    m_setup.bankId    = 0;
    m_setup.numFourOps = -1;
    m_setup.numChips   = 2;
    m_setup.deepTremoloMode     = -1;
    m_setup.deepVibratoMode     = -1;
    m_setup.rhythmMode   = -1;
    m_setup.logarithmicVolumes  = false;
    m_setup.volumeScaleModel = ADLMIDI_VolumeModel_AUTO;
    //m_setup.SkipForward = 0;
    m_setup.scaleModulators     = -1;
    m_setup.fullRangeBrightnessCC74 = false;
    m_setup.delay = 0.0;
    m_setup.carry = 0.0;
    m_setup.tick_skip_samples_delay = 0;

#ifndef ADLMIDI_DISABLE_MIDI_SEQUENCER
    initSequencerInterface();
#endif
    resetMIDI();
    applySetup();
    realTime_ResetState();
}

void MIDIplay::applySetup()
{
    m_synth.m_musicMode = OPL3::MODE_MIDI;

    m_setup.tick_skip_samples_delay = 0;

    m_synth.m_runAtPcmRate = m_setup.runAtPcmRate;

#ifndef DISABLE_EMBEDDED_BANKS
    if(m_synth.m_embeddedBank != OPL3::CustomBankTag)
        m_synth.m_insBankSetup = adlbanksetup[m_setup.bankId];
#endif

    m_synth.m_deepTremoloMode     = m_setup.deepTremoloMode < 0 ?
                              m_synth.m_insBankSetup.deepTremolo :
                              (m_setup.deepTremoloMode != 0);
    m_synth.m_deepVibratoMode     = m_setup.deepVibratoMode < 0 ?
                              m_synth.m_insBankSetup.deepVibrato :
                              (m_setup.deepVibratoMode != 0);
    m_synth.m_rhythmMode   = m_setup.rhythmMode < 0 ?
                              m_synth.m_insBankSetup.adLibPercussions :
                              (m_setup.rhythmMode != 0);
    m_synth.m_scaleModulators     = m_setup.scaleModulators < 0 ?
                              m_synth.m_insBankSetup.scaleModulators :
                              (m_setup.scaleModulators != 0);

    if(m_setup.logarithmicVolumes)
        m_synth.setVolumeScaleModel(ADLMIDI_VolumeModel_NativeOPL3);
    else
        m_synth.setVolumeScaleModel(static_cast<ADLMIDI_VolumeModels>(m_setup.volumeScaleModel));

    if(m_setup.volumeScaleModel == ADLMIDI_VolumeModel_AUTO)//Use bank default volume model
        m_synth.m_volumeScale = (OPL3::VolumesScale)m_synth.m_insBankSetup.volumeModel;

    m_synth.m_numChips    = m_setup.numChips;
    m_cmfPercussionMode = false;

    if(m_setup.numFourOps >= 0)
        m_synth.m_numFourOps  = m_setup.numFourOps;
    else
        adlCalculateFourOpChannels(this, true);

    m_synth.reset(m_setup.emulator, m_setup.PCM_RATE, this);
    m_chipChannels.clear();
    m_chipChannels.resize(m_synth.m_numChannels);

    // Reset the arpeggio counter
    m_arpeggioCounter = 0;
}

void MIDIplay::partialReset()
{
    realTime_panic();
    m_setup.tick_skip_samples_delay = 0;
    m_synth.m_runAtPcmRate = m_setup.runAtPcmRate;
    m_synth.reset(m_setup.emulator, m_setup.PCM_RATE, this);
    m_chipChannels.clear();
    m_chipChannels.resize((size_t)m_synth.m_numChannels);
}

void MIDIplay::resetMIDI()
{
    m_masterVolume = MasterVolumeDefault;
    m_sysExDeviceId = 0;
    m_synthMode = Mode_XG;
    m_arpeggioCounter = 0;

    m_midiChannels.clear();
    m_midiChannels.resize(16, MIDIchannel());

    caugh_missing_instruments.clear();
    caugh_missing_banks_melodic.clear();
    caugh_missing_banks_percussion.clear();
}

void MIDIplay::TickIterators(double s)
{
    for(uint16_t c = 0; c < m_synth.m_numChannels; ++c)
        m_chipChannels[c].addAge(static_cast<int64_t>(s * 1e6));
    updateVibrato(s);
    updateArpeggio(s);
#if !defined(ADLMIDI_AUDIO_TICK_HANDLER)
    updateGlide(s);
#endif
}

void MIDIplay::realTime_ResetState()
{
    for(size_t ch = 0; ch < m_midiChannels.size(); ch++)
    {
        MIDIchannel &chan = m_midiChannels[ch];
        chan.resetAllControllers();
        chan.volume = (m_synth.m_musicMode == OPL3::MODE_RSXX) ? 127 : 100;
        chan.vibpos = 0.0;
        chan.lastlrpn = 0;
        chan.lastmrpn = 0;
        chan.nrpn = false;
        if((m_synthMode & Mode_GS) != 0)// Reset custom drum channels on GS
            chan.is_xg_percussion = false;
        noteUpdateAll(uint16_t(ch), Upd_All);
        noteUpdateAll(uint16_t(ch), Upd_Off);
    }
    m_masterVolume = MasterVolumeDefault;
}

bool MIDIplay::realTime_NoteOn(uint8_t channel, uint8_t note, uint8_t velocity)
{
    if(note >= 128)
        note = 127;

    if((m_synth.m_musicMode == OPL3::MODE_RSXX) && (velocity != 0))
    {
        // Check if this is just a note after-touch
        MIDIchannel::activenoteiterator i = m_midiChannels[channel].activenotes_find(note);
        if(i)
        {
            const int veloffset = i->ains->midi_velocity_offset;
            velocity = (uint8_t)std::min(127, std::max(1, (int)velocity + veloffset));
            i->vol = velocity;
            noteUpdate(channel, i, Upd_Volume);
            return false;
        }
    }

    if(static_cast<size_t>(channel) > m_midiChannels.size())
        channel = channel % 16;
    noteOff(channel, note);
    // On Note on, Keyoff the note first, just in case keyoff
    // was omitted; this fixes Dance of sugar-plum fairy
    // by Microsoft. Now that we've done a Keyoff,
    // check if we still need to do a Keyon.
    // vol=0 and event 8x are both Keyoff-only.
    if(velocity == 0)
        return false;

    MIDIchannel &midiChan = m_midiChannels[channel];

    size_t midiins = midiChan.patch;
    bool isPercussion = (channel % 16 == 9) || midiChan.is_xg_percussion;

    size_t bank = (midiChan.bank_msb * 256) + midiChan.bank_lsb;

    if(isPercussion)
    {
        // == XG bank numbers ==
        // 0x7E00 - XG "SFX Kits" SFX1/SFX2 channel (16128 signed decimal)
        // 0x7F00 - XG "Drum Kits" Percussion channel (16256 signed decimal)

        // MIDI instrument defines the patch:
        if((m_synthMode & Mode_XG) != 0)
        {
            // Let XG SFX1/SFX2 bank will go in 128...255 range of LSB in WOPN file)
            // Let XG Percussion bank will use (0...127 LSB range in WOPN file)

            // Choose: SFX or Drum Kits
            bank = midiins + ((bank == 0x7E00) ? 128 : 0);
        }
        else
        {
            bank = midiins;
        }
        midiins = note; // Percussion instrument
    }

    if(isPercussion)
        bank += OPL3::PercussionTag;

    const adlinsdata2 *ains = &OPL3::m_emptyInstrument;

    //Set bank bank
    const OPL3::Bank *bnk = NULL;
    bool caughtMissingBank = false;
    if((bank & ~static_cast<uint16_t>(OPL3::PercussionTag)) > 0)
    {
        OPL3::BankMap::iterator b = m_synth.m_insBanks.find(bank);
        if(b != m_synth.m_insBanks.end())
            bnk = &b->second;
        if(bnk)
            ains = &bnk->ins[midiins];
        else
            caughtMissingBank = true;
    }

    //Or fall back to bank ignoring LSB (GS)
    if((ains->flags & adlinsdata::Flag_NoSound) && ((m_synthMode & Mode_GS) != 0))
    {
        size_t fallback = bank & ~(size_t)0x7F;
        if(fallback != bank)
        {
            OPL3::BankMap::iterator b = m_synth.m_insBanks.find(fallback);
            caughtMissingBank = false;
            if(b != m_synth.m_insBanks.end())
                bnk = &b->second;
            if(bnk)
                ains = &bnk->ins[midiins];
            else
                caughtMissingBank = true;
        }
    }

    if(caughtMissingBank && hooks.onDebugMessage)
    {
        std::set<size_t> &missing = (isPercussion) ?
                                    caugh_missing_banks_percussion : caugh_missing_banks_melodic;
        const char *text = (isPercussion) ?
                           "percussion" : "melodic";
        if(missing.insert(bank).second)
        {
            hooks.onDebugMessage(hooks.onDebugMessage_userData,
                                 "[%i] Playing missing %s MIDI bank %i (patch %i)",
                                 channel, text, (bank & ~static_cast<uint16_t>(OPL3::PercussionTag)), midiins);
        }
    }

    //Or fall back to first bank
    if((ains->flags & adlinsdata::Flag_NoSound) != 0)
    {
        OPL3::BankMap::iterator b = m_synth.m_insBanks.find(bank & OPL3::PercussionTag);
        if(b != m_synth.m_insBanks.end())
            bnk = &b->second;
        if(bnk)
            ains = &bnk->ins[midiins];
    }

    const int veloffset = ains->midi_velocity_offset;
    velocity = (uint8_t)std::min(127, std::max(1, (int)velocity + veloffset));

    int32_t tone = note;
    if(!isPercussion && (bank > 0)) // For non-zero banks
    {
        if(ains->flags & adlinsdata::Flag_NoSound)
        {
            if(hooks.onDebugMessage && caugh_missing_instruments.insert(static_cast<uint8_t>(midiins)).second)
            {
                hooks.onDebugMessage(hooks.onDebugMessage_userData,
                     "[%i] Caught a blank instrument %i (offset %i) in the MIDI bank %u",
                     channel, m_midiChannels[channel].patch, midiins, bank);
            }
            bank = 0;
            midiins = midiChan.patch;
        }
    }

    if(ains->tone)
    {
        /*if(ains->tone < 20)
            tone += ains->tone;
        else*/
        if(ains->tone < 128)
            tone = ains->tone;
        else
            tone -= ains->tone - 128;
    }

    //uint16_t i[2] = { ains->adlno1, ains->adlno2 };
    bool is_2op = !(ains->flags & (adlinsdata::Flag_Pseudo4op|adlinsdata::Flag_Real4op));
    bool pseudo_4op = ains->flags & adlinsdata::Flag_Pseudo4op;
#ifndef __WATCOMC__
    MIDIchannel::NoteInfo::Phys voices[MIDIchannel::NoteInfo::MaxNumPhysChans] =
    {
        {0, ains->adl[0], false},
        {0, (!is_2op) ? ains->adl[1] : ains->adl[0], pseudo_4op}
    };
#else /* Unfortunately, WatCom can't brace-initialize structure that incluses structure fields */
    MIDIchannel::NoteInfo::Phys voices[MIDIchannel::NoteInfo::MaxNumPhysChans];
    voices[0].chip_chan = 0;
    voices[0].ains = ains->adl[0];
    voices[0].pseudo4op = false;
    voices[1].chip_chan = 0;
    voices[1].ains = (!is_2op) ? ains->adl[1] : ains->adl[0];
    voices[1].pseudo4op = pseudo_4op;
#endif /* __WATCOMC__ */

    if((m_synth.m_rhythmMode == 1) && PercussionMap[midiins & 0xFF])
        voices[1] = voices[0];//i[1] = i[0];

    bool isBlankNote = (ains->flags & adlinsdata::Flag_NoSound) != 0;

    if(hooks.onDebugMessage)
    {
        if(isBlankNote && caugh_missing_instruments.insert(static_cast<uint8_t>(midiins)).second)
            hooks.onDebugMessage(hooks.onDebugMessage_userData, "[%i] Playing missing instrument %i", channel, midiins);
    }

    if(isBlankNote)
    {
        // Don't even try to play the blank instrument! But, insert the dummy note.
        std::pair<MIDIchannel::activenoteiterator, bool>
        dummy = midiChan.activenotes_insert(note);
        dummy.first->isBlank = true;
        dummy.first->ains = NULL;
        dummy.first->chip_channels_count = 0;
        // Record the last note on MIDI channel as source of portamento
        midiChan.portamentoSource = static_cast<int8_t>(note);
        return false;
    }

    // Allocate AdLib channel (the physical sound channel for the note)
    int32_t adlchannel[MIDIchannel::NoteInfo::MaxNumPhysChans] = { -1, -1 };

    for(uint32_t ccount = 0; ccount < MIDIchannel::NoteInfo::MaxNumPhysChans; ++ccount)
    {
        if(ccount == 1)
        {
            if(voices[0] == voices[1])
                break; // No secondary channel
            if(adlchannel[0] == -1)
                break; // No secondary if primary failed
        }

        int32_t c = -1;
        int32_t bs = -0x7FFFFFFFl;

        for(size_t a = 0; a < (size_t)m_synth.m_numChannels; ++a)
        {
            if(ccount == 1 && static_cast<int32_t>(a) == adlchannel[0]) continue;
            // ^ Don't use the same channel for primary&secondary

            if(is_2op || pseudo_4op)
            {
                // Only use regular channels
                uint32_t expected_mode = 0;

                if(m_synth.m_rhythmMode)
                {
                    if(m_cmfPercussionMode)
                        expected_mode = channel  < 11 ? 0 : (3 + channel  - 11); // CMF
                    else
                        expected_mode = PercussionMap[midiins & 0xFF];
                }

                if(m_synth.m_channelCategory[a] != expected_mode)
                    continue;
            }
            else
            {
                if(ccount == 0)
                {
                    // Only use four-op master channels
                    if(m_synth.m_channelCategory[a] != OPL3::ChanCat_4op_Master)
                        continue;
                }
                else
                {
                    // The secondary must be played on a specific channel.
                    if(a != static_cast<uint32_t>(adlchannel[0]) + 3)
                        continue;
                }
            }

            int64_t s = calculateChipChannelGoodness(a, voices[ccount]);
            if(s > bs)
            {
                bs = (int32_t)s;    // Best candidate wins
                c = static_cast<int32_t>(a);
            }
        }

        if(c < 0)
        {
            if(hooks.onDebugMessage)
                hooks.onDebugMessage(hooks.onDebugMessage_userData,
                                     "ignored unplaceable note [bank %i, inst %i, note %i, MIDI channel %i]",
                                     bank, midiChan.patch, note, channel);
            continue; // Could not play this note. Ignore it.
        }

        prepareChipChannelForNewNote(static_cast<size_t>(c), voices[ccount]);
        adlchannel[ccount] = c;
    }

    if(adlchannel[0] < 0 && adlchannel[1] < 0)
    {
        // The note could not be played, at all.
        return false;
    }

    //if(hooks.onDebugMessage)
    //    hooks.onDebugMessage(hooks.onDebugMessage_userData, "i1=%d:%d, i2=%d:%d", i[0],adlchannel[0], i[1],adlchannel[1]);

    if(midiChan.softPedal) // Apply Soft Pedal level reducing
        velocity = static_cast<uint8_t>(std::floor(static_cast<float>(velocity) * 0.8f));

    // Allocate active note for MIDI channel
    std::pair<MIDIchannel::activenoteiterator, bool>
    ir = midiChan.activenotes_insert(note);
    ir.first->vol     = velocity;
    ir.first->vibrato = midiChan.noteAftertouch[note];
    ir.first->noteTone = static_cast<int16_t>(tone);
    ir.first->currentTone = tone;
    ir.first->glideRate = HUGE_VAL;
    ir.first->midiins = midiins;
    ir.first->isPercussion = isPercussion;
    ir.first->isBlank = isBlankNote;
    ir.first->ains = ains;
    ir.first->chip_channels_count = 0;

    int8_t currentPortamentoSource = midiChan.portamentoSource;
    double currentPortamentoRate = midiChan.portamentoRate;
    bool portamentoEnable =
        midiChan.portamentoEnable && currentPortamentoRate != HUGE_VAL && !isPercussion;
    // Record the last note on MIDI channel as source of portamento
    midiChan.portamentoSource = static_cast<int8_t>(note);
    // midiChan.portamentoSource = portamentoEnable ? (int8_t)note : (int8_t)-1;

    // Enable gliding on portamento note
    if (portamentoEnable && currentPortamentoSource >= 0)
    {
        ir.first->currentTone = currentPortamentoSource;
        ir.first->glideRate = currentPortamentoRate;
        ++midiChan.gliding_note_count;
    }

    for(unsigned ccount = 0; ccount < MIDIchannel::NoteInfo::MaxNumPhysChans; ++ccount)
    {
        int32_t c = adlchannel[ccount];
        if(c < 0)
            continue;
        uint16_t chipChan = static_cast<uint16_t>(adlchannel[ccount]);
        ir.first->phys_ensure_find_or_create(chipChan)->assign(voices[ccount]);
    }

    noteUpdate(channel, ir.first, Upd_All | Upd_Patch);

    for(unsigned ccount = 0; ccount < MIDIchannel::NoteInfo::MaxNumPhysChans; ++ccount)
    {
        int32_t c = adlchannel[ccount];
        if(c < 0)
            continue;
        m_chipChannels[c].recent_ins = voices[ccount];
        m_chipChannels[c].addAge(0);
    }

    return true;
}

void MIDIplay::realTime_NoteOff(uint8_t channel, uint8_t note)
{
    if(static_cast<size_t>(channel) > m_midiChannels.size())
        channel = channel % 16;
    noteOff(channel, note);
}

void MIDIplay::realTime_NoteAfterTouch(uint8_t channel, uint8_t note, uint8_t atVal)
{
    if(static_cast<size_t>(channel) > m_midiChannels.size())
        channel = channel % 16;
    MIDIchannel &chan = m_midiChannels[channel];
    MIDIchannel::activenoteiterator i = m_midiChannels[channel].activenotes_find(note);
    if(i)
    {
        i->vibrato = atVal;
    }

    uint8_t oldAtVal = chan.noteAftertouch[note % 128];
    if(atVal != oldAtVal)
    {
        chan.noteAftertouch[note % 128] = atVal;
        bool inUse = atVal != 0;
        for(unsigned n = 0; !inUse && n < 128; ++n)
            inUse = chan.noteAftertouch[n] != 0;
        chan.noteAfterTouchInUse = inUse;
    }
}

void MIDIplay::realTime_ChannelAfterTouch(uint8_t channel, uint8_t atVal)
{
    if(static_cast<size_t>(channel) > m_midiChannels.size())
        channel = channel % 16;
    m_midiChannels[channel].aftertouch = atVal;
}

void MIDIplay::realTime_Controller(uint8_t channel, uint8_t type, uint8_t value)
{
    if(static_cast<size_t>(channel) > m_midiChannels.size())
        channel = channel % 16;
    switch(type)
    {
    case 1: // Adjust vibrato
        //UI.PrintLn("%u:vibrato %d", MidCh,value);
        m_midiChannels[channel].vibrato = value;
        break;

    case 0: // Set bank msb (GM bank)
        m_midiChannels[channel].bank_msb = value;
        if((m_synthMode & Mode_GS) == 0)// Don't use XG drums on GS synth mode
            m_midiChannels[channel].is_xg_percussion = isXgPercChannel(m_midiChannels[channel].bank_msb, m_midiChannels[channel].bank_lsb);
        break;

    case 32: // Set bank lsb (XG bank)
        m_midiChannels[channel].bank_lsb = value;
        if((m_synthMode & Mode_GS) == 0)// Don't use XG drums on GS synth mode
            m_midiChannels[channel].is_xg_percussion = isXgPercChannel(m_midiChannels[channel].bank_msb, m_midiChannels[channel].bank_lsb);
        break;

    case 5: // Set portamento msb
        m_midiChannels[channel].portamento = static_cast<uint16_t>((m_midiChannels[channel].portamento & 0x007F) | (value << 7));
        updatePortamento(channel);
        break;

    case 37: // Set portamento lsb
        m_midiChannels[channel].portamento = static_cast<uint16_t>((m_midiChannels[channel].portamento & 0x3F80) | (value));
        updatePortamento(channel);
        break;

    case 65: // Enable/disable portamento
        m_midiChannels[channel].portamentoEnable = value >= 64;
        updatePortamento(channel);
        break;

    case 7: // Change volume
        m_midiChannels[channel].volume = value;
        noteUpdateAll(channel, Upd_Volume);
        break;

    case 74: // Change brightness
        m_midiChannels[channel].brightness = value;
        noteUpdateAll(channel, Upd_Volume);
        break;

    case 64: // Enable/disable sustain
        m_midiChannels[channel].sustain = (value >= 64);
        if(!m_midiChannels[channel].sustain)
            killSustainingNotes(channel, -1, AdlChannel::LocationData::Sustain_Pedal);
        break;

    case 66: // Enable/disable sostenuto
        if(value >= 64) //Find notes and mark them as sostenutoed
            markSostenutoNotes(channel);
        else
            killSustainingNotes(channel, -1, AdlChannel::LocationData::Sustain_Sostenuto);
        break;

    case 67: // Enable/disable soft-pedal
        m_midiChannels[channel].softPedal = (value >= 64);
        break;

    case 11: // Change expression (another volume factor)
        m_midiChannels[channel].expression = value;
        noteUpdateAll(channel, Upd_Volume);
        break;

    case 10: // Change panning
        m_midiChannels[channel].panning = value;

        noteUpdateAll(channel, Upd_Pan);
        break;

    case 121: // Reset all controllers
        m_midiChannels[channel].resetAllControllers();
        noteUpdateAll(channel, Upd_Pan + Upd_Volume + Upd_Pitch);
        // Kill all sustained notes
        killSustainingNotes(channel, -1, AdlChannel::LocationData::Sustain_ANY);
        break;

    case 120: // All sounds off
        noteUpdateAll(channel, Upd_OffMute);
        break;

    case 123: // All notes off
        noteUpdateAll(channel, Upd_Off);
        break;

    case 91:
        break; // Reverb effect depth. We don't do per-channel reverb.

    case 92:
        break; // Tremolo effect depth. We don't do...

    case 93:
        break; // Chorus effect depth. We don't do.

    case 94:
        break; // Celeste effect depth. We don't do.

    case 95:
        break; // Phaser effect depth. We don't do.

    case 98:
        m_midiChannels[channel].lastlrpn = value;
        m_midiChannels[channel].nrpn = true;
        break;

    case 99:
        m_midiChannels[channel].lastmrpn = value;
        m_midiChannels[channel].nrpn = true;
        break;

    case 100:
        m_midiChannels[channel].lastlrpn = value;
        m_midiChannels[channel].nrpn = false;
        break;

    case 101:
        m_midiChannels[channel].lastmrpn = value;
        m_midiChannels[channel].nrpn = false;
        break;

    case 113:
        break; // Related to pitch-bender, used by missimp.mid in Duke3D

    case  6:
        setRPN(channel, value, true);
        break;

    case 38:
        setRPN(channel, value, false);
        break;

    case 103:
        if(m_synth.m_musicMode == OPL3::MODE_CMF)
            m_cmfPercussionMode = (value != 0);
        break; // CMF (ctrl 0x67) rhythm mode

    default:
        break;
        //UI.PrintLn("Ctrl %d <- %d (ch %u)", ctrlno, value, MidCh);
    }
}

void MIDIplay::realTime_PatchChange(uint8_t channel, uint8_t patch)
{
    if(static_cast<size_t>(channel) > m_midiChannels.size())
        channel = channel % 16;
    m_midiChannels[channel].patch = patch;
}

void MIDIplay::realTime_PitchBend(uint8_t channel, uint16_t pitch)
{
    if(static_cast<size_t>(channel) > m_midiChannels.size())
        channel = channel % 16;
    m_midiChannels[channel].bend = int(pitch) - 8192;
    noteUpdateAll(channel, Upd_Pitch);
}

void MIDIplay::realTime_PitchBend(uint8_t channel, uint8_t msb, uint8_t lsb)
{
    if(static_cast<size_t>(channel) > m_midiChannels.size())
        channel = channel % 16;
    m_midiChannels[channel].bend = int(lsb) + int(msb) * 128 - 8192;
    noteUpdateAll(channel, Upd_Pitch);
}

void MIDIplay::realTime_BankChangeLSB(uint8_t channel, uint8_t lsb)
{
    if(static_cast<size_t>(channel) > m_midiChannels.size())
        channel = channel % 16;
    m_midiChannels[channel].bank_lsb = lsb;
}

void MIDIplay::realTime_BankChangeMSB(uint8_t channel, uint8_t msb)
{
    if(static_cast<size_t>(channel) > m_midiChannels.size())
        channel = channel % 16;
    m_midiChannels[channel].bank_msb = msb;
}

void MIDIplay::realTime_BankChange(uint8_t channel, uint16_t bank)
{
    if(static_cast<size_t>(channel) > m_midiChannels.size())
        channel = channel % 16;
    m_midiChannels[channel].bank_lsb = uint8_t(bank & 0xFF);
    m_midiChannels[channel].bank_msb = uint8_t((bank >> 8) & 0xFF);
}

void MIDIplay::setDeviceId(uint8_t id)
{
    m_sysExDeviceId = id;
}

bool MIDIplay::realTime_SysEx(const uint8_t *msg, size_t size)
{
    if(size < 4 || msg[0] != 0xF0 || msg[size - 1] != 0xF7)
        return false;

    unsigned manufacturer = msg[1];
    unsigned dev = msg[2];
    msg += 3;
    size -= 4;

    switch(manufacturer)
    {
    default:
        break;
    case Manufacturer_UniversalNonRealtime:
    case Manufacturer_UniversalRealtime:
        return doUniversalSysEx(
            dev, manufacturer == Manufacturer_UniversalRealtime, msg, size);
    case Manufacturer_Roland:
        return doRolandSysEx(dev, msg, size);
    case Manufacturer_Yamaha:
        return doYamahaSysEx(dev, msg, size);
    }

    return false;
}

bool MIDIplay::doUniversalSysEx(unsigned dev, bool realtime, const uint8_t *data, size_t size)
{
    bool devicematch = dev == 0x7F || dev == m_sysExDeviceId;
    if(size < 2 || !devicematch)
        return false;

    unsigned address =
        (((unsigned)data[0] & 0x7F) << 8) |
        (((unsigned)data[1] & 0x7F));
    data += 2;
    size -= 2;

    switch(((unsigned)realtime << 16) | address)
    {
        case (0 << 16) | 0x0901: // GM System On
            if(hooks.onDebugMessage)
                hooks.onDebugMessage(hooks.onDebugMessage_userData, "SysEx: GM System On");
            m_synthMode = Mode_GM;
            realTime_ResetState();
            return true;
        case (0 << 16) | 0x0902: // GM System Off
            if(hooks.onDebugMessage)
                hooks.onDebugMessage(hooks.onDebugMessage_userData, "SysEx: GM System Off");
            m_synthMode = Mode_XG;//TODO: TEMPORARY, make something RIGHT
            realTime_ResetState();
            return true;
        case (1 << 16) | 0x0401: // MIDI Master Volume
            if(size != 2)
                break;
            unsigned volume =
                (((unsigned)data[0] & 0x7F)) |
                (((unsigned)data[1] & 0x7F) << 7);
            m_masterVolume = static_cast<uint8_t>(volume >> 7);
            for(size_t ch = 0; ch < m_midiChannels.size(); ch++)
                noteUpdateAll(uint16_t(ch), Upd_Volume);
            return true;
    }

    return false;
}

bool MIDIplay::doRolandSysEx(unsigned dev, const uint8_t *data, size_t size)
{
    bool devicematch = dev == 0x7F || (dev & 0x0F) == m_sysExDeviceId;
    if(size < 6 || !devicematch)
        return false;

    unsigned model = data[0] & 0x7F;
    unsigned mode = data[1] & 0x7F;
    unsigned checksum = data[size - 1] & 0x7F;
    data += 2;
    size -= 3;

#if !defined(ADLMIDI_SKIP_ROLAND_CHECKSUM)
    {
        unsigned checkvalue = 0;
        for(size_t i = 0; i < size; ++i)
            checkvalue += data[i] & 0x7F;
        checkvalue = (128 - (checkvalue & 127)) & 127;
        if(checkvalue != checksum)
        {
            if(hooks.onDebugMessage)
                hooks.onDebugMessage(hooks.onDebugMessage_userData, "SysEx: Caught invalid roland SysEx message!");
            return false;
        }
    }
#endif

    unsigned address =
        (((unsigned)data[0] & 0x7F) << 16) |
        (((unsigned)data[1] & 0x7F) << 8)  |
        (((unsigned)data[2] & 0x7F));
    unsigned target_channel = 0;

    /* F0 41 10 42 12 40 00 7F 00 41 F7 */

    if((address & 0xFFF0FF) == 0x401015) // Turn channel 1 into percussion
    {
        address = 0x401015;
        target_channel = data[1] & 0x0F;
    }

    data += 3;
    size -= 3;

    if(mode != RolandMode_Send) // don't have MIDI-Out reply ability
        return false;

    // Mode Set
    // F0 {41 10 42 12} {40 00 7F} {00 41} F7

    // Custom drum channels
    // F0 {41 10 42 12} {40 1<ch> 15} {<state> <sum>} F7

    switch((model << 24) | address)
    {
    case (RolandModel_GS << 24) | 0x00007F: // System Mode Set
    {
        if(size != 1 || (dev & 0xF0) != 0x10)
            break;
        unsigned mode = data[0] & 0x7F;
        ADL_UNUSED(mode);//TODO: Hook this correctly!
        if(hooks.onDebugMessage)
            hooks.onDebugMessage(hooks.onDebugMessage_userData, "SysEx: Caught Roland System Mode Set: %02X", mode);
        m_synthMode = Mode_GS;
        realTime_ResetState();
        return true;
    }
    case (RolandModel_GS << 24) | 0x40007F: // Mode Set
    {
        if(size != 1 || (dev & 0xF0) != 0x10)
            break;
        unsigned value = data[0] & 0x7F;
        ADL_UNUSED(value);//TODO: Hook this correctly!
        if(hooks.onDebugMessage)
            hooks.onDebugMessage(hooks.onDebugMessage_userData, "SysEx: Caught Roland Mode Set: %02X", value);
        m_synthMode = Mode_GS;
        realTime_ResetState();
        return true;
    }
    case (RolandModel_GS << 24) | 0x401015: // Percussion channel
    {
        if(size != 1 || (dev & 0xF0) != 0x10)
            break;
        if(m_midiChannels.size() < 16)
            break;
        unsigned value = data[0] & 0x7F;
        const uint8_t channels_map[16] =
        {
            9, 0, 1, 2, 3, 4, 5, 6, 7, 8, 10, 11, 12, 13, 14, 15
        };
        if(hooks.onDebugMessage)
            hooks.onDebugMessage(hooks.onDebugMessage_userData,
                                 "SysEx: Caught Roland Percussion set: %02X on channel %u (from %X)",
                                 value, channels_map[target_channel], target_channel);
        m_midiChannels[channels_map[target_channel]].is_xg_percussion = ((value == 0x01)) || ((value == 0x02));
        return true;
    }
    }

    return false;
}

bool MIDIplay::doYamahaSysEx(unsigned dev, const uint8_t *data, size_t size)
{
    bool devicematch = dev == 0x7F || (dev & 0x0F) == m_sysExDeviceId;
    if(size < 1 || !devicematch)
        return false;

    unsigned model = data[0] & 0x7F;
    ++data;
    --size;

    switch((model << 8) | (dev & 0xF0))
    {
    case (YamahaModel_XG << 8) | 0x10:  // parameter change
    {
        if(size < 3)
            break;

        unsigned address =
            (((unsigned)data[0] & 0x7F) << 16) |
            (((unsigned)data[1] & 0x7F) << 8)  |
            (((unsigned)data[2] & 0x7F));
        data += 3;
        size -= 3;

        switch(address)
        {
        case 0x00007E:  // XG System On
            if(size != 1)
                break;
            unsigned value = data[0] & 0x7F;
            ADL_UNUSED(value);//TODO: Hook this correctly!
            if(hooks.onDebugMessage)
                hooks.onDebugMessage(hooks.onDebugMessage_userData, "SysEx: Caught Yamaha XG System On: %02X", value);
            m_synthMode = Mode_XG;
            realTime_ResetState();
            return true;
        }

        break;
    }
    }

    return false;
}

void MIDIplay::realTime_panic()
{
    panic();
    killSustainingNotes(-1, -1, AdlChannel::LocationData::Sustain_ANY);
}

void MIDIplay::realTime_deviceSwitch(size_t track, const char *data, size_t length)
{
    const std::string indata(data, length);
    m_currentMidiDevice[track] = chooseDevice(indata);
}

size_t MIDIplay::realTime_currentDevice(size_t track)
{
    if(m_currentMidiDevice.empty())
        return 0;
    return m_currentMidiDevice[track];
}

void MIDIplay::realTime_rawOPL(uint8_t reg, uint8_t value)
{
    if((reg & 0xF0) == 0xC0)
        value |= 0x30;
    //std::printf("OPL poke %02X, %02X\n", reg, value);
    //std::fflush(stdout);
    m_synth.writeReg(0, reg, value);
}

#if defined(ADLMIDI_AUDIO_TICK_HANDLER)
void MIDIplay::AudioTick(uint32_t chipId, uint32_t rate)
{
    if(chipId != 0)  // do first chip ticks only
        return;

    uint32_t tickNumber = m_audioTickCounter++;
    double timeDelta = 1.0 / rate;

    enum { portamentoInterval = 32 }; // for efficiency, set rate limit on pitch updates

    if(tickNumber % portamentoInterval == 0)
    {
        double portamentoDelta = timeDelta * portamentoInterval;
        updateGlide(portamentoDelta);
    }
}
#endif

void MIDIplay::noteUpdate(size_t midCh,
                          MIDIplay::MIDIchannel::activenoteiterator i,
                          unsigned props_mask,
                          int32_t select_adlchn)
{
    MIDIchannel::NoteInfo &info = *i;
    const int16_t noteTone    = info.noteTone;
    const double currentTone    = info.currentTone;
    const uint8_t vol     = info.vol;
    const int midiins     = static_cast<int>(info.midiins);
    const adlinsdata2 &ains = *info.ains;
    AdlChannel::Location my_loc;
    my_loc.MidCh = static_cast<uint16_t>(midCh);
    my_loc.note  = info.note;

    if(info.isBlank)
    {
        if(props_mask & Upd_Off)
            m_midiChannels[midCh].activenotes_erase(i);
        return;
    }

    for(unsigned ccount = 0, ctotal = info.chip_channels_count; ccount < ctotal; ccount++)
    {
        const MIDIchannel::NoteInfo::Phys &ins = info.chip_channels[ccount];
        uint16_t c   = ins.chip_chan;

        if(select_adlchn >= 0 && c != select_adlchn) continue;

        if(props_mask & Upd_Patch)
        {
            m_synth.setPatch(c, ins.ains);
            AdlChannel::LocationData *d = m_chipChannels[c].users_find_or_create(my_loc);
            if(d)    // inserts if necessary
            {
                d->sustained = AdlChannel::LocationData::Sustain_None;
                d->vibdelay_us = 0;
                d->fixed_sustain = (ains.ms_sound_kon == static_cast<uint16_t>(adlNoteOnMaxTime));
                d->kon_time_until_neglible_us = 1000 * ains.ms_sound_kon;
                d->ins       = ins;
            }
        }
    }

    for(unsigned ccount = 0; ccount < info.chip_channels_count; ccount++)
    {
        const MIDIchannel::NoteInfo::Phys &ins = info.chip_channels[ccount];
        uint16_t c          = ins.chip_chan;
        uint16_t c_slave    = info.chip_channels[1].chip_chan;

        if(select_adlchn >= 0 && c != select_adlchn)
            continue;

        if(props_mask & Upd_Off) // note off
        {
            if(!m_midiChannels[midCh].sustain)
            {
                AdlChannel::LocationData *k = m_chipChannels[c].users_find(my_loc);
                bool do_erase_user = (k && ((k->sustained & AdlChannel::LocationData::Sustain_Sostenuto) == 0));
                if(do_erase_user)
                    m_chipChannels[c].users_erase(k);

                if(hooks.onNote)
                    hooks.onNote(hooks.onNote_userData, c, noteTone, midiins, 0, 0.0);

                if(do_erase_user && m_chipChannels[c].users_empty())
                {
                    m_synth.noteOff(c);
                    if(props_mask & Upd_Mute) // Mute the note
                    {
                        m_synth.touchNote(c, 0);
                        m_chipChannels[c].koff_time_until_neglible_us = 0;
                    }
                    else
                    {
                        m_chipChannels[c].koff_time_until_neglible_us = 1000 * int64_t(ains.ms_sound_koff);
                    }
                }
            }
            else
            {
                // Sustain: Forget about the note, but don't key it off.
                //          Also will avoid overwriting it very soon.
                AdlChannel::LocationData *d = m_chipChannels[c].users_find_or_create(my_loc);
                if(d)
                    d->sustained |= AdlChannel::LocationData::Sustain_Pedal; // note: not erased!
                if(hooks.onNote)
                    hooks.onNote(hooks.onNote_userData, c, noteTone, midiins, -1, 0.0);
            }

            info.phys_erase_at(&ins);  // decrements channel count
            --ccount;  // adjusts index accordingly
            continue;
        }

        if(props_mask & Upd_Pan)
            m_synth.setPan(c, m_midiChannels[midCh].panning);

        if(props_mask & Upd_Volume)
        {
            uint_fast32_t volume;
            bool is_percussion = (midCh == 9) || m_midiChannels[midCh].is_xg_percussion;
            uint_fast32_t brightness = is_percussion ? 127 : m_midiChannels[midCh].brightness;

            if(!m_setup.fullRangeBrightnessCC74)
            {
                // Simulate post-High-Pass filter result which affects sounding by half level only
                if(brightness >= 64)
                    brightness = 127;
                else
                    brightness *= 2;
            }

            switch(m_synth.m_volumeScale)
            {
            default:
            case OPL3::VOLUME_Generic:
            {
                volume = vol * m_masterVolume * m_midiChannels[midCh].volume * m_midiChannels[midCh].expression;

                /* If the channel has arpeggio, the effective volume of
                     * *this* instrument is actually lower due to timesharing.
                     * To compensate, add extra volume that corresponds to the
                     * time this note is *not* heard.
                     * Empirical tests however show that a full equal-proportion
                     * increment sounds wrong. Therefore, using the square root.
                     */
                //volume = (int)(volume * std::sqrt( (double) ch[c].users.size() ));

                // The formula below: SOLVE(V=127^4 * 2^( (A-63.49999) / 8), A)
                volume = volume > (8725 * 127) ? static_cast<uint_fast32_t>(std::log(static_cast<double>(volume)) * 11.541560327111707 - 1.601379199767093e+02) : 0;
                // The incorrect formula below: SOLVE(V=127^4 * (2^(A/63)-1), A)
                //opl.Touch_Real(c, volume>(11210*127) ? 91.61112 * std::log((4.8819E-7/127)*volume + 1.0)+0.5 : 0);
            }
            break;

            case OPL3::VOLUME_NATIVE:
            {
                volume = vol * m_midiChannels[midCh].volume * m_midiChannels[midCh].expression;
                // volume = volume * m_masterVolume / (127 * 127 * 127) / 2;
                volume = (volume * m_masterVolume) / 4096766;
            }
            break;

            case OPL3::VOLUME_DMX:
            {
                volume = 2 * (m_midiChannels[midCh].volume * m_midiChannels[midCh].expression * m_masterVolume / 16129) + 1;
                //volume = 2 * (Ch[MidCh].volume) + 1;
                volume = (DMX_volume_mapping_table[(vol < 128) ? vol : 127] * volume) >> 9;
            }
            break;

            case OPL3::VOLUME_APOGEE:
            {
                volume = (m_midiChannels[midCh].volume * m_midiChannels[midCh].expression * m_masterVolume / 16129);
                volume = ((64 * (vol + 0x80)) * volume) >> 15;
                //volume = ((63 * (vol + 0x80)) * Ch[MidCh].volume) >> 15;
            }
            break;

            case OPL3::VOLUME_9X:
            {
                //volume = 63 - W9X_volume_mapping_table[(((vol * Ch[MidCh].volume /** Ch[MidCh].expression*/) * m_masterVolume / 16129 /*2048383*/) >> 2)];
                volume = 63 - W9X_volume_mapping_table[((vol * m_midiChannels[midCh].volume * m_midiChannels[midCh].expression * m_masterVolume / 2048383) >> 2)];
                //volume = W9X_volume_mapping_table[vol >> 2] + volume;
            }
            break;
            }

            m_synth.touchNote(c, static_cast<uint8_t>(volume), static_cast<uint8_t>(brightness));

            /* DEBUG ONLY!!!
            static uint32_t max = 0;

            if(volume == 0)
                max = 0;

            if(volume > max)
                max = volume;

            printf("%d\n", max);
            fflush(stdout);
            */
        }

        if(props_mask & Upd_Pitch)
        {
            AdlChannel::LocationData *d = m_chipChannels[c].users_find(my_loc);

            // Don't bend a sustained note
            if(!d || (d->sustained == AdlChannel::LocationData::Sustain_None))
            {
                double midibend = m_midiChannels[midCh].bend * m_midiChannels[midCh].bendsense;
                double bend = midibend + ins.ains.finetune;
                double phase = 0.0;
                uint8_t vibrato = std::max(m_midiChannels[midCh].vibrato, m_midiChannels[midCh].aftertouch);
                vibrato = std::max(vibrato, i->vibrato);

                if((ains.flags & adlinsdata::Flag_Pseudo4op) && ins.pseudo4op)
                {
                    phase = ains.voice2_fine_tune;//0.125; // Detune the note slightly (this is what Doom does)
                }

                if(vibrato && (!d || d->vibdelay_us >= m_midiChannels[midCh].vibdelay_us))
                    bend += static_cast<double>(vibrato) * m_midiChannels[midCh].vibdepth * std::sin(m_midiChannels[midCh].vibpos);

#define BEND_COEFFICIENT 172.4387
                m_synth.noteOn(c, c_slave, BEND_COEFFICIENT * std::exp(0.057762265 * (currentTone + bend + phase)));
#undef BEND_COEFFICIENT
                if(hooks.onNote)
                    hooks.onNote(hooks.onNote_userData, c, noteTone, midiins, vol, midibend);
            }
        }
    }

    if(info.chip_channels_count == 0)
    {
        if(i->glideRate != HUGE_VAL)
            --m_midiChannels[midCh].gliding_note_count;
        m_midiChannels[midCh].activenotes_erase(i);
    }
}

void MIDIplay::noteUpdateAll(size_t midCh, unsigned props_mask)
{
    for(MIDIchannel::activenoteiterator
        i = m_midiChannels[midCh].activenotes_begin(); i;)
    {
        MIDIchannel::activenoteiterator j(i++);
        noteUpdate(midCh, j, props_mask);
    }
}

const std::string &MIDIplay::getErrorString()
{
    return errorStringOut;
}

void MIDIplay::setErrorString(const std::string &err)
{
    errorStringOut = err;
}

int64_t MIDIplay::calculateChipChannelGoodness(size_t c, const MIDIchannel::NoteInfo::Phys &ins) const
{
    const AdlChannel &chan = m_chipChannels[c];
    int64_t koff_ms = chan.koff_time_until_neglible_us / 1000;
    int64_t s = -koff_ms;

    // Rate channel with a releasing note
    if(s < 0 && chan.users_empty())
    {
        s -= 40000;
        // If it's same instrument, better chance to get it when no free channels
        if(chan.recent_ins == ins)
            s = (m_synth.m_musicMode == OPL3::MODE_CMF) ? 0 : -koff_ms;
        return s;
    }

    // Same midi-instrument = some stability
    for(AdlChannel::LocationData *j = chan.users_first; j; j = j->next)
    {
        s -= 4000000;

        int64_t kon_ms = j->kon_time_until_neglible_us / 1000;
        s -= (j->sustained == AdlChannel::LocationData::Sustain_None) ?
            kon_ms : (kon_ms / 2);

        MIDIchannel::activenoteiterator
        k = const_cast<MIDIchannel &>(m_midiChannels[j->loc.MidCh]).activenotes_find(j->loc.note);

        if(k)
        {
            // Same instrument = good
            if(j->ins == ins)
            {
                s += 300;
                // Arpeggio candidate = even better
                if(j->vibdelay_us < 70000
                   || j->kon_time_until_neglible_us > 20000000)
                    s += 10;
            }

            // Percussion is inferior to melody
            s += k->isPercussion ? 50 : 0;
            /*
                    if(k->second.midiins >= 25
                    && k->second.midiins < 40
                    && j->second.ins != ins)
                    {
                        s -= 14000; // HACK: Don't clobber the bass or the guitar
                    }
                    */
        }

        // If there is another channel to which this note
        // can be evacuated to in the case of congestion,
        // increase the score slightly.
        unsigned n_evacuation_stations = 0;

        for(size_t c2 = 0; c2 < static_cast<size_t>(m_synth.m_numChannels); ++c2)
        {
            if(c2 == c) continue;

            if(m_synth.m_channelCategory[c2]
               != m_synth.m_channelCategory[c]) continue;

            for(AdlChannel::LocationData *m = m_chipChannels[c2].users_first; m; m = m->next)
            {
                if(m->sustained != AdlChannel::LocationData::Sustain_None) continue;
                if(m->vibdelay_us >= 200000) continue;
                if(m->ins != j->ins) continue;
                n_evacuation_stations += 1;
            }
        }

        s += (int64_t)n_evacuation_stations * 4;
    }

    return s;
}


void MIDIplay::prepareChipChannelForNewNote(size_t c, const MIDIchannel::NoteInfo::Phys &ins)
{
    if(m_chipChannels[c].users_empty()) return; // Nothing to do

    //bool doing_arpeggio = false;
    for(AdlChannel::LocationData *jnext = m_chipChannels[c].users_first; jnext;)
    {
        AdlChannel::LocationData *j = jnext;
        jnext = jnext->next;

        if(j->sustained == AdlChannel::LocationData::Sustain_None)
        {
            // Collision: Kill old note,
            // UNLESS we're going to do arpeggio
            MIDIchannel::activenoteiterator i
            (m_midiChannels[j->loc.MidCh].activenotes_ensure_find(j->loc.note));

            // Check if we can do arpeggio.
            if((j->vibdelay_us < 70000
                || j->kon_time_until_neglible_us > 20000000)
               && j->ins == ins)
            {
                // Do arpeggio together with this note.
                //doing_arpeggio = true;
                continue;
            }

            killOrEvacuate(c, j, i);
            // ^ will also erase j from ch[c].users.
        }
    }

    // Kill all sustained notes on this channel
    // Don't keep them for arpeggio, because arpeggio requires
    // an intact "activenotes" record. This is a design flaw.
    killSustainingNotes(-1, static_cast<int32_t>(c), AdlChannel::LocationData::Sustain_ANY);

    // Keyoff the channel so that it can be retriggered,
    // unless the new note will be introduced as just an arpeggio.
    if(m_chipChannels[c].users_empty())
        m_synth.noteOff(c);
}

void MIDIplay::killOrEvacuate(size_t from_channel,
                              AdlChannel::LocationData *j,
                              MIDIplay::MIDIchannel::activenoteiterator i)
{
    uint32_t maxChannels = ADL_MAX_CHIPS * 18;

    // Before killing the note, check if it can be
    // evacuated to another channel as an arpeggio
    // instrument. This helps if e.g. all channels
    // are full of strings and we want to do percussion.
    // FIXME: This does not care about four-op entanglements.
    for(uint32_t c = 0; c < m_synth.m_numChannels; ++c)
    {
        uint16_t cs = static_cast<uint16_t>(c);

        if(c >= maxChannels)
            break;
        if(c == from_channel)
            continue;
        if(m_synth.m_channelCategory[c] != m_synth.m_channelCategory[from_channel])
            continue;

        AdlChannel &adlch = m_chipChannels[c];
        if(adlch.users_size == AdlChannel::users_max)
            continue;  // no room for more arpeggio on channel

        for(AdlChannel::LocationData *m = adlch.users_first; m; m = m->next)
        {
            if(m->vibdelay_us >= 200000
               && m->kon_time_until_neglible_us < 10000000) continue;
            if(m->ins != j->ins)
                continue;
            if(hooks.onNote)
            {
                hooks.onNote(hooks.onNote_userData,
                             (int)from_channel,
                             i->noteTone,
                             static_cast<int>(i->midiins), 0, 0.0);
                hooks.onNote(hooks.onNote_userData,
                             (int)c,
                             i->noteTone,
                             static_cast<int>(i->midiins),
                             i->vol, 0.0);
            }

            i->phys_erase(static_cast<uint16_t>(from_channel));
            i->phys_ensure_find_or_create(cs)->assign(j->ins);
            if(!m_chipChannels[cs].users_insert(*j))
                assert(false);
            m_chipChannels[from_channel].users_erase(j);
            return;
        }
    }

    /*UI.PrintLn(
                "collision @%u: [%ld] <- ins[%3u]",
                c,
                //ch[c].midiins<128?'M':'P', ch[c].midiins&127,
                ch[c].age, //adlins[ch[c].insmeta].ms_sound_kon,
                ins
                );*/
    // Kill it
    noteUpdate(j->loc.MidCh,
               i,
               Upd_Off,
               static_cast<int32_t>(from_channel));
}

void MIDIplay::panic()
{
    for(uint8_t chan = 0; chan < m_midiChannels.size(); chan++)
    {
        for(uint8_t note = 0; note < 128; note++)
            realTime_NoteOff(chan, note);
    }
}

void MIDIplay::killSustainingNotes(int32_t midCh, int32_t this_adlchn, uint32_t sustain_type)
{
    uint32_t first = 0, last = m_synth.m_numChannels;

    if(this_adlchn >= 0)
    {
        first = static_cast<uint32_t>(this_adlchn);
        last = first + 1;
    }

    for(uint32_t c = first; c < last; ++c)
    {
        if(m_chipChannels[c].users_empty())
            continue; // Nothing to do

        for(AdlChannel::LocationData *jnext = m_chipChannels[c].users_first; jnext;)
        {
            AdlChannel::LocationData *j = jnext;
            jnext = jnext->next;

            if((midCh < 0 || j->loc.MidCh == midCh)
                && ((j->sustained & sustain_type) != 0))
            {
                int midiins = '?';
                if(hooks.onNote)
                    hooks.onNote(hooks.onNote_userData, (int)c, j->loc.note, midiins, 0, 0.0);
                j->sustained &= ~sustain_type;
                if(j->sustained == AdlChannel::LocationData::Sustain_None)
                    m_chipChannels[c].users_erase(j);//Remove only when note is clean from any holders
            }
        }

        // Keyoff the channel, if there are no users left.
        if(m_chipChannels[c].users_empty())
            m_synth.noteOff(c);
    }
}

void MIDIplay::markSostenutoNotes(int32_t midCh)
{
    uint32_t first = 0, last = m_synth.m_numChannels;
    for(uint32_t c = first; c < last; ++c)
    {
        if(m_chipChannels[c].users_empty())
            continue; // Nothing to do

        for(AdlChannel::LocationData *jnext = m_chipChannels[c].users_first; jnext;)
        {
            AdlChannel::LocationData *j = jnext;
            jnext = jnext->next;
            if((j->loc.MidCh == midCh) && (j->sustained == AdlChannel::LocationData::Sustain_None))
                j->sustained |= AdlChannel::LocationData::Sustain_Sostenuto;
        }
    }
}

void MIDIplay::setRPN(size_t midCh, unsigned value, bool MSB)
{
    bool nrpn = m_midiChannels[midCh].nrpn;
    unsigned addr = m_midiChannels[midCh].lastmrpn * 0x100 + m_midiChannels[midCh].lastlrpn;

    switch(addr + nrpn * 0x10000 + MSB * 0x20000)
    {
    case 0x0000 + 0*0x10000 + 1*0x20000: // Pitch-bender sensitivity
        m_midiChannels[midCh].bendsense_msb = value;
        m_midiChannels[midCh].updateBendSensitivity();
        break;
    case 0x0000 + 0*0x10000 + 0*0x20000: // Pitch-bender sensitivity LSB
        m_midiChannels[midCh].bendsense_lsb = value;
        m_midiChannels[midCh].updateBendSensitivity();
        break;
    case 0x0108 + 1*0x10000 + 1*0x20000:
        if((m_synthMode & Mode_XG) != 0) // Vibrato speed
        {
            if(value == 64)      m_midiChannels[midCh].vibspeed = 1.0;
            else if(value < 100) m_midiChannels[midCh].vibspeed = 1.0 / (1.6e-2 * (value ? value : 1));
            else                 m_midiChannels[midCh].vibspeed = 1.0 / (0.051153846 * value - 3.4965385);
            m_midiChannels[midCh].vibspeed *= 2 * 3.141592653 * 5.0;
        }
        break;
    case 0x0109 + 1*0x10000 + 1*0x20000:
        if((m_synthMode & Mode_XG) != 0) // Vibrato depth
        {
            m_midiChannels[midCh].vibdepth = (((int)value - 64) * 0.15) * 0.01;
        }
        break;
    case 0x010A + 1*0x10000 + 1*0x20000:
        if((m_synthMode & Mode_XG) != 0) // Vibrato delay in millisecons
        {
            m_midiChannels[midCh].vibdelay_us = value ? int64_t(209.2 * std::exp(0.0795 * (double)value)) : 0;
        }
        break;
    default:/* UI.PrintLn("%s %04X <- %d (%cSB) (ch %u)",
                "NRPN"+!nrpn, addr, value, "LM"[MSB], MidCh);*/
        break;
    }
}

void MIDIplay::updatePortamento(size_t midCh)
{
    double rate = HUGE_VAL;
    uint16_t midival = m_midiChannels[midCh].portamento;
    if(m_midiChannels[midCh].portamentoEnable && midival > 0)
        rate = 350.0 * std::pow(2.0, -0.062 * (1.0 / 128) * midival);
    m_midiChannels[midCh].portamentoRate = rate;
}


void MIDIplay::noteOff(size_t midCh, uint8_t note)
{
    MIDIchannel::activenoteiterator
    i = m_midiChannels[midCh].activenotes_find(note);
    if(i)
        noteUpdate(midCh, i, Upd_Off);
}


void MIDIplay::updateVibrato(double amount)
{
    for(size_t a = 0, b = m_midiChannels.size(); a < b; ++a)
    {
        if(m_midiChannels[a].hasVibrato() && !m_midiChannels[a].activenotes_empty())
        {
            noteUpdateAll(static_cast<uint16_t>(a), Upd_Pitch);
            m_midiChannels[a].vibpos += amount * m_midiChannels[a].vibspeed;
        }
        else
            m_midiChannels[a].vibpos = 0.0;
    }
}

size_t MIDIplay::chooseDevice(const std::string &name)
{
    std::map<std::string, size_t>::iterator i = m_midiDevices.find(name);

    if(i != m_midiDevices.end())
        return i->second;

    size_t n = m_midiDevices.size() * 16;
    m_midiDevices.insert(std::make_pair(name, n));
    m_midiChannels.resize(n + 16);
    return n;
}

void MIDIplay::updateArpeggio(double) // amount = amount of time passed
{
    // If there is an adlib channel that has multiple notes
    // simulated on the same channel, arpeggio them.
#if 0
    const unsigned desired_arpeggio_rate = 40; // Hz (upper limit)
#   if 1
    static unsigned cache = 0;
    amount = amount; // Ignore amount. Assume we get a constant rate.
    cache += MaxSamplesAtTime * desired_arpeggio_rate;

    if(cache < PCM_RATE) return;

    cache %= PCM_RATE;
#   else
    static double arpeggio_cache = 0;
    arpeggio_cache += amount * desired_arpeggio_rate;

    if(arpeggio_cache < 1.0) return;

    arpeggio_cache = 0.0;
#   endif
#endif

    ++m_arpeggioCounter;

    for(uint32_t c = 0; c < m_synth.m_numChannels; ++c)
    {
retry_arpeggio:
        if(c > uint32_t(std::numeric_limits<int32_t>::max()))
            break;

        size_t n_users = m_chipChannels[c].users_size;

        if(n_users > 1)
        {
            AdlChannel::LocationData *i = m_chipChannels[c].users_first;
            size_t rate_reduction = 3;

            if(n_users >= 3)
                rate_reduction = 2;

            if(n_users >= 4)
                rate_reduction = 1;

            for(size_t count = (m_arpeggioCounter / rate_reduction) % n_users,
                n = 0; n < count; ++n)
                i = i->next;

            if(i->sustained == AdlChannel::LocationData::Sustain_None)
            {
                if(i->kon_time_until_neglible_us <= 0)
                {
                    noteUpdate(
                        i->loc.MidCh,
                        m_midiChannels[ i->loc.MidCh ].activenotes_ensure_find(i->loc.note),
                        Upd_Off,
                        static_cast<int32_t>(c));
                    goto retry_arpeggio;
                }

                noteUpdate(
                    i->loc.MidCh,
                    m_midiChannels[ i->loc.MidCh ].activenotes_ensure_find(i->loc.note),
                    Upd_Pitch | Upd_Volume | Upd_Pan,
                    static_cast<int32_t>(c));
            }
        }
    }
}

void MIDIplay::updateGlide(double amount)
{
    size_t num_channels = m_midiChannels.size();

    for(size_t channel = 0; channel < num_channels; ++channel)
    {
        MIDIchannel &midiChan = m_midiChannels[channel];
        if(midiChan.gliding_note_count == 0)
            continue;

        for(MIDIchannel::activenoteiterator it = midiChan.activenotes_begin();
            it; ++it)
        {
            double finalTone = it->noteTone;
            double previousTone = it->currentTone;

            bool directionUp = previousTone < finalTone;
            double toneIncr = amount * (directionUp ? +it->glideRate : -it->glideRate);

            double currentTone = previousTone + toneIncr;
            bool glideFinished = !(directionUp ? (currentTone < finalTone) : (currentTone > finalTone));
            currentTone = glideFinished ? finalTone : currentTone;

            if(currentTone != previousTone)
            {
                it->currentTone = currentTone;
                noteUpdate(static_cast<uint16_t>(channel), it, Upd_Pitch);
            }
        }
    }
}

void MIDIplay::describeChannels(char *str, char *attr, size_t size)
{
    if (!str || size <= 0)
        return;

    OPL3 &synth = m_synth;
    uint32_t numChannels = synth.m_numChannels;

    uint32_t index = 0;
    while(index < numChannels && index < size - 1)
    {
        const AdlChannel &adlChannel = m_chipChannels[index];

        AdlChannel::LocationData *loc = adlChannel.users_first;
        if(!loc)  // off
        {
            str[index] = '-';
        }
        else if(loc->next)  // arpeggio
        {
            str[index] = '@';
        }
        else  // on
        {
            switch(synth.m_channelCategory[index])
            {
            case OPL3::ChanCat_Regular:
                str[index] = '+';
                break;
            case OPL3::ChanCat_4op_Master:
            case OPL3::ChanCat_4op_Slave:
                str[index] = '#';
                break;
            default:  // rhythm-mode percussion
                str[index] = 'r';
                break;
            }
        }

        uint8_t attribute = 0;
        if (loc)  // 4-bit color index of MIDI channel
            attribute |= (uint8_t)(loc->loc.MidCh & 0xF);

        attr[index] = (char)attribute;
        ++index;
    }

    str[index] = 0;
    attr[index] = 0;
}

#ifndef ADLMIDI_DISABLE_CPP_EXTRAS

struct AdlInstrumentTester::Impl
{
    uint32_t cur_gm;
    uint32_t ins_idx;
    std::vector<uint32_t> adl_ins_list;
    OPL3 *opl;
    MIDIplay *play;
};

ADLMIDI_EXPORT AdlInstrumentTester::AdlInstrumentTester(ADL_MIDIPlayer *device)
    : P(new Impl)
{
#ifndef DISABLE_EMBEDDED_BANKS
    MIDIplay *play = reinterpret_cast<MIDIplay *>(device->adl_midiPlayer);
    P->cur_gm = 0;
    P->ins_idx = 0;
    P->play = play;
    P->opl = play ? &play->m_synth : NULL;
#else
    ADL_UNUSED(device);
#endif
}

ADLMIDI_EXPORT AdlInstrumentTester::~AdlInstrumentTester()
{
    delete P;
}

ADLMIDI_EXPORT void AdlInstrumentTester::FindAdlList()
{
#ifndef DISABLE_EMBEDDED_BANKS
    const unsigned NumBanks = (unsigned)adl_getBanksCount();
    std::set<unsigned> adl_ins_set;
    for(unsigned bankno = 0; bankno < NumBanks; ++bankno)
        adl_ins_set.insert(banks[bankno][P->cur_gm]);
    P->adl_ins_list.assign(adl_ins_set.begin(), adl_ins_set.end());
    P->ins_idx = 0;
    NextAdl(0);
    P->opl->silenceAll();
#endif
}



ADLMIDI_EXPORT void AdlInstrumentTester::Touch(unsigned c, unsigned volume) // Volume maxes at 127*127*127
{
#ifndef DISABLE_EMBEDDED_BANKS
    OPL3 *opl = P->opl;
    if(opl->m_volumeScale == OPL3::VOLUME_NATIVE)
        opl->touchNote(c, static_cast<uint8_t>(volume * 127 / (127 * 127 * 127) / 2));
    else
    {
        // The formula below: SOLVE(V=127^3 * 2^( (A-63.49999) / 8), A)
        opl->touchNote(c, static_cast<uint8_t>(volume > 8725 ? static_cast<unsigned int>(std::log((double)volume) * 11.541561 + (0.5 - 104.22845)) : 0));
        // The incorrect formula below: SOLVE(V=127^3 * (2^(A/63)-1), A)
        //Touch_Real(c, volume>11210 ? 91.61112 * std::log(4.8819E-7*volume + 1.0)+0.5 : 0);
    }
#else
    ADL_UNUSED(c);
    ADL_UNUSED(volume);
#endif
}

ADLMIDI_EXPORT void AdlInstrumentTester::DoNote(int note)
{
#ifndef DISABLE_EMBEDDED_BANKS
    MIDIplay *play = P->play;
    OPL3 *opl = P->opl;
    if(P->adl_ins_list.empty()) FindAdlList();
    const unsigned meta = P->adl_ins_list[P->ins_idx];
    const adlinsdata2 ains = adlinsdata2::from_adldata(::adlins[meta]);

    int tone = (P->cur_gm & 128) ? (P->cur_gm & 127) : (note + 50);
    if(ains.tone)
    {
        /*if(ains.tone < 20)
                tone += ains.tone;
            else */
        if(ains.tone < 128)
            tone = ains.tone;
        else
            tone -= ains.tone - 128;
    }
    double hertz = 172.00093 * std::exp(0.057762265 * (tone + 0.0));
    int32_t adlchannel[2] = { 0, 3 };
    if((ains.flags & (adlinsdata::Flag_Pseudo4op|adlinsdata::Flag_Real4op)) == 0)
    {
        adlchannel[1] = -1;
        adlchannel[0] = 6; // single-op
        if(play->hooks.onDebugMessage)
        {
            play->hooks.onDebugMessage(play->hooks.onDebugMessage_userData,
                                       "noteon at %d for %g Hz\n", adlchannel[0], hertz);
        }
    }
    else
    {
        if(play->hooks.onDebugMessage)
        {
            play->hooks.onDebugMessage(play->hooks.onDebugMessage_userData,
                                       "noteon at %d and %d for %g Hz\n", adlchannel[0], adlchannel[1], hertz);
        }
    }

    opl->noteOff(0);
    opl->noteOff(3);
    opl->noteOff(6);
    for(unsigned c = 0; c < 2; ++c)
    {
        if(adlchannel[c] < 0) continue;
        opl->setPatch(static_cast<size_t>(adlchannel[c]), ains.adl[c]);
        opl->touchNote(static_cast<size_t>(adlchannel[c]), 63);
        opl->setPan(static_cast<size_t>(adlchannel[c]), 0x30);
        opl->noteOn(static_cast<size_t>(adlchannel[c]), static_cast<size_t>(adlchannel[1]), hertz);
    }
#else
    ADL_UNUSED(note);
#endif
}

ADLMIDI_EXPORT void AdlInstrumentTester::NextGM(int offset)
{
#ifndef DISABLE_EMBEDDED_BANKS
    P->cur_gm = (P->cur_gm + 256 + (uint32_t)offset) & 0xFF;
    FindAdlList();
#else
    ADL_UNUSED(offset);
#endif
}

ADLMIDI_EXPORT void AdlInstrumentTester::NextAdl(int offset)
{
#ifndef DISABLE_EMBEDDED_BANKS
    //OPL3 *opl = P->opl;
    if(P->adl_ins_list.empty()) FindAdlList();
    const unsigned NumBanks = (unsigned)adl_getBanksCount();
    P->ins_idx = (uint32_t)((int32_t)P->ins_idx + (int32_t)P->adl_ins_list.size() + offset) % (int32_t)P->adl_ins_list.size();

#if 0
    UI.Color(15);
    std::fflush(stderr);
    std::printf("SELECTED G%c%d\t%s\n",
                cur_gm < 128 ? 'M' : 'P', cur_gm < 128 ? cur_gm + 1 : cur_gm - 128,
                "<-> select GM, ^v select ins, qwe play note");
    std::fflush(stdout);
    UI.Color(7);
    std::fflush(stderr);
#endif

    for(size_t a = 0, n = P->adl_ins_list.size(); a < n; ++a)
    {
        const unsigned i = P->adl_ins_list[a];
        const adlinsdata2 ains = adlinsdata2::from_adldata(::adlins[i]);

        char ToneIndication[8] = "   ";
        if(ains.tone)
        {
            /*if(ains.tone < 20)
                    snprintf(ToneIndication, 8, "+%-2d", ains.tone);
                else*/
            if(ains.tone < 128)
                snprintf(ToneIndication, 8, "=%-2d", ains.tone);
            else
                snprintf(ToneIndication, 8, "-%-2d", ains.tone - 128);
        }
        std::printf("%s%s%s%u\t",
                    ToneIndication,
                    (ains.flags & (adlinsdata::Flag_Pseudo4op|adlinsdata::Flag_Real4op)) ? "[2]" : "   ",
                    (P->ins_idx == a) ? "->" : "\t",
                    i
                   );

        for(unsigned bankno = 0; bankno < NumBanks; ++bankno)
            if(banks[bankno][P->cur_gm] == i)
                std::printf(" %u", bankno);

        std::printf("\n");
    }
#else
    ADL_UNUSED(offset);
#endif
}

ADLMIDI_EXPORT bool AdlInstrumentTester::HandleInputChar(char ch)
{
#ifndef DISABLE_EMBEDDED_BANKS
    static const char notes[] = "zsxdcvgbhnjmq2w3er5t6y7ui9o0p";
    //                           c'd'ef'g'a'bC'D'EF'G'A'Bc'd'e
    switch(ch)
    {
    case '/':
    case 'H':
    case 'A':
        NextAdl(-1);
        break;
    case '*':
    case 'P':
    case 'B':
        NextAdl(+1);
        break;
    case '-':
    case 'K':
    case 'D':
        NextGM(-1);
        break;
    case '+':
    case 'M':
    case 'C':
        NextGM(+1);
        break;
    case 3:
#if !((!defined(__WIN32__) || defined(__CYGWIN__)) && !defined(__DJGPP__))
    case 27:
#endif
        return false;
    default:
        const char *p = std::strchr(notes, ch);
        if(p && *p)
            DoNote((int)(p - notes) - 12);
    }
#else
    ADL_UNUSED(ch);
#endif
    return true;
}

#endif /* ADLMIDI_DISABLE_CPP_EXTRAS */

// Implement the user map data structure.

bool MIDIplay::AdlChannel::users_empty() const
{
    return !users_first;
}

MIDIplay::AdlChannel::LocationData *MIDIplay::AdlChannel::users_find(Location loc)
{
    LocationData *user = NULL;
    for(LocationData *curr = users_first; !user && curr; curr = curr->next)
        if(curr->loc == loc)
            user = curr;
    return user;
}

MIDIplay::AdlChannel::LocationData *MIDIplay::AdlChannel::users_allocate()
{
    // remove free cells front
    LocationData *user = users_free_cells;
    if(!user)
        return NULL;
    users_free_cells = user->next;
    if(users_free_cells)
        users_free_cells->prev = NULL;
    // add to users front
    if(users_first)
        users_first->prev = user;
    user->prev = NULL;
    user->next = users_first;
    users_first = user;
    ++users_size;
    return user;
}

MIDIplay::AdlChannel::LocationData *MIDIplay::AdlChannel::users_find_or_create(Location loc)
{
    LocationData *user = users_find(loc);
    if(!user)
    {
        user = users_allocate();
        if(!user)
            return NULL;
        LocationData *prev = user->prev, *next = user->next;
        *user = LocationData();
        user->prev = prev;
        user->next = next;
        user->loc = loc;
    }
    return user;
}

MIDIplay::AdlChannel::LocationData *MIDIplay::AdlChannel::users_insert(const LocationData &x)
{
    LocationData *user = users_find(x.loc);
    if(!user)
    {
        user = users_allocate();
        if(!user)
            return NULL;
        LocationData *prev = user->prev, *next = user->next;
        *user = x;
        user->prev = prev;
        user->next = next;
    }
    return user;
}

void MIDIplay::AdlChannel::users_erase(LocationData *user)
{
    if(user->prev)
        user->prev->next = user->next;
    if(user->next)
        user->next->prev = user->prev;
    if(user == users_first)
        users_first = user->next;
    user->prev = NULL;
    user->next = users_free_cells;
    users_free_cells = user;
    --users_size;
}

void MIDIplay::AdlChannel::users_clear()
{
    users_first = NULL;
    users_free_cells = users_cells;
    users_size = 0;
    for(size_t i = 0; i < users_max; ++i)
    {
        users_cells[i].prev = (i > 0) ? &users_cells[i - 1] : NULL;
        users_cells[i].next = (i + 1 < users_max) ? &users_cells[i + 1] : NULL;
    }
}

void MIDIplay::AdlChannel::users_assign(const LocationData *users, size_t count)
{
    ADL_UNUSED(count);//Avoid warning for release builds
    assert(count <= users_max);
    if(users == users_first && users)
    {
        // self assignment
        assert(users_size == count);
        return;
    }
    users_clear();
    const LocationData *src_cell = users;
    // move to the last
    if(src_cell)
    {
        while(src_cell->next)
            src_cell = src_cell->next;
    }
    // push cell copies in reverse order
    while(src_cell)
    {
        LocationData *dst_cell = users_allocate();
        assert(dst_cell);
        LocationData *prev = dst_cell->prev, *next = dst_cell->next;
        *dst_cell = *src_cell;
        dst_cell->prev = prev;
        dst_cell->next = next;
        src_cell = src_cell->prev;
    }
    assert(users_size == count);
}
