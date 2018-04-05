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
** gl_swscene.cpp
** common overlay code for software and hardware renderer
**
*/

#include "gl/system/gl_system.h"
#include "gl/system/gl_interface.h"
#include "gl/system/gl_debug.h"
#include "gl/data/gl_vertexbuffer.h"
#include "gl/shaders/gl_shader.h"
#include "gl/renderer/gl_renderstate.h"
#include "v_palette.h"
#include "v_video.h"
#include "gl_swscene.h"
#include "w_wad.h"
#include "d_player.h"
#include "textures/bitmap.h"
#include "swrenderer/scene/r_light.h"

// [RH] Base blending values (for e.g. underwater)
int BaseBlendR, BaseBlendG, BaseBlendB;
float BaseBlendA;

EXTERN_CVAR(Bool, swtruecolor)


// using FDummyTexture as base because that implements the required software renderer functions.

class FSWPaletteTexture : public FDummyTexture
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
		memcpy(bmp->GetPixels(), GPalette.BaseColors, 1024);
		return 0;
	}
};

class FSWSceneTexture : public FDummyTexture
{
public:
	FHardwareTexture *hwtex;

	FSWSceneTexture(int w, int h, int bits)
	{
		Width = w;
		Height = h;
		WidthBits = bits;
		UseType = ETextureType::SWCanvas;

		hwtex = new FHardwareTexture(true);
		new FGLTexture(this, hwtex);
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
	PaletteTexture = new FSWPaletteTexture;
}

SWSceneDrawer::~SWSceneDrawer()
{
	if (PaletteTexture != nullptr) delete PaletteTexture;
	if (FBTexture != nullptr) delete FBTexture;
}

void SWSceneDrawer::RenderView(player_t *player)
{
	DCanvas buffer(screen->GetWidth(), screen->GetHeight(), swtruecolor);
	if (FBTexture == nullptr || FBTexture->hwtex == nullptr || FBTexture->GetWidth() != screen->GetWidth() || FBTexture->GetHeight() != screen->GetHeight() || (swtruecolor?4:1) != FBTexture->WidthBits)
	{
		// This manually constructs its own material here.
		if (FBTexture != nullptr) delete FBTexture;
		FBTexture = new FSWSceneTexture(screen->GetWidth(), screen->GetHeight(), swtruecolor);
		FBTexture->hwtex->AllocateBuffer(screen->GetWidth(), screen->GetHeight(), swtruecolor ? 4 : 1);
		auto mat = FMaterial::ValidateTexture(FBTexture, false);
		mat->AddTextureLayer(PaletteTexture);
	}
	auto buf = FBTexture->hwtex->MapBuffer();
	if (!buf) I_FatalError("Unable to map buffer for software rendering");
	buffer.SetBuffer(screen->GetWidth(), screen->GetHeight(), screen->GetWidth() *  (swtruecolor ? 4 : 1), buf);
	SWRenderer->RenderView(player, &buffer);
	FBTexture->hwtex->CreateTexture(nullptr, screen->GetWidth(), screen->GetHeight(), 0, false, 0, "swbuffer");

	auto map = swrenderer::CameraLight::Instance()->ShaderColormap();
	screen->Begin2D(false);
	screen->DrawTexture(FBTexture, 0, 0, DTA_SpecialColormap, map, TAG_DONE);
	SWRenderer->DrawRemainingPlayerSprites();
	GLRenderer->DrawBlend(r_viewpoint.sector, !!map, swtruecolor);
}
