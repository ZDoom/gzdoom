/*
** p_tags.cpp
** everything to do with tags and their management
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
**
*/


#include "p_tags.h"
#include "c_dispatch.h"

FTagManager tagManager;

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

static inline int sectindex(const sector_t *sector)
{
	return (int)(intptr_t)(sector - sectors);
}

static inline int lineindex(const line_t *line)
{
	return (int)(intptr_t)(line - lines);
}

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

void FTagManager::AddSectorTag(int sector, int tag)
{
	if (tag == 0) return;

	// This function assumes that all tags for a single sector get added sequentially.
	// Should there ever be some need for compatibility.txt to add tags to sectors which already have a tag this function needs to be changed to adjust the startForSector indices.
	while (startForSector.Size() <= (unsigned int)sector)
	{
		startForSector.Push(-1);
	}
	if (startForSector[sector] == -1)
	{
		startForSector[sector] = allTags.Size();
	}
	else
	{
		// check if the key was already defined
		for (unsigned i = startForSector[sector]; i < allTags.Size(); i++)
		{
			if (allTags[i].tag == tag)
			{
				return;
			}
		}
	}
	FTagItem it = { sector, tag, -1 };
	allTags.Push(it);
}

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

void FTagManager::RemoveSectorTags(int sect)
{
	if (startForSector.Size() > (unsigned int)sect)
	{
		int start = startForSector[sect];
		if (start >= 0)
		{
			while (allTags[start].target == sect)
			{
				allTags[start].tag = allTags[start].target = -1;
				start++;
			}
		}
	}
}

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

void FTagManager::AddLineID(int line, int tag)
{
	if (tag == -1) return;	// For line IDs -1 means 'not set', unlike sectors.

	// This function assumes that all ids for a single line get added sequentially.
	while (startForLine.Size() <= (unsigned int)line)
	{
		startForLine.Push(-1);
	}
	if (startForLine[line] == -1)
	{
		startForLine[line] = allIDs.Size();
	}
	else
	{
		// check if the key was already defined
		for (unsigned i = startForLine[line]; i < allIDs.Size(); i++)
		{
			if (allIDs[i].tag == tag)
			{
				return;
			}
		}
	}
	FTagItem it = { line, tag, -1 };
	allIDs.Push(it);
}

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

void FTagManager::HashTags()
{
	// add an end marker so we do not need to check for the array's size in the other functions.
	static FTagItem it = { -1, -1, -1 };
	allTags.Push(it);
	allIDs.Push(it);

	// Initially make all slots empty.
	memset(TagHashFirst, -1, sizeof(TagHashFirst));
	memset(IDHashFirst, -1, sizeof(IDHashFirst));

	// Proceed from last to first so that lower targets appear first
	for (int i = allTags.Size() - 1; i >= 0; i--)
	{
		if (allTags[i].target >= 0)	// only link valid entries
		{
			int hash = ((unsigned int)allTags[i].tag) % FTagManager::TAG_HASH_SIZE;
			allTags[i].nexttag = TagHashFirst[hash];
			TagHashFirst[hash] = i;
		}
	}

	for (int i = allIDs.Size() - 1; i >= 0; i--)
	{
		if (allIDs[i].target >= 0)	// only link valid entries
		{
			int hash = ((unsigned int)allIDs[i].tag) % FTagManager::TAG_HASH_SIZE;
			allIDs[i].nexttag = IDHashFirst[hash];
			IDHashFirst[hash] = i;
		}
	}

}

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

bool FTagManager::SectorHasTags(const sector_t *sector) const
{
	int i = sectindex(sector);
	return SectorHasTags(i);
}

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

int FTagManager::GetFirstSectorTag(const sector_t *sect) const
{
	int i = sectindex(sect);
	return SectorHasTags(i) ? allTags[startForSector[i]].tag : 0;
}

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

bool FTagManager::SectorHasTag(int i, int tag) const
{
	if (SectorHasTags(i))
	{
		int ndx = startForSector[i];
		while (allTags[ndx].target == i)
		{
			if (allTags[ndx].tag == tag) return true;
			ndx++;
		}
	}
	return false;
}

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

bool FTagManager::SectorHasTag(const sector_t *sector, int tag) const
{
	return SectorHasTag(sectindex(sector), tag);
}

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

int FTagManager::GetFirstLineID(const line_t *line) const
{
	int i = lineindex(line);
	return LineHasIDs(i) ? allIDs[startForLine[i]].tag : 0;
}

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

bool FTagManager::LineHasID(int i, int tag) const
{
	if (LineHasIDs(i))
	{
		int ndx = startForLine[i];
		while (allIDs[ndx].target == i)
		{
			if (allIDs[ndx].tag == tag) return true;
			ndx++;
		}
	}
	return false;
}

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

bool FTagManager::LineHasID(const line_t *line, int tag) const
{
	return LineHasID(lineindex(line), tag);
}

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

void FTagManager::DumpTags()
{
	for (unsigned i = 0; i < allTags.Size(); i++)
	{
		Printf("Sector %d, tag %d\n", allTags[i].target, allTags[i].tag);
	}
	for (unsigned i = 0; i < allIDs.Size(); i++)
	{
		Printf("Line %d, ID %d\n", allIDs[i].target, allIDs[i].tag);
	}
}

CCMD(dumptags)
{
	tagManager.DumpTags();
}

//-----------------------------------------------------------------------------
//
// RETURN NEXT SECTOR # THAT LINE TAG REFERS TO
//
// Find the next sector with a specified tag.
// Rewritten by Lee Killough to use chained hashing to improve speed
//
//-----------------------------------------------------------------------------

int FSectorTagIterator::Next()
{
	int ret;
	if (searchtag == INT_MIN)
	{
		ret = start;
		start = -1;
	}
	else if (searchtag != 0)
	{
		while (start >= 0 && tagManager.allTags[start].tag != searchtag) start = tagManager.allTags[start].nexttag;
		if (start == -1) return -1;
		ret = tagManager.allTags[start].target;
		start = tagManager.allTags[start].nexttag;
	}
	else
	{
		// with the tag manager, searching for tag 0 has to be different, because it won't create entries for untagged sectors.
		while (start < numsectors && tagManager.SectorHasTags(start))
		{
			start++;
		}
		if (start == numsectors) return -1;
		ret = start;
		start++;
	}
	return ret;
}

//-----------------------------------------------------------------------------
//
// linear search for compatible stair building
//
//-----------------------------------------------------------------------------

int FSectorTagIterator::NextCompat(bool compat, int start)
{
	if (!compat) return Next();

	for (int i = start + 1; i < numsectors; i++)
	{
		if (tagManager.SectorHasTag(i, searchtag)) return i;
	}
	return -1;
}


//-----------------------------------------------------------------------------
//
// killough 4/16/98: Same thing, only for linedefs
//
//-----------------------------------------------------------------------------

int FLineIdIterator::Next()
{
	while (start >= 0 && tagManager.allIDs[start].tag != searchtag) start = tagManager.allIDs[start].nexttag;
	if (start == -1) return -1;
	int ret = tagManager.allIDs[start].target;
	start = tagManager.allIDs[start].nexttag;
	return ret;
}
