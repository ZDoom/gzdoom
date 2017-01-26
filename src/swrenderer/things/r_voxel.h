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

#include "r_visiblesprite.h"

struct kvxslab_t;
struct FVoxelMipLevel;
struct FVoxel;

namespace swrenderer
{
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

	class RenderVoxel : public VisibleSprite
	{
	public:
		static void Project(AActor *thing, DVector3 pos, FVoxelDef *voxel, const DVector2 &spriteScale, int renderflags, WaterFakeSide fakeside, F3DFloor *fakefloor, F3DFloor *fakeceiling, sector_t *current_sector, int spriteshade, bool foggy, FDynamicColormap *basecolormap);

		static void Deinit();

	protected:
		bool IsVoxel() const override { return true; }
		void Render(short *cliptop, short *clipbottom, int minZ, int maxZ) override;

	private:
		struct posang
		{
			FVector3 vpos = { 0.0f, 0.0f, 0.0f }; // view origin
			FAngle vang = { 0.0f }; // view angle
		};

		posang pa;
		DAngle Angle = { 0.0 };
		fixed_t xscale = 0;
		FVoxel *voxel = nullptr;

		uint32_t Translation = 0;
		uint32_t FillColor = 0;

		enum { DVF_OFFSCREEN = 1, DVF_SPANSONLY = 2, DVF_MIRRORED = 4 };

		static void FillBox(DVector3 origin, double extentX, double extentY, int color, short *cliptop, short *clipbottom, bool viewspace, bool pixelstretch);

		static kvxslab_t *GetSlabStart(const FVoxelMipLevel &mip, int x, int y);
		static kvxslab_t *GetSlabEnd(const FVoxelMipLevel &mip, int x, int y);
		static kvxslab_t *NextSlab(kvxslab_t *slab);

		static void CheckOffscreenBuffer(int width, int height, bool spansonly);

		static FCoverageBuffer *OffscreenCoverageBuffer;
		static int OffscreenBufferWidth;
		static int OffscreenBufferHeight;
		static uint8_t *OffscreenColorBuffer;
	};
}
