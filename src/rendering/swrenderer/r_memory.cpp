//-----------------------------------------------------------------------------
//
// Copyright 1999-2016 Randy Heit
// Copyright 2016 Magnus Norddahl
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/
//
//-----------------------------------------------------------------------------

#include <stdlib.h>
#include "templates.h"
#include "doomdef.h"
#include "m_bbox.h"

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
