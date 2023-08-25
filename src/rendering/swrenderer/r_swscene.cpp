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

#include "hw_ihwtexture.h"
#include "hw_material.h"
#include "swrenderer/r_renderer.h"
#include "r_swscene.h"
#include "filesystem.h"
#include "d_player.h"
#include "bitmap.h"
#include "swrenderer/scene/r_light.h"
#include "image.h"
#include "engineerrors.h"
#include "texturemanager.h"
#include "d_main.h"
#include "v_draw.h"

class FSWPaletteTexture : public FImageSource
{
public:
	FSWPaletteTexture()
	{
		Width = 256;
		Height = 1;
	}

	int CopyPixels(FBitmap *bmp, int conversion, int frame = 0) override
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
		// We need to wrap this in a game texture object to have it managed by the texture manager, even though it will never be used as a material.
		auto tex = MakeGameTexture(new FImageTexture(new FSWPaletteTexture), "@@palette@@", ETextureType::Special);
		texid = TexMan.AddGameTexture(tex);
	}
	PaletteTexture = TexMan.GetGameTexture(texid)->GetTexture();
}

SWSceneDrawer::~SWSceneDrawer()
{
}

sector_t *SWSceneDrawer::RenderView(player_t *player)
{
	if (!V_IsTrueColor() || !screen->IsPoly())
	{
		// Avoid using the pixel buffer from the last frame
		FBTextureIndex = (FBTextureIndex + 1) % 2;
		auto &fbtex = FBTexture[FBTextureIndex];

		auto GetSystemTexture = [&]() { return fbtex->GetTexture()->GetHardwareTexture(0, 0); };

		if (fbtex == nullptr || GetSystemTexture() == nullptr ||
			fbtex->GetTexelWidth() != screen->GetWidth() || 
			fbtex->GetTexelHeight() != screen->GetHeight() || 
			(V_IsTrueColor() ? 1:0) != static_cast<FWrapperTexture*>(fbtex->GetTexture())->GetColorFormat())
		{
			// This manually constructs its own material here.
			fbtex.reset();
			fbtex.reset(MakeGameTexture(new FWrapperTexture(screen->GetWidth(), screen->GetHeight(), V_IsTrueColor()), nullptr, ETextureType::SWCanvas));
			GetSystemTexture()->AllocateBuffer(screen->GetWidth(), screen->GetHeight(), V_IsTrueColor() ? 4 : 1);
			auto mat = FMaterial::ValidateTexture(fbtex.get(), false);
			mat->AddTextureLayer(PaletteTexture, false, MaterialLayerSampling::Default);

			Canvas.reset();
			Canvas.reset(new DCanvas(screen->GetWidth(), screen->GetHeight(), V_IsTrueColor()));
		}

		IHardwareTexture *systemTexture = GetSystemTexture();
		auto buf = systemTexture->MapBuffer();
		if (!buf) I_FatalError("Unable to map buffer for software rendering");
		SWRenderer->RenderView(player, Canvas.get(), buf, systemTexture->GetBufferPitch());
		systemTexture->CreateTexture(nullptr, screen->GetWidth(), screen->GetHeight(), 0, false, "swbuffer");

		auto map = swrenderer::CameraLight::Instance()->ShaderColormap();
		DrawTexture(twod, fbtex.get(), 0, 0, DTA_SpecialColormap, map, TAG_DONE);
		screen->Draw2D();
		twod->Clear();
		screen->PostProcessScene(true, CM_DEFAULT, 1.f, [&]() {
			SWRenderer->DrawRemainingPlayerSprites();
			screen->Draw2D();
			twod->Clear();
		});
	}
	else
	{
		// With softpoly truecolor we render directly to the target framebuffer

		DCanvas *canvas = screen->GetCanvas();
		SWRenderer->RenderView(player, canvas, canvas->GetPixels(), canvas->GetPitch());

		int cm = CM_DEFAULT;
		auto map = swrenderer::CameraLight::Instance()->ShaderColormap();
		if (map) cm = (int)(ptrdiff_t)(map - SpecialColormaps.Data()) + CM_FIRSTSPECIALCOLORMAP;
		screen->PostProcessScene(true, cm, 1.f, [&]() { });

		SWRenderer->DrawRemainingPlayerSprites();
		screen->Draw2D();
		twod->Clear();
	}

	return r_viewpoint.sector;
}
