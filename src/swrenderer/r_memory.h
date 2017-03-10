//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// The source is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
// for more details.
//

#pragma once

#include <memory>

namespace swrenderer
{
	// Memory needed for the duration of a frame rendering
	class RenderMemory
	{
	public:
		void Clear();
		
		template<typename T>
		T *AllocMemory(int size = 1)
		{
			return (T*)AllocBytes(sizeof(T) * size);
		}
		
		template<typename T, typename... Types>
		T *NewObject(Types &&... args)
		{
			void *ptr = AllocBytes(sizeof(T));
			return new (ptr)T(std::forward<Types>(args)...);
		}
		
	private:
		void *AllocBytes(int size);
		
		enum { BlockSize = 1024 * 1024 };
		
		struct MemoryBlock
		{
			MemoryBlock() : Data(new uint8_t[BlockSize]), Position(0) { }
			~MemoryBlock() { delete[] Data; }
			
			MemoryBlock(const MemoryBlock &) = delete;
			MemoryBlock &operator=(const MemoryBlock &) = delete;
			
			uint8_t *Data;
			uint32_t Position;
		};
		std::vector<std::unique_ptr<MemoryBlock>> UsedBlocks;
		std::vector<std::unique_ptr<MemoryBlock>> FreeBlocks;
	};
}
