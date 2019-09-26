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

#include "opnmidi_private.hpp"

/* Unify MIDI player casting and interface between ADLMIDI and OPNMIDI */
#define GET_MIDI_PLAYER(device) reinterpret_cast<OPNMIDIplay *>((device)->opn2_midiPlayer)
typedef OPNMIDIplay MidiPlayer;

static OPN2_Version opn2_version = {
    OPNMIDI_VERSION_MAJOR,
    OPNMIDI_VERSION_MINOR,
    OPNMIDI_VERSION_PATCHLEVEL
};

static const OPNMIDI_AudioFormat opn2_DefaultAudioFormat =
{
    OPNMIDI_SampleType_S16,
    sizeof(int16_t),
    2 * sizeof(int16_t),
};

/*---------------------------EXPORTS---------------------------*/

OPNMIDI_EXPORT struct OPN2_MIDIPlayer *opn2_init(long sample_rate)
{
    OPN2_MIDIPlayer *midi_device;
    midi_device = (OPN2_MIDIPlayer *)malloc(sizeof(OPN2_MIDIPlayer));
    if(!midi_device)
    {
        OPN2MIDI_ErrorString = "Can't initialize OPNMIDI: out of memory!";
        return NULL;
    }

    OPNMIDIplay *player = new(std::nothrow) OPNMIDIplay(static_cast<unsigned long>(sample_rate));
    if(!player)
    {
        free(midi_device);
        OPN2MIDI_ErrorString = "Can't initialize OPNMIDI: out of memory!";
        return NULL;
    }
    midi_device->opn2_midiPlayer = player;
    return midi_device;
}

OPNMIDI_EXPORT int opn2_setDeviceIdentifier(OPN2_MIDIPlayer *device, unsigned id)
{
    if(!device || id > 0x0f)
        return -1;
    MidiPlayer *play = GET_MIDI_PLAYER(device);
    assert(play);
    play->setDeviceId(static_cast<uint8_t>(id));
    return 0;
}

OPNMIDI_EXPORT int opn2_setNumChips(OPN2_MIDIPlayer *device, int numCards)
{
    if(device == NULL)
        return -2;

    MidiPlayer *play = GET_MIDI_PLAYER(device);
    assert(play);
    play->m_setup.numChips = static_cast<unsigned int>(numCards);
    if(play->m_setup.numChips < 1 || play->m_setup.numChips > OPN_MAX_CHIPS)
    {
        play->setErrorString("number of chips may only be 1.." OPN_MAX_CHIPS_STR ".\n");
        return -1;
    }

    if(!play->m_synth.setupLocked())
    {
        play->m_synth.m_numChips = play->m_setup.numChips;
        play->partialReset();
    }

    return 0;
}

OPNMIDI_EXPORT int opn2_getNumChips(struct OPN2_MIDIPlayer *device)
{
    if(device == NULL)
        return -2;
    MidiPlayer *play = GET_MIDI_PLAYER(device);
    assert(play);
    return (int)play->m_setup.numChips;
}

OPNMIDI_EXPORT int opn2_getNumChipsObtained(struct OPN2_MIDIPlayer *device)
{
    if(device == NULL)
        return -2;
    MidiPlayer *play = GET_MIDI_PLAYER(device);
    assert(play);
    return (int)play->m_synth.m_numChips;
}


OPNMIDI_EXPORT int opn2_reserveBanks(OPN2_MIDIPlayer *device, unsigned banks)
{
    if(!device)
        return -1;
    MidiPlayer *play = GET_MIDI_PLAYER(device);
    assert(play);
    OPN2::BankMap &map = play->m_synth.m_insBanks;
    map.reserve(banks);
    return (int)map.capacity();
}

OPNMIDI_EXPORT int opn2_getBank(OPN2_MIDIPlayer *device, const OPN2_BankId *idp, int flags, OPN2_Bank *bank)
{
    if(!device || !idp || !bank)
        return -1;

    OPN2_BankId id = *idp;
    if(id.lsb > 127 || id.msb > 127 || id.percussive > 1)
        return -1;
    size_t idnumber = ((id.msb << 8) | id.lsb | (id.percussive ? size_t(OPN2::PercussionTag) : 0));

    MidiPlayer *play = GET_MIDI_PLAYER(device);
    assert(play);
    OPN2::BankMap &map = play->m_synth.m_insBanks;

    OPN2::BankMap::iterator it;
    if(!(flags & OPNMIDI_Bank_Create))
    {
        it = map.find(idnumber);
        if(it == map.end())
            return -1;
    }
    else
    {
        std::pair<size_t, OPN2::Bank> value;
        value.first = idnumber;
        memset(&value.second, 0, sizeof(value.second));
        for (unsigned i = 0; i < 128; ++i)
            value.second.ins[i].flags = opnInstMeta::Flag_NoSound;

        std::pair<OPN2::BankMap::iterator, bool> ir;
        if(flags & OPNMIDI_Bank_CreateRt)
        {
            ir = map.insert(value, OPN2::BankMap::do_not_expand_t());
            if(ir.first == map.end())
                return -1;
        }
        else
            ir = map.insert(value);
        it = ir.first;
    }

    it.to_ptrs(bank->pointer);
    return 0;
}

OPNMIDI_EXPORT int opn2_getBankId(OPN2_MIDIPlayer *device, const OPN2_Bank *bank, OPN2_BankId *id)
{
    if(!device || !bank)
        return -1;

    OPN2::BankMap::iterator it = OPN2::BankMap::iterator::from_ptrs(bank->pointer);
    OPN2::BankMap::key_type idnumber = it->first;
    id->msb = (idnumber >> 8) & 127;
    id->lsb = idnumber & 127;
    id->percussive = (idnumber & OPN2::PercussionTag) ? 1 : 0;
    return 0;
}

OPNMIDI_EXPORT int opn2_removeBank(OPN2_MIDIPlayer *device, OPN2_Bank *bank)
{
    if(!device || !bank)
        return -1;

    MidiPlayer *play = GET_MIDI_PLAYER(device);
    assert(play);
    OPN2::BankMap &map = play->m_synth.m_insBanks;
    OPN2::BankMap::iterator it = OPN2::BankMap::iterator::from_ptrs(bank->pointer);
    size_t size = map.size();
    map.erase(it);
    return (map.size() != size) ? 0 : -1;
}

OPNMIDI_EXPORT int opn2_getFirstBank(OPN2_MIDIPlayer *device, OPN2_Bank *bank)
{
    if(!device)
        return -1;

    MidiPlayer *play = GET_MIDI_PLAYER(device);
    assert(play);
    OPN2::BankMap &map = play->m_synth.m_insBanks;

    OPN2::BankMap::iterator it = map.begin();
    if(it == map.end())
        return -1;

    it.to_ptrs(bank->pointer);
    return 0;
}

OPNMIDI_EXPORT int opn2_getNextBank(OPN2_MIDIPlayer *device, OPN2_Bank *bank)
{
    if(!device)
        return -1;

    MidiPlayer *play = GET_MIDI_PLAYER(device);
    assert(play);
    OPN2::BankMap &map = play->m_synth.m_insBanks;

    OPN2::BankMap::iterator it = OPN2::BankMap::iterator::from_ptrs(bank->pointer);
    if(++it == map.end())
        return -1;

    it.to_ptrs(bank->pointer);
    return 0;
}

OPNMIDI_EXPORT int opn2_getInstrument(OPN2_MIDIPlayer *device, const OPN2_Bank *bank, unsigned index, OPN2_Instrument *ins)
{
    if(!device || !bank || index > 127 || !ins)
        return -1;

    OPN2::BankMap::iterator it = OPN2::BankMap::iterator::from_ptrs(bank->pointer);
    cvt_FMIns_to_OPNI(*ins, it->second.ins[index]);
    ins->version = 0;
    return 0;
}

OPNMIDI_EXPORT int opn2_setInstrument(OPN2_MIDIPlayer *device, OPN2_Bank *bank, unsigned index, const OPN2_Instrument *ins)
{
    if(!device || !bank || index > 127 || !ins)
        return -1;

    if(ins->version != 0)
        return -1;

    OPN2::BankMap::iterator it = OPN2::BankMap::iterator::from_ptrs(bank->pointer);
    cvt_OPNI_to_FMIns(it->second.ins[index], *ins);
    return 0;
}

OPNMIDI_EXPORT int opn2_openBankFile(OPN2_MIDIPlayer *device, const char *filePath)
{
    if(device)
    {
        MidiPlayer *play = GET_MIDI_PLAYER(device);
        assert(play);
        play->m_setup.tick_skip_samples_delay = 0;
        if(!play->LoadBank(filePath))
        {
            std::string err = play->getErrorString();
            if(err.empty())
                play->setErrorString("OPN2 MIDI: Can't load file");
            return -1;
        }
        else
            return 0;
    }
    OPN2MIDI_ErrorString = "Can't load file: OPN2 MIDI is not initialized";
    return -1;
}

OPNMIDI_EXPORT int opn2_openBankData(OPN2_MIDIPlayer *device, const void *mem, long size)
{
    if(device)
    {
        MidiPlayer *play = GET_MIDI_PLAYER(device);
        assert(play);
        play->m_setup.tick_skip_samples_delay = 0;
        if(!play->LoadBank(mem, static_cast<size_t>(size)))
        {
            std::string err = play->getErrorString();
            if(err.empty())
                play->setErrorString("OPN2 MIDI: Can't load data from memory");
            return -1;
        }
        else return 0;
    }

    OPN2MIDI_ErrorString = "Can't load file: OPN2 MIDI is not initialized";
    return -1;
}

OPNMIDI_EXPORT void opn2_setLfoEnabled(struct OPN2_MIDIPlayer *device, int lfoEnable)
{
    if(!device) return;
    MidiPlayer *play = GET_MIDI_PLAYER(device);
    assert(play);
    play->m_setup.lfoEnable = lfoEnable;
    play->m_synth.m_lfoEnable = (lfoEnable < 0 ?
                                play->m_synth.m_insBankSetup.lfoEnable :
                                play->m_setup.lfoEnable) != 0;
    play->m_synth.commitLFOSetup();
}

OPNMIDI_EXPORT int opn2_getLfoEnabled(struct OPN2_MIDIPlayer *device)
{
    if(!device) return -1;
    MidiPlayer *play = GET_MIDI_PLAYER(device);
    assert(play);
    return play->m_synth.m_lfoEnable;
}

OPNMIDI_EXPORT void opn2_setLfoFrequency(struct OPN2_MIDIPlayer *device, int lfoFrequency)
{
    if(!device) return;
    MidiPlayer *play = GET_MIDI_PLAYER(device);
    assert(play);
    play->m_setup.lfoFrequency = lfoFrequency;
    play->m_synth.m_lfoFrequency = lfoFrequency < 0 ?
                                   play->m_synth.m_insBankSetup.lfoFrequency :
                                   (uint8_t)play->m_setup.lfoFrequency;
    play->m_synth.commitLFOSetup();
}

OPNMIDI_EXPORT int opn2_getLfoFrequency(struct OPN2_MIDIPlayer *device)
{
    if(!device) return -1;
    MidiPlayer *play = GET_MIDI_PLAYER(device);
    assert(play);
    return play->m_synth.m_lfoFrequency;
}

OPNMIDI_EXPORT void opn2_setScaleModulators(OPN2_MIDIPlayer *device, int smod)
{
    if(!device)
        return;
    MidiPlayer *play = GET_MIDI_PLAYER(device);
    assert(play);
    play->m_setup.ScaleModulators = smod;
    play->m_synth.m_scaleModulators = (play->m_setup.ScaleModulators != 0);
}

OPNMIDI_EXPORT void opn2_setFullRangeBrightness(struct OPN2_MIDIPlayer *device, int fr_brightness)
{
    if(!device)
        return;
    MidiPlayer *play = GET_MIDI_PLAYER(device);
    assert(play);
    play->m_setup.fullRangeBrightnessCC74 = (fr_brightness != 0);
}

OPNMIDI_EXPORT void opn2_setLoopEnabled(OPN2_MIDIPlayer *device, int loopEn)
{
    if(!device)
        return;
#ifndef OPNMIDI_DISABLE_MIDI_SEQUENCER
	MidiPlayer *play = GET_MIDI_PLAYER(device);
	assert(play);
    play->m_sequencer.setLoopEnabled(loopEn != 0);
#else
    ADL_UNUSED(loopEn);
#endif
}

OPNMIDI_EXPORT void opn2_setSoftPanEnabled(OPN2_MIDIPlayer *device, int softPanEn)
{
    if(!device)
        return;
    MidiPlayer *play = GET_MIDI_PLAYER(device);
    assert(play);
    play->m_synth.m_softPanning = (softPanEn != 0);
}

/* !!!DEPRECATED!!! */
OPNMIDI_EXPORT void opn2_setLogarithmicVolumes(struct OPN2_MIDIPlayer *device, int logvol)
{
    if(!device)
        return;
    MidiPlayer *play = GET_MIDI_PLAYER(device);
    assert(play);
    play->m_setup.LogarithmicVolumes = static_cast<unsigned int>(logvol);
    if(!play->m_synth.setupLocked())
    {
        if(play->m_setup.LogarithmicVolumes != 0)
            play->m_synth.setVolumeScaleModel(OPNMIDI_VolumeModel_NativeOPN2);
        else
            play->m_synth.setVolumeScaleModel(static_cast<OPNMIDI_VolumeModels>(play->m_setup.VolumeModel));
    }
}

OPNMIDI_EXPORT void opn2_setVolumeRangeModel(OPN2_MIDIPlayer *device, int volumeModel)
{
    if(!device)
        return;
    MidiPlayer *play = GET_MIDI_PLAYER(device);
    assert(play);
    play->m_setup.VolumeModel = volumeModel;
    if(!play->m_synth.setupLocked())
    {
        if(play->m_setup.VolumeModel == OPNMIDI_VolumeModel_AUTO)//Use bank default volume model
            play->m_synth.m_volumeScale = (OPN2::VolumesScale)play->m_synth.m_insBankSetup.volumeModel;
        else
            play->m_synth.setVolumeScaleModel(static_cast<OPNMIDI_VolumeModels>(volumeModel));
    }
}

OPNMIDI_EXPORT int opn2_getVolumeRangeModel(struct OPN2_MIDIPlayer *device)
{
    if(!device)
        return -1;
    MidiPlayer *play = GET_MIDI_PLAYER(device);
    assert(play);
    return play->m_synth.getVolumeScaleModel();
}

OPNMIDI_EXPORT int opn2_openFile(OPN2_MIDIPlayer *device, const char *filePath)
{
    if(device)
    {
        MidiPlayer *play = GET_MIDI_PLAYER(device);
        assert(play);
#ifndef OPNMIDI_DISABLE_MIDI_SEQUENCER
        play->m_setup.tick_skip_samples_delay = 0;
        if(!play->LoadMIDI(filePath))
        {
            std::string err = play->getErrorString();
            if(err.empty())
                play->setErrorString("OPN2 MIDI: Can't load file");
            return -1;
        }
        else return 0;
#else
        ADL_UNUSED(filePath);
        play->setErrorString("OPNMIDI: MIDI Sequencer is not supported in this build of library!");
        return -1;
#endif
    }

    OPN2MIDI_ErrorString = "Can't load file: OPN2 MIDI is not initialized";
    return -1;
}

OPNMIDI_EXPORT int opn2_openData(OPN2_MIDIPlayer *device, const void *mem, unsigned long size)
{
    if(device)
    {
        MidiPlayer *play = GET_MIDI_PLAYER(device);
        assert(play);
#ifndef OPNMIDI_DISABLE_MIDI_SEQUENCER
        play->m_setup.tick_skip_samples_delay = 0;
        if(!play->LoadMIDI(mem, static_cast<size_t>(size)))
        {
            std::string err = play->getErrorString();
            if(err.empty())
                play->setErrorString("OPN2 MIDI: Can't load data from memory");
            return -1;
        }
        else return 0;
#else
        ADL_UNUSED(mem);
        ADL_UNUSED(size);
        play->setErrorString("OPNMIDI: MIDI Sequencer is not supported in this build of library!");
        return -1;
#endif
    }

    OPN2MIDI_ErrorString = "Can't load file: OPN2 MIDI is not initialized";
    return -1;
}

OPNMIDI_EXPORT const char *opn2_emulatorName()
{
    return "<opn2_emulatorName() is deprecated! Use opn2_chipEmulatorName() instead!>";
}

OPNMIDI_EXPORT const char *opn2_chipEmulatorName(struct OPN2_MIDIPlayer *device)
{
    if(device)
    {
        MidiPlayer *play = GET_MIDI_PLAYER(device);
        assert(play);
        if(!play->m_synth.m_chips.empty())
            return play->m_synth.m_chips[0]->emulatorName();
    }
    return "Unknown";
}

OPNMIDI_EXPORT int opn2_switchEmulator(struct OPN2_MIDIPlayer *device, int emulator)
{
    if(device)
    {
        MidiPlayer *play = GET_MIDI_PLAYER(device);
        assert(play);
        if(opn2_isEmulatorAvailable(emulator))
        {
            play->m_setup.emulator = emulator;
            play->partialReset();
            return 0;
        }
        play->setErrorString("OPN2 MIDI: Unknown emulation core!");
    }
    return -1;
}


OPNMIDI_EXPORT int opn2_setRunAtPcmRate(OPN2_MIDIPlayer *device, int enabled)
{
    if(device)
    {
        MidiPlayer *play = GET_MIDI_PLAYER(device);
        assert(play);
        play->m_setup.runAtPcmRate = (enabled != 0);
        if(!play->m_synth.setupLocked())
            play->partialReset();
        return 0;
    }
    return -1;
}


OPNMIDI_EXPORT const char *opn2_linkedLibraryVersion()
{
#if !defined(OPNMIDI_ENABLE_HQ_RESAMPLER)
    return OPNMIDI_VERSION;
#else
    return OPNMIDI_VERSION " (HQ)";
#endif
}

OPNMIDI_EXPORT const OPN2_Version *opn2_linkedVersion()
{
    return &opn2_version;
}

OPNMIDI_EXPORT const char *opn2_errorString()
{
    return OPN2MIDI_ErrorString.c_str();
}

OPNMIDI_EXPORT const char *opn2_errorInfo(struct OPN2_MIDIPlayer *device)
{
    if(!device)
        return opn2_errorString();
    MidiPlayer *play = GET_MIDI_PLAYER(device);
    if(!play)
        return opn2_errorString();
    return play->getErrorString().c_str();
}

OPNMIDI_EXPORT void opn2_close(OPN2_MIDIPlayer *device)
{
    if(!device)
        return;
    MidiPlayer *play = GET_MIDI_PLAYER(device);
    assert(play);
    delete play;
    device->opn2_midiPlayer = NULL;
    free(device);
    device = NULL;
}

OPNMIDI_EXPORT void opn2_reset(OPN2_MIDIPlayer *device)
{
    if(!device)
        return;
    MidiPlayer *play = GET_MIDI_PLAYER(device);
    assert(play);
    play->partialReset();
    play->resetMIDI();
}

OPNMIDI_EXPORT double opn2_totalTimeLength(struct OPN2_MIDIPlayer *device)
{
#ifndef OPNMIDI_DISABLE_MIDI_SEQUENCER
    if(!device)
        return -1.0;
    MidiPlayer *play = GET_MIDI_PLAYER(device);
    assert(play);
    return play->m_sequencer.timeLength();
#else
    ADL_UNUSED(device);
    return -1.0;
#endif
}

OPNMIDI_EXPORT double opn2_loopStartTime(struct OPN2_MIDIPlayer *device)
{
#ifndef OPNMIDI_DISABLE_MIDI_SEQUENCER
    if(!device)
        return -1.0;
    MidiPlayer *play = GET_MIDI_PLAYER(device);
    assert(play);
    return play->m_sequencer.getLoopStart();
#else
    ADL_UNUSED(device);
    return -1.0;
#endif
}

OPNMIDI_EXPORT double opn2_loopEndTime(struct OPN2_MIDIPlayer *device)
{
#ifndef OPNMIDI_DISABLE_MIDI_SEQUENCER
    if(!device)
        return -1.0;
    MidiPlayer *play = GET_MIDI_PLAYER(device);
    assert(play);
    return play->m_sequencer.getLoopEnd();
#else
    ADL_UNUSED(device);
    return -1.0;
#endif
}

OPNMIDI_EXPORT double opn2_positionTell(struct OPN2_MIDIPlayer *device)
{
#ifndef OPNMIDI_DISABLE_MIDI_SEQUENCER
    if(!device)
        return -1.0;
    MidiPlayer *play = GET_MIDI_PLAYER(device);
    assert(play);
    return play->m_sequencer.tell();
#else
    ADL_UNUSED(device);
    return -1.0;
#endif
}

OPNMIDI_EXPORT void opn2_positionSeek(struct OPN2_MIDIPlayer *device, double seconds)
{
#ifndef OPNMIDI_DISABLE_MIDI_SEQUENCER
    if(seconds < 0.0)
        return;//Seeking negative position is forbidden! :-P
    if(!device)
        return;
    MidiPlayer *play = GET_MIDI_PLAYER(device);
    assert(play);
    play->realTime_panic();
    play->m_setup.delay = play->m_sequencer.seek(seconds, play->m_setup.mindelay);
    play->m_setup.carry = 0.0;
#else
    ADL_UNUSED(device);
    ADL_UNUSED(seconds);
#endif
}

OPNMIDI_EXPORT void opn2_positionRewind(struct OPN2_MIDIPlayer *device)
{
#ifndef OPNMIDI_DISABLE_MIDI_SEQUENCER
    if(!device)
        return;
    MidiPlayer *play = GET_MIDI_PLAYER(device);
    assert(play);
    play->realTime_panic();
    play->m_sequencer.rewind();
#else
    ADL_UNUSED(device);
#endif
}

OPNMIDI_EXPORT void opn2_setTempo(struct OPN2_MIDIPlayer *device, double tempo)
{
#ifndef OPNMIDI_DISABLE_MIDI_SEQUENCER
    if(!device || (tempo <= 0.0))
        return;
    MidiPlayer *play = GET_MIDI_PLAYER(device);
    assert(play);
    play->m_sequencer.setTempo(tempo);
#else
    ADL_UNUSED(device);
    ADL_UNUSED(tempo);
#endif
}


OPNMIDI_EXPORT int opn2_describeChannels(struct OPN2_MIDIPlayer *device, char *str, char *attr, size_t size)
{
    if(!device)
        return -1;
    MidiPlayer *play = GET_MIDI_PLAYER(device);
    assert(play);
    play->describeChannels(str, attr, size);
    return 0;
}


OPNMIDI_EXPORT const char *opn2_metaMusicTitle(struct OPN2_MIDIPlayer *device)
{
#ifndef OPNMIDI_DISABLE_MIDI_SEQUENCER
    if(!device)
        return "";
    MidiPlayer *play = GET_MIDI_PLAYER(device);
    assert(play);
    return play->m_sequencer.getMusicTitle().c_str();
#else
    ADL_UNUSED(device);
    return "";
#endif
}


OPNMIDI_EXPORT const char *opn2_metaMusicCopyright(struct OPN2_MIDIPlayer *device)
{
#ifndef OPNMIDI_DISABLE_MIDI_SEQUENCER
    if(!device)
        return "";
    MidiPlayer *play = GET_MIDI_PLAYER(device);
    assert(play);
    return play->m_sequencer.getMusicCopyright().c_str();
#else
    ADL_UNUSED(device);
    return 0;
#endif
}

OPNMIDI_EXPORT size_t opn2_metaTrackTitleCount(struct OPN2_MIDIPlayer *device)
{
#ifndef OPNMIDI_DISABLE_MIDI_SEQUENCER
    if(!device)
        return 0;
    MidiPlayer *play = GET_MIDI_PLAYER(device);
    assert(play);
    return play->m_sequencer.getTrackTitles().size();
#else
    ADL_UNUSED(device);
    return 0;
#endif
}

OPNMIDI_EXPORT const char *opn2_metaTrackTitle(struct OPN2_MIDIPlayer *device, size_t index)
{
#ifndef OPNMIDI_DISABLE_MIDI_SEQUENCER
    if(!device)
        return "";
    MidiPlayer *play = GET_MIDI_PLAYER(device);
    assert(play);
    const std::vector<std::string> &titles = play->m_sequencer.getTrackTitles();
    if(index >= titles.size())
        return "INVALID";
    return titles[index].c_str();
#else
    ADL_UNUSED(device);
    ADL_UNUSED(index);
    return "NOT SUPPORTED";
#endif
}


OPNMIDI_EXPORT size_t opn2_metaMarkerCount(struct OPN2_MIDIPlayer *device)
{
#ifndef OPNMIDI_DISABLE_MIDI_SEQUENCER
    if(!device)
        return 0;
    MidiPlayer *play = GET_MIDI_PLAYER(device);
    assert(play);
    return play->m_sequencer.getMarkers().size();
#else
    ADL_UNUSED(device);
    return 0;
#endif
}

OPNMIDI_EXPORT Opn2_MarkerEntry opn2_metaMarker(struct OPN2_MIDIPlayer *device, size_t index)
{
    struct Opn2_MarkerEntry marker;

#ifndef OPNMIDI_DISABLE_MIDI_SEQUENCER
    if(!device)
    {
        marker.label = "INVALID";
        marker.pos_time = 0.0;
        marker.pos_ticks = 0;
        return marker;
    }

    MidiPlayer *play = GET_MIDI_PLAYER(device);
    assert(play);

    const std::vector<MidiSequencer::MIDI_MarkerEntry> &markers = play->m_sequencer.getMarkers();
    if(index >= markers.size())
    {
        marker.label = "INVALID";
        marker.pos_time = 0.0;
        marker.pos_ticks = 0;
        return marker;
    }

    const MidiSequencer::MIDI_MarkerEntry &mk = markers[index];
    marker.label = mk.label.c_str();
    marker.pos_time = mk.pos_time;
    marker.pos_ticks = (unsigned long)mk.pos_ticks;
#else
    (void)device; (void)index;
    marker.label = "NOT SUPPORTED";
    marker.pos_time = 0.0;
    marker.pos_ticks = 0;
#endif
    return marker;
}

OPNMIDI_EXPORT void opn2_setRawEventHook(struct OPN2_MIDIPlayer *device, OPN2_RawEventHook rawEventHook, void *userData)
{
#ifndef OPNMIDI_DISABLE_MIDI_SEQUENCER
    if(!device)
        return;
    MidiPlayer *play = GET_MIDI_PLAYER(device);
    assert(play);
    play->m_sequencerInterface.onEvent = rawEventHook;
    play->m_sequencerInterface.onEvent_userData = userData;
#else
    ADL_UNUSED(device);
    ADL_UNUSED(rawEventHook);
    ADL_UNUSED(userData);
#endif
}

/* Set note hook */
OPNMIDI_EXPORT void opn2_setNoteHook(struct OPN2_MIDIPlayer *device, OPN2_NoteHook noteHook, void *userData)
{
    if(!device)
        return;
    MidiPlayer *play = GET_MIDI_PLAYER(device);
    assert(play);
    play->hooks.onNote = noteHook;
    play->hooks.onNote_userData = userData;
}

/* Set debug message hook */
OPNMIDI_EXPORT void opn2_setDebugMessageHook(struct OPN2_MIDIPlayer *device, OPN2_DebugMessageHook debugMessageHook, void *userData)
{
    if(!device)
        return;
    MidiPlayer *play = GET_MIDI_PLAYER(device);
    assert(play);
    play->hooks.onDebugMessage = debugMessageHook;
    play->hooks.onDebugMessage_userData = userData;
#ifndef OPNMIDI_DISABLE_MIDI_SEQUENCER
    play->m_sequencerInterface.onDebugMessage = debugMessageHook;
    play->m_sequencerInterface.onDebugMessage_userData = userData;
#endif
}


template <class Dst>
static void CopySamplesRaw(OPN2_UInt8 *dstLeft, OPN2_UInt8 *dstRight, const int32_t *src,
                           size_t frameCount, unsigned sampleOffset)
{
    for(size_t i = 0; i < frameCount; ++i) {
        *(Dst *)(dstLeft + (i * sampleOffset)) = src[2 * i];
        *(Dst *)(dstRight + (i * sampleOffset)) = src[(2 * i) + 1];
    }
}

template <class Dst, class Ret>
static void CopySamplesTransformed(OPN2_UInt8 *dstLeft, OPN2_UInt8 *dstRight, const int32_t *src,
                                   size_t frameCount, unsigned sampleOffset,
                                   Ret(&transform)(int32_t))
{
    for(size_t i = 0; i < frameCount; ++i) {
        *(Dst *)(dstLeft + (i * sampleOffset)) = static_cast<Dst>(transform(src[2 * i]));
        *(Dst *)(dstRight + (i * sampleOffset)) = static_cast<Dst>(transform(src[(2 * i) + 1]));
    }
}

static int SendStereoAudio(int         samples_requested,
                           ssize_t     in_size,
                           int32_t    *_in,
                           ssize_t     out_pos,
                           OPN2_UInt8 *left,
                           OPN2_UInt8 *right,
                           const OPNMIDI_AudioFormat *format)
{
    if(!in_size)
        return 0;
    size_t outputOffset = static_cast<size_t>(out_pos);
    size_t inSamples    = static_cast<size_t>(in_size * 2);
    size_t maxSamples   = static_cast<size_t>(samples_requested) - outputOffset;
    size_t toCopy       = std::min(maxSamples, inSamples);

    OPNMIDI_SampleType sampleType = format->type;
    const unsigned containerSize = format->containerSize;
    const unsigned sampleOffset = format->sampleOffset;

    left  += (outputOffset / 2) * sampleOffset;
    right += (outputOffset / 2) * sampleOffset;

    typedef int32_t(&pfnConvert)(int32_t);

    switch(sampleType) {
    case OPNMIDI_SampleType_S8:
    case OPNMIDI_SampleType_U8:
    {
        pfnConvert cvt = (sampleType == OPNMIDI_SampleType_S8) ? opn2_cvtS8 : opn2_cvtU8;
        switch(containerSize) {
        case sizeof(int8_t):
            CopySamplesTransformed<int8_t>(left, right, _in, toCopy / 2, sampleOffset, cvt);
            break;
        case sizeof(int16_t):
            CopySamplesTransformed<int16_t>(left, right, _in, toCopy / 2, sampleOffset, cvt);
            break;
        case sizeof(int32_t):
            CopySamplesTransformed<int32_t>(left, right, _in, toCopy / 2, sampleOffset, cvt);
            break;
        default:
            return -1;
        }
        break;
    }
    case OPNMIDI_SampleType_S16:
    case OPNMIDI_SampleType_U16:
    {
        pfnConvert cvt = (sampleType == OPNMIDI_SampleType_S16) ? opn2_cvtS16 : opn2_cvtU16;
        switch(containerSize) {
        case sizeof(int16_t):
            CopySamplesTransformed<int16_t>(left, right, _in, toCopy / 2, sampleOffset, cvt);
            break;
        case sizeof(int32_t):
            CopySamplesRaw<int32_t>(left, right, _in, toCopy / 2, sampleOffset);
            break;
        default:
            return -1;
        }
        break;
    }
    case OPNMIDI_SampleType_S24:
    case OPNMIDI_SampleType_U24:
    {
        pfnConvert cvt = (sampleType == OPNMIDI_SampleType_S24) ? opn2_cvtS24 : opn2_cvtU24;
        switch(containerSize) {
        case sizeof(int32_t):
            CopySamplesTransformed<int32_t>(left, right, _in, toCopy / 2, sampleOffset, cvt);
            break;
        default:
            return -1;
        }
        break;
    }
    case OPNMIDI_SampleType_S32:
    case OPNMIDI_SampleType_U32:
    {
        pfnConvert cvt = (sampleType == OPNMIDI_SampleType_S32) ? opn2_cvtS32 : opn2_cvtU32;
        switch(containerSize) {
        case sizeof(int32_t):
            CopySamplesTransformed<int32_t>(left, right, _in, toCopy / 2, sampleOffset, cvt);
            break;
        default:
            return -1;
        }
        break;
    }
    case OPNMIDI_SampleType_F32:
        if(containerSize != sizeof(float))
            return -1;
        CopySamplesTransformed<float>(left, right, _in, toCopy / 2, sampleOffset, opn2_cvtReal<float>);
        break;
    case OPNMIDI_SampleType_F64:
        if(containerSize != sizeof(double))
            return -1;
        CopySamplesTransformed<double>(left, right, _in, toCopy / 2, sampleOffset, opn2_cvtReal<double>);
        break;
    default:
        return -1;
    }

    return 0;
}


OPNMIDI_EXPORT int opn2_play(struct OPN2_MIDIPlayer *device, int sampleCount, short *out)
{
    return opn2_playFormat(device, sampleCount, (OPN2_UInt8 *)out, (OPN2_UInt8 *)(out + 1), &opn2_DefaultAudioFormat);
}

OPNMIDI_EXPORT int opn2_playFormat(OPN2_MIDIPlayer *device, int sampleCount,
                                   OPN2_UInt8 *out_left, OPN2_UInt8 *out_right,
                                   const OPNMIDI_AudioFormat *format)
{
#if defined(OPNMIDI_DISABLE_MIDI_SEQUENCER)
    ADL_UNUSED(device);
    ADL_UNUSED(sampleCount);
    ADL_UNUSED(out_left);
    ADL_UNUSED(out_right);
    ADL_UNUSED(format);
    return 0;
#endif

#ifndef OPNMIDI_DISABLE_MIDI_SEQUENCER
    sampleCount -= sampleCount % 2; //Avoid even sample requests
    if(sampleCount < 0)
        return 0;
    if(!device)
        return 0;

    MidiPlayer *player = GET_MIDI_PLAYER(device);
    assert(player);
    MidiPlayer::Setup &setup = player->m_setup;

    ssize_t gotten_len = 0;
    ssize_t n_periodCountStereo = 512;
    //ssize_t n_periodCountPhys = n_periodCountStereo * 2;
    int left = sampleCount;
    bool hasSkipped = setup.tick_skip_samples_delay > 0;

    while(left > 0)
    {
        {//
            const double eat_delay = setup.delay < setup.maxdelay ? setup.delay : setup.maxdelay;
            if(hasSkipped)
            {
                size_t samples = setup.tick_skip_samples_delay > sampleCount ? sampleCount : setup.tick_skip_samples_delay;
                n_periodCountStereo = samples / 2;
            }
            else
            {
                setup.delay -= eat_delay;
                setup.carry += double(setup.PCM_RATE) * eat_delay;
                n_periodCountStereo = static_cast<ssize_t>(setup.carry);
                setup.carry -= double(n_periodCountStereo);
            }

            //if(setup.SkipForward > 0)
            //    setup.SkipForward -= 1;
            //else
            {
                if((player->m_sequencer.positionAtEnd()) && (setup.delay <= 0.0))
                    break;//Stop to fetch samples at reaching the song end with disabled loop

                ssize_t leftSamples = left / 2;
                if(n_periodCountStereo > leftSamples)
                {
                    setup.tick_skip_samples_delay = (n_periodCountStereo - leftSamples) * 2;
                    n_periodCountStereo = leftSamples;
                }
                //! Count of stereo samples
                ssize_t in_generatedStereo = (n_periodCountStereo > 512) ? 512 : n_periodCountStereo;
                //! Total count of samples
                ssize_t in_generatedPhys = in_generatedStereo * 2;
                //! Unsigned total sample count
                //fill buffer with zeros
                int32_t *out_buf = player->m_outBuf;
                std::memset(out_buf, 0, static_cast<size_t>(in_generatedPhys) * sizeof(out_buf[0]));
                unsigned int chips = player->m_synth.m_numChips;
                if(chips == 1)
                    player->m_synth.m_chips[0]->generate32(out_buf, (size_t)in_generatedStereo);
                else/* if(n_periodCountStereo > 0)*/
                {
                    /* Generate data from every chip and mix result */
                    for(size_t card = 0; card < chips; ++card)
                        player->m_synth.m_chips[card]->generateAndMix32(out_buf, (size_t)in_generatedStereo);
                }
                /* Process it */
                if(SendStereoAudio(sampleCount, in_generatedStereo, out_buf, gotten_len, out_left, out_right, format) == -1)
                    return 0;

                left -= (int)in_generatedPhys;
                gotten_len += (in_generatedPhys) /* - setup.stored_samples*/;
            }
            if(hasSkipped)
            {
                setup.tick_skip_samples_delay -= n_periodCountStereo * 2;
                hasSkipped = setup.tick_skip_samples_delay > 0;
            }
            else
                setup.delay = player->Tick(eat_delay, setup.mindelay);
        }//
    }

    return static_cast<int>(gotten_len);
#endif //OPNMIDI_DISABLE_MIDI_SEQUENCER
}


OPNMIDI_EXPORT int opn2_generate(struct OPN2_MIDIPlayer *device, int sampleCount, short *out)
{
    return opn2_generateFormat(device, sampleCount, (OPN2_UInt8 *)out, (OPN2_UInt8 *)(out + 1), &opn2_DefaultAudioFormat);
}

OPNMIDI_EXPORT int opn2_generateFormat(struct OPN2_MIDIPlayer *device, int sampleCount,
                                       OPN2_UInt8 *out_left, OPN2_UInt8 *out_right,
                                       const OPNMIDI_AudioFormat *format)
{
    sampleCount -= sampleCount % 2; //Avoid even sample requests
    if(sampleCount < 0)
        return 0;
    if(!device)
        return 0;

    MidiPlayer *player = GET_MIDI_PLAYER(device);
    assert(player);
    MidiPlayer::Setup &setup = player->m_setup;

    ssize_t gotten_len = 0;
    ssize_t n_periodCountStereo = 512;

    int     left = sampleCount;
    double  delay = double(sampleCount) / double(setup.PCM_RATE);

    while(left > 0)
    {
        {//
            const double eat_delay = delay < setup.maxdelay ? delay : setup.maxdelay;
            delay -= eat_delay;
            setup.carry += double(setup.PCM_RATE) * eat_delay;
            n_periodCountStereo = static_cast<ssize_t>(setup.carry);
            setup.carry -= double(n_periodCountStereo);

            {
                ssize_t leftSamples = left / 2;
                if(n_periodCountStereo > leftSamples)
                    n_periodCountStereo = leftSamples;
                //! Count of stereo samples
                ssize_t in_generatedStereo = (n_periodCountStereo > 512) ? 512 : n_periodCountStereo;
                //! Total count of samples
                ssize_t in_generatedPhys = in_generatedStereo * 2;
                //! Unsigned total sample count
                //fill buffer with zeros
                int32_t *out_buf = player->m_outBuf;
                std::memset(out_buf, 0, static_cast<size_t>(in_generatedPhys) * sizeof(out_buf[0]));
                unsigned int chips = player->m_synth.m_numChips;
                if(chips == 1)
                    player->m_synth.m_chips[0]->generate32(out_buf, (size_t)in_generatedStereo);
                else/* if(n_periodCountStereo > 0)*/
                {
                    /* Generate data from every chip and mix result */
                    for(size_t card = 0; card < chips; ++card)
                        player->m_synth.m_chips[card]->generateAndMix32(out_buf, (size_t)in_generatedStereo);
                }
                /* Process it */
                if(SendStereoAudio(sampleCount, in_generatedStereo, out_buf, gotten_len, out_left, out_right, format) == -1)
                    return 0;

                left -= (int)in_generatedPhys;
                gotten_len += (in_generatedPhys) /* - setup.stored_samples*/;
            }

            player->TickIterators(eat_delay);
        }//...
    }

    return static_cast<int>(gotten_len);
}

OPNMIDI_EXPORT double opn2_tickEvents(struct OPN2_MIDIPlayer *device, double seconds, double granuality)
{
#ifndef OPNMIDI_DISABLE_MIDI_SEQUENCER
    if(!device)
        return -1.0;
    MidiPlayer *play = GET_MIDI_PLAYER(device);
    assert(play);
    return play->Tick(seconds, granuality);
#else
    ADL_UNUSED(device);
    ADL_UNUSED(seconds);
    ADL_UNUSED(granuality);
    return -1.0;
#endif
}

OPNMIDI_EXPORT int opn2_atEnd(struct OPN2_MIDIPlayer *device)
{
#ifndef OPNMIDI_DISABLE_MIDI_SEQUENCER
    if(!device)
        return 1;
    MidiPlayer *play = GET_MIDI_PLAYER(device);
    assert(play);
    return (int)play->m_sequencer.positionAtEnd();
#else
    ADL_UNUSED(device);
    return 1;
#endif
}

OPNMIDI_EXPORT size_t opn2_trackCount(struct OPN2_MIDIPlayer *device)
{
#ifndef OPNMIDI_DISABLE_MIDI_SEQUENCER
    if(!device)
        return 0;
    MidiPlayer *play = GET_MIDI_PLAYER(device);
    assert(play);
    return play->m_sequencer.getTrackCount();
#else
    ADL_UNUSED(device);
    return 0;
#endif
}

OPNMIDI_EXPORT int opn2_setTrackOptions(struct OPN2_MIDIPlayer *device, size_t trackNumber, unsigned trackOptions)
{
#ifndef OPNMIDI_DISABLE_MIDI_SEQUENCER
    if(!device)
        return -1;
    MidiPlayer *play = GET_MIDI_PLAYER(device);
    assert(play);
    MidiSequencer &seq = play->m_sequencer;

    unsigned enableFlag = trackOptions & 3;
    trackOptions &= ~3u;

    // handle on/off/solo
    switch(enableFlag)
    {
    default:
        break;
    case OPNMIDI_TrackOption_On:
    case OPNMIDI_TrackOption_Off:
        if(!seq.setTrackEnabled(trackNumber, enableFlag == OPNMIDI_TrackOption_On))
            return -1;
        break;
    case OPNMIDI_TrackOption_Solo:
        seq.setSoloTrack(trackNumber);
        break;
    }

    // handle others...
    if(trackOptions != 0)
        return -1;

    return 0;

#else
    ADL_UNUSED(device);
    ADL_UNUSED(trackNumber);
    ADL_UNUSED(trackOptions);
    return -1;
#endif
}

OPNMIDI_EXPORT void opn2_panic(struct OPN2_MIDIPlayer *device)
{
    if(!device)
        return;
    MidiPlayer *play = GET_MIDI_PLAYER(device);
    assert(play);
    play->realTime_panic();
}

OPNMIDI_EXPORT void opn2_rt_resetState(struct OPN2_MIDIPlayer *device)
{
    if(!device)
        return;
    MidiPlayer *play = GET_MIDI_PLAYER(device);
    assert(play);
    play->realTime_ResetState();
}

OPNMIDI_EXPORT int opn2_rt_noteOn(struct OPN2_MIDIPlayer *device, OPN2_UInt8 channel, OPN2_UInt8 note, OPN2_UInt8 velocity)
{
    if(!device)
        return 0;
    MidiPlayer *play = GET_MIDI_PLAYER(device);
    assert(play);
    return (int)play->realTime_NoteOn(channel, note, velocity);
}

OPNMIDI_EXPORT void opn2_rt_noteOff(struct OPN2_MIDIPlayer *device, OPN2_UInt8 channel, OPN2_UInt8 note)
{
    if(!device)
        return;
    MidiPlayer *play = GET_MIDI_PLAYER(device);
    assert(play);
    play->realTime_NoteOff(channel, note);
}

OPNMIDI_EXPORT void opn2_rt_noteAfterTouch(struct OPN2_MIDIPlayer *device, OPN2_UInt8 channel, OPN2_UInt8 note, OPN2_UInt8 atVal)
{
    if(!device)
        return;
    MidiPlayer *play = GET_MIDI_PLAYER(device);
    assert(play);
    play->realTime_NoteAfterTouch(channel, note, atVal);
}

OPNMIDI_EXPORT void opn2_rt_channelAfterTouch(struct OPN2_MIDIPlayer *device, OPN2_UInt8 channel, OPN2_UInt8 atVal)
{
    if(!device)
        return;
    MidiPlayer *play = GET_MIDI_PLAYER(device);
    assert(play);
    play->realTime_ChannelAfterTouch(channel, atVal);
}

OPNMIDI_EXPORT void opn2_rt_controllerChange(struct OPN2_MIDIPlayer *device, OPN2_UInt8 channel, OPN2_UInt8 type, OPN2_UInt8 value)
{
    if(!device)
        return;
    MidiPlayer *play = GET_MIDI_PLAYER(device);
    assert(play);
    play->realTime_Controller(channel, type, value);
}

OPNMIDI_EXPORT void opn2_rt_patchChange(struct OPN2_MIDIPlayer *device, OPN2_UInt8 channel, OPN2_UInt8 patch)
{
    if(!device)
        return;
    MidiPlayer *play = GET_MIDI_PLAYER(device);
    assert(play);
    play->realTime_PatchChange(channel, patch);
}

OPNMIDI_EXPORT void opn2_rt_pitchBend(struct OPN2_MIDIPlayer *device, OPN2_UInt8 channel, OPN2_UInt16 pitch)
{
    if(!device)
        return;
    MidiPlayer *play = GET_MIDI_PLAYER(device);
    assert(play);
    play->realTime_PitchBend(channel, pitch);
}

OPNMIDI_EXPORT void opn2_rt_pitchBendML(struct OPN2_MIDIPlayer *device, OPN2_UInt8 channel, OPN2_UInt8 msb, OPN2_UInt8 lsb)
{
    if(!device)
        return;
    MidiPlayer *play = GET_MIDI_PLAYER(device);
    assert(play);
    play->realTime_PitchBend(channel, msb, lsb);
}

OPNMIDI_EXPORT void opn2_rt_bankChangeLSB(struct OPN2_MIDIPlayer *device, OPN2_UInt8 channel, OPN2_UInt8 lsb)
{
    if(!device)
        return;
    MidiPlayer *play = GET_MIDI_PLAYER(device);
    assert(play);
    play->realTime_BankChangeLSB(channel, lsb);
}

OPNMIDI_EXPORT void opn2_rt_bankChangeMSB(struct OPN2_MIDIPlayer *device, OPN2_UInt8 channel, OPN2_UInt8 msb)
{
    if(!device)
        return;
    MidiPlayer *play = GET_MIDI_PLAYER(device);
    assert(play);
    play->realTime_BankChangeMSB(channel, msb);
}

OPNMIDI_EXPORT void opn2_rt_bankChange(struct OPN2_MIDIPlayer *device, OPN2_UInt8 channel, OPN2_SInt16 bank)
{
    if(!device)
        return;
    MidiPlayer *play = GET_MIDI_PLAYER(device);
    assert(play);
    play->realTime_BankChange(channel, (uint16_t)bank);
}

OPNMIDI_EXPORT int opn2_rt_systemExclusive(struct OPN2_MIDIPlayer *device, const OPN2_UInt8 *msg, size_t size)
{
    if(!device)
        return -1;
    MidiPlayer *play = GET_MIDI_PLAYER(device);
    assert(play);
    return play->realTime_SysEx(msg, size);
}
