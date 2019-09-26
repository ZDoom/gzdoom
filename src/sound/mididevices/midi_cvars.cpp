
#include "i_musicinterns.h"
#include "i_soundfont.h"
#include "adlmidi.h"

static void CheckRestart(int devtype)
{
	if (currSong != nullptr && currSong->GetDeviceType() == devtype)
	{
		MIDIDeviceChanged(-1, true);
	}
}

// This is part of the config struct so that the device does not have to depend on the sound font manager and can work without it.
static const char *ADL_FullPath(const char *bankfile)
{
	auto info = sfmanager.FindSoundFont(bankfile, SF_WOPL);
	if(info == nullptr) return nullptr;
	return info->mFilename;
}

ADLConfig adlConfig = { ADL_FullPath };
	

CUSTOM_CVAR(Int, adl_chips_count, 6, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	CheckRestart(MDEV_ADL);
	adlConfig.adl_chips_count = self;
}

CUSTOM_CVAR(Int, adl_emulator_id, 0, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	CheckRestart(MDEV_ADL);
	adlConfig.adl_emulator_id = self;
}

CUSTOM_CVAR(Bool, adl_run_at_pcm_rate, 0, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	CheckRestart(MDEV_ADL);
	adlConfig.adl_run_at_pcm_rate = self;
}

CUSTOM_CVAR(Bool, adl_fullpan, 1, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	CheckRestart(MDEV_ADL);
	adlConfig.adl_fullpan = self;
}

CUSTOM_CVAR(Int, adl_bank, 14, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	CheckRestart(MDEV_ADL);
	adlConfig.adl_bank = self;
}

CUSTOM_CVAR(Bool, adl_use_custom_bank, 0, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	CheckRestart(MDEV_ADL);
	adlConfig.adl_use_custom_bank = self;
}

CUSTOM_CVAR(String, adl_custom_bank, "", CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	CheckRestart(MDEV_ADL);
	adlConfig.adl_custom_bank = self;
}

CUSTOM_CVAR(Int, adl_volume_model, ADLMIDI_VolumeModel_DMX, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	CheckRestart(MDEV_ADL);
	adlConfig.adl_volume_model = self;
}

