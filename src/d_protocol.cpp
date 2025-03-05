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

char* ReadString(uint8_t **stream)
{
	char *string = *((char **)stream);

	*stream += strlen(string) + 1;
	return copystring(string);
}

const char* ReadStringConst(uint8_t **stream)
{
	const char *string = *((const char **)stream);
	*stream += strlen(string) + 1;
	return string;
}

uint8_t ReadInt8(uint8_t **stream)
{
	uint8_t v = **stream;
	*stream += 1;
	return v;
}

int16_t ReadInt16(uint8_t **stream)
{
	int16_t v = (((*stream)[0]) << 8) | (((*stream)[1]));
	*stream += 2;
	return v;
}

int32_t ReadInt32(uint8_t **stream)
{
	int32_t v = (((*stream)[0]) << 24) | (((*stream)[1]) << 16) | (((*stream)[2]) << 8) | (((*stream)[3]));
	*stream += 4;
	return v;
}

int64_t ReadInt64(uint8_t** stream)
{
	int64_t v = (int64_t((*stream)[0]) << 56) | (int64_t((*stream)[1]) << 48) | (int64_t((*stream)[2]) << 40) | (int64_t((*stream)[3]) << 32)
				| (int64_t((*stream)[4]) << 24) | (int64_t((*stream)[5]) << 16) | (int64_t((*stream)[6]) << 8) | (int64_t((*stream)[7]));
	*stream += 8;
	return v;
}

float ReadFloat(uint8_t **stream)
{
	union
	{
		int32_t i;
		float f;
	} fakeint;
	fakeint.i = ReadInt32 (stream);
	return fakeint.f;
}

double ReadDouble(uint8_t** stream)
{
	union
	{
		int64_t i;
		double f;
	} fakeint;
	fakeint.i = ReadInt64(stream);
	return fakeint.f;
}

void WriteString(const char *string, uint8_t **stream)
{
	char *p = *((char **)stream);

	while (*string) {
		*p++ = *string++;
	}

	*p++ = 0;
	*stream = (uint8_t *)p;
}

void WriteInt8(uint8_t v, uint8_t **stream)
{
	**stream = v;
	*stream += 1;
}

void WriteInt16(int16_t v, uint8_t **stream)
{
	(*stream)[0] = v >> 8;
	(*stream)[1] = v & 255;
	*stream += 2;
}

void WriteInt32(int32_t v, uint8_t **stream)
{
	(*stream)[0] = v >> 24;
	(*stream)[1] = (v >> 16) & 255;
	(*stream)[2] = (v >> 8) & 255;
	(*stream)[3] = v & 255;
	*stream += 4;
}

void WriteInt64(int64_t v, uint8_t** stream)
{
	(*stream)[0] = v >> 56;
	(*stream)[1] = (v >> 48) & 255;
	(*stream)[2] = (v >> 40) & 255;
	(*stream)[3] = (v >> 32) & 255;
	(*stream)[4] = (v >> 24) & 255;
	(*stream)[5] = (v >> 16) & 255;
	(*stream)[6] = (v >> 8) & 255;
	(*stream)[7] = v & 255;
	*stream += 8;
}

void WriteFloat(float v, uint8_t **stream)
{
	union
	{
		int32_t i;
		float f;
	} fakeint;
	fakeint.f = v;
	WriteInt32 (fakeint.i, stream);
}

void WriteDouble(double v, uint8_t** stream)
{
	union
	{
		int64_t i;
		double f;
	} fakeint;
	fakeint.f = v;
	WriteInt64(fakeint.i, stream);
}

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

// Returns the number of bytes read
int UnpackUserCmd(usercmd_t& cmd, const usercmd_t* basis, uint8_t*& stream)
{
	const uint8_t* start = stream;

	if (basis != nullptr)
	{
		if (basis != &cmd)
			memcpy(&cmd, basis, sizeof(usercmd_t));
	}
	else
	{
		memset(&cmd, 0, sizeof(usercmd_t));
	}

	uint8_t flags = ReadInt8(&stream);
	if (flags)
	{
		// We can support up to 29 buttons using 1 to 4 bytes to store them. The most
		// significant bit of each button byte is a flag to indicate whether or not more buttons
		// should be read in excluding the last one which supports all 8 bits.
		if (flags & UCMDF_BUTTONS)
		{
			uint8_t in = ReadInt8(&stream);
			uint32_t buttons = (cmd.buttons & ~0x7F) | (in & 0x7F);
			if (in & MoreButtons)
			{
				in = ReadInt8(&stream);
				buttons = (buttons & ~(0x7F << 7)) | ((in & 0x7F) << 7);
				if (in & MoreButtons)
				{
					in = ReadInt8(&stream);
					buttons = (buttons & ~(0x7F << 14)) | ((in & 0x7F) << 14);
					if (in & MoreButtons)
					{
						in = ReadInt8(&stream);
						buttons = (buttons & ~(0xFF << 21)) | (in << 21);
					}
				}
			}
			cmd.buttons = buttons;
		}
		if (flags & UCMDF_PITCH)
			cmd.pitch = ReadInt16(&stream);
		if (flags & UCMDF_YAW)
			cmd.yaw = ReadInt16(&stream);
		if (flags & UCMDF_FORWARDMOVE)
			cmd.forwardmove = ReadInt16(&stream);
		if (flags & UCMDF_SIDEMOVE)
			cmd.sidemove = ReadInt16(&stream);
		if (flags & UCMDF_UPMOVE)
			cmd.upmove = ReadInt16(&stream);
		if (flags & UCMDF_ROLL)
			cmd.roll = ReadInt16(&stream);
	}

	return int(stream - start);
}

int PackUserCmd(const usercmd_t& cmd, const usercmd_t* basis, uint8_t*& stream)
{
	uint8_t flags = 0;
	uint8_t* start = stream;

	usercmd_t blank;
	if (basis == nullptr)
	{
		memset(&blank, 0, sizeof(blank));
		basis = &blank;
	}

	++stream; // Make room for the flags.
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
		WriteInt8(bytes[0], &stream);
		if (bytes[0] & MoreButtons)
		{
			WriteInt8(bytes[1], &stream);
			if (bytes[1] & MoreButtons)
			{
				WriteInt8(bytes[2], &stream);
				if (bytes[2] & MoreButtons)
					WriteInt8(bytes[3], &stream);
			}
		}
	}
	if (cmd.pitch != basis->pitch)
	{
		flags |= UCMDF_PITCH;
		WriteInt16(cmd.pitch, &stream);
	}
	if (cmd.yaw != basis->yaw)
	{
		flags |= UCMDF_YAW;
		WriteInt16 (cmd.yaw, &stream);
	}
	if (cmd.forwardmove != basis->forwardmove)
	{
		flags |= UCMDF_FORWARDMOVE;
		WriteInt16 (cmd.forwardmove, &stream);
	}
	if (cmd.sidemove != basis->sidemove)
	{
		flags |= UCMDF_SIDEMOVE;
		WriteInt16(cmd.sidemove, &stream);
	}
	if (cmd.upmove != basis->upmove)
	{
		flags |= UCMDF_UPMOVE;
		WriteInt16(cmd.upmove, &stream);
	}
	if (cmd.roll != basis->roll)
	{
		flags |= UCMDF_ROLL;
		WriteInt16(cmd.roll, &stream);
	}

	// Write the packing bits
	WriteInt8(flags, &start);

	return int(stream - start);
}

int WriteUserCmdMessage(const usercmd_t& cmd, const usercmd_t* basis, uint8_t*& stream)
{
	if (basis == nullptr)
	{
		if (cmd.buttons
			|| cmd.pitch || cmd.yaw || cmd.roll
			|| cmd.forwardmove || cmd.sidemove || cmd.upmove)
		{
			WriteInt8(DEM_USERCMD, &stream);
			return PackUserCmd(cmd, basis, stream) + 1;
		}
	}
	else if (cmd.buttons != basis->buttons
			|| cmd.yaw != basis->yaw || cmd.pitch != basis->pitch || cmd.roll != basis->roll
			|| cmd.forwardmove != basis->forwardmove || cmd.sidemove != basis->sidemove || cmd.upmove != basis->upmove)
	{
		WriteInt8(DEM_USERCMD, &stream);
		return PackUserCmd(cmd, basis, stream) + 1;
	}

	WriteInt8(DEM_EMPTYUSERCMD, &stream);
	return 1;
}

// Reads through the user command without actually setting any of its info. Used to get the size
// of the command when getting the length of the stream.
int SkipUserCmdMessage(uint8_t*& stream)
{
	const uint8_t* start = stream;

	while (true)
	{
		const uint8_t type = *stream++;
		if (type == DEM_USERCMD)
		{
			int skip = 1;
			if (*stream & UCMDF_PITCH)
				skip += 2;
			if (*stream & UCMDF_YAW)
				skip += 2;
			if (*stream & UCMDF_FORWARDMOVE)
				skip += 2;
			if (*stream & UCMDF_SIDEMOVE)
				skip += 2;
			if (*stream & UCMDF_UPMOVE)
				skip += 2;
			if (*stream & UCMDF_ROLL)
				skip += 2;
			if (*stream & UCMDF_BUTTONS)
			{
				if (*++stream & MoreButtons)
				{
					if (*++stream & MoreButtons)
					{
						if (*++stream & MoreButtons)
							++stream;
					}
				}
			}
			stream += skip;
			break;
		}
		else if (type == DEM_EMPTYUSERCMD)
		{
			break;
		}
		else
		{
			Net_SkipCommand(type, &stream);
		}
	}

	return int(stream - start);
}

int ReadUserCmdMessage(uint8_t*& stream, int player, int tic)
{
	const int ticMod = tic % BACKUPTICS;

	auto& curTic = ClientStates[player].Tics[ticMod];
	usercmd_t& ticCmd = curTic.Command;

	const uint8_t* start = stream;

	// Skip until we reach the player command. Event data will get read off once the
	// tick is actually executed.
	int type;
	while ((type = ReadInt8(&stream)) != DEM_USERCMD && type != DEM_EMPTYUSERCMD)
		Net_SkipCommand(type, &stream);

	// Subtract a byte to account for the fact the stream head is now sitting on the
	// user command.
	curTic.Data.SetData(start, int(stream - start - 1));

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

	return int(stream - start);
}

void RunPlayerCommands(int player, int tic)
{
	// We don't have the full command yet, so don't run it.
	if (gametic % TicDup)
		return;

	int len;
	auto& data = ClientStates[player].Tics[tic % BACKUPTICS].Data;
	uint8_t* stream = data.GetData(&len);
	if (stream != nullptr)
	{
		uint8_t* end = stream + len;
		while (stream < end)
			Net_DoCommand(ReadInt8(&stream), &stream, player);

		if (!demorecording)
			data.SetData(nullptr, 0);
	}
}

// Demo related functionality

uint8_t* streamPos = nullptr;

// Write the header of an IFF chunk and leave space
// for the length field.
void StartChunk(int id, uint8_t **stream)
{
	WriteInt32(id, stream);
	streamPos = *stream;
	*stream += 4;
}

// Write the length field for the chunk and insert
// pad byte if the chunk is odd-sized.
void FinishChunk(uint8_t **stream)
{
	if (streamPos == nullptr)
		return;

	int len = int(*stream - streamPos - 4);
	WriteInt32(len, &streamPos);
	if (len & 1)
		WriteInt8(0, stream);

	streamPos = nullptr;
}

// Skip past an unknown chunk. *stream should be
// pointing to the chunk's length field.
void SkipChunk(uint8_t **stream)
{
	int len = ReadInt32(stream);
	*stream += len + (len & 1);
}
