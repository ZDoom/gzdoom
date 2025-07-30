/*
** music_timidity_mididevice.cpp
** Provides access to TiMidity as a generic MIDI device.
**
**---------------------------------------------------------------------------
** Copyright 2008 Randy Heit
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
*/

// HEADER FILES ------------------------------------------------------------

#include <stdexcept>
#include <stdlib.h>

#include "zmusic/zmusic_internal.h"
#include "mididevice.h"

#ifdef HAVE_ADL
#include "adlmidi.h"
#include "wopl/wopl_file.h"
#include "oplsynth/genmidi.h"

ADLConfig adlConfig;

class ADLMIDIDevice : public SoftSynthMIDIDevice
{
	struct ADL_MIDIPlayer *Renderer;
	float OutputGainFactor;
	float ConfigGainFactor;
	std::string custom_bank;
	std::vector<uint8_t> genmidi;
	bool use_custom_bank;
	bool use_genmidi;
	int last_bank;
public:
	ADLMIDIDevice(const ADLConfig *config);
	~ADLMIDIDevice();
	
	int OpenRenderer() override;
	int GetDeviceType() const override { return MDEV_ADL; }
	void ChangeSettingInt(const char *setting, int value) override;
	void ChangeSettingNum(const char *setting, double value) override;
	void ChangeSettingString(const char *setting, const char *value) override;

protected:
	
	void HandleEvent(int status, int parm1, int parm2) override;
	void HandleLongEvent(const uint8_t *data, int len) override;
	void ComputeOutput(float *buffer, int len) override;
	
private:
	void initGain();
	int LoadCustomBank(const ADLConfig *config);
	void OP2_To_WOPL(const ADLConfig *config);
};


enum
{
	ME_NOTEOFF = 0x80,
	ME_NOTEON = 0x90,
	ME_KEYPRESSURE = 0xA0,
	ME_CONTROLCHANGE = 0xB0,
	ME_PROGRAM = 0xC0,
	ME_CHANNELPRESSURE = 0xD0,
	ME_PITCHWHEEL = 0xE0
};

//==========================================================================
//
// ADLMIDIDevice Constructor
//
//==========================================================================

ADLMIDIDevice::ADLMIDIDevice(const ADLConfig *config)
	:SoftSynthMIDIDevice(44100)
{
	Renderer = adl_init(44100);	// todo: make it configurable
	OutputGainFactor = 3.5f;
	ConfigGainFactor = 1.0f;
	if (Renderer != nullptr)
	{
		adl_switchEmulator(Renderer, config->adl_emulator_id);
		adl_setRunAtPcmRate(Renderer, config->adl_run_at_pcm_rate);
		last_bank = config->adl_bank;
		use_genmidi = config->adl_use_genmidi;
		if (config->adl_genmidi_set)
			OP2_To_WOPL(config);
		if (!LoadCustomBank(config))
			adl_setBank(Renderer, config->adl_bank);
		adl_setNumChips(Renderer, config->adl_chips_count);
		adl_setVolumeRangeModel(Renderer, config->adl_volume_model);
		adl_setChannelAllocMode(Renderer, config->adl_chan_alloc);
		adl_setSoftPanEnabled(Renderer, config->adl_fullpan);
		adl_setAutoArpeggio(Renderer, (int)config->adl_auto_arpeggio);
		ConfigGainFactor = config->adl_gain;
		initGain();
	}
	else throw std::runtime_error("Failed to create ADL MIDI renderer.");
}

//==========================================================================
//
// ADLMIDIDevice Destructor
//
//==========================================================================

ADLMIDIDevice::~ADLMIDIDevice()
{
	Close();
	if (Renderer != nullptr)
	{
		adl_close(Renderer);
	}
}

//==========================================================================
//
// ADLMIDIDevice :: LoadCustomBank
//
// Loads a custom WOPL bank for libADLMIDI. Returns 1 when bank has been
// loaded, otherwise, returns 0 when custom banks are disabled or failed
//
//==========================================================================

int ADLMIDIDevice::LoadCustomBank(const ADLConfig *config)
{
	if (config)
	{
		custom_bank = config->adl_custom_bank;
		use_custom_bank = config->adl_use_custom_bank;
	}

	const char *bankfile = custom_bank.c_str();
	if(!use_custom_bank)
		return 0;
	if (use_genmidi && genmidi.size()) // Try to set GENMIDI as a bank, otherwise, try regular custom bank
	{
		if(adl_openBankData(Renderer, genmidi.data(), (unsigned long)genmidi.size()) == 0)
			return true;
	}
	if(!*bankfile)
		return 0;
	return (adl_openBankFile(Renderer, bankfile) == 0);
}

//==========================================================================
//
// gen2ins
//
// Routin to convert a single GENMIDI instrument into WOPL format
//
//==========================================================================

static void gen2ins(const GenMidiInstrument *genmidi, WOPLInstrument *inst, bool isDrum)
{
	uint8_t notenum = genmidi->fixed_note;
	int16_t noteOffset[2];

	inst->inst_flags = 0;

	if (genmidi->flags & 0x04)
	{
		inst->inst_flags |= WOPL_Ins_4op | WOPL_Ins_Pseudo4op;
	}

	if (isDrum)
	{
		noteOffset[0] = 12;
		noteOffset[1] = 12;
	}
	else
	{
		noteOffset[0] = genmidi->voices[0].base_note_offset + 12;
		noteOffset[1] = genmidi->voices[1].base_note_offset + 12;
	}

	if (isDrum)
	{
		notenum = (genmidi->flags & 0x01) ? genmidi->fixed_note : 60;
	}

	while(notenum && notenum < 20)
	{
		notenum += 12;
		noteOffset[0] -= 12;
		noteOffset[1] -= 12;
	}

	inst->note_offset1 = noteOffset[0];
	inst->note_offset2 = noteOffset[1];

	inst->percussion_key_number = notenum;
	inst->second_voice_detune = static_cast<char>(static_cast<int>(genmidi->fine_tuning) - 128);

	inst->operators[WOPL_OP_MODULATOR1].avekf_20 = genmidi->voices[0].modulator.tremolo;
	inst->operators[WOPL_OP_MODULATOR1].atdec_60 = genmidi->voices[0].modulator.attack;
	inst->operators[WOPL_OP_MODULATOR1].susrel_80 = genmidi->voices[0].modulator.sustain;
	inst->operators[WOPL_OP_MODULATOR1].waveform_E0 = genmidi->voices[0].modulator.waveform;
	inst->operators[WOPL_OP_MODULATOR1].ksl_l_40 = genmidi->voices[0].modulator.scale | genmidi->voices[0].modulator.level;

	inst->operators[WOPL_OP_CARRIER1].avekf_20 = genmidi->voices[0].carrier.tremolo;
	inst->operators[WOPL_OP_CARRIER1].atdec_60 = genmidi->voices[0].carrier.attack;
	inst->operators[WOPL_OP_CARRIER1].susrel_80 = genmidi->voices[0].carrier.sustain;
	inst->operators[WOPL_OP_CARRIER1].waveform_E0 = genmidi->voices[0].carrier.waveform;
	inst->operators[WOPL_OP_CARRIER1].ksl_l_40 = genmidi->voices[0].carrier.scale | genmidi->voices[0].carrier.level;

	inst->fb_conn1_C0 = genmidi->voices[0].feedback;

	inst->operators[WOPL_OP_MODULATOR2].avekf_20 = genmidi->voices[1].modulator.tremolo;
	inst->operators[WOPL_OP_MODULATOR2].atdec_60 = genmidi->voices[1].modulator.attack;
	inst->operators[WOPL_OP_MODULATOR2].susrel_80 = genmidi->voices[1].modulator.sustain;
	inst->operators[WOPL_OP_MODULATOR2].waveform_E0 = genmidi->voices[1].modulator.waveform;
	inst->operators[WOPL_OP_MODULATOR2].ksl_l_40 = genmidi->voices[1].modulator.scale | genmidi->voices[1].modulator.level;

	inst->operators[WOPL_OP_CARRIER2].avekf_20 = genmidi->voices[1].carrier.tremolo;
	inst->operators[WOPL_OP_CARRIER2].atdec_60 = genmidi->voices[1].carrier.attack;
	inst->operators[WOPL_OP_CARRIER2].susrel_80 = genmidi->voices[1].carrier.sustain;
	inst->operators[WOPL_OP_CARRIER2].waveform_E0 = genmidi->voices[1].carrier.waveform;
	inst->operators[WOPL_OP_CARRIER2].ksl_l_40 = genmidi->voices[1].carrier.scale | genmidi->voices[1].carrier.level;

	// FIXME: Supposed to be computed from instrument data, set just constant for a test
	inst->delay_on_ms = 5000;
	inst->delay_off_ms = 5000;

	inst->fb_conn2_C0 = genmidi->voices[1].feedback;
}

//==========================================================================
//
// ADLMIDIDevice :: OP2_To_WOPL
//
// Converts the WAD's GENMIDI bank into compatible WOPL format which can be
// loaded
//
//==========================================================================

void ADLMIDIDevice::OP2_To_WOPL(const ADLConfig *config)
{
	size_t bank_size;
	genmidi.clear();

	if (!config->adl_genmidi_set)
	{
		return; // No GENMIDI bank presented
	}

	const GenMidiInstrument *ginst = reinterpret_cast<const GenMidiInstrument *>(config->adl_genmidi_bank);
	WOPLFile *wopl_bank = WOPL_Init(1, 1);

	wopl_bank->volume_model = (ADLMIDI_VolumeModel_DMX - 1);

	for(size_t i = 0; i < 128; ++i)
	{
		wopl_bank->banks_melodic[0].ins[i].inst_flags = WOPL_Ins_IsBlank;
		wopl_bank->banks_percussive[0].ins[i].inst_flags = WOPL_Ins_IsBlank;
	}

	for (size_t i = 0; i < GENMIDI_NUM_INSTRS; ++i, ++ginst)
	{
		gen2ins(ginst, &wopl_bank->banks_melodic->ins[i], false);
	}

	for (size_t i = GENMIDI_FIST_PERCUSSION; i < GENMIDI_FIST_PERCUSSION + GENMIDI_NUM_PERCUSSION; ++i, ++ginst)
	{
		gen2ins(ginst, &wopl_bank->banks_percussive->ins[i], true);
	}

	bank_size = WOPL_CalculateBankFileSize(wopl_bank, 0);

	genmidi.resize(bank_size);
	int ret = WOPL_SaveBankToMem(wopl_bank, genmidi.data(), genmidi.size(), 0, 0);

	if (ret != 0)
	{
		genmidi.clear();

		const char *err = "Unknown";
		switch (ret)
		{
		case WOPL_ERR_UNEXPECTED_ENDING:
			err = "Unexpected buffer ending";
			break;
		default:
			break;
		}

		ZMusic_Printf(ZMUSIC_MSG_ERROR, "Failed to convert GENMIDI bank to WOPL: %s.\n", err);
	}

	WOPL_Free(wopl_bank);
}


//==========================================================================
//
// ADLMIDIDevice :: Open
//
// Returns 0 on success.
//
//==========================================================================

int ADLMIDIDevice::OpenRenderer()
{
	adl_rt_resetState(Renderer);
	return 0;
}

//==========================================================================
//
// ADLMIDIDevice :: ChangeSettingInt
//
// Changes an integer setting.
//
//==========================================================================

void ADLMIDIDevice::ChangeSettingInt(const char *setting, int value)
{
	if (Renderer == nullptr || strncmp(setting, "libadl.", 7))
	{
		return;
	}
	setting += 7;

	if (strcmp(setting, "volumemodel") == 0)
	{
		adl_setVolumeRangeModel(Renderer, value);
		initGain(); // Gain should be recomputed after changing this
	}
	else if (strcmp(setting, "chanalloc") == 0)
	{
		adl_setChannelAllocMode(Renderer, value);
	}
	else if (strcmp(setting, "emulator") == 0)
	{
		adl_switchEmulator(Renderer, value);
	}
	else if (strcmp(setting, "numchips") == 0)
	{
		adl_setNumChips(Renderer, value);
	}
	else if (strcmp(setting, "fullpan") == 0)
	{
		adl_setSoftPanEnabled(Renderer, value);
	}
	else if (strcmp(setting, "runatpcmrate") == 0)
	{
		adl_setRunAtPcmRate(Renderer, value);
	}
	else if (strcmp(setting, "autoarpeggio") == 0)
	{
		adl_setAutoArpeggio(Renderer, value);
	}
	else if (strcmp(setting, "usecustombank") == 0)
	{
		bool bvalue = (value != 0);
		bool update = (bvalue != use_custom_bank);
		use_custom_bank = bvalue;
		if (update)
		{
			if (!LoadCustomBank(nullptr))
			{
				adl_setBank(Renderer, last_bank);
				initGain();
			}
		}
	}
	else if (strcmp(setting, "usegenmidi") == 0)
	{
		bool bvalue = (value != 0);
		bool update = (bvalue != use_genmidi);
		use_genmidi = bvalue;
		if (update && !genmidi.empty())
		{
			if (!LoadCustomBank(nullptr))
			{
				adl_setBank(Renderer, last_bank);
				initGain();
			}
		}
	}
	else if (strcmp(setting, "banknum") == 0)
	{
		bool update = (value != last_bank);
		last_bank = value;
		if (update)
		{
			adl_setBank(Renderer, last_bank);
			initGain();
		}
	}
}

//==========================================================================
//
// ADLMIDIDevice :: ChangeSettingNum
//
// Changes a numeric setting.
//
//==========================================================================

void ADLMIDIDevice::ChangeSettingNum(const char *setting, double value)
{
	if (Renderer == nullptr || strncmp(setting, "libadl.", 7))
	{
		return;
	}
	setting += 7;

	if (strcmp(setting, "gain") == 0)
	{
		ConfigGainFactor = value;
		initGain();
	}
}

//==========================================================================
//
// ADLMIDIDevice :: ChangeSettingString
//
// Changes a string setting.
//
//==========================================================================

void ADLMIDIDevice::ChangeSettingString(const char *setting, const char *value)
{
	if (Renderer == nullptr || strncmp(setting, "libadl.", 7))
	{
		return;
	}
	setting += 7;

	if (strcmp(setting, "custombank") == 0)
	{
		custom_bank = value;
		if (use_custom_bank)
		{
			if (!LoadCustomBank(nullptr))
				adl_setBank(Renderer, last_bank);
			initGain();
		}
	}
}

//==========================================================================
//
// ADLMIDIDevice :: HandleEvent
//
//==========================================================================

void ADLMIDIDevice::HandleEvent(int status, int parm1, int parm2)
{
	int command = status & 0xF0;
	int chan	= status & 0x0F;

	switch (command)
	{
	case ME_NOTEON:
		adl_rt_noteOn(Renderer, chan, parm1, parm2);
		break;

	case ME_NOTEOFF:
		adl_rt_noteOff(Renderer, chan, parm1);
		break;

	case ME_KEYPRESSURE:
		adl_rt_noteAfterTouch(Renderer, chan, parm1, parm2);
		break;

	case ME_CONTROLCHANGE:
		adl_rt_controllerChange(Renderer, chan, parm1, parm2);
		break;

	case ME_PROGRAM:
		adl_rt_patchChange(Renderer, chan, parm1);
		break;

	case ME_CHANNELPRESSURE:
		adl_rt_channelAfterTouch(Renderer, chan, parm1);
		break;

	case ME_PITCHWHEEL:
		adl_rt_pitchBendML(Renderer, chan, parm2, parm1);
		break;
	}
}

//==========================================================================
//
// ADLMIDIDevice :: HandleLongEvent
//
//==========================================================================

void ADLMIDIDevice::HandleLongEvent(const uint8_t *data, int len)
{
	adl_rt_systemExclusive(Renderer, data, len);
}

static const ADLMIDI_AudioFormat audio_output_format =
{
	ADLMIDI_SampleType_F32,
	sizeof(float),
	2 * sizeof(float)
};

//==========================================================================
//
// ADLMIDIDevice :: ComputeOutput
//
//==========================================================================

void ADLMIDIDevice::ComputeOutput(float *buffer, int len)
{
	ADL_UInt8* left = reinterpret_cast<ADL_UInt8*>(buffer);
	ADL_UInt8* right = reinterpret_cast<ADL_UInt8*>(buffer + 1);
	auto result = adl_generateFormat(Renderer, len * 2, left, right, &audio_output_format);
	for(int i=0; i < result; i++)
	{
		buffer[i] *= OutputGainFactor;
	}
}

//==========================================================================
//
// ADLMIDIDevice :: initGain
//
//==========================================================================

void ADLMIDIDevice::initGain()
{
	if (Renderer == NULL)
	{
		return;
	}

	OutputGainFactor = 3.5f;

	// TODO: Please tune the factor for each volume model to avoid too loud or too silent sounding
	switch (adl_getVolumeRangeModel(Renderer))
	{
	// Louder models
	case ADLMIDI_VolumeModel_Generic:
	case ADLMIDI_VolumeModel_9X:
	case ADLMIDI_VolumeModel_9X_GENERIC_FM:
		OutputGainFactor = 2.0f;
		break;
	// Middle volume models
	case ADLMIDI_VolumeModel_HMI:
	case ADLMIDI_VolumeModel_HMI_OLD:
		OutputGainFactor = 2.5f;
		break;
	default:
	// Quite models
	case ADLMIDI_VolumeModel_DMX:
	case ADLMIDI_VolumeModel_DMX_Fixed:
	case ADLMIDI_VolumeModel_APOGEE:
	case ADLMIDI_VolumeModel_APOGEE_Fixed:
	case ADLMIDI_VolumeModel_AIL:
		OutputGainFactor = 3.5f;
		break;
	// Quiter models
	case ADLMIDI_VolumeModel_NativeOPL3:
		OutputGainFactor = 3.8f;
		break;
	}

	OutputGainFactor *= ConfigGainFactor;
}

//==========================================================================
//
//
//
//==========================================================================

extern ADLConfig adlConfig;

MIDIDevice *CreateADLMIDIDevice(const char *Args)
{
	ADLConfig config = adlConfig;

	const char* bank = Args && *Args ? Args : adlConfig.adl_use_custom_bank ? adlConfig.adl_custom_bank.c_str() : nullptr;
	if (bank && *bank)
	{
		if (*bank >= '0' && *bank <= '9')
		{
			// Args specify a bank by index.
			config.adl_bank = (int)strtoll(bank, nullptr, 10);
			config.adl_use_custom_bank = false;
		}
		else
		{
			const char* info;
			if (musicCallbacks.PathForSoundfont)
			{
				info = musicCallbacks.PathForSoundfont(bank, SF_WOPL);
			}
			else
			{
				info = bank;
			}
			if (info == nullptr)
			{
				config.adl_custom_bank = "";
				config.adl_use_custom_bank = false;
			}
			else
			{
				config.adl_custom_bank = info;
				config.adl_use_custom_bank = true;
			}
		}
	}
	return new ADLMIDIDevice(&config);
}

DLL_EXPORT int ZMusic_GetADLBanks(const char* const** pNames)
{
	if (pNames) *pNames = adl_getBankNames();
	return adl_getBanksCount();
}

#else
MIDIDevice* CreateADLMIDIDevice(const char* Args)
{
	throw std::runtime_error("ADL device not supported in this configuration");
}

DLL_EXPORT int ZMusic_GetADLBanks(const char* const** pNames)
{
	// The export needs to be there even if non-functional.
	return 0;
}

#endif

