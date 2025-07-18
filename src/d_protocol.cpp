/*
** d_protocol.cpp
** Basic network packet creation routines and simple IFF parsing
**
**---------------------------------------------------------------------------
** Copyright 1998-2006 Randy Heit
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

#include "d_protocol.h"
#include "d_net.h"
#include "doomstat.h"
#include "cmdlib.h"
#include "serializer.h"

FSerializer& Serialize(FSerializer& arc, const char* key, usercmd_t& cmd, usercmd_t* def)
{
	// This used packed data with the old serializer but that's totally counterproductive when
	// having a text format that is human-readable. So this compression has been undone here.
	// The few bytes of file size it saves are not worth the obfuscation.

	if (arc.BeginObject(key))
	{
		arc("buttons", cmd.buttons)
			("pitch", cmd.pitch)
			("yaw", cmd.yaw)
			("roll", cmd.roll)
			("forwardmove", cmd.forwardmove)
			("sidemove", cmd.sidemove)
			("upmove", cmd.upmove)
			.EndObject();
	}
	return arc;
}

void UnpackUserCmd(usercmd_t& cmd, const usercmd_t* basis, TArrayView<uint8_t>& stream)
{
	if (basis != nullptr)
	{
		if (basis != &cmd)
			memcpy(&cmd, basis, sizeof(usercmd_t));
	}
	else
	{
		memset(&cmd, 0, sizeof(usercmd_t));
	}

	uint8_t flags = ReadInt8(stream);
	if (flags)
	{
		// We can support up to 29 buttons using 1 to 4 bytes to store them. The most
		// significant bit of each button byte is a flag to indicate whether or not more buttons
		// should be read in excluding the last one which supports all 8 bits.
		if (flags & UCMDF_BUTTONS)
		{
			uint8_t in = ReadInt8(stream);
			uint32_t buttons = (cmd.buttons & ~0x7F) | (in & 0x7F);
			if (in & MoreButtons)
			{
				in = ReadInt8(stream);
				buttons = (buttons & ~(0x7F << 7)) | ((in & 0x7F) << 7);
				if (in & MoreButtons)
				{
					in = ReadInt8(stream);
					buttons = (buttons & ~(0x7F << 14)) | ((in & 0x7F) << 14);
					if (in & MoreButtons)
					{
						in = ReadInt8(stream);
						buttons = (buttons & ~(0xFF << 21)) | (in << 21);
					}
				}
			}
			cmd.buttons = buttons;
		}
		if (flags & UCMDF_PITCH)
			cmd.pitch = ReadInt16(stream);
		if (flags & UCMDF_YAW)
			cmd.yaw = ReadInt16(stream);
		if (flags & UCMDF_FORWARDMOVE)
			cmd.forwardmove = ReadInt16(stream);
		if (flags & UCMDF_SIDEMOVE)
			cmd.sidemove = ReadInt16(stream);
		if (flags & UCMDF_UPMOVE)
			cmd.upmove = ReadInt16(stream);
		if (flags & UCMDF_ROLL)
			cmd.roll = ReadInt16(stream);
	}
}

void PackUserCmd(const usercmd_t& cmd, const usercmd_t* basis, TArrayView<uint8_t>& stream)
{
	uint8_t flags = 0;
	auto flagsPosition = TArrayView(stream.Data(), 1);

	usercmd_t blank;
	if (basis == nullptr)
	{
		memset(&blank, 0, sizeof(blank));
		basis = &blank;
	}

	AdvanceStream(stream, 1); // Make room for the flags.
	uint32_t buttons_changed = cmd.buttons ^ basis->buttons;
	if (buttons_changed != 0)
	{
		uint8_t bytes[4] = {  uint8_t(cmd.buttons       & 0x7F),
							uint8_t((cmd.buttons >> 7)  & 0x7F),
							uint8_t((cmd.buttons >> 14) & 0x7F),
							uint8_t((cmd.buttons >> 21) & 0xFF) };

		flags |= UCMDF_BUTTONS;
		if (buttons_changed & 0xFFFFFF80)
		{
			bytes[0] |= MoreButtons;
			if (buttons_changed & 0xFFFFC000)
			{
				bytes[1] |= MoreButtons;
				if (buttons_changed & 0xFFE00000)
					bytes[2] |= MoreButtons;
			}
		}
		WriteInt8(bytes[0], stream);
		if (bytes[0] & MoreButtons)
		{
			WriteInt8(bytes[1], stream);
			if (bytes[1] & MoreButtons)
			{
				WriteInt8(bytes[2], stream);
				if (bytes[2] & MoreButtons)
					WriteInt8(bytes[3], stream);
			}
		}
	}
	if (cmd.pitch != basis->pitch)
	{
		flags |= UCMDF_PITCH;
		WriteInt16(cmd.pitch, stream);
	}
	if (cmd.yaw != basis->yaw)
	{
		flags |= UCMDF_YAW;
		WriteInt16 (cmd.yaw, stream);
	}
	if (cmd.forwardmove != basis->forwardmove)
	{
		flags |= UCMDF_FORWARDMOVE;
		WriteInt16 (cmd.forwardmove, stream);
	}
	if (cmd.sidemove != basis->sidemove)
	{
		flags |= UCMDF_SIDEMOVE;
		WriteInt16(cmd.sidemove, stream);
	}
	if (cmd.upmove != basis->upmove)
	{
		flags |= UCMDF_UPMOVE;
		WriteInt16(cmd.upmove, stream);
	}
	if (cmd.roll != basis->roll)
	{
		flags |= UCMDF_ROLL;
		WriteInt16(cmd.roll, stream);
	}

	// Write the packing bits
	WriteInt8(flags, flagsPosition);
}

void WriteUserCmdMessage(const usercmd_t& cmd, const usercmd_t* basis, TArrayView<uint8_t>& stream)
{
	if (basis == nullptr)
	{
		if (cmd.buttons
			|| cmd.pitch || cmd.yaw || cmd.roll
			|| cmd.forwardmove || cmd.sidemove || cmd.upmove)
		{
			WriteInt8(DEM_USERCMD, stream);
			PackUserCmd(cmd, basis, stream);
			return;
		}
	}
	else if (cmd.buttons != basis->buttons
			|| cmd.yaw != basis->yaw || cmd.pitch != basis->pitch || cmd.roll != basis->roll
			|| cmd.forwardmove != basis->forwardmove || cmd.sidemove != basis->sidemove || cmd.upmove != basis->upmove)
	{
		WriteInt8(DEM_USERCMD, stream);
		PackUserCmd(cmd, basis, stream);
		return;
	}

	WriteInt8(DEM_EMPTYUSERCMD, stream);
}

// Reads through the user command without actually setting any of its info. Used to get the size
// of the command when getting the length of the stream.
void SkipUserCmdMessage(TArrayView<uint8_t>& stream)
{
	while (true)
	{
		const uint8_t type = ReadInt8(stream);
		if (type == DEM_USERCMD)
		{
			int skip = 1;
			if (stream[0] & UCMDF_PITCH)
				skip += 2;
			if (stream[0] & UCMDF_YAW)
				skip += 2;
			if (stream[0] & UCMDF_FORWARDMOVE)
				skip += 2;
			if (stream[0] & UCMDF_SIDEMOVE)
				skip += 2;
			if (stream[0] & UCMDF_UPMOVE)
				skip += 2;
			if (stream[0] & UCMDF_ROLL)
				skip += 2;
			if (stream[0] & UCMDF_BUTTONS)
			{
				AdvanceStream(stream, 1);
				if (stream[0] & MoreButtons)
				{
					AdvanceStream(stream, 1);
					if (stream[0] & MoreButtons)
					{
						AdvanceStream(stream, 1);
						if (stream[0] & MoreButtons)
							AdvanceStream(stream, 1);
					}
				}
			}
			AdvanceStream(stream, skip);
			break;
		}
		else if (type == DEM_EMPTYUSERCMD)
		{
			break;
		}
		else
		{
			Net_SkipCommand(type, stream);
		}
	}
}

void ReadUserCmdMessage(TArrayView<uint8_t>& stream, int player, int tic)
{
	const int ticMod = tic % BACKUPTICS;

	auto& curTic = ClientStates[player].Tics[ticMod];
	usercmd_t& ticCmd = curTic.Command;

	const uint8_t* start = stream.Data();

	// Skip until we reach the player command. Event data will get read off once the
	// tick is actually executed.
	int type;
	while ((type = ReadInt8(stream)) != DEM_USERCMD && type != DEM_EMPTYUSERCMD)
		Net_SkipCommand(type, stream);

	// Subtract a byte to account for the fact the stream head is now sitting on the
	// user command.
	curTic.Data.SetData(start, int(stream.Data() - start - 1));

	if (type == DEM_USERCMD)
	{
		UnpackUserCmd(ticCmd,
			tic > 0 ? &ClientStates[player].Tics[(tic - 1) % BACKUPTICS].Command : nullptr, stream);
	}
	else
	{
		if (tic > 0)
			memcpy(&ticCmd, &ClientStates[player].Tics[(tic - 1) % BACKUPTICS].Command, sizeof(ticCmd));
		else
			memset(&ticCmd, 0, sizeof(ticCmd));
	}
}

void RunPlayerCommands(int player, int tic)
{
	// We don't have the full command yet, so don't run it.
	if (gametic % TicDup)
		return;

	auto& data = ClientStates[player].Tics[tic % BACKUPTICS].Data;
	auto stream = data.GetTArrayView();
	if (stream.Size())
	{
		while (stream.Size() > 0)
			Net_DoCommand(ReadInt8(stream), stream, player);

		if (!demorecording)
			data.SetData(nullptr, 0);
	}
}

// Demo related functionality

uint8_t* streamPos = nullptr;

// Write the header of an IFF chunk and leave space
// for the length field.
void StartChunk(int id, TArrayView<uint8_t>& stream)
{
	WriteInt32(id, stream);
	streamPos = stream.Data();
	AdvanceStream(stream, 4);
}

// Write the length field for the chunk and insert
// pad byte if the chunk is odd-sized.
void FinishChunk(TArrayView<uint8_t>& stream)
{
	if (streamPos == nullptr)
		return;

	int len = int(stream.Data() - streamPos - 4);
	auto streamPosView = TArrayView<uint8_t>(streamPos, 4);
	WriteInt32(len, streamPosView);
	if (len & 1)
		WriteInt8(0, stream);

	streamPos = nullptr;
}

// Skip past an unknown chunk. *stream should be
// pointing to the chunk's length field.
void SkipChunk(TArrayView<uint8_t>& stream)
{
	int len = ReadInt32(stream);
	AdvanceStream(stream, len + (len & 1));
}
