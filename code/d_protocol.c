#include <malloc.h>
#include <stdlib.h>

#include "d_protocol.h"
#include "doomdef.h"
#include "cmdlib.h"

extern byte *demo_p;

char *ReadString (byte **stream)
{
	char *string = *stream;

	*stream += strlen (string) + 1;
	return copystring (string);
}

int ReadByte (byte **stream)
{
	byte v = **stream;
	*stream += 1;
	return v;
}

int ReadWord (byte **stream)
{
	short v = (((*stream)[0]) << 8) | (((*stream)[1]));
	*stream += 2;
	return v;
}

int ReadLong (byte **stream)
{
	int v = (((*stream)[0]) << 24) | (((*stream)[1]) << 16) | (((*stream)[2]) << 8) | (((*stream)[3]));
	*stream += 4;
	return v;
}

void WriteString (char *string, byte **stream)
{
	char *p = *stream;

	while (*string) {
		*p++ = *string++;
	}

	*p++ = 0;
	*stream = p;
}


void WriteByte (byte v, byte **stream)
{
	**stream = v;
	*stream += 1;
}

void WriteWord (short v, byte **stream)
{
	(*stream)[0] = v >> 8;
	(*stream)[1] = v & 255;
	*stream += 2;
}

void WriteLong (int v, byte **stream)
{
	(*stream)[0] = v >> 24;
	(*stream)[1] = (v >> 16) & 255;
	(*stream)[2] = (v >> 8) & 255;
	(*stream)[3] = v & 255;
	*stream += 4;
}

// Returns the number of bytes read
int UnpackUserCmd (usercmd_t *ucmd, byte **stream)
{
	byte *start = *stream;
	short flags = ReadByte (stream);

	ucmd->buttons = ReadByte (stream);
	if (flags & UCMDF_PITCH)
		ucmd->pitch = ReadWord (stream);
	if (flags & UCMDF_YAW)
		ucmd->yaw = ReadWord (stream);
	if (flags & UCMDF_ROLL)
		ucmd->roll = ReadWord (stream);
	if (flags & UCMDF_FORWARDMOVE)
		ucmd->forwardmove = ReadWord (stream);
	if (flags & UCMDF_SIDEMOVE)
		ucmd->sidemove = ReadWord (stream);
	if (flags & UCMDF_UPMOVE)
		ucmd->upmove = ReadWord (stream);
	if (flags & UCMDF_IMPULSE)
		ucmd->impulse = ReadByte (stream);
	if (flags & UCMDF_LIGHTLEVEL)
		ucmd->lightlevel = ReadByte (stream);

	return *stream - start;
}

// Returns the number of bytes written
int PackUserCmd (usercmd_t *ucmd, byte **stream)
{
	byte flags = 0;
	byte *temp = *stream;
	byte *start = *stream;

	WriteByte (0, stream);			// Make room for the packing bits

	WriteByte (ucmd->buttons, stream);
	if (ucmd->pitch) {
		flags |= UCMDF_PITCH;
		WriteWord (ucmd->pitch, stream);
	}
	if (ucmd->yaw) {
		flags |= UCMDF_YAW;
		WriteWord (ucmd->yaw, stream);
	}
	if (ucmd->roll) {
		flags |= UCMDF_ROLL;
		WriteWord (ucmd->roll, stream);
	}
	if (ucmd->forwardmove) {
		flags |= UCMDF_FORWARDMOVE;
		WriteWord (ucmd->forwardmove, stream);
	}
	if (ucmd->sidemove) {
		flags |= UCMDF_SIDEMOVE;
		WriteWord (ucmd->sidemove, stream);
	}
	if (ucmd->upmove) {
		flags |= UCMDF_UPMOVE;
		WriteWord (ucmd->upmove, stream);
	}
	if (ucmd->impulse) {
		flags |= UCMDF_IMPULSE;
		WriteByte (ucmd->impulse, stream);
	}
	if (ucmd->lightlevel) {
		flags |= UCMDF_LIGHTLEVEL;
		WriteByte (ucmd->lightlevel, stream);
	}

	WriteByte (flags, &temp);

	return *stream - start;
}

int WriteUserCmdMessage (usercmd_t *ucmd, byte **stream)
{
	byte *lenspot;
	short len;

	lenspot = *stream;
	WriteWord (0, stream);
	WriteByte (SVC_USERCMD, stream);
	len = PackUserCmd (ucmd, stream);
	WriteWord (len, &lenspot);
	return len + 3;
}