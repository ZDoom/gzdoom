/*
** lumpconfigfile.cpp
** Reads an .ini file in a lump
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
** I don't remember what I was going to use this for, but it's small, so I'll
** leave it in for now.
*/

#include <string.h>
#include <stdio.h>

#include "doomtype.h"
#include "lumpconfigfile.h"
#include "w_wad.h"

#if 0
FLumpConfigFile::FLumpConfigFile (int lump)
{
	State.lump = lump;
	State.lumpdata = (char *)W_MapLumpNum (lump);
	State.end = State.lumpdata + lumpinfo[lump].size;
	State.pos = State.lumpdata;
}

FLumpConfigFile::~FLumpConfigFile ()
{
	W_UnMapLump (State.lumpdata);
}

void FLumpConfigFile::LoadConfigFile ()
{
	ReadConfig (&State);
}

char *FLumpConfigFile::ReadLine (char *string, int n, void *file) const
{
	LumpState *state = (LumpState *)file;
	char *pos = state->pos;
	int len = 0;

	n--;
	if (state->pos + n > state->end)
		n = (int)(state->end - state->pos);

	while (len < n && pos[n] != '\n')
		n++;

	string[n] = 0;
	if (n == 0)
	{
		return NULL;
	}

	memcpy (string, pos, n);
	state->pos += n;
	return string;
}
#endif
