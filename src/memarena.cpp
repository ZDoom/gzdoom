/*
** memarena.cpp
** Implements memory arenas.
**
**---------------------------------------------------------------------------
** Copyright 2010 Randy Heit
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
** A memory arena is used for efficient allocation of many small objects that
** will all be freed at once. Note that since individual destructors are not
** called, you must not use an arena to allocate any objects that use a
** destructor, either explicitly or implicitly (because they have members
** with destructors).
*/

#include "doomtype.h"
#include "memarena.h"
#include "c_dispatch.h"

struct FMemArena::Block
{
	Block *NextBlock;
	void *Limit;			// End of this block
	void *Avail;			// Start of free space in this block
	void *alignme;			// align to 16 bytes.

	void Reset();
	void *Alloc(size_t size);
};

//==========================================================================
//
// RoundPointer
//
// Rounds a pointer up to a pointer-sized boundary.
//
//==========================================================================

static inline void *RoundPointer(void *ptr)
{
	return (void *)(((size_t)ptr + sizeof(void*) - 1) & ~(sizeof(void*) - 1));
}

//==========================================================================
//
// FMemArena Constructor
//
//==========================================================================

FMemArena::FMemArena(size_t blocksize)
{
	TopBlock = NULL;
	FreeBlocks = NULL;
	BlockSize = blocksize;
}

//==========================================================================
//
// FMemArena Destructor
//
//==========================================================================

FMemArena::~FMemArena()
{
	FreeAllBlocks();
}

//==========================================================================
//
// FMemArena :: Alloc
//
//==========================================================================

void *FMemArena::iAlloc(size_t size)
{
	Block *block;

	for (block = TopBlock; block != NULL; block = block->NextBlock)
	{
		void *res = block->Alloc(size);
		if (res != NULL)
		{
			return res;
		}
	}
	block = AddBlock(size);
	return block->Alloc(size);
}

void *FMemArena::Alloc(size_t size)
{
	return iAlloc((size + 15) & ~15);
}

//==========================================================================
//
// FMemArena :: FreeAll
//
// Moves all blocks to the free list. No system-level deallocation occurs.
//
//==========================================================================

void FMemArena::FreeAll()
{
	for (Block *next, *block = TopBlock; block != NULL; block = next)
	{
		next = block->NextBlock;
		block->Reset();
		block->NextBlock = FreeBlocks;
		FreeBlocks = block;
	}
	TopBlock = NULL;
}

//==========================================================================
//
// FMemArena :: FreeAllBlocks
//
// Frees all blocks used by this arena.
//
//==========================================================================

void FMemArena::FreeAllBlocks()
{
	FreeBlockChain(TopBlock);
	FreeBlockChain(FreeBlocks);
}

//==========================================================================
//
// FMemArena :: DumpInfo
//
// Prints some info about this arena
//
//==========================================================================

void FMemArena::DumpInfo()
{
	size_t allocated = 0;
	size_t used = 0;
	for (auto block = TopBlock; block != NULL; block = block->NextBlock)
	{
		allocated += BlockSize;
		used += BlockSize - ((char*)block->Limit - (char*)block->Avail);
	}
	Printf("%zu bytes allocated, %zu bytes in use\n", allocated, used);
}

//==========================================================================
//
// FMemArena :: DumpInfo
//
// Dumps the arena to a file (for debugging)
//
//==========================================================================

void FMemArena::DumpData(FILE *f)
{
	for (auto block = TopBlock; block != NULL; block = block->NextBlock)
	{
		auto used = BlockSize - ((char*)block->Limit - (char*)block->Avail);
		fwrite(block, 1, used, f);
	}
}

//==========================================================================
//
// FMemArena :: FreeBlockChain
//
// Frees a chain of blocks.
//
//==========================================================================

void FMemArena::FreeBlockChain(Block *&top)
{
	for (Block *next, *block = top; block != NULL; block = next)
	{
		next = block->NextBlock;
		M_Free(block);
	}
	top = NULL;
}

//==========================================================================
//
// FMemArena :: AddBlock
//
// Allocates a block large enough to hold at least <size> bytes and adds it
// to the TopBlock chain.
//
//==========================================================================

FMemArena::Block *FMemArena::AddBlock(size_t size)
{
	Block *mem, **last;
	size += sizeof(Block);		// Account for header size

	// Search for a free block to use
	for (last = &FreeBlocks, mem = FreeBlocks; mem != NULL; last = &mem->NextBlock, mem = mem->NextBlock)
	{
		if ((uint8_t *)mem->Limit - (uint8_t *)mem >= (ptrdiff_t)size)
		{
			*last = mem->NextBlock;
			break;
		}
	}
	if (mem == NULL)
	{
		// Allocate a new block
		if (size < BlockSize)
		{
			size = BlockSize;
		}
		else
		{ // Stick some free space at the end so we can use this block for
		  // other things.
			size += BlockSize/2;
		}
		mem = (Block *)M_Malloc(size);
		mem->Limit = (uint8_t *)mem + size;
	}
	mem->Reset();
	mem->NextBlock = TopBlock;
	TopBlock = mem;
	return mem;
}

//==========================================================================
//
// FMemArena :: Block :: Reset
//
// Resets this block's Avail pointer.
//
//==========================================================================

void FMemArena::Block::Reset()
{
	Avail = RoundPointer(reinterpret_cast<char*>(this) + sizeof(*this));
}

//==========================================================================
//
// FMemArena :: Block :: Alloc
//
// Allocates memory from the block if it has space. Returns NULL if not.
//
//==========================================================================

void *FMemArena::Block::Alloc(size_t size)
{
	if ((char *)Avail + size > Limit)
	{
		return NULL;
	}
	void *res = Avail;
	Avail = RoundPointer((char *)Avail + size);
	return res;
}

//==========================================================================
//
// FSharedStringArena Constructor
//
//==========================================================================

FSharedStringArena::FSharedStringArena()
{
	memset(Buckets, 0, sizeof(Buckets));
}

//==========================================================================
//
// FSharedStringArena Destructor
//
//==========================================================================

FSharedStringArena::~FSharedStringArena()
{
	FreeAll();
	// FMemArena destructor will free the blocks.
}

//==========================================================================
//
// FSharedStringArena :: Alloc
//
// Allocates a new string and initializes it with the passed string. This
// version takes an FString as a parameter, so it won't need to allocate any
// memory for the string text if it already exists in the arena.
//
//==========================================================================

FString *FSharedStringArena::Alloc(const FString &source)
{
	unsigned int hash;
	Node *strnode;

	strnode = FindString(source, source.Len(), hash);
	if (strnode == NULL)
	{
		strnode = (Node *)iAlloc(sizeof(Node));
		::new(&strnode->String) FString(source);
		strnode->Hash = hash;
		hash %= countof(Buckets);
		strnode->Next = Buckets[hash];
		Buckets[hash] = strnode;
	}
	return &strnode->String;
}

//==========================================================================
//
// FSharedStringArena :: Alloc
//
//==========================================================================

FString *FSharedStringArena::Alloc(const char *source)
{
	return Alloc(source, strlen(source));
}

//==========================================================================
//
// FSharedStringArena :: Alloc
//
//==========================================================================

FString *FSharedStringArena::Alloc(const char *source, size_t strlen)
{
	unsigned int hash;
	Node *strnode;

	strnode = FindString(source, strlen, hash);
	if (strnode == NULL)
	{
		strnode = (Node *)iAlloc(sizeof(Node));
		::new(&strnode->String) FString(source, strlen);
		strnode->Hash = hash;
		hash %= countof(Buckets);
		strnode->Next = Buckets[hash];
		Buckets[hash] = strnode;
	}
	return &strnode->String;
}

//==========================================================================
//
// FSharedStringArena :: FindString
//
// Finds the string if it's already in the arena. Returns NULL if not.
//
//==========================================================================

FSharedStringArena::Node *FSharedStringArena::FindString(const char *str, size_t strlen, unsigned int &hash)
{
	hash = SuperFastHash(str, strlen);

	for (Node *node = Buckets[hash % countof(Buckets)]; node != NULL; node = node->Next)
	{
		if (node->Hash == hash && node->String.Len() == strlen && memcmp(&node->String[0], str, strlen) == 0)
		{
			return node;
		}
	}
	return NULL;
}

//==========================================================================
//
// FSharedStringArena :: FreeAll
//
// In addition to moving all used blocks onto the free list, all FStrings
// they contain will have their destructors called.
//
//==========================================================================

void FSharedStringArena::FreeAll()
{
	for (Block *next, *block = TopBlock; block != NULL; block = next)
	{
		next = block->NextBlock;
		void *limit = block->Avail;
		block->Reset();
		for (Node *string = (Node *)block->Avail; string < limit; ++string)
		{
			string->~Node();
		}
		block->NextBlock = FreeBlocks;
		FreeBlocks = block;
	}
	memset(Buckets, 0, sizeof(Buckets));
	TopBlock = NULL;
}
