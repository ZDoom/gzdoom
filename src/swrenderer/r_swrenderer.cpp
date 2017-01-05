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


#include "r_main.h"
#include "swrenderer/things/r_playersprite.h"
#include "v_palette.h"
#include "v_video.h"
#include "m_png.h"
#include "r_swrenderer.h"
#include "scene/r_bsp.h"
#include "scene/r_3dfloors.h"
#include "scene/r_portal.h"
#include "textures/textures.h"
#include "r_data/voxels.h"
#include "drawers/r_draw_rgba.h"
#include "drawers/r_drawers.h"
#include "polyrenderer/poly_renderer.h"
#include "p_setup.h"

void gl_ParseDefs();
void gl_InitData();

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

namespace swrenderer
{

void R_SWRSetWindow(int windowSize, int fullWidth, int fullHeight, int stHeight, float trueratio);
void R_SetupColormap(player_t *);
void R_SetupFreelook();
void R_InitRenderer();

}

using namespace swrenderer;

FSoftwareRenderer::FSoftwareRenderer()
{
}

FSoftwareRenderer::~FSoftwareRenderer()
{
}

//==========================================================================
//
// DCanvas :: Init
//
//==========================================================================

void FSoftwareRenderer::Init()
{
	gl_ParseDefs();

	r_swtruecolor = screen->IsBgra();
	R_InitRenderer();
}

//==========================================================================
//
// DCanvas :: UsesColormap
//
//==========================================================================

bool FSoftwareRenderer::UsesColormap() const
{
	return true;
}

//===========================================================================
//
// Texture precaching
//
//===========================================================================

void FSoftwareRenderer::PrecacheTexture(FTexture *tex, int cache)
{
	if (tex != NULL)
	{
		if (cache & FTextureManager::HIT_Columnmode)
		{
			const FTexture::Span *spanp;
			if (r_swtruecolor)
				tex->GetColumnBgra(0, &spanp);
			else
				tex->GetColumn(0, &spanp);
		}
		else if (cache != 0)
		{
			if (r_swtruecolor)
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

void FSoftwareRenderer::Precache(BYTE *texhitlist, TMap<PClassActor*, bool> &actorhitlist)
{
	BYTE *spritelist = new BYTE[sprites.Size()];
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

//===========================================================================
//
// Render the view 
//
//===========================================================================

void FSoftwareRenderer::RenderView(player_t *player)
{
	if (r_polyrenderer)
	{
		bool saved_swtruecolor = r_swtruecolor;
		r_swtruecolor = screen->IsBgra();
		
		PolyRenderer::Instance()->RenderActorView(player->mo, false);
		FCanvasTextureInfo::UpdateAll();
		
		// Apply special colormap if the target cannot do it
		if (realfixedcolormap && r_swtruecolor && !(r_shadercolormaps && screen->Accel2D))
		{
			R_BeginDrawerCommands();
			DrawerCommandQueue::QueueCommand<ApplySpecialColormapRGBACommand>(realfixedcolormap, screen);
			R_EndDrawerCommands();
		}
		
		r_swtruecolor = saved_swtruecolor;
		
		return;
	}

	if (r_swtruecolor != screen->IsBgra())
	{
		r_swtruecolor = screen->IsBgra();
		R_InitColumnDrawers();
	}

	R_BeginDrawerCommands();
	R_RenderActorView (player->mo);
	// [RH] Let cameras draw onto textures that were visible this frame.
	FCanvasTextureInfo::UpdateAll ();

	// Apply special colormap if the target cannot do it
	if (realfixedcolormap && r_swtruecolor && !(r_shadercolormaps && screen->Accel2D))
	{
		DrawerCommandQueue::QueueCommand<ApplySpecialColormapRGBACommand>(realfixedcolormap, screen);
	}

	R_EndDrawerCommands();
}

//==========================================================================
//
//
//
//==========================================================================

void FSoftwareRenderer::RemapVoxels()
{
	for (unsigned i=0; i<Voxels.Size(); i++)
	{
		Voxels[i]->Remap();
	}
}

//===========================================================================
//
// Render the view to a savegame picture
//
//===========================================================================

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
		R_RenderViewToCanvas (player->mo, pic, 0, 0, width, height);
	screen->GetFlashedPalette (palette);
	M_CreatePNG (file, pic->GetBuffer(), palette, SS_PAL, width, height, pic->GetPitch());
	pic->Unlock ();
	pic->Destroy();
	pic->ObjectFlags |= OF_YesReallyDelete;
	delete pic;
}

//===========================================================================
//
// 
//
//===========================================================================

void FSoftwareRenderer::DrawRemainingPlayerSprites()
{
	if (!r_polyrenderer)
	{
		R_DrawRemainingPlayerSprites();
	}
	else
	{
		PolyRenderer::Instance()->RenderRemainingPlayerSprites();
	}
}

//===========================================================================
//
// Get max. view angle (renderer specific information so it goes here now)
//
//===========================================================================
#define MAX_DN_ANGLE	56		// Max looking down angle
#define MAX_UP_ANGLE	32		// Max looking up angle

int FSoftwareRenderer::GetMaxViewPitch(bool down)
{
	return (r_polyrenderer) ? int(maxviewpitch) : (down ? MAX_DN_ANGLE : MAX_UP_ANGLE);
}

bool FSoftwareRenderer::RequireGLNodes()
{
	return true;
}

//==========================================================================
//
// OnModeSet
//
// Called from V_SetResolution()
//
//==========================================================================

void FSoftwareRenderer::OnModeSet ()
{
	R_MultiresInit ();

	RenderTarget = screen;
	screen->Lock (true);
	R_SetupBuffer ();
	screen->Unlock ();
}

//===========================================================================
//
// 
//
//===========================================================================

void FSoftwareRenderer::ErrorCleanup ()
{
	Clip3DFloors::Instance()->Cleanup();
}

//===========================================================================
//
// 
//
//===========================================================================

void FSoftwareRenderer::ClearBuffer(int color)
{
	// [SP] For now, for truecolor, this just outputs black. We'll figure out how to get something more meaningful
	// later when this actually matters more. This is just to clear HOMs for now.
	if (!r_swtruecolor)
		memset(RenderTarget->GetBuffer(), color, RenderTarget->GetPitch() * RenderTarget->GetHeight());
	else
		memset(RenderTarget->GetBuffer(), 0, RenderTarget->GetPitch() * RenderTarget->GetHeight() * 4);
}

//===========================================================================
//
// 
//
//===========================================================================

void FSoftwareRenderer::SetWindow (int windowSize, int fullWidth, int fullHeight, int stHeight, float trueratio)
{
	R_SWRSetWindow(windowSize, fullWidth, fullHeight, stHeight, trueratio);
}

//===========================================================================
//
// 
//
//===========================================================================

void FSoftwareRenderer::SetupFrame(player_t *player)
{
	R_SetupColormap(player);
	R_SetupFreelook();
}

//==========================================================================
//
// R_CopyStackedViewParameters
//
//==========================================================================

void FSoftwareRenderer::CopyStackedViewParameters() 
{
	RenderPortal::Instance()->CopyStackedViewParameters();
}

//==========================================================================
//
//
//
//==========================================================================

void FSoftwareRenderer::RenderTextureView (FCanvasTexture *tex, AActor *viewpoint, int fov)
{
	BYTE *Pixels = r_swtruecolor ? (BYTE*)tex->GetPixelsBgra() : (BYTE*)tex->GetPixels();
	DSimpleCanvas *Canvas = r_swtruecolor ? tex->GetCanvasBgra() : tex->GetCanvas();

	// curse Doom's overuse of global variables in the renderer.
	// These get clobbered by rendering to a camera texture but they need to be preserved so the final rendering can be done with the correct palette.
	FSWColormap *savecolormap = fixedcolormap;
	FSpecialColormap *savecm = realfixedcolormap;

	DAngle savedfov = FieldOfView;
	R_SetFOV ((double)fov);
	if (r_polyrenderer)
		PolyRenderer::Instance()->RenderViewToCanvas(viewpoint, Canvas, 0, 0, tex->GetWidth(), tex->GetHeight(), tex->bFirstUpdate);
	else
		R_RenderViewToCanvas (viewpoint, Canvas, 0, 0, tex->GetWidth(), tex->GetHeight(), tex->bFirstUpdate);
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

	if (r_swtruecolor)
	{
		// True color render still sometimes uses palette textures (for sprites, mostly).
		// We need to make sure that both pixel buffers contain data:
		int width = tex->GetWidth();
		int height = tex->GetHeight();
		BYTE *palbuffer = (BYTE *)tex->GetPixels();
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

	fixedcolormap = savecolormap;
	realfixedcolormap = savecm;
}

sector_t *FSoftwareRenderer::FakeFlat(sector_t *sec, sector_t *tempsec, int *floorlightlevel, int *ceilinglightlevel)
{
	return RenderBSP::Instance()->FakeFlat(sec, tempsec, floorlightlevel, ceilinglightlevel, nullptr, 0, 0, 0, 0);
}

