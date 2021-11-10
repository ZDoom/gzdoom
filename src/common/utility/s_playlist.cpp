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
#include <errno.h>

#include "cmdlib.h"
#include "s_playlist.h"

#include "v_text.h"
#include "files.h"

FPlayList::~FPlayList ()
{
}

bool FPlayList::ChangeList (const char *path)
{
	FString playlistdir;
	FString song;
	FileReader fr;
	bool first;
	bool pls;
	int i;

	if (!fr.OpenFile(path))
	{
		return false;
	}
	Songs.Clear();
	Position = 0;

	first = true;
	pls = false;
	playlistdir = ExtractFilePath(path);
	while ((song = NextLine(fr)).IsNotEmpty())
	{
		if (first)
		{
			first = false;
			// Check for ID tags.
			if (song.Compare("[playlist]") == 0)
			{
				pls = true;
				continue;
			}
		}

		// For a .PLS file, skip anything that doesn't start with File[0-9]+=
		if (pls)
		{
			if (strncmp(song, "File", 4) != 0)
			{
				continue;
			}
			for (i = 4; song[i] >= '0' && song[i] <= '9'; ++i)
			{
			}
			if (song[i] != '=')
			{
				continue;
			}
			song = song.Mid(i + 1);
		}

		// Check for relative paths.
		auto slashpos = song.IndexOf('/');

		if (slashpos == 0)
		{
			// First character is a slash, so it's absolute.
		}
#ifdef _WIN32
		else if (slashpos == 2 && song[1] == ':')
		{
			// Name is something like X:/, so it's absolute.
		}
#endif
		else if (song.IndexOf("://") == slashpos - 1)
		{
			// Name is a URL, so it's absolute.
		}
		else
		{
			// Path is relative; append it to the playlist directory.
			song = playlistdir + song;
		}

		// Just to make sure
		if (song.IsNotEmpty())
		{
			Songs.Push(song);
		}
	}
	return Songs.Size() != 0;
}

FString FPlayList::NextLine (FileReader &file)
{
	char buffer[512];
	char *skipper;

	do
	{
		if (nullptr == file.Gets (buffer, countof(buffer)))
			return "";

		for (skipper = buffer; *skipper != 0 && *skipper <= ' '; skipper++)
			;
	} while (*skipper == '#' || *skipper == 0);

	FString str(skipper);
	str.StripRight("\r\n");
	FixPathSeperator(str);
	return str;
}

// Shuffles the playlist and resets the position to the start
void FPlayList::Shuffle ()
{
	unsigned int numsongs = Songs.Size();
	unsigned int i;

	for (i = 0; i < numsongs; ++i)
	{
		std::swap (Songs[i], Songs[(rand() % (numsongs - i)) + i]);
	}
	Position = 0;
}

int FPlayList::GetNumSongs () const
{
	return (int)Songs.Size();
}

int FPlayList::SetPosition (int position)
{
	if ((unsigned)position >= Songs.Size())
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
	if (++Position >= Songs.Size())
	{
		Position = 0;
	}
	return Position;
}

int FPlayList::Backup ()
{
	if (Position-- == 0)
	{
		Position = Songs.Size() - 1;
	}
	return Position;
}

const char *FPlayList::GetSong (int position) const
{
	if ((unsigned)position >= Songs.Size())
		return NULL;

	return Songs[position];
}
