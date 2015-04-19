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

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

void FTagManager::AddSectorTag(int sector, int tag)
{
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

	// Initially make all slots empty.
	memset(TagHashFirst, -1, sizeof(TagHashFirst));

	// Proceed from last to first so that lower targets appear first
	for (int i = allTags.Size() - 1; i >= 0; i--)
	{
		if (allTags[i].target > 0)	// only link valid entries
		{
			int hash = ((unsigned int)allTags[i].tag) % FTagManager::TAG_HASH_SIZE;
			allTags[i].nexttag = TagHashFirst[hash];
			TagHashFirst[hash] = i;
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
	else
	{
		while (start >= 0 && tagManager.allTags[start].tag != searchtag) start = tagManager.allTags[start].nexttag;
		if (start == -1) return -1;
		ret = start;
		start = start = tagManager.allTags[start].nexttag;
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
	while (start != -1 && lines[start].id != searchtag) start = lines[start].nextid;
	if (start == -1) return -1;
	int ret = start;
	start = lines[start].nextid;
	return ret;
}


