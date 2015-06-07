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


#include "r_local.h"
#include "v_palette.h"
#include "v_video.h"
#include "m_png.h"
#include "r_bsp.h"
#include "r_swrenderer.h"
#include "r_3dfloors.h"
#include "textures/textures.h"
#include "r_data/voxels.h"


class FArchive;
void R_SWRSetWindow(int windowSize, int fullWidth, int fullHeight, int stHeight, int trueratio);
void R_SetupColormap(player_t *);
void R_SetupFreelook();
void R_InitRenderer();

extern float LastFOV;

//==========================================================================
//
// DCanvas :: Init
//
//==========================================================================

void FSoftwareRenderer::Init()
{
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
			tex->GetColumn(0, &spanp);
		}
		else if (cache != 0)
		{
			tex->GetPixels ();
		}
		else
		{
			tex->Unload ();
		}
	}
}

//===========================================================================
//
// Render the view 
//
//===========================================================================

void FSoftwareRenderer::RenderView(player_t *player)
{
	R_RenderActorView (player->mo);
	// [RH] Let cameras draw onto textures that were visible this frame.
	FCanvasTextureInfo::UpdateAll ();
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

void FSoftwareRenderer::WriteSavePic (player_t *player, FILE *file, int width, int height)
{
	DCanvas *pic = new DSimpleCanvas (width, height);
	PalEntry palette[256];

	// Take a snapshot of the player's view
	pic->ObjectFlags |= OF_Fixed;
	pic->Lock ();
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
	R_DrawRemainingPlayerSprites();
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
	return down ? MAX_DN_ANGLE : MAX_UP_ANGLE;
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
	fakeActive = 0;
	fake3D = 0;
	while (CurrentSkybox)
	{
		R_3D_DeleteHeights();
		R_3D_LeaveSkybox();
	}
	R_3D_ResetClip();
	R_3D_DeleteHeights();
}

//===========================================================================
//
// 
//
//===========================================================================

void FSoftwareRenderer::ClearBuffer(int color)
{
	memset(RenderTarget->GetBuffer(), color, RenderTarget->GetPitch() * RenderTarget->GetHeight());
}

//===========================================================================
//
// 
//
//===========================================================================

void FSoftwareRenderer::SetWindow (int windowSize, int fullWidth, int fullHeight, int stHeight, int trueratio)
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
	R_CopyStackedViewParameters();
}

//==========================================================================
//
//
//
//==========================================================================

void FSoftwareRenderer::RenderTextureView (FCanvasTexture *tex, AActor *viewpoint, int fov)
{
	BYTE *Pixels = const_cast<BYTE*>(tex->GetPixels());
	DSimpleCanvas *Canvas = tex->GetCanvas();

	// curse Doom's overuse of global variables in the renderer.
	// These get clobbered by rendering to a camera texture but they need to be preserved so the final rendering can be done with the correct palette.
	unsigned char *savecolormap = fixedcolormap;
	FSpecialColormap *savecm = realfixedcolormap;

	float savedfov = LastFOV;
	R_SetFOV ((float)fov);
	R_RenderViewToCanvas (viewpoint, Canvas, 0, 0, tex->GetWidth(), tex->GetHeight(), tex->bFirstUpdate);
	R_SetFOV (savedfov);
	if (Pixels == Canvas->GetBuffer())
	{
		FTexture::FlipSquareBlockRemap (Pixels, tex->GetWidth(), tex->GetHeight(), GPalette.Remap);
	}
	else
	{
		FTexture::FlipNonSquareBlockRemap (Pixels, Canvas->GetBuffer(), tex->GetWidth(), tex->GetHeight(), Canvas->GetPitch(), GPalette.Remap);
	}
	tex->SetUpdated();
	fixedcolormap = savecolormap;
	realfixedcolormap = savecm;
}

//==========================================================================
//
//
//
//==========================================================================

sector_t *FSoftwareRenderer::FakeFlat(sector_t *sec, sector_t *tempsec, int *floorlightlevel, int *ceilinglightlevel, bool back)
{
	return R_FakeFlat(sec, tempsec, floorlightlevel, ceilinglightlevel, back);
}

