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

#include <stdlib.h>
#include "templates.h"
#include "doomdef.h"
#include "m_bbox.h"
#include "i_system.h"
#include "p_lnspec.h"
#include "p_setup.h"
#include "swrenderer/drawers/r_draw.h"
#include "swrenderer/plane/r_visibleplane.h"
#include "a_sharedglobal.h"
#include "g_level.h"
#include "p_effect.h"
#include "doomstat.h"
#include "r_state.h"
#include "v_palette.h"
#include "r_sky.h"
#include "po_man.h"
#include "r_data/colormaps.h"
#include "r_memory.h"

namespace swrenderer
{
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
				UsedBlocks.push_back(std::make_unique<MemoryBlock>());
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
}
