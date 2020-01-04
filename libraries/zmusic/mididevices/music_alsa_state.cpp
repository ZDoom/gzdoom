/*
** Provides an implementation of an ALSA sequencer wrapper
**
**---------------------------------------------------------------------------
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
#include "music_alsa_state.h"

#if defined __linux__ && defined HAVE_SYSTEM_MIDI

#include <alsa/asoundlib.h>
#include <sstream>

EMidiDeviceClass MidiOutDeviceInternal::GetDeviceClass() const
{
	if (type & SND_SEQ_PORT_TYPE_SYNTH)
		return MIDIDEV_FMSYNTH;
	if (type & (SND_SEQ_PORT_TYPE_DIRECT_SAMPLE|SND_SEQ_PORT_TYPE_SAMPLE))
		return MIDIDEV_SYNTH;
	if (type & (SND_SEQ_PORT_TYPE_MIDI_GENERIC|SND_SEQ_PORT_TYPE_APPLICATION))
		return MIDIDEV_MIDIPORT;
	// assume FM synth otherwise
	return MIDIDEV_FMSYNTH;
}

AlsaSequencer & AlsaSequencer::Get() {
	static AlsaSequencer sequencer;
	return sequencer;
}

AlsaSequencer::AlsaSequencer() {
	Open();
}

AlsaSequencer::~AlsaSequencer() {
	Close();
}

bool AlsaSequencer::Open() {
	error = snd_seq_open(&handle, "default", SND_SEQ_OPEN_OUTPUT, SND_SEQ_NONBLOCK);
	if(error) {
		return false;
	}
	error = snd_seq_set_client_name(handle, "GZDoom");
	if(error) {
		snd_seq_close(handle);
		handle = nullptr;
		return false;
	}
	OurId = snd_seq_client_id(handle);
	if (OurId < 0) {
		error = OurId;
		OurId = -1;

		snd_seq_close(handle);
		handle = nullptr;
		return false;
	}

	return true;
}


void AlsaSequencer::Close() {
	if(!handle) {
		return;
	}
	snd_seq_close(handle);
	handle = nullptr;
}

namespace {

bool filter(snd_seq_port_info_t *pinfo)
{
	int capability = snd_seq_port_info_get_capability(pinfo);
	if(capability & SND_SEQ_PORT_CAP_NO_EXPORT) {
		return false;
	}
	const int writable = (SND_SEQ_PORT_CAP_WRITE|SND_SEQ_PORT_CAP_SUBS_WRITE);
	if((capability & writable) != writable) {
		return false;
	}
	// TODO: filter based on type here? maybe?
	// int type = snd_seq_port_info_get_type(pinfo);
	return true;
}
}

int AlsaSequencer::EnumerateDevices() {
	if(!handle) {
		return 0;
	}

	snd_seq_client_info_t *cinfo;
	snd_seq_port_info_t *pinfo;

	snd_seq_client_info_alloca(&cinfo);
	snd_seq_port_info_alloca(&pinfo);

	int index = 0;

	// enumerate clients
	snd_seq_client_info_set_client(cinfo, -1);
	while (snd_seq_query_next_client(handle, cinfo) >= 0) {
		snd_seq_port_info_set_client(pinfo, snd_seq_client_info_get_client(cinfo));

		// Ignore 'ALSA oddities' that we don't want to use
		int clientID = snd_seq_client_info_get_client(cinfo);
		if(clientID < 16) {
			continue;
		}

		snd_seq_port_info_set_port(pinfo, -1);
		// enumerate ports
		while (snd_seq_query_next_port(handle, pinfo) >= 0) {
			if (!filter(pinfo)) {
				continue;
			}
			internalDevices.emplace_back();

			auto & itemInternal = internalDevices.back();
			itemInternal.ID = index++;
			const char *name = snd_seq_port_info_get_name(pinfo);
			int portNumber = snd_seq_port_info_get_port(pinfo);
			if(!name) {
				std::ostringstream out;
				out << "MIDI Port " << clientID << ":" << portNumber;
				itemInternal.Name = out.str();
			}
			else {
				itemInternal.Name = name;
			}
			itemInternal.ClientID = clientID;
			itemInternal.PortNumber = portNumber;
			itemInternal.type = snd_seq_port_info_get_type(pinfo);
		}
	}
	return index;
}

const std::vector<MidiOutDeviceInternal> & AlsaSequencer::GetInternalDevices()
{
	return internalDevices;
}

#endif
