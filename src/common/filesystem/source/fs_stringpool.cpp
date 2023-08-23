/*
** stringpool.cpp
** allocate static strings from larger blocks
**
**---------------------------------------------------------------------------
** Copyright 2010 Randy Heit
** Copyright 2023 Christoph Oelckers
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
#include <stdint.h>
#include <string.h>
#include "fs_stringpool.h"

namespace FileSys {
	
struct StringPool::Block
{
	Block *NextBlock;
	void *Limit;			// End of this block
	void *Avail;			// Start of free space in this block
	void *alignme;			// align to 16 bytes.

	void *Alloc(size_t size);
};

//==========================================================================
//
// StringPool Destructor
//
//==========================================================================

StringPool::~StringPool()
{
	for (Block *next, *block = TopBlock; block != nullptr; block = next)
	{
		next = block->NextBlock;
		free(block);
	}
	TopBlock = nullptr;
}

//==========================================================================
//
// StringPool :: Alloc
//
//==========================================================================

void *StringPool::Block::Alloc(size_t size)
{
	if ((char *)Avail + size > Limit)
	{
		return nullptr;
	}
	void *res = Avail;
	Avail = ((char *)Avail + size);
	return res;
}

StringPool::Block *StringPool::AddBlock(size_t size)
{
	size += sizeof(Block);		// Account for header size

	// Allocate a new block
	if (size < BlockSize)
	{
		size = BlockSize;
	}
	auto mem = (Block *)malloc(size);
	if (mem == nullptr)
	{
		
	}
	mem->Limit = (uint8_t *)mem + size;
	mem->Avail = &mem[1];
	mem->NextBlock = TopBlock;
	TopBlock = mem;
	return mem;
}

void *StringPool::iAlloc(size_t size)
{
	Block *block;

	for (block = TopBlock; block != nullptr; block = block->NextBlock)
	{
		void *res = block->Alloc(size);
		if (res != nullptr)
		{
			return res;
		}
	}
	block = AddBlock(size);
	return block->Alloc(size);
}

const char* StringPool::Strdup(const char* str)
{
	char* p = (char*)iAlloc((strlen(str) + 8) & ~7 );
	strcpy(p, str);
	return p;
}

}
