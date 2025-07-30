/*
** music_opnmidi_mididevice.cpp
** Provides access to libOPNMIDI as a generic MIDI device.
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
#include "mididevice.h"
#include "zmusic/zmusic_internal.h"

#ifdef HAVE_OPN
#include "opnmidi.h"

OpnConfig opnConfig;

class OPNMIDIDevice : public SoftSynthMIDIDevice
{
	struct OPN2_MIDIPlayer *Renderer;
	float OutputGainFactor;
	std::vector<uint8_t> default_bank;
	std::string custom_bank;
	bool use_custom_bank;

public:
	OPNMIDIDevice(const OpnConfig *config);
	~OPNMIDIDevice();

	int OpenRenderer() override;
	int GetDeviceType() const override { return MDEV_OPN; }
	void ChangeSettingInt(const char *setting, int value) override;
	void ChangeSettingNum(const char *setting, double value) override;
	void ChangeSettingString(const char *setting, const char *value) override;

protected:
	void HandleEvent(int status, int parm1, int parm2) override;
	void HandleLongEvent(const uint8_t *data, int len) override;
	void ComputeOutput(float *buffer, int len) override;
	
private:
	int LoadCustomBank(const OpnConfig *config);
	void LoadDefaultBank();
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
// OPNMIDIDevice Constructor
//
//==========================================================================
#include "data/xg.h"

OPNMIDIDevice::OPNMIDIDevice(const OpnConfig *config)
	:SoftSynthMIDIDevice(44100)
{
	Renderer = opn2_init(44100);	// todo: make it configurable
	OutputGainFactor = 4.0f;

	if (Renderer != nullptr)
	{
		default_bank = config->default_bank;

		if (!LoadCustomBank(config))
			LoadDefaultBank();

		OutputGainFactor *= config->opn_gain;

		opn2_switchEmulator(Renderer, (int)config->opn_emulator_id);
		opn2_setRunAtPcmRate(Renderer, (int)config->opn_run_at_pcm_rate);
		opn2_setNumChips(Renderer, config->opn_chips_count);
		opn2_setVolumeRangeModel(Renderer, config->opn_volume_model);
		opn2_setChannelAllocMode(Renderer, config->opn_chan_alloc);
		opn2_setSoftPanEnabled(Renderer, (int)config->opn_fullpan);
		opn2_setAutoArpeggio(Renderer, (int)config->opn_auto_arpeggio);
	}
	else
	{
		throw std::runtime_error("Unable to create OPN renderer.");
	}
}

//==========================================================================
//
// OPNMIDIDevice Destructor
//
//==========================================================================

OPNMIDIDevice::~OPNMIDIDevice()
{
	Close();
	if (Renderer != nullptr)
	{
		opn2_close(Renderer);
	}
}

//==========================================================================
//
// OPNMIDIDevice :: LoadCustomBank
//
// Loads a custom WOPN bank for libOPNMIDI. Returns 1 when bank has been
// loaded, otherwise, returns 0 when custom banks are disabled or failed
//
//==========================================================================


int OPNMIDIDevice::LoadCustomBank(const OpnConfig *config)
{
	if (config)
	{
		custom_bank = config->opn_custom_bank;
		use_custom_bank = config->opn_use_custom_bank;
	}

	const char *bankfile = custom_bank.c_str();
	if(!use_custom_bank)
		return 0;
	if(!*bankfile)
		return 0;
	return (opn2_openBankFile(Renderer, bankfile) == 0);
}

//==========================================================================
//
// OPNMIDIDevice :: LoadDefaultBank
//
// Loads default bank for libOPNMIDI
//
//==========================================================================

void OPNMIDIDevice::LoadDefaultBank()
{
	if (Renderer == nullptr)
	{
		return;
	}

	if(default_bank.size() == 0)
	{
		opn2_openBankData(Renderer, xg_default, sizeof(xg_default));
	}
	else opn2_openBankData(Renderer, default_bank.data(), (long)default_bank.size());
}

//==========================================================================
//
// OPNMIDIDevice :: Open
//
// Returns 0 on success.
//
//==========================================================================

int OPNMIDIDevice::OpenRenderer()
{
	opn2_rt_resetState(Renderer);
	return 0;
}

//==========================================================================
//
// OPNMIDIDevice :: ChangeSettingInt
//
// Changes an integer setting.
//
//==========================================================================

void OPNMIDIDevice::ChangeSettingInt(const char *setting, int value)
{
	if (Renderer == nullptr || strncmp(setting, "libopn.", 7))
	{
		return;
	}
	setting += 7;

	if (strcmp(setting, "volumemodel") == 0)
	{
		opn2_setVolumeRangeModel(Renderer, value);
	}
	else if (strcmp(setting, "chanalloc") == 0)
	{
		opn2_setChannelAllocMode(Renderer, value);
	}
	else if (strcmp(setting, "emulator") == 0)
	{
		opn2_switchEmulator(Renderer, value);
	}
	else if (strcmp(setting, "numchips") == 0)
	{
		opn2_setNumChips(Renderer, value);
	}
	else if (strcmp(setting, "fullpan") == 0)
	{
		opn2_setSoftPanEnabled(Renderer, value);
	}
	else if (strcmp(setting, "runatpcmrate") == 0)
	{
		opn2_setRunAtPcmRate(Renderer, value);
	}
	else if (strcmp(setting, "autoarpeggio") == 0)
	{
		opn2_setAutoArpeggio(Renderer, value);
	}
	else if (strcmp(setting, "usecustombank") == 0)
	{
		bool bvalue = (value != 0);
		bool update = (bvalue != use_custom_bank);
		use_custom_bank = bvalue;
		if (update)
		{
			if (!LoadCustomBank(nullptr))
				LoadDefaultBank();
		}
	}
}

//==========================================================================
//
// OPNMIDIDevice :: ChangeSettingNum
//
// Changes a numeric setting.
//
//==========================================================================

void OPNMIDIDevice::ChangeSettingNum(const char *setting, double value)
{
	if (Renderer == nullptr || strncmp(setting, "libopn.", 7))
	{
		return;
	}
	setting += 7;

	if (strcmp(setting, "gain") == 0)
	{
		OutputGainFactor = 4.0f * value;
	}
}

//==========================================================================
//
// OPNMIDIDevice :: ChangeSettingString
//
// Changes a string setting.
//
//==========================================================================

void OPNMIDIDevice::ChangeSettingString(const char *setting, const char *value)
{
	if (Renderer == nullptr || strncmp(setting, "libopn.", 7))
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
				LoadDefaultBank();
		}
	}
}

//==========================================================================
//
// OPNMIDIDevice :: HandleEvent
//
//==========================================================================

void OPNMIDIDevice::HandleEvent(int status, int parm1, int parm2)
{
	int command = status & 0xF0;
	int chan	= status & 0x0F;

	switch (command)
	{
	case ME_NOTEON:
		opn2_rt_noteOn(Renderer, chan, parm1, parm2);
		break;

	case ME_NOTEOFF:
		opn2_rt_noteOff(Renderer, chan, parm1);
		break;

	case ME_KEYPRESSURE:
		opn2_rt_noteAfterTouch(Renderer, chan, parm1, parm2);
		break;

	case ME_CONTROLCHANGE:
		opn2_rt_controllerChange(Renderer, chan, parm1, parm2);
		break;

	case ME_PROGRAM:
		opn2_rt_patchChange(Renderer, chan, parm1);
		break;

	case ME_CHANNELPRESSURE:
		opn2_rt_channelAfterTouch(Renderer, chan, parm1);
		break;

	case ME_PITCHWHEEL:
		opn2_rt_pitchBendML(Renderer, chan, parm2, parm1);
		break;
	}
}

//==========================================================================
//
// OPNMIDIDevice :: HandleLongEvent
//
//==========================================================================

void OPNMIDIDevice::HandleLongEvent(const uint8_t *data, int len)
{
	opn2_rt_systemExclusive(Renderer, data, len);
}

static const OPNMIDI_AudioFormat audio_output_format =
{
	OPNMIDI_SampleType_F32,
	sizeof(float),
	2 * sizeof(float)
};

//==========================================================================
//
// OPNMIDIDevice :: ComputeOutput
//
//==========================================================================

void OPNMIDIDevice::ComputeOutput(float *buffer, int len)
{
	OPN2_UInt8* left = reinterpret_cast<OPN2_UInt8*>(buffer);
	OPN2_UInt8* right = reinterpret_cast<OPN2_UInt8*>(buffer + 1);
	auto result = opn2_generateFormat(Renderer, len * 2, left, right, &audio_output_format);
	for(int i=0; i < result; i++)
	{
		buffer[i] *= OutputGainFactor;
	}
}

//==========================================================================
//
//
//
//==========================================================================

MIDIDevice *CreateOPNMIDIDevice(const char *Args)
{
	OpnConfig config = opnConfig;

	const char* bank = Args && *Args ? Args : opnConfig.opn_use_custom_bank ? opnConfig.opn_custom_bank.c_str() : nullptr;
	if (bank && *bank)
	{
		const char* info;
		if (musicCallbacks.PathForSoundfont)
		{ 
			info = musicCallbacks.PathForSoundfont(bank, SF_WOPN);
		}
		else
		{
			info = bank;
		}

		if(info == nullptr)
		{
			config.opn_custom_bank = "";
			config.opn_use_custom_bank = false;
		}
		else
		{
			config.opn_custom_bank = info;
			config.opn_use_custom_bank = true;
		}
	}

	return new OPNMIDIDevice(&config);
}

#else
MIDIDevice* CreateOPNMIDIDevice(const char* Args)
{
	throw std::runtime_error("OPN device not supported in this configuration");
}
#endif
