/*
** s_playlist.cpp
** Implementation of the FPlayList class. Handles m3u playlist parsing
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

#include <stdlib.h>

#include "cmdlib.h"
#include "s_playlist.h"
#include "templates.h"

FPlayList::FPlayList (const char *path)
{
	Songs = NULL;
	SongList = NULL;
	ChangeList (path);
}

FPlayList::~FPlayList ()
{
	if (Songs)		delete[] Songs;
	if (SongList)	delete[] SongList;
}

bool FPlayList::ChangeList (const char *path)
{
	char linebuff[256];
	size_t songlengths;
	int songcount;
	FILE *file;

	if (Songs)
	{
		delete[] Songs;
		Songs = NULL;
	}
	if (SongList)
	{
		delete[] SongList;
		SongList = NULL;
	}
	Position = 0;
	NumSongs = 0;

	if ( (file = fopen (path, "r")) == NULL)
		return false;

	songlengths = 0;
	songcount = 0;

	while (NextLine (file, linebuff, sizeof(linebuff)))
	{
		songcount++;
		songlengths += strlen (linebuff) + 1;
	}

	rewind (file);

	if (songcount > 0)
	{
		Songs = new char *[songcount];
		SongList = new char[songlengths];
		NumSongs = songcount;
		songlengths = 0;

		for (songcount = 0; songcount < NumSongs &&
			NextLine (file, linebuff, sizeof(linebuff)); songcount++)
		{
			size_t len = strlen (linebuff) + 1;

			memcpy (SongList + songlengths, linebuff, len);
			Songs[songcount] = SongList + songlengths;
			songlengths += len;
		}

		NumSongs = songcount;
	}

	fclose (file);

	return NumSongs > 0;
}

bool FPlayList::NextLine (FILE *file, char *buffer, int n)
{
	char *skipper;

	do
	{
		if (NULL == fgets (buffer, n, file))
			return false;

		for (skipper = buffer; *skipper != 0 && *skipper <= ' '; skipper++)
			;
	} while (*skipper == '#' || *skipper == 0);

	if (skipper > buffer)
		memmove (buffer, skipper, strlen (skipper)+1);

	if (buffer[strlen (buffer)-1] == '\n')
		buffer[strlen (buffer)-1] = 0;

	FixPathSeperator (buffer);

	return true;
}

// Shuffles the playlist and resets the position to the start
void FPlayList::Shuffle ()
{
	int i;

	for (i = 0; i < NumSongs; ++i)
	{
		swap (Songs[i], Songs[(rand() % (NumSongs - i)) + i]);
	}
	Position = 0;
}

int FPlayList::GetNumSongs () const
{
	return NumSongs;
}

int FPlayList::SetPosition (int position)
{
	if ((unsigned)position >= (unsigned)NumSongs)
	{
		Position = 0;
	}
	else
	{
		Position = position;
	}
	DPrintf ("Playlist position set to %d\n", Position);
	return Position;
}

int FPlayList::GetPosition () const
{
	return Position;
}

int FPlayList::Advance ()
{
	if (++Position >= NumSongs)
	{
		Position = 0;
	}
	DPrintf ("Playlist advanced to song %d\n", Position);
	return Position;
}

int FPlayList::Backup ()
{
	if (--Position < 0)
	{
		Position = NumSongs - 1;
	}
	DPrintf ("Playlist backed up to song %d\n", Position);
	return Position;
}

const char *FPlayList::GetSong (int position) const
{
	if ((unsigned)position >= (unsigned)NumSongs)
		return NULL;

	return Songs[position];
}
