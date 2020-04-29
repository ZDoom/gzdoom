/*
**  Render memory allocation
**  Copyright (c) 2016-2020 Magnus Norddahl
**
**  This software is provided 'as-is', without any express or implied
**  warranty.  In no event will the authors be held liable for any damages
**  arising from the use of this software.
**
**  Permission is granted to anyone to use this software for any purpose,
**  including commercial applications, and to alter it and redistribute it
**  freely, subject to the following restrictions:
**
**  1. The origin of this software must not be misrepresented; you must not
**     claim that you wrote the original software. If you use this software
**     in a product, an acknowledgment in the product documentation would be
**     appreciated but is not required.
**  2. Altered source versions must be plainly marked as such, and must not be
**     misrepresented as being the original software.
**  3. This notice may not be removed or altered from any source distribution.
**
*/

#include <stdlib.h>
#include "templates.h"
#include "r_memory.h"
#include <stdlib.h>

void *RenderMemory::AllocBytes(int size)
{
	size = (size + 15) / 16 * 16; // 16-byte align
		
	if (UsedBlocks.empty() || UsedBlocks.back()->Position + size > BlockSize)
	{
		if (!FreeBlocks.empty())
		{
			auto block = std::move(FreeBlocks.back());
			block->Position = 0;
			FreeBlocks.pop_back();
			UsedBlocks.push_back(std::move(block));
		}
		else
		{
			UsedBlocks.push_back(std::unique_ptr<MemoryBlock>(new MemoryBlock()));
		}
	}
		
	auto &block = UsedBlocks.back();
	void *data = block->Data + block->Position;
	block->Position += size;

	return data;
}
	
void RenderMemory::Clear()
{
	while (!UsedBlocks.empty())
	{
		auto block = std::move(UsedBlocks.back());
		UsedBlocks.pop_back();
		FreeBlocks.push_back(std::move(block));
	}
}

static void* Aligned_Alloc(size_t alignment, size_t size)
{
	void* ptr;
#if defined (_MSC_VER) || defined (__MINGW32__)
	ptr = _aligned_malloc(size, alignment);
	if (!ptr)
		throw std::bad_alloc();
#else
	// posix_memalign required alignment to be a min of sizeof(void *)
	if (alignment < sizeof(void*))
		alignment = sizeof(void*);

	if (posix_memalign((void**)&ptr, alignment, size))
		throw std::bad_alloc();
#endif
	return ptr;
}

static void Aligned_Free(void* ptr)
{
	if (ptr)
	{
#if defined _MSC_VER
		_aligned_free(ptr);
#else
		free(ptr);
#endif
	}
}

RenderMemory::MemoryBlock::MemoryBlock() : Data(static_cast<uint8_t*>(Aligned_Alloc(16, BlockSize))), Position(0)
{
}

RenderMemory::MemoryBlock::~MemoryBlock()
{
	Aligned_Free(Data);
}
