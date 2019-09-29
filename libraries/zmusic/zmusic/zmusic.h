#pragma once

#include "mididefs.h"

// These exports is needed by the MIDI dumpers which need to remain on the client side.
class MIDISource;	// abstract for the client
EMIDIType IdentifyMIDIType(uint32_t *id, int size);
MIDISource *CreateMIDISource(const uint8_t *data, size_t length, EMIDIType miditype);
