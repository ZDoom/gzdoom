/*
**  Handling drawing a player sprite
**  Copyright (c) 2016 Magnus Norddahl
**
**  This software is provided 'as-is', without any express or implied
**  warranty.  In no event will the authors be held liable for any damages
**  arising from the use of this software.
**
**  Permission is granted to anyone to use this software for any purpose,
**  including commercial applications, and to alter it and redistribute it
**  freely, subject to the following restrictions:
**
**  1. The origin of this software must not be misrepresented; you must not
**     claim that you wrote the original software. If you use this software
**     in a product, an acknowledgment in the product documentation would be
**     appreciated but is not required.
**  2. Altered source versions must be plainly marked as such, and must not be
**     misrepresented as being the original software.
**  3. This notice may not be removed or altered from any source distribution.
**
*/

#include <stdlib.h>
#include "templates.h"
#include "doomdef.h"
#include "sbar.h"
#include "r_data/r_translate.h"
#include "poly_playersprite.h"
#include "polyrenderer/poly_renderer.h"
#include "d_player.h"
#include "swrenderer/scene/r_viewport.h"
#include "swrenderer/scene/r_light.h"

EXTERN_CVAR(Bool, r_drawplayersprites)
EXTERN_CVAR(Bool, r_deathcamera)
EXTERN_CVAR(Bool, st_scale)
EXTERN_CVAR(Bool, r_fullbrightignoresectorcolor)
EXTERN_CVAR(Bool, r_shadercolormaps)

void RenderPolyPlayerSprites::Render()
{
	// This code cannot be moved directly to RenderRemainingSprites because the engine
	// draws the canvas textures between this call and the final call to RenderRemainingSprites..

	if (!r_drawplayersprites ||
		!camera ||
		!camera->player ||
		(players[consoleplayer].cheats & CF_CHASECAM) ||
		(r_deathcamera && camera->health <= 0))
		return;

	float bobx, boby;
	P_BobWeapon(camera->player, &bobx, &boby, r_TicFracF);

	// Interpolate the main weapon layer once so as to be able to add it to other layers.
	double wx, wy;
	DPSprite *weapon = camera->player->FindPSprite(PSP_WEAPON);
	if (weapon)
	{
		if (weapon->firstTic)
		{
			wx = weapon->x;
			wy = weapon->y;
		}
		else
		{
			wx = weapon->oldx + (weapon->x - weapon->oldx) * r_TicFracF;
			wy = weapon->oldy + (weapon->y - weapon->oldy) * r_TicFracF;
		}
	}
	else
	{
		wx = 0;
		wy = 0;
	}

	for (DPSprite *sprite = camera->player->psprites; sprite != nullptr; sprite = sprite->GetNext())
	{
		// [RH] Don't draw the targeter's crosshair if the player already has a crosshair set.
		// It's possible this psprite's caller is now null but the layer itself hasn't been destroyed
		// because it didn't tick yet (if we typed 'take all' while in the console for example).
		// In this case let's simply not draw it to avoid crashing.
		if ((sprite->GetID() != PSP_TARGETCENTER || CrosshairImage == nullptr) && sprite->GetCaller() != nullptr)
		{
			RenderSprite(sprite, camera, bobx, boby, wx, wy, r_TicFracF);
		}
	}
}

void RenderPolyPlayerSprites::RenderRemainingSprites()
{
	for (auto &sprite : ScreenSprites)
		sprite.Render();
	ScreenSprites.clear();
}

void RenderPolyPlayerSprites::RenderSprite(DPSprite *sprite, AActor *owner, float bobx, float boby, double wx, double wy, double ticfrac)
{
	// decide which patch to use
	if ((unsigned)sprite->GetSprite() >= (unsigned)sprites.Size())
	{
		DPrintf(DMSG_ERROR, "RenderPlayerSprite: invalid sprite number %i\n", sprite->GetSprite());
		return;
	}

	spritedef_t *def = &sprites[sprite->GetSprite()];
	if (sprite->GetFrame() >= def->numframes)
	{
		DPrintf(DMSG_ERROR, "RenderPlayerSprite: invalid sprite frame %i : %i\n", sprite->GetSprite(), sprite->GetFrame());
		return;
	}

	spriteframe_t *frame = &SpriteFrames[def->spriteframes + sprite->GetFrame()];
	FTextureID picnum = frame->Texture[0];
	bool flip = (frame->Flip & 1) != 0;

	FTexture *tex = TexMan(picnum);
	if (tex->UseType == FTexture::TEX_Null)
		return;

	// Can't interpolate the first tic.
	if (sprite->firstTic)
	{
		sprite->firstTic = false;
		sprite->oldx = sprite->x;
		sprite->oldy = sprite->y;
	}

	double sx = sprite->oldx + (sprite->x - sprite->oldx) * ticfrac;
	double sy = sprite->oldy + (sprite->y - sprite->oldy) * ticfrac;

	if (sprite->Flags & PSPF_ADDBOB)
	{
		sx += bobx;
		sy += boby;
	}

	if (sprite->Flags & PSPF_ADDWEAPON && sprite->GetID() != PSP_WEAPON)
	{
		sx += wx;
		sy += wy;
	}

	double pspritexscale = centerxwide / 160.0;
	double pspriteyscale = pspritexscale * swrenderer::YaspectMul;
	double pspritexiscale = 1 / pspritexscale;

	// calculate edges of the shape
	double tx = sx - BaseXCenter;

	tx -= tex->GetScaledLeftOffset();
	int x1 = xs_RoundToInt(swrenderer::CenterX + tx * pspritexscale);

	// off the right side
	if (x1 > viewwidth)
		return;

	tx += tex->GetScaledWidth();
	int x2 = xs_RoundToInt(swrenderer::CenterX + tx * pspritexscale);

	// off the left side
	if (x2 <= 0)
		return;

	double texturemid = (BaseYCenter - sy) * tex->Scale.Y + tex->TopOffset;

	// Adjust PSprite for fullscreen views
	if (camera->player && (swrenderer::RenderTarget != screen || viewheight == swrenderer::RenderTarget->GetHeight() || (swrenderer::RenderTarget->GetWidth() > (BaseXCenter * 2) && !st_scale)))
	{
		AWeapon *weapon = dyn_cast<AWeapon>(sprite->GetCaller());
		if (weapon != nullptr && weapon->YAdjust != 0)
		{
			if (swrenderer::RenderTarget != screen || viewheight == swrenderer::RenderTarget->GetHeight())
			{
				texturemid -= weapon->YAdjust;
			}
			else
			{
				texturemid -= StatusBar->GetDisplacement() * weapon->YAdjust;
			}
		}
	}

	// Move the weapon down for 1280x1024.
	if (sprite->GetID() < PSP_TARGETCENTER)
	{
		texturemid -= AspectPspriteOffset(WidescreenRatio);
	}

	int clipped_x1 = MAX(x1, 0);
	int clipped_x2 = MIN(x2, viewwidth);
	double xscale = pspritexscale / tex->Scale.X;
	double yscale = pspriteyscale / tex->Scale.Y;
	uint32_t translation = 0; // [RH] Use default colors

	double xiscale, startfrac;
	if (flip)
	{
		xiscale = -pspritexiscale * tex->Scale.X;
		startfrac = 1;
	}
	else
	{
		xiscale = pspritexiscale * tex->Scale.X;
		startfrac = 0;
	}

	if (clipped_x1 > x1)
		startfrac += xiscale * (clipped_x1 - x1);

	bool noaccel = false;

	FDynamicColormap *basecolormap = viewsector->ColorMap;
	FDynamicColormap *colormap_to_use = basecolormap;

	visstyle_t visstyle;
	visstyle.ColormapNum = 0;
	visstyle.BaseColormap = basecolormap;
	visstyle.Alpha = 0;
	visstyle.RenderStyle = STYLE_Normal;

	bool foggy = false;
	int actualextralight = foggy ? 0 : extralight << 4;
	int spriteshade = LIGHT2SHADE(owner->Sector->lightlevel + actualextralight);
	double minz = double((2048 * 4) / double(1 << 20));
	visstyle.ColormapNum = GETPALOOKUP(swrenderer::r_SpriteVisibility / minz, spriteshade);

	if (sprite->GetID() < PSP_TARGETCENTER)
	{
		visstyle.Alpha = float(owner->Alpha);
		visstyle.RenderStyle = owner->RenderStyle;

		// The software renderer cannot invert the source without inverting the overlay
		// too. That means if the source is inverted, we need to do the reverse of what
		// the invert overlay flag says to do.
		INTBOOL invertcolormap = (visstyle.RenderStyle.Flags & STYLEF_InvertOverlay);

		if (visstyle.RenderStyle.Flags & STYLEF_InvertSource)
		{
			invertcolormap = !invertcolormap;
		}

		FDynamicColormap *mybasecolormap = basecolormap;

		if (visstyle.RenderStyle.Flags & STYLEF_FadeToBlack)
		{
			if (invertcolormap)
			{ // Fade to white
				mybasecolormap = GetSpecialLights(mybasecolormap->Color, MAKERGB(255, 255, 255), mybasecolormap->Desaturate);
				invertcolormap = false;
			}
			else
			{ // Fade to black
				mybasecolormap = GetSpecialLights(mybasecolormap->Color, MAKERGB(0, 0, 0), mybasecolormap->Desaturate);
			}
		}

		/*
		if (swrenderer::realfixedcolormap != nullptr && (!swrenderer::r_swtruecolor || (r_shadercolormaps && screen->Accel2D)))
		{ // fixed color
			visstyle.BaseColormap = swrenderer::realfixedcolormap;
			visstyle.ColormapNum = 0;
		}
		else
		{
			if (invertcolormap)
			{
				mybasecolormap = GetSpecialLights(mybasecolormap->Color, mybasecolormap->Fade.InverseColor(), mybasecolormap->Desaturate);
			}
			if (swrenderer::fixedlightlev >= 0)
			{
				visstyle.BaseColormap = (r_fullbrightignoresectorcolor) ? &FullNormalLight : mybasecolormap;
				visstyle.ColormapNum = swrenderer::fixedlightlev >> COLORMAPSHIFT;
			}
			else if (!foggy && sprite->GetState()->GetFullbright())
			{ // full bright
				visstyle.BaseColormap = (r_fullbrightignoresectorcolor) ? &FullNormalLight : mybasecolormap;	// [RH] use basecolormap
				visstyle.ColormapNum = 0;
			}
			else
			{ // local light
				visstyle.BaseColormap = mybasecolormap;
				visstyle.ColormapNum = GETPALOOKUP(0, spriteshade);
			}
		}
		*/

		if (camera->Inventory != nullptr)
		{
			BYTE oldcolormapnum = visstyle.ColormapNum;
			FSWColormap *oldcolormap = visstyle.BaseColormap;
			camera->Inventory->AlterWeaponSprite(&visstyle);
			if (visstyle.BaseColormap != oldcolormap || visstyle.ColormapNum != oldcolormapnum)
			{
				// The colormap has changed. Is it one we can easily identify?
				// If not, then don't bother trying to identify it for
				// hardware accelerated drawing.
				if (visstyle.BaseColormap < &SpecialColormaps[0] ||
					visstyle.BaseColormap > &SpecialColormaps.Last())
				{
					noaccel = true;
				}
				// Has the basecolormap changed? If so, we can't hardware accelerate it,
				// since we don't know what it is anymore.
				else if (visstyle.BaseColormap != mybasecolormap)
				{
					noaccel = true;
				}
			}
		}
		// If we're drawing with a special colormap, but shaders for them are disabled, do
		// not accelerate.
		if (!r_shadercolormaps && (visstyle.BaseColormap >= &SpecialColormaps[0] &&
			visstyle.BaseColormap <= &SpecialColormaps.Last()))
		{
			noaccel = true;
		}
		// If drawing with a BOOM colormap, disable acceleration.
		if (mybasecolormap == &NormalLight && NormalLight.Maps != realcolormaps.Maps)
		{
			noaccel = true;
		}
		// If the main colormap has fixed lights, and this sprite is being drawn with that
		// colormap, disable acceleration so that the lights can remain fixed.
		if (!noaccel && swrenderer::realfixedcolormap == nullptr &&
			NormalLightHasFixedLights && mybasecolormap == &NormalLight &&
			tex->UseBasePalette())
		{
			noaccel = true;
		}
		// [SP] If emulating GZDoom fullbright, disable acceleration
		if (r_fullbrightignoresectorcolor && swrenderer::fixedlightlev >= 0)
			mybasecolormap = &FullNormalLight;
		if (r_fullbrightignoresectorcolor && !foggy && sprite->GetState()->GetFullbright())
			mybasecolormap = &FullNormalLight;
		colormap_to_use = mybasecolormap;
	}

	// Check for hardware-assisted 2D. If it's available, and this sprite is not
	// fuzzy, don't draw it until after the switch to 2D mode.
	if (!noaccel && swrenderer::RenderTarget == screen && (DFrameBuffer *)screen->Accel2D)
	{
		FRenderStyle style = visstyle.RenderStyle;
		style.CheckFuzz();
		if (style.BlendOp != STYLEOP_Fuzz)
		{
			PolyScreenSprite screenSprite;
			screenSprite.Pic = tex;
			screenSprite.X1 = viewwindowx + x1;
			screenSprite.Y1 = viewwindowy + viewheight / 2 - texturemid * yscale - 0.5;
			screenSprite.Width = tex->GetWidth() * xscale;
			screenSprite.Height = tex->GetHeight() * yscale;
			screenSprite.Translation = TranslationToTable(translation);
			screenSprite.Flip = xiscale < 0;
			screenSprite.visstyle = visstyle;
			screenSprite.Colormap = colormap_to_use;
			ScreenSprites.push_back(screenSprite);
			return;
		}
	}

	// To do: draw sprite same way as R_DrawVisSprite(vis) here

	// Draw the fuzzy weapon:
	FRenderStyle style = visstyle.RenderStyle;
	style.CheckFuzz();
	if (style.BlendOp == STYLEOP_Fuzz)
	{
		visstyle.RenderStyle = LegacyRenderStyles[STYLE_Shadow];

		PolyScreenSprite screenSprite;
		screenSprite.Pic = tex;
		screenSprite.X1 = viewwindowx + x1;
		screenSprite.Y1 = viewwindowy + viewheight / 2 - texturemid * yscale - 0.5;
		screenSprite.Width = tex->GetWidth() * xscale;
		screenSprite.Height = tex->GetHeight() * yscale;
		screenSprite.Translation = TranslationToTable(translation);
		screenSprite.Flip = xiscale < 0;
		screenSprite.visstyle = visstyle;
		screenSprite.Colormap = colormap_to_use;
		ScreenSprites.push_back(screenSprite);
	}
}

void PolyScreenSprite::Render()
{
	FSpecialColormap *special = nullptr;
	FColormapStyle colormapstyle;
	PalEntry overlay = 0;
	bool usecolormapstyle = false;
	if (visstyle.BaseColormap >= &SpecialColormaps[0] &&
		visstyle.BaseColormap < &SpecialColormaps[SpecialColormaps.Size()])
	{
		special = static_cast<FSpecialColormap*>(visstyle.BaseColormap);
	}
	else if (Colormap->Color == PalEntry(255, 255, 255) &&
		Colormap->Desaturate == 0)
	{
		overlay = Colormap->Fade;
		overlay.a = BYTE(visstyle.ColormapNum * 255 / NUMCOLORMAPS);
	}
	else
	{
		usecolormapstyle = true;
		colormapstyle.Color = Colormap->Color;
		colormapstyle.Fade = Colormap->Fade;
		colormapstyle.Desaturate = Colormap->Desaturate;
		colormapstyle.FadeLevel = visstyle.ColormapNum / float(NUMCOLORMAPS);
	}

	screen->DrawTexture(Pic,
		X1,
		Y1,
		DTA_DestWidthF, Width,
		DTA_DestHeightF, Height,
		DTA_Translation, Translation,
		DTA_FlipX, Flip,
		DTA_TopOffset, 0,
		DTA_LeftOffset, 0,
		DTA_ClipLeft, viewwindowx,
		DTA_ClipTop, viewwindowy,
		DTA_ClipRight, viewwindowx + viewwidth,
		DTA_ClipBottom, viewwindowy + viewheight,
		DTA_AlphaF, visstyle.Alpha,
		DTA_RenderStyle, visstyle.RenderStyle,
		DTA_FillColor, FillColor,
		DTA_SpecialColormap, special,
		DTA_ColorOverlay, overlay.d,
		DTA_ColormapStyle, usecolormapstyle ? &colormapstyle : nullptr,
		TAG_DONE);
}
