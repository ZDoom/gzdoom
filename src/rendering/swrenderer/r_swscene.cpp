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
#include "swrenderer/r_renderer.h"
#include "r_swscene.h"
#include "w_wad.h"
#include "d_player.h"
#include "textures/bitmap.h"
#include "swrenderer/scene/r_light.h"
#include "image.h"
#include "doomerrors.h"

// [RH] Base blending values (for e.g. underwater)
int BaseBlendR, BaseBlendG, BaseBlendB;
float BaseBlendA;



class FSWPaletteTexture : public FImageSource
{
public:
	FSWPaletteTexture()
	{
		Width = 256;
		Height = 1;
	}

	int CopyPixels(FBitmap *bmp, int conversion) override
	{
		PalEntry *pe = (PalEntry*)bmp->GetPixels();
		for (int i = 0; i < 256; i++)
		{
			pe[i] = GPalette.BaseColors[i].d | 0xff000000;
		}
		return 0;
	}
};


//==========================================================================
//
// SWSceneDrawer :: CreateResources
//
//==========================================================================

SWSceneDrawer::SWSceneDrawer()
{
	auto texid = TexMan.CheckForTexture("@@palette@@", ETextureType::Any);
	if (!texid.Exists())
	{
		auto tex = new FImageTexture(new FSWPaletteTexture, "@@palette@@");
		texid = TexMan.AddTexture(tex);
	}
	PaletteTexture = TexMan.GetTexture(texid);
}

SWSceneDrawer::~SWSceneDrawer()
{
}

sector_t *SWSceneDrawer::RenderView(player_t *player)
{
	// Avoid using the pixel buffer from the last frame
	FBTextureIndex = (FBTextureIndex + 1) % 2;
	auto &fbtex = FBTexture[FBTextureIndex];

	if (fbtex == nullptr || fbtex->GetSystemTexture() == nullptr ||
		fbtex->GetDisplayWidth() != screen->GetWidth() || 
		fbtex->GetDisplayHeight() != screen->GetHeight() || 
		(V_IsTrueColor() ? 1:0) != fbtex->GetColorFormat())
	{
		// This manually constructs its own material here.
		fbtex.reset();
		fbtex.reset(new FWrapperTexture(screen->GetWidth(), screen->GetHeight(), V_IsTrueColor()));
		fbtex->GetSystemTexture()->AllocateBuffer(screen->GetWidth(), screen->GetHeight(), V_IsTrueColor() ? 4 : 1);
		auto mat = FMaterial::ValidateTexture(fbtex.get(), false);
		mat->AddTextureLayer(PaletteTexture);

		Canvas.reset();
		Canvas.reset(new DCanvas(screen->GetWidth(), screen->GetHeight(), V_IsTrueColor()));
	}

	IHardwareTexture *systemTexture = fbtex->GetSystemTexture();
	auto buf = systemTexture->MapBuffer();
	if (!buf) I_FatalError("Unable to map buffer for software rendering");
	SWRenderer->RenderView(player, Canvas.get(), buf, systemTexture->GetBufferPitch());
	systemTexture->CreateTexture(nullptr, screen->GetWidth(), screen->GetHeight(), 0, false, 0, "swbuffer");

	auto map = swrenderer::CameraLight::Instance()->ShaderColormap();
	screen->DrawTexture(fbtex.get(), 0, 0, DTA_SpecialColormap, map, TAG_DONE);
	screen->Draw2D();
	screen->Clear2D();
	screen->PostProcessScene(CM_DEFAULT, [&]() {
		SWRenderer->DrawRemainingPlayerSprites();
		screen->Draw2D();
		screen->Clear2D();
	});

	return r_viewpoint.sector;
}
