#include <string.h>
#include <stdio.h>

#include "doomtype.h"
#include "lumpconfigfile.h"
#include "w_wad.h"
#include "z_zone.h"

FLumpConfigFile::FLumpConfigFile (int lump)
{
	State.lump = lump;
	State.lumpdata = (char *)W_CacheLumpNum (lump, PU_STATIC);
	State.end = State.lumpdata + lumpinfo[lump].size;
	State.pos = State.lumpdata;
}

FLumpConfigFile::~FLumpConfigFile ()
{
	Z_Free (State.lumpdata);
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
		n = state->end - state->pos;

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
