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


char *ReadString (uint8_t **stream)
{
	char *string = *((char **)stream);

	*stream += strlen (string) + 1;
	return copystring (string);
}

const char *ReadStringConst(uint8_t **stream)
{
	const char *string = *((const char **)stream);
	*stream += strlen (string) + 1;
	return string;
}

uint8_t ReadInt8 (uint8_t **stream)
{
	uint8_t v = **stream;
	*stream += 1;
	return v;
}

int16_t ReadInt16 (uint8_t **stream)
{
	int16_t v = (((*stream)[0]) << 8) | (((*stream)[1]));
	*stream += 2;
	return v;
}

int32_t ReadInt32 (uint8_t **stream)
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

float ReadFloat (uint8_t **stream)
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

void WriteString (const char *string, uint8_t **stream)
{
	char *p = *((char **)stream);

	while (*string) {
		*p++ = *string++;
	}

	*p++ = 0;
	*stream = (uint8_t *)p;
}


void WriteInt8 (uint8_t v, uint8_t **stream)
{
	**stream = v;
	*stream += 1;
}

void WriteInt16 (int16_t v, uint8_t **stream)
{
	(*stream)[0] = v >> 8;
	(*stream)[1] = v & 255;
	*stream += 2;
}

void WriteInt32 (int32_t v, uint8_t **stream)
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

void WriteFloat (float v, uint8_t **stream)
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

// Returns the number of bytes read
int UnpackUserCmd (usercmd_t *ucmd, const usercmd_t *basis, uint8_t **stream)
{
	uint8_t *start = *stream;
	uint8_t flags;

	if (basis != NULL)
	{
		if (basis != ucmd)
		{
			memcpy (ucmd, basis, sizeof(usercmd_t));
		}
	}
	else
	{
		memset (ucmd, 0, sizeof(usercmd_t));
	}

	flags = ReadInt8 (stream);

	if (flags)
	{
		// We can support up to 29 buttons, using from 0 to 4 bytes to store them.
		if (flags & UCMDF_BUTTONS)
		{
			uint32_t buttons = ucmd->buttons;
			uint8_t in = ReadInt8(stream);

			buttons = (buttons & ~0x7F) | (in & 0x7F);
			if (in & 0x80)
			{
				in = ReadInt8(stream);
				buttons = (buttons & ~(0x7F << 7)) | ((in & 0x7F) << 7);
				if (in & 0x80)
				{
					in = ReadInt8(stream);
					buttons = (buttons & ~(0x7F << 14)) | ((in & 0x7F) << 14);
					if (in & 0x80)
					{
						in = ReadInt8(stream);
						buttons = (buttons & ~(0xFF << 21)) | (in << 21);
					}
				}
			}
			ucmd->buttons = buttons;
		}
		if (flags & UCMDF_PITCH)
			ucmd->pitch = ReadInt16 (stream);
		if (flags & UCMDF_YAW)
			ucmd->yaw = ReadInt16 (stream);
		if (flags & UCMDF_FORWARDMOVE)
			ucmd->forwardmove = ReadInt16 (stream);
		if (flags & UCMDF_SIDEMOVE)
			ucmd->sidemove = ReadInt16 (stream);
		if (flags & UCMDF_UPMOVE)
			ucmd->upmove = ReadInt16 (stream);
		if (flags & UCMDF_ROLL)
			ucmd->roll = ReadInt16 (stream);
	}

	return int(*stream - start);
}

// Returns the number of bytes written
int PackUserCmd (const usercmd_t *ucmd, const usercmd_t *basis, uint8_t **stream)
{
	uint8_t flags = 0;
	uint8_t *temp = *stream;
	uint8_t *start = *stream;
	usercmd_t blank;
	uint32_t buttons_changed;

	if (basis == NULL)
	{
		memset (&blank, 0, sizeof(blank));
		basis = &blank;
	}

	WriteInt8 (0, stream);			// Make room for the packing bits

	buttons_changed = ucmd->buttons ^ basis->buttons;
	if (buttons_changed != 0)
	{
		uint8_t bytes[4] = {  uint8_t(ucmd->buttons        & 0x7F),
						  uint8_t((ucmd->buttons >> 7)  & 0x7F),
						  uint8_t((ucmd->buttons >> 14) & 0x7F),
						  uint8_t((ucmd->buttons >> 21) & 0xFF) };

		flags |= UCMDF_BUTTONS;

		if (buttons_changed & 0xFFFFFF80)
		{
			bytes[0] |= 0x80;
			if (buttons_changed & 0xFFFFC000)
			{
				bytes[1] |= 0x80;
				if (buttons_changed & 0xFFE00000)
				{
					bytes[2] |= 0x80;
				}
			}
		}
		WriteInt8 (bytes[0], stream);
		if (bytes[0] & 0x80)
		{
			WriteInt8 (bytes[1], stream);
			if (bytes[1] & 0x80)
			{
				WriteInt8 (bytes[2], stream);
				if (bytes[2] & 0x80)
				{
					WriteInt8 (bytes[3], stream);
				}
			}
		}
	}
	if (ucmd->pitch != basis->pitch)
	{
		flags |= UCMDF_PITCH;
		WriteInt16 (ucmd->pitch, stream);
	}
	if (ucmd->yaw != basis->yaw)
	{
		flags |= UCMDF_YAW;
		WriteInt16 (ucmd->yaw, stream);
	}
	if (ucmd->forwardmove != basis->forwardmove)
	{
		flags |= UCMDF_FORWARDMOVE;
		WriteInt16 (ucmd->forwardmove, stream);
	}
	if (ucmd->sidemove != basis->sidemove)
	{
		flags |= UCMDF_SIDEMOVE;
		WriteInt16 (ucmd->sidemove, stream);
	}
	if (ucmd->upmove != basis->upmove)
	{
		flags |= UCMDF_UPMOVE;
		WriteInt16 (ucmd->upmove, stream);
	}
	if (ucmd->roll != basis->roll)
	{
		flags |= UCMDF_ROLL;
		WriteInt16 (ucmd->roll, stream);
	}

	// Write the packing bits
	WriteInt8 (flags, &temp);

	return int(*stream - start);
}

FSerializer &Serialize(FSerializer &arc, const char *key, ticcmd_t &cmd, ticcmd_t *def)
{
	if (arc.BeginObject(key))
	{
		arc("consistency", cmd.consistancy)
			("ucmd", cmd.ucmd)
			.EndObject();
	}
	return arc;
}

FSerializer &Serialize(FSerializer &arc, const char *key, usercmd_t &cmd, usercmd_t *def)
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

int WriteUserCmdMessage (usercmd_t *ucmd, const usercmd_t *basis, uint8_t **stream)
{
	if (basis == NULL)
	{
		if (ucmd->buttons != 0 ||
			ucmd->pitch != 0 ||
			ucmd->yaw != 0 ||
			ucmd->forwardmove != 0 ||
			ucmd->sidemove != 0 ||
			ucmd->upmove != 0 ||
			ucmd->roll != 0)
		{
			WriteInt8 (DEM_USERCMD, stream);
			return PackUserCmd (ucmd, basis, stream) + 1;
		}
	}
	else
	if (ucmd->buttons != basis->buttons ||
		ucmd->pitch != basis->pitch ||
		ucmd->yaw != basis->yaw ||
		ucmd->forwardmove != basis->forwardmove ||
		ucmd->sidemove != basis->sidemove ||
		ucmd->upmove != basis->upmove ||
		ucmd->roll != basis->roll)
	{
		WriteInt8 (DEM_USERCMD, stream);
		return PackUserCmd (ucmd, basis, stream) + 1;
	}

	WriteInt8 (DEM_EMPTYUSERCMD, stream);
	return 1;
}


int SkipTicCmd (uint8_t **stream, int count)
{
	int i, skip;
	uint8_t *flow = *stream;

	for (i = count; i > 0; i--)
	{
		bool moreticdata = true;

		flow += 2;		// Skip consistancy marker
		while (moreticdata)
		{
			uint8_t type = *flow++;

			if (type == DEM_USERCMD)
			{
				moreticdata = false;
				skip = 1;
				if (*flow & UCMDF_PITCH)		skip += 2;
				if (*flow & UCMDF_YAW)			skip += 2;
				if (*flow & UCMDF_FORWARDMOVE)	skip += 2;
				if (*flow & UCMDF_SIDEMOVE)		skip += 2;
				if (*flow & UCMDF_UPMOVE)		skip += 2;
				if (*flow & UCMDF_ROLL)			skip += 2;
				if (*flow & UCMDF_BUTTONS)
				{
					if (*++flow & 0x80)
					{
						if (*++flow & 0x80)
						{
							if (*++flow & 0x80)
							{
								++flow;
							}
						}
					}
				}
				flow += skip;
			}
			else if (type == DEM_EMPTYUSERCMD)
			{
				moreticdata = false;
			}
			else
			{
				Net_SkipCommand (type, &flow);
			}
		}
	}

	skip = int(flow - *stream);
	*stream = flow;

	return skip;
}

extern short consistancy[MAXPLAYERS][BACKUPTICS];
void ReadTicCmd (uint8_t **stream, int player, int tic)
{
	int type;
	uint8_t *start;
	ticcmd_t *tcmd;

	int ticmod = tic % BACKUPTICS;

	tcmd = &netcmds[player][ticmod];
	tcmd->consistancy = ReadInt16 (stream);

	start = *stream;

	while ((type = ReadInt8 (stream)) != DEM_USERCMD && type != DEM_EMPTYUSERCMD)
		Net_SkipCommand (type, stream);

	NetSpecs[player][ticmod].SetData (start, int(*stream - start - 1));

	if (type == DEM_USERCMD)
	{
		UnpackUserCmd (&tcmd->ucmd,
			tic ? &netcmds[player][(tic-1)%BACKUPTICS].ucmd : NULL, stream);
	}
	else
	{
		if (tic)
		{
			memcpy (&tcmd->ucmd, &netcmds[player][(tic-1)%BACKUPTICS].ucmd, sizeof(tcmd->ucmd));
		}
		else
		{
			memset (&tcmd->ucmd, 0, sizeof(tcmd->ucmd));
		}
	}

	if (player==consoleplayer&&tic>BACKUPTICS)
		assert(consistancy[player][ticmod] == tcmd->consistancy);
}

void RunNetSpecs (int player, int buf)
{
	uint8_t *stream;
	int len;

	if (gametic % ticdup == 0)
	{
		stream = NetSpecs[player][buf].GetData (&len);
		if (stream)
		{
			uint8_t *end = stream + len;
			while (stream < end)
			{
				int type = ReadInt8 (&stream);
				Net_DoCommand (type, &stream, player);
			}
			if (!demorecording)
				NetSpecs[player][buf].SetData (NULL, 0);
		}
	}
}

uint8_t *lenspot;

// Write the header of an IFF chunk and leave space
// for the length field.
void StartChunk (int id, uint8_t **stream)
{
	WriteInt32 (id, stream);
	lenspot = *stream;
	*stream += 4;
}

// Write the length field for the chunk and insert
// pad byte if the chunk is odd-sized.
void FinishChunk (uint8_t **stream)
{
	int len;
	
	if (!lenspot)
		return;

	len = int(*stream - lenspot - 4);
	WriteInt32 (len, &lenspot);
	if (len & 1)
		WriteInt8 (0, stream);

	lenspot = NULL;
}

// Skip past an unknown chunk. *stream should be
// pointing to the chunk's length field.
void SkipChunk (uint8_t **stream)
{
	int len;

	len = ReadInt32 (stream);
	*stream += len + (len & 1);
}
