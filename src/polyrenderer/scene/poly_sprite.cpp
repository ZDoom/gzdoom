/*
**  Handling drawing a sprite
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
#include "poly_sprite.h"
#include "polyrenderer/poly_renderer.h"
#include "polyrenderer/scene/poly_light.h"
#include "r_data/r_vanillatrans.h"

EXTERN_CVAR(Float, transsouls)
EXTERN_CVAR(Int, r_drawfuzz)
EXTERN_CVAR (Bool, r_debug_disable_vis_filter)
extern uint32_t r_renderercaps;

bool RenderPolySprite::GetLine(AActor *thing, DVector2 &left, DVector2 &right)
{
	if (IsThingCulled(thing))
		return false;

	const auto &viewpoint = PolyRenderer::Instance()->Viewpoint;
	DVector3 pos = thing->InterpolatedPosition(viewpoint.TicFrac);

	bool flipTextureX = false;
	FTexture *tex = GetSpriteTexture(thing, flipTextureX);
	if (tex == nullptr)
		return false;

	DVector2 spriteScale = thing->Scale;
	double thingxscalemul = spriteScale.X / tex->Scale.X;
	double thingyscalemul = spriteScale.Y / tex->Scale.Y;

	if (flipTextureX)
		pos.X -= (tex->GetWidth() - tex->LeftOffset) * thingxscalemul;
	else
		pos.X -= tex->LeftOffset * thingxscalemul;

	double spriteHalfWidth = thingxscalemul * tex->GetWidth() * 0.5;
	double spriteHeight = thingyscalemul * tex->GetHeight();

	pos.X += spriteHalfWidth;

	left = DVector2(pos.X - viewpoint.Sin * spriteHalfWidth, pos.Y + viewpoint.Cos * spriteHalfWidth);
	right = DVector2(pos.X + viewpoint.Sin * spriteHalfWidth, pos.Y - viewpoint.Cos * spriteHalfWidth);
	return true;
}

void RenderPolySprite::Render(const TriMatrix &worldToClip, const PolyClipPlane &clipPlane, AActor *thing, subsector_t *sub, uint32_t subsectorDepth, uint32_t stencilValue, float t1, float t2)
{
	DVector2 line[2];
	if (!GetLine(thing, line[0], line[1]))
		return;
	
	const auto &viewpoint = PolyRenderer::Instance()->Viewpoint;
	DVector3 pos = thing->InterpolatedPosition(viewpoint.TicFrac);
	pos.Z += thing->GetBobOffset(viewpoint.TicFrac);

	bool flipTextureX = false;
	FTexture *tex = GetSpriteTexture(thing, flipTextureX);
	if (tex == nullptr || tex->UseType == FTexture::TEX_Null)
		return;

	DVector2 spriteScale = thing->Scale;
	double thingxscalemul = spriteScale.X / tex->Scale.X;
	double thingyscalemul = spriteScale.Y / tex->Scale.Y;

	if (flipTextureX)
		pos.X -= (tex->GetWidth() - tex->LeftOffset) * thingxscalemul;
	else
		pos.X -= tex->LeftOffset * thingxscalemul;

	//pos.Z -= tex->TopOffset * thingyscalemul;
	pos.Z -= (tex->GetHeight() - tex->TopOffset) * thingyscalemul + thing->Floorclip;

	double spriteHalfWidth = thingxscalemul * tex->GetWidth() * 0.5;
	double spriteHeight = thingyscalemul * tex->GetHeight();

	pos.X += spriteHalfWidth;

	//double depth = 1.0;
	//visstyle_t visstyle = GetSpriteVisStyle(thing, depth);
	// Rumor has it that AlterWeaponSprite needs to be called with visstyle passed in somewhere around here..
	//R_SetColorMapLight(visstyle.BaseColormap, 0, visstyle.ColormapNum << FRACBITS);

	TriVertex *vertices = PolyRenderer::Instance()->FrameMemory.AllocMemory<TriVertex>(4);

	bool foggy = false;
	int actualextralight = foggy ? 0 : viewpoint.extralight << 4;

	std::pair<float, float> offsets[4] =
	{
		{ t1,  1.0f },
		{ t2,  1.0f },
		{ t2,  0.0f },
		{ t1,  0.0f },
	};

	DVector2 points[2] =
	{
		line[0] * (1.0 - t1) + line[1] * t1,
		line[0] * (1.0 - t2) + line[1] * t2
	};

	for (int i = 0; i < 4; i++)
	{
		auto &p = (i == 0 || i == 3) ? points[0] : points[1];

		vertices[i].x = (float)p.X;
		vertices[i].y = (float)p.Y;
		vertices[i].z = (float)(pos.Z + spriteHeight * offsets[i].second);
		vertices[i].w = 1.0f;
		vertices[i].u = (float)(offsets[i].first * tex->Scale.X);
		vertices[i].v = (float)((1.0f - offsets[i].second) * tex->Scale.Y);
		if (flipTextureX)
			vertices[i].u = 1.0f - vertices[i].u;
	}

	bool fullbrightSprite = ((thing->renderflags & RF_FULLBRIGHT) || (thing->flags5 & MF5_BRIGHT));
	int lightlevel = fullbrightSprite ? 255 : thing->Sector->lightlevel + actualextralight;

	PolyDrawArgs args;
	args.SetLight(GetColorTable(sub->sector->Colormap, sub->sector->SpecialColors[sector_t::sprites], true), lightlevel, PolyRenderer::Instance()->Light.SpriteGlobVis(foggy), fullbrightSprite);
	args.SetSubsectorDepth(subsectorDepth);
	args.SetTransform(&worldToClip);
	args.SetFaceCullCCW(true);
	args.SetStencilTestValue(stencilValue);
	args.SetWriteStencil(true, stencilValue);
	args.SetClipPlane(clipPlane);
	if ((thing->renderflags & RF_ZDOOMTRANS) && r_UseVanillaTransparency)
		args.SetStyle(LegacyRenderStyles[STYLE_Normal], 1.0f, thing->fillcolor, thing->Translation, tex, fullbrightSprite);
	else
		args.SetStyle(thing->RenderStyle, thing->Alpha, thing->fillcolor, thing->Translation, tex, fullbrightSprite);
	args.SetSubsectorDepthTest(true);
	args.SetWriteSubsectorDepth(false);
	args.SetWriteStencil(false);
	args.DrawArray(vertices, 4, PolyDrawMode::TriangleFan);
}

bool RenderPolySprite::IsThingCulled(AActor *thing)
{
	FIntCVar *cvar = thing->GetInfo()->distancecheck;
	if (cvar != nullptr && *cvar >= 0)
	{
		double dist = (thing->Pos() - PolyRenderer::Instance()->Viewpoint.Pos).LengthSquared();
		double check = (double)**cvar;
		if (dist >= check * check)
			return true;
	}

	// Don't waste time projecting sprites that are definitely not visible.
	if (thing == nullptr ||
		(thing->renderflags & RF_INVISIBLE) ||
		!thing->RenderStyle.IsVisible(thing->Alpha) ||
		!thing->IsVisibleToPlayer())
	{
		return true;
	}

	// check renderrequired vs ~r_rendercaps, if anything matches we don't support that feature,
	// check renderhidden vs r_rendercaps, if anything matches we do support that feature and should hide it.
	if (!r_debug_disable_vis_filter && (!!(thing->RenderRequired & ~r_renderercaps)) ||
		(!!(thing->RenderHidden & r_renderercaps)))
		return true;

	return false;
}

FTexture *RenderPolySprite::GetSpriteTexture(AActor *thing, /*out*/ bool &flipX)
{
	const auto &viewpoint = PolyRenderer::Instance()->Viewpoint;
	flipX = false;

	if (thing->renderflags & RF_FLATSPRITE)
		return nullptr;	// do not draw flat sprites.

	if (thing->picnum.isValid())
	{
		FTexture *tex = TexMan(thing->picnum);
		if (tex->UseType == FTexture::TEX_Null)
		{
			return nullptr;
		}

		if (tex->Rotations != 0xFFFF)
		{
			// choose a different rotation based on player view
			spriteframe_t *sprframe = &SpriteFrames[tex->Rotations];
			DVector3 pos = thing->InterpolatedPosition(viewpoint.TicFrac);
			pos.Z += thing->GetBobOffset(viewpoint.TicFrac);
			DAngle ang = (pos - viewpoint.Pos).Angle();
			angle_t rot;
			if (sprframe->Texture[0] == sprframe->Texture[1])
			{
				rot = (ang - thing->Angles.Yaw + 45.0 / 2 * 9).BAMs() >> 28;
			}
			else
			{
				rot = (ang - thing->Angles.Yaw + (45.0 / 2 * 9 - 180.0 / 16)).BAMs() >> 28;
			}
			flipX = (sprframe->Flip & (1 << rot)) != 0;
			tex = TexMan[sprframe->Texture[rot]];	// Do not animate the rotation
		}
		return tex;
	}
	else
	{
		// decide which texture to use for the sprite
		int spritenum = thing->sprite;
		if (spritenum >= (signed)sprites.Size() || spritenum < 0)
			return nullptr;

		spritedef_t *sprdef = &sprites[spritenum];
		if (thing->frame >= sprdef->numframes)
		{
			// If there are no frames at all for this sprite, don't draw it.
			return nullptr;
		}
		else
		{
			//picnum = SpriteFrames[sprdef->spriteframes + thing->frame].Texture[0];
			// choose a different rotation based on player view

			DVector3 pos = thing->InterpolatedPosition(viewpoint.TicFrac);
			pos.Z += thing->GetBobOffset(viewpoint.TicFrac);
			DAngle ang = (pos - viewpoint.Pos).Angle();

			DAngle sprangle = thing->GetSpriteAngle((pos - viewpoint.Pos).Angle(), viewpoint.TicFrac);
			FTextureID tex = sprdef->GetSpriteFrame(thing->frame, -1, sprangle, &flipX);
			if (!tex.isValid()) return nullptr;
			return TexMan[tex];
		}
	}
}
