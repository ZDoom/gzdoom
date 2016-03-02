#include "i_musicinterns.h"
#include "i_cd.h"
#include "files.h"

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

CDDAFile::CDDAFile (FileReader &reader)
	: CDSong ()
{
	DWORD chunk;
	WORD track;
	DWORD discid;
	long endpos = reader.Tell() + reader.GetLength() - 8;

	// I_RegisterSong already identified this as a CDDA file, so we
	// just need to check the contents we're interested in.
	reader.Seek(12, SEEK_CUR);

	while (reader.Tell() < endpos)
	{
        reader.Read(&chunk, 4);
		if (chunk != (('f')|(('m')<<8)|(('t')<<16)|((' ')<<24)))
		{
            reader.Read(&chunk, 4);
            reader.Seek(chunk, SEEK_CUR);
		}
		else
		{
            reader.Seek(6, SEEK_CUR);
            reader.Read(&track, 2);
            reader.Read(&discid, 4);

			if (CD_InitID (discid) && CD_CheckTrack (track))
			{
				m_Inited = true;
				m_Track = track;
			}
			return;
		}
	}
}
