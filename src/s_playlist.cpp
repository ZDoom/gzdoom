//**************************************************************************
//**
//** s_playlist.cpp
//**
//** [RH] Implementation of the FPlayList class. Handles WinAmp (m3u)
//** playlist parsing.
//**
//**************************************************************************

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
	int songlengths;
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
			int len = strlen (linebuff) + 1;

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

		for (skipper = buffer; *skipper <= ' '; skipper++)
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
		swap (Songs[i], Songs[rand()]);
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
	return Position;
}

int FPlayList::Backup ()
{
	if (--Position < 0)
	{
		Position = NumSongs - 1;
	}
	return Position;
}

const char *FPlayList::GetSong (int position) const
{
	if ((unsigned)position >= (unsigned)NumSongs)
		return NULL;

	return Songs[position];
}
