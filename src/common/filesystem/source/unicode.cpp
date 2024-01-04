/*
** unicode.cpp
** handling for conversion / comparison of filenames
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

#include "unicode.h"
#include "utf8proc.h"

namespace FileSys {
//==========================================================================
//
//
//
//==========================================================================

static void utf8_encode(int32_t codepoint, std::vector<char>& buffer)
{
	if (codepoint < 0 || codepoint > 0x10FFFF || (codepoint >= 0xD800 && codepoint < 0xDFFF))
	{
		codepoint = 0xFFFD;
	}
	uint8_t buf[4];
	auto size = utf8proc_encode_char(codepoint, buf);
	for(int i = 0; i < size; i++)
		buffer.push_back(buf[i]);
}


//==========================================================================
//
// convert UTF16 to UTF8 (needed for 7z)
//
//==========================================================================

void utf16_to_utf8(const unsigned short* in, std::vector<char>& buffer)
{
	buffer.clear();
	if (!*in) return;

	while (int char1 = *in++)
	{
		if (char1 >= 0xD800 && char1 <= 0xDBFF)
		{
			int char2 = *in;

			if (char2 >= 0xDC00 && char2 <= 0xDFFF)
			{
				in++;
				char1 -= 0xD800;
				char2 -= 0xDC00;
				char1 <<= 10;
				char1 += char2;
				char1 += 0x010000;
			}
			else
			{
				// invalid code point - replace with placeholder
				char1 = 0xFFFD;
			}
		}
		else if (char1 >= 0xDC00 && char1 <= 0xDFFF)
		{
			// invalid code point - replace with placeholder
			char1 = 0xFFFD;
		}
		utf8_encode(char1, buffer);
	}
	buffer.push_back(0);
}

//==========================================================================
//
// convert UTF16 to UTF8 (needed for Zip)
//
//==========================================================================

void ibm437_to_utf8(const char* in, std::vector<char>& buffer)
{
	static const uint16_t ibm437map[] = {
		0x00c7,	0x00fc,	0x00e9,	0x00e2,	0x00e4,	0x00e0,	0x00e5,	0x00e7,	0x00ea,	0x00eb,	0x00e8,	0x00ef,	0x00ee,	0x00ec,	0x00c4,	0x00c5,
		0x00c9,	0x00e6,	0x00c6,	0x00f4,	0x00f6,	0x00f2,	0x00fb,	0x00f9,	0x00ff,	0x00d6,	0x00dc,	0x00a2,	0x00a3,	0x00a5,	0x20a7,	0x0192,
		0x00e1,	0x00ed,	0x00f3,	0x00fa,	0x00f1,	0x00d1,	0x00aa,	0x00ba,	0x00bf,	0x2310,	0x00ac,	0x00bd,	0x00bc,	0x00a1,	0x00ab,	0x00bb,
		0x2591,	0x2592,	0x2593,	0x2502,	0x2524,	0x2561,	0x2562,	0x2556,	0x2555,	0x2563,	0x2551,	0x2557,	0x255d,	0x255c,	0x255b,	0x2510,
		0x2514,	0x2534,	0x252c,	0x251c,	0x2500,	0x253c,	0x255e,	0x255f,	0x255a,	0x2554,	0x2569,	0x2566,	0x2560,	0x2550,	0x256c,	0x2567,
		0x2568,	0x2564,	0x2565,	0x2559,	0x2558,	0x2552,	0x2553,	0x256b,	0x256a,	0x2518,	0x250c,	0x2588,	0x2584,	0x258c,	0x2590,	0x2580,
		0x03b1,	0x00df,	0x0393,	0x03c0,	0x03a3,	0x03c3,	0x00b5,	0x03c4,	0x03a6,	0x0398,	0x03a9,	0x03b4,	0x221e,	0x03c6,	0x03b5,	0x2229,
		0x2261,	0x00b1,	0x2265,	0x2264,	0x2320,	0x2321,	0x00f7,	0x2248,	0x00b0,	0x2219,	0x00b7,	0x221a,	0x207f,	0x00b2,	0x25a0,	0x00a0,
	};
	
	buffer.clear();
	if (!*in) return;

	while (int char1 = (uint8_t)*in++)
	{
		if (char1 >= 0x80) char1 = ibm437map[char1 - 0x80];
		utf8_encode(char1, buffer);
	}
	buffer.push_back(0);
}

//==========================================================================
//
// create a normalized lowercase version of a string.
//
//==========================================================================

char *tolower_normalize(const char *str) 
{
	utf8proc_uint8_t *retval;
	utf8proc_map((const uint8_t*)str, 0, &retval, (utf8proc_option_t)(UTF8PROC_NULLTERM | UTF8PROC_STABLE | UTF8PROC_COMPOSE | UTF8PROC_CASEFOLD));
	return (char*)retval;
}

//==========================================================================
//
// validates the string for proper UTF-8
//
//==========================================================================

bool unicode_validate(const char* str)
{
	while (*str != 0)
	{
		int cp;
		auto result = utf8proc_iterate((const uint8_t*)str, -1, &cp);
		if (result < 0) return false;
	}
	return true;
}


}
