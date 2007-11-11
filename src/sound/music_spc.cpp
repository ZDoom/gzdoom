#ifdef _WIN32

#include "i_musicinterns.h"
#include "templates.h"
#include "c_cvars.h"
#include "doomdef.h"

struct XID6Tag
{
	BYTE ID;
	BYTE Type;
	WORD Value;
};

EXTERN_CVAR (Int, snd_samplerate)

CVAR (Int, spc_amp, 30, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR (Bool, spc_8bit, false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR (Bool, spc_stereo, true, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR (Bool, spc_lowpass, false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR (Bool, spc_surround, false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR (Bool, spc_oldsamples, false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR (Bool, spc_noecho, false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)

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

SPCSong::SPCSong (FILE *iofile, char * musiccache, int len)
{

	if (!LoadEmu ())
	{
		return;
	}

	FileReader * file;

	if (iofile != NULL) file = new FileReader(iofile, len);
	else file = new MemoryReader(musiccache, len);

	// No sense in using a higher frequency than the final output
	int freq = MIN (*spc_frequency, *snd_samplerate);

	Is8Bit = spc_8bit;
	Stereo = spc_stereo;

	m_Stream = GSnd->CreateStream (FillStream, 16384,
		(Stereo ? 0 : SoundStream::Mono) |
		(Is8Bit ? SoundStream::Bits8 : 0),
		freq, this);
	if (m_Stream == NULL)
	{
		Printf (PRINT_BOLD, "Could not create music stream.\n");
		CloseEmu ();
		delete file;
		return;
	}

	ResetAPU (spc_amp);
	SetAPUOpt (~0, Stereo + 1, Is8Bit ? 8 : 16, freq, spc_quality,
		(spc_lowpass ? 1 : 0) | (spc_oldsamples ? 2 : 0) | (spc_surround ? 4 : 0) | (spc_noecho ? 16 : 0));

	BYTE spcfile[66048];

	file->Read (spcfile, 66048);

	if (LoadSPCFile != NULL)
	{
		LoadSPCFile (spcfile);
	}
	else
	{
		void *apuram;
		BYTE *extraram;
		void *dsp;

		GetAPUData (&apuram, &extraram, NULL, NULL, &dsp, NULL, NULL, NULL);

		memcpy (apuram, spcfile + 0x100, 65536);
		memcpy (dsp, spcfile + 0x10100, 128);
		memcpy (extraram, spcfile + 0x101c0, 64);

		FixAPU (spcfile[37]+spcfile[38]*256, spcfile[39], spcfile[41], spcfile[40], spcfile[42], spcfile[43]);
	}

	// Search for amplification tag in extended ID666 info
	if (len > 66056)
	{
		DWORD id;

		file->Read (&id, 4);
		if (id == MAKE_ID('x','i','d','6'))
		{
			DWORD size;

			(*file) >> size;
			DWORD pos = 66056;

			while (pos < size)
			{
				XID6Tag tag;
				
				file->Read (&tag, 4);
				if (tag.Type == 0)
				{
					// Don't care about these
				}
				else
				{
					if (pos + LittleShort(tag.Value) <= size)
					{
						if (tag.Type == 4 && tag.ID == 0x36)
						{
							DWORD amp;
							(*file) >> amp;
							if (APUVersion < 98)
							{
								amp >>= 12;
							}
							SetDSPAmp (amp);
							break;
						}
					}
					file->Seek (LittleShort(tag.Value), SEEK_CUR);
				}
			}
		}
	}
	delete file;
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
	m_Status = STATE_Stopped;
	m_Looping = true;

	if (m_Stream->Play (true, snd_musicvolume))
	{
		m_Status = STATE_Playing;
	}
}

bool SPCSong::FillStream (SoundStream *stream, void *buff, int len, void *userdata)
{
	SPCSong *song = (SPCSong *)userdata;
	song->EmuAPU (buff, len >> (song->Stereo + !song->Is8Bit), 1);
	if (song->Is8Bit)
	{
		BYTE *bytebuff = (BYTE *)buff;
		for (int i = 0; i < len; ++i)
		{
			bytebuff[i] -= 128;
		}
	}
	return true;
}

bool SPCSong::LoadEmu ()
{
	APUVersion = 0;

	HandleAPU = LoadLibraryA ("snesapu.dll");
	if (HandleAPU == NULL)
	{
		Printf ("Could not load snesapu.dll\n");
		return false;
	}

	SNESAPUInfo = (SNESAPUInfo_TYPE)GetProcAddress (HandleAPU, "SNESAPUInfo");
	if (SNESAPUInfo == NULL)
	{
		Printf ("This snesapu.dll is too old.\n");
	}
	else
	{
		DWORD ver, min, opt;

		SNESAPUInfo (&ver, &min, &opt);

		if ((min & 0xffff00) >= 0x8500 && (min & 0xffff00) < 0x9800)
		{
			APUVersion = 85;
		}
		else if ((min & 0xffff00) == 0x9800)
		{
			APUVersion = 98;
		}
		else if ((min & 0xffff00) == 0x11000)
		{
			APUVersion = 110;
		}
		else
		{
			char letters[4];
			letters[0] = (char)ver; letters[1] = 0;
			letters[2] = (char)min; letters[3] = 0;
			Printf ("This snesapu.dll is too new.\nIt is version %lx.%02lx%s and"
				"is backward compatible with DLL version %lx.%02lx%s.\n"
				"ZDoom is only known to support DLL versions 0.95 - 2.0\n",
				(ver>>16) & 255, (ver>>8) & 255, letters,
				(min>>16) & 255, (min>>8) & 255, letters+2);
		}
		if (APUVersion != 0)
		{
			if (!(GetAPUData = (GetAPUData_TYPE)GetProcAddress (HandleAPU, "GetAPUData")) ||
				!(ResetAPU = (ResetAPU_TYPE)GetProcAddress (HandleAPU, "ResetAPU")) ||
				!(SetDSPAmp = (SetDSPAmp_TYPE)GetProcAddress (HandleAPU, "SetDSPAmp")) ||
				!(FixAPU = (FixAPU_TYPE)GetProcAddress (HandleAPU, "FixAPU")) ||
				!(SetAPUOpt = (SetAPUOpt_TYPE)GetProcAddress (HandleAPU, "SetAPUOpt")) ||
				!(EmuAPU = (EmuAPU_TYPE)GetProcAddress (HandleAPU, "EmuAPU")))
			{
				Printf ("Snesapu.dll is missing some functions.\n");
				APUVersion = 0;
			}
			LoadSPCFile = (LoadSPCFile_TYPE)GetProcAddress (HandleAPU, "LoadSPCFile");
		}
	}
	if (APUVersion == 0)
	{
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
