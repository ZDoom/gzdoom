/*
**  Voxel rendering
**  Copyright (c) 1998-2016 Randy Heit
**  Copyright (c) 2016 Magnus Norddahl
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

#pragma once

struct kvxslab_t;
struct FVoxelMipLevel;

namespace swrenderer
{
	struct vissprite_t;

	enum { DVF_OFFSCREEN = 1, DVF_SPANSONLY = 2, DVF_MIRRORED = 4 };

	void R_DrawVisVoxel(vissprite_t *sprite, int minZ, int maxZ, short *cliptop, short *clipbottom);
	void R_FillBox(DVector3 origin, double extentX, double extentY, int color, short *cliptop, short *clipbottom, bool viewspace, bool pixelstretch);
	kvxslab_t *R_GetSlabStart(const FVoxelMipLevel &mip, int x, int y);
	kvxslab_t *R_GetSlabEnd(const FVoxelMipLevel &mip, int x, int y);
	kvxslab_t *R_NextSlab(kvxslab_t *slab);

	void R_CheckOffscreenBuffer(int width, int height, bool spansonly);
	void R_DeinitRenderVoxel();

	// [RH] A c-buffer. Used for keeping track of offscreen voxel spans.
	struct FCoverageBuffer
	{
		struct Span
		{
			Span *NextSpan;
			short Start, Stop;
		};

		FCoverageBuffer(int size);
		~FCoverageBuffer();

		void Clear();
		void InsertSpan(int listnum, int start, int stop);
		Span *AllocSpan();

		FMemArena SpanArena;
		Span **Spans;	// [0..NumLists-1] span lists
		Span *FreeSpans;
		unsigned int NumLists;
	};
}
