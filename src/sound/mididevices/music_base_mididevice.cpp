/*
** music_midistream.cpp
** Implements base class for MIDI and MUS streaming.
**
**---------------------------------------------------------------------------
** Copyright 2008 Randy Heit
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


#include "mididevice.h"


//==========================================================================
//
// MIDIDevice stubs.
//
//==========================================================================

MIDIDevice::~MIDIDevice()
{
}

//==========================================================================
//
// MIDIDevice :: PrecacheInstruments
//
// The MIDIStreamer calls this method between device open and the first
// buffered stream with a list of instruments known to be used by the song.
// If the device can benefit from preloading the instruments, it can do so
// now.
//
// Each entry is packed as follows:
//   Bits 0- 6: Instrument number
//   Bits 7-13: Bank number
//   Bit    14: Select drum set if 1, tone bank if 0
//
//==========================================================================

void MIDIDevice::PrecacheInstruments(const uint16_t *instruments, int count)
{
}

//==========================================================================
//
// MIDIDevice :: PrepareHeader
//
// Wrapper for MCI's midiOutPrepareHeader.
//
//==========================================================================

int MIDIDevice::PrepareHeader(MidiHeader *header)
{
	return 0;
}

//==========================================================================
//
// MIDIDevice :: UnprepareHeader
//
// Wrapper for MCI's midiOutUnprepareHeader.
//
//==========================================================================

int MIDIDevice::UnprepareHeader(MidiHeader *header)
{
	return 0;
}

//==========================================================================
//
// MIDIDevice :: FakeVolume
//
// Since most implementations render as a normal stream, their volume is
// controlled through the GSnd interface, not here.
//
//==========================================================================

bool MIDIDevice::FakeVolume()
{
	return false;
}

//==========================================================================
//
//
//
//==========================================================================

void MIDIDevice::InitPlayback()
{
}

//==========================================================================
//
//
//
//==========================================================================

bool MIDIDevice::Update()
{
	return true;
}

//==========================================================================
//
// MIDIDevice :: ChangeSettingInt
//
//==========================================================================

void MIDIDevice::ChangeSettingInt(const char *setting, int value)
{
}

//==========================================================================
//
// MIDIDevice :: ChangeSettingNum
//
//==========================================================================

void MIDIDevice::ChangeSettingNum(const char *setting, double value)
{
}

//==========================================================================
//
// MIDIDevice :: ChangeSettingString
//
//==========================================================================

void MIDIDevice::ChangeSettingString(const char *setting, const char *value)
{
}

//==========================================================================
//
// MIDIDevice :: GetStats
//
//==========================================================================

std::string MIDIDevice::GetStats()
{
	return "This MIDI device does not have any stats.";
}

//==========================================================================
//
// MIDIDevice :: GetStreamInfo
//
//==========================================================================

SoundStreamInfo MIDIDevice::GetStreamInfo() const
{
	return { 0, 0, 0 };	// i.e. do not use streaming.
}
