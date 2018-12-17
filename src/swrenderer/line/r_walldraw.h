//-----------------------------------------------------------------------------
//
// Copyright 1993-1996 id Software
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
//

#pragma once

#include "swrenderer/viewport/r_walldrawer.h"
#include "r_line.h"

class FTexture;
struct FLightNode;
struct seg_t;
struct FLightNode;
struct FDynamicColormap;

namespace swrenderer
{
	class RenderThread;
	struct DrawSegment;
	struct FWallCoords;
	class ProjectedWallLine;
	class ProjectedWallTexcoords;
	struct WallSampler;

	class RenderWallPart
	{
	public:
		RenderWallPart(RenderThread *thread);

		void Render(
			sector_t *frontsector,
			seg_t *curline,
			const FWallCoords &WallC,
			FSoftwareTexture *rw_pic,
			int x1,
			int x2,
			const short *walltop,
			const short *wallbottom,
			double texturemid,
			float *swall,
			fixed_t *lwall,
			double yscale,
			double top,
			double bottom,
			bool mask,
			bool additive,
			fixed_t alpha,
			int lightlevel,
			fixed_t xoffset,
			float light,
			float lightstep,
			FLightNode *light_list,
			bool foggy,
			FDynamicColormap *basecolormap);

		RenderThread *Thread = nullptr;

	private:
		void ProcessWallNP2(const short *uwal, const short *dwal, double texturemid, float *swal, fixed_t *lwal, double top, double bot);
		void ProcessWall(const short *uwal, const short *dwal, double texturemid, float *swal, fixed_t *lwal);
		void ProcessStripedWall(const short *uwal, const short *dwal, double texturemid, float *swal, fixed_t *lwal);
		void ProcessNormalWall(const short *uwal, const short *dwal, double texturemid, float *swal, fixed_t *lwal);
		void ProcessWallWorker(const short *uwal, const short *dwal, double texturemid, float *swal, fixed_t *lwal);
		void Draw1Column(int x, int y1, int y2, WallSampler &sampler);

		int x1 = 0;
		int x2 = 0;
		FSoftwareTexture *rw_pic = nullptr;
		sector_t *frontsector = nullptr;
		seg_t *curline = nullptr;
		FWallCoords WallC;

		double yrepeat = 0.0;
		int lightlevel = 0;
		fixed_t xoffset = 0;
		float light = 0.0f;
		float lightstep = 0.0f;
		bool foggy = false;
		FDynamicColormap *basecolormap = nullptr;
		FLightNode *light_list = nullptr;
		bool mask = false;
		bool additive = false;
		fixed_t alpha = 0;

		WallDrawerArgs drawerargs;
	};

	struct WallSampler
	{
		WallSampler() { }
		WallSampler(RenderViewport *viewport, int y1, double texturemid, float swal, double yrepeat, fixed_t xoffset, double xmagnitude, FSoftwareTexture *texture);

		uint32_t uv_pos;
		uint32_t uv_step;
		uint32_t uv_max;

		const uint8_t *source;
		const uint8_t *source2;
		uint32_t texturefracx;
		uint32_t height;
	};
}
