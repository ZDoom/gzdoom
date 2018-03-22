// 
//---------------------------------------------------------------------------
//
// Voxel rendering
// Copyright(c) 1998 - 2016 Randy Heit
// All rights reserved.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/
//
//--------------------------------------------------------------------------
//

#pragma once

#include "r_visiblesprite.h"

struct kvxslab_t;
struct FVoxelMipLevel;
struct FVoxel;

namespace swrenderer
{
	class SpriteDrawerArgs;

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
		static void Project(RenderThread *thread, AActor *thing, DVector3 pos, FVoxelDef *voxel, const DVector2 &spriteScale, int renderflags, WaterFakeSide fakeside, F3DFloor *fakefloor, F3DFloor *fakeceiling, sector_t *current_sector, int spriteshade, bool foggy, FDynamicColormap *basecolormap);

		static void Deinit();

	protected:
		bool IsVoxel() const override { return true; }
		void Render(RenderThread *thread, short *cliptop, short *clipbottom, int minZ, int maxZ, Fake3DTranslucent clip3DFloor) override;

	private:
		struct posang
		{
			FVector3 vpos = { 0.0f, 0.0f, 0.0f }; // view origin
			FAngle vang = { 0.0f }; // view angle
		};

		struct VoxelBlockEntry
		{
			VoxelBlock *block;
			VoxelBlockEntry *next;
		};

		posang pa;
		DAngle Angle = { 0.0 };
		fixed_t xscale = 0;
		FVoxel *voxel = nullptr;
		bool bInMirror = false;

		uint32_t Translation = 0;
		uint32_t FillColor = 0;

		enum { DVF_OFFSCREEN = 1, DVF_SPANSONLY = 2, DVF_MIRRORED = 4, DVF_FIND_X1X2 = 8 };

		static kvxslab_t *GetSlabStart(const FVoxelMipLevel &mip, int x, int y);
		static kvxslab_t *GetSlabEnd(const FVoxelMipLevel &mip, int x, int y);
		static kvxslab_t *NextSlab(kvxslab_t *slab);

		static void CheckOffscreenBuffer(int width, int height, bool spansonly);

		static FCoverageBuffer *OffscreenCoverageBuffer;
		static int OffscreenBufferWidth;
		static int OffscreenBufferHeight;
		static uint8_t *OffscreenColorBuffer;

		void DrawVoxel(
			RenderThread *thread, SpriteDrawerArgs &drawerargs,
			const FVector3 &globalpos, FAngle viewangle, const FVector3 &dasprpos, DAngle dasprang, fixed_t daxscale, fixed_t dayscale,
			FVoxel *voxobj, short *daumost, short *dadmost, int minslabz, int maxslabz, int flags);

		int sgn(int v)
		{
			return v < 0 ? -1 : v > 0 ? 1 : 0;
		}
	};
}
