#include "i_musicinterns.h"

void MODSong::SetVolume (float volume)
{
	if (m_Module)
	{
		m_Module->SetVolume (volume);
	}
}

void MODSong::Play (bool looping)
{
	m_Status = STATE_Stopped;
	m_Looping = looping;

	if (m_Module->Play ())
	{
		m_Module->SetVolume (snd_musicvolume);
		m_Status = STATE_Playing;
	}
}

void MODSong::Pause ()
{
	if (m_Status == STATE_Playing)
	{
		if (m_Module->SetPaused (true))
			m_Status = STATE_Paused;
	}
}

void MODSong::Resume ()
{
	if (m_Status == STATE_Paused)
	{
		if (m_Module->SetPaused (false))
			m_Status = STATE_Playing;
	}
}

void MODSong::Stop ()
{
	if (m_Status != STATE_Stopped && m_Module)
	{
		m_Module->Stop ();
	}
	m_Status = STATE_Stopped;
}

MODSong::~MODSong ()
{
	Stop ();
	if (m_Module != NULL)
	{
		delete m_Module;
		m_Module = NULL;
	}
}

MODSong::MODSong (const char *file_or_data, int offset, int length)
{
	m_Module = GSnd->OpenModule (file_or_data, offset, length);
}

bool MODSong::IsPlaying ()
{
	if (m_Status != STATE_Stopped)
	{
		if (m_Module->IsPlaying ())
		{
			if (!m_Looping && m_Module->IsFinished ())
			{
				Stop ();
				return false;
			}
			return true;
		}
		else if (m_Looping)
		{
			Play (true);
			return m_Status != STATE_Stopped;
		}
	}
	return false;
}

bool MODSong::SetPosition (int order)
{
	return m_Module->SetOrder (order);
}

