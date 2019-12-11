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

#include "zmusic/musinfo.h"
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
	bool IsValid () const override { return m_Source != nullptr; }
	bool SetPosition (unsigned int pos) override;
	bool SetSubsong (int subsong) override;
	std::string GetStats() override;
	void ChangeSettingInt(const char *name, int value) override { if (m_Source) m_Source->ChangeSettingInt(name, value); }
	void ChangeSettingNum(const char *name, double value) override { if (m_Source) m_Source->ChangeSettingNum(name, value); }
	void ChangeSettingString(const char *name, const char *value) override { if(m_Source) m_Source->ChangeSettingString(name, value); }
	bool ServiceStream(void* buff, int len) override;
	SoundStreamInfo GetStreamInfo() const override { return m_Source->GetFormat(); }

	
protected:
	
	StreamSource *m_Source = nullptr;
};



void StreamSong::Play (bool looping, int subsong)
{
	m_Status = STATE_Stopped;
	m_Looping = looping;

	if (m_Source != nullptr)
	{
		m_Source->SetPlayMode(looping);
		m_Source->SetSubsong(subsong);
		if (m_Source->Start())
		{
			m_Status = STATE_Playing;
		}
	}
}

void StreamSong::Pause ()
{
	m_Status = STATE_Paused;
}

void StreamSong::Resume ()
{
	m_Status = STATE_Playing;
}

void StreamSong::Stop ()
{
	m_Status = STATE_Stopped;
}

StreamSong::~StreamSong ()
{
	Stop ();
	if (m_Source != nullptr)
	{
		delete m_Source;
		m_Source = nullptr;
	}
}

StreamSong::StreamSong (StreamSource *source)
{
	m_Source = source;
}

bool StreamSong::IsPlaying ()
{
	if (m_Status != STATE_Stopped)
	{
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
	return m_Source->SetSubsong(subsong);
}

std::string StreamSong::GetStats()
{
	std::string s1, s2;
	if (m_Source != NULL)
	{
		auto stat = m_Source->GetStats();
		s2 = stat.c_str();
	}
	if (s1.empty() && s2.empty()) return "No song loaded\n";
	if (s1.empty()) return s2;
	if (s2.empty()) return s1;
	return s1 + "\n" + s2;
}

bool StreamSong::ServiceStream (void *buff, int len)
{
	bool written = m_Source->GetData(buff, len);
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

