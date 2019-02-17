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

#pragma once

#include "swrenderer/line/r_line.h"
#include "swrenderer/scene/r_light.h"
#include "swrenderer/scene/r_opaque_pass.h"
#include "swrenderer/things/r_visiblespritelist.h"

#define MINZ double((2048*4) / double(1 << 20))

namespace swrenderer
{
	class RenderThread;

	class VisibleSprite
	{
	public:
		VisibleSprite() { RenderStyle = STYLE_Normal; }
		virtual ~VisibleSprite() { }
		
		void Render(RenderThread *thread, Fake3DTranslucent clip3DFloor);

		bool IsCurrentPortalUniq(int portalUniq) const { return CurrentPortalUniq == portalUniq; }
		const FVector3 &WorldPos() const { return gpos; }

		float SortDist() const { return idepth; }

		int SubsectorDepth;

	protected:
		virtual bool IsParticle() const { return false; }
		virtual bool IsVoxel() const { return false; }
		virtual bool IsWallSprite() const { return false; }
		virtual bool IsModel() const { return false; }

		virtual void Render(RenderThread *thread, short *cliptop, short *clipbottom, int minZ, int maxZ, Fake3DTranslucent clip3DFloor) = 0;

		FSoftwareTexture *pic = nullptr;

		short x1 = 0, x2 = 0;
		float gzb = 0.0f, gzt = 0.0f; // global bottom / top for silhouette clipping

		double floorclip = 0.0;

		double texturemid = 0.0; // floorclip
		float yscale = 0.0f; // voxel and floorclip

		sector_t *heightsec = nullptr; // height sector for underwater/fake ceiling
		WaterFakeSide FakeFlatStat = WaterFakeSide::Center; // which side of fake/floor ceiling sprite is on

		F3DFloor *fakefloor = nullptr; // 3d floor clipping
		F3DFloor *fakeceiling = nullptr;

		FVector3 gpos = { 0.0f, 0.0f, 0.0f }; // origin in world coordinates
		sector_t *sector = nullptr; // sector this sprite is in
		FSection *section;

		ColormapLight Light;
		float Alpha = 0.0f;
		FRenderStyle RenderStyle;
		bool foggy = false;
		short renderflags = 0;

		float depth = 0.0f; // Sort (draw segments), also light

		float deltax = 0.0f, deltay = 0.0f; // Sort (2d/voxel version)
		float idepth = 0.0f; // Sort (non-voxel version)

		int CurrentPortalUniq = 0; // to identify the portal that this thing is in. used for clipping.
	};
}
