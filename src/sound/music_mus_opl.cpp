#include "i_musicinterns.h"
#include "oplsynth/muslib.h"
#include "oplsynth/opl.h"

static bool OPL_Active;

CUSTOM_CVAR (Int, opl_numchips, 2, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	if (*self <= 0)
	{
		self = 1;
	}
	else if (*self > MAXOPL2CHIPS)
	{
		self = MAXOPL2CHIPS;
	}
	else if (OPL_Active && currSong != NULL)
	{
		static_cast<OPLMUSSong *>(currSong)->ResetChips ();
	}
}

CVAR(Int, opl_core, 0, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)

OPLMUSSong::OPLMUSSong (FILE *file, BYTE *musiccache, int len)
{
	int samples = int(OPL_SAMPLE_RATE / 14);

	Music = new OPLmusicFile (file, musiccache, len);

	m_Stream = GSnd->CreateStream (FillStream, samples*4,
		(opl_core == 0 ? SoundStream::Mono : 0) | SoundStream::Float, int(OPL_SAMPLE_RATE), this);
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

void OPLMUSSong::Play (bool looping, int subsong)
{
	m_Status = STATE_Stopped;
	m_Looping = looping;

	Music->SetLooping (looping);
	Music->Restart ();

	if (m_Stream == NULL || m_Stream->Play (true, snd_musicvolume))
	{
		m_Status = STATE_Playing;
	}
}

bool OPLMUSSong::FillStream (SoundStream *stream, void *buff, int len, void *userdata)
{
	OPLMUSSong *song = (OPLMUSSong *)userdata;
	return song->Music->ServiceStream (buff, len);
}

MusInfo *OPLMUSSong::GetOPLDumper(const char *filename)
{
	return new OPLMUSDumper(this, filename);
}

OPLMUSSong::OPLMUSSong(const OPLMUSSong *original, const char *filename)
{
	Music = new OPLmusicFile(original->Music, filename);
	m_Stream = NULL;
}

OPLMUSDumper::OPLMUSDumper(const OPLMUSSong *original, const char *filename)
: OPLMUSSong(original, filename)
{
}

void OPLMUSDumper::Play(bool looping, int)
{
	Music->Dump();
}
