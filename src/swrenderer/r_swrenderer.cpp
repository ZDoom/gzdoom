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

void gl_ParseDefs();
void gl_InitData();
void gl_SetActorLights(AActor *);
void gl_PreprocessLevel();
void gl_CleanLevelData();

EXTERN_CVAR(Bool, r_shadercolormaps)
EXTERN_CVAR(Float, maxviewpitch)	// [SP] CVAR from GZDoom

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
	gl_ParseDefs();

	mScene.Init();
}

bool FSoftwareRenderer::UsesColormap() const
{
	return true;
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

		for (int i = 0; i < cls->NumOwnedStates; i++)
		{
			spritelist[cls->OwnedStates[i].sprite] = true;
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
		PolyRenderer::Instance()->RenderView(player);
	else
		mScene.RenderView(player);

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
	pic->ObjectFlags |= OF_Fixed;
	pic->Lock ();
	if (r_polyrenderer)
		PolyRenderer::Instance()->RenderViewToCanvas(player->mo, pic, 0, 0, width, height, true);
	else
		mScene.RenderViewToCanvas (player->mo, pic, 0, 0, width, height);
	screen->GetFlashedPalette (palette);
	M_CreatePNG (file, pic->GetBuffer(), palette, SS_PAL, width, height, pic->GetPitch());
	pic->Unlock ();
	pic->Destroy();
	pic->ObjectFlags |= OF_YesReallyDelete;
	delete pic;
}

void FSoftwareRenderer::DrawRemainingPlayerSprites()
{
	if (!r_polyrenderer)
	{
		mScene.MainThread()->PlayerSprites->RenderRemaining();
	}
	else
	{
		PolyRenderer::Instance()->RenderRemainingPlayerSprites();
	}
}

int FSoftwareRenderer::GetMaxViewPitch(bool down)
{
	const int MAX_DN_ANGLE = 56; // Max looking down angle
	const int MAX_UP_ANGLE = 32; // Max looking up angle
	return (r_polyrenderer) ? int(maxviewpitch) : (down ? MAX_DN_ANGLE : MAX_UP_ANGLE);
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

void FSoftwareRenderer::RenderTextureView (FCanvasTexture *tex, AActor *viewpoint, int fov)
{
	auto viewport = RenderViewport::Instance();
	
	uint8_t *Pixels = viewport->RenderTarget->IsBgra() ? (uint8_t*)tex->GetPixelsBgra() : (uint8_t*)tex->GetPixels();
	DSimpleCanvas *Canvas = viewport->RenderTarget->IsBgra() ? tex->GetCanvasBgra() : tex->GetCanvas();

	// curse Doom's overuse of global variables in the renderer.
	// These get clobbered by rendering to a camera texture but they need to be preserved so the final rendering can be done with the correct palette.
	CameraLight savedCameraLight = *CameraLight::Instance();

	DAngle savedfov = FieldOfView;
	R_SetFOV ((double)fov);

	if (r_polyrenderer)
		PolyRenderer::Instance()->RenderViewToCanvas(viewpoint, Canvas, 0, 0, tex->GetWidth(), tex->GetHeight(), tex->bFirstUpdate);
	else
		mScene.RenderViewToCanvas(viewpoint, Canvas, 0, 0, tex->GetWidth(), tex->GetHeight(), tex->bFirstUpdate);

	R_SetFOV (savedfov);

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

	if (viewport->RenderTarget->IsBgra())
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
}

sector_t *FSoftwareRenderer::FakeFlat(sector_t *sec, sector_t *tempsec, int *floorlightlevel, int *ceilinglightlevel)
{
	return mScene.MainThread()->OpaquePass->FakeFlat(sec, tempsec, floorlightlevel, ceilinglightlevel, nullptr, 0, 0, 0, 0);
}

void FSoftwareRenderer::StateChanged(AActor *actor)
{
	gl_SetActorLights(actor);
}

void FSoftwareRenderer::PreprocessLevel()
{
	gl_PreprocessLevel();
}

void FSoftwareRenderer::CleanLevelData()
{
	gl_CleanLevelData();
}
