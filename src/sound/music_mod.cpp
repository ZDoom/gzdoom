#include "i_musicinterns.h"

void MODSong::SetVolume (float volume)
{
	if (m_Module)
	{
		FMUSIC_SetMasterVolume (m_Module, (int)(volume * 256));
	}
}

void MODSong::Play (bool looping)
{
	m_Status = STATE_Stopped;
	m_Looping = looping;

	if (FMUSIC_PlaySong (m_Module))
	{
		FMUSIC_SetMasterVolume (m_Module, (int)(snd_musicvolume * 256));
		m_Status = STATE_Playing;
	}
}

void MODSong::Pause ()
{
	if (m_Status == STATE_Playing)
	{
		if (FMUSIC_SetPaused (m_Module, TRUE))
			m_Status = STATE_Paused;
	}
}

void MODSong::Resume ()
{
	if (m_Status == STATE_Paused)
	{
		if (FMUSIC_SetPaused (m_Module, FALSE))
			m_Status = STATE_Playing;
	}
}

void MODSong::Stop ()
{
	if (m_Status != STATE_Stopped && m_Module)
	{
		FMUSIC_StopSong (m_Module);
	}
	m_Status = STATE_Stopped;
}

MODSong::~MODSong ()
{
	Stop ();
	if (m_Module != NULL)
	{
		FMUSIC_FreeSong (m_Module);
		m_Module = NULL;
	}
}

MODSong::MODSong (const void *mem, int len)
{
	// Is this const_cast safe or do I need to copy the lump to
	// readable memory?
	m_Module = FMUSIC_LoadSongMemory (const_cast<void *>(mem), len);
}

bool MODSong::IsPlaying ()
{
	if (m_Status != STATE_Stopped)
	{
		if (FMUSIC_IsPlaying (m_Module))
		{
			if (!m_Looping && FMUSIC_IsFinished (m_Module))
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
	return !!FMUSIC_SetOrder (m_Module, order);
}

