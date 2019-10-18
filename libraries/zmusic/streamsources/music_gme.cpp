/*
** music_gme.cpp
** General game music player, using Game Music Emu for decoding.
**
**---------------------------------------------------------------------------
** Copyright 2009 Randy Heit
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

// HEADER FILES ------------------------------------------------------------

// Uncomment if you are using the DLL version of GME.
//#define GME_DLL

#include <algorithm>
#include "streamsource.h"
#include <gme/gme.h>
#include <mutex>
#include "../..//libraries/music_common/fileio.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

class GMESong : public StreamSource
{
public:
	GMESong(Music_Emu *emu, int sample_rate);
	~GMESong();
	bool SetSubsong(int subsong) override;
	bool Start() override;
	void ChangeSettingNum(const char *name, double val) override;
	std::string GetStats() override;
	bool GetData(void *buffer, size_t len) override;
	SoundStreamInfo GetFormat() override;

protected:
	Music_Emu *Emu;
	gme_info_t *TrackInfo;
	int SampleRate;
	int CurrTrack;
	bool started = false;

	bool StartTrack(int track, bool getcritsec=true);
	bool GetTrackInfo();
	int CalcSongLength();
};

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// Currently not used.

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

//==========================================================================
//
// GME_CheckFormat
//
//==========================================================================

const char *GME_CheckFormat(uint32_t id)
{
	return gme_identify_header(&id);
}

//==========================================================================
//
// GME_OpenSong
//
//==========================================================================

StreamSource *GME_OpenSong(MusicIO::FileInterface *reader, const char *fmt, int sample_rate)
{
	gme_type_t type;
	gme_err_t err;
	uint8_t *song;
	Music_Emu *emu;
	
	type = gme_identify_extension(fmt);
	if (type == NULL)
	{
		return NULL;
	}
	emu = gme_new_emu(type, sample_rate);
	if (emu == nullptr)
	{
		return nullptr;
	}

    auto fpos = reader->tell();
	auto len = reader->filelength();

	song = new uint8_t[len];
    if (reader->read(song, len) != len)
    {
        delete[] song;
        gme_delete(emu);
        reader->seek(fpos, SEEK_SET);
        return nullptr;
    }

	err = gme_load_data(emu, song, (long)len);
    delete[] song;

	if (err != nullptr)
	{
		gme_delete(emu);
		throw std::runtime_error(err);
	}
	gme_set_stereo_depth(emu, std::min(std::max(miscConfig.gme_stereodepth, 0.f), 1.f));
	gme_set_fade(emu, -1); // Enable infinite loop

#if GME_VERSION >= 0x602
	gme_set_autoload_playback_limit(emu, 0);
#endif // GME_VERSION >= 0x602

	return new GMESong(emu, sample_rate);
}

//==========================================================================
//
// GMESong - Constructor
//
//==========================================================================

GMESong::GMESong(Music_Emu *emu, int sample_rate)
{
	Emu = emu;
	SampleRate = sample_rate;
	CurrTrack = 0;
	TrackInfo = NULL;
}


SoundStreamInfo GMESong::GetFormat()
{
	return { 32*1024, SampleRate, -2 };
}

//==========================================================================
//
// GMESong - Destructor
//
//==========================================================================

GMESong::~GMESong()
{
	if (TrackInfo != NULL)
	{
		gme_free_info(TrackInfo);
	}
	if (Emu != NULL)
	{
		gme_delete(Emu);
	}
}


//==========================================================================
//
// GMESong :: GMEDepthChanged
//
//==========================================================================

void GMESong::ChangeSettingNum(const char *name, double val)
{
	if (Emu != nullptr && !stricmp(name, "gme.stereodepth"))
	{
		gme_set_stereo_depth(Emu, std::min(std::max(0., val), 1.));
	}
}

//==========================================================================
//
// GMESong :: Play
//
//==========================================================================

bool GMESong::Start()
{
	return StartTrack(CurrTrack);
}


//==========================================================================
//
// GMESong :: SetSubsong
//
//==========================================================================

bool GMESong::SetSubsong(int track)
{
	if (CurrTrack == track)
	{
		return true;
	}
	if (!started)
	{
		CurrTrack = track;
		return true;
	}
	return StartTrack(track);
}

//==========================================================================
//
// GMESong :: StartTrack
//
//==========================================================================

bool GMESong::StartTrack(int track, bool getcritsec)
{
	gme_err_t err;

	if (getcritsec)
	{
		err = gme_start_track(Emu, track);
	}
	else
	{
		err = gme_start_track(Emu, track);
	}
	if (err != NULL)
	{
		// This is called in the data reader thread which may not interact with the UI.
		// TBD: How to get the message across? An exception may not be used here!
		// Printf("Could not start track %d: %s\n", track, err);
		return false;
	}
	CurrTrack = track;
	GetTrackInfo();
	if (!m_Looping)
	{
		gme_set_fade(Emu, CalcSongLength());
	}
	return true;
}

//==========================================================================
//
// GMESong :: GetStats
//
//==========================================================================

std::string GMESong::GetStats()
{
	char out[80];

	if (TrackInfo != NULL)
	{
		int time = gme_tell(Emu);
		snprintf(out, 80, 
			"Track: %d  Time: %3d:%02d:%03d  System: %s",
			CurrTrack,
			time/60000,
			(time/1000) % 60,
			time % 1000,
			TrackInfo->system);
	}
	return out;
}

//==========================================================================
//
// GMESong :: GetTrackInfo
//
//==========================================================================

bool GMESong::GetTrackInfo()
{
	gme_err_t err;

	if (TrackInfo != NULL)
	{
		gme_free_info(TrackInfo);
		TrackInfo = NULL;
	}
	err = gme_track_info(Emu, &TrackInfo, CurrTrack);
	if (err != NULL)
	{
		// This is called in the data reader thread which may not interact with the UI.
		// TBD: How to get the message across? An exception may not be used here!
		//Printf("Could not get track %d info: %s\n", CurrTrack, err);
		return false;
	}
	return true;
}

//==========================================================================
//
// GMESong :: CalcSongLength
//
//==========================================================================

int GMESong::CalcSongLength()
{
	if (TrackInfo == NULL)
	{
		return 150000;
	}
	if (TrackInfo->length > 0)
	{
		return TrackInfo->length;
	}
	if (TrackInfo->loop_length > 0)
	{
		return TrackInfo->intro_length + TrackInfo->loop_length * 2;
	}
	return 150000;
}

//==========================================================================
//
// GMESong :: Read													STATIC
//
//==========================================================================

bool GMESong::GetData(void *buffer, size_t len)
{
	gme_err_t err;

	if (gme_track_ended(Emu))
	{
		if (m_Looping)
		{
			StartTrack(CurrTrack, false);
		}
		else
		{
			memset(buffer, 0, len);
			return false;
		}
	}
	err = gme_play(Emu, int(len >> 1), (short *)buffer);
	return (err == NULL);
}
