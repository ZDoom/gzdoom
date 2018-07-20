// 
//---------------------------------------------------------------------------
//
// Copyright(C) 2004-2018 Christoph Oelckers
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
/*
** r_swscene.cpp
** render the software scene through the hardware rendering backend
**
*/

#include "hwrenderer/textures/hw_ihwtexture.h"
#include "hwrenderer/textures/hw_material.h"
#include "r_renderer.h"
#include "r_swscene.h"
#include "w_wad.h"
#include "d_player.h"
#include "textures/bitmap.h"
#include "swrenderer/scene/r_light.h"

// [RH] Base blending values (for e.g. underwater)
int BaseBlendR, BaseBlendG, BaseBlendB;
float BaseBlendA;



class FSWPaletteTexture : public FTexture
{
public:
	FSWPaletteTexture()
	{
		Width = 256;
		Height = 1;
		UseType = ETextureType::MiscPatch;
	}

	int CopyTrueColorPixels(FBitmap *bmp, int x, int y, int rotate, FCopyInfo *inf)
	{
		PalEntry *pe = (PalEntry*)bmp->GetPixels();
		for (int i = 0; i < 256; i++)
		{
			pe[i] = GPalette.BaseColors[i].d | 0xff000000;
		}
		return 0;
	}
};

class FSWSceneTexture : public FTexture
{
public:

	FSWSceneTexture(int w, int h, int bits)
	{
		Width = w;
		Height = h;
		WidthBits = bits;
		UseType = ETextureType::SWCanvas;
		bNoCompress = true;
		SystemTexture[0] = screen->CreateHardwareTexture(this);
	}

	// This is just a wrapper around the hardware texture and should never call the bitmap getters - if it does, something is wrong.
};

//==========================================================================
//
// SWSceneDrawer :: CreateResources
//
//==========================================================================

SWSceneDrawer::SWSceneDrawer()
{
	PaletteTexture.reset(new FSWPaletteTexture);
}

SWSceneDrawer::~SWSceneDrawer()
{
}

sector_t *SWSceneDrawer::RenderView(player_t *player)
{
	// Avoid using the pixel buffer from the last frame
	FBTextureIndex = (FBTextureIndex + 1) % 2;
	auto &fbtex = FBTexture[FBTextureIndex];

	if (fbtex == nullptr || fbtex->SystemTexture[0] == nullptr || 
		fbtex->GetWidth() != screen->GetWidth() || 
		fbtex->GetHeight() != screen->GetHeight() || 
		(V_IsTrueColor() ? 1:0) != fbtex->WidthBits)
	{
		// This manually constructs its own material here.
		fbtex.reset();
		fbtex.reset(new FSWSceneTexture(screen->GetWidth(), screen->GetHeight(), V_IsTrueColor()));
		fbtex->SystemTexture[0]->AllocateBuffer(screen->GetWidth(), screen->GetHeight(), V_IsTrueColor() ? 4 : 1);
		auto mat = FMaterial::ValidateTexture(fbtex.get(), false);
		mat->AddTextureLayer(PaletteTexture.get());

		Canvas.reset();
		Canvas.reset(new DSimpleCanvas(screen->GetWidth(), screen->GetHeight(), V_IsTrueColor()));
	}

	auto buf = fbtex->SystemTexture[0]->MapBuffer();
	if (!buf) I_FatalError("Unable to map buffer for software rendering");
	SWRenderer->RenderView(player, Canvas.get(), buf);
	fbtex->SystemTexture[0]->CreateTexture(nullptr, screen->GetWidth(), screen->GetHeight(), 0, false, 0, "swbuffer");

	auto map = swrenderer::CameraLight::Instance()->ShaderColormap();
	screen->DrawTexture(fbtex.get(), 0, 0, DTA_SpecialColormap, map, TAG_DONE);
	SWRenderer->DrawRemainingPlayerSprites();
	return r_viewpoint.sector;
}
