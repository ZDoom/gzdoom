/*
**  Polygon Doom software renderer
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
#include "polyrenderer/scene/poly_light.h"
#include "polyrenderer/scene/poly_model.h"

EXTERN_CVAR(Bool, r_drawplayersprites)
EXTERN_CVAR(Bool, r_deathcamera)
EXTERN_CVAR(Bool, r_fullbrightignoresectorcolor)
EXTERN_CVAR(Bool, r_shadercolormaps)

void RenderPolyPlayerSprites::Render(PolyRenderThread *thread)
{
	// This code cannot be moved directly to RenderRemainingSprites because the engine
	// draws the canvas textures between this call and the final call to RenderRemainingSprites..
	//
	// We also can't move it because the model render code relies on it

	//renderHUDModel = gl_IsHUDModelForPlayerAvailable(players[consoleplayer].camera->player);

	const auto &viewpoint = PolyRenderer::Instance()->Viewpoint;

	int 		i;
	int 		lightnum;
	DPSprite*	psp;
	DPSprite*	weapon;
	sector_t*	sec = nullptr;
	int			floorlight, ceilinglight;
	F3DFloor *rover;

	if (!r_drawplayersprites ||
		!viewpoint.camera ||
		!viewpoint.camera->player ||
		(players[consoleplayer].cheats & CF_CHASECAM) ||
		(r_deathcamera && viewpoint.camera->health <= 0))
		return;

	PolyTransferHeights fakeflat(viewpoint.camera->subsector);

	FDynamicColormap *basecolormap;
	PolyCameraLight *cameraLight = PolyCameraLight::Instance();
	if (cameraLight->FixedLightLevel() < 0 && viewpoint.sector->e && viewpoint.sector->e->XFloor.lightlist.Size())
	{
		for (i = viewpoint.sector->e->XFloor.lightlist.Size() - 1; i >= 0; i--)
		{
			if (viewpoint.Pos.Z <= viewpoint.sector->e->XFloor.lightlist[i].plane.Zat0())
			{
				rover = viewpoint.sector->e->XFloor.lightlist[i].caster;
				if (rover)
				{
					if (rover->flags & FF_DOUBLESHADOW && viewpoint.Pos.Z <= rover->bottom.plane->Zat0())
						break;
					sec = rover->model;
					if (rover->flags & FF_FADEWALLS)
						basecolormap = GetColorTable(sec->Colormap, sec->SpecialColors[sector_t::sprites], true);
					else
						basecolormap = GetColorTable(viewpoint.sector->e->XFloor.lightlist[i].extra_colormap, sec->SpecialColors[sector_t::sprites], true);
				}
				break;
			}
		}
		if (!sec)
		{
			sec = viewpoint.sector;
			basecolormap = GetColorTable(sec->Colormap, sec->SpecialColors[sector_t::sprites], true);
		}
		floorlight = ceilinglight = sec->lightlevel;
	}
	else
	{	// This used to use camera->Sector but due to interpolation that can be incorrect
		// when the interpolated viewpoint is in a different sector than the camera.

		sec = fakeflat.FrontSector;
		floorlight = fakeflat.FloorLightLevel;
		ceilinglight = fakeflat.CeilingLightLevel;

		// [RH] set basecolormap
		basecolormap = GetColorTable(sec->Colormap, sec->SpecialColors[sector_t::sprites], true);
	}

	// [RH] set foggy flag
	bool foggy = (level.fadeto || basecolormap->Fade || (level.flags & LEVEL_HASFADETABLE));

	// get light level
	lightnum = ((floorlight + ceilinglight) >> 1) + (foggy ? 0 : viewpoint.extralight << 4);
	int spriteshade = LightLevelToShade(lightnum, foggy) - 24 * FRACUNIT;

	if (viewpoint.camera->player != nullptr)
	{
		double wx, wy;
		float bobx, boby;

		P_BobWeapon(viewpoint.camera->player, &bobx, &boby, viewpoint.TicFrac);

		// Interpolate the main weapon layer once so as to be able to add it to other layers.
		if ((weapon = viewpoint.camera->player->FindPSprite(PSP_WEAPON)) != nullptr)
		{
			if (weapon->firstTic)
			{
				wx = weapon->x;
				wy = weapon->y;
			}
			else
			{
				wx = weapon->oldx + (weapon->x - weapon->oldx) * viewpoint.TicFrac;
				wy = weapon->oldy + (weapon->y - weapon->oldy) * viewpoint.TicFrac;
			}
		}
		else
		{
			wx = 0;
			wy = 0;
		}

		// add all active psprites
		psp = viewpoint.camera->player->psprites;
		while (psp)
		{
			// [RH] Don't draw the targeter's crosshair if the player already has a crosshair set.
			// It's possible this psprite's caller is now null but the layer itself hasn't been destroyed
			// because it didn't tick yet (if we typed 'take all' while in the console for example).
			// In this case let's simply not draw it to avoid crashing.

			if ((psp->GetID() != PSP_TARGETCENTER || CrosshairImage == nullptr) && psp->GetCaller() != nullptr)
			{
				RenderSprite(thread, psp, viewpoint.camera, bobx, boby, wx, wy, viewpoint.TicFrac, spriteshade, basecolormap, foggy);
			}

			psp = psp->GetNext();
		}
	}
}

void RenderPolyPlayerSprites::RenderRemainingSprites()
{
	for (const PolyHWAccelPlayerSprite &sprite : AcceleratedSprites)
	{
		screen->DrawTexture(sprite.pic,
			viewwindowx + sprite.x1,
			viewwindowy + viewheight / 2 - sprite.texturemid * sprite.yscale - 0.5,
			DTA_DestWidthF, FIXED2DBL(sprite.pic->GetWidth() * sprite.xscale),
			DTA_DestHeightF, sprite.pic->GetHeight() * sprite.yscale,
			DTA_TranslationIndex, sprite.Translation,
			DTA_FlipX, sprite.flip,
			DTA_TopOffset, 0,
			DTA_LeftOffset, 0,
			DTA_ClipLeft, viewwindowx,
			DTA_ClipTop, viewwindowy,
			DTA_ClipRight, viewwindowx + viewwidth,
			DTA_ClipBottom, viewwindowy + viewheight,
			DTA_Alpha, sprite.Alpha,
			DTA_RenderStyle, sprite.RenderStyle,
			DTA_FillColor, sprite.FillColor,
			DTA_SpecialColormap, sprite.special,
			DTA_ColorOverlay, sprite.overlay.d,
			DTA_ColormapStyle, sprite.usecolormapstyle ? &sprite.colormapstyle : nullptr,
			TAG_DONE);
	}

	AcceleratedSprites.Clear();
}

void RenderPolyPlayerSprites::RenderSprite(PolyRenderThread *thread, DPSprite *pspr, AActor *owner, float bobx, float boby, double wx, double wy, double ticfrac, int spriteshade, FDynamicColormap *basecolormap, bool foggy)
{
	double 				tx;
	int 				x1;
	int 				x2;
	double				sx, sy;
	spritedef_t*		sprdef;
	spriteframe_t*		sprframe;
	FTextureID			picnum;
	uint16_t				flip;
	FTexture*			tex;
	bool				noaccel;
	double				alpha = owner->Alpha;

	// decide which patch to use
	if ((unsigned)pspr->GetSprite() >= (unsigned)sprites.Size())
	{
		DPrintf(DMSG_ERROR, "R_DrawPSprite: invalid sprite number %i\n", pspr->GetSprite());
		return;
	}
	sprdef = &sprites[pspr->GetSprite()];
	if (pspr->GetFrame() >= sprdef->numframes)
	{
		DPrintf(DMSG_ERROR, "R_DrawPSprite: invalid sprite frame %i : %i\n", pspr->GetSprite(), pspr->GetFrame());
		return;
	}

	const auto &viewpoint = PolyRenderer::Instance()->Viewpoint;
	const auto &viewwindow = PolyRenderer::Instance()->Viewwindow;
	DCanvas *renderTarget = PolyRenderer::Instance()->RenderTarget;

	sprframe = &SpriteFrames[sprdef->spriteframes + pspr->GetFrame()];

	picnum = sprframe->Texture[0];
	flip = sprframe->Flip & 1;
	tex = TexMan(picnum);

	if (tex->UseType == FTexture::TEX_Null)
		return;

	if (pspr->firstTic)
	{ // Can't interpolate the first tic.
		pspr->firstTic = false;
		pspr->oldx = pspr->x;
		pspr->oldy = pspr->y;
	}

	sx = pspr->oldx + (pspr->x - pspr->oldx) * ticfrac;
	sy = pspr->oldy + (pspr->y - pspr->oldy) * ticfrac + WEAPON_FUDGE_Y;

	if (pspr->Flags & PSPF_ADDBOB)
	{
		sx += (pspr->Flags & PSPF_MIRROR) ? -bobx : bobx;
		sy += boby;
	}

	if (pspr->Flags & PSPF_ADDWEAPON && pspr->GetID() != PSP_WEAPON)
	{
		sx += wx;
		sy += wy;
	}

	if (renderHUDModel)
	{
		PolyRenderHUDModel(thread, PolyRenderer::Instance()->WorldToClip, PolyClipPlane(), 1, pspr, (float)sx, (float)sy);
		return;
	}

	double yaspectMul = 1.2 * ((double)SCREENHEIGHT / SCREENWIDTH) * r_viewwindow.WidescreenRatio;

	double pspritexscale = viewwindow.centerxwide / 160.0;
	double pspriteyscale = pspritexscale * yaspectMul;
	double pspritexiscale = 1 / pspritexscale;

	int tleft = tex->GetScaledLeftOffset();
	int twidth = tex->GetScaledWidth();

	// calculate edges of the shape
	//tx = sx - BASEXCENTER;
	tx = (pspr->Flags & PSPF_MIRROR) ? ((BASEXCENTER - twidth) - (sx - tleft)) : ((sx - BASEXCENTER) - tleft);

	x1 = xs_RoundToInt(viewwindow.centerx + tx * pspritexscale);

	// off the right side
	if (x1 > viewwidth)
		return;

	tx += twidth;
	x2 = xs_RoundToInt(viewwindow.centerx + tx * pspritexscale);

	// off the left side
	if (x2 <= 0)
		return;

	// store information in a vissprite
	PolyNoAccelPlayerSprite vis;

	vis.renderflags = owner->renderflags;

	vis.texturemid = (BASEYCENTER - sy) * tex->Scale.Y + tex->TopOffset;

	if (viewpoint.camera->player && (renderTarget != screen ||
		viewheight == renderTarget->GetHeight() ||
		(renderTarget->GetWidth() > (BASEXCENTER * 2))))
	{	// Adjust PSprite for fullscreen views
		AWeapon *weapon = dyn_cast<AWeapon>(pspr->GetCaller());
		if (weapon != nullptr && weapon->YAdjust != 0)
		{
			if (renderTarget != screen || viewheight == renderTarget->GetHeight())
			{
				vis.texturemid -= weapon->YAdjust;
			}
			else
			{
				vis.texturemid -= StatusBar->GetDisplacement() * weapon->YAdjust;
			}
		}
	}
	if (pspr->GetID() < PSP_TARGETCENTER)
	{ // Move the weapon down for 1280x1024.
		vis.texturemid -= AspectPspriteOffset(viewwindow.WidescreenRatio);
	}
	vis.x1 = x1 < 0 ? 0 : x1;
	vis.x2 = x2 >= viewwidth ? viewwidth : x2;
	vis.xscale = FLOAT2FIXED(pspritexscale / tex->Scale.X);
	vis.yscale = float(pspriteyscale / tex->Scale.Y);
	vis.pic = tex;

	// If flip is used, provided that it's not already flipped (that would just invert itself)
	// (It's an XOR...)
	if (!(flip) != !(pspr->Flags & PSPF_FLIP))
	{
		vis.xiscale = -FLOAT2FIXED(pspritexiscale * tex->Scale.X);
		vis.startfrac = (tex->GetWidth() << FRACBITS) - 1;
	}
	else
	{
		vis.xiscale = FLOAT2FIXED(pspritexiscale * tex->Scale.X);
		vis.startfrac = 0;
	}

	if (vis.x1 > x1)
		vis.startfrac += vis.xiscale*(vis.x1 - x1);

	noaccel = false;
	FDynamicColormap *colormap_to_use = nullptr;
	if (pspr->GetID() < PSP_TARGETCENTER)
	{
		auto rs = pspr->GetRenderStyle(owner->RenderStyle, owner->Alpha);
		vis.RenderStyle = rs.first;
		vis.Alpha = rs.second;

		if (!vis.RenderStyle.IsVisible(vis.Alpha))
			return;

		//-----------------------------------------------------------------------------

		// The software renderer cannot invert the source without inverting the overlay
		// too. That means if the source is inverted, we need to do the reverse of what
		// the invert overlay flag says to do.
		bool invertcolormap = (vis.RenderStyle.Flags & STYLEF_InvertOverlay) != 0;

		if (vis.RenderStyle.Flags & STYLEF_InvertSource)
		{
			invertcolormap = !invertcolormap;
		}

		bool fullbright = !foggy && pspr->GetState()->GetFullbright();
		bool fadeToBlack = (vis.RenderStyle.Flags & STYLEF_FadeToBlack) != 0;

		vis.Light.SetColormap(0, spriteshade, basecolormap, fullbright, invertcolormap, fadeToBlack);

		colormap_to_use = (FDynamicColormap*)vis.Light.BaseColormap;

		if (viewpoint.camera->Inventory != nullptr)
		{
			visstyle_t visstyle;
			visstyle.Alpha = vis.Alpha;
			visstyle.RenderStyle = STYLE_Count;
			visstyle.Invert = false;

			viewpoint.camera->Inventory->AlterWeaponSprite(&visstyle);

			if (!(pspr->Flags & PSPF_FORCEALPHA)) vis.Alpha = visstyle.Alpha;

			if (visstyle.RenderStyle != STYLE_Count && !(pspr->Flags & PSPF_FORCESTYLE))
			{
				vis.RenderStyle = visstyle.RenderStyle;
			}

			if (visstyle.Invert)
			{
				vis.Light.BaseColormap = &SpecialSWColormaps[INVERSECOLORMAP];
				vis.Light.ColormapNum = 0;
				noaccel = true;
			}
		}
		// If we're drawing with a special colormap, but shaders for them are disabled, do
		// not accelerate.
		if (!r_shadercolormaps && (vis.Light.BaseColormap >= &SpecialSWColormaps[0] &&
			vis.Light.BaseColormap <= &SpecialSWColormaps.Last()))
		{
			noaccel = true;
		}
		// If drawing with a BOOM colormap, disable acceleration.
		if (vis.Light.BaseColormap == &NormalLight && NormalLight.Maps != realcolormaps.Maps)
		{
			noaccel = true;
		}
		// If the main colormap has fixed lights, and this sprite is being drawn with that
		// colormap, disable acceleration so that the lights can remain fixed.
		PolyCameraLight *cameraLight = PolyCameraLight::Instance();
		if (!noaccel && cameraLight->ShaderColormap() == nullptr &&
			NormalLightHasFixedLights && vis.Light.BaseColormap == &NormalLight &&
			vis.pic->UseBasePalette())
		{
			noaccel = true;
		}
	}
	else
	{
		colormap_to_use = basecolormap;

		vis.Light.BaseColormap = basecolormap;
		vis.Light.ColormapNum = 0;
	}

	// Check for hardware-assisted 2D. If it's available, and this sprite is not
	// fuzzy, don't draw it until after the switch to 2D mode.
	if (!noaccel && renderTarget == screen && (DFrameBuffer *)screen->Accel2D)
	{
		FRenderStyle style = vis.RenderStyle;
		style.CheckFuzz();
		if (style.BlendOp != STYLEOP_Fuzz)
		{
			PolyHWAccelPlayerSprite accelSprite;

			accelSprite.pic = vis.pic;
			accelSprite.texturemid = vis.texturemid;
			accelSprite.yscale = vis.yscale;
			accelSprite.xscale = vis.xscale;

			accelSprite.Alpha = vis.Alpha;
			accelSprite.RenderStyle = vis.RenderStyle;
			accelSprite.Translation = vis.Translation;
			accelSprite.FillColor = vis.FillColor;

			accelSprite.basecolormap = colormap_to_use;
			accelSprite.x1 = x1;
			accelSprite.flip = vis.xiscale < 0;

			if (vis.Light.BaseColormap >= &SpecialSWColormaps[0] &&
				vis.Light.BaseColormap < &SpecialSWColormaps[SpecialColormaps.Size()])
			{
				accelSprite.special = &SpecialColormaps[vis.Light.BaseColormap - &SpecialSWColormaps[0]];
			}
			else if (PolyCameraLight::Instance()->ShaderColormap())
			{
				accelSprite.special = PolyCameraLight::Instance()->ShaderColormap();
			}
			else if (colormap_to_use->Color == PalEntry(255, 255, 255) &&
				colormap_to_use->Desaturate == 0)
			{
				accelSprite.overlay = colormap_to_use->Fade;
				accelSprite.overlay.a = uint8_t(vis.Light.ColormapNum * 255 / NUMCOLORMAPS);
			}
			else
			{
				accelSprite.usecolormapstyle = true;
				accelSprite.colormapstyle.Color = colormap_to_use->Color;
				accelSprite.colormapstyle.Fade = colormap_to_use->Fade;
				accelSprite.colormapstyle.Desaturate = colormap_to_use->Desaturate;
				accelSprite.colormapstyle.FadeLevel = vis.Light.ColormapNum / float(NUMCOLORMAPS);
			}

			AcceleratedSprites.Push(accelSprite);
			return;
		}
	}

	vis.Render(thread);
}

fixed_t RenderPolyPlayerSprites::LightLevelToShade(int lightlevel, bool foggy)
{
	bool nolightfade = !foggy && ((level.flags3 & LEVEL3_NOLIGHTFADE));
	if (nolightfade)
	{
		return (MAX(255 - lightlevel, 0) * NUMCOLORMAPS) << (FRACBITS - 8);
	}
	else
	{
		// Convert a light level into an unbounded colormap index (shade). Result is
		// fixed point. Why the +12? I wish I knew, but experimentation indicates it
		// is necessary in order to best reproduce Doom's original lighting.
		return (NUMCOLORMAPS * 2 * FRACUNIT) - ((lightlevel + 12) * (FRACUNIT*NUMCOLORMAPS / 128));
	}
}

/////////////////////////////////////////////////////////////////////////

void PolyNoAccelPlayerSprite::Render(PolyRenderThread *thread)
{
	if (xscale == 0 || fabs(yscale) < (1.0f / 32000.0f))
	{ // scaled to 0; can't see
		return;
	}

	RectDrawArgs args;
	args.SetStyle(RenderStyle, Alpha, FillColor, Translation, pic, false);
	args.SetLight(Light.BaseColormap, 255 - (Light.ColormapNum << 3));

	double centerY = viewheight / 2;
	double y1, y2;
	if (renderflags & RF_YFLIP)
	{
		y1 = centerY + (texturemid - pic->GetHeight()) * (-yscale);
		y2 = y1 + pic->GetHeight() * (-yscale);
	}
	else
	{
		y1 = centerY - texturemid * yscale;
		y2 = y1 + pic->GetHeight() * yscale;
	}
	args.Draw(thread, x1, x2, y1, y2, 0.0f, 1.0f, 0.0f, 1.0f);
}

/////////////////////////////////////////////////////////////////////////////

void PolyColormapLight::SetColormap(double visibility, int shade, FDynamicColormap *basecolormap, bool fullbright, bool invertColormap, bool fadeToBlack)
{
	if (fadeToBlack)
	{
		if (invertColormap) // Fade to white
		{
			basecolormap = GetSpecialLights(basecolormap->Color, MAKERGB(255, 255, 255), basecolormap->Desaturate);
			invertColormap = false;
		}
		else // Fade to black
		{
			basecolormap = GetSpecialLights(basecolormap->Color, MAKERGB(0, 0, 0), basecolormap->Desaturate);
		}
	}

	if (invertColormap)
	{
		basecolormap = GetSpecialLights(basecolormap->Color, basecolormap->Fade.InverseColor(), basecolormap->Desaturate);
	}

	PolyCameraLight *cameraLight = PolyCameraLight::Instance();
	if (cameraLight->FixedColormap())
	{
		BaseColormap = cameraLight->FixedColormap();
		ColormapNum = 0;
	}
	else if (cameraLight->FixedLightLevel() >= 0)
	{
		BaseColormap = (r_fullbrightignoresectorcolor) ? &FullNormalLight : basecolormap;
		ColormapNum = cameraLight->FixedLightLevel() >> COLORMAPSHIFT;
	}
	else if (fullbright)
	{
		BaseColormap = (r_fullbrightignoresectorcolor) ? &FullNormalLight : basecolormap;
		ColormapNum = 0;
	}
	else
	{
		BaseColormap = basecolormap;
		ColormapNum = GETPALOOKUP(visibility, shade);
	}
}
