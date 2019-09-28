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

#include "i_musicinterns.h"
#include "../libraries/oplsynth/oplsynth/opl.h"
#include "../libraries/oplsynth/oplsynth/opl_mus_player.h"

EXTERN_CVAR (Int, opl_numchips)
EXTERN_CVAR(Int, opl_core)


//==========================================================================
//
// OPL file played by a software OPL2 synth and streamed through the sound system
//
//==========================================================================

class OPLMUSSong : public StreamSource
{
public:
	OPLMUSSong (FileReader &reader, const char *args);
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

OPLMUSSong::OPLMUSSong (FileReader &reader, const char *args)
{
	current_opl_core = opl_core;
	if (args != NULL && *args >= '0' && *args < '4') current_opl_core = *args - '0';

	Music = nullptr ;// new OPLmusicFile(reader, current_opl_core);
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

StreamSource *OPL_OpenSong(FileReader &reader, const char *args)
{
	return new OPLMUSSong(reader, args);
}
