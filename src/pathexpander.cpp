/*
** pathexpander.cpp
** Utility class for expanding a given path with a range of directories
**
**---------------------------------------------------------------------------
** Copyright 2015 Christoph Oelckers
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

#include "pathexpander.h"
#include "cmdlib.h"
#include "w_wad.h"

//============================================================================
//
//
//
//============================================================================

static FString BuildPath(const FString &base, const char *name)
{
	FString current;
	if (base.IsNotEmpty())
	{
		current = base;
		if (current.Back() != '/') current += '/';
	}
	current += name;
	return current;
}

//============================================================================
//
// This is meant to find and open files for reading.
//
//============================================================================

FileReader *PathExpander::openFileReader(const char *name, int *plumpnum)
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


	if (openmode != OM_FILE)
	{
		int lumpnum = Wads.CheckNumForFullName(current_filename);
		if (lumpnum >= 0)
		{
			fp = Wads.ReopenLumpNum(lumpnum);
			if (plumpnum) *plumpnum = lumpnum;
			return fp;
		}
		if (openmode == OM_LUMP)	// search the path list when not loading the main config
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
	return NULL;
}

/* This adds a directory to the path list */
void PathExpander::addToPathlist(const char *s)
{
	FString copy = s;
	FixPathSeperator(copy);
	PathList.Push(copy);
}

void PathExpander::clearPathlist()
{
	PathList.Clear();
}
