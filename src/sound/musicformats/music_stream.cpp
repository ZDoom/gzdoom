/*
** music_stream.cpp
** Wrapper around the sound system's streaming feature for music.
**
**---------------------------------------------------------------------------
** Copyright 2008 Randy Heit
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
*/

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

StreamSong::StreamSong (FileReader &reader)
{
    m_Stream = GSnd->OpenStream (reader, SoundStream::Loop);
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
