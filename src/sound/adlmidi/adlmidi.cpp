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

#ifdef ADLMIDI_HW_OPL
#define MaxCards 1
#define MaxCards_STR "1" //Why not just "#MaxCards" ? Watcom fails to pass this with "syntax error" :-P
#else
#define MaxCards 100
#define MaxCards_STR "100"
#endif

static ADL_Version adl_version = {
    ADLMIDI_VERSION_MAJOR,
    ADLMIDI_VERSION_MINOR,
    ADLMIDI_VERSION_PATCHLEVEL
};

static const ADLMIDI_AudioFormat adl_DefaultAudioFormat =
{
    ADLMIDI_SampleType_S16,
    sizeof(int16_t),
    2 * sizeof(int16_t),
};

/*---------------------------EXPORTS---------------------------*/

ADLMIDI_EXPORT struct ADL_MIDIPlayer *adl_init(long sample_rate)
{
    ADL_MIDIPlayer *midi_device;
    midi_device = (ADL_MIDIPlayer *)malloc(sizeof(ADL_MIDIPlayer));
    if(!midi_device)
    {
        ADLMIDI_ErrorString = "Can't initialize ADLMIDI: out of memory!";
        return NULL;
    }

    MIDIplay *player = new MIDIplay(static_cast<unsigned long>(sample_rate));
    if(!player)
    {
        free(midi_device);
        ADLMIDI_ErrorString = "Can't initialize ADLMIDI: out of memory!";
        return NULL;
    }
    midi_device->adl_midiPlayer = player;
    adlRefreshNumCards(midi_device);
    return midi_device;
}

ADLMIDI_EXPORT int adl_setNumChips(ADL_MIDIPlayer *device, int numCards)
{
    if(device == NULL)
        return -2;

    MIDIplay *play = reinterpret_cast<MIDIplay *>(device->adl_midiPlayer);
#ifdef ADLMIDI_HW_OPL
    (void)numCards;
    play->m_setup.NumCards = 1;
#else
    play->m_setup.NumCards = static_cast<unsigned int>(numCards);
#endif
    if(play->m_setup.NumCards < 1 || play->m_setup.NumCards > MaxCards)
    {
        play->setErrorString("number of chips may only be 1.." MaxCards_STR ".\n");
        return -1;
    }

    play->opl.NumCards = play->m_setup.NumCards;
    adl_reset(device);

    return adlRefreshNumCards(device);
}

ADLMIDI_EXPORT int adl_getNumChips(struct ADL_MIDIPlayer *device)
{
    if(device == NULL)
        return -2;
    MIDIplay *play = reinterpret_cast<MIDIplay *>(device->adl_midiPlayer);
    if(play)
        return (int)play->m_setup.NumCards;
    return -2;
}

ADLMIDI_EXPORT int adl_setBank(ADL_MIDIPlayer *device, int bank)
{
    #ifdef DISABLE_EMBEDDED_BANKS
    ADL_UNUSED(device);
    ADL_UNUSED(bank);
    ADLMIDI_ErrorString = "This build of libADLMIDI has no embedded banks. Please load bank by using of adl_openBankFile() or adl_openBankData() functions instead of adl_setBank()";
    return -1;
    #else
    const uint32_t NumBanks = static_cast<uint32_t>(maxAdlBanks());
    int32_t bankno = bank;

    if(bankno < 0)
        bankno = 0;

    MIDIplay *play = reinterpret_cast<MIDIplay *>(device->adl_midiPlayer);
    if(static_cast<uint32_t>(bankno) >= NumBanks)
    {
        char errBuf[150];
        snprintf(errBuf, 150, "Embedded bank number may only be 0..%u!\n", static_cast<unsigned int>(NumBanks - 1));
        play->setErrorString(errBuf);
        return -1;
    }

    play->m_setup.AdlBank = static_cast<uint32_t>(bankno);
    play->opl.setEmbeddedBank(play->m_setup.AdlBank);
    play->applySetup();

    return adlRefreshNumCards(device);
    #endif
}

ADLMIDI_EXPORT int adl_getBanksCount()
{
    return maxAdlBanks();
}

ADLMIDI_EXPORT const char *const *adl_getBankNames()
{
    return banknames;
}

ADLMIDI_EXPORT int adl_reserveBanks(ADL_MIDIPlayer *device, unsigned banks)
{
    if(!device)
        return -1;
    MIDIplay *play = reinterpret_cast<MIDIplay *>(device->adl_midiPlayer);
    OPL3::BankMap &map = play->opl.dynamic_banks;
    map.reserve(banks);
    return (int)map.capacity();
}

ADLMIDI_EXPORT int adl_getBank(ADL_MIDIPlayer *device, const ADL_BankId *idp, int flags, ADL_Bank *bank)
{
    if(!device || !idp || !bank)
        return -1;

    ADL_BankId id = *idp;
    if(id.lsb > 127 || id.msb > 127 || id.percussive > 1)
        return -1;
    unsigned idnumber = (id.msb << 8) | id.lsb | (id.percussive ? OPL3::PercussionTag : 0);

    MIDIplay *play = reinterpret_cast<MIDIplay *>(device->adl_midiPlayer);
    OPL3::BankMap &map = play->opl.dynamic_banks;

    OPL3::BankMap::iterator it;
    if(!(flags & ADLMIDI_Bank_Create))
    {
        it = map.find(idnumber);
        if(it == map.end())
            return -1;
    }
    else
    {
        std::pair<uint16_t, OPL3::Bank> value;
        value.first = idnumber;
        memset(&value.second, 0, sizeof(value.second));
        for (unsigned i = 0; i < 128; ++i)
            value.second.ins[i].flags = adlinsdata::Flag_NoSound;

        std::pair<OPL3::BankMap::iterator, bool> ir;
        if(flags & ADLMIDI_Bank_CreateRt)
        {
            ir = map.insert(value, OPL3::BankMap::do_not_expand_t());
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

ADLMIDI_EXPORT int adl_getBankId(ADL_MIDIPlayer *device, const ADL_Bank *bank, ADL_BankId *id)
{
    if(!device || !bank)
        return -1;

    OPL3::BankMap::iterator it = OPL3::BankMap::iterator::from_ptrs(bank->pointer);
    unsigned idnumber = it->first;
    id->msb = (idnumber >> 8) & 127;
    id->lsb = idnumber & 127;
    id->percussive = (idnumber & OPL3::PercussionTag) ? 1 : 0;
    return 0;
}

ADLMIDI_EXPORT int adl_removeBank(ADL_MIDIPlayer *device, ADL_Bank *bank)
{
    if(!device || !bank)
        return -1;

    MIDIplay *play = reinterpret_cast<MIDIplay *>(device->adl_midiPlayer);
    OPL3::BankMap &map = play->opl.dynamic_banks;
    OPL3::BankMap::iterator it = OPL3::BankMap::iterator::from_ptrs(bank->pointer);
    size_t size = map.size();
    map.erase(it);
    return (map.size() != size) ? 0 : -1;
}

ADLMIDI_EXPORT int adl_getFirstBank(ADL_MIDIPlayer *device, ADL_Bank *bank)
{
    if(!device)
        return -1;

    MIDIplay *play = reinterpret_cast<MIDIplay *>(device->adl_midiPlayer);
    OPL3::BankMap &map = play->opl.dynamic_banks;

    OPL3::BankMap::iterator it = map.begin();
    if(it == map.end())
        return -1;

    it.to_ptrs(bank->pointer);
    return 0;
}

ADLMIDI_EXPORT int adl_getNextBank(ADL_MIDIPlayer *device, ADL_Bank *bank)
{
    if(!device)
        return -1;

    MIDIplay *play = reinterpret_cast<MIDIplay *>(device->adl_midiPlayer);
    OPL3::BankMap &map = play->opl.dynamic_banks;

    OPL3::BankMap::iterator it = OPL3::BankMap::iterator::from_ptrs(bank->pointer);
    if(++it == map.end())
        return -1;

    it.to_ptrs(bank->pointer);
    return 0;
}

ADLMIDI_EXPORT int adl_getInstrument(ADL_MIDIPlayer *device, const ADL_Bank *bank, unsigned index, ADL_Instrument *ins)
{
    if(!device || !bank || index > 127 || !ins)
        return 1;

    OPL3::BankMap::iterator it = OPL3::BankMap::iterator::from_ptrs(bank->pointer);
    cvt_FMIns_to_ADLI(*ins, it->second.ins[index]);
    ins->version = 0;
    return 0;
}

ADLMIDI_EXPORT int adl_setInstrument(ADL_MIDIPlayer *device, ADL_Bank *bank, unsigned index, const ADL_Instrument *ins)
{
    if(!device || !bank || index > 127 || !ins)
        return 1;

    if(ins->version != 0)
        return 1;

    OPL3::BankMap::iterator it = OPL3::BankMap::iterator::from_ptrs(bank->pointer);
    cvt_ADLI_to_FMIns(it->second.ins[index], *ins);
    return 0;
}

ADLMIDI_EXPORT int adl_setNumFourOpsChn(ADL_MIDIPlayer *device, int ops4)
{
    if(!device)
        return -1;
    MIDIplay *play = reinterpret_cast<MIDIplay *>(device->adl_midiPlayer);
    if((unsigned int)ops4 > 6 * play->m_setup.NumCards)
    {
        char errBuff[250];
        snprintf(errBuff, 250, "number of four-op channels may only be 0..%u when %u OPL3 cards are used.\n", (6 * (play->m_setup.NumCards)), play->m_setup.NumCards);
        play->setErrorString(errBuff);
        return -1;
    }

    play->m_setup.NumFourOps = static_cast<unsigned int>(ops4);
    play->opl.NumFourOps = play->m_setup.NumFourOps;

    return 0; //adlRefreshNumCards(device);
}

ADLMIDI_EXPORT int adl_getNumFourOpsChn(struct ADL_MIDIPlayer *device)
{
    if(!device)
        return -1;
    MIDIplay *play = reinterpret_cast<MIDIplay *>(device->adl_midiPlayer);
    if(play)
        return (int)play->m_setup.NumFourOps;
    return -1;
}

ADLMIDI_EXPORT void adl_setPercMode(ADL_MIDIPlayer *device, int percmod)
{
    if(!device) return;
    MIDIplay *play = reinterpret_cast<MIDIplay *>(device->adl_midiPlayer);
    play->m_setup.AdlPercussionMode = percmod;
    play->opl.AdlPercussionMode     = play->m_setup.AdlPercussionMode < 0 ?
                                      play->opl.dynamic_bank_setup.adLibPercussions :
                                      (play->m_setup.AdlPercussionMode != 0);
    play->opl.updateFlags();
}

ADLMIDI_EXPORT void adl_setHVibrato(ADL_MIDIPlayer *device, int hvibro)
{
    if(!device) return;
    MIDIplay *play = reinterpret_cast<MIDIplay *>(device->adl_midiPlayer);
    play->m_setup.HighVibratoMode = hvibro;
    play->opl.HighVibratoMode     = play->m_setup.HighVibratoMode < 0 ?
                                    play->opl.dynamic_bank_setup.deepVibrato :
                                    (play->m_setup.HighVibratoMode != 0);
    play->opl.updateDeepFlags();
}

ADLMIDI_EXPORT void adl_setHTremolo(ADL_MIDIPlayer *device, int htremo)
{
    if(!device) return;
    MIDIplay *play = reinterpret_cast<MIDIplay *>(device->adl_midiPlayer);
    play->m_setup.HighTremoloMode = htremo;
    play->opl.HighTremoloMode     = play->m_setup.HighTremoloMode < 0 ?
                                    play->opl.dynamic_bank_setup.deepTremolo :
                                    (play->m_setup.HighTremoloMode != 0);
    play->opl.updateDeepFlags();
}

ADLMIDI_EXPORT void adl_setScaleModulators(ADL_MIDIPlayer *device, int smod)
{
    if(!device) return;
    MIDIplay *play = reinterpret_cast<MIDIplay *>(device->adl_midiPlayer);
    play->m_setup.ScaleModulators = smod;
    play->opl.ScaleModulators     = play->m_setup.ScaleModulators < 0 ?
                                    play->opl.dynamic_bank_setup.scaleModulators :
                                    (play->m_setup.ScaleModulators != 0);
}

ADLMIDI_EXPORT void adl_setFullRangeBrightness(struct ADL_MIDIPlayer *device, int fr_brightness)
{
    if(!device) return;
    MIDIplay *play = reinterpret_cast<MIDIplay *>(device->adl_midiPlayer);
    play->m_setup.fullRangeBrightnessCC74 = (fr_brightness != 0);
}

ADLMIDI_EXPORT void adl_setLoopEnabled(ADL_MIDIPlayer *device, int loopEn)
{
    if(!device) return;
    MIDIplay *play = reinterpret_cast<MIDIplay *>(device->adl_midiPlayer);
    play->m_setup.loopingIsEnabled = (loopEn != 0);
}

/* !!!DEPRECATED!!! */
ADLMIDI_EXPORT void adl_setLogarithmicVolumes(struct ADL_MIDIPlayer *device, int logvol)
{
    if(!device) return;
    MIDIplay *play = reinterpret_cast<MIDIplay *>(device->adl_midiPlayer);
    play->m_setup.LogarithmicVolumes = (logvol != 0);
    if(play->m_setup.LogarithmicVolumes)
        play->opl.ChangeVolumeRangesModel(ADLMIDI_VolumeModel_NativeOPL3);
    else
        play->opl.ChangeVolumeRangesModel(static_cast<ADLMIDI_VolumeModels>(play->opl.m_volumeScale));
}

ADLMIDI_EXPORT void adl_setVolumeRangeModel(struct ADL_MIDIPlayer *device, int volumeModel)
{
    if(!device) return;
    MIDIplay *play = reinterpret_cast<MIDIplay *>(device->adl_midiPlayer);
    play->m_setup.VolumeModel = volumeModel;
    if(play->m_setup.VolumeModel == ADLMIDI_VolumeModel_AUTO)//Use bank default volume model
        play->opl.m_volumeScale = (OPL3::VolumesScale)play->opl.dynamic_bank_setup.volumeModel;
    else
        play->opl.ChangeVolumeRangesModel(static_cast<ADLMIDI_VolumeModels>(volumeModel));
}

ADLMIDI_EXPORT int adl_openBankFile(struct ADL_MIDIPlayer *device, const char *filePath)
{
    if(device && device->adl_midiPlayer)
    {
        MIDIplay *play = reinterpret_cast<MIDIplay *>(device->adl_midiPlayer);
        play->m_setup.tick_skip_samples_delay = 0;
        if(!play->LoadBank(filePath))
        {
            std::string err = play->getErrorString();
            if(err.empty())
                play->setErrorString("ADL MIDI: Can't load file");
            return -1;
        }
        else return adlRefreshNumCards(device);
    }

    ADLMIDI_ErrorString = "Can't load file: ADLMIDI is not initialized";
    return -1;
}

ADLMIDI_EXPORT int adl_openBankData(struct ADL_MIDIPlayer *device, const void *mem, unsigned long size)
{
    if(device && device->adl_midiPlayer)
    {
        MIDIplay *play = reinterpret_cast<MIDIplay *>(device->adl_midiPlayer);
        play->m_setup.tick_skip_samples_delay = 0;
        if(!play->LoadBank(mem, static_cast<size_t>(size)))
        {
            std::string err = play->getErrorString();
            if(err.empty())
                play->setErrorString("ADL MIDI: Can't load data from memory");
            return -1;
        }
        else return adlRefreshNumCards(device);
    }

    ADLMIDI_ErrorString = "Can't load file: ADL MIDI is not initialized";
    return -1;
}

ADLMIDI_EXPORT int adl_openFile(ADL_MIDIPlayer *device, const char *filePath)
{
    if(device && device->adl_midiPlayer)
    {
        MIDIplay *play = reinterpret_cast<MIDIplay *>(device->adl_midiPlayer);
#ifndef ADLMIDI_DISABLE_MIDI_SEQUENCER
        play->m_setup.tick_skip_samples_delay = 0;
        if(!play->LoadMIDI(filePath))
        {
            std::string err = play->getErrorString();
            if(err.empty())
                play->setErrorString("ADL MIDI: Can't load file");
            return -1;
        }
        else return 0;
#else
        (void)filePath;
        play->setErrorString("ADLMIDI: MIDI Sequencer is not supported in this build of library!");
        return -1;
#endif //ADLMIDI_DISABLE_MIDI_SEQUENCER
    }

    ADLMIDI_ErrorString = "Can't load file: ADL MIDI is not initialized";
    return -1;
}

ADLMIDI_EXPORT int adl_openData(ADL_MIDIPlayer *device, const void *mem, unsigned long size)
{
    if(device && device->adl_midiPlayer)
    {
        MIDIplay *play = reinterpret_cast<MIDIplay *>(device->adl_midiPlayer);
#ifndef ADLMIDI_DISABLE_MIDI_SEQUENCER
        play->m_setup.tick_skip_samples_delay = 0;
        if(!play->LoadMIDI(mem, static_cast<size_t>(size)))
        {
            std::string err = play->getErrorString();
            if(err.empty())
                play->setErrorString("ADL MIDI: Can't load data from memory");
            return -1;
        }
        else return 0;
#else
        (void)mem;(void)size;
        play->setErrorString("ADLMIDI: MIDI Sequencer is not supported in this build of library!");
        return -1;
#endif //ADLMIDI_DISABLE_MIDI_SEQUENCER
    }
    ADLMIDI_ErrorString = "Can't load file: ADL MIDI is not initialized";
    return -1;
}


ADLMIDI_EXPORT const char *adl_emulatorName()
{
    return "<adl_emulatorName() is deprecated! Use adl_chipEmulatorName() instead!>";
}

ADLMIDI_EXPORT const char *adl_chipEmulatorName(struct ADL_MIDIPlayer *device)
{
    if(device)
    {
        #ifndef ADLMIDI_HW_OPL
        MIDIplay *play = reinterpret_cast<MIDIplay *>(device->adl_midiPlayer);
        if(play && !play->opl.cardsOP2.empty())
            return play->opl.cardsOP2[0]->emulatorName();
        #else
        return "Hardware OPL3 chip on 0x330";
        #endif
    }
    return "Unknown";
}

ADLMIDI_EXPORT int adl_switchEmulator(struct ADL_MIDIPlayer *device, int emulator)
{
    if(device)
    {
        MIDIplay *play = reinterpret_cast<MIDIplay *>(device->adl_midiPlayer);
        if(play && (emulator >= 0) && (emulator < ADLMIDI_EMU_end))
        {
            play->m_setup.emulator = emulator;
            adl_reset(device);
            return 0;
        }
        play->setErrorString("OPL3 MIDI: Unknown emulation core!");
    }
    return -1;
}


ADLMIDI_EXPORT int adl_setRunAtPcmRate(ADL_MIDIPlayer *device, int enabled)
{
    if(device)
    {
        MIDIplay *play = reinterpret_cast<MIDIplay *>(device->adl_midiPlayer);
        if(play)
        {
            play->m_setup.runAtPcmRate = (enabled != 0);
            adl_reset(device);
            return 0;
        }
    }
    return -1;
}


ADLMIDI_EXPORT const char *adl_linkedLibraryVersion()
{
#if !defined(ADLMIDI_ENABLE_HQ_RESAMPLER)
    return ADLMIDI_VERSION;
#else
    return ADLMIDI_VERSION " (HQ)";
#endif
}

ADLMIDI_EXPORT const ADL_Version *adl_linkedVersion()
{
    return &adl_version;
}

ADLMIDI_EXPORT const char *adl_errorString()
{
    return ADLMIDI_ErrorString.c_str();
}

ADLMIDI_EXPORT const char *adl_errorInfo(struct ADL_MIDIPlayer *device)
{
    if(!device)
        return adl_errorString();
    MIDIplay *play = reinterpret_cast<MIDIplay *>(device->adl_midiPlayer);
    if(!play)
        return adl_errorString();
    return play->getErrorString().c_str();
}

ADLMIDI_EXPORT const char *adl_getMusicTitle(struct ADL_MIDIPlayer *device)
{
    if(!device)
        return "";
    MIDIplay *play = reinterpret_cast<MIDIplay *>(device->adl_midiPlayer);
    if(!play)
        return "";
    #ifndef ADLMIDI_DISABLE_MIDI_SEQUENCER
    return play->musTitle.c_str();
    #else
    return "";
    #endif
}

ADLMIDI_EXPORT void adl_close(struct ADL_MIDIPlayer *device)
{
    if(device->adl_midiPlayer)
        delete reinterpret_cast<MIDIplay *>(device->adl_midiPlayer);
    device->adl_midiPlayer = NULL;
    free(device);
    device = NULL;
}

ADLMIDI_EXPORT void adl_reset(struct ADL_MIDIPlayer *device)
{
    if(!device)
        return;
    MIDIplay *play = reinterpret_cast<MIDIplay *>(device->adl_midiPlayer);
    play->m_setup.tick_skip_samples_delay = 0;
    play->opl.runAtPcmRate = play->m_setup.runAtPcmRate;
    play->opl.Reset(play->m_setup.emulator, play->m_setup.PCM_RATE, play);
    play->ch.clear();
    play->ch.resize((size_t)play->opl.NumChannels);
}

ADLMIDI_EXPORT double adl_totalTimeLength(struct ADL_MIDIPlayer *device)
{
    if(!device)
        return -1.0;
    #ifndef ADLMIDI_DISABLE_MIDI_SEQUENCER
    return reinterpret_cast<MIDIplay *>(device->adl_midiPlayer)->timeLength();
    #else
    return -1.0;
    #endif
}

ADLMIDI_EXPORT double adl_loopStartTime(struct ADL_MIDIPlayer *device)
{
    if(!device)
        return -1.0;
    #ifndef ADLMIDI_DISABLE_MIDI_SEQUENCER
    return reinterpret_cast<MIDIplay *>(device->adl_midiPlayer)->getLoopStart();
    #else
    return -1.0;
    #endif
}

ADLMIDI_EXPORT double adl_loopEndTime(struct ADL_MIDIPlayer *device)
{
    if(!device)
        return -1.0;
    #ifndef ADLMIDI_DISABLE_MIDI_SEQUENCER
    return reinterpret_cast<MIDIplay *>(device->adl_midiPlayer)->getLoopEnd();
    #else
    return -1.0;
    #endif
}

ADLMIDI_EXPORT double adl_positionTell(struct ADL_MIDIPlayer *device)
{
    if(!device)
        return -1.0;
    #ifndef ADLMIDI_DISABLE_MIDI_SEQUENCER
    return reinterpret_cast<MIDIplay *>(device->adl_midiPlayer)->tell();
    #else
    return -1.0;
    #endif
}

ADLMIDI_EXPORT void adl_positionSeek(struct ADL_MIDIPlayer *device, double seconds)
{
    if(!device)
        return;
    #ifndef ADLMIDI_DISABLE_MIDI_SEQUENCER
    reinterpret_cast<MIDIplay *>(device->adl_midiPlayer)->seek(seconds);
    #endif
}

ADLMIDI_EXPORT void adl_positionRewind(struct ADL_MIDIPlayer *device)
{
    if(!device)
        return;
    #ifndef ADLMIDI_DISABLE_MIDI_SEQUENCER
    reinterpret_cast<MIDIplay *>(device->adl_midiPlayer)->rewind();
    #endif
}

ADLMIDI_EXPORT void adl_setTempo(struct ADL_MIDIPlayer *device, double tempo)
{
    if(!device || (tempo <= 0.0))
        return;
    #ifndef ADLMIDI_DISABLE_MIDI_SEQUENCER
    reinterpret_cast<MIDIplay *>(device->adl_midiPlayer)->setTempo(tempo);
    #endif
}


ADLMIDI_EXPORT const char *adl_metaMusicTitle(struct ADL_MIDIPlayer *device)
{
    if(!device)
        return "";
    #ifndef ADLMIDI_DISABLE_MIDI_SEQUENCER
    return reinterpret_cast<MIDIplay *>(device->adl_midiPlayer)->musTitle.c_str();
    #else
    return "";
    #endif
}


ADLMIDI_EXPORT const char *adl_metaMusicCopyright(struct ADL_MIDIPlayer *device)
{
    if(!device)
        return "";
    #ifndef ADLMIDI_DISABLE_MIDI_SEQUENCER
    return reinterpret_cast<MIDIplay *>(device->adl_midiPlayer)->musCopyright.c_str();
    #else
    return "";
    #endif
}

ADLMIDI_EXPORT size_t adl_metaTrackTitleCount(struct ADL_MIDIPlayer *device)
{
    if(!device)
        return 0;
#ifndef ADLMIDI_DISABLE_MIDI_SEQUENCER
    return reinterpret_cast<MIDIplay *>(device->adl_midiPlayer)->musTrackTitles.size();
#else
    return 0;
#endif
}

ADLMIDI_EXPORT const char *adl_metaTrackTitle(struct ADL_MIDIPlayer *device, size_t index)
{
    if(!device)
        return 0;
#ifndef ADLMIDI_DISABLE_MIDI_SEQUENCER
    MIDIplay *play = reinterpret_cast<MIDIplay *>(device->adl_midiPlayer);
    if(index >= play->musTrackTitles.size())
        return "INVALID";
    return play->musTrackTitles[index].c_str();
#else
    (void)device; (void)index;
    return "NOT SUPPORTED";
#endif
}


ADLMIDI_EXPORT size_t adl_metaMarkerCount(struct ADL_MIDIPlayer *device)
{
    if(!device)
        return 0;
#ifndef ADLMIDI_DISABLE_MIDI_SEQUENCER
    return reinterpret_cast<MIDIplay *>(device->adl_midiPlayer)->musMarkers.size();
#else
    return 0;
#endif
}

ADLMIDI_EXPORT Adl_MarkerEntry adl_metaMarker(struct ADL_MIDIPlayer *device, size_t index)
{
    struct Adl_MarkerEntry marker;
    #ifndef ADLMIDI_DISABLE_MIDI_SEQUENCER
    MIDIplay *play = reinterpret_cast<MIDIplay *>(device->adl_midiPlayer);
    if(!device || !play || (index >= play->musMarkers.size()))
    {
        marker.label = "INVALID";
        marker.pos_time = 0.0;
        marker.pos_ticks = 0;
        return marker;
    }
    else
    {
        MIDIplay::MIDI_MarkerEntry &mk = play->musMarkers[index];
        marker.label = mk.label.c_str();
        marker.pos_time = mk.pos_time;
        marker.pos_ticks = (unsigned long)mk.pos_ticks;
    }
    #else
    (void)device; (void)index;
    marker.label = "NOT SUPPORTED";
    marker.pos_time = 0.0;
    marker.pos_ticks = 0;
    #endif
    return marker;
}

ADLMIDI_EXPORT void adl_setRawEventHook(struct ADL_MIDIPlayer *device, ADL_RawEventHook rawEventHook, void *userData)
{
    if(!device)
        return;
    MIDIplay *play = reinterpret_cast<MIDIplay *>(device->adl_midiPlayer);
    play->hooks.onEvent = rawEventHook;
    play->hooks.onEvent_userData = userData;
}

/* Set note hook */
ADLMIDI_EXPORT void adl_setNoteHook(struct ADL_MIDIPlayer *device, ADL_NoteHook noteHook, void *userData)
{
    if(!device)
        return;
    MIDIplay *play = reinterpret_cast<MIDIplay *>(device->adl_midiPlayer);
    play->hooks.onNote = noteHook;
    play->hooks.onNote_userData = userData;
}

/* Set debug message hook */
ADLMIDI_EXPORT void adl_setDebugMessageHook(struct ADL_MIDIPlayer *device, ADL_DebugMessageHook debugMessageHook, void *userData)
{
    if(!device)
        return;
    MIDIplay *play = reinterpret_cast<MIDIplay *>(device->adl_midiPlayer);
    play->hooks.onDebugMessage = debugMessageHook;
    play->hooks.onDebugMessage_userData = userData;
}

#ifndef ADLMIDI_HW_OPL
template <class Dst>
static void CopySamplesRaw(ADL_UInt8 *dstLeft, ADL_UInt8 *dstRight, const int32_t *src,
                           size_t frameCount, unsigned sampleOffset)
{
    for(size_t i = 0; i < frameCount; ++i) {
        *(Dst *)(dstLeft + (i * sampleOffset)) = src[2 * i];
        *(Dst *)(dstRight + (i * sampleOffset)) = src[(2 * i) + 1];
    }
}

template <class Dst, class Ret>
static void CopySamplesTransformed(ADL_UInt8 *dstLeft, ADL_UInt8 *dstRight, const int32_t *src,
                                   size_t frameCount, unsigned sampleOffset,
                                   Ret(&transform)(int32_t))
{
    for(size_t i = 0; i < frameCount; ++i) {
        *(Dst *)(dstLeft + (i * sampleOffset)) = (Dst)transform(src[2 * i]);
        *(Dst *)(dstRight + (i * sampleOffset)) = (Dst)transform(src[(2 * i) + 1]);
    }
}

static int SendStereoAudio(int        samples_requested,
                           ssize_t    in_size,
                           int32_t   *_in,
                           ssize_t    out_pos,
                           ADL_UInt8 *left,
                           ADL_UInt8 *right,
                           const ADLMIDI_AudioFormat *format)
{
    if(!in_size)
        return 0;
    size_t outputOffset = static_cast<size_t>(out_pos);
    size_t inSamples    = static_cast<size_t>(in_size * 2);
    size_t maxSamples   = static_cast<size_t>(samples_requested) - outputOffset;
    size_t toCopy       = std::min(maxSamples, inSamples);

    ADLMIDI_SampleType sampleType = format->type;
    const unsigned containerSize = format->containerSize;
    const unsigned sampleOffset = format->sampleOffset;

    left  += (outputOffset / 2) * sampleOffset;
    right += (outputOffset / 2) * sampleOffset;

    typedef int32_t(&pfnConvert)(int32_t);
    typedef float(&ffnConvert)(int32_t);
    typedef double(&dfnConvert)(int32_t);

    switch(sampleType) {
    case ADLMIDI_SampleType_S8:
    case ADLMIDI_SampleType_U8:
    {
        pfnConvert cvt = (sampleType == ADLMIDI_SampleType_S8) ? adl_cvtS8 : adl_cvtU8;
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
    case ADLMIDI_SampleType_S16:
    case ADLMIDI_SampleType_U16:
    {
        pfnConvert cvt = (sampleType == ADLMIDI_SampleType_S16) ? adl_cvtS16 : adl_cvtU16;
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
    case ADLMIDI_SampleType_S24:
    case ADLMIDI_SampleType_U24:
    {
        pfnConvert cvt = (sampleType == ADLMIDI_SampleType_S24) ? adl_cvtS24 : adl_cvtU24;
        switch(containerSize) {
        case sizeof(int32_t):
            CopySamplesTransformed<int32_t>(left, right, _in, toCopy / 2, sampleOffset, cvt);
            break;
        default:
            return -1;
        }
        break;
    }
    case ADLMIDI_SampleType_S32:
    case ADLMIDI_SampleType_U32:
    {
        pfnConvert cvt = (sampleType == ADLMIDI_SampleType_S32) ? adl_cvtS32 : adl_cvtU32;
        switch(containerSize) {
        case sizeof(int32_t):
            CopySamplesTransformed<int32_t>(left, right, _in, toCopy / 2, sampleOffset, cvt);
            break;
        default:
            return -1;
        }
        break;
    }
    case ADLMIDI_SampleType_F32:
    {
        if(containerSize != sizeof(float))
            return -1;
        ffnConvert cvt = adl_cvtReal<float>;
        CopySamplesTransformed<float>(left, right, _in, toCopy / 2, sampleOffset, cvt);
        break;
    }
    case ADLMIDI_SampleType_F64:
    {
        if(containerSize != sizeof(double))
            return -1;
        dfnConvert cvt = adl_cvtReal<double>;
        CopySamplesTransformed<double>(left, right, _in, toCopy / 2, sampleOffset, cvt);
        break;
    }
    default:
        return -1;
    }

    return 0;
}
#endif

ADLMIDI_EXPORT int adl_play(struct ADL_MIDIPlayer *device, int sampleCount, short *out)
{
    return adl_playFormat(device, sampleCount, (ADL_UInt8 *)out, (ADL_UInt8 *)(out + 1), &adl_DefaultAudioFormat);
}

ADLMIDI_EXPORT int adl_playFormat(ADL_MIDIPlayer *device, int sampleCount,
                                  ADL_UInt8 *out_left, ADL_UInt8 *out_right,
                                  const ADLMIDI_AudioFormat *format)
{
    #ifndef ADLMIDI_DISABLE_MIDI_SEQUENCER
    #ifdef ADLMIDI_HW_OPL
    (void)device;
    (void)sampleCount;
    (void)out_left;
    (void)out_right;
    (void)format;
    return 0;
    #else
    sampleCount -= sampleCount % 2; //Avoid even sample requests
    if(sampleCount < 0)
        return 0;
    if(!device)
        return 0;

    MIDIplay *player = reinterpret_cast<MIDIplay *>(device->adl_midiPlayer);
    MIDIplay::Setup &setup = player->m_setup;

    ssize_t gotten_len = 0;
    ssize_t n_periodCountStereo = 512;
    //ssize_t n_periodCountPhys = n_periodCountStereo * 2;
    int left = sampleCount;
    bool hasSkipped = setup.tick_skip_samples_delay > 0;

    while(left > 0)
    {
        {//...
            const double eat_delay = setup.delay < setup.maxdelay ? setup.delay : setup.maxdelay;
            if(hasSkipped)
            {
                size_t samples = setup.tick_skip_samples_delay > sampleCount ? sampleCount : setup.tick_skip_samples_delay;
                n_periodCountStereo = samples / 2;
            }
            else
            {
                setup.delay -= eat_delay;
                setup.carry += setup.PCM_RATE * eat_delay;
                n_periodCountStereo = static_cast<ssize_t>(setup.carry);
                setup.carry -= n_periodCountStereo;
            }

            //if(setup.SkipForward > 0)
            //    setup.SkipForward -= 1;
            //else
            {
                if((player->atEnd) && (setup.delay <= 0.0))
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
                int32_t *out_buf = player->outBuf;
                std::memset(out_buf, 0, static_cast<size_t>(in_generatedPhys) * sizeof(out_buf[0]));
                unsigned int chips = player->opl.NumCards;
                if(chips == 1)
                {
                    player->opl.cardsOP2[0]->generate32(out_buf, (size_t)in_generatedStereo);
                }
                else if(n_periodCountStereo > 0)
                {
                    /* Generate data from every chip and mix result */
                    for(size_t card = 0; card < chips; ++card)
                        player->opl.cardsOP2[card]->generateAndMix32(out_buf, (size_t)in_generatedStereo);
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

        }//...
    }

    return static_cast<int>(gotten_len);
    #endif
    #else
    return 0;
    #endif //ADLMIDI_DISABLE_MIDI_SEQUENCER
}


ADLMIDI_EXPORT int adl_generate(struct ADL_MIDIPlayer *device, int sampleCount, short *out)
{
    return adl_generateFormat(device, sampleCount, (ADL_UInt8 *)out, (ADL_UInt8 *)(out + 1), &adl_DefaultAudioFormat);
}

ADLMIDI_EXPORT int adl_generateFormat(struct ADL_MIDIPlayer *device, int sampleCount,
                                      ADL_UInt8 *out_left, ADL_UInt8 *out_right,
                                      const ADLMIDI_AudioFormat *format)
{
    #ifdef ADLMIDI_HW_OPL
    (void)device;
    (void)sampleCount;
    (void)out_left;
    (void)out_right;
    (void)format;
    return 0;
    #else
    sampleCount -= sampleCount % 2; //Avoid even sample requests
    if(sampleCount < 0)
        return 0;
    if(!device)
        return 0;

    MIDIplay *player = reinterpret_cast<MIDIplay *>(device->adl_midiPlayer);
    MIDIplay::Setup &setup = player->m_setup;

    ssize_t gotten_len = 0;
    ssize_t n_periodCountStereo = 512;

    int     left = sampleCount;
    double  delay = double(sampleCount) / double(setup.PCM_RATE);

    while(left > 0)
    {
        {//...
            const double eat_delay = delay < setup.maxdelay ? delay : setup.maxdelay;
            delay -= eat_delay;
            setup.carry += setup.PCM_RATE * eat_delay;
            n_periodCountStereo = static_cast<ssize_t>(setup.carry);
            setup.carry -= n_periodCountStereo;

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
                int32_t *out_buf = player->outBuf;
                std::memset(out_buf, 0, static_cast<size_t>(in_generatedPhys) * sizeof(out_buf[0]));
                unsigned int chips = player->opl.NumCards;
                if(chips == 1)
                    player->opl.cardsOP2[0]->generate32(out_buf, (size_t)in_generatedStereo);
                else if(n_periodCountStereo > 0)
                {
                    /* Generate data from every chip and mix result */
                    for(unsigned card = 0; card < chips; ++card)
                        player->opl.cardsOP2[card]->generateAndMix32(out_buf, (size_t)in_generatedStereo);
                }
                /* Process it */
                if(SendStereoAudio(sampleCount, in_generatedStereo, out_buf, gotten_len, out_left, out_right, format) == -1)
                    return 0;

                left -= (int)in_generatedPhys;
                gotten_len += (in_generatedPhys) /* - setup.stored_samples*/;
            }

            player->TickIteratos(eat_delay);
        }//...
    }

    return static_cast<int>(gotten_len);
    #endif
}

ADLMIDI_EXPORT double adl_tickEvents(struct ADL_MIDIPlayer *device, double seconds, double granuality)
{
#ifndef ADLMIDI_DISABLE_MIDI_SEQUENCER
    if(!device)
        return -1.0;
    MIDIplay *player = reinterpret_cast<MIDIplay *>(device->adl_midiPlayer);
    if(!player)
        return -1.0;
    return player->Tick(seconds, granuality);
#else
    (void)seconds; (void)granuality;
    return -1.0;
#endif
}

ADLMIDI_EXPORT int adl_atEnd(struct ADL_MIDIPlayer *device)
{
#ifndef ADLMIDI_DISABLE_MIDI_SEQUENCER
    if(!device)
        return 1;
    MIDIplay *player = reinterpret_cast<MIDIplay *>(device->adl_midiPlayer);
    if(!player)
        return 1;
    return (int)player->atEnd;
#else
    return 1;
#endif
}

ADLMIDI_EXPORT void adl_panic(struct ADL_MIDIPlayer *device)
{
    if(!device)
        return;
    MIDIplay *player = reinterpret_cast<MIDIplay *>(device->adl_midiPlayer);
    if(!player)
        return;
    player->realTime_panic();
}

ADLMIDI_EXPORT void adl_rt_resetState(struct ADL_MIDIPlayer *device)
{
    if(!device)
        return;
    MIDIplay *player = reinterpret_cast<MIDIplay *>(device->adl_midiPlayer);
    if(!player)
        return;
    player->realTime_ResetState();
}

ADLMIDI_EXPORT int adl_rt_noteOn(struct ADL_MIDIPlayer *device, ADL_UInt8 channel, ADL_UInt8 note, ADL_UInt8 velocity)
{
    if(!device)
        return 0;
    MIDIplay *player = reinterpret_cast<MIDIplay *>(device->adl_midiPlayer);
    if(!player)
        return 0;
    return (int)player->realTime_NoteOn(channel, note, velocity);
}

ADLMIDI_EXPORT void adl_rt_noteOff(struct ADL_MIDIPlayer *device, ADL_UInt8 channel, ADL_UInt8 note)
{
    if(!device)
        return;
    MIDIplay *player = reinterpret_cast<MIDIplay *>(device->adl_midiPlayer);
    if(!player)
        return;
    player->realTime_NoteOff(channel, note);
}

ADLMIDI_EXPORT void adl_rt_noteAfterTouch(struct ADL_MIDIPlayer *device, ADL_UInt8 channel, ADL_UInt8 note, ADL_UInt8 atVal)
{
    if(!device)
        return;
    MIDIplay *player = reinterpret_cast<MIDIplay *>(device->adl_midiPlayer);
    if(!player)
        return;
    player->realTime_NoteAfterTouch(channel, note, atVal);
}

ADLMIDI_EXPORT void adl_rt_channelAfterTouch(struct ADL_MIDIPlayer *device, ADL_UInt8 channel, ADL_UInt8 atVal)
{
    if(!device)
        return;
    MIDIplay *player = reinterpret_cast<MIDIplay *>(device->adl_midiPlayer);
    if(!player)
        return;
    player->realTime_ChannelAfterTouch(channel, atVal);
}

ADLMIDI_EXPORT void adl_rt_controllerChange(struct ADL_MIDIPlayer *device, ADL_UInt8 channel, ADL_UInt8 type, ADL_UInt8 value)
{
    if(!device)
        return;
    MIDIplay *player = reinterpret_cast<MIDIplay *>(device->adl_midiPlayer);
    if(!player)
        return;
    player->realTime_Controller(channel, type, value);
}

ADLMIDI_EXPORT void adl_rt_patchChange(struct ADL_MIDIPlayer *device, ADL_UInt8 channel, ADL_UInt8 patch)
{
    if(!device)
        return;
    MIDIplay *player = reinterpret_cast<MIDIplay *>(device->adl_midiPlayer);
    if(!player)
        return;
    player->realTime_PatchChange(channel, patch);
}

ADLMIDI_EXPORT void adl_rt_pitchBend(struct ADL_MIDIPlayer *device, ADL_UInt8 channel, ADL_UInt16 pitch)
{
    if(!device)
        return;
    MIDIplay *player = reinterpret_cast<MIDIplay *>(device->adl_midiPlayer);
    if(!player)
        return;
    player->realTime_PitchBend(channel, pitch);
}

ADLMIDI_EXPORT void adl_rt_pitchBendML(struct ADL_MIDIPlayer *device, ADL_UInt8 channel, ADL_UInt8 msb, ADL_UInt8 lsb)
{
    if(!device)
        return;
    MIDIplay *player = reinterpret_cast<MIDIplay *>(device->adl_midiPlayer);
    if(!player)
        return;
    player->realTime_PitchBend(channel, msb, lsb);
}

ADLMIDI_EXPORT void adl_rt_bankChangeLSB(struct ADL_MIDIPlayer *device, ADL_UInt8 channel, ADL_UInt8 lsb)
{
    if(!device)
        return;
    MIDIplay *player = reinterpret_cast<MIDIplay *>(device->adl_midiPlayer);
    if(!player)
        return;
    player->realTime_BankChangeLSB(channel, lsb);
}

ADLMIDI_EXPORT void adl_rt_bankChangeMSB(struct ADL_MIDIPlayer *device, ADL_UInt8 channel, ADL_UInt8 msb)
{
    if(!device)
        return;
    MIDIplay *player = reinterpret_cast<MIDIplay *>(device->adl_midiPlayer);
    if(!player)
        return;
    player->realTime_BankChangeMSB(channel, msb);
}

ADLMIDI_EXPORT void adl_rt_bankChange(struct ADL_MIDIPlayer *device, ADL_UInt8 channel, ADL_SInt16 bank)
{
    if(!device)
        return;
    MIDIplay *player = reinterpret_cast<MIDIplay *>(device->adl_midiPlayer);
    if(!player)
        return;
    player->realTime_BankChange(channel, (uint16_t)bank);
}
