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

#include "r_visiblesprite.h"
#include "r_data/colormaps.h"

class DPSprite;

namespace swrenderer
{
	class NoAccelPlayerSprite
	{
	public:
		short x1 = 0;
		short x2 = 0;

		double texturemid = 0.0;

		fixed_t xscale = 0;
		float yscale = 0.0f;

		FTexture *pic = nullptr;

		fixed_t xiscale = 0;
		fixed_t startfrac = 0;

		float Alpha = 0.0f;
		FRenderStyle RenderStyle;
		uint32_t Translation = 0;
		uint32_t FillColor = 0;

		ColormapLight Light;

		short renderflags = 0;

		void Render(RenderThread *thread);
	};

	class HWAccelPlayerSprite
	{
	public:
		FTexture *pic = nullptr;
		double texturemid = 0.0;
		float yscale = 0.0f;
		fixed_t xscale = 0;

		float Alpha = 0.0f;
		FRenderStyle RenderStyle;
		uint32_t Translation = 0;
		uint32_t FillColor = 0;

		FDynamicColormap *basecolormap = nullptr;
		int x1 = 0;

		bool flip = false;
		FSpecialColormap *special = nullptr;
		PalEntry overlay = 0;
		FColormapStyle colormapstyle;
		bool usecolormapstyle = false;
	};

	class RenderPlayerSprites
	{
	public:
		RenderPlayerSprites(RenderThread *thread);

		void Render();
		void RenderRemaining();

		RenderThread *Thread = nullptr;

	private:
		void RenderSprite(DPSprite *pspr, AActor *owner, float bobx, float boby, double wx, double wy, double ticfrac, int spriteshade, FDynamicColormap *basecolormap, bool foggy);

		enum { BASEXCENTER = 160 };
		enum { BASEYCENTER = 100 };

		TArray<HWAccelPlayerSprite> AcceleratedSprites;
		sector_t tempsec;
		bool renderHUDModel = false;
	};
}
