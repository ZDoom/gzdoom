/*
** r_swrender.cpp
** Software renderer interface
**
**---------------------------------------------------------------------------
** Copyright 2011 Christoph Oelckers
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
*/

#include "swrenderer/scene/r_scene.h"
#include "swrenderer/viewport/r_viewport.h"
#include "swrenderer/things/r_playersprite.h"
#include "swrenderer/scene/r_scene.h"
#include "swrenderer/scene/r_light.h"
#include "swrenderer/r_swcolormaps.h"
#include "v_palette.h"
#include "v_video.h"
#include "m_png.h"
#include "r_swrenderer.h"
#include "scene/r_opaque_pass.h"
#include "scene/r_3dfloors.h"
#include "scene/r_portal.h"
#include "textures/textures.h"
#include "r_data/voxels.h"
#include "drawers/r_draw_rgba.h"
#include "polyrenderer/poly_renderer.h"
#include "p_setup.h"
#include "g_levellocals.h"
#include "image.h"
#include "imagehelpers.h"

// [BB] Use ZDoom's freelook limit for the sotfware renderer.
// Note: ZDoom's limit is chosen such that the sky is rendered properly.
CUSTOM_CVAR (Bool, cl_oldfreelooklimit, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOINITCALL)
{
	if (usergame) // [SP] Update pitch limits to the netgame/gamesim.
		players[consoleplayer].SendPitchLimits();
}

EXTERN_CVAR(Float, maxviewpitch)	// [SP] CVAR from OpenGL Renderer
EXTERN_CVAR(Bool, r_drawvoxels)

using namespace swrenderer;

FSoftwareRenderer::FSoftwareRenderer()
{
}

void FSoftwareRenderer::Init()
{
	R_InitShadeMaps();
	InitSWColorMaps();
}

FRenderer *CreateSWRenderer()
{
	return new FSoftwareRenderer;
}

void FSoftwareRenderer::PreparePrecache(FTexture *ttex, int cache)
{
	bool isbgra = V_IsTrueColor();

	if (ttex != nullptr && ttex->isValid() && !ttex->isCanvas())
	{
		FSoftwareTexture *tex = ttex->GetSoftwareTexture();

		if (tex->CheckPixels())
		{
			if (cache == 0) tex->Unload();
		}
		else if (cache != 0)
		{
			FImageSource::RegisterForPrecache(ttex->GetImage());
		}
	}
}

void FSoftwareRenderer::PrecacheTexture(FTexture *ttex, int cache)
{
	bool isbgra = V_IsTrueColor();

	if (ttex != nullptr && ttex->isValid() && !ttex->isCanvas())
	{
		FSoftwareTexture *tex = ttex->GetSoftwareTexture();
		if (cache & FTextureManager::HIT_Columnmode)
		{
			const FSoftwareTextureSpan *spanp;
			if (isbgra)
				tex->GetColumnBgra(0, &spanp);
			else
				tex->GetColumn(DefaultRenderStyle(), 0, &spanp);
		}
		else if (cache != 0)
		{
			if (isbgra)
				tex->GetPixelsBgra();
			else
				tex->GetPixels (DefaultRenderStyle());
		}
	}
}

void FSoftwareRenderer::Precache(uint8_t *texhitlist, TMap<PClassActor*, bool> &actorhitlist)
{
	uint8_t *spritelist = new uint8_t[sprites.Size()];
	TMap<PClassActor*, bool>::Iterator it(actorhitlist);
	TMap<PClassActor*, bool>::Pair *pair;

	memset(spritelist, 0, sprites.Size());

	while (it.NextPair(pair))
	{
		PClassActor *cls = pair->Key;

		for (unsigned i = 0; i < cls->GetStateCount(); i++)
		{
			spritelist[cls->GetStates()[i].sprite] = true;
		}
	}

	// Precache textures (and sprites).

	for (int i = (int)(sprites.Size() - 1); i >= 0; i--)
	{
		if (spritelist[i])
		{
			int j, k;
			for (j = 0; j < sprites[i].numframes; j++)
			{
				const spriteframe_t *frame = &SpriteFrames[sprites[i].spriteframes + j];

				for (k = 0; k < 16; k++)
				{
					FTextureID pic = frame->Texture[k];
					if (pic.isValid())
					{
						texhitlist[pic.GetIndex()] = FTextureManager::HIT_Sprite;
					}
				}
			}
		}
	}
	delete[] spritelist;

	int cnt = TexMan.NumTextures();

	FImageSource::BeginPrecaching();
	for (int i = cnt - 1; i >= 0; i--)
	{
		PreparePrecache(TexMan.ByIndex(i), texhitlist[i]);
	}

	for (int i = cnt - 1; i >= 0; i--)
	{
		PrecacheTexture(TexMan.ByIndex(i), texhitlist[i]);
	}
	FImageSource::EndPrecaching();
}

void FSoftwareRenderer::RenderView(player_t *player, DCanvas *target, void *videobuffer)
{
	if (V_IsPolyRenderer())
	{
		PolyRenderer::Instance()->Viewpoint = r_viewpoint;
		PolyRenderer::Instance()->Viewwindow = r_viewwindow;
		PolyRenderer::Instance()->RenderView(player, target, videobuffer);
		r_viewpoint = PolyRenderer::Instance()->Viewpoint;
		r_viewwindow = PolyRenderer::Instance()->Viewwindow;
	}
	else
	{
		mScene.MainThread()->Viewport->viewpoint = r_viewpoint;
		mScene.MainThread()->Viewport->viewwindow = r_viewwindow;
		mScene.RenderView(player, target, videobuffer);
		r_viewpoint = mScene.MainThread()->Viewport->viewpoint;
		r_viewwindow = mScene.MainThread()->Viewport->viewwindow;
	}

	r_viewpoint.ViewLevel->canvasTextureInfo.UpdateAll([&](AActor *camera, FCanvasTexture *camtex, double fov)
	{
		RenderTextureView(camtex, camera, fov);
	});
}

void DoWriteSavePic(FileWriter *file, ESSType ssformat, uint8_t *scr, int width, int height, sector_t *viewsector, bool upsidedown);

void FSoftwareRenderer::WriteSavePic (player_t *player, FileWriter *file, int width, int height)
{
	DCanvas pic(width, height, false);

	// Take a snapshot of the player's view
	if (V_IsPolyRenderer())
	{
		PolyRenderer::Instance()->Viewpoint = r_viewpoint;
		PolyRenderer::Instance()->Viewwindow = r_viewwindow;
		PolyRenderer::Instance()->RenderViewToCanvas(player->mo, &pic, 0, 0, width, height, true);
		r_viewpoint = PolyRenderer::Instance()->Viewpoint;
		r_viewwindow = PolyRenderer::Instance()->Viewwindow;
	}
	else
	{
		mScene.MainThread()->Viewport->viewpoint = r_viewpoint;
		mScene.MainThread()->Viewport->viewwindow = r_viewwindow;
		mScene.RenderViewToCanvas(player->mo, &pic, 0, 0, width, height);
		r_viewpoint = mScene.MainThread()->Viewport->viewpoint;
		r_viewwindow = mScene.MainThread()->Viewport->viewwindow;
	}
	DoWriteSavePic(file, SS_PAL, pic.GetPixels(), width, height, r_viewpoint.sector, false);
}

void FSoftwareRenderer::DrawRemainingPlayerSprites()
{
	if (!V_IsPolyRenderer())
	{
		mScene.MainThread()->Viewport->viewpoint = r_viewpoint;
		mScene.MainThread()->Viewport->viewwindow = r_viewwindow;
		mScene.MainThread()->PlayerSprites->RenderRemaining();
		r_viewpoint = mScene.MainThread()->Viewport->viewpoint;
		r_viewwindow = mScene.MainThread()->Viewport->viewwindow;
	}
	else
	{
		PolyRenderer::Instance()->Viewpoint = r_viewpoint;
		PolyRenderer::Instance()->Viewwindow = r_viewwindow;
		PolyRenderer::Instance()->RenderRemainingPlayerSprites();
		r_viewpoint = PolyRenderer::Instance()->Viewpoint;
		r_viewwindow = PolyRenderer::Instance()->Viewwindow;
	}
}

void FSoftwareRenderer::SetClearColor(int color)
{
	mScene.SetClearColor(color);
}

void FSoftwareRenderer::RenderTextureView (FCanvasTexture *camtex, AActor *viewpoint, double fov)
{
	auto renderTarget = V_IsPolyRenderer() ? PolyRenderer::Instance()->RenderTarget : mScene.MainThread()->Viewport->RenderTarget;
	auto &cameraViewpoint = V_IsPolyRenderer() ? PolyRenderer::Instance()->Viewpoint : mScene.MainThread()->Viewport->viewpoint;
	auto &cameraViewwindow = V_IsPolyRenderer() ? PolyRenderer::Instance()->Viewwindow : mScene.MainThread()->Viewport->viewwindow;

	// Grab global state shared with rest of zdoom
	cameraViewpoint = r_viewpoint;
	cameraViewwindow = r_viewwindow;

	auto tex = static_cast<FSWCanvasTexture*>(camtex->GetSoftwareTexture());
	
	DCanvas *Canvas = renderTarget->IsBgra() ? tex->GetCanvasBgra() : tex->GetCanvas();

	// curse Doom's overuse of global variables in the renderer.
	// These get clobbered by rendering to a camera texture but they need to be preserved so the final rendering can be done with the correct palette.
	CameraLight savedCameraLight = *CameraLight::Instance();

	DAngle savedfov = cameraViewpoint.FieldOfView;
	R_SetFOV (cameraViewpoint, fov);

	if (V_IsPolyRenderer())
		PolyRenderer::Instance()->RenderViewToCanvas(viewpoint, Canvas, 0, 0, tex->GetWidth(), tex->GetHeight(), camtex->bFirstUpdate);
	else
		mScene.RenderViewToCanvas(viewpoint, Canvas, 0, 0, tex->GetWidth(), tex->GetHeight(), camtex->bFirstUpdate);

	R_SetFOV (cameraViewpoint, savedfov);

	tex->UpdatePixels(renderTarget->IsBgra());

	*CameraLight::Instance() = savedCameraLight;

	// Sync state back to zdoom
	r_viewpoint = cameraViewpoint;
	r_viewwindow = cameraViewwindow;
}

void FSoftwareRenderer::SetColormap(FLevelLocals *Level)
{
	// This just sets the default colormap for the spftware renderer.
	NormalLight.Maps = realcolormaps.Maps;
	NormalLight.ChangeColor(PalEntry(255, 255, 255), 0);
	NormalLight.ChangeFade(Level->fadeto);
	if (Level->fadeto == 0)
	{
		SetDefaultColormap(Level->info->FadeTable);
	}
}

