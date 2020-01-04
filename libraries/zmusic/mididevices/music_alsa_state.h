/*
** Provides an implementation of an ALSA sequencer wrapper
**
**---------------------------------------------------------------------------
** Copyright 2008-2010 Randy Heit
** Copyright 2020 Petr Mrazek
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

#pragma once

#if defined __linux__ && defined HAVE_SYSTEM_MIDI

#include "zmusic/zmusic.h"
#include <vector>
#include <string>
typedef struct _snd_seq snd_seq_t;

// FIXME: make not visible from outside
struct MidiOutDeviceInternal {
	std::string Name;
	int ID = -1;
	int ClientID = -1;
	int PortNumber = -1;
	unsigned int type = 0;
	EMidiDeviceClass GetDeviceClass() const;
};

// NOTE: the sequencer state is shared between actually playing MIDI music and device enumeration, therefore we keep it around.
class AlsaSequencer {
private:
	AlsaSequencer();
	~AlsaSequencer();

public:
	static AlsaSequencer &Get();
	bool Open();
	void Close();
	bool IsOpen() const {
		return nullptr != handle;
	}

	int EnumerateDevices();
	const std::vector<MidiOutDeviceInternal> &GetInternalDevices();

	snd_seq_t *handle = nullptr;

	int OurId = -1;
	int error = -1;

private:
	std::vector<MidiOutDeviceInternal> internalDevices;
};

#endif
