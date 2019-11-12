/*
** music_cd.cpp
**
**---------------------------------------------------------------------------
** Copyright 1999-2003 Randy Heit
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
**
*/

#include "zmusic/zmusic.h"
#include "zmusic/musinfo.h"

#ifdef _WIN32

#include "zmusic/m_swap.h"
#include "win32/i_cd.h"


// CD track/disk played through the multimedia system -----------------------

class CDSong : public MusInfo
{
public:
	CDSong(int track, int id);
	~CDSong();
	void Play(bool looping, int subsong);
	void Pause();
	void Resume();
	void Stop();
	bool IsPlaying();
	bool IsValid() const { return m_Inited; }

protected:
	CDSong() : m_Inited(false) {}

	int m_Track;
	bool m_Inited;
};

// CD track on a specific disk played through the multimedia system ---------

class CDDAFile : public CDSong
{
public:
	CDDAFile(MusicIO::FileInterface* reader);
};



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
		success = CD_Init (-1);
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

CDDAFile::CDDAFile (MusicIO::FileInterface* reader)
	: CDSong ()
{
	uint32_t chunk;
	uint16_t track;
	uint32_t discid;
	auto endpos = reader->tell() + reader->filelength() - 8;

	// ZMusic_OpenSong already identified this as a CDDA file, so we
	// just need to check the contents we're interested in.
	reader->seek(12, SEEK_CUR);

	while (reader->tell() < endpos)
	{
        reader->read(&chunk, 4);
		if (chunk != (('f')|(('m')<<8)|(('t')<<16)|((' ')<<24)))
		{
            reader->read(&chunk, 4);
            reader->seek(LittleLong(chunk), SEEK_CUR);
		}
		else
		{
            reader->seek(6, SEEK_CUR);
            reader->read(&track, 2);
            reader->read(&discid, 4);

			if (CD_InitID (LittleLong(discid)) && CD_CheckTrack (LittleShort(track)))
			{
				m_Inited = true;
				m_Track = track;
			}
			return;
		}
	}
}

MusInfo* CD_OpenSong(int track, int id)
{
	return new CDSong(track, id);
}

MusInfo* CDDA_OpenSong(MusicIO::FileInterface* reader)
{
	return new CDDAFile(reader);
}

#else

MusInfo* CD_OpenSong(int track, int id)
{
	return nullptr;
}

MusInfo* CDDA_OpenSong(MusicIO::FileInterface* reader)
{
	return nullptr;
}


#endif
