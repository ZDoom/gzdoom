/*

	TiMidity -- Experimental MIDI to WAVE converter
	Copyright (C) 1995 Tuukka Toivonen <toivonen@clinet.fi>

	This library is free software; you can redistribute it and/or
	modify it under the terms of the GNU Lesser General Public
	License as published by the Free Software Foundation; either
	version 2.1 of the License, or (at your option) any later version.

	This library is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
	Lesser General Public License for more details.

	You should have received a copy of the GNU Lesser General Public
	License along with this library; if not, write to the Free Software
	Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

	common.c

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "timidity.h"
#include "zstring.h"
#include "tarray.h"
#include "i_system.h"

namespace Timidity
{

/* I guess "rb" should be right for any libc */
#define OPEN_MODE "rb"

FString current_filename;

static TArray<FString> PathList;

/* Try to open a file for reading. */
static FILE *try_to_open(const char *name, int decompress, int noise_mode)
{
	FILE *fp;

	fp = fopen(name, OPEN_MODE);

	if (!fp)
		return 0;

	return fp;
}

/* This is meant to find and open files for reading. */
FILE *open_file(const char *name, int decompress, int noise_mode)
{
	FILE *fp;

	if (!name || !(*name))
	{
		return 0;
	}

	/* First try the given name */
	current_filename = name;

	if ((fp = try_to_open(current_filename, decompress, noise_mode)))
		return fp;

#ifdef ENOENT
	if (noise_mode && (errno != ENOENT))
	{
		return 0;
	}
#endif

	if (name[0] != '/'
#ifdef _WIN32
		&& name[0] != '\\'
#endif
	)
	{
		for (unsigned int plp = PathList.Size(); plp-- != 0; )
		{ /* Try along the path then */
			current_filename = "";
			if (PathList[plp].IsNotEmpty())
			{
				current_filename = PathList[plp];
				if (current_filename[current_filename.Len() - 1] != '/'
#ifdef _WIN32
					&& current_filename[current_filename.Len() - 1] != '\\'
#endif
					)
				{
					current_filename += '/';
				}
			}
			current_filename += name;
			if ((fp = try_to_open(current_filename, decompress, noise_mode)))
				return fp;
			if (noise_mode && (errno != ENOENT))
			{
				return 0;
			}
		}
	}

	/* Nothing could be opened. */
	current_filename = "";
	return 0;
}

/* This closes files opened with open_file */
void close_file(FILE *fp)
{
	fclose(fp);
}

/* This is meant for skipping a few bytes in a file or fifo. */
void skip(FILE *fp, size_t len)
{
	fseek(fp, (long)len, SEEK_CUR);
}

/* This'll allocate memory or die. */
void *safe_malloc(size_t count)
{
	void *p;
	if (count > (1 << 21))
	{
		I_Error("Timidity: Tried allocating %d bytes. This must be a bug.", count);
	}
	else if ((p = malloc(count)))
	{
		return p;
	}
	else
	{
		I_Error("Timidity: Couldn't malloc %d bytes.", count);
	}
	return 0;	// Unreachable.
}

/* This adds a directory to the path list */
void add_to_pathlist(const char *s)
{
	PathList.Push(s);
}

}
