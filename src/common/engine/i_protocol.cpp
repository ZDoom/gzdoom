/*
** i_protocol.cpp
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

#include "i_protocol.h"
#include "engineerrors.h"
#include "cmdlib.h"

// Unchecked stream functions.
// Use the checked versions instead unless you're checking the stream size yourself!

char* UncheckedReadString(uint8_t** stream)
{
	char* string = *((char**)stream);

	*stream += strlen(string) + 1;
	return copystring(string);
}

const char* UncheckedReadStringConst(uint8_t** stream)
{
	const char* string = *((const char**)stream);
	*stream += strlen(string) + 1;
	return string;
}

uint8_t UncheckedReadInt8(uint8_t** stream)
{
	uint8_t v = **stream;
	*stream += 1;
	return v;
}

int16_t UncheckedReadInt16(uint8_t** stream)
{
	int16_t v = (((*stream)[0]) << 8) | (((*stream)[1]));
	*stream += 2;
	return v;
}

int32_t UncheckedReadInt32(uint8_t** stream)
{
	int32_t v = (((*stream)[0]) << 24) | (((*stream)[1]) << 16) | (((*stream)[2]) << 8) | (((*stream)[3]));
	*stream += 4;
	return v;
}

int64_t UncheckedReadInt64(uint8_t** stream)
{
	int64_t v = (int64_t((*stream)[0]) << 56) | (int64_t((*stream)[1]) << 48) | (int64_t((*stream)[2]) << 40) | (int64_t((*stream)[3]) << 32)
		| (int64_t((*stream)[4]) << 24) | (int64_t((*stream)[5]) << 16) | (int64_t((*stream)[6]) << 8) | (int64_t((*stream)[7]));
	*stream += 8;
	return v;
}

float UncheckedReadFloat(uint8_t** stream)
{
	union
	{
		int32_t i;
		float f;
	} fakeint;
	fakeint.i = UncheckedReadInt32(stream);
	return fakeint.f;
}

double UncheckedReadDouble(uint8_t** stream)
{
	union
	{
		int64_t i;
		double f;
	} fakeint;
	fakeint.i = UncheckedReadInt64(stream);
	return fakeint.f;
}

void UncheckedWriteString(const char* string, uint8_t** stream)
{
	char* p = *((char**)stream);

	while (*string) {
		*p++ = *string++;
	}

	*p++ = 0;
	*stream = (uint8_t*)p;
}

void UncheckedWriteInt8(uint8_t v, uint8_t** stream)
{
	**stream = v;
	*stream += 1;
}

void UncheckedWriteInt16(int16_t v, uint8_t** stream)
{
	(*stream)[0] = v >> 8;
	(*stream)[1] = v & 255;
	*stream += 2;
}

void UncheckedWriteInt32(int32_t v, uint8_t** stream)
{
	(*stream)[0] = v >> 24;
	(*stream)[1] = (v >> 16) & 255;
	(*stream)[2] = (v >> 8) & 255;
	(*stream)[3] = v & 255;
	*stream += 4;
}

void UncheckedWriteInt64(int64_t v, uint8_t** stream)
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

void UncheckedWriteFloat(float v, uint8_t** stream)
{
	union
	{
		int32_t i;
		float f;
	} fakeint;
	fakeint.f = v;
	UncheckedWriteInt32(fakeint.i, stream);
}

void UncheckedWriteDouble(double v, uint8_t** stream)
{
	union
	{
		int64_t i;
		double f;
	} fakeint;
	fakeint.f = v;
	UncheckedWriteInt64(fakeint.i, stream);
}

void AdvanceStream(TArrayView<uint8_t>& stream, size_t bytes)
{
	assert(bytes <= stream.Size());
	stream = TArrayView(stream.Data() + bytes, stream.Size() - bytes);
}

// Checked stream functions

char* ReadString(TArrayView<uint8_t>& stream)
{
	char* string = (char*)stream.Data();
	size_t len = strnlen(string, stream.Size());
	if (len == stream.Size())
	{
		I_Error("Attempted to read past end of stream");
	}
	AdvanceStream(stream, len + 1);
	return copystring(string);
}

const char* ReadStringConst(TArrayView<uint8_t>& stream)
{
	const char* string = (const char*)stream.Data();
	size_t len = strnlen(string, stream.Size());
	if (len == stream.Size())
	{
		I_Error("Attempted to read past end of stream");
	}
	AdvanceStream(stream, len + 1);
	return string;
}

void ReadBytes(TArrayView<uint8_t>& dst, TArrayView<uint8_t>& stream)
{
	if (dst.Size() > stream.Size())
	{
		I_Error("Attempted to read past end of stream");
	}
	if (dst.Size())
	{
		memcpy(dst.Data(), stream.Data(), dst.Size());
		AdvanceStream(stream, dst.Size());
	}
}

uint8_t ReadInt8(TArrayView<uint8_t>& stream)
{
	if (stream.Size() < 1)
	{
		I_Error("Attempted to read past end of stream");
	}
	uint8_t v = stream[0];
	AdvanceStream(stream, 1);
	return v;
}

int16_t ReadInt16(TArrayView<uint8_t>& stream)
{
	if (stream.Size() < 2)
	{
		I_Error("Attempted to read past end of stream");
	}
	int16_t v = ((stream[0]) << 8) | stream[1];
	AdvanceStream(stream, 2);
	return v;
}

int32_t ReadInt32(TArrayView<uint8_t>& stream)
{
	if (stream.Size() < 4)
	{
		I_Error("Attempted to read past end of stream");
	}
	int32_t v = (stream[0] << 24) | (stream[1] << 16) | (stream[2] << 8) | stream[3];
	AdvanceStream(stream, 4);
	return v;
}

int64_t ReadInt64(TArrayView<uint8_t>& stream)
{
	if (stream.Size() < 8)
	{
		I_Error("Attempted to read past end of stream");
	}
	int64_t v = (int64_t(stream[0]) << 56) | (int64_t(stream[1]) << 48) | (int64_t(stream[2]) << 40) | (int64_t(stream[3]) << 32)
		| (int64_t(stream[4]) << 24) | (int64_t(stream[5]) << 16) | (int64_t(stream[6]) << 8) | int64_t(stream[7]);
	AdvanceStream(stream, 8);
	return v;
}

float ReadFloat(TArrayView<uint8_t>& stream)
{
	union
	{
		int32_t i;
		float f;
	} fakeint;
	fakeint.i = ReadInt32(stream);
	return fakeint.f;
}

double ReadDouble(TArrayView<uint8_t>& stream)
{
	union
	{
		int64_t i;
		double f;
	} fakeint;
	fakeint.i = ReadInt64(stream);
	return fakeint.f;
}

void WriteString(const char* string, TArrayView<uint8_t>& stream)
{
	char* p = (char*)stream.Data();
	unsigned int remaining = stream.Size();

	while (*string) {
		if (remaining-- == 0)
		{
			I_Error("Attempted to write past end of stream");
		}
		*p++ = *string++;
	}

	if (remaining == 0)
	{
		I_Error("Attempted to write past end of stream");
	}
	*p++ = 0;
	AdvanceStream(stream, p - (char*)stream.Data());
}

void WriteBytes(const TArrayView<uint8_t>& source, TArrayView<uint8_t>& stream)
{
	if (source.Size() > stream.Size())
	{
		I_Error("Attempted to write past end of stream");
	}
	if (source.Size())
	{
		memcpy(stream.Data(), source.Data(), source.Size());
		AdvanceStream(stream, source.Size());
	}
}

void WriteFString(FString& string, TArrayView<uint8_t>& stream)
{
	WriteBytes(string.GetTArrayView(), stream);
}

void WriteInt8(uint8_t v, TArrayView<uint8_t>& stream)
{
	if (stream.Size() < 1)
	{
		I_Error("Attempted to write past end of stream");
	}
	stream[0] = v;
	AdvanceStream(stream, 1);
}

void WriteInt16(int16_t v, TArrayView<uint8_t>& stream)
{
	if (stream.Size() < 2)
	{
		I_Error("Attempted to write past end of stream");
	}
	stream[0] = v >> 8;
	stream[1] = v & 255;
	AdvanceStream(stream, 2);
}

void WriteInt32(int32_t v, TArrayView<uint8_t>& stream)
{
	if (stream.Size() < 4)
	{
		I_Error("Attempted to write past end of stream");
	}
	stream[0] = v >> 24;
	stream[1] = (v >> 16) & 255;
	stream[2] = (v >> 8) & 255;
	stream[3] = v & 255;
	AdvanceStream(stream, 4);
}

void WriteInt64(int64_t v, TArrayView<uint8_t>& stream)
{
	if (stream.Size() < 8)
	{
		I_Error("Attempted to write past end of stream");
	}
	stream[0] = v >> 56;
	stream[1] = (v >> 48) & 255;
	stream[2] = (v >> 40) & 255;
	stream[3] = (v >> 32) & 255;
	stream[4] = (v >> 24) & 255;
	stream[5] = (v >> 16) & 255;
	stream[6] = (v >> 8) & 255;
	stream[7] = v & 255;
	AdvanceStream(stream, 8);
}

void WriteFloat(float v, TArrayView<uint8_t>& stream)
{
	union
	{
		int32_t i;
		float f;
	} fakeint;
	fakeint.f = v;
	WriteInt32(fakeint.i, stream);
}

void WriteDouble(double v, TArrayView<uint8_t>& stream)
{
	union
	{
		int64_t i;
		double f;
	} fakeint;
	fakeint.f = v;
	WriteInt64(fakeint.i, stream);
}
