#include "i_musicinterns.h"

void StreamSong::SetVolume (float volume)
{
	if (m_Channel)
	{
		FSOUND_SetVolumeAbsolute (m_Channel, (int)(volume * 255));
	}
}

void StreamSong::Play (bool looping)
{
	int volume = (int)(snd_musicvolume * 255);

	m_Status = STATE_Stopped;
	m_Looping = looping;

	m_Channel = FSOUND_Stream_PlayEx (FSOUND_FREE, m_Stream, NULL, true);
	if (m_Channel != -1)
	{
		FSOUND_SetVolumeAbsolute (m_Channel, volume);
		FSOUND_SetPan (m_Channel, FSOUND_STEREOPAN);
		FSOUND_SetPaused (m_Channel, false);
		m_Status = STATE_Playing;
		m_LastPos = 0;
	}
}

void StreamSong::Pause ()
{
	if (m_Status == STATE_Playing && m_Stream != NULL)
	{
		if (FSOUND_SetPaused (m_Channel, TRUE))
			m_Status = STATE_Paused;
	}
}

void StreamSong::Resume ()
{
	if (m_Status == STATE_Paused && m_Stream != NULL)
	{
		if (FSOUND_SetPaused (m_Channel, FALSE))
			m_Status = STATE_Playing;
	}
}

void StreamSong::Stop ()
{
	if (m_Status != STATE_Stopped && m_Stream)
	{
		FSOUND_Stream_Stop (m_Stream);
		m_Channel = -1;
	}
	m_Status = STATE_Stopped;
}

StreamSong::~StreamSong ()
{
	Stop ();
	if (m_Stream != NULL)
	{
		FSOUND_Stream_Close (m_Stream);
		m_Stream = NULL;

		// If the stream was not created, then I_RegisterSong()
		// deletes the file object, and we should not do the same.
		if (m_File != NULL)
		{
			delete m_File;
			m_File = NULL;
		}
	}
}

StreamSong::StreamSong (FileReader *file)
: m_Channel (-1), m_File (file)
{
	m_Stream = FSOUND_Stream_Open ((const char *)file,
		FSOUND_LOOP_NORMAL|FSOUND_NORMAL|FSOUND_2D, 0, 0);
}

bool StreamSong::IsPlaying ()
{
	if (m_Channel != -1)
	{
		if (m_Looping)
			return true;

		int pos = FSOUND_Stream_GetPosition (m_Stream);

		if (pos < m_LastPos)
		{
			Stop ();
			return false;
		}

		m_LastPos = pos;
		return true;
	}
	return false;
}

