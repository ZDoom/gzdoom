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

#include "opnmidi_midiplay.hpp"
#include "opnmidi_opn2.hpp"
#include "opnmidi_private.hpp"
#ifndef OPNMIDI_DISABLE_MIDI_SEQUENCER
#include "midi_sequencer.hpp"
#endif
#include "chips/opn_chip_base.h"

// Minimum life time of percussion notes
static const double drum_note_min_time = 0.03;

enum { MasterVolumeDefault = 127 };

inline bool isXgPercChannel(uint8_t msb, uint8_t lsb)
{
    ADL_UNUSED(lsb);
    return (msb == 0x7E || msb == 0x7F);
}

void OPNMIDIplay::OpnChannel::addAge(int64_t us)
{
    const int64_t neg = 1000 * static_cast<int64_t>(-0x1FFFFFFFl);
    if(users.empty())
    {
        koff_time_until_neglible_us = std::max(koff_time_until_neglible_us - us, neg);
        if(koff_time_until_neglible_us < 0)
            koff_time_until_neglible_us = 0;
    }
    else
    {
        koff_time_until_neglible_us = 0;
        for(users_iterator i = users.begin(); !i.is_end(); ++i)
        {
            LocationData &d = i->value;
            if(!d.fixed_sustain)
                d.kon_time_until_neglible_us = std::max(d.kon_time_until_neglible_us - us, neg);
            d.vibdelay_us += us;
        }
    }
}

OPNMIDIplay::OPNMIDIplay(unsigned long sampleRate) :
    m_sysExDeviceId(0),
    m_synthMode(Mode_XG),
    m_arpeggioCounter(0)
#if defined(ADLMIDI_AUDIO_TICK_HANDLER)
    , m_audioTickCounter(0)
#endif
{
    m_midiDevices.clear();

    m_setup.emulator = opn2_getLowestEmulator();
    m_setup.runAtPcmRate = false;

    m_setup.PCM_RATE = sampleRate;
    m_setup.mindelay = 1.0 / static_cast<double>(m_setup.PCM_RATE);
    m_setup.maxdelay = 512.0 / static_cast<double>(m_setup.PCM_RATE);

    m_setup.OpnBank    = 0;
    m_setup.numChips   = 2;
    m_setup.LogarithmicVolumes  = false;
    m_setup.VolumeModel = OPNMIDI_VolumeModel_AUTO;
    m_setup.lfoEnable = -1;
    m_setup.lfoFrequency = -1;
    m_setup.chipType = -1;
    //m_setup.SkipForward = 0;
    m_setup.ScaleModulators     = 0;
    m_setup.fullRangeBrightnessCC74 = false;
    m_setup.enableAutoArpeggio = false;
    m_setup.delay = 0.0;
    m_setup.carry = 0.0;
    m_setup.tick_skip_samples_delay = 0;

    m_synth.reset(new Synth);

#ifndef OPNMIDI_DISABLE_MIDI_SEQUENCER
    m_sequencer.reset(new MidiSequencer);
    initSequencerInterface();
#endif
    resetMIDI();
    applySetup();
    realTime_ResetState();
}

OPNMIDIplay::~OPNMIDIplay()
{
}

void OPNMIDIplay::applySetup()
{
    Synth &synth = *m_synth;

    synth.m_musicMode             = Synth::MODE_MIDI;

    m_setup.tick_skip_samples_delay = 0;

    synth.m_runAtPcmRate            = m_setup.runAtPcmRate;

    synth.m_scaleModulators         = (m_setup.ScaleModulators != 0);

    if(m_setup.LogarithmicVolumes != 0)
        synth.setVolumeScaleModel(OPNMIDI_VolumeModel_NativeOPN2);
    else
        synth.setVolumeScaleModel(static_cast<OPNMIDI_VolumeModels>(m_setup.VolumeModel));

    if(m_setup.VolumeModel == OPNMIDI_VolumeModel_AUTO)
        synth.m_volumeScale = static_cast<Synth::VolumesScale>(synth.m_insBankSetup.volumeModel);

    synth.m_numChips    = m_setup.numChips;

    if(m_setup.lfoEnable < 0)
        synth.m_lfoEnable = (synth.m_insBankSetup.lfoEnable != 0);
    else
        synth.m_lfoEnable = (m_setup.lfoEnable != 0);

    if(m_setup.lfoFrequency < 0)
        synth.m_lfoFrequency = static_cast<uint8_t>(synth.m_insBankSetup.lfoFrequency);
    else
        synth.m_lfoFrequency = static_cast<uint8_t>(m_setup.lfoFrequency);

    int chipType;
    if(m_setup.chipType < 0)
        chipType = synth.m_insBankSetup.chipType;
    else
        chipType = m_setup.chipType;

    synth.reset(m_setup.emulator, m_setup.PCM_RATE, static_cast<OPNFamily>(chipType), this);
    m_chipChannels.clear();
    m_chipChannels.resize(synth.m_numChannels, OpnChannel());
    resetMIDIDefaults();
#if defined(OPNMIDI_MIDI2VGM) && !defined(OPNMIDI_DISABLE_MIDI_SEQUENCER)
    m_sequencerInterface->onloopStart = synth.m_loopStartHook;
    m_sequencerInterface->onloopStart_userData = synth.m_loopStartHookData;
    m_sequencerInterface->onloopEnd = synth.m_loopEndHook;
    m_sequencerInterface->onloopEnd_userData = synth.m_loopEndHookData;
    m_sequencer->setLoopHooksOnly(m_sequencerInterface->onloopStart != NULL);
#endif
    // Reset the arpeggio counter
    m_arpeggioCounter = 0;
}

void OPNMIDIplay::partialReset()
{
    Synth &synth = *m_synth;
    realTime_panic();
    m_setup.tick_skip_samples_delay = 0;
    synth.m_runAtPcmRate = m_setup.runAtPcmRate;
    synth.reset(m_setup.emulator, m_setup.PCM_RATE, synth.chipFamily(), this);
    m_chipChannels.clear();
    m_chipChannels.resize(synth.m_numChannels);
    resetMIDIDefaults();
#if defined(OPNMIDI_MIDI2VGM) && !defined(OPNMIDI_DISABLE_MIDI_SEQUENCER)
    m_sequencerInterface->onloopStart = synth.m_loopStartHook;
    m_sequencerInterface->onloopStart_userData = synth.m_loopStartHookData;
    m_sequencerInterface->onloopEnd = synth.m_loopEndHook;
    m_sequencerInterface->onloopEnd_userData = synth.m_loopEndHookData;
    m_sequencer->setLoopHooksOnly(m_sequencerInterface->onloopStart != NULL);
#endif
}

void OPNMIDIplay::resetMIDI()
{
    Synth &synth = *m_synth;
    synth.m_masterVolume = MasterVolumeDefault;
    m_sysExDeviceId = 0;
    m_synthMode = Mode_XG;
    m_arpeggioCounter = 0;

    m_midiChannels.clear();
    m_midiChannels.resize(16, MIDIchannel());

    resetMIDIDefaults();

    caugh_missing_instruments.clear();
    caugh_missing_banks_melodic.clear();
    caugh_missing_banks_percussion.clear();
}

void OPNMIDIplay::resetMIDIDefaults(int offset)
{
    Synth &synth = *m_synth;

    for(size_t c = offset, n = m_midiChannels.size(); c < n; ++c)
    {
        MIDIchannel &ch = m_midiChannels[c];

        if(synth.m_musicMode == Synth::MODE_RSXX)
            ch.def_volume = 127;
        else if(synth.m_insBankSetup.mt32defaults)
        {
            ch.def_volume = 127;
            ch.def_bendsense_lsb = 0;
            ch.def_bendsense_msb = 12;
        }
    }
}

void OPNMIDIplay::TickIterators(double s)
{
    Synth &synth = *m_synth;
    for(uint32_t c = 0, n = synth.m_numChannels; c < n; ++c)
    {
        OpnChannel &ch = m_chipChannels[c];
        ch.addAge(static_cast<int64_t>(s * 1e6));
    }

    // Resolve "hell of all times" of too short drum notes
    for(size_t c = 0, n = m_midiChannels.size(); c < n; ++c)
    {
        MIDIchannel &ch = m_midiChannels[c];
        if(ch.extended_note_count == 0)
            continue;

        for(MIDIchannel::notes_iterator inext = ch.activenotes.begin(); !inext.is_end();)
        {
            MIDIchannel::notes_iterator i(inext++);
            MIDIchannel::NoteInfo &ni = i->value;

            double ttl = ni.ttl;
            if(ttl <= 0)
                continue;

            ni.ttl = ttl = ttl - s;
            if(ttl <= 0)
            {
                --ch.extended_note_count;
                if(ni.isOnExtendedLifeTime)
                {
                    noteUpdate(c, i, Upd_Off);
                    ni.isOnExtendedLifeTime = false;
                }
            }
        }
    }

    updateVibrato(s);
    updateArpeggio(s);
#if !defined(ADLMIDI_AUDIO_TICK_HANDLER)
    updateGlide(s);
#endif
}

void OPNMIDIplay::realTime_ResetState()
{
    Synth &synth = *m_synth;
    for(size_t ch = 0; ch < m_midiChannels.size(); ch++)
    {
        MIDIchannel &chan = m_midiChannels[ch];
        chan.resetAllControllers();
        chan.vibpos = 0.0;
        chan.lastlrpn = 0;
        chan.lastmrpn = 0;
        chan.nrpn = false;
        if((m_synthMode & Mode_GS) != 0)// Reset custom drum channels on GS
            chan.is_xg_percussion = false;
        noteUpdateAll(uint16_t(ch), Upd_All);
        noteUpdateAll(uint16_t(ch), Upd_Off);
    }
    synth.m_masterVolume = MasterVolumeDefault;
}

bool OPNMIDIplay::realTime_NoteOn(uint8_t channel, uint8_t note, uint8_t velocity)
{
    Synth &synth = *m_synth;

    if(note >= 127)
        note = 127;

    if((synth.m_musicMode == Synth::MODE_RSXX) && (velocity != 0))
    {
        // Check if this is just a note after-touch
        MIDIchannel::notes_iterator i = m_midiChannels[channel].find_activenote(note);
        if(!i.is_end())
        {
            MIDIchannel::NoteInfo &ni = i->value;
            const int veloffset = ni.ains ? ni.ains->midiVelocityOffset : 0;
            velocity = static_cast<uint8_t>(std::min(127, std::max(1, static_cast<int>(velocity) + veloffset)));
            ni.vol = velocity;
            if(ni.ains)
                noteUpdate(channel, i, Upd_Volume);
            return false;
        }
    }

    if(static_cast<size_t>(channel) > m_midiChannels.size())
        channel = channel % 16;

    // noteOff(channel, note, velocity != 0);
    // On Note on, Keyoff the note first, just in case keyoff
    // was omitted; this fixes Dance of sugar-plum fairy
    // by Microsoft. Now that we've done a Keyoff,
    // check if we still need to do a Keyon.
    // vol=0 and event 8x are both Keyoff-only.
    if(velocity == 0)
    {
        noteOff(channel, note, false);
        return false;
    }

    MIDIchannel &midiChan = m_midiChannels[channel];

    size_t midiins = midiChan.patch;
    bool isPercussion = (channel % 16 == 9) || midiChan.is_xg_percussion;

    size_t bank = 0;
    if(midiChan.bank_msb || midiChan.bank_lsb)
    {
        if((m_synthMode & Mode_GS) != 0) //in GS mode ignore LSB
            bank = (midiChan.bank_msb * 256);
        else
            bank = (midiChan.bank_msb * 256) + midiChan.bank_lsb;
    }

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
            bank = midiins + ((midiChan.bank_msb == 0x7E) ? 128 : 0);
        }
        else
        {
            bank = midiins;
        }
        midiins = note; // Percussion instrument
    }

    if(isPercussion)
        bank += Synth::PercussionTag;

    const OpnInstMeta *ains = &Synth::m_emptyInstrument;

    //Set bank bank
    const Synth::Bank *bnk = NULL;
    bool caughtMissingBank = false;
    if((bank & ~static_cast<uint16_t>(Synth::PercussionTag)) > 0)
    {
        Synth::BankMap::iterator b = synth.m_insBanks.find(bank);
        if(b != synth.m_insBanks.end())
            bnk = &b->second;
        if(bnk)
            ains = &bnk->ins[midiins];
        else
            caughtMissingBank = true;
    }

    //Or fall back to bank ignoring LSB (GS/XG)
    if(ains->flags & OpnInstMeta::Flag_NoSound)
    {
        size_t fallback = bank & ~(size_t)0x7F;
        if(fallback != bank)
        {
            Synth::BankMap::iterator b = synth.m_insBanks.find(fallback);
            caughtMissingBank = false;
            if(b != synth.m_insBanks.end())
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
                                 channel, text, (bank & ~static_cast<uint16_t>(Synth::PercussionTag)), midiins);
        }
    }

    //Or fall back to first bank
    if((ains->flags & OpnInstMeta::Flag_NoSound) != 0)
    {
        Synth::BankMap::iterator b = synth.m_insBanks.find(bank & Synth::PercussionTag);
        if(b != synth.m_insBanks.end())
            bnk = &b->second;
        if(bnk)
            ains = &bnk->ins[midiins];
    }

    const int veloffset = ains->midiVelocityOffset;
    velocity = static_cast<uint8_t>(std::min(127, std::max(1, static_cast<int>(velocity) + veloffset)));

    int32_t tone = note;
    if(!isPercussion && (bank > 0)) // For non-zero banks
    {
        if(ains->flags & OpnInstMeta::Flag_NoSound)
        {
            if(hooks.onDebugMessage)
            {
                if(!caugh_missing_instruments.count(static_cast<uint8_t>(midiins)))
                {
                    hooks.onDebugMessage(hooks.onDebugMessage_userData, "[%i] Caught a blank instrument %i (offset %i) in the MIDI bank %u", channel, midiChan.patch, midiins, bank);
                    caugh_missing_instruments.insert(static_cast<uint8_t>(midiins));
                }
            }
            bank = 0;
            midiins = midiChan.patch;
        }
    }

    if(ains->drumTone)
    {
        if(ains->drumTone >= 128)
            tone = ains->drumTone - 128;
        else
            tone = ains->drumTone;
    }

    MIDIchannel::NoteInfo::Phys voices[MIDIchannel::NoteInfo::MaxNumPhysChans] = {
        {0, ains->op[0], /*false*/},
        {0, ains->op[1], /*pseudo_4op*/},
    };
    //bool pseudo_4op = ains.flags & opnInstMeta::Flag_Pseudo8op;
    //if((opn.AdlPercussionMode == 1) && PercussionMap[midiins & 0xFF]) i[1] = i[0];

    bool isBlankNote = (ains->flags & OpnInstMeta::Flag_NoSound) != 0;

    if(hooks.onDebugMessage)
    {
        if(!caugh_missing_instruments.count(static_cast<uint8_t>(midiins)) && isBlankNote)
        {
            hooks.onDebugMessage(hooks.onDebugMessage_userData, "[%i] Playing missing instrument %i", channel, midiins);
            caugh_missing_instruments.insert(static_cast<uint8_t>(midiins));
        }
    }

    if(isBlankNote)
    {
        // Don't even try to play the blank instrument! But, insert the dummy note.
        if(midiChan.activenotes.size() >= midiChan.activenotes.capacity())
            return false; // Overflow!

        MIDIchannel::notes_iterator i = midiChan.ensure_create_activenote(note);
        MIDIchannel::NoteInfo &dummy = i->value;
        dummy.isBlank = true;
        dummy.isOnExtendedLifeTime = false;
        dummy.ttl = 0;
        dummy.ains = NULL;
        dummy.chip_channels_count = 0;
        // Record the last note on MIDI channel as source of portamento
        midiChan.portamentoSource = static_cast<int8_t>(note);
        return false;
    }

    // Allocate OPN2 channel (the physical sound channel for the note)
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

        for(size_t a = 0; a < static_cast<size_t>(synth.m_numChannels); ++a)
        {
            if(ccount == 1 && static_cast<int32_t>(a) == adlchannel[0]) continue;
            // ^ Don't use the same channel for primary&secondary
            // ===== Kept for future pseudo-8-op mode
            //if(voices[0] == voices[1] || pseudo_4op)
            //{
            //    // Only use regular channels
            //    uint8_t expected_mode = 0;
            //    if(opn.AdlPercussionMode == 1)
            //    {
            //        if(cmf_percussion_mode)
            //            expected_mode = MidCh < 11 ? 0 : (3 + MidCh - 11); // CMF
            //        else
            //            expected_mode = PercussionMap[midiins & 0xFF];
            //    }
            //    if(opn.four_op_category[a] != expected_mode)
            //        continue;
            //}
            int64_t s = calculateChipChannelGoodness(a, voices[ccount]);
            if(s > bs)
            {
                bs = static_cast<int32_t>(s);    // Best candidate wins
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
    if(midiChan.activenotes.size() >= midiChan.activenotes.capacity())
        return false; // Overflow!

    MIDIchannel::notes_iterator ir = midiChan.ensure_create_activenote(note);
    MIDIchannel::NoteInfo &ni = ir->value;
    ni.vol     = velocity;
    ni.vibrato = midiChan.noteAftertouch[note];
    ni.noteTone = static_cast<int16_t>(tone);
    ni.currentTone = tone;
    ni.glideRate = HUGE_VAL;
    ni.midiins = midiins;
    ni.isPercussion = isPercussion;
    ni.isBlank = isBlankNote;
    ni.isOnExtendedLifeTime = false;
    ni.ttl = 0;
    ni.ains = ains;
    ni.chip_channels_count = 0;

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
        ni.currentTone = currentPortamentoSource;
        ni.glideRate = currentPortamentoRate;
        ++midiChan.gliding_note_count;
    }

    // Enable life time extension on percussion note
    if (isPercussion)
    {
        ni.ttl = drum_note_min_time;
        ++midiChan.extended_note_count;
    }

    for(unsigned ccount = 0; ccount < MIDIchannel::NoteInfo::MaxNumPhysChans; ++ccount)
    {
        int32_t c = adlchannel[ccount];
        if(c < 0)
            continue;
        uint16_t chipChan = static_cast<uint16_t>(adlchannel[ccount]);
        ni.phys_ensure_find_or_create(chipChan)->assign(voices[ccount]);
    }

    noteUpdate(channel, ir, Upd_All | Upd_Patch);

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

void OPNMIDIplay::realTime_NoteOff(uint8_t channel, uint8_t note)
{
    if(static_cast<size_t>(channel) > m_midiChannels.size())
        channel = channel % 16;
    noteOff(channel, note);
}

void OPNMIDIplay::realTime_NoteAfterTouch(uint8_t channel, uint8_t note, uint8_t atVal)
{
    if(static_cast<size_t>(channel) > m_midiChannels.size())
        channel = channel % 16;
    MIDIchannel &chan = m_midiChannels[channel];
    MIDIchannel::notes_iterator i = m_midiChannels[channel].find_activenote(note);
    if(!i.is_end())
    {
        i->value.vibrato = atVal;
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

void OPNMIDIplay::realTime_ChannelAfterTouch(uint8_t channel, uint8_t atVal)
{
    if(static_cast<size_t>(channel) > m_midiChannels.size())
        channel = channel % 16;
    m_midiChannels[channel].aftertouch = atVal;
}

void OPNMIDIplay::realTime_Controller(uint8_t channel, uint8_t type, uint8_t value)
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
            killSustainingNotes(channel, -1, OpnChannel::LocationData::Sustain_Pedal);
        break;

    case 66: // Enable/disable sostenuto
        if(value >= 64) //Find notes and mark them as sostenutoed
            markSostenutoNotes(channel);
        else
            killSustainingNotes(channel, -1, OpnChannel::LocationData::Sustain_Sostenuto);
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
        m_midiChannels[channel].resetAllControllers121();
        noteUpdateAll(channel, Upd_Pan + Upd_Volume + Upd_Pitch);
        // Kill all sustained notes
        killSustainingNotes(channel, -1, OpnChannel::LocationData::Sustain_ANY);
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

    //case 103:
    //    cmf_percussion_mode = (value != 0);
    //    break; // CMF (ctrl 0x67) rhythm mode

    default:
        break;
        //UI.PrintLn("Ctrl %d <- %d (ch %u)", ctrlno, value, MidCh);
    }
}

void OPNMIDIplay::realTime_PatchChange(uint8_t channel, uint8_t patch)
{
    if(static_cast<size_t>(channel) > m_midiChannels.size())
        channel = channel % 16;
    m_midiChannels[channel].patch = patch;
}

void OPNMIDIplay::realTime_PitchBend(uint8_t channel, uint16_t pitch)
{
    if(static_cast<size_t>(channel) > m_midiChannels.size())
        channel = channel % 16;
    m_midiChannels[channel].bend = int(pitch) - 8192;
    noteUpdateAll(channel, Upd_Pitch);
}

void OPNMIDIplay::realTime_PitchBend(uint8_t channel, uint8_t msb, uint8_t lsb)
{
    if(static_cast<size_t>(channel) > m_midiChannels.size())
        channel = channel % 16;
    m_midiChannels[channel].bend = int(lsb) + int(msb) * 128 - 8192;
    noteUpdateAll(channel, Upd_Pitch);
}

void OPNMIDIplay::realTime_BankChangeLSB(uint8_t channel, uint8_t lsb)
{
    if(static_cast<size_t>(channel) > m_midiChannels.size())
        channel = channel % 16;
    m_midiChannels[channel].bank_lsb = lsb;
}

void OPNMIDIplay::realTime_BankChangeMSB(uint8_t channel, uint8_t msb)
{
    if(static_cast<size_t>(channel) > m_midiChannels.size())
        channel = channel % 16;
    m_midiChannels[channel].bank_msb = msb;
}

void OPNMIDIplay::realTime_BankChange(uint8_t channel, uint16_t bank)
{
    if(static_cast<size_t>(channel) > m_midiChannels.size())
        channel = channel % 16;
    m_midiChannels[channel].bank_lsb = uint8_t(bank & 0xFF);
    m_midiChannels[channel].bank_msb = uint8_t((bank >> 8) & 0xFF);
}

void OPNMIDIplay::setDeviceId(uint8_t id)
{
    m_sysExDeviceId = id;
}

bool OPNMIDIplay::realTime_SysEx(const uint8_t *msg, size_t size)
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

bool OPNMIDIplay::doUniversalSysEx(unsigned dev, bool realtime, const uint8_t *data, size_t size)
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
            if(m_synth.get())
                m_synth->m_masterVolume = static_cast<uint8_t>(volume >> 7);
            for(size_t ch = 0; ch < m_midiChannels.size(); ch++)
                noteUpdateAll(uint16_t(ch), Upd_Volume);
            return true;
    }

    return false;
}

bool OPNMIDIplay::doRolandSysEx(unsigned dev, const uint8_t *data, size_t size)
{
    bool devicematch = dev == 0x7F || (dev & 0x0F) == m_sysExDeviceId;
    if(size < 6 || !devicematch)
        return false;

    unsigned model = data[0] & 0x7F;
    unsigned mode = data[1] & 0x7F;
    unsigned checksum = data[size - 1] & 0x7F;
    data += 2;
    size -= 3;

#if !defined(OPNMIDI_SKIP_ROLAND_CHECKSUM)
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
        unsigned gs_mode = data[0] & 0x7F;
        ADL_UNUSED(gs_mode);//TODO: Hook this correctly!
        if(hooks.onDebugMessage)
            hooks.onDebugMessage(hooks.onDebugMessage_userData, "SysEx: Caught Roland System Mode Set: %02X", gs_mode);
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

bool OPNMIDIplay::doYamahaSysEx(unsigned dev, const uint8_t *data, size_t size)
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

void OPNMIDIplay::realTime_panic()
{
    panic();
    killSustainingNotes(-1, -1, OpnChannel::LocationData::Sustain_ANY);
}

void OPNMIDIplay::realTime_deviceSwitch(size_t track, const char *data, size_t length)
{
    const std::string indata(data, length);
    m_currentMidiDevice[track] = chooseDevice(indata);
}

size_t OPNMIDIplay::realTime_currentDevice(size_t track)
{
    if(m_currentMidiDevice.empty())
        return 0;
    return m_currentMidiDevice[track];
}

#if defined(ADLMIDI_AUDIO_TICK_HANDLER)
void OPNMIDIplay::AudioTick(uint32_t chipId, uint32_t rate)
{
    if(chipId != 0)  // do first chip ticks only
        return;

    uint32_t tickNumber = m_audioTickCounter++;
    double timeDelta = 1.0 / rate;

    enum { portamentoInterval = 32 }; // for efficiency, set rate limit on pitch updates

    if(tickNumber % portamentoInterval == 0)
    {
        double portamentoDelta = timeDelta * portamentoInterval;
        UpdateGlide(portamentoDelta);
    }
}
#endif

void OPNMIDIplay::noteUpdate(size_t midCh,
                          OPNMIDIplay::MIDIchannel::notes_iterator i,
                          unsigned props_mask,
                          int32_t select_adlchn)
{
    Synth &synth = *m_synth;
    MIDIchannel::NoteInfo &info = i->value;
    const int16_t noteTone    = info.noteTone;
    const double currentTone    = info.currentTone;
    const uint8_t vol     = info.vol;
    const size_t midiins = info.midiins;
    const OpnInstMeta &ains = *info.ains;
    OpnChannel::Location my_loc;
    my_loc.MidCh = static_cast<uint16_t>(midCh);
    my_loc.note  = info.note;

    if(info.isBlank)
    {
        if(props_mask & Upd_Off)
            m_midiChannels[midCh].activenotes.erase(i);
        return;
    }

    for(unsigned ccount = 0, ctotal = info.chip_channels_count; ccount < ctotal; ccount++)
    {
        const MIDIchannel::NoteInfo::Phys &ins = info.chip_channels[ccount];
        uint16_t c   = ins.chip_chan;

        if(select_adlchn >= 0 && c != select_adlchn) continue;

        if(props_mask & Upd_Patch)
        {
            synth.setPatch(c, ins.ains);
            OpnChannel::users_iterator ci = m_chipChannels[c].find_or_create_user(my_loc);
            if(!ci.is_end())    // inserts if necessary
            {
                OpnChannel::LocationData &d = ci->value;
                d.sustained = OpnChannel::LocationData::Sustain_None;
                d.vibdelay_us  = 0;
                d.fixed_sustain = (ains.soundKeyOnMs == static_cast<uint16_t>(opnNoteOnMaxTime));
                d.kon_time_until_neglible_us = 1000 * ains.soundKeyOnMs;
                d.ins       = ins;
            }
        }
    }

    for(unsigned ccount = 0; ccount < info.chip_channels_count; ccount++)
    {
        const MIDIchannel::NoteInfo::Phys &ins = info.chip_channels[ccount];
        uint16_t c   = ins.chip_chan;

        if(select_adlchn >= 0 && c != select_adlchn)
            continue;

        if(props_mask & Upd_Off) // note off
        {
            if(m_midiChannels[midCh].sustain == 0)
            {
                OpnChannel::users_iterator k = m_chipChannels[c].find_user(my_loc);
                bool do_erase_user = (!k.is_end() && ((k->value.sustained & OpnChannel::LocationData::Sustain_Sostenuto) == 0));
                if(do_erase_user)
                    m_chipChannels[c].users.erase(k);

                if(hooks.onNote)
                    hooks.onNote(hooks.onNote_userData, c, noteTone, static_cast<int>(midiins), 0, 0.0);

                if(do_erase_user && m_chipChannels[c].users.empty())
                {
                    synth.noteOff(c);
                    if(props_mask & Upd_Mute) // Mute the note
                    {
                        synth.touchNote(c, 0);
                        m_chipChannels[c].koff_time_until_neglible_us = 0;
                    }
                    else
                    {
                        m_chipChannels[c].koff_time_until_neglible_us = 1000 * int64_t(ains.soundKeyOffMs);
                    }
                }
            }
            else
            {
                // Sustain: Forget about the note, but don't key it off.
                //          Also will avoid overwriting it very soon.
                OpnChannel::users_iterator d = m_chipChannels[c].find_or_create_user(my_loc);
                if(!d.is_end())
                    d->value.sustained |= OpnChannel::LocationData::Sustain_Pedal; // note: not erased!
                if(hooks.onNote)
                    hooks.onNote(hooks.onNote_userData, c, noteTone, static_cast<int>(midiins), -1, 0.0);
            }

            info.phys_erase_at(&ins);  // decrements channel count
            --ccount;  // adjusts index accordingly
            continue;
        }

        if(props_mask & Upd_Pan)
            synth.setPan(c, m_midiChannels[midCh].panning);

        if(props_mask & Upd_Volume)
        {
            const MIDIchannel &ch = m_midiChannels[midCh];
            bool is_percussion = (midCh == 9) || ch.is_xg_percussion;
            uint_fast32_t brightness = is_percussion ? 127 : ch.brightness;

            if(!m_setup.fullRangeBrightnessCC74)
            {
                // Simulate post-High-Pass filter result which affects sounding by half level only
                if(brightness >= 64)
                    brightness = 127;
                else
                    brightness *= 2;
            }

            synth.touchNote(c,
                            vol,
                            ch.volume,
                            ch.expression,
                            static_cast<uint8_t>(brightness));
        }

        if(props_mask & Upd_Pitch)
        {
            OpnChannel::users_iterator d = m_chipChannels[c].find_user(my_loc);

            // Don't bend a sustained note
            if(d.is_end() || (d->value.sustained == OpnChannel::LocationData::Sustain_None))
            {
                MIDIchannel &chan = m_midiChannels[midCh];
                double midibend = chan.bend * chan.bendsense;
                double bend = midibend + ins.ains.noteOffset;
                double phase = 0.0;
                uint8_t vibrato = std::max(chan.vibrato, chan.aftertouch);

                vibrato = std::max(vibrato, info.vibrato);

                if((ains.flags & OpnInstMeta::Flag_Pseudo8op) && ins.ains == ains.op[1])
                {
                    phase = ains.voice2_fine_tune;
                }

                if(vibrato && (d.is_end() || d->value.vibdelay_us >= chan.vibdelay_us))
                    bend += static_cast<double>(vibrato) * chan.vibdepth * std::sin(chan.vibpos);

                synth.noteOn(c, currentTone + bend + phase);

                if(hooks.onNote)
                    hooks.onNote(hooks.onNote_userData, c, noteTone, static_cast<int>(midiins), vol, midibend);
            }
        }
    }

    if(info.chip_channels_count == 0)
    {
        m_midiChannels[midCh].cleanupNote(i);
        m_midiChannels[midCh].activenotes.erase(i);
    }
}

void OPNMIDIplay::noteUpdateAll(size_t midCh, unsigned props_mask)
{
    for(MIDIchannel::notes_iterator
        i = m_midiChannels[midCh].activenotes.begin(); !i.is_end();)
    {
        MIDIchannel::notes_iterator j(i++);
        noteUpdate(midCh, j, props_mask);
    }
}

const std::string &OPNMIDIplay::getErrorString()
{
    return errorStringOut;
}

void OPNMIDIplay::setErrorString(const std::string &err)
{
    errorStringOut = err;
}

int64_t OPNMIDIplay::calculateChipChannelGoodness(size_t c, const MIDIchannel::NoteInfo::Phys &ins) const
{
    Synth &synth = *m_synth;
    const OpnChannel &chan = m_chipChannels[c];
    int64_t koff_ms = chan.koff_time_until_neglible_us / 1000;
    int64_t s = -koff_ms;
    OPNMIDI_ChannelAlloc allocType = synth.m_channelAlloc;

    if(allocType == OPNMIDI_ChanAlloc_AUTO)
    {
        if(synth.m_musicMode == Synth::MODE_CMF)
            allocType = OPNMIDI_ChanAlloc_SameInst;
        else
            allocType = OPNMIDI_ChanAlloc_OffDelay;
    }

    // Rate channel with a releasing note
    if(s < 0 && chan.users.empty())
    {
        bool isSame = (chan.recent_ins == ins);
        s -= 40000;

        // If it's same instrument, better chance to get it when no free channels
        switch(allocType)
        {
        case OPNMIDI_ChanAlloc_SameInst:
            if(isSame)
                s = 0; // Re-use releasing channel with the same instrument
            break;
        case OPNMIDI_ChanAlloc_AnyReleased:
            s = 0; // Re-use any releasing channel
            break;
        default:
        case OPNMIDI_ChanAlloc_OffDelay:
            if(isSame)
                s =  -koff_ms; // Wait until releasing sound will complete
            break;
        }

        return s;
    }

    // Same midi-instrument = some stability
    for(OpnChannel::const_users_iterator j = chan.users.begin(); !j.is_end(); ++j)
    {
        const OpnChannel::LocationData &jd = j->value;

        int64_t kon_ms = jd.kon_time_until_neglible_us / 1000;
        s -= (jd.sustained == OpnChannel::LocationData::Sustain_None) ?
            (4000000 + kon_ms) : (500000 + (kon_ms / 2));

        MIDIchannel::notes_iterator
        k = const_cast<MIDIchannel &>(m_midiChannels[jd.loc.MidCh]).find_activenote(jd.loc.note);

        if(!k.is_end())
        {
            const MIDIchannel::NoteInfo &info = k->value;

            // Same instrument = good
            if(jd.ins == ins)
            {
                s += 300;
                // Arpeggio candidate = even better
                if(jd.vibdelay_us < 70000
                   || jd.kon_time_until_neglible_us > 20000000)
                    s += 10;
            }

            // Percussion is inferior to melody
            s += info.isPercussion ? 50 : 0;
        }

        // If there is another channel to which this note
        // can be evacuated to in the case of congestion,
        // increase the score slightly.
//        unsigned n_evacuation_stations = 0;

//        for(unsigned c2 = 0; c2 < opn.NumChannels; ++c2)
//        {
//            if(c2 == c) continue;

//            if(opn.four_op_category[c2]
//               != opn.four_op_category[c]) continue;

//            for(AdlChannel::const_users_iterator m = m_chipChannels[c2].users.begin(); !m.is_end(); ++m)
//            {
//                const AdlChannel::LocationData &md = m->value;
//                if(md.sustained != OpnChannel::LocationData::Sustain_None) continue;
//                if(md.vibdelay >= 200000) continue;
//                if(md.ins != jd.ins) continue;
//                n_evacuation_stations += 1;
//            }
//        }

//        s += n_evacuation_stations * 4;
    }

    return s;
}


void OPNMIDIplay::prepareChipChannelForNewNote(size_t c, const MIDIchannel::NoteInfo::Phys &ins)
{
    if(m_chipChannels[c].users.empty()) return; // Nothing to do

    Synth &synth = *m_synth;

    if(!m_setup.enableAutoArpeggio)
    {
        // Kill all notes on this channel with no mercy
        for(OpnChannel::users_iterator jnext = m_chipChannels[c].users.begin(); !jnext.is_end();)
        {
            OpnChannel::users_iterator j = jnext;
            OpnChannel::LocationData &jd = j->value;
            ++jnext;

            m_midiChannels[jd.loc.MidCh].clear_all_phys_users(c);
            m_chipChannels[c].users.erase(j);
        }

        synth.noteOff(c);
        assert(m_chipChannels[c].users.empty()); // No users should remain!

        return;
    }

    //bool doing_arpeggio = false;
    for(OpnChannel::users_iterator jnext = m_chipChannels[c].users.begin(); !jnext.is_end();)
    {
        OpnChannel::users_iterator j = jnext;
        OpnChannel::LocationData &jd = jnext->value;
        ++jnext;

        if(jd.sustained == OpnChannel::LocationData::Sustain_None)
        {
            // Collision: Kill old note,
            // UNLESS we're going to do arpeggio
            MIDIchannel::notes_iterator i
            (m_midiChannels[jd.loc.MidCh].ensure_find_activenote(jd.loc.note));

            // Check if we can do arpeggio.
            if((jd.vibdelay_us < 70000 || jd.kon_time_until_neglible_us > 20000000) && jd.ins == ins)
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
    killSustainingNotes(-1, static_cast<int32_t>(c), OpnChannel::LocationData::Sustain_ANY);

    // Keyoff the channel so that it can be retriggered,
    // unless the new note will be introduced as just an arpeggio.
    if(m_chipChannels[c].users.empty())
        synth.noteOff(c);
}

void OPNMIDIplay::killOrEvacuate(size_t from_channel,
                                 OpnChannel::users_iterator j,
                                 OPNMIDIplay::MIDIchannel::notes_iterator i)
{
    Synth &synth = *m_synth;
    uint32_t maxChannels = OPN_MAX_CHIPS * 6;
    OpnChannel::LocationData &jd = j->value;
    MIDIchannel::NoteInfo &info = i->value;

    // Before killing the note, check if it can be
    // evacuated to another channel as an arpeggio
    // instrument. This helps if e.g. all channels
    // are full of strings and we want to do percussion.
    // FIXME: This does not care about four-op entanglements.
    for(uint32_t c = 0; c < synth.m_numChannels; ++c)
    {
        uint16_t cs = static_cast<uint16_t>(c);

        if(c >= maxChannels)
            break;
        if(c == from_channel)
            continue;
        //if(opn.four_op_category[c] != opn.four_op_category[from_channel])
        //    continue;

        OpnChannel &adlch = m_chipChannels[c];
        if(adlch.users.size() == adlch.users.capacity())
            continue;  // no room for more arpeggio on channel

        if(!m_chipChannels[cs].find_user(jd.loc).is_end())
            continue;  // channel already has this note playing (sustained)
                       // avoid introducing a duplicate location.

        for(OpnChannel::users_iterator m = adlch.users.begin(); !m.is_end(); ++m)
        {
            OpnChannel::LocationData &mv = m->value;

            if(mv.vibdelay_us >= 200000
               && mv.kon_time_until_neglible_us < 10000000) continue;
            if(mv.ins != jd.ins)
                continue;
            if(hooks.onNote)
            {
                hooks.onNote(hooks.onNote_userData,
                             static_cast<int>(from_channel),
                             info.noteTone,
                             static_cast<int>(info.midiins), 0, 0.0);
                hooks.onNote(hooks.onNote_userData,
                             static_cast<int>(c),
                             info.noteTone,
                             static_cast<int>(info.midiins),
                             info.vol, 0.0);
            }

            info.phys_erase(static_cast<uint16_t>(from_channel));
            info.phys_ensure_find_or_create(cs)->assign(jd.ins);
            m_chipChannels[cs].users.push_back(jd);
            m_chipChannels[from_channel].users.erase(j);
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
    noteUpdate(jd.loc.MidCh,
               i,
               Upd_Off,
               static_cast<int32_t>(from_channel));
}

void OPNMIDIplay::panic()
{
    for(uint8_t chan = 0; chan < m_midiChannels.size(); chan++)
    {
        for(uint8_t note = 0; note < 128; note++)
            realTime_NoteOff(chan, note);
    }
}

void OPNMIDIplay::killSustainingNotes(int32_t midCh, int32_t this_adlchn, uint32_t sustain_type)
{
    Synth &synth = *m_synth;
    uint32_t first = 0, last = synth.m_numChannels;

    if(this_adlchn >= 0)
    {
        first = static_cast<uint32_t>(this_adlchn);
        last = first + 1;
    }

    for(uint32_t c = first; c < last; ++c)
    {
        if(m_chipChannels[c].users.empty())
            continue; // Nothing to do

        for(OpnChannel::users_iterator jnext = m_chipChannels[c].users.begin(); !jnext.is_end();)
        {
            OpnChannel::users_iterator j = jnext;
            OpnChannel::LocationData &jd = j->value;
            ++jnext;

            if((midCh < 0 || jd.loc.MidCh == midCh)
                && ((jd.sustained & sustain_type) != 0))
            {
                int midiins = '?';
                if(hooks.onNote)
                    hooks.onNote(hooks.onNote_userData, static_cast<int>(c), jd.loc.note, midiins, 0, 0.0);
                jd.sustained &= ~sustain_type;
                if(jd.sustained == OpnChannel::LocationData::Sustain_None)
                    m_chipChannels[c].users.erase(j);//Remove only when note is clean from any holders
            }
        }

        // Keyoff the channel, if there are no users left.
        if(m_chipChannels[c].users.empty())
            synth.noteOff(c);
    }
}

void OPNMIDIplay::markSostenutoNotes(int32_t midCh)
{
    Synth &synth = *m_synth;
    uint32_t first = 0, last = synth.m_numChannels;
    for(uint32_t c = first; c < last; ++c)
    {
        if(m_chipChannels[c].users.empty())
            continue; // Nothing to do

        for(OpnChannel::users_iterator jnext = m_chipChannels[c].users.begin(); !jnext.is_end();)
        {
            OpnChannel::users_iterator j = jnext;
            OpnChannel::LocationData &jd = j->value;
            ++jnext;
            if((jd.loc.MidCh == midCh) && (jd.sustained == OpnChannel::LocationData::Sustain_None))
                jd.sustained |= OpnChannel::LocationData::Sustain_Sostenuto;
        }
    }
}

void OPNMIDIplay::setRPN(size_t midCh, unsigned value, bool MSB)
{
    bool nrpn = m_midiChannels[midCh].nrpn;
    unsigned addr = m_midiChannels[midCh].lastmrpn * 0x100 + m_midiChannels[midCh].lastlrpn;

    switch(addr + nrpn * 0x10000 + MSB * 0x20000)
    {
    case 0x0000 + 0*0x10000 + 1*0x20000: // Pitch-bender sensitivity
        m_midiChannels[midCh].bendsense_msb = static_cast<int>(value);
        m_midiChannels[midCh].updateBendSensitivity();
        break;
    case 0x0000 + 0*0x10000 + 0*0x20000: // Pitch-bender sensitivity LSB
        m_midiChannels[midCh].bendsense_lsb = static_cast<int>(value);
        m_midiChannels[midCh].updateBendSensitivity();
        break;
    case 0x0108 + 1*0x10000 + 1*0x20000: // Vibrato speed
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
            m_midiChannels[midCh].vibdepth = ((static_cast<int>(value) - 64) * 0.15) * 0.01;
        }
        break;
    case 0x010A + 1*0x10000 + 1*0x20000:
        if((m_synthMode & Mode_XG) != 0) // Vibrato delay in millisecons
        {
            m_midiChannels[midCh].vibdelay_us = value ? int64_t(209.2 * std::exp(0.0795 * static_cast<double>(value))) : 0;
        }
        break;
    default:/* UI.PrintLn("%s %04X <- %d (%cSB) (ch %u)",
                "NRPN"+!nrpn, addr, value, "LM"[MSB], MidCh);*/
        break;
    }
}

void OPNMIDIplay::updatePortamento(size_t midCh)
{
    double rate = HUGE_VAL;
    uint16_t midival = m_midiChannels[midCh].portamento;
    if(m_midiChannels[midCh].portamentoEnable && midival > 0)
        rate = 350.0 * std::pow(2.0, -0.062 * (1.0 / 128) * midival);
    m_midiChannels[midCh].portamentoRate = rate;
}

void OPNMIDIplay::noteOff(size_t midCh, uint8_t note, bool forceNow)
{
    MIDIchannel &ch = m_midiChannels[midCh];
    MIDIchannel::notes_iterator i = ch.find_activenote(note);

    if(!i.is_end())
    {
        MIDIchannel::NoteInfo &ni = i->value;
        if(forceNow || ni.ttl <= 0)
            noteUpdate(midCh, i, Upd_Off);
        else
            ni.isOnExtendedLifeTime = true;
    }
}


void OPNMIDIplay::updateVibrato(double amount)
{
    for(size_t a = 0, b = m_midiChannels.size(); a < b; ++a)
    {
        if(m_midiChannels[a].hasVibrato() && !m_midiChannels[a].activenotes.empty())
        {
            noteUpdateAll(static_cast<uint16_t>(a), Upd_Pitch);
            m_midiChannels[a].vibpos += amount * m_midiChannels[a].vibspeed;
        }
        else
            m_midiChannels[a].vibpos = 0.0;
    }
}




size_t OPNMIDIplay::chooseDevice(const std::string &name)
{
    std::map<std::string, size_t>::iterator i = m_midiDevices.find(name);

    if(i != m_midiDevices.end())
        return i->second;

    size_t n = m_midiDevices.size() * 16;
    m_midiDevices.insert(std::make_pair(name, n));
    m_midiChannels.resize(n + 16);
    resetMIDIDefaults(static_cast<int>(n));
    return n;
}

void OPNMIDIplay::updateArpeggio(double) // amount = amount of time passed
{
    // If there is an adlib channel that has multiple notes
    // simulated on the same channel, arpeggio them.

    Synth &synth = *m_synth;

    if(!m_setup.enableAutoArpeggio) // Arpeggio was disabled
    {
        if(m_arpeggioCounter != 0)
            m_arpeggioCounter = 0;
        return;
    }

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

    for(uint32_t c = 0; c < synth.m_numChannels; ++c)
    {
retry_arpeggio:
        if(c > uint32_t(std::numeric_limits<int32_t>::max()))
            break;

        size_t n_users = m_chipChannels[c].users.size();

        if(n_users > 1)
        {
            OpnChannel::users_iterator i = m_chipChannels[c].users.begin();
            size_t rate_reduction = 3;

            if(n_users >= 3)
                rate_reduction = 2;

            if(n_users >= 4)
                rate_reduction = 1;

            for(size_t count = (m_arpeggioCounter / rate_reduction) % n_users,
                     n = 0; n < count; ++n)
                ++i;

            OpnChannel::LocationData &d = i->value;
            if(d.sustained == OpnChannel::LocationData::Sustain_None)
            {
                if(d.kon_time_until_neglible_us <= 0)
                {
                    noteUpdate(
                        d.loc.MidCh,
                        m_midiChannels[ d.loc.MidCh ].ensure_find_activenote(d.loc.note),
                        Upd_Off,
                        static_cast<int32_t>(c));
                    goto retry_arpeggio;
                }

                noteUpdate(
                    d.loc.MidCh,
                    m_midiChannels[ d.loc.MidCh ].ensure_find_activenote(d.loc.note),
                    Upd_Pitch | Upd_Volume | Upd_Pan,
                    static_cast<int32_t>(c));
            }
        }
    }
}

void OPNMIDIplay::updateGlide(double amount)
{
    size_t num_channels = m_midiChannels.size();

    for(size_t channel = 0; channel < num_channels; ++channel)
    {
        MIDIchannel &midiChan = m_midiChannels[channel];
        if(midiChan.gliding_note_count == 0)
            continue;

        for(MIDIchannel::notes_iterator it = midiChan.activenotes.begin();
            !it.is_end(); ++it)
        {
            MIDIchannel::NoteInfo &info = it->value;
            double finalTone = info.noteTone;
            double previousTone = info.currentTone;

            bool directionUp = previousTone < finalTone;
            double toneIncr = amount * (directionUp ? +info.glideRate : -info.glideRate);

            double currentTone = previousTone + toneIncr;
            bool glideFinished = !(directionUp ? (currentTone < finalTone) : (currentTone > finalTone));
            currentTone = glideFinished ? finalTone : currentTone;

            if(int64_t(currentTone * 1000000.0) != int64_t(previousTone * 1000000.0))
            {
                info.currentTone = currentTone;
                noteUpdate(static_cast<uint16_t>(channel), it, Upd_Pitch);
            }
        }
    }
}

void OPNMIDIplay::describeChannels(char *str, char *attr, size_t size)
{
    if (!str || size <= 0)
        return;

    Synth &synth = *m_synth;
    uint32_t numChannels = synth.m_numChannels;

    uint32_t index = 0;
    while(index < numChannels && index < size - 1)
    {
        const OpnChannel &adlChannel = m_chipChannels[index];

        OpnChannel::const_users_iterator loc = adlChannel.users.begin();
        OpnChannel::const_users_iterator locnext(loc);
        if(!loc.is_end()) ++locnext;

        if(loc.is_end())  // off
        {
            str[index] = '-';
        }
        else if(!locnext.is_end())  // arpeggio
        {
            str[index] = '@';
        }
        else  // on
        {
            str[index] = '+';
        }

        uint8_t attribute = 0;
        if (!loc.is_end())  // 4-bit color index of MIDI channel
            attribute |= uint8_t(loc->value.loc.MidCh & 0xF);

        attr[index] = static_cast<char>(attribute);
        ++index;
    }

    str[index] = 0;
    attr[index] = 0;
}

/* TODO */

//#ifndef ADLMIDI_DISABLE_CPP_EXTRAS

//ADLMIDI_EXPORT AdlInstrumentTester::AdlInstrumentTester(ADL_MIDIPlayer *device)
//{
//    cur_gm   = 0;
//    ins_idx  = 0;
//    play = reinterpret_cast<MIDIplay *>(device->adl_midiPlayer);
//    if(!play)
//        return;
//    opl = &play->opl;
//}

//ADLMIDI_EXPORT AdlInstrumentTester::~AdlInstrumentTester()
//{}

//ADLMIDI_EXPORT void AdlInstrumentTester::FindAdlList()
//{
//    const unsigned NumBanks = (unsigned)adl_getBanksCount();
//    std::set<unsigned> adl_ins_set;
//    for(unsigned bankno = 0; bankno < NumBanks; ++bankno)
//        adl_ins_set.insert(banks[bankno][cur_gm]);
//    adl_ins_list.assign(adl_ins_set.begin(), adl_ins_set.end());
//    ins_idx = 0;
//    NextAdl(0);
//    opl->Silence();
//}



//ADLMIDI_EXPORT void AdlInstrumentTester::Touch(unsigned c, unsigned volume) // Volume maxes at 127*127*127
//{
//    if(opl->LogarithmicVolumes)
//        opl->Touch_Real(c, volume * 127 / (127 * 127 * 127) / 2);
//    else
//    {
//        // The formula below: SOLVE(V=127^3 * 2^( (A-63.49999) / 8), A)
//        opl->Touch_Real(c, volume > 8725 ? static_cast<unsigned int>(std::log((double)volume) * 11.541561 + (0.5 - 104.22845)) : 0);
//        // The incorrect formula below: SOLVE(V=127^3 * (2^(A/63)-1), A)
//        //Touch_Real(c, volume>11210 ? 91.61112 * std::log(4.8819E-7*volume + 1.0)+0.5 : 0);
//    }
//}

//ADLMIDI_EXPORT void AdlInstrumentTester::DoNote(int note)
//{
//    if(adl_ins_list.empty()) FindAdlList();
//    const unsigned meta = adl_ins_list[ins_idx];
//    const adlinsdata &ains = opl->GetAdlMetaIns(meta);

//    int tone = (cur_gm & 128) ? (cur_gm & 127) : (note + 50);
//    if(ains.tone)
//    {
//        /*if(ains.tone < 20)
//                tone += ains.tone;
//            else */
//        if(ains.tone < 128)
//            tone = ains.tone;
//        else
//            tone -= ains.tone - 128;
//    }
//    double hertz = 172.00093 * std::exp(0.057762265 * (tone + 0.0));
//    int i[2] = { ains.adlno1, ains.adlno2 };
//    int32_t adlchannel[2] = { 0, 3 };
//    if(i[0] == i[1])
//    {
//        adlchannel[1] = -1;
//        adlchannel[0] = 6; // single-op
//        if(play->hooks.onDebugMessage)
//        {
//            play->hooks.onDebugMessage(play->hooks.onDebugMessage_userData,
//                                       "noteon at %d(%d) for %g Hz\n", adlchannel[0], i[0], hertz);
//        }
//    }
//    else
//    {
//        if(play->hooks.onDebugMessage)
//        {
//            play->hooks.onDebugMessage(play->hooks.onDebugMessage_userData,
//                                       "noteon at %d(%d) and %d(%d) for %g Hz\n", adlchannel[0], i[0], adlchannel[1], i[1], hertz);
//        }
//    }

//    opl->NoteOff(0);
//    opl->NoteOff(3);
//    opl->NoteOff(6);
//    for(unsigned c = 0; c < 2; ++c)
//    {
//        if(adlchannel[c] < 0) continue;
//        opl->Patch((uint16_t)adlchannel[c], (uint16_t)i[c]);
//        opl->Touch_Real((uint16_t)adlchannel[c], 127 * 127 * 100);
//        opl->Pan((uint16_t)adlchannel[c], 0x30);
//        opl->NoteOn((uint16_t)adlchannel[c], hertz);
//    }
//}

//ADLMIDI_EXPORT void AdlInstrumentTester::NextGM(int offset)
//{
//    cur_gm = (cur_gm + 256 + (uint32_t)offset) & 0xFF;
//    FindAdlList();
//}

//ADLMIDI_EXPORT void AdlInstrumentTester::NextAdl(int offset)
//{
//    if(adl_ins_list.empty()) FindAdlList();
//    const unsigned NumBanks = (unsigned)adl_getBanksCount();
//    ins_idx = (uint32_t)((int32_t)ins_idx + (int32_t)adl_ins_list.size() + offset) % adl_ins_list.size();

//    #if 0
//    UI.Color(15);
//    std::fflush(stderr);
//    std::printf("SELECTED G%c%d\t%s\n",
//                cur_gm < 128 ? 'M' : 'P', cur_gm < 128 ? cur_gm + 1 : cur_gm - 128,
//                "<-> select GM, ^v select ins, qwe play note");
//    std::fflush(stdout);
//    UI.Color(7);
//    std::fflush(stderr);
//    #endif

//    for(unsigned a = 0; a < adl_ins_list.size(); ++a)
//    {
//        const unsigned i = adl_ins_list[a];
//        const adlinsdata &ains = opl->GetAdlMetaIns(i);

//        char ToneIndication[8] = "   ";
//        if(ains.tone)
//        {
//            /*if(ains.tone < 20)
//                    snprintf(ToneIndication, 8, "+%-2d", ains.tone);
//                else*/
//            if(ains.tone < 128)
//                snprintf(ToneIndication, 8, "=%-2d", ains.tone);
//            else
//                snprintf(ToneIndication, 8, "-%-2d", ains.tone - 128);
//        }
//        std::printf("%s%s%s%u\t",
//                    ToneIndication,
//                    ains.adlno1 != ains.adlno2 ? "[2]" : "   ",
//                    (ins_idx == a) ? "->" : "\t",
//                    i
//                   );

//        for(unsigned bankno = 0; bankno < NumBanks; ++bankno)
//            if(banks[bankno][cur_gm] == i)
//                std::printf(" %u", bankno);

//        std::printf("\n");
//    }
//}

//ADLMIDI_EXPORT bool AdlInstrumentTester::HandleInputChar(char ch)
//{
//    static const char notes[] = "zsxdcvgbhnjmq2w3er5t6y7ui9o0p";
//    //                           c'd'ef'g'a'bC'D'EF'G'A'Bc'd'e
//    switch(ch)
//    {
//    case '/':
//    case 'H':
//    case 'A':
//        NextAdl(-1);
//        break;
//    case '*':
//    case 'P':
//    case 'B':
//        NextAdl(+1);
//        break;
//    case '-':
//    case 'K':
//    case 'D':
//        NextGM(-1);
//        break;
//    case '+':
//    case 'M':
//    case 'C':
//        NextGM(+1);
//        break;
//    case 3:
//        #if !((!defined(__WIN32__) || defined(__CYGWIN__)) && !defined(__DJGPP__))
//    case 27:
//        #endif
//        return false;
//    default:
//        const char *p = std::strchr(notes, ch);
//        if(p && *p)
//            DoNote((int)(p - notes) - 12);
//    }
//    return true;
//}

//#endif//ADLMIDI_DISABLE_CPP_EXTRAS
