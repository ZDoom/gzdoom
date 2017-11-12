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

// [BB] Use ZDoom's freelook limit for the sotfware renderer.
// Note: ZDoom's limit is chosen such that the sky is rendered properly.
CUSTOM_CVAR (Bool, cl_oldfreelooklimit, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOINITCALL)
{
	if (usergame) // [SP] Update pitch limits to the netgame/gamesim.
		players[consoleplayer].SendPitchLimits();
}

EXTERN_CVAR(Bool, r_shadercolormaps)
EXTERN_CVAR(Float, maxviewpitch)	// [SP] CVAR from OpenGL Renderer
EXTERN_CVAR(Bool, r_drawvoxels)

CUSTOM_CVAR(Bool, r_polyrenderer, 0, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOINITCALL)
{
	if (self == 1 && !hasglnodes)
	{
		Printf("No GL BSP detected. You must restart the map before rendering will be correct\n");
	}

	if (usergame)
	{
		// [SP] Update pitch limits to the netgame/gamesim.
		players[consoleplayer].SendPitchLimits();
	}
}

using namespace swrenderer;

FSoftwareRenderer::FSoftwareRenderer()
{
}

FSoftwareRenderer::~FSoftwareRenderer()
{
}

void FSoftwareRenderer::Init()
{
	mScene.Init();
	InitSWColorMaps();		
}

void FSoftwareRenderer::PrecacheTexture(FTexture *tex, int cache)
{
	bool isbgra = screen->IsBgra();

	if (tex != NULL)
	{
		if (cache & FTextureManager::HIT_Columnmode)
		{
			const FTexture::Span *spanp;
			if (isbgra)
				tex->GetColumnBgra(0, &spanp);
			else
				tex->GetColumn(0, &spanp);
		}
		else if (cache != 0)
		{
			if (isbgra)
				tex->GetPixelsBgra();
			else
				tex->GetPixels ();
		}
		else
		{
			tex->Unload ();
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
	for (int i = cnt - 1; i >= 0; i--)
	{
		PrecacheTexture(TexMan.ByIndex(i), texhitlist[i]);
	}
}

void FSoftwareRenderer::RenderView(player_t *player)
{
	if (r_polyrenderer)
	{
		PolyRenderer::Instance()->Viewpoint = r_viewpoint;
		PolyRenderer::Instance()->Viewwindow = r_viewwindow;
		PolyRenderer::Instance()->RenderView(player);
		r_viewpoint = PolyRenderer::Instance()->Viewpoint;
		r_viewwindow = PolyRenderer::Instance()->Viewwindow;
	}
	else
	{
		mScene.MainThread()->Viewport->viewpoint = r_viewpoint;
		mScene.MainThread()->Viewport->viewwindow = r_viewwindow;
		mScene.RenderView(player);
		r_viewpoint = mScene.MainThread()->Viewport->viewpoint;
		r_viewwindow = mScene.MainThread()->Viewport->viewwindow;
	}

	FCanvasTextureInfo::UpdateAll();
}

void FSoftwareRenderer::RemapVoxels()
{
	for (unsigned i=0; i<Voxels.Size(); i++)
	{
		Voxels[i]->CreateBgraSlabData();
		Voxels[i]->Remap();
	}
}

void FSoftwareRenderer::WriteSavePic (player_t *player, FileWriter *file, int width, int height)
{
	DCanvas *pic = new DSimpleCanvas (width, height, false);
	PalEntry palette[256];

	// Take a snapshot of the player's view
	pic->Lock ();
	if (r_polyrenderer)
	{
		PolyRenderer::Instance()->Viewpoint = r_viewpoint;
		PolyRenderer::Instance()->Viewwindow = r_viewwindow;
		PolyRenderer::Instance()->RenderViewToCanvas(player->mo, pic, 0, 0, width, height, true);
		r_viewpoint = PolyRenderer::Instance()->Viewpoint;
		r_viewwindow = PolyRenderer::Instance()->Viewwindow;
	}
	else
	{
		mScene.MainThread()->Viewport->viewpoint = r_viewpoint;
		mScene.MainThread()->Viewport->viewwindow = r_viewwindow;
		mScene.RenderViewToCanvas(player->mo, pic, 0, 0, width, height);
		r_viewpoint = mScene.MainThread()->Viewport->viewpoint;
		r_viewwindow = mScene.MainThread()->Viewport->viewwindow;
	}
	screen->GetFlashedPalette (palette);
	M_CreatePNG (file, pic->GetBuffer(), palette, SS_PAL, width, height, pic->GetPitch());
	pic->Unlock ();
	delete pic;
}

void FSoftwareRenderer::DrawRemainingPlayerSprites()
{
	if (!r_polyrenderer)
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

int FSoftwareRenderer::GetMaxViewPitch(bool down)
{
	int MAX_DN_ANGLE = MIN(56, (int)maxviewpitch); // Max looking down angle
	int MAX_UP_ANGLE = MIN(32, (int)maxviewpitch); // Max looking up angle
	return (r_polyrenderer) ? int(maxviewpitch) : (down ? MAX_DN_ANGLE : ((cl_oldfreelooklimit) ? MAX_UP_ANGLE : MAX_DN_ANGLE));
}

bool FSoftwareRenderer::RequireGLNodes()
{
	return true;
}

void FSoftwareRenderer::OnModeSet ()
{
	mScene.ScreenResized();
}

void FSoftwareRenderer::SetClearColor(int color)
{
	mScene.SetClearColor(color);
}

void FSoftwareRenderer::RenderTextureView (FCanvasTexture *tex, AActor *viewpoint, double fov)
{
	auto renderTarget = r_polyrenderer ? PolyRenderer::Instance()->RenderTarget : mScene.MainThread()->Viewport->RenderTarget;
	auto &cameraViewpoint = r_polyrenderer ? PolyRenderer::Instance()->Viewpoint : mScene.MainThread()->Viewport->viewpoint;
	auto &cameraViewwindow = r_polyrenderer ? PolyRenderer::Instance()->Viewwindow : mScene.MainThread()->Viewport->viewwindow;

	// Grab global state shared with rest of zdoom
	cameraViewpoint = r_viewpoint;
	cameraViewwindow = r_viewwindow;
	
	uint8_t *Pixels = renderTarget->IsBgra() ? (uint8_t*)tex->GetPixelsBgra() : (uint8_t*)tex->GetPixels();
	DSimpleCanvas *Canvas = renderTarget->IsBgra() ? tex->GetCanvasBgra() : tex->GetCanvas();

	// curse Doom's overuse of global variables in the renderer.
	// These get clobbered by rendering to a camera texture but they need to be preserved so the final rendering can be done with the correct palette.
	CameraLight savedCameraLight = *CameraLight::Instance();

	DAngle savedfov = cameraViewpoint.FieldOfView;
	R_SetFOV (cameraViewpoint, fov);

	if (r_polyrenderer)
		PolyRenderer::Instance()->RenderViewToCanvas(viewpoint, Canvas, 0, 0, tex->GetWidth(), tex->GetHeight(), tex->bFirstUpdate);
	else
		mScene.RenderViewToCanvas(viewpoint, Canvas, 0, 0, tex->GetWidth(), tex->GetHeight(), tex->bFirstUpdate);

	R_SetFOV (cameraViewpoint, savedfov);

	if (Canvas->IsBgra())
	{
		if (Pixels == Canvas->GetBuffer())
		{
			FTexture::FlipSquareBlockBgra((uint32_t*)Pixels, tex->GetWidth(), tex->GetHeight());
		}
		else
		{
			FTexture::FlipNonSquareBlockBgra((uint32_t*)Pixels, (const uint32_t*)Canvas->GetBuffer(), tex->GetWidth(), tex->GetHeight(), Canvas->GetPitch());
		}
	}
	else
	{
		if (Pixels == Canvas->GetBuffer())
		{
			FTexture::FlipSquareBlockRemap(Pixels, tex->GetWidth(), tex->GetHeight(), GPalette.Remap);
		}
		else
		{
			FTexture::FlipNonSquareBlockRemap(Pixels, Canvas->GetBuffer(), tex->GetWidth(), tex->GetHeight(), Canvas->GetPitch(), GPalette.Remap);
		}
	}

	if (renderTarget->IsBgra())
	{
		// True color render still sometimes uses palette textures (for sprites, mostly).
		// We need to make sure that both pixel buffers contain data:
		int width = tex->GetWidth();
		int height = tex->GetHeight();
		uint8_t *palbuffer = (uint8_t *)tex->GetPixels();
		uint32_t *bgrabuffer = (uint32_t*)tex->GetPixelsBgra();
		for (int x = 0; x < width; x++)
		{
			for (int y = 0; y < height; y++)
			{
				uint32_t color = bgrabuffer[y];
				int r = RPART(color);
				int g = GPART(color);
				int b = BPART(color);
				palbuffer[y] = RGB32k.RGB[r >> 3][g >> 3][b >> 3];
			}
			palbuffer += height;
			bgrabuffer += height;
		}
	}

	tex->SetUpdated();

	*CameraLight::Instance() = savedCameraLight;

	// Sync state back to zdoom
	r_viewpoint = cameraViewpoint;
	r_viewwindow = cameraViewwindow;
}

void FSoftwareRenderer::PreprocessLevel()
{
	// This just sets the default colormap for the spftware renderer.
	NormalLight.Maps = realcolormaps.Maps;
	NormalLight.ChangeColor(PalEntry(255, 255, 255), 0);
	NormalLight.ChangeFade(level.fadeto);

	if (level.fadeto == 0)
	{
		SetDefaultColormap(level.info->FadeTable);
		if (level.flags & LEVEL_HASFADETABLE)
		{
			// This should really be done differently.
			level.fadeto = 0xff939393; //[SP] Hexen True-color compatibility, just use gray.
			for (auto &s : level.sectors)
			{
				s.Colormap.FadeColor = level.fadeto;
			}
		}
	}
}

void FSoftwareRenderer::CleanLevelData()
{
}

uint32_t FSoftwareRenderer::GetCaps()
{
	ActorRenderFeatureFlags FlagSet = 0;

	if (r_polyrenderer)
		FlagSet |= RFF_POLYGONAL | RFF_TILTPITCH | RFF_SLOPE3DFLOORS;
	else
	{
		FlagSet |= RFF_UNCLIPPEDTEX;
		if (r_drawvoxels)
			FlagSet |= RFF_VOXELS;
	}

	if (screen && screen->IsBgra())
		FlagSet |= RFF_TRUECOLOR;
	else
		FlagSet |= RFF_COLORMAP;

	return (uint32_t)FlagSet;
}