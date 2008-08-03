#include "i_musicinterns.h"
#include "i_cd.h"

void CDSong::Play (bool looping, int subsong)
{
	m_Status = STATE_Stopped;
	m_Looping = looping;
	if (m_Track != 0 ? CD_Play (m_Track, looping) : CD_PlayCD (looping))
	{
		m_Status = STATE_Playing;
	}
}

void CDSong::Pause ()
{
	if (m_Status == STATE_Playing)
	{
		CD_Pause ();
		m_Status = STATE_Paused;
	}
}

void CDSong::Resume ()
{
	if (m_Status == STATE_Paused)
	{
		if (CD_Resume ())
			m_Status = STATE_Playing;
	}
}

void CDSong::Stop ()
{
	if (m_Status != STATE_Stopped)
	{
		m_Status = STATE_Stopped;
		CD_Stop ();
	}
}

CDSong::~CDSong ()
{
	Stop ();
	m_Inited = false;
}

CDSong::CDSong (int track, int id)
{
	bool success;

	m_Inited = false;

	if (id != 0)
	{
		success = CD_InitID (id);
	}
	else
	{
		success = CD_Init ();
	}

	if (success && (track == 0 || CD_CheckTrack (track)))
	{
		m_Inited = true;
		m_Track = track;
	}
}

bool CDSong::IsPlaying ()
{
	if (m_Status == STATE_Playing)
	{
		if (CD_GetMode () != CDMode_Play)
		{
			Stop ();
		}
	}
	return m_Status != STATE_Stopped;
}

CDDAFile::CDDAFile (FILE *file, int length)
	: CDSong ()
{
	DWORD chunk;
	WORD track;
	DWORD discid;
	long endpos = ftell (file) + length - 8;

	// I_RegisterSong already identified this as a CDDA file, so we
	// just need to check the contents we're interested in.
	fseek (file, 12, SEEK_CUR);

	while (ftell (file) < endpos)
	{
		fread (&chunk, 4, 1, file);
		if (chunk != (('f')|(('m')<<8)|(('t')<<16)|((' ')<<24)))
		{
			fread (&chunk, 4, 1, file);
			fseek (file, chunk, SEEK_CUR);
		}
		else
		{
			fseek (file, 6, SEEK_CUR);
			fread (&track, 2, 1, file);
			fread (&discid, 4, 1, file);

			if (CD_InitID (discid) && CD_CheckTrack (track))
			{
				m_Inited = true;
				m_Track = track;
			}
			return;
		}
	}
}
