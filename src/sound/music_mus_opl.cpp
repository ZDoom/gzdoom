#include "i_musicinterns.h"

CUSTOM_CVAR (Int, opl_frequency, 49716, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{ // Clamp frequency to FMOD's limits
	if (self < 4000)
		self = 4000;
	else if (self > 49716)	// No need to go higher than this
		self = 49716;
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


OPLMUSSong::OPLMUSSong (FILE *file, char * musiccache, int len)
{
	int rate = *opl_frequency;
	int samples = rate/14;

	Music = new OPLmusicBlock (file, musiccache, len, rate, samples);

	m_Stream = GSnd->CreateStream (FillStream, samples*2,
		SoundStream::Mono, rate, this);
	if (m_Stream == NULL)
	{
		Printf (PRINT_BOLD, "Could not create music stream.\n");
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
	m_Status = STATE_Stopped;
	m_Looping = looping;

	Music->SetLooping (looping);
	Music->Restart ();

	if (m_Stream->Play (true, snd_musicvolume))
	{
		m_Status = STATE_Playing;
	}
}

bool OPLMUSSong::FillStream (SoundStream *stream, void *buff, int len, void *userdata)
{
	OPLMUSSong *song = (OPLMUSSong *)userdata;
	return song->Music->ServiceStream (buff, len);
}
