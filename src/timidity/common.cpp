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
#include "tarray.h"
#include "i_system.h"
#include "w_wad.h"
#include "files.h"
#include "cmdlib.h"

namespace Timidity
{

static TArray<FString> PathList;

FString BuildPath(FString base, const char *name)
{
	FString current;
	if (base.IsNotEmpty())
	{
		current = base;
		if (current[current.Len() - 1] != '/') current += '/';
	}
	current += name;
	return current;
}

/* This is meant to find and open files for reading. */
FileReader *open_filereader(const char *name, int open, int *plumpnum)
{
	FileReader *fp;
	FString current_filename;

	if (!name || !(*name))
	{
		return 0;
	}

	/* First try the given name */
	current_filename = name;
	FixPathSeperator(current_filename);

	int lumpnum = Wads.CheckNumForFullName(current_filename);

	if (open != OM_FILE)
	{
		if (lumpnum >= 0)
		{
			fp = Wads.ReopenLumpNum(lumpnum);
			if (plumpnum) *plumpnum = lumpnum;
			return fp;
		}
		if (open == OM_LUMP)	// search the path list when not loading the main config
		{
			for (unsigned int plp = PathList.Size(); plp-- != 0; )
			{ /* Try along the path then */
				current_filename = BuildPath(PathList[plp], name);
				lumpnum = Wads.CheckNumForFullName(current_filename);
				if (lumpnum >= 0)
				{
					fp = Wads.ReopenLumpNum(lumpnum);
					if (plumpnum) *plumpnum = lumpnum;
					return fp;
				}
			}
			return NULL;
		}
	}
	if (plumpnum) *plumpnum = -1;


	fp = new FileReader;
	if (fp->Open(current_filename)) return fp;

	if (name[0] != '/')
	{
		for (unsigned int plp = PathList.Size(); plp-- != 0; )
		{ /* Try along the path then */
			current_filename = BuildPath(PathList[plp], name);
			if (fp->Open(current_filename)) return fp;
		}
	}
	delete fp;

	/* Nothing could be opened. */
	current_filename = "";
	return NULL;
}



/* This'll allocate memory or die. */
void *safe_malloc(size_t count)
{
	void *p;
	if (count > (1 << 21))
	{
		I_Error("Timidity: Tried allocating %zu bytes. This must be a bug.", count);
	}
	else if ((p = malloc(count)))
	{
		return p;
	}
	else
	{
		I_Error("Timidity: Couldn't malloc %zu bytes.", count);
	}
	return 0;	// Unreachable.
}

/* This adds a directory to the path list */
void add_to_pathlist(const char *s)
{
	FString copy = s;
	FixPathSeperator(copy);
	PathList.Push(copy);
}

void clear_pathlist()
{
	PathList.Clear();
}

}
