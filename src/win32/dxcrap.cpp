/*
** dxcrap.cpp
** Contains data normally obtained from dxguid.lib
**
**---------------------------------------------------------------------------
** Copyright 1998-2001 Randy Heit
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
** This file was originally assisted to help people with non-MS compilers
** who could not use dxguid.lib. Since then, I can no longer figure out how
** to get ZDoom to link with dxguid.lib and use the copies there, so dxcrap
** will probably stay forever.
*/

#include <dinput.h>

const GUID GUID_XAxis		= {0xA36D02E0,0xC9F3,0x11CF,0xBF,0xC7,0x44,0x45,0x53,0x54,0x00,0x00};
const GUID GUID_YAxis		= {0xA36D02E1,0xC9F3,0x11CF,0xBF,0xC7,0x44,0x45,0x53,0x54,0x00,0x00};
const GUID GUID_ZAxis		= {0xA36D02E2,0xC9F3,0x11CF,0xBF,0xC7,0x44,0x45,0x53,0x54,0x00,0x00};
const GUID GUID_Key			= {0x55728220,0xD33C,0x11CF,0xBF,0xC7,0x44,0x45,0x53,0x54,0x00,0x00};
const GUID GUID_SysMouse	= {0x6F1D2B60,0xD5A0,0x11CF,0xBF,0xC7,0x44,0x45,0x53,0x54,0x00,0x00};
const GUID GUID_SysKeyboard	= {0x6F1D2B61,0xD5A0,0x11CF,0xBF,0xC7,0x44,0x45,0x53,0x54,0x00,0x00};

static DIOBJECTDATAFORMAT KeyboardObjectData[256];

const DIDATAFORMAT c_dfDIKeyboard =
{
	24, 16, 2, 256, 256, KeyboardObjectData
};

static DIOBJECTDATAFORMAT MouseObjectData[7] =
{
	{&GUID_XAxis,  0, 0x00ffff03, 0},
	{&GUID_YAxis,  4, 0x00ffff03, 0},
	{&GUID_ZAxis,  8, 0x80ffff03, 0},
	{NULL,		  12, 0x00ffff0c, 0},
	{NULL,		  13, 0x00ffff0c, 0},
	{NULL,		  14, 0x80ffff0c, 0},
	{NULL,		  15, 0x80ffff0c, 0}
};

const DIDATAFORMAT c_dfDIMouse =
{
	24, 16, 2, 16, 7, MouseObjectData
};

// Using this instead of using a pre-initialized array
// chops ~4k off the size of the executable. Big deal,
// but I felt like doing it anyway.
void InitKeyboardObjectData (void)
{
	int i;

	for (i = 0; i < 256; i++)
	{
		KeyboardObjectData[i].pguid = &GUID_Key;
		KeyboardObjectData[i].dwOfs = i;
		KeyboardObjectData[i].dwType = 0x8000000c | (i << 8);
		KeyboardObjectData[i].dwFlags = 0;
	}
}
