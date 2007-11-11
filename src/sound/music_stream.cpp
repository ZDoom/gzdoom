#include "i_musicinterns.h"

void StreamSong::SetVolume (float volume)
{
	if (m_Stream!=NULL) m_Stream->SetVolume (volume);
}

void StreamSong::Play (bool looping)
{
	m_Status = STATE_Stopped;
	m_Looping = looping;

	if (m_Stream->Play (m_Looping, snd_musicvolume))
	{
		m_Status = STATE_Playing;
		m_LastPos = 0;
	}
}

void StreamSong::Pause ()
{
	if (m_Status == STATE_Playing && m_Stream != NULL)
	{
		if (m_Stream->SetPaused (true))
			m_Status = STATE_Paused;
	}
}

void StreamSong::Resume ()
{
	if (m_Status == STATE_Paused && m_Stream != NULL)
	{
		if (m_Stream->SetPaused (false))
			m_Status = STATE_Playing;
	}
}

void StreamSong::Stop ()
{
	if (m_Status != STATE_Stopped && m_Stream)
	{
		m_Stream->Stop ();
	}
	m_Status = STATE_Stopped;
}

StreamSong::~StreamSong ()
{
	Stop ();
	if (m_Stream != NULL)
	{
		delete m_Stream;
		m_Stream = NULL;
	}
}

StreamSong::StreamSong (const char *filename_or_data, int offset, int len)
{
	m_Stream = GSnd->OpenStream (filename_or_data, SoundStream::Loop, offset, len);
}

bool StreamSong::IsPlaying ()
{
	if (m_Status != STATE_Stopped)
	{
		if (m_Looping)
			return true;

		int pos = m_Stream->GetPosition ();

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

