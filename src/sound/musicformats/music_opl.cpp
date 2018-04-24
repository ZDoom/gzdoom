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
#include "oplsynth/opl.h"

static bool OPL_Active;

CUSTOM_CVAR (Int, opl_numchips, 2, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	if (*self <= 0)
	{
		self = 1;
	}
	else if (*self > MAXOPL2CHIPS)
	{
		self = MAXOPL2CHIPS;
	}
	else if (OPL_Active && currSong != NULL)
	{
		static_cast<OPLMUSSong *>(currSong)->ResetChips ();
	}
}

CUSTOM_CVAR(Int, opl_core, 0, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	if (currSong != nullptr && currSong->GetDeviceType() == MDEV_OPL)
	{
		MIDIDeviceChanged(-1, true);
	}
}
int current_opl_core;

// Get OPL core override from $mididevice
void OPL_SetCore(const char *args)
{
	current_opl_core = opl_core;
	if (args != NULL && *args >= '0' && *args < '4') current_opl_core = *args - '0';
}

OPLMUSSong::OPLMUSSong (FileReader &reader, const char *args)
{
	int samples = int(OPL_SAMPLE_RATE / 14);

	OPL_SetCore(args);
	Music = new OPLmusicFile (reader);

	m_Stream = GSnd->CreateStream (FillStream, samples*4,
		(current_opl_core == 0 ? SoundStream::Mono : 0) | SoundStream::Float, int(OPL_SAMPLE_RATE), this);
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

void OPLMUSSong::ResetChips ()
{
	Music->ResetChips ();
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

MusInfo *OPLMUSSong::GetOPLDumper(const char *filename)
{
	return new OPLMUSDumper(this, filename);
}

OPLMUSSong::OPLMUSSong(const OPLMUSSong *original, const char *filename)
{
	Music = new OPLmusicFile(original->Music, filename);
	m_Stream = NULL;
}

OPLMUSDumper::OPLMUSDumper(const OPLMUSSong *original, const char *filename)
: OPLMUSSong(original, filename)
{
}

void OPLMUSDumper::Play(bool looping, int)
{
	Music->Dump();
}
