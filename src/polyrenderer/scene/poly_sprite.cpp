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
#include "polyrenderer/math/poly_intersection.h"
#include "swrenderer/scene/r_light.h"

EXTERN_CVAR(Float, transsouls)
EXTERN_CVAR(Int, r_drawfuzz)

bool RenderPolySprite::GetLine(AActor *thing, DVector2 &left, DVector2 &right)
{
	if (IsThingCulled(thing))
		return false;

	DVector3 pos = thing->InterpolatedPosition(r_TicFracF);

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

	left = DVector2(pos.X - ViewSin * spriteHalfWidth, pos.Y + ViewCos * spriteHalfWidth);
	right = DVector2(pos.X + ViewSin * spriteHalfWidth, pos.Y - ViewCos * spriteHalfWidth);
	return true;
}

void RenderPolySprite::Render(const TriMatrix &worldToClip, const Vec4f &clipPlane, AActor *thing, subsector_t *sub, uint32_t subsectorDepth, uint32_t stencilValue, float t1, float t2)
{
	DVector2 line[2];
	if (!GetLine(thing, line[0], line[1]))
		return;
	
	DVector3 pos = thing->InterpolatedPosition(r_TicFracF);
	pos.Z += thing->GetBobOffset(r_TicFracF);

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

	TriVertex *vertices = PolyVertexBuffer::GetVertices(4);
	if (!vertices)
		return;

	bool foggy = false;
	int actualextralight = foggy ? 0 : extralight << 4;

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
		vertices[i].varying[0] = (float)(offsets[i].first * tex->Scale.X);
		vertices[i].varying[1] = (float)((1.0f - offsets[i].second) * tex->Scale.Y);
		if (flipTextureX)
			vertices[i].varying[0] = 1.0f - vertices[i].varying[0];
	}

	bool fullbrightSprite = ((thing->renderflags & RF_FULLBRIGHT) || (thing->flags5 & MF5_BRIGHT));
	swrenderer::CameraLight *cameraLight = swrenderer::CameraLight::Instance();

	PolyDrawArgs args;
	args.uniforms.globvis = (float)swrenderer::LightVisibility::Instance()->SpriteGlobVis();
	args.uniforms.flags = 0;
	if (fullbrightSprite || cameraLight->FixedLightLevel() >= 0 || cameraLight->FixedColormap())
	{
		args.uniforms.light = 256;
		args.uniforms.flags |= TriUniforms::fixed_light;
	}
	else
	{
		args.uniforms.light = (uint32_t)((thing->Sector->lightlevel + actualextralight) / 255.0f * 256.0f);
	}
	args.uniforms.subsectorDepth = subsectorDepth;

	args.objectToClip = &worldToClip;
	args.vinput = vertices;
	args.vcount = 4;
	args.mode = TriangleDrawMode::Fan;
	args.ccw = true;
	args.stenciltestvalue = stencilValue;
	args.stencilwritevalue = stencilValue;
	args.SetTexture(tex, thing->Translation);
	args.SetColormap(sub->sector->ColorMap);
	args.SetClipPlane(clipPlane.x, clipPlane.y, clipPlane.z, clipPlane.w);

	TriBlendMode blendmode;
	
	if (thing->RenderStyle == LegacyRenderStyles[STYLE_Normal] ||
		 (r_drawfuzz == 0 && thing->RenderStyle == LegacyRenderStyles[STYLE_OptFuzzy]))
	{
		args.uniforms.destalpha = 0;
		args.uniforms.srcalpha = 256;
		blendmode = args.translation ? TriBlendMode::TranslateAdd : TriBlendMode::Add;
	}
	else if (thing->RenderStyle == LegacyRenderStyles[STYLE_Add] && fullbrightSprite && thing->Alpha == 1.0 && args.translation == nullptr)
	{
		args.uniforms.destalpha = 256;
		args.uniforms.srcalpha = 256;
		blendmode = TriBlendMode::AddSrcColorOneMinusSrcColor;
	}
	else if (thing->RenderStyle == LegacyRenderStyles[STYLE_Add])
	{
		args.uniforms.destalpha = (uint32_t)(1.0 * 256);
		args.uniforms.srcalpha = (uint32_t)(thing->Alpha * 256);
		blendmode = args.translation ? TriBlendMode::TranslateAdd : TriBlendMode::Add;
	}
	else if (thing->RenderStyle == LegacyRenderStyles[STYLE_Subtract])
	{
		args.uniforms.destalpha = (uint32_t)(1.0 * 256);
		args.uniforms.srcalpha = (uint32_t)(thing->Alpha * 256);
		blendmode = args.translation ? TriBlendMode::TranslateRevSub : TriBlendMode::RevSub;
	}
	else if (thing->RenderStyle == LegacyRenderStyles[STYLE_SoulTrans])
	{
		args.uniforms.destalpha = (uint32_t)(256 - transsouls * 256);
		args.uniforms.srcalpha = (uint32_t)(transsouls * 256);
		blendmode = args.translation ? TriBlendMode::TranslateAdd : TriBlendMode::Add;
	}
	else if (thing->RenderStyle == LegacyRenderStyles[STYLE_Fuzzy] ||
		 (r_drawfuzz == 2 && thing->RenderStyle == LegacyRenderStyles[STYLE_OptFuzzy]))
	{	// NYI - Fuzzy - for now, just a copy of "Shadow"
		args.uniforms.destalpha = 160;
		args.uniforms.srcalpha = 0;
		blendmode = args.translation ? TriBlendMode::TranslateAdd : TriBlendMode::Add;
	}
	else if (thing->RenderStyle == LegacyRenderStyles[STYLE_Shadow] ||
		 (r_drawfuzz == 1 && thing->RenderStyle == LegacyRenderStyles[STYLE_OptFuzzy]))
	{
		args.uniforms.destalpha = 160;
		args.uniforms.srcalpha = 0;
		blendmode = args.translation ? TriBlendMode::TranslateAdd : TriBlendMode::Add;
	}
	else if (thing->RenderStyle == LegacyRenderStyles[STYLE_TranslucentStencil])
	{
		args.uniforms.destalpha = (uint32_t)(256 - thing->Alpha * 256);
		args.uniforms.srcalpha = (uint32_t)(thing->Alpha * 256);
		args.uniforms.color = 0xff000000 | thing->fillcolor;
		blendmode = TriBlendMode::Stencil;
	}
	else if (thing->RenderStyle == LegacyRenderStyles[STYLE_AddStencil])
	{
		args.uniforms.destalpha = 256;
		args.uniforms.srcalpha = (uint32_t)(thing->Alpha * 256);
		args.uniforms.color = 0xff000000 | thing->fillcolor;
		blendmode = TriBlendMode::Stencil;
	}
	else if (thing->RenderStyle == LegacyRenderStyles[STYLE_Shaded])
	{
		args.uniforms.srcalpha = (uint32_t)(thing->Alpha * 256);
		args.uniforms.destalpha = 256 - args.uniforms.srcalpha;
		args.uniforms.color = 0;
		blendmode = TriBlendMode::Shaded;
	}
	else if (thing->RenderStyle == LegacyRenderStyles[STYLE_AddShaded])
	{
		args.uniforms.destalpha = 256;
		args.uniforms.srcalpha = (uint32_t)(thing->Alpha * 256);
		args.uniforms.color = 0;
		blendmode = TriBlendMode::Shaded;
	}
	else
	{
		args.uniforms.destalpha = (uint32_t)(256 - thing->Alpha * 256);
		args.uniforms.srcalpha = (uint32_t)(thing->Alpha * 256);
		blendmode = args.translation ? TriBlendMode::TranslateAdd : TriBlendMode::Add;
	}
	
	if (blendmode == TriBlendMode::Shaded)
	{
		args.SetTexture(tex, thing->Translation, true);
	}
	
	if (!swrenderer::RenderViewport::Instance()->RenderTarget->IsBgra())
	{
		uint32_t r = (args.uniforms.color >> 16) & 0xff;
		uint32_t g = (args.uniforms.color >> 8) & 0xff;
		uint32_t b = args.uniforms.color & 0xff;
		args.uniforms.color = RGB32k.RGB[r >> 3][g >> 3][b >> 3];

		if (blendmode == TriBlendMode::Sub) // Sub crashes in pal mode for some weird reason.
			blendmode = TriBlendMode::Add;
	}

	args.subsectorTest = true;
	args.writeSubsector = false;
	args.writeStencil = false;
	args.blendmode = blendmode;
	PolyTriangleDrawer::draw(args);
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

#if 0
visstyle_t RenderPolySprite::GetSpriteVisStyle(AActor *thing, double z)
{
	visstyle_t visstyle;

	bool foggy = false;
	int actualextralight = foggy ? 0 : extralight << 4;
	int spriteshade = LIGHT2SHADE(thing->Sector->lightlevel + actualextralight);

	FRenderStyle RenderStyle;
	RenderStyle = thing->RenderStyle;
	float Alpha = float(thing->Alpha);
	int ColormapNum = 0;

	// The software renderer cannot invert the source without inverting the overlay
	// too. That means if the source is inverted, we need to do the reverse of what
	// the invert overlay flag says to do.
	bool invertcolormap = (RenderStyle.Flags & STYLEF_InvertOverlay) != 0;

	if (RenderStyle.Flags & STYLEF_InvertSource)
	{
		invertcolormap = !invertcolormap;
	}

	FDynamicColormap *mybasecolormap = thing->Sector->ColorMap;

	// Sprites that are added to the scene must fade to black.
	if (RenderStyle == LegacyRenderStyles[STYLE_Add] && mybasecolormap->Fade != 0)
	{
		mybasecolormap = GetSpecialLights(mybasecolormap->Color, 0, mybasecolormap->Desaturate);
	}

	if (RenderStyle.Flags & STYLEF_FadeToBlack)
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
	if (swrenderer::fixedcolormap != nullptr)
	{ // fixed map
		BaseColormap = swrenderer::fixedcolormap;
		ColormapNum = 0;
	}
	else
	{
		if (invertcolormap)
		{
			mybasecolormap = GetSpecialLights(mybasecolormap->Color, mybasecolormap->Fade.InverseColor(), mybasecolormap->Desaturate);
		}
		if (swrenderer::fixedlightlev >= 0)
		{
			BaseColormap = mybasecolormap;
			ColormapNum = swrenderer::fixedlightlev >> COLORMAPSHIFT;
		}
		else if (!foggy && ((thing->renderflags & RF_FULLBRIGHT) || (thing->flags5 & MF5_BRIGHT)))
		{ // full bright
			BaseColormap = mybasecolormap;
			ColormapNum = 0;
		}
		else
		{ // diminished light
			double minz = double((2048 * 4) / double(1 << 20));
			ColormapNum = GETPALOOKUP(swrenderer::r_SpriteVisibility / MAX(z, minz), spriteshade);
			BaseColormap = mybasecolormap;
		}
	}

	return visstyle;
}
#endif

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
