#include "i_musicinterns.h"

void StreamSong::Play (bool looping)
{
	m_Status = STATE_Stopped;
	m_Looping = looping;

	if (m_Stream->Play (m_Looping, 1, false))
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
    if (GSnd != NULL)
    {
    	m_Stream = GSnd->OpenStream (filename_or_data, SoundStream::Loop, offset, len);
    }
    else
    {
        m_Stream = NULL;
    }
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

//
// StreamSong :: SetPosition
//
// Sets the current order number for a MOD-type song, or the position in ms
// for anything else.

bool StreamSong::SetPosition(int order)
{
	if (m_Stream != NULL)
	{
		return m_Stream->SetPosition(order);
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
