#include "i_musicinterns.h"
#include "templates.h"
#include "c_cvars.h"

#ifndef NOSPC

EXTERN_CVAR (Int, snd_samplerate)

CVAR (Int, spc_amp, 30, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR (Bool, spc_8bit, false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR (Bool, spc_stereo, true, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR (Bool, spc_lowpass, false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR (Bool, spc_surround, false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR (Bool, spc_oldsamples, false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)

CUSTOM_CVAR (Int, spc_quality, 1, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	if (spc_quality < 0)
	{
		spc_quality = 0;
	}
	else if (spc_quality > 3)
	{
		spc_quality = 3;
	}
}

CUSTOM_CVAR (Int, spc_frequency, 32000, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	if (spc_frequency < 8000)
	{
		spc_frequency = 8000;
	}
	else if (spc_frequency > 65535)
	{
		spc_frequency = 65535;
	}
}

SPCSong::SPCSong (const void *mem, int len)
{
	if (!LoadEmu ())
	{
		return;
	}

	// No sense in using a higher frequency than the final output
	int freq = MIN (*spc_frequency, *snd_samplerate);

	m_Stream = FSOUND_Stream_Create (FillStream, 16384,
		FSOUND_SIGNED | FSOUND_2D |
		(spc_stereo ? FSOUND_STEREO : FSOUND_MONO) |
		(spc_8bit ? FSOUND_8BITS : FSOUND_16BITS),
		freq, (int)this);
	if (m_Stream == NULL)
	{
		Printf (PRINT_BOLD, "Could not create FMOD music stream.\n");
		CloseEmu ();
		return;
	}

	ResetAPU (spc_amp);
	SetAPUOpt (0, spc_stereo + 1, spc_8bit ? 8 : 16, freq, spc_quality,
		(spc_lowpass ? 1 : 0) | (spc_oldsamples ? 2 : 0) | (spc_surround ? 4 : 0));

	BYTE spcfile[66048];

	memcpy (spcfile, mem, 66048);

	void *apuram;
	BYTE *extraram;
	void *dsp;

	GetAPUData (&apuram, &extraram, NULL, NULL, &dsp, NULL, NULL, NULL);

	memcpy (apuram, spcfile + 0x100, 65536);
	memcpy (dsp, spcfile + 0x10100, 128);
	memcpy (extraram, spcfile + 0x101c0, 64);

	FixAPU (spcfile[37]+spcfile[38]*256, spcfile[39], spcfile[41], spcfile[40], spcfile[42], spcfile[43]);
}

SPCSong::~SPCSong ()
{
	Stop ();
	CloseEmu ();
}

bool SPCSong::IsValid () const
{
	return HandleAPU != NULL;
}

bool SPCSong::IsPlaying ()
{
	return m_Status == STATE_Playing;
}

void SPCSong::Play (bool looping)
{
	int volume = (int)(snd_musicvolume * 255);

	m_Status = STATE_Stopped;
	m_Looping = true;

	m_Channel = FSOUND_Stream_PlayEx (FSOUND_FREE, m_Stream, NULL, true);
	if (m_Channel != -1)
	{
		FSOUND_SetVolumeAbsolute (m_Channel, volume);
		FSOUND_SetPan (m_Channel, FSOUND_STEREOPAN);
		FSOUND_SetPaused (m_Channel, false);
		m_Status = STATE_Playing;
	}
}

signed char STACK_ARGS SPCSong::FillStream (FSOUND_STREAM *stream, void *buff, int len, int param)
{
	SPCSong *song = (SPCSong *)param;
	int div = 1 << (spc_stereo + !spc_8bit);
	song->EmuAPU (buff, len/div, 1);
	return TRUE;
}

bool SPCSong::LoadEmu ()
{
	HandleAPU = LoadLibraryA ("snesapu.dll");
	if (HandleAPU == NULL)
	{
		Printf ("Could not load snesapu.dll\n");
		return false;
	}

	DWORD ver, min, opt;

	if (!(SNESAPUInfo = (SNESAPUInfo_TYPE)GetProcAddress (HandleAPU, "SNESAPUInfo")) ||
		!(GetAPUData = (GetAPUData_TYPE)GetProcAddress (HandleAPU, "GetAPUData")) ||
		!(ResetAPU = (ResetAPU_TYPE)GetProcAddress (HandleAPU, "ResetAPU")) ||
		!(FixAPU = (FixAPU_TYPE)GetProcAddress (HandleAPU, "FixAPU")) ||
		!(SetAPUOpt = (SetAPUOpt_TYPE)GetProcAddress (HandleAPU, "SetAPUOpt")) ||
		!(EmuAPU = (EmuAPU_TYPE)GetProcAddress (HandleAPU, "EmuAPU")) ||
		(SNESAPUInfo (ver, min, opt), ((min>>8)&0xffff) > 0x095))
	{
		Printf ("snesapu.dll is wrong version\n");
		FreeLibrary (HandleAPU);
		HandleAPU = NULL;
		return false;
	}
	return true;
}

void SPCSong::CloseEmu ()
{
	if (HandleAPU != NULL)
	{
		FreeLibrary (HandleAPU);
		HandleAPU = NULL;
	}
}
#endif
