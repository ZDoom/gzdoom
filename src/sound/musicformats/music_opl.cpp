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

static bool OPL_Active;

EXTERN_CVAR (Int, opl_numchips)
EXTERN_CVAR(Int, opl_core)

int getOPLCore(const char* args)
{
	int current_opl_core = opl_core;
	if (args != NULL && *args >= '0' && *args < '4') current_opl_core = *args - '0';
	return current_opl_core;
}


OPLMUSSong::OPLMUSSong (FileReader &reader, const char *args)
{
	int samples = int(OPL_SAMPLE_RATE / 14);
	const char* error;

	int core = getOPLCore(args);
	auto data = reader.Read();
	Music = new OPLmusicFile (data.Data(), data.Size(), core, opl_numchips, error);
	if (!Music->IsValid())
	{
		Printf(PRINT_BOLD, "%s", error? error : "Invalid OPL format\n");
		delete Music;
		return;
	}

	m_Stream = GSnd->CreateStream (FillStream, samples*4,
		(core == 0 ? SoundStream::Mono : 0) | SoundStream::Float, int(OPL_SAMPLE_RATE), this);
	if (m_Stream == NULL)
	{
		Printf (PRINT_BOLD, "Could not create music stream.\n");
		delete Music;
		return;
	}
	OPL_Active = true;
}

OPLMUSSong::~OPLMUSSong ()
{
	OPL_Active = false;
	Stop ();
	if (Music != NULL)
	{
		delete Music;
	}
}

bool OPLMUSSong::IsValid () const
{
	return m_Stream != NULL;
}

void OPLMUSSong::ChangeSettingInt(const char * name, int val)
{
	if (!strcmp(name, "opl.numchips"))
		Music->ResetChips (val);
}

bool OPLMUSSong::IsPlaying ()
{
	return m_Status == STATE_Playing;
}

void OPLMUSSong::Play (bool looping, int subsong)
{
	m_Status = STATE_Stopped;
	m_Looping = looping;

	Music->SetLooping (looping);
	Music->Restart ();

	if (m_Stream == NULL || m_Stream->Play (true, snd_musicvolume))
	{
		m_Status = STATE_Playing;
	}
}

bool OPLMUSSong::FillStream (SoundStream *stream, void *buff, int len, void *userdata)
{
	OPLMUSSong *song = (OPLMUSSong *)userdata;
	return song->Music->ServiceStream (buff, len);
}

