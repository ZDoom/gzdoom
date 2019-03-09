/*
** utf8.cpp
** UTF-8 utilities
**
**---------------------------------------------------------------------------
** Copyright 2019 Christoph Oelckers
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
#include <stdint.h>
#include "tarray.h"


//==========================================================================
//
//
//
//==========================================================================

int utf8_encode(int32_t codepoint, uint8_t *buffer, int *size)
{
	if (codepoint < 0)
		return -1;
	else if (codepoint < 0x80)
	{
		buffer[0] = (char)codepoint;
		*size = 1;
	}
	else if (codepoint < 0x800)
	{
		buffer[0] = 0xC0 + ((codepoint & 0x7C0) >> 6);
		buffer[1] = 0x80 + ((codepoint & 0x03F));
		*size = 2;
	}
	else if (codepoint < 0x10000)
	{
		buffer[0] = 0xE0 + ((codepoint & 0xF000) >> 12);
		buffer[1] = 0x80 + ((codepoint & 0x0FC0) >> 6);
		buffer[2] = 0x80 + ((codepoint & 0x003F));
		*size = 3;
	}
	else if (codepoint <= 0x10FFFF)
	{
		buffer[0] = 0xF0 + ((codepoint & 0x1C0000) >> 18);
		buffer[1] = 0x80 + ((codepoint & 0x03F000) >> 12);
		buffer[2] = 0x80 + ((codepoint & 0x000FC0) >> 6);
		buffer[3] = 0x80 + ((codepoint & 0x00003F));
		*size = 4;
	}
	else
		return -1;

	return 0;
}

//==========================================================================
//
//
//
//==========================================================================

int utf8_decode(const uint8_t *src, int *size) 
{
	int c = src[0];
	int r;

	*size = 1;
	if ((c & 0x80) == 0)
	{
		return c;
	}

	int c1 = src[1];
	if (c1 < 0x80 || c1 >= 0xc0) return -1;
	c1 &= 0x3f;

	if ((c & 0xE0) == 0xC0) 
	{
		r = ((c & 0x1F) << 6) | c1;
		if (r >= 128) 
		{
			*size = 2;
			return r;
		}
		return -1;
	}

	int c2 = src[2];
	if (c2 < 0x80 || c2 >= 0xc0) return -1;
	c2 &= 0x3f;

	if ((c & 0xF0) == 0xE0) 
	{
		r = ((c & 0x0F) << 12) | (c1 << 6) | c2;
		if (r >= 2048 && (r < 55296 || r > 57343)) 
		{
			*size = 3;
			return r;
		}
		return -1;
	}
	
	int c3 = src[3];
	if (c3 < 0x80 || c1 >= 0xc0) return -1;
	c3 &= 0x3f;

	if ((c & 0xF8) == 0xF0) 
	{
		r = ((c & 0x07) << 18) | (c1 << 12) | (c2 << 6) | c3;
		if (r >= 65536 && r <= 1114111) 
		{
			*size = 4;
			return r;
		}
	}
	return -1;
}

//==========================================================================
//
// Unicode mapping for the 0x80-0x9f range of the Windows 1252 code page
//
//==========================================================================

uint16_t win1252map[] = {
	0x20AC,
	0x81  ,
	0x201A,
	0x0192,
	0x201E,
	0x2026,
	0x2020,
	0x2021,
	0x02C6,
	0x2030,
	0x0160,
	0x2039,
	0x0152,
	0x8d  ,
	0x017D,
	0x8f  ,
	0x90  ,
	0x2018,
	0x2019,
	0x201C,
	0x201D,
	0x2022,
	0x2013,
	0x2014,
	0x02DC,
	0x2122,
	0x0161,
	0x203A,
	0x0153,
	0x9d  ,
	0x017E,
	0x0178,
};

//==========================================================================
//
// reads one character from the string.
// This can handle both ISO 8859-1/Windows-1252 and UTF-8, as well as mixed strings
// between both encodings, which may happen if inconsistent encoding is 
// used between different files in a mod.
//
//==========================================================================

int GetCharFromString(const uint8_t *&string)
{
	int z;

	z = *string;

	if (z < 192)
	{
		string++;
		
		// Handle Windows 1252 characters
		if (z >= 128 && z < 160)
		{
			return win1252map[z - 128];
		}
		return z;
	}
	else 
	{
		int size = 0;
		auto chr = utf8_decode(string, &size);
		if (chr >= 0)
		{
			string += size;
			return chr;
		}
		string++;
		return z;
	}
}

//==========================================================================
//
// convert a potentially mixed-encoded string to pure UTF-8
// this returns a pointer to a static buffer, 
// assuming that its caller will immediately process the result. 
//
//==========================================================================

static TArray<char> UTF8String;

const char *MakeUTF8(const char *outline, int *numchars = nullptr)
{
	UTF8String.Clear();
	const uint8_t *in = (const uint8_t*)outline;

	if (numchars) *numchars = 0;
	while (int chr = GetCharFromString(in))
	{
		int size = 0;
		uint8_t encode[4];
		if (!utf8_encode(chr, encode, &size))
		{
			for (int i = 0; i < size; i++)
			{
				UTF8String.Push(encode[i]);
			}
		}
		if (numchars) *numchars++;
	}
	UTF8String.Push(0);
	return UTF8String.Data();
}

const char *MakeUTF8(int codepoint, int *psize)
{
	int size = 0;
	UTF8String.Resize(5);
	utf8_encode(codepoint, (uint8_t*)UTF8String.Data(), &size);
	UTF8String[size] = 0;
	if (psize) *psize = size;
	return UTF8String.Data();
}
