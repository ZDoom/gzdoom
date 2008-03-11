#ifdef _WIN32

#include "i_musicinterns.h"
#include "templates.h"
#include "c_cvars.h"
#include "doomdef.h"
#include "SNES_SPC.h"
#include "SPC_Filter.h"

struct XID6Tag
{
	BYTE ID;
	BYTE Type;
	WORD Value;
};

EXTERN_CVAR (Int, snd_samplerate)

CVAR (Float, spc_amp, 1.875f, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)

CUSTOM_CVAR (Int, spc_frequency, 32000, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	if (spc_frequency < 8000)
	{
		spc_frequency = 8000;
	}
	else if (spc_frequency > 32000)
	{
		spc_frequency = 32000;
	}
}

SPCSong::SPCSong (FILE *iofile, char *musiccache, int len)
{
	FileReader *file;

	if (iofile != NULL)
	{
		file = new FileReader(iofile, len);
	}
	else
	{
		file = new MemoryReader(musiccache, len);
	}

	// No sense in using a higher frequency than the final output
	int freq = MIN (*spc_frequency, *snd_samplerate);

	SPC = new SNES_SPC;
	SPC->init();
	Filter = new SPC_Filter;

	BYTE spcfile[66048];

	file->Read (spcfile, 66048);

	SPC->load_spc(spcfile, 66048);
	SPC->clear_echo();
	Filter->set_gain(int(SPC_Filter::gain_unit * spc_amp));

	m_Stream = GSnd->CreateStream (FillStream, 16384, 0, freq, this);
	if (m_Stream == NULL)
	{
		Printf (PRINT_BOLD, "Could not create music stream.\n");
		delete file;
		return;
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
							Filter->set_gain(amp >> 8);
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
	Stop();
	delete Filter;
	delete SPC;
}

bool SPCSong::IsValid () const
{
	return SPC != NULL;
}

bool SPCSong::IsPlaying ()
{
	return m_Status == STATE_Playing;
}

void SPCSong::Play (bool looping)
{
	m_Status = STATE_Stopped;
	m_Looping = true;

	if (m_Stream->Play (true, snd_musicvolume, false))
	{
		m_Status = STATE_Playing;
	}
}

bool SPCSong::FillStream (SoundStream *stream, void *buff, int len, void *userdata)
{
	SPCSong *song = (SPCSong *)userdata;
	song->SPC->play(len >> 1, (short *)buff);
	song->Filter->run((short *)buff, len >> 1);
	return true;
}

#endif
