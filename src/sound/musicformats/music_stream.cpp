/*
** music_stream.cpp
** Plays a streaming song from a StreamSource 
**
**---------------------------------------------------------------------------
** Copyright 2008 Randy Heit
** Copyright 2019 Christoph Oelckers
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
#include "streamsources/streamsource.h"

class StreamSong : public MusInfo
{
public:
	StreamSong (StreamSource *source);
	~StreamSong ();
	void Play (bool looping, int subsong) override;
	void Pause () override;
	void Resume () override;
	void Stop () override;
	bool IsPlaying () override;
	bool IsValid () const override { return m_Stream != nullptr && m_Source != nullptr; }
	bool SetPosition (unsigned int pos) override;
	bool SetSubsong (int subsong) override;
	FString GetStats() override;
	void ChangeSettingInt(const char *name, int value) override { if (m_Source) m_Source->ChangeSettingInt(name, value); }
	void ChangeSettingNum(const char *name, double value) override { if (m_Source) m_Source->ChangeSettingNum(name, value); }
	void ChangeSettingString(const char *name, const char *value) override { if(m_Source) m_Source->ChangeSettingString(name, value); }

	
protected:
	
	SoundStream *m_Stream = nullptr;
	StreamSource *m_Source = nullptr;
	
private:
	static bool FillStream (SoundStream *stream, void *buff, int len, void *userdata);
};



void StreamSong::Play (bool looping, int subsong)
{
	m_Status = STATE_Stopped;
	m_Looping = looping;

	if (m_Stream != nullptr && m_Source != nullptr)
	{
		m_Source->SetPlayMode(looping);
		m_Source->SetSubsong(subsong);
		if (m_Source->Start())
		{
			m_Status = STATE_Playing;
			m_Stream->Play(m_Looping, 1);
		}
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
	if (m_Stream != nullptr)
	{
		delete m_Stream;
		m_Stream = nullptr;
	}
	if (m_Source != nullptr)
	{
		delete m_Source;
		m_Source = nullptr;
	}
}

StreamSong::StreamSong (StreamSource *source)
{
	m_Source = source;
	auto fmt = source->GetFormat();
	int flags = fmt.mNumChannels < 0? 0 : SoundStream::Float;
	if (abs(fmt.mNumChannels) < 2) flags |= SoundStream::Mono;

	m_Stream = GSnd->CreateStream(FillStream, fmt.mBufferSize, flags, fmt.mSampleRate, this);
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
	if (m_Source != nullptr)
	{
		return m_Source->SetPosition(pos);
	}
	else
	{
		return false;
	}
}

bool StreamSong::SetSubsong(int subsong)
{
	if (m_Stream != nullptr)
	{
		return m_Source->SetSubsong(subsong);
	}
	else
	{
		return false;
	}
}

FString StreamSong::GetStats()
{
	FString s1, s2;
	if (m_Stream != NULL)
	{
		s1 = m_Stream->GetStats();
	}
	if (m_Source != NULL)
	{
		auto stat = m_Source->GetStats();
		s2 = stat.c_str();
	}
	if (s1.IsEmpty() && s2.IsEmpty()) return "No song loaded\n";
	if (s1.IsEmpty()) return s2;
	if (s2.IsEmpty()) return s1;
	return FStringf("%s\n%s", s1.GetChars(), s2.GetChars());
}

bool StreamSong::FillStream (SoundStream *stream, void *buff, int len, void *userdata)
{
	StreamSong *song = (StreamSong *)userdata;
	
	bool written = song->m_Source->GetData(buff, len);
	if (!written)
	{
		memset((char*)buff, 0, len);
		return false;
	}
	return true;
}

MusInfo *OpenStreamSong(StreamSource *source)
{
	auto song = new StreamSong(source);
	if (song->IsValid()) return song;
	delete song;
	return nullptr;
}

