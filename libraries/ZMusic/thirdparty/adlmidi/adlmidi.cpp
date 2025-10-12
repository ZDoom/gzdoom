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

#include "adlmidi_midiplay.hpp"
#include "adlmidi_opl3.hpp"
#include "adlmidi_private.hpp"
#include "chips/opl_chip_base.h"
#ifndef ADLMIDI_DISABLE_MIDI_SEQUENCER
#   define BWMIDI_ENABLE_OPL_MUSIC_SUPPORT
#   include "midi_sequencer.hpp"
#endif
#ifdef ENABLE_HW_OPL_DOS
#   include "chips/dos_hw_opl.h"
#endif

#if defined(_MSC_VER) && _MSC_VER < 1900

#define snprintf c99_snprintf
#define vsnprintf c99_vsnprintf

__inline int c99_vsnprintf(char *outBuf, size_t size, const char *format, va_list ap)
{
    int count = -1;

    if (size != 0)
        count = _vsnprintf_s(outBuf, size, _TRUNCATE, format, ap);
    if (count == -1)
        count = _vscprintf(format, ap);

    return count;
}

__inline int c99_snprintf(char *outBuf, size_t size, const char *format, ...)
{
    int count;
    va_list ap;

    va_start(ap, format);
    count = c99_vsnprintf(outBuf, size, format, ap);
    va_end(ap);

    return count;
}
#endif

/* Unify MIDI player casting and interface between ADLMIDI and OPNMIDI */
#define GET_MIDI_PLAYER(device) reinterpret_cast<MIDIplay *>((device)->adl_midiPlayer)
typedef MIDIplay MidiPlayer;

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

    MIDIplay *player = new(std::nothrow) MIDIplay(static_cast<unsigned long>(sample_rate));
    if(!player)
    {
        free(midi_device);
        ADLMIDI_ErrorString = "Can't initialize ADLMIDI: out of memory!";
        return NULL;
    }
    midi_device->adl_midiPlayer = player;
    adlCalculateFourOpChannels(player);
    return midi_device;
}

ADLMIDI_EXPORT void adl_close(struct ADL_MIDIPlayer *device)
{
    if(!device)
        return;
    MidiPlayer *play = GET_MIDI_PLAYER(device);
    assert(play);
    delete play;
    device->adl_midiPlayer = NULL;
    free(device);
    device = NULL;
}

ADLMIDI_EXPORT int adl_setDeviceIdentifier(ADL_MIDIPlayer *device, unsigned id)
{
    if(!device || id > 0x0f)
        return -1;
    MidiPlayer *play = GET_MIDI_PLAYER(device);
    assert(play);
    play->setDeviceId(static_cast<uint8_t>(id));
    return 0;
}

ADLMIDI_EXPORT int adl_setNumChips(ADL_MIDIPlayer *device, int numChips)
{
    if(device == NULL)
        return -2;

    MidiPlayer *play = GET_MIDI_PLAYER(device);
    assert(play);

#ifdef ADLMIDI_ENABLE_HW_SERIAL
    if(play->m_setup.serial)
        numChips = 1;
#endif

#ifdef ADLMIDI_HW_OPL
    play->m_setup.numChips = numChips > 2 ? 1 : static_cast<unsigned int>(numChips);
#else
    play->m_setup.numChips = static_cast<unsigned int>(numChips);
#endif

    if((play->m_setup.numChips < 1) || (play->m_setup.numChips > ADL_MAX_CHIPS))
    {
        play->setErrorString("number of chips may only be 1.." ADL_MAX_CHIPS_STR ".\n");
        return -1;
    }

    int maxFourOps = static_cast<int>(play->m_setup.numChips * 6);

    if(play->m_setup.numFourOps > maxFourOps)
        play->m_setup.numFourOps = maxFourOps;
    else if(play->m_setup.numFourOps < -1)
        play->m_setup.numFourOps = -1;

    Synth &synth = *play->m_synth;
    if(!synth.setupLocked())
    {
        synth.m_numChips = play->m_setup.numChips;
        if(play->m_setup.numFourOps < 0)
            adlCalculateFourOpChannels(play, true);
        else
            synth.m_numFourOps = static_cast<uint32_t>(play->m_setup.numFourOps);
        play->partialReset();
        return 0;
    }

    return 0;
}

ADLMIDI_EXPORT int adl_getNumChips(struct ADL_MIDIPlayer *device)
{
    if(device == NULL)
        return -2;
    MidiPlayer *play = GET_MIDI_PLAYER(device);
    assert(play);
    return (int)play->m_setup.numChips;
}

ADLMIDI_EXPORT int adl_getNumChipsObtained(struct ADL_MIDIPlayer *device)
{
    if(device == NULL)
        return -2;
    MidiPlayer *play = GET_MIDI_PLAYER(device);
    assert(play);
    return (int)play->m_synth->m_numChips;
}

ADLMIDI_EXPORT int adl_setBank(ADL_MIDIPlayer *device, int bank)
{
#ifdef DISABLE_EMBEDDED_BANKS
    ADL_UNUSED(bank);
    MidiPlayer *play = GET_MIDI_PLAYER(device);
    assert(play);
    play->setErrorString("This build of libADLMIDI has no embedded banks. "
                         "Please load banks by using adl_openBankFile() or "
                         "adl_openBankData() functions instead of adl_setBank().");
    return -1;
#else
    const uint32_t NumBanks = static_cast<uint32_t>(g_embeddedBanksCount);
    int32_t bankno = bank;

    if(bankno < 0)
        bankno = 0;

    MidiPlayer *play = GET_MIDI_PLAYER(device);
    assert(play);
    if(static_cast<uint32_t>(bankno) >= NumBanks)
    {
        char errBuf[150];
        snprintf(errBuf, 150, "Embedded bank number may only be 0..%u!\n", static_cast<unsigned int>(NumBanks - 1));
        play->setErrorString(errBuf);
        return -1;
    }

    Synth &synth = *play->m_synth;
    play->m_setup.bankId = static_cast<uint32_t>(bankno);
    synth.setEmbeddedBank(play->m_setup.bankId);
    play->applySetup();

    return 0;
#endif
}

ADLMIDI_EXPORT int adl_getBanksCount()
{
#ifndef DISABLE_EMBEDDED_BANKS
    return static_cast<int>(g_embeddedBanksCount);
#else
    return 0;
#endif
}

ADLMIDI_EXPORT const char *const *adl_getBankNames()
{
#ifndef DISABLE_EMBEDDED_BANKS
    return g_embeddedBankNames;
#else
    return NULL;
#endif
}

ADLMIDI_EXPORT int adl_reserveBanks(ADL_MIDIPlayer *device, unsigned banks)
{
    if(!device)
        return -1;
    MidiPlayer *play = GET_MIDI_PLAYER(device);
    assert(play);
    Synth::BankMap &map = play->m_synth->m_insBanks;
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
    size_t idnumber = ((id.msb << 8) | id.lsb | (id.percussive ? size_t(Synth::PercussionTag) : 0));

    MidiPlayer *play = GET_MIDI_PLAYER(device);
    assert(play);
    Synth::BankMap &map = play->m_synth->m_insBanks;

    Synth::BankMap::iterator it;
    if(!(flags & ADLMIDI_Bank_Create))
    {
        it = map.find(idnumber);
        if(it == map.end())
            return -1;
    }
    else
    {
        std::pair<size_t, Synth::Bank> value;
        value.first = idnumber;
        memset(&value.second, 0, sizeof(value.second));
        for (unsigned i = 0; i < 128; ++i)
            value.second.ins[i].flags = OplInstMeta::Flag_NoSound;

        std::pair<Synth::BankMap::iterator, bool> ir;
        if((flags & ADLMIDI_Bank_CreateRt) == ADLMIDI_Bank_CreateRt)
        {
            ir = map.insert(value, Synth::BankMap::do_not_expand_t());
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

    Synth::BankMap::iterator it = Synth::BankMap::iterator::from_ptrs(bank->pointer);
    Synth::BankMap::key_type idnumber = it->first;
    id->msb = (idnumber >> 8) & 127;
    id->lsb = idnumber & 127;
    id->percussive = (idnumber & Synth::PercussionTag) ? 1 : 0;
    return 0;
}

ADLMIDI_EXPORT int adl_removeBank(ADL_MIDIPlayer *device, ADL_Bank *bank)
{
    if(!device || !bank)
        return -1;

    MidiPlayer *play = GET_MIDI_PLAYER(device);
    assert(play);
    Synth::BankMap &map = play->m_synth->m_insBanks;
    Synth::BankMap::iterator it = Synth::BankMap::iterator::from_ptrs(bank->pointer);
    size_t size = map.size();
    map.erase(it);
    return (map.size() != size) ? 0 : -1;
}

ADLMIDI_EXPORT int adl_getFirstBank(ADL_MIDIPlayer *device, ADL_Bank *bank)
{
    if(!device)
        return -1;

    MidiPlayer *play = GET_MIDI_PLAYER(device);
    assert(play);
    Synth::BankMap &map = play->m_synth->m_insBanks;

    Synth::BankMap::iterator it = map.begin();
    if(it == map.end())
        return -1;

    it.to_ptrs(bank->pointer);
    return 0;
}

ADLMIDI_EXPORT int adl_getNextBank(ADL_MIDIPlayer *device, ADL_Bank *bank)
{
    if(!device)
        return -1;

    MidiPlayer *play = GET_MIDI_PLAYER(device);
    assert(play);
    Synth::BankMap &map = play->m_synth->m_insBanks;

    Synth::BankMap::iterator it = Synth::BankMap::iterator::from_ptrs(bank->pointer);
    if(++it == map.end())
        return -1;

    it.to_ptrs(bank->pointer);
    return 0;
}

ADLMIDI_EXPORT int adl_getInstrument(ADL_MIDIPlayer *device, const ADL_Bank *bank, unsigned index, ADL_Instrument *ins)
{
    if(!device || !bank || index > 127 || !ins)
        return -1;

    Synth::BankMap::iterator it = Synth::BankMap::iterator::from_ptrs(bank->pointer);
    cvt_FMIns_to_ADLI(*ins, it->second.ins[index]);
    ins->version = 0;
    return 0;
}

ADLMIDI_EXPORT int adl_setInstrument(ADL_MIDIPlayer *device, ADL_Bank *bank, unsigned index, const ADL_Instrument *ins)
{
    if(!device || !bank || index > 127 || !ins)
        return -1;

    if(ins->version != 0)
        return -1;

    Synth::BankMap::iterator it = Synth::BankMap::iterator::from_ptrs(bank->pointer);
    cvt_ADLI_to_FMIns(it->second.ins[index], *ins);
    return 0;
}

ADLMIDI_EXPORT int adl_loadEmbeddedBank(struct ADL_MIDIPlayer *device, ADL_Bank *bank, int num)
{
    if(!device)
        return -1;

#ifdef DISABLE_EMBEDDED_BANKS
    ADL_UNUSED(bank);
    ADL_UNUSED(num);
    MidiPlayer *play = GET_MIDI_PLAYER(device);
    assert(play);
    play->setErrorString("This build of libADLMIDI has no embedded banks. "
                         "Please load banks by using adl_openBankFile() or "
                         "adl_openBankData() functions instead of adl_loadEmbeddedBank().");
    return -1;
#else
    if(num < 0 || num >= static_cast<int>(g_embeddedBanksCount))
        return -1;

    Synth::BankMap::iterator it = Synth::BankMap::iterator::from_ptrs(bank->pointer);
    size_t id = it->first;

    const BanksDump::BankEntry &bankEntry = g_embeddedBanks[num];

    bool ss = (id & Synth::PercussionTag);
    const size_t bankID = 0;

//    bank_count_t maxBanks = ss ? bankEntry.banksPercussionCount : bankEntry.banksMelodicCount;
    bank_count_t banksOffset = ss ? bankEntry.banksOffsetPercussive : bankEntry.banksOffsetMelodic;
    size_t bankIndex = g_embeddedBanksMidiIndex[banksOffset + bankID];
    const BanksDump::MidiBank &bankData = g_embeddedBanksMidi[bankIndex];

    for (unsigned i = 0; i < 128; ++i)
    {
        midi_bank_idx_t instIdx = bankData.insts[i];
        if(instIdx < 0)
        {
            it->second.ins[i].flags = OplInstMeta::Flag_NoSound;
            continue;
        }
        BanksDump::InstrumentEntry instIn = g_embeddedBanksInstruments[instIdx];
        adlFromInstrument(instIn, it->second.ins[i]);
    }
    return 0;
#endif
}

ADLMIDI_EXPORT int adl_setNumFourOpsChn(ADL_MIDIPlayer *device, int ops4)
{
    if(!device)
        return -1;

    MidiPlayer *play = GET_MIDI_PLAYER(device);
    assert(play);
    if(ops4 > 6 * static_cast<int>(play->m_setup.numChips))
    {
        char errBuff[250];
        snprintf(errBuff, 250, "number of four-op channels may only be 0..%u when %u OPL3 cards are used.\n", (6 * (play->m_setup.numChips)), play->m_setup.numChips);
        play->setErrorString(errBuff);
        return -1;
    }

    Synth &synth = *play->m_synth;
    play->m_setup.numFourOps = ops4;
    if(!synth.setupLocked())
    {
        if(play->m_setup.numFourOps < 0)
            adlCalculateFourOpChannels(play, true);
        else
            synth.m_numFourOps = static_cast<uint32_t>(play->m_setup.numFourOps);
        synth.updateChannelCategories();
    }

    return 0;
}

ADLMIDI_EXPORT int adl_getNumFourOpsChn(struct ADL_MIDIPlayer *device)
{
    if(!device)
        return -2;
    MidiPlayer *play = GET_MIDI_PLAYER(device);
    assert(play);
    return play->m_setup.numFourOps;
}

ADLMIDI_EXPORT int adl_getNumFourOpsChnObtained(struct ADL_MIDIPlayer *device)
{
    if(!device)
        return -2;
    MidiPlayer *play = GET_MIDI_PLAYER(device);
    assert(play);
    return (int)play->m_synth->m_numFourOps;
}

/* !!!DEPRECATED!!! AND !!DUMMIED!! */
ADLMIDI_EXPORT void adl_setPercMode(ADL_MIDIPlayer *device, int percmod)
{
    ADL_UNUSED(device);
    ADL_UNUSED(percmod);
}

ADLMIDI_EXPORT void adl_setHVibrato(ADL_MIDIPlayer *device, int hvibro)
{
    if(!device) return;
    MidiPlayer *play = GET_MIDI_PLAYER(device);
    assert(play);
    Synth &synth = *play->m_synth;
    play->m_setup.deepVibratoMode = hvibro;
    if(!synth.setupLocked())
    {
        synth.m_deepVibratoMode     = play->m_setup.deepVibratoMode < 0 ?
                                        synth.m_insBankSetup.deepVibrato :
                                        (play->m_setup.deepVibratoMode != 0);
        synth.commitDeepFlags();
    }
}

ADLMIDI_EXPORT int adl_getHVibrato(struct ADL_MIDIPlayer *device)
{
    if(!device) return -1;
    MidiPlayer *play = GET_MIDI_PLAYER(device);
    assert(play);
    return play->m_synth->m_deepVibratoMode;
}

ADLMIDI_EXPORT void adl_setHTremolo(ADL_MIDIPlayer *device, int htremo)
{
    if(!device) return;
    MidiPlayer *play = GET_MIDI_PLAYER(device);
    assert(play);
    Synth &synth = *play->m_synth;
    play->m_setup.deepTremoloMode = htremo;
    if(!synth.setupLocked())
    {
        synth.m_deepTremoloMode     = play->m_setup.deepTremoloMode < 0 ?
                                        synth.m_insBankSetup.deepTremolo :
                                        (play->m_setup.deepTremoloMode != 0);
        synth.commitDeepFlags();
    }
}

ADLMIDI_EXPORT int adl_getHTremolo(struct ADL_MIDIPlayer *device)
{
    if(!device) return -1;
    MidiPlayer *play = GET_MIDI_PLAYER(device);
    assert(play);
    return play->m_synth->m_deepTremoloMode;
}

ADLMIDI_EXPORT void adl_setScaleModulators(ADL_MIDIPlayer *device, int smod)
{
    if(!device)
        return;
    MidiPlayer *play = GET_MIDI_PLAYER(device);
    assert(play);
    Synth &synth = *play->m_synth;
    play->m_setup.scaleModulators = smod;
    if(!synth.setupLocked())
    {
        synth.m_scaleModulators     = play->m_setup.scaleModulators < 0 ?
                                        synth.m_insBankSetup.scaleModulators :
                                        (play->m_setup.scaleModulators != 0);
    }
}

ADLMIDI_EXPORT void adl_setFullRangeBrightness(struct ADL_MIDIPlayer *device, int fr_brightness)
{
    if(!device)
        return;
    MidiPlayer *play = GET_MIDI_PLAYER(device);
    assert(play);
    play->m_setup.fullRangeBrightnessCC74 = (fr_brightness != 0);
}

ADLMIDI_EXPORT void adl_setAutoArpeggio(ADL_MIDIPlayer *device, int aaEn)
{
    if(!device)
        return;
    MidiPlayer *play = GET_MIDI_PLAYER(device);
    assert(play);
    play->m_setup.enableAutoArpeggio = (aaEn != 0);
}

ADLMIDI_EXPORT int adl_getAutoArpeggio(ADL_MIDIPlayer *device)
{
    if(!device)
        return 0;
    MidiPlayer *play = GET_MIDI_PLAYER(device);
    assert(play);
    return play->m_setup.enableAutoArpeggio ? 1 : 0;
}

ADLMIDI_EXPORT void adl_setLoopEnabled(ADL_MIDIPlayer *device, int loopEn)
{
#ifndef ADLMIDI_DISABLE_MIDI_SEQUENCER
    if(!device)
        return;
    MidiPlayer *play = GET_MIDI_PLAYER(device);
    assert(play);
    play->m_sequencer->setLoopEnabled(loopEn != 0);
#else
    ADL_UNUSED(device);
    ADL_UNUSED(loopEn);
#endif
}

ADLMIDI_EXPORT void adl_setLoopCount(ADL_MIDIPlayer *device, int loopCount)
{
#ifndef ADLMIDI_DISABLE_MIDI_SEQUENCER
    if(!device)
        return;
    MidiPlayer *play = GET_MIDI_PLAYER(device);
    assert(play);
    play->m_sequencer->setLoopsCount(loopCount);
#else
    ADL_UNUSED(device);
    ADL_UNUSED(loopCount);
#endif
}

ADLMIDI_EXPORT void adl_setLoopHooksOnly(ADL_MIDIPlayer *device, int loopHooksOnly)
{
#ifndef ADLMIDI_DISABLE_MIDI_SEQUENCER
    if(!device)
        return;
    MidiPlayer *play = GET_MIDI_PLAYER(device);
    assert(play);
    play->m_sequencer->setLoopHooksOnly(loopHooksOnly);
#else
    ADL_UNUSED(device);
    ADL_UNUSED(loopHooksOnly);
#endif
}

ADLMIDI_EXPORT void adl_setSoftPanEnabled(ADL_MIDIPlayer *device, int softPanEn)
{
    if(!device)
        return;
    MidiPlayer *play = GET_MIDI_PLAYER(device);
    assert(play);
    play->m_synth->m_softPanning = (softPanEn != 0);
#ifdef ADLMIDI_ENABLE_HW_SERIAL
    if(play->m_setup.serial) // Soft-panning won't work while serial is active
        play->m_synth->m_softPanning = false;
#endif
}

/* !!!DEPRECATED!!! */
ADLMIDI_EXPORT void adl_setLogarithmicVolumes(struct ADL_MIDIPlayer *device, int logvol)
{
    if(!device)
        return;
    MidiPlayer *play = GET_MIDI_PLAYER(device);
    assert(play);
    Synth &synth = *play->m_synth;
    play->m_setup.logarithmicVolumes = (logvol != 0);
    if(!synth.setupLocked())
    {
        if(play->m_setup.logarithmicVolumes)
            synth.setVolumeScaleModel(ADLMIDI_VolumeModel_NativeOPL3);
        else
            synth.setVolumeScaleModel(static_cast<ADLMIDI_VolumeModels>(synth.m_volumeScale));
    }
}

ADLMIDI_EXPORT void adl_setVolumeRangeModel(struct ADL_MIDIPlayer *device, int volumeModel)
{
    if(!device)
        return;
    MidiPlayer *play = GET_MIDI_PLAYER(device);
    assert(play);
    Synth &synth = *play->m_synth;
    play->m_setup.volumeScaleModel = volumeModel;
    if(!synth.setupLocked())
    {
        if(play->m_setup.volumeScaleModel == ADLMIDI_VolumeModel_AUTO)//Use bank default volume model
            synth.m_volumeScale = (Synth::VolumesScale)synth.m_insBankSetup.volumeModel;
        else
            synth.setVolumeScaleModel(static_cast<ADLMIDI_VolumeModels>(volumeModel));
    }
}

ADLMIDI_EXPORT int adl_getVolumeRangeModel(struct ADL_MIDIPlayer *device)
{
    if(!device)
        return -1;
    MidiPlayer *play = GET_MIDI_PLAYER(device);
    assert(play);
    return play->m_synth->getVolumeScaleModel();
}

ADLMIDI_EXPORT void adl_setChannelAllocMode(struct ADL_MIDIPlayer *device, int chanalloc)
{
    if(!device)
        return;
    MidiPlayer *play = GET_MIDI_PLAYER(device);
    assert(play);
    Synth &synth = *play->m_synth;

    if(chanalloc < -1 || chanalloc >= ADLMIDI_ChanAlloc_Count)
        chanalloc = ADLMIDI_ChanAlloc_AUTO;

    synth.m_channelAlloc = static_cast<ADLMIDI_ChannelAlloc>(chanalloc);
}

ADLMIDI_EXPORT int adl_getChannelAllocMode(struct ADL_MIDIPlayer *device)
{
    if(!device)
        return -1;
    MidiPlayer *play = GET_MIDI_PLAYER(device);
    assert(play);
    return static_cast<int>(play->m_synth->m_channelAlloc);
}

ADLMIDI_EXPORT int adl_openBankFile(struct ADL_MIDIPlayer *device, const char *filePath)
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
                play->setErrorString("ADL MIDI: Can't load file");
            return -1;
        }
        else
            return adlCalculateFourOpChannels(play, true);
    }

    ADLMIDI_ErrorString = "Can't load file: ADLMIDI is not initialized";
    return -1;
}

ADLMIDI_EXPORT int adl_openBankData(struct ADL_MIDIPlayer *device, const void *mem, unsigned long size)
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
                play->setErrorString("ADL MIDI: Can't load data from memory");
            return -1;
        }
        else
            return adlCalculateFourOpChannels(play, true);
    }

    ADLMIDI_ErrorString = "Can't load file: ADL MIDI is not initialized";
    return -1;
}

ADLMIDI_EXPORT int adl_openFile(ADL_MIDIPlayer *device, const char *filePath)
{
    if(device)
    {
        MidiPlayer *play = GET_MIDI_PLAYER(device);
        assert(play);
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
        ADL_UNUSED(filePath);
        play->setErrorString("ADLMIDI: MIDI Sequencer is not supported in this build of library!");
        return -1;
#endif //ADLMIDI_DISABLE_MIDI_SEQUENCER
    }

    ADLMIDI_ErrorString = "Can't load file: ADL MIDI is not initialized";
    return -1;
}

ADLMIDI_EXPORT int adl_openData(ADL_MIDIPlayer *device, const void *mem, unsigned long size)
{
    if(device)
    {
        MidiPlayer *play = GET_MIDI_PLAYER(device);
        assert(play);
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
        ADL_UNUSED(mem);
        ADL_UNUSED(size);
        play->setErrorString("ADLMIDI: MIDI Sequencer is not supported in this build of library!");
        return -1;
#endif //ADLMIDI_DISABLE_MIDI_SEQUENCER
    }
    ADLMIDI_ErrorString = "Can't load file: ADL MIDI is not initialized";
    return -1;
}

ADLMIDI_EXPORT void adl_selectSongNum(struct ADL_MIDIPlayer *device, int songNumber)
{
#ifndef ADLMIDI_DISABLE_MIDI_SEQUENCER
    if(!device)
        return;

    MidiPlayer *play = GET_MIDI_PLAYER(device);
    assert(play);
    play->m_sequencer->setSongNum(songNumber);
#else
    ADL_UNUSED(device);
    ADL_UNUSED(songNumber);
#endif
}

ADLMIDI_EXPORT int adl_getSongsCount(struct ADL_MIDIPlayer *device)
{
#ifndef ADLMIDI_DISABLE_MIDI_SEQUENCER
    if(!device)
        return 0;

    MidiPlayer *play = GET_MIDI_PLAYER(device);
    assert(play);
    return play->m_sequencer->getSongsCount();
#else
    ADL_UNUSED(device);
    return 0;
#endif
}

ADLMIDI_EXPORT const char *adl_emulatorName()
{
    return "<adl_emulatorName() is deprecated! Use adl_chipEmulatorName() instead!>";
}

ADLMIDI_EXPORT const char *adl_chipEmulatorName(struct ADL_MIDIPlayer *device)
{
    if(device)
    {
        MidiPlayer *play = GET_MIDI_PLAYER(device);
        assert(play);
        Synth &synth = *play->m_synth;
        if(!synth.m_chips.empty())
            return synth.m_chips[0]->emulatorName();
    }
    return "Unknown";
}

ADLMIDI_EXPORT int adl_switchEmulator(struct ADL_MIDIPlayer *device, int emulator)
{
    if(device)
    {
        MidiPlayer *play = GET_MIDI_PLAYER(device);
        assert(play);
        if(adl_isEmulatorAvailable(emulator))
        {
            play->m_setup.emulator = emulator;
#ifdef ADLMIDI_ENABLE_HW_SERIAL
            play->m_setup.serial = false;
#endif
            play->partialReset();
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
        MidiPlayer *play = GET_MIDI_PLAYER(device);
        assert(play);
        Synth &synth = *play->m_synth;
        play->m_setup.runAtPcmRate = (enabled != 0);
        if(!synth.setupLocked())
            play->partialReset();
        return 0;
    }
    return -1;
}

ADLMIDI_EXPORT int adl_switchSerialHW(struct ADL_MIDIPlayer *device,
                                      const char *name,
                                      unsigned baud,
                                      unsigned protocol)
{
    if(device)
    {
        MidiPlayer *play = GET_MIDI_PLAYER(device);
        assert(play);
#ifdef ADLMIDI_ENABLE_HW_SERIAL
        play->m_setup.serial = true;
        play->m_setup.serialName = std::string(name);
        play->m_setup.serialBaud = baud;
        play->m_setup.serialProtocol = protocol;
        play->partialReset();
        return 0;
#else
        (void)name; (void)baud; (void)protocol;
        play->setErrorString("ADLMIDI: The hardware serial mode is not enabled in this build");
        return -1;
#endif
    }

    return -1;
}

int adl_switchDOSHW(int chipType, ADL_UInt16 baseAddress)
{
#ifdef ENABLE_HW_OPL_DOS
    if(baseAddress > 0)
        DOS_HW_OPL::setOplAddress(baseAddress);

    switch(chipType)
    {
    case ADLMIDI_DOS_ChipAuto:
        break;

    case ADLMIDI_DOS_ChipOPL2:
        DOS_HW_OPL::setChipType(OPLChipBase::CHIPTYPE_OPL2);
        break;
    case ADLMIDI_DOS_ChipOPL3:
        DOS_HW_OPL::setChipType(OPLChipBase::CHIPTYPE_OPL3);
        break;
    }
#else
    (void)chipType;
    (void)baseAddress;
#endif
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
    MidiPlayer *play = GET_MIDI_PLAYER(device);
    if(!play)
        return adl_errorString();
    return play->getErrorString().c_str();
}

ADLMIDI_EXPORT void adl_reset(struct ADL_MIDIPlayer *device)
{
    if(!device)
        return;
    MidiPlayer *play = GET_MIDI_PLAYER(device);
    assert(play);
    play->partialReset();
    play->resetMIDI();
}

ADLMIDI_EXPORT double adl_totalTimeLength(struct ADL_MIDIPlayer *device)
{
#ifndef ADLMIDI_DISABLE_MIDI_SEQUENCER
    if(!device)
        return -1.0;
    MidiPlayer *play = GET_MIDI_PLAYER(device);
    assert(play);
    return play->m_sequencer->timeLength();
#else
    ADL_UNUSED(device);
    return -1.0;
#endif
}

ADLMIDI_EXPORT double adl_loopStartTime(struct ADL_MIDIPlayer *device)
{
#ifndef ADLMIDI_DISABLE_MIDI_SEQUENCER
    if(!device)
        return -1.0;
    MidiPlayer *play = GET_MIDI_PLAYER(device);
    assert(play);
    return play->m_sequencer->getLoopStart();
#else
    ADL_UNUSED(device);
    return -1.0;
#endif
}

ADLMIDI_EXPORT double adl_loopEndTime(struct ADL_MIDIPlayer *device)
{
#ifndef ADLMIDI_DISABLE_MIDI_SEQUENCER
    if(!device)
        return -1.0;
    MidiPlayer *play = GET_MIDI_PLAYER(device);
    assert(play);
    return play->m_sequencer->getLoopEnd();
#else
    ADL_UNUSED(device);
    return -1.0;
#endif
}

ADLMIDI_EXPORT double adl_positionTell(struct ADL_MIDIPlayer *device)
{
#ifndef ADLMIDI_DISABLE_MIDI_SEQUENCER
    if(!device)
        return -1.0;
    MidiPlayer *play = GET_MIDI_PLAYER(device);
    assert(play);
    return play->m_sequencer->tell();
#else
    ADL_UNUSED(device);
    return -1.0;
#endif
}

ADLMIDI_EXPORT void adl_positionSeek(struct ADL_MIDIPlayer *device, double seconds)
{
#ifndef ADLMIDI_DISABLE_MIDI_SEQUENCER
    if(seconds < 0.0)
        return;//Seeking negative position is forbidden! :-P
    if(!device)
        return;
    MidiPlayer *play = GET_MIDI_PLAYER(device);
    assert(play);
    play->realTime_panic();
    play->m_setup.delay = play->m_sequencer->seek(seconds, play->m_setup.mindelay);
    play->m_setup.carry = 0.0;
#else
    ADL_UNUSED(device);
    ADL_UNUSED(seconds);
#endif
}

ADLMIDI_EXPORT void adl_positionRewind(struct ADL_MIDIPlayer *device)
{
#ifndef ADLMIDI_DISABLE_MIDI_SEQUENCER
    if(!device)
        return;
    MidiPlayer *play = GET_MIDI_PLAYER(device);
    assert(play);
    play->realTime_panic();
    play->m_sequencer->rewind();
#else
    ADL_UNUSED(device);
#endif
}

ADLMIDI_EXPORT void adl_setTempo(struct ADL_MIDIPlayer *device, double tempo)
{
#ifndef ADLMIDI_DISABLE_MIDI_SEQUENCER
    if(!device || (tempo <= 0.0))
        return;
    MidiPlayer *play = GET_MIDI_PLAYER(device);
    assert(play);
    play->m_sequencer->setTempo(tempo);
#else
    ADL_UNUSED(device);
    ADL_UNUSED(tempo);
#endif
}


ADLMIDI_EXPORT int adl_describeChannels(struct ADL_MIDIPlayer *device, char *str, char *attr, size_t size)
{
    if(!device)
        return -1;
    MidiPlayer *play = GET_MIDI_PLAYER(device);
    assert(play);
    play->describeChannels(str, attr, size);
    return 0;
}


ADLMIDI_EXPORT const char *adl_metaMusicTitle(struct ADL_MIDIPlayer *device)
{
#ifndef ADLMIDI_DISABLE_MIDI_SEQUENCER
    if(!device)
        return "";
    MidiPlayer *play = GET_MIDI_PLAYER(device);
    assert(play);
    return play->m_sequencer->getMusicTitle().c_str();
#else
    ADL_UNUSED(device);
    return "";
#endif
}


ADLMIDI_EXPORT const char *adl_metaMusicCopyright(struct ADL_MIDIPlayer *device)
{
#ifndef ADLMIDI_DISABLE_MIDI_SEQUENCER
    if(!device)
        return "";
    MidiPlayer *play = GET_MIDI_PLAYER(device);
    assert(play);
    return play->m_sequencer->getMusicCopyright().c_str();
#else
    ADL_UNUSED(device);
    return "";
#endif
}

ADLMIDI_EXPORT size_t adl_metaTrackTitleCount(struct ADL_MIDIPlayer *device)
{
#ifndef ADLMIDI_DISABLE_MIDI_SEQUENCER
    if(!device)
        return 0;
    MidiPlayer *play = GET_MIDI_PLAYER(device);
    assert(play);
    return play->m_sequencer->getTrackTitles().size();
#else
    ADL_UNUSED(device);
    return 0;
#endif
}

ADLMIDI_EXPORT const char *adl_metaTrackTitle(struct ADL_MIDIPlayer *device, size_t index)
{
#ifndef ADLMIDI_DISABLE_MIDI_SEQUENCER
    if(!device)
        return "";
    MidiPlayer *play = GET_MIDI_PLAYER(device);
    assert(play);
    const std::vector<std::string> &titles = play->m_sequencer->getTrackTitles();
    if(index >= titles.size())
        return "INVALID";
    return titles[index].c_str();
#else
    ADL_UNUSED(device);
    ADL_UNUSED(index);
    return "NOT SUPPORTED";
#endif
}


ADLMIDI_EXPORT size_t adl_metaMarkerCount(struct ADL_MIDIPlayer *device)
{
#ifndef ADLMIDI_DISABLE_MIDI_SEQUENCER
    if(!device)
        return 0;
    MidiPlayer *play = GET_MIDI_PLAYER(device);
    assert(play);
    return play->m_sequencer->getMarkers().size();
#else
    ADL_UNUSED(device);
    return 0;
#endif
}

ADLMIDI_EXPORT Adl_MarkerEntry adl_metaMarker(struct ADL_MIDIPlayer *device, size_t index)
{
    struct Adl_MarkerEntry marker;

#ifndef ADLMIDI_DISABLE_MIDI_SEQUENCER
    if(!device)
    {
        marker.label = "INVALID";
        marker.pos_time = 0.0;
        marker.pos_ticks = 0;
        return marker;
    }

    MidiPlayer *play = GET_MIDI_PLAYER(device);
    assert(play);

    const std::vector<MidiSequencer::MIDI_MarkerEntry> &markers = play->m_sequencer->getMarkers();
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
    ADL_UNUSED(device);
    ADL_UNUSED(index);
    marker.label = "NOT SUPPORTED";
    marker.pos_time = 0.0;
    marker.pos_ticks = 0;
#endif
    return marker;
}

ADLMIDI_EXPORT void adl_setRawEventHook(struct ADL_MIDIPlayer *device, ADL_RawEventHook rawEventHook, void *userData)
{
#ifndef ADLMIDI_DISABLE_MIDI_SEQUENCER
    if(!device)
        return;
    MidiPlayer *play = GET_MIDI_PLAYER(device);
    assert(play);
    play->m_sequencerInterface->onEvent = rawEventHook;
    play->m_sequencerInterface->onEvent_userData = userData;
#else
    ADL_UNUSED(device);
    ADL_UNUSED(rawEventHook);
    ADL_UNUSED(userData);
#endif
}

/* Set note hook */
ADLMIDI_EXPORT void adl_setNoteHook(struct ADL_MIDIPlayer *device, ADL_NoteHook noteHook, void *userData)
{
    if(!device)
        return;
    MidiPlayer *play = GET_MIDI_PLAYER(device);
    assert(play);
    play->hooks.onNote = noteHook;
    play->hooks.onNote_userData = userData;
}

/* Set debug message hook */
ADLMIDI_EXPORT void adl_setDebugMessageHook(struct ADL_MIDIPlayer *device, ADL_DebugMessageHook debugMessageHook, void *userData)
{
    if(!device)
        return;
    MidiPlayer *play = GET_MIDI_PLAYER(device);
    assert(play);
    play->hooks.onDebugMessage = debugMessageHook;
    play->hooks.onDebugMessage_userData = userData;
#ifndef ADLMIDI_DISABLE_MIDI_SEQUENCER
    play->m_sequencerInterface->onDebugMessage = debugMessageHook;
    play->m_sequencerInterface->onDebugMessage_userData = userData;
#endif
}

/* Set loop start hook */
ADLMIDI_EXPORT void adl_setLoopStartHook(struct ADL_MIDIPlayer *device, ADL_LoopPointHook loopStartHook, void *userData)
{
    if(!device)
        return;
    MidiPlayer *play = GET_MIDI_PLAYER(device);
    assert(play);
    play->hooks.onLoopStart = loopStartHook;
    play->hooks.onLoopStart_userData = userData;
#ifndef ADLMIDI_DISABLE_MIDI_SEQUENCER
    play->m_sequencerInterface->onloopStart = loopStartHook;
    play->m_sequencerInterface->onloopStart_userData = userData;
#endif
}

/* Set loop end hook */
ADLMIDI_EXPORT void adl_setLoopEndHook(struct ADL_MIDIPlayer *device, ADL_LoopPointHook loopEndHook, void *userData)
{
    if(!device)
        return;
    MidiPlayer *play = GET_MIDI_PLAYER(device);
    assert(play);
    play->hooks.onLoopEnd = loopEndHook;
    play->hooks.onLoopEnd_userData = userData;
#ifndef ADLMIDI_DISABLE_MIDI_SEQUENCER
    play->m_sequencerInterface->onloopEnd = loopEndHook;
    play->m_sequencerInterface->onloopEnd_userData = userData;
#endif
}

#ifndef ADLMIDI_HW_OPL

#   ifndef __WATCOMC__
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
        *(Dst *)(dstLeft + (i * sampleOffset)) = static_cast<Dst>(transform(src[2 * i]));
        *(Dst *)(dstRight + (i * sampleOffset)) = static_cast<Dst>(transform(src[(2 * i) + 1]));
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
#   else // __WATCOMC__

/*
    Workaround for OpenWattcom where templates are declared above are causing compiler to be crashed
*/
static void CopySamplesTransformed(ADL_UInt8 *dstLeft, ADL_UInt8 *dstRight, const int32_t *src,
                                   size_t frameCount, unsigned sampleOffset,
                                   int32_t(&transform)(int32_t))
{
    for(size_t i = 0; i < frameCount; ++i) {
        *(int16_t *)(dstLeft + (i * sampleOffset)) = (int16_t)transform(src[2 * i]);
        *(int16_t *)(dstRight + (i * sampleOffset)) = (int16_t)transform(src[(2 * i) + 1]);
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

    if(sampleType == ADLMIDI_SampleType_U16)
    {
        switch(containerSize) {
        case sizeof(int16_t):
            CopySamplesTransformed(left, right, _in, toCopy / 2, sampleOffset, adl_cvtS16);
            break;
        default:
            return -1;
        }
    }
    else
        return -1;
    return 0;
}
#   endif // __WATCOM__

#endif // ADLMIDI_HW_OPL


ADLMIDI_EXPORT int adl_play(struct ADL_MIDIPlayer *device, int sampleCount, short *out)
{
    return adl_playFormat(device, sampleCount, (ADL_UInt8 *)out, (ADL_UInt8 *)(out + 1), &adl_DefaultAudioFormat);
}

ADLMIDI_EXPORT int adl_playFormat(ADL_MIDIPlayer *device, int sampleCount,
                                  ADL_UInt8 *out_left, ADL_UInt8 *out_right,
                                  const ADLMIDI_AudioFormat *format)
{
#if defined(ADLMIDI_DISABLE_MIDI_SEQUENCER) || defined(ADLMIDI_HW_OPL)
    ADL_UNUSED(device);
    ADL_UNUSED(sampleCount);
    ADL_UNUSED(out_left);
    ADL_UNUSED(out_right);
    ADL_UNUSED(format);
    return 0;
#endif

#if !defined(ADLMIDI_DISABLE_MIDI_SEQUENCER) && !defined(ADLMIDI_HW_OPL)
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
            if((player->m_sequencer->positionAtEnd()) && (setup.delay <= 0.0))
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
            Synth &synth = *player->m_synth;
            unsigned int chips = synth.m_numChips;
            if(chips == 1)
                synth.m_chips[0]->generate32(out_buf, (size_t)in_generatedStereo);
            else/* if(n_periodCountStereo > 0)*/
            {
                /* Generate data from every chip and mix result */
                for(size_t card = 0; card < chips; ++card)
                    synth.m_chips[card]->generateAndMix32(out_buf, (size_t)in_generatedStereo);
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
        {
            setup.delay = player->Tick(eat_delay, setup.mindelay);
            player->TickIterators(eat_delay);
        }
    }

    return static_cast<int>(gotten_len);
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
    ADL_UNUSED(device);
    ADL_UNUSED(sampleCount);
    ADL_UNUSED(out_left);
    ADL_UNUSED(out_right);
    ADL_UNUSED(format);
    return 0;
#else
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
    double  delay = double(sampleCount / 2) / double(setup.PCM_RATE);

    while(left > 0)
    {
        if(delay <= 0.0)
            delay = double(left / 2) / double(setup.PCM_RATE);
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
            Synth &synth = *player->m_synth;
            unsigned int chips = synth.m_numChips;
            if(chips == 1)
                synth.m_chips[0]->generate32(out_buf, (size_t)in_generatedStereo);
            else if(n_periodCountStereo > 0)
            {
                /* Generate data from every chip and mix result */
                for(unsigned card = 0; card < chips; ++card)
                    synth.m_chips[card]->generateAndMix32(out_buf, (size_t)in_generatedStereo);
            }
            /* Process it */
            if(SendStereoAudio(sampleCount, in_generatedStereo, out_buf, gotten_len, out_left, out_right, format) == -1)
                return 0;

            left -= (int)in_generatedPhys;
            gotten_len += (in_generatedPhys) /* - setup.stored_samples*/;
        }

        player->TickIterators(eat_delay);
    }

    return static_cast<int>(gotten_len);
#endif
}

ADLMIDI_EXPORT double adl_tickEvents(struct ADL_MIDIPlayer *device, double seconds, double granulality)
{
#ifndef ADLMIDI_DISABLE_MIDI_SEQUENCER
    if(!device)
        return -1.0;
    MidiPlayer *play = GET_MIDI_PLAYER(device);
    assert(play);
    double ret = play->Tick(seconds, granulality);
    play->TickIterators(seconds);
    return ret;
#else
    ADL_UNUSED(device);
    ADL_UNUSED(seconds);
    ADL_UNUSED(granulality);
    return -1.0;
#endif
}

ADLMIDI_EXPORT double adl_tickEventsOnly(struct ADL_MIDIPlayer *device, double seconds, double granulality)
{
#ifndef ADLMIDI_DISABLE_MIDI_SEQUENCER
    if(!device)
        return -1.0;
    MidiPlayer *play = GET_MIDI_PLAYER(device);
    assert(play);
    return play->Tick(seconds, granulality);
#else
    ADL_UNUSED(device);
    ADL_UNUSED(seconds);
    ADL_UNUSED(granulality);
    return -1.0;
#endif
}

ADLMIDI_EXPORT void adl_tickIterators(struct ADL_MIDIPlayer *device, double seconds)
{
    if(!device)
        return;
    MidiPlayer *play = GET_MIDI_PLAYER(device);
    play->TickIterators(seconds);
}

ADLMIDI_EXPORT int adl_atEnd(struct ADL_MIDIPlayer *device)
{
#ifndef ADLMIDI_DISABLE_MIDI_SEQUENCER
    if(!device)
        return 1;
    MidiPlayer *play = GET_MIDI_PLAYER(device);
    assert(play);
    return (int)play->m_sequencer->positionAtEnd();
#else
    ADL_UNUSED(device);
    return 1;
#endif
}

ADLMIDI_EXPORT size_t adl_trackCount(struct ADL_MIDIPlayer *device)
{
#ifndef ADLMIDI_DISABLE_MIDI_SEQUENCER
    if(!device)
        return 0;
    MidiPlayer *play = GET_MIDI_PLAYER(device);
    assert(play);
    return play->m_sequencer->getTrackCount();
#else
    ADL_UNUSED(device);
    return 0;
#endif
}

ADLMIDI_EXPORT int adl_setTrackOptions(struct ADL_MIDIPlayer *device, size_t trackNumber, unsigned trackOptions)
{
#ifndef ADLMIDI_DISABLE_MIDI_SEQUENCER
    if(!device)
        return -1;
    MidiPlayer *play = GET_MIDI_PLAYER(device);
    assert(play);
    MidiSequencer &seq = *play->m_sequencer;

    unsigned enableFlag = trackOptions & 3;
    trackOptions &= ~3u;

    // handle on/off/solo
    switch(enableFlag)
    {
    default:
        break;
    case ADLMIDI_TrackOption_On:
    case ADLMIDI_TrackOption_Off:
        if(!seq.setTrackEnabled(trackNumber, enableFlag == ADLMIDI_TrackOption_On))
            return -1;
        break;
    case ADLMIDI_TrackOption_Solo:
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

ADLMIDI_EXPORT int adl_setChannelEnabled(struct ADL_MIDIPlayer *device, size_t channelNumber, int enabled)
{
#ifndef ADLMIDI_DISABLE_MIDI_SEQUENCER
    if(!device)
        return -1;

    MidiPlayer *play = GET_MIDI_PLAYER(device);
    assert(play);
    MidiSequencer &seq = *play->m_sequencer;

    if(!seq.setChannelEnabled(channelNumber, (bool)enabled))
        return -1;
    return 0;
#else
    ADL_UNUSED(device);
    ADL_UNUSED(channelNumber);
    ADL_UNUSED(enabled);
    return -1;
#endif
}

ADLMIDI_EXPORT int adl_setTriggerHandler(struct ADL_MIDIPlayer *device, ADL_TriggerHandler handler, void *userData)
{
#ifndef ADLMIDI_DISABLE_MIDI_SEQUENCER
    if(!device)
        return -1;
    MidiPlayer *play = GET_MIDI_PLAYER(device);
    assert(play);
    MidiSequencer &seq = *play->m_sequencer;
    seq.setTriggerHandler(handler, userData);
    return 0;
#else
    ADL_UNUSED(device);
    ADL_UNUSED(handler);
    ADL_UNUSED(userData);
    return -1;
#endif
}

ADLMIDI_EXPORT void adl_panic(struct ADL_MIDIPlayer *device)
{
    if(!device)
        return;
    MidiPlayer *play = GET_MIDI_PLAYER(device);
    assert(play);
    play->realTime_panic();
}

ADLMIDI_EXPORT void adl_rt_resetState(struct ADL_MIDIPlayer *device)
{
    if(!device)
        return;
    MidiPlayer *play = GET_MIDI_PLAYER(device);
    assert(play);
    play->realTime_ResetState();
}

ADLMIDI_EXPORT int adl_rt_noteOn(struct ADL_MIDIPlayer *device, ADL_UInt8 channel, ADL_UInt8 note, ADL_UInt8 velocity)
{
    if(!device)
        return 0;
    MidiPlayer *play = GET_MIDI_PLAYER(device);
    assert(play);
    return (int)play->realTime_NoteOn(channel, note, velocity);
}

ADLMIDI_EXPORT void adl_rt_noteOff(struct ADL_MIDIPlayer *device, ADL_UInt8 channel, ADL_UInt8 note)
{
    if(!device)
        return;
    MidiPlayer *play = GET_MIDI_PLAYER(device);
    assert(play);
    play->realTime_NoteOff(channel, note);
}

ADLMIDI_EXPORT void adl_rt_noteAfterTouch(struct ADL_MIDIPlayer *device, ADL_UInt8 channel, ADL_UInt8 note, ADL_UInt8 atVal)
{
    if(!device)
        return;
    MidiPlayer *play = GET_MIDI_PLAYER(device);
    assert(play);
    play->realTime_NoteAfterTouch(channel, note, atVal);
}

ADLMIDI_EXPORT void adl_rt_channelAfterTouch(struct ADL_MIDIPlayer *device, ADL_UInt8 channel, ADL_UInt8 atVal)
{
    if(!device)
        return;
    MidiPlayer *play = GET_MIDI_PLAYER(device);
    assert(play);
    play->realTime_ChannelAfterTouch(channel, atVal);
}

ADLMIDI_EXPORT void adl_rt_controllerChange(struct ADL_MIDIPlayer *device, ADL_UInt8 channel, ADL_UInt8 type, ADL_UInt8 value)
{
    if(!device)
        return;
    MidiPlayer *play = GET_MIDI_PLAYER(device);
    assert(play);
    play->realTime_Controller(channel, type, value);
}

ADLMIDI_EXPORT void adl_rt_patchChange(struct ADL_MIDIPlayer *device, ADL_UInt8 channel, ADL_UInt8 patch)
{
    if(!device)
        return;
    MidiPlayer *play = GET_MIDI_PLAYER(device);
    assert(play);
    play->realTime_PatchChange(channel, patch);
}

ADLMIDI_EXPORT void adl_rt_pitchBend(struct ADL_MIDIPlayer *device, ADL_UInt8 channel, ADL_UInt16 pitch)
{
    if(!device)
        return;
    MidiPlayer *play = GET_MIDI_PLAYER(device);
    assert(play);
    play->realTime_PitchBend(channel, pitch);
}

ADLMIDI_EXPORT void adl_rt_pitchBendML(struct ADL_MIDIPlayer *device, ADL_UInt8 channel, ADL_UInt8 msb, ADL_UInt8 lsb)
{
    if(!device)
        return;
    MidiPlayer *play = GET_MIDI_PLAYER(device);
    assert(play);
    play->realTime_PitchBend(channel, msb, lsb);
}

ADLMIDI_EXPORT void adl_rt_bankChangeLSB(struct ADL_MIDIPlayer *device, ADL_UInt8 channel, ADL_UInt8 lsb)
{
    if(!device)
        return;
    MidiPlayer *play = GET_MIDI_PLAYER(device);
    assert(play);
    play->realTime_BankChangeLSB(channel, lsb);
}

ADLMIDI_EXPORT void adl_rt_bankChangeMSB(struct ADL_MIDIPlayer *device, ADL_UInt8 channel, ADL_UInt8 msb)
{
    if(!device)
        return;
    MidiPlayer *play = GET_MIDI_PLAYER(device);
    assert(play);
    play->realTime_BankChangeMSB(channel, msb);
}

ADLMIDI_EXPORT void adl_rt_bankChange(struct ADL_MIDIPlayer *device, ADL_UInt8 channel, ADL_SInt16 bank)
{
    if(!device)
        return;
    MidiPlayer *play = GET_MIDI_PLAYER(device);
    assert(play);
    play->realTime_BankChange(channel, (uint16_t)bank);
}

ADLMIDI_EXPORT int adl_rt_systemExclusive(struct ADL_MIDIPlayer *device, const ADL_UInt8 *msg, size_t size)
{
    if(!device)
        return -1;
    MidiPlayer *play = GET_MIDI_PLAYER(device);
    assert(play);
    return play->realTime_SysEx(msg, size);
}
