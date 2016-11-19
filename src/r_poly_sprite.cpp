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
#include "r_poly_sprite.h"
#include "r_poly.h"

void RenderPolySprite::Render(const TriMatrix &worldToClip, AActor *thing, subsector_t *sub, uint32_t subsectorDepth)
{
	if (IsThingCulled(thing))
		return;

	DVector3 pos = thing->InterpolatedPosition(r_TicFracF);
	pos.Z += thing->GetBobOffset(r_TicFracF);

	bool flipTextureX = false;
	FTexture *tex = GetSpriteTexture(thing, flipTextureX);
	if (tex == nullptr)
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

	DVector2 points[2] =
	{
		{ pos.X - ViewSin * spriteHalfWidth, pos.Y + ViewCos * spriteHalfWidth },
		{ pos.X + ViewSin * spriteHalfWidth, pos.Y - ViewCos * spriteHalfWidth }
	};

	// Is this sprite inside? (To do: clip the points)
	for (int i = 0; i < 2; i++)
	{
		for (uint32_t i = 0; i < sub->numlines; i++)
		{
			seg_t *line = &sub->firstline[i];
			double nx = line->v1->fY() - line->v2->fY();
			double ny = line->v2->fX() - line->v1->fX();
			double d = -(line->v1->fX() * nx + line->v1->fY() * ny);
			if (pos.X * nx + pos.Y * ny + d > 0.0)
				return;
		}
	}

	//double depth = 1.0;
	//visstyle_t visstyle = GetSpriteVisStyle(thing, depth);
	// Rumor has it that AlterWeaponSprite needs to be called with visstyle passed in somewhere around here..
	//R_SetColorMapLight(visstyle.BaseColormap, 0, visstyle.ColormapNum << FRACBITS);

	TriVertex *vertices = PolyVertexBuffer::GetVertices(4);
	if (!vertices)
		return;

	bool foggy = false;
	int actualextralight = foggy ? 0 : extralight << 4;

	std::pair<float, float> offsets[4] =
	{
		{ 0.0f,  1.0f },
		{ 1.0f,  1.0f },
		{ 1.0f,  0.0f },
		{ 0.0f,  0.0f },
	};

	for (int i = 0; i < 4; i++)
	{
		auto &p = (i == 0 || i == 3) ? points[0] : points[1];

		vertices[i].x = (float)p.X;
		vertices[i].y = (float)p.Y;
		vertices[i].z = (float)(pos.Z + spriteHeight * offsets[i].second);
		vertices[i].w = 1.0f;
		vertices[i].varying[0] = (float)(offsets[i].first * tex->Scale.X);
		vertices[i].varying[1] = (float)((1.0f - offsets[i].second) * tex->Scale.Y);
		if (flipTextureX)
			vertices[i].varying[0] = 1.0f - vertices[i].varying[0];
	}

	bool fullbrightSprite = ((thing->renderflags & RF_FULLBRIGHT) || (thing->flags5 & MF5_BRIGHT));

	TriUniforms uniforms;
	uniforms.objectToClip = worldToClip;
	if (fullbrightSprite || fixedlightlev >= 0 || fixedcolormap)
	{
		uniforms.light = 256;
		uniforms.flags = TriUniforms::fixed_light;
	}
	else
	{
		uniforms.light = (uint32_t)((thing->Sector->lightlevel + actualextralight) / 255.0f * 256.0f);
		uniforms.flags = 0;
	}
	uniforms.subsectorDepth = subsectorDepth;

	PolyDrawArgs args;
	args.uniforms = uniforms;
	args.vinput = vertices;
	args.vcount = 4;
	args.mode = TriangleDrawMode::Fan;
	args.ccw = true;
	args.stenciltestvalue = 0;
	args.stencilwritevalue = 1;
	args.SetTexture(tex, thing->Translation);

	if (args.translation)
		PolyTriangleDrawer::draw(args, TriDrawVariant::DrawSubsector, TriBlendMode::TranslateAlphaBlend);
	else
		PolyTriangleDrawer::draw(args, TriDrawVariant::DrawSubsector, TriBlendMode::AlphaBlend);
}

bool RenderPolySprite::IsThingCulled(AActor *thing)
{
	FIntCVar *cvar = thing->GetClass()->distancecheck;
	if (cvar != nullptr && *cvar >= 0)
	{
		double dist = (thing->Pos() - ViewPos).LengthSquared();
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

	return false;
}

visstyle_t RenderPolySprite::GetSpriteVisStyle(AActor *thing, double z)
{
	visstyle_t visstyle;

	bool foggy = false;
	int actualextralight = foggy ? 0 : extralight << 4;
	int spriteshade = LIGHT2SHADE(thing->Sector->lightlevel + actualextralight);

	visstyle.RenderStyle = thing->RenderStyle;
	visstyle.Alpha = float(thing->Alpha);
	visstyle.ColormapNum = 0;

	// The software renderer cannot invert the source without inverting the overlay
	// too. That means if the source is inverted, we need to do the reverse of what
	// the invert overlay flag says to do.
	bool invertcolormap = (visstyle.RenderStyle.Flags & STYLEF_InvertOverlay) != 0;

	if (visstyle.RenderStyle.Flags & STYLEF_InvertSource)
	{
		invertcolormap = !invertcolormap;
	}

	FDynamicColormap *mybasecolormap = thing->Sector->ColorMap;

	// Sprites that are added to the scene must fade to black.
	if (visstyle.RenderStyle == LegacyRenderStyles[STYLE_Add] && mybasecolormap->Fade != 0)
	{
		mybasecolormap = GetSpecialLights(mybasecolormap->Color, 0, mybasecolormap->Desaturate);
	}

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

	// get light level
	if (fixedcolormap != nullptr)
	{ // fixed map
		visstyle.BaseColormap = fixedcolormap;
		visstyle.ColormapNum = 0;
	}
	else
	{
		if (invertcolormap)
		{
			mybasecolormap = GetSpecialLights(mybasecolormap->Color, mybasecolormap->Fade.InverseColor(), mybasecolormap->Desaturate);
		}
		if (fixedlightlev >= 0)
		{
			visstyle.BaseColormap = mybasecolormap;
			visstyle.ColormapNum = fixedlightlev >> COLORMAPSHIFT;
		}
		else if (!foggy && ((thing->renderflags & RF_FULLBRIGHT) || (thing->flags5 & MF5_BRIGHT)))
		{ // full bright
			visstyle.BaseColormap = mybasecolormap;
			visstyle.ColormapNum = 0;
		}
		else
		{ // diminished light
			double minz = double((2048 * 4) / double(1 << 20));
			visstyle.ColormapNum = GETPALOOKUP(r_SpriteVisibility / MAX(z, minz), spriteshade);
			visstyle.BaseColormap = mybasecolormap;
		}
	}

	return visstyle;
}

FTexture *RenderPolySprite::GetSpriteTexture(AActor *thing, /*out*/ bool &flipX)
{
	flipX = false;
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
			DVector3 pos = thing->InterpolatedPosition(r_TicFracF);
			pos.Z += thing->GetBobOffset(r_TicFracF);
			DAngle ang = (pos - ViewPos).Angle();
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
			spriteframe_t *sprframe = &SpriteFrames[sprdef->spriteframes + thing->frame];
			DVector3 pos = thing->InterpolatedPosition(r_TicFracF);
			pos.Z += thing->GetBobOffset(r_TicFracF);
			DAngle ang = (pos - ViewPos).Angle();
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
			return TexMan[sprframe->Texture[rot]];	// Do not animate the rotation
		}
	}
}
