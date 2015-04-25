#include "i_musicinterns.h"

void StreamSong::Play (bool looping, int subsong)
{
	m_Status = STATE_Stopped;
	m_Looping = looping;

	if (m_Stream->Play (m_Looping, 1))
	{
		if (subsong != 0)
		{
			m_Stream->SetOrder (subsong);
		}
		m_Status = STATE_Playing;
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

StreamSong::StreamSong (FileReader *reader)
{
    m_Stream = GSnd->OpenStream (reader, SoundStream::Loop);
}

StreamSong::StreamSong (const char *url)
{
    m_Stream = GSnd->OpenStream (url, SoundStream::Loop);
}

bool StreamSong::IsPlaying ()
{
	if (m_Status != STATE_Stopped)
	{
		if (m_Stream->IsEnded())
		{
			Stop();
			return false;
		}
		return true;
	}
	return false;
}

//
// StreamSong :: SetPosition
//
// Sets the position in ms.

bool StreamSong::SetPosition(unsigned int pos)
{
	if (m_Stream != NULL)
	{
		return m_Stream->SetPosition(pos);
	}
	else
	{
		return false;
	}
}

bool StreamSong::SetSubsong(int subsong)
{
	if (m_Stream != NULL)
	{
		return m_Stream->SetOrder(subsong);
	}
	else
	{
		return false;
	}
}

FString StreamSong::GetStats()
{
	if (m_Stream != NULL)
	{
		return m_Stream->GetStats();
	}
	return "No song loaded\n";
}
