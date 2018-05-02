//-----------------------------------------------------------------------------
//
// Copyright 2002-2016 Randy Heit
// Copyright 2005-2014 Simon Howard 
// Copyright 2017 Christoph Oelckers
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/
//
//-----------------------------------------------------------------------------
//
// This is mostly a reimplementation of the interface provided by
// MusLib based on Chocolate-Doom's OPL player, although the
// interface has been cleaned up a bit to be more consistent and readable.
//
//

#include <stdlib.h>
#include <string.h>
#include "musicblock.h"

#include "c_cvars.h"

musicBlock::musicBlock ()
{
	memset (this, 0, sizeof(*this));
	for(auto &oplchannel : oplchannels) oplchannel.Panning = 64;	// default to center panning.
	for(auto &voice : voices) voice.index = ~0u;	// mark all free.
}

musicBlock::~musicBlock ()
{
}

//----------------------------------------------------------------------------
//
// 
//
//----------------------------------------------------------------------------

int musicBlock::releaseVoice(uint32_t slot, uint32_t killed)
{
	struct OPLVoice *ch = &voices[slot];
	io->WriteFrequency(slot, ch->note, ch->pitch, 0);
	ch->index = ~0u;
	ch->sustained = false;
	if (!killed) ch->timestamp = ++timeCounter;
	if (killed) io->MuteChannel(slot);
	return slot;
}

//----------------------------------------------------------------------------
//
// 
//
//----------------------------------------------------------------------------

int musicBlock::findFreeVoice()
{
	// We want to prefer the least recently freed voice, as more recently
	// freed voices can still play a tone from their release state.
	// Sustained voices are replaced when there are no free voices.
	uint32_t min_value = ~0u;
	int result = -1;
	for (uint32_t i = 0; i < io->NumChannels; ++i)
	{
		uint32_t voice_value = voices[i].timestamp + (voices[i].sustained ? (1 << 31) : 0);
		if ((voices[i].index == ~0u || voices[i].sustained) && (voice_value < min_value))
		{
			min_value = voice_value;
			result = i;
		}
	}
	if (result >= 0)
	{
		releaseVoice(result, 1);
	}
	return result;
}

//----------------------------------------------------------------------------
//
// When all voices are in use, we must discard an existing voice to
// play a new note.  Find and free an existing voice.  The channel
// passed to the function is the channel for the new note to be
// played.
//
//----------------------------------------------------------------------------

int musicBlock::replaceExistingVoice()
{
	// Check the allocated voices, if we find an instrument that is
	// of a lower priority to the new instrument, discard it.
	// If a voice is being used to play the second voice of an instrument,
	// use that, as second voices are non-essential.
	// Lower numbered MIDI channels implicitly have a higher priority
	// than higher-numbered channels, eg. MIDI channel 1 is never
	// discarded for MIDI channel 2.
	int result = 0;

	for (uint32_t i = 0; i < io->NumChannels; ++i)
	{
		if (voices[i].current_instr_voice == &voices[i].current_instr->voices[1] ||
			voices[i].index >= voices[result].index)
		{
			result = i;
		}
	}

	releaseVoice(result, 1);
	return result;
}

//----------------------------------------------------------------------------
//
// 
//
//----------------------------------------------------------------------------

void musicBlock::voiceKeyOn(uint32_t slot, uint32_t channo, GenMidiInstrument *instrument, uint32_t instrument_voice, uint32_t key, uint32_t volume)
{
	struct OPLVoice *voice = &voices[slot];
	auto &channel = oplchannels[channo];
	GenMidiVoice *gmvoice;

	voice->index = channo;
	voice->key = key;

	// Program the voice with the instrument data:
	voice->current_instr = instrument;
	gmvoice = voice->current_instr_voice = &instrument->voices[instrument_voice];
	io->WriteInstrument(slot,gmvoice, channel.Vibrato);
	io->WritePan(slot, gmvoice, channel.Panning);

	// Set the volume level.
	voice->note_volume = volume;
	io->WriteVolume(slot, gmvoice, channel.Volume, channel.Expression, volume);

	// Write the frequency value to turn the note on.

	// Work out the note to use.  This is normally the same as
	// the key, unless it is a fixed pitch instrument.
	int note;
	if (instrument->flags & GENMIDI_FLAG_FIXED)	note = instrument->fixed_note;
	else if (channo == CHAN_PERCUSSION) note = 60;	
	else note = key;

	// If this is the second voice of a double voice instrument, the
	// frequency index can be adjusted by the fine tuning field.
	voice->fine_tuning = (instrument_voice != 0) ? (voice->current_instr->fine_tuning / 2) - 64 : 0;
	voice->pitch = voice->fine_tuning + channel.Pitch;

	if (!(instrument->flags & GENMIDI_FLAG_FIXED) && channo != CHAN_PERCUSSION)
	{
		note += gmvoice->base_note_offset;
	}

	// Avoid possible overflow due to base note offset:

	while (note < 0)
	{
		note += 12;
	}

	while (note > HIGHEST_NOTE)
	{
		note -= 12;
	}
	voice->note = note;
	io->WriteFrequency(slot, note, voice->pitch, 1);
}

//----------------------------------------------------------------------------
//
// 
//
//----------------------------------------------------------------------------

CVAR(Bool, opl_singlevoice, 0, 0)

void musicBlock::noteOn(uint32_t channel, uint8_t key, int volume)
{
	if (volume <= 0)
	{
		noteOff(channel, key);
		return;
	}
	GenMidiInstrument *instrument;

	// Percussion channel is treated differently.
	if (channel == CHAN_PERCUSSION)
	{
		if (key < GENMIDI_FIST_PERCUSSION || key >= GENMIDI_FIST_PERCUSSION + GENMIDI_NUM_PERCUSSION)
		{
			return;
		}

		instrument = &OPLinstruments[key + (GENMIDI_NUM_INSTRS - GENMIDI_FIST_PERCUSSION)];
	}
	else
	{
		auto inst = oplchannels[channel].Instrument;
		if (inst >= GENMIDI_NUM_TOTAL) return;	// better safe than sorry.
		instrument = &OPLinstruments[inst];
	}

	bool double_voice = ((instrument->flags) & GENMIDI_FLAG_2VOICE) && !opl_singlevoice;

	int i = findFreeVoice();
	if (i < 0) i = replaceExistingVoice();

	if (i >= 0)
	{
		voiceKeyOn(i, channel, instrument, 0, key, volume);
		if (double_voice)
		{
			i = findFreeVoice();
			if (i >= 0)
			{
				voiceKeyOn(i, channel, instrument, 1, key, volume);
			}
		}
	}
}

//----------------------------------------------------------------------------
//
// 
//
//----------------------------------------------------------------------------

void musicBlock::noteOff(uint32_t id, uint8_t note)
{
	uint32_t sustain = oplchannels[id].Sustain;

	for(uint32_t i = 0; i < io->NumChannels; i++)
	{
		if (voices[i].index == id && voices[i].key == note)
		{
			if (sustain >= MIN_SUSTAIN)
			{
				voices[i].sustained = true;
				voices[i].timestamp = ++timeCounter;
			}
			else releaseVoice(i, 0);
		}
	}
}

//----------------------------------------------------------------------------
//
// 
//
//----------------------------------------------------------------------------

void musicBlock::changePitch(uint32_t id, int val1, int val2)
{
	// Convert pitch from 14-bit to 7-bit, then scale it, since the player
	// code only understands sensitivities of 2 semitones.
	int pitch = ((val1 | (val2 << 7)) - 8192) * oplchannels[id].PitchSensitivity / (200 * 128) + 64;
	oplchannels[id].Pitch = pitch;
	for(uint32_t i = 0; i < io->NumChannels; i++)
	{
		auto &ch = voices[i];
		if (ch.index == id)
		{
			ch.pitch = ch.fine_tuning + pitch;
			io->WriteFrequency(i, ch.note, ch.pitch, 1);
		}
	}
}

//----------------------------------------------------------------------------
//
// 
//
//----------------------------------------------------------------------------

void musicBlock::changeModulation(uint32_t id, int value)
{
	bool vibrato = (value >= VIBRATO_THRESHOLD);
	oplchannels[id].Vibrato = vibrato;
	for (uint32_t i = 0; i < io->NumChannels; i++)
	{
		auto &ch = voices[i];
		if (ch.index == id)
		{
			io->WriteTremolo(i, ch.current_instr_voice, vibrato);
		}
	}
}

//----------------------------------------------------------------------------
//
// 
//
//----------------------------------------------------------------------------

void musicBlock::changeSustain(uint32_t id, int value)
{
	oplchannels[id].Sustain = value;
	if (value < MIN_SUSTAIN)
	{
		for (uint32_t i = 0; i < io->NumChannels; i++)
		{
			if (voices[i].index == id && voices[i].sustained)
				releaseVoice(i, 0);
		}
	}
}

//----------------------------------------------------------------------------
//
// Change volume or expression.
// Since both go to the same register, one function can handle both.
//
//----------------------------------------------------------------------------

void musicBlock::changeVolume(uint32_t id, int value, bool expression)
{
	auto &chan = oplchannels[id];
	if (!expression) chan.Volume = value;
	else chan.Expression = value;
	for (uint32_t i = 0; i < io->NumChannels; i++)
	{
		auto &ch = voices[i];
		if (ch.index == id)
		{
			io->WriteVolume(i, ch.current_instr_voice, chan.Volume, chan.Expression, ch.note_volume);
		}
	}
}

//----------------------------------------------------------------------------
//
// 
//
//----------------------------------------------------------------------------

void musicBlock::changePanning(uint32_t id, int value)
{
	oplchannels[id].Panning = value;
	for(uint32_t i = 0; i < io->NumChannels; i++)
	{
		auto &ch = voices[i];
		if (ch.index == id)
		{
			io->WritePan(i, ch.current_instr_voice, value);
		}
	}
}

//----------------------------------------------------------------------------
//
// 
//
//----------------------------------------------------------------------------

void musicBlock::notesOff(uint32_t id, int value)
{
	for (uint32_t i = 0; i < io->NumChannels; ++i)
	{
		if (voices[i].index == id)
		{
			if (oplchannels[id].Sustain >= MIN_SUSTAIN) 
			{
				voices[i].sustained = true;
				voices[i].timestamp = ++timeCounter;
			}
			else releaseVoice(i, 0);
		}
	}
}

//----------------------------------------------------------------------------
//
// release all notes for this channel
//
//----------------------------------------------------------------------------

void musicBlock::allNotesOff(uint32_t id, int value)
{
	for (uint32_t i = 0; i < io->NumChannels; ++i)
	{
		if (voices[i].index == id)
		{
			releaseVoice(i, 0);
		}
	}
}

//----------------------------------------------------------------------------
//
// 
//
//----------------------------------------------------------------------------

void musicBlock::changeExtended(uint32_t id, uint8_t controller, int value)
{
	switch (controller)
	{
	case ctrlRPNHi:
		oplchannels[id].RPN = (oplchannels[id].RPN & 0x007F) | (value << 7);
		break;

	case ctrlRPNLo:
		oplchannels[id].RPN = (oplchannels[id].RPN & 0x3F80) | value;
		break;

	case ctrlNRPNLo:
	case ctrlNRPNHi:
		oplchannels[id].RPN = 0x3FFF;
		break;

	case ctrlDataEntryHi:
		if (oplchannels[id].RPN == 0)
		{
			oplchannels[id].PitchSensitivity = value * 100 + (oplchannels[id].PitchSensitivity % 100);
		}
		break;

	case ctrlDataEntryLo:
		if (oplchannels[id].RPN == 0)
		{
			oplchannels[id].PitchSensitivity = value + (oplchannels[id].PitchSensitivity / 100) * 100;
		}
		break;
	}
}

//----------------------------------------------------------------------------
//
// 
//
//----------------------------------------------------------------------------

void musicBlock::resetControllers(uint32_t chan, int vol)
{
	auto &channel = oplchannels[chan];

	channel.Volume = vol;
	channel.Expression = 127;
	channel.Sustain = 0;
	channel.Pitch = 64;
	channel.RPN = 0x3fff;
	channel.PitchSensitivity = 200;
}

//----------------------------------------------------------------------------
//
// 
//
//----------------------------------------------------------------------------

void musicBlock::programChange(uint32_t channel, int value)
{
	oplchannels[channel].Instrument = value;
}

//----------------------------------------------------------------------------
//
// 
//
//----------------------------------------------------------------------------

void musicBlock::resetAllControllers(int vol)
{
	uint32_t i;

	for (i = 0; i < NUM_CHANNELS; i++)
	{
		resetControllers(i, vol);
	}
}

//----------------------------------------------------------------------------
//
// 
//
//----------------------------------------------------------------------------

void musicBlock::stopAllVoices()
{
	for (uint32_t i = 0; i < io->NumChannels; i++)
	{
		if (voices[i].index != ~0u) releaseVoice(i, 1);
		voices[i].timestamp = 0;
	}
	timeCounter = 0;
}
