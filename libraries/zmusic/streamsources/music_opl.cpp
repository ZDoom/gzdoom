/*
** music_opl.cpp
** Plays raw OPL formats
**
**---------------------------------------------------------------------------
** Copyright 1998-2008 Randy Heit
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

#include "streamsource.h"
#include "oplsynth/opl.h"
#include "oplsynth/opl_mus_player.h"
#include "../../libraries/music_common/fileio.h"
#include "zmusic/midiconfig.h"


//==========================================================================
//
// OPL file played by a software OPL2 synth and streamed through the sound system
//
//==========================================================================

class OPLMUSSong : public StreamSource
{
public:
	OPLMUSSong (MusicIO::FileInterface *reader, OPLConfig *config);
	~OPLMUSSong ();
	bool Start() override;
	void ChangeSettingInt(const char *name, int value) override;
	SoundStreamInfo GetFormat() override;

protected:
	bool GetData(void *buffer, size_t len) override;

	OPLmusicFile *Music;
	int current_opl_core;
};


//==========================================================================
//
//
//
//==========================================================================

OPLMUSSong::OPLMUSSong(MusicIO::FileInterface* reader, OPLConfig* config)
{
	const char* error = nullptr;
	reader->seek(0, SEEK_END);
	auto fs = reader->tell();
	reader->seek(0, SEEK_SET);
	std::vector<uint8_t> data(fs);
	reader->read(data.data(), (int)data.size());
	Music = new OPLmusicFile(data.data(), data.size(), config->core, config->numchips, error);
	if (error)
	{
		delete Music;
		throw std::runtime_error(error);
	}
}

//==========================================================================
//
//
//
//==========================================================================

SoundStreamInfo OPLMUSSong::GetFormat()
{
	int samples = int(OPL_SAMPLE_RATE / 14);
	return { samples * 4, int(OPL_SAMPLE_RATE), current_opl_core == 0? 1:2  };
}

//==========================================================================
//
//
//
//==========================================================================

OPLMUSSong::~OPLMUSSong ()
{
	if (Music != NULL)
	{
		delete Music;
	}
}

//==========================================================================
//
//
//
//==========================================================================

void OPLMUSSong::ChangeSettingInt(const char * name, int val)
{
	if (!strcmp(name, "opl.numchips"))
		Music->ResetChips (val);
}

//==========================================================================
//
//
//
//==========================================================================

bool OPLMUSSong::Start()
{
	Music->SetLooping (m_Looping);
	Music->Restart ();
	return true;
}

//==========================================================================
//
//
//
//==========================================================================

bool OPLMUSSong::GetData(void *buffer, size_t len)
{
	return Music->ServiceStream(buffer, int(len)) ? len : 0;
}

StreamSource *OPL_OpenSong(MusicIO::FileInterface* reader, OPLConfig *config)
{
	return new OPLMUSSong(reader, config);
}
