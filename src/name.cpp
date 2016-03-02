/*
** name.cpp
** Implements int-as-string mapping.
**
**---------------------------------------------------------------------------
** Copyright 2005-2007 Randy Heit
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

#include <string.h>
#include "name.h"
#include "c_dispatch.h"

// MACROS ------------------------------------------------------------------

// The number of bytes to allocate to each NameBlock unless somebody is evil
// and wants a really long name. In that case, it gets its own NameBlock
// that is just large enough to hold it.
#define BLOCK_SIZE			4096

// How many entries to grow the NameArray by when it needs to grow.
#define NAME_GROW_AMOUNT	256

// TYPES -------------------------------------------------------------------

// Name text is stored in a linked list of NameBlock structures. This
// is really the header for the block, with the remainder of the block
// being populated by text for names.

struct FName::NameManager::NameBlock
{
	size_t NextAlloc;
	NameBlock *NextBlock;
};

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

FName::NameManager FName::NameData;
bool FName::NameManager::Inited;

// Define the predefined names.
static const char *PredefinedNames[] =
{
#define xx(n) #n,
#include "namedef.h"
#undef xx
};

// CODE --------------------------------------------------------------------

//==========================================================================
//
// FName :: NameManager :: FindName
//
// Returns the index of a name. If the name does not exist and noCreate is
// true, then it returns false. If the name does not exist and noCreate is
// false, then the name is added to the table and its new index is returned.
//
//==========================================================================

int FName::NameManager::FindName (const char *text, bool noCreate)
{
	if (!Inited)
	{
		InitBuckets ();
	}

	if (text == NULL)
	{
		return 0;
	}

	unsigned int hash = MakeKey (text);
	unsigned int bucket = hash % HASH_SIZE;
	int scanner = Buckets[bucket];

	// See if the name already exists.
	while (scanner >= 0)
	{
		if (NameArray[scanner].Hash == hash && stricmp (NameArray[scanner].Text, text) == 0)
		{
			return scanner;
		}
		scanner = NameArray[scanner].NextHash;
	}

	// If we get here, then the name does not exist.
	if (noCreate)
	{
		return 0;
	}

	return AddName (text, hash, bucket);
}

//==========================================================================
//
// The same as above, but the text length is also passed, for creating
// a name from a substring or for speed if the length is already known.
//
//==========================================================================

int FName::NameManager::FindName (const char *text, size_t textLen, bool noCreate)
{
	if (!Inited)
	{
		InitBuckets ();
	}

	if (text == NULL)
	{
		return 0;
	}

	unsigned int hash = MakeKey (text, textLen);
	unsigned int bucket = hash % HASH_SIZE;
	int scanner = Buckets[bucket];

	// See if the name already exists.
	while (scanner >= 0)
	{
		if (NameArray[scanner].Hash == hash &&
			strnicmp (NameArray[scanner].Text, text, textLen) == 0 &&
			NameArray[scanner].Text[textLen] == '\0')
		{
			return scanner;
		}
		scanner = NameArray[scanner].NextHash;
	}

	// If we get here, then the name does not exist.
	if (noCreate)
	{
		return 0;
	}

	return AddName (text, hash, bucket);
}

//==========================================================================
//
// FName :: NameManager :: InitBuckets
//
// Sets up the hash table and inserts all the default names into the table.
//
//==========================================================================

void FName::NameManager::InitBuckets ()
{
	Inited = true;
	memset (Buckets, -1, sizeof(Buckets));

	// Register built-in names. 'None' must be name 0.
	for (size_t i = 0; i < countof(PredefinedNames); ++i)
	{
		assert((0 == FindName(PredefinedNames[i], true)) && "Predefined name already inserted");
		FindName (PredefinedNames[i], false);
	}
}

//==========================================================================
//
// FName :: NameManager :: AddName
//
// Adds a new name to the name table.
//
//==========================================================================

int FName::NameManager::AddName (const char *text, unsigned int hash, unsigned int bucket)
{
	char *textstore;
	NameBlock *block = Blocks;
	size_t len = strlen (text) + 1;

	// Get a block large enough for the name. Only the first block in the
	// list is ever considered for name storage.
	if (block == NULL || block->NextAlloc + len >= BLOCK_SIZE)
	{
		block = AddBlock (len);
	}

	// Copy the string into the block.
	textstore = (char *)block + block->NextAlloc;
	strcpy (textstore, text);
	block->NextAlloc += len;

	// Add an entry for the name to the NameArray
	if (NumNames >= MaxNames)
	{
		// If no names have been defined yet, make the first allocation
		// large enough to hold all the predefined names.
		MaxNames += MaxNames == 0 ? countof(PredefinedNames) + NAME_GROW_AMOUNT : NAME_GROW_AMOUNT;

		NameArray = (NameEntry *)M_Realloc (NameArray, MaxNames * sizeof(NameEntry));
	}

	NameArray[NumNames].Text = textstore;
	NameArray[NumNames].Hash = hash;
	NameArray[NumNames].NextHash = Buckets[bucket];
	Buckets[bucket] = NumNames;

	return NumNames++;
}

//==========================================================================
//
// FName :: NameManager :: AddBlock
//
// Creates a new NameBlock at least large enough to hold the required
// number of chars.
//
//==========================================================================

FName::NameManager::NameBlock *FName::NameManager::AddBlock (size_t len)
{
	NameBlock *block;

	len += sizeof(NameBlock);
	if (len < BLOCK_SIZE)
	{
		len = BLOCK_SIZE;
	}
	block = (NameBlock *)M_Malloc (len);
	block->NextAlloc = sizeof(NameBlock);
	block->NextBlock = Blocks;
	Blocks = block;
	return block;
}

//==========================================================================
//
// FName :: NameManager :: ~NameManager
//
// Release all the memory used for name bookkeeping.
//
//==========================================================================

FName::NameManager::~NameManager()
{
	NameBlock *block, *next;

	for (block = Blocks; block != NULL; block = next)
	{
		next = block->NextBlock;
		M_Free (block);
	}
	Blocks = NULL;

	if (NameArray != NULL)
	{
		M_Free (NameArray);
		NameArray = NULL;
	}
	NumNames = MaxNames = 0;
	memset (Buckets, -1, sizeof(Buckets));
}
