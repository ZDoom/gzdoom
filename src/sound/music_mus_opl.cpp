#include "i_musicinterns.h"

CUSTOM_CVAR (Int, opl_frequency, 11025, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{ // Clamp frequency to FMOD's limits
	if (self < 4000)
		self = 4000;
	else if (self > 65535)
		self = 65535;
}

CVAR (Bool, opl_enable, false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)

static bool OPL_Active;

CUSTOM_CVAR (Bool, opl_onechip, false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	if (OPL_Active && currSong != NULL)
	{
		static_cast<OPLMUSSong *>(currSong)->ResetChips ();
	}
}


OPLMUSSong::OPLMUSSong (FILE *file, int len)
{
	int rate = *opl_frequency;
	int samples = rate/14;

	Music = new OPLmusicBlock (file, len, rate, samples);

	m_Stream = FSOUND_Stream_Create (FillStream, samples*2,
		FSOUND_SIGNED | FSOUND_2D | FSOUND_MONO | FSOUND_16BITS,
		rate, (int)this);
	if (m_Stream == NULL)
	{
		Printf (PRINT_BOLD, "Could not create FMOD music stream.\n");
		delete Music;
		return;
	}
	OPL_Active = true;
}

OPLMUSSong::~OPLMUSSong ()
{
	OPL_Active = false;
	Stop ();
	if (Music != NULL)
	{
		delete Music;
	}
}

bool OPLMUSSong::IsValid () const
{
	return m_Stream != NULL;
}

void OPLMUSSong::ResetChips ()
{
	Music->ResetChips ();
}

bool OPLMUSSong::IsPlaying ()
{
	return m_Status == STATE_Playing;
}

void OPLMUSSong::Play (bool looping)
{
	int volume = (int)(snd_musicvolume * 255);

	m_Status = STATE_Stopped;
	m_Looping = looping;

	Music->SetLooping (looping);
	Music->Restart ();

	m_Channel = FSOUND_Stream_PlayEx (FSOUND_FREE, m_Stream, NULL, true);
	if (m_Channel != -1)
	{
		FSOUND_SetVolumeAbsolute (m_Channel, volume);
		FSOUND_SetPan (m_Channel, FSOUND_STEREOPAN);
		FSOUND_SetPaused (m_Channel, false);
		m_Status = STATE_Playing;
	}
}

signed char F_CALLBACKAPI OPLMUSSong::FillStream (FSOUND_STREAM *stream, void *buff, int len, int param)
{
	OPLMUSSong *song = (OPLMUSSong *)param;
	song->Music->ServiceStream (buff, len);
	return TRUE;
}
