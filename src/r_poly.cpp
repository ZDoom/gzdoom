/*
**  Experimental Doom software renderer
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
#include "r_poly.h"
#include "r_draw.h"
#include "r_plane.h" // for yslope
#include "r_sky.h" // for skyflatnum
#include "r_things.h" // for pspritexscale

EXTERN_CVAR(Bool, r_drawplayersprites)
EXTERN_CVAR(Bool, r_deathcamera)
EXTERN_CVAR(Bool, st_scale)

CVAR(Bool, r_debug_cull, 0, 0)

/////////////////////////////////////////////////////////////////////////////

void RenderPolyBsp::Render()
{
	PolyVertexBuffer::Clear();
	SolidSegments.clear();
	SolidSegments.reserve(MAXWIDTH / 2 + 2);
	SolidSegments.push_back({ -0x7fff, 0 });
	SolidSegments.push_back({ viewwidth, 0x7fff });

	SectorSpriteRanges.clear();
	SectorSpriteRanges.resize(numsectors);
	SortedSprites.clear();

	// Perspective correct:
	float ratio = WidescreenRatio;
	float fovratio = (WidescreenRatio >= 1.3f) ? 1.333333f : ratio;
	float fovy = (float)(2 * DAngle::ToDegrees(atan(tan(FieldOfView.Radians() / 2) / fovratio)).Degrees);
	TriMatrix worldToView =
		TriMatrix::scale(1.0f, (float)YaspectMul, 1.0f) *
		TriMatrix::rotate((float)ViewPitch.Radians(), 1.0f, 0.0f, 0.0f) *
		TriMatrix::rotate((float)(ViewAngle - 90).Radians(), 0.0f, -1.0f, 0.0f) *
		TriMatrix::swapYZ() *
		TriMatrix::translate((float)-ViewPos.X, (float)-ViewPos.Y, (float)-ViewPos.Z);
	worldToClip = TriMatrix::perspective(fovy, ratio, 5.0f, 65535.0f) * worldToView;

	// Y shearing like the Doom renderer:
	//worldToClip = TriMatrix::viewToClip() * TriMatrix::worldToView();

	// Cull front to back (ok, so we dont cull yet, but we should during this!):
	if (numnodes == 0)
		PvsSectors.push_back(subsectors); // RenderSubsector(subsectors);
	else
		RenderNode(nodes + numnodes - 1);	// The head node is the last node output.

	static PolySkyDome skydome;
	skydome.Render(worldToClip);

	// Render back to front (we don't have a zbuffer at the moment, sniff!):
	if (!r_debug_cull)
	{
		for (auto it = PvsSectors.rbegin(); it != PvsSectors.rend(); ++it)
			RenderSubsector(*it);
	}
	else
	{
		for (auto it = PvsSectors.begin(); it != PvsSectors.end(); ++it)
			RenderSubsector(*it);
	}

	RenderPlayerSprites();
	DrawerCommandQueue::WaitForWorkers();
	RenderScreenSprites(); // To do: should be called by FSoftwareRenderer::DrawRemainingPlayerSprites instead of here
}

void RenderPolyBsp::RenderScreenSprites()
{
	for (auto &sprite : ScreenSprites)
		sprite.Render();
}

TriVertex RenderPolyBsp::PlaneVertex(vertex_t *v1, sector_t *sector, const secplane_t &plane)
{
	TriVertex v;
	v.x = (float)v1->fPos().X;
	v.y = (float)v1->fPos().Y;
	v.z = (float)plane.ZatPoint(v1);
	v.w = 1.0f;
	v.varying[0] = v.x / 64.0f;
	v.varying[1] = v.y / 64.0f;

	/*
	double vis = r_FloorVisibility / (plane.Zat0() - ViewPos.Z);
	if (fixedlightlev >= 0)
		R_SetDSColorMapLight(sector->ColorMap, 0, FIXEDLIGHT2SHADE(fixedlightlev));
	else if (fixedcolormap)
		R_SetDSColorMapLight(fixedcolormap, 0, 0);
	else
		R_SetDSColorMapLight(sector->ColorMap, (float)(vis * fabs(CenterY - y)), LIGHT2SHADE(sector->lightlevel));
	*/

	return v;
}

void RenderPolyBsp::RenderSubsector(subsector_t *sub)
{
	sector_t *frontsector = sub->sector;
	frontsector->MoreFlags |= SECF_DRAWN;

	for (uint32_t i = 0; i < sub->numlines; i++)
	{
		seg_t *line = &sub->firstline[i];
		if (line->sidedef == NULL || !(line->sidedef->Flags & WALLF_POLYOBJ))
			AddLine(line, frontsector);
	}

	FTextureID floorpicnum = frontsector->GetTexture(sector_t::floor);
	FTexture *floortex = TexMan(floorpicnum);
	if (floortex->UseType != FTexture::TEX_Null && floorpicnum != skyflatnum)
	{
		TriVertex *vertices = PolyVertexBuffer::GetVertices(sub->numlines);
		if (!vertices)
			return;

		for (uint32_t i = 0; i < sub->numlines; i++)
		{
			seg_t *line = &sub->firstline[i];
			vertices[i] = PlaneVertex(line->v1, frontsector, frontsector->floorplane);
		}

		TriUniforms uniforms;
		uniforms.objectToClip = worldToClip;
		uniforms.light = (uint32_t)(frontsector->lightlevel / 255.0f * 256.0f);
		if (fixedlightlev >= 0)
			uniforms.light = (uint32_t)(fixedlightlev / 255.0f * 256.0f);
		else if (fixedcolormap)
			uniforms.light = 256;
		uniforms.flags = 0;
		PolyTriangleDrawer::draw(uniforms, vertices, sub->numlines, TriangleDrawMode::Fan, true, 0, viewwidth, 0, viewheight, floortex);
	}

	FTextureID ceilpicnum = frontsector->GetTexture(sector_t::ceiling);
	FTexture *ceiltex = TexMan(ceilpicnum);
	if (ceiltex->UseType != FTexture::TEX_Null && ceilpicnum != skyflatnum)
	{
		TriVertex *vertices = PolyVertexBuffer::GetVertices(sub->numlines);
		if (!vertices)
			return;

		for (uint32_t i = 0; i < sub->numlines; i++)
		{
			seg_t *line = &sub->firstline[i];
			vertices[sub->numlines - 1 - i] = PlaneVertex(line->v1, frontsector, frontsector->ceilingplane);
		}

		TriUniforms uniforms;
		uniforms.objectToClip = worldToClip;
		uniforms.light = (uint32_t)(frontsector->lightlevel / 255.0f * 256.0f);
		if (fixedlightlev >= 0)
			uniforms.light = (uint32_t)(fixedlightlev / 255.0f * 256.0f);
		else if (fixedcolormap)
			uniforms.light = 256;
		uniforms.flags = 0;
		PolyTriangleDrawer::draw(uniforms, vertices, sub->numlines, TriangleDrawMode::Fan, true, 0, viewwidth, 0, viewheight, ceiltex);
	}

	SpriteRange sprites = GetSpritesForSector(sub->sector);
	for (int i = 0; i < sprites.Count; i++)
	{
		AActor *thing = SortedSprites[sprites.Start + i].Thing;
		if ((thing->renderflags & RF_SPRITETYPEMASK) == RF_WALLSPRITE)
			AddWallSprite(thing, sub);
		else
			AddSprite(thing, sub);
	}
}

SpriteRange RenderPolyBsp::GetSpritesForSector(sector_t *sector)
{
	if (SectorSpriteRanges.size() < sector->sectornum || sector->sectornum < 0)
		return SpriteRange();

	auto &range = SectorSpriteRanges[sector->sectornum];
	if (range.Start == -1)
	{
		range.Start = (int)SortedSprites.size();
		range.Count = 0;
		for (AActor *thing = sector->thinglist; thing != nullptr; thing = thing->snext)
		{
			SortedSprites.push_back({ thing, (thing->Pos() - ViewPos).LengthSquared() });
			range.Count++;
		}
		std::stable_sort(SortedSprites.begin() + range.Start, SortedSprites.begin() + range.Start + range.Count);
	}
	return range;
}

void RenderPolyBsp::AddLine(seg_t *line, sector_t *frontsector)
{
	// Reject lines not facing viewer
	DVector2 pt1 = line->v1->fPos() - ViewPos;
	DVector2 pt2 = line->v2->fPos() - ViewPos;
	if (pt1.Y * (pt1.X - pt2.X) + pt1.X * (pt2.Y - pt1.Y) >= 0)
		return;

	double frontceilz1 = frontsector->ceilingplane.ZatPoint(line->v1);
	double frontfloorz1 = frontsector->floorplane.ZatPoint(line->v1);
	double frontceilz2 = frontsector->ceilingplane.ZatPoint(line->v2);
	double frontfloorz2 = frontsector->floorplane.ZatPoint(line->v2);

	//VisiblePlaneKey ceilingPlaneKey(frontsector->GetTexture(sector_t::ceiling), frontsector->ColorMap, frontsector->lightlevel, frontsector->ceilingplane, frontsector->planes[sector_t::ceiling].xform);
	//VisiblePlaneKey floorPlaneKey(frontsector->GetTexture(sector_t::floor), frontsector->ColorMap, frontsector->lightlevel, frontsector->floorplane, frontsector->planes[sector_t::floor].xform);

	RenderPolyWall wall;
	wall.Line = line;
	wall.Colormap = frontsector->ColorMap;
	wall.Masked = false;

	if (line->backsector == nullptr)
	{
		wall.SetCoords(line->v1->fPos(), line->v2->fPos(), frontceilz1, frontfloorz1, frontceilz2, frontfloorz2);
		wall.TopZ = frontceilz1;
		wall.BottomZ = frontfloorz1;
		wall.UnpeggedCeil = frontceilz1;
		wall.Texpart = side_t::mid;
		wall.Render(worldToClip);
	}
	else
	{
		sector_t *backsector = (line->backsector != line->frontsector) ? line->backsector : line->frontsector;

		double backceilz1 = backsector->ceilingplane.ZatPoint(line->v1);
		double backfloorz1 = backsector->floorplane.ZatPoint(line->v1);
		double backceilz2 = backsector->ceilingplane.ZatPoint(line->v2);
		double backfloorz2 = backsector->floorplane.ZatPoint(line->v2);

		double topceilz1 = frontceilz1;
		double topceilz2 = frontceilz2;
		double topfloorz1 = MIN(backceilz1, frontceilz1);
		double topfloorz2 = MIN(backceilz2, frontceilz2);
		double bottomceilz1 = MAX(frontfloorz1, backfloorz1);
		double bottomceilz2 = MAX(frontfloorz2, backfloorz2);
		double bottomfloorz1 = frontfloorz1;
		double bottomfloorz2 = frontfloorz2;
		double middleceilz1 = topfloorz1;
		double middleceilz2 = topfloorz2;
		double middlefloorz1 = MIN(bottomceilz1, middleceilz1);
		double middlefloorz2 = MIN(bottomceilz2, middleceilz2);

		bool bothSkyCeiling = frontsector->GetTexture(sector_t::ceiling) == skyflatnum && backsector->GetTexture(sector_t::ceiling) == skyflatnum;
		bool bothSkyFloor = frontsector->GetTexture(sector_t::floor) == skyflatnum && backsector->GetTexture(sector_t::floor) == skyflatnum;

		if ((topceilz1 > topfloorz1 || topceilz2 > topfloorz2) && !bothSkyCeiling && line->sidedef)
		{
			wall.SetCoords(line->v1->fPos(), line->v2->fPos(), topceilz1, topfloorz1, topceilz2, topfloorz2);
			wall.TopZ = topceilz1;
			wall.BottomZ = topfloorz1;
			wall.UnpeggedCeil = topceilz1;
			wall.Texpart = side_t::top;
			wall.Render(worldToClip);
		}

		if ((bottomfloorz1 < bottomceilz1 || bottomfloorz2 < bottomceilz2) && !bothSkyFloor && line->sidedef)
		{
			wall.SetCoords(line->v1->fPos(), line->v2->fPos(), bottomceilz1, bottomfloorz2, bottomceilz2, bottomfloorz2);
			wall.TopZ = bottomceilz1;
			wall.BottomZ = bottomfloorz2;
			wall.UnpeggedCeil = topceilz1;
			wall.Texpart = side_t::bottom;
			wall.Render(worldToClip);
		}

		if (line->sidedef)
		{
			FTexture *midtex = TexMan(line->sidedef->GetTexture(side_t::mid), true);
			if (midtex && midtex->UseType != FTexture::TEX_Null)
			{
				wall.SetCoords(line->v1->fPos(), line->v2->fPos(), middleceilz1, middlefloorz1, middleceilz2, middlefloorz2);
				wall.TopZ = middleceilz1;
				wall.BottomZ = middlefloorz1;
				wall.UnpeggedCeil = topceilz1;
				wall.Texpart = side_t::mid;
				wall.Masked = true;
				wall.Render(worldToClip);
			}
		}
	}
}

bool RenderPolyBsp::IsThingCulled(AActor *thing)
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

void RenderPolyBsp::AddSprite(AActor *thing, subsector_t *sub)
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

	TriUniforms uniforms;
	uniforms.objectToClip = worldToClip;
	uniforms.light = (uint32_t)((thing->Sector->lightlevel + actualextralight) / 255.0f * 256.0f);
	uniforms.flags = 0;
	PolyTriangleDrawer::draw(uniforms, vertices, 4, TriangleDrawMode::Fan, true, 0, viewwidth, 0, viewheight, tex);
}

void RenderPolyBsp::AddWallSprite(AActor *thing, subsector_t *sub)
{
	if (IsThingCulled(thing))
		return;
}

visstyle_t RenderPolyBsp::GetSpriteVisStyle(AActor *thing, double z)
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
	if (fixedcolormap != NULL)
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

FTexture *RenderPolyBsp::GetSpriteTexture(AActor *thing, /*out*/ bool &flipX)
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

void RenderPolyBsp::RenderNode(void *node)
{
	while (!((size_t)node & 1))  // Keep going until found a subsector
	{
		node_t *bsp = (node_t *)node;

		// Decide which side the view point is on.
		int side = PointOnSide(ViewPos, bsp);

		// Recursively divide front space (toward the viewer).
		RenderNode(bsp->children[side]);

		// Possibly divide back space (away from the viewer).
		side ^= 1;
		if (!CheckBBox(bsp->bbox[side]))
			return;

		node = bsp->children[side];
	}

	// Mark that we need to render this
	subsector_t *sub = (subsector_t *)((BYTE *)node - 1);
	PvsSectors.push_back(sub);

	// Update culling info for further bsp clipping
	for (uint32_t i = 0; i < sub->numlines; i++)
	{
		seg_t *line = &sub->firstline[i];
		if ((line->sidedef == nullptr || !(line->sidedef->Flags & WALLF_POLYOBJ)) && line->backsector == nullptr)
		{
			int sx1, sx2;
			if (GetSegmentRangeForLine(line->v1->fX(), line->v1->fY(), line->v2->fX(), line->v2->fY(), sx1, sx2))
			{
				MarkSegmentCulled(sx1, sx2);
			}
		}
	}
}

void RenderPolyBsp::RenderPlayerSprites()
{
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
			RenderPlayerSprite(sprite, camera, bobx, boby, wx, wy, r_TicFracF);
		}
	}
}

void RenderPolyBsp::RenderPlayerSprite(DPSprite *sprite, AActor *owner, float bobx, float boby, double wx, double wy, double ticfrac)
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

	// calculate edges of the shape
	double tx = sx - BaseXCenter;

	tx -= tex->GetScaledLeftOffset();
	int x1 = xs_RoundToInt(CenterX + tx * pspritexscale);

	// off the right side
	if (x1 > viewwidth)
		return;

	tx += tex->GetScaledWidth();
	int x2 = xs_RoundToInt(CenterX + tx * pspritexscale);

	// off the left side
	if (x2 <= 0)
		return;

	double texturemid = (BaseYCenter - sy) * tex->Scale.Y + tex->TopOffset;

	// Adjust PSprite for fullscreen views
	if (camera->player && (RenderTarget != screen || viewheight == RenderTarget->GetHeight() || (RenderTarget->GetWidth() > (BaseXCenter * 2) && !st_scale)))
	{
		AWeapon *weapon = dyn_cast<AWeapon>(sprite->GetCaller());
		if (weapon != nullptr && weapon->YAdjust != 0)
		{
			if (RenderTarget != screen || viewheight == RenderTarget->GetHeight())
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
	visstyle.ColormapNum = GETPALOOKUP(r_SpriteVisibility / minz, spriteshade);

	if (sprite->GetID() < PSP_TARGETCENTER)
	{
		// Lots of complicated style and noaccel stuff
	}

	// Check for hardware-assisted 2D. If it's available, and this sprite is not
	// fuzzy, don't draw it until after the switch to 2D mode.
	if (!noaccel && RenderTarget == screen && (DFrameBuffer *)screen->Accel2D)
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

	//R_DrawVisSprite(vis);
}

bool RenderPolyBsp::IsSegmentCulled(int x1, int x2) const
{
	int next = 0;
	while (SolidSegments[next].X2 <= x2)
		next++;
	return (x1 >= SolidSegments[next].X1 && x2 <= SolidSegments[next].X2);
}

void RenderPolyBsp::MarkSegmentCulled(int x1, int x2)
{
	if (x1 >= x2)
		return;

	int cur = 1;
	while (true)
	{
		if (SolidSegments[cur].X1 <= x1 && SolidSegments[cur].X2 >= x2) // Already fully marked
		{
			break;
		}
		else if (cur + 1 != SolidSegments.size() && SolidSegments[cur].X2 >= x1 && SolidSegments[cur].X1 <= x2) // Merge segments
		{
			// Find last segment
			int merge = cur;
			while (merge + 2 != SolidSegments.size() && SolidSegments[merge + 1].X1 <= x2)
				merge++;

			// Apply new merged range
			SolidSegments[cur].X1 = MIN(SolidSegments[cur].X1, x1);
			SolidSegments[cur].X2 = MAX(SolidSegments[merge].X2, x2);

			// Remove additional segments we merged with
			if (merge > cur)
				SolidSegments.erase(SolidSegments.begin() + (cur + 1), SolidSegments.begin() + (merge + 1));

			break;
		}
		else if (SolidSegments[cur].X1 > x1) // Insert new segment
		{
			SolidSegments.insert(SolidSegments.begin() + cur, { x1, x2 });
			break;
		}
		cur++;
	}
}

int RenderPolyBsp::PointOnSide(const DVector2 &pos, const node_t *node)
{
	return DMulScale32(FLOAT2FIXED(pos.Y) - node->y, node->dx, node->x - FLOAT2FIXED(pos.X), node->dy) > 0;
}

bool RenderPolyBsp::CheckBBox(float *bspcoord)
{
	static const int checkcoord[12][4] =
	{
		{ 3,0,2,1 },
		{ 3,0,2,0 },
		{ 3,1,2,0 },
		{ 0 },
		{ 2,0,2,1 },
		{ 0,0,0,0 },
		{ 3,1,3,0 },
		{ 0 },
		{ 2,0,3,1 },
		{ 2,1,3,1 },
		{ 2,1,3,0 }
	};

	int 				boxx;
	int 				boxy;
	int 				boxpos;

	double	 			x1, y1, x2, y2;
	double				rx1, ry1, rx2, ry2;
	int					sx1, sx2;

	// Find the corners of the box
	// that define the edges from current viewpoint.
	if (ViewPos.X <= bspcoord[BOXLEFT])
		boxx = 0;
	else if (ViewPos.X < bspcoord[BOXRIGHT])
		boxx = 1;
	else
		boxx = 2;

	if (ViewPos.Y >= bspcoord[BOXTOP])
		boxy = 0;
	else if (ViewPos.Y > bspcoord[BOXBOTTOM])
		boxy = 1;
	else
		boxy = 2;

	boxpos = (boxy << 2) + boxx;
	if (boxpos == 5)
		return true;

	x1 = bspcoord[checkcoord[boxpos][0]] - ViewPos.X;
	y1 = bspcoord[checkcoord[boxpos][1]] - ViewPos.Y;
	x2 = bspcoord[checkcoord[boxpos][2]] - ViewPos.X;
	y2 = bspcoord[checkcoord[boxpos][3]] - ViewPos.Y;

	// check clip list for an open space

	// Sitting on a line?
	if (y1 * (x1 - x2) + x1 * (y2 - y1) >= -EQUAL_EPSILON)
		return true;

	rx1 = x1 * ViewSin - y1 * ViewCos;
	rx2 = x2 * ViewSin - y2 * ViewCos;
	ry1 = x1 * ViewTanCos + y1 * ViewTanSin;
	ry2 = x2 * ViewTanCos + y2 * ViewTanSin;

	/*if (MirrorFlags & RF_XFLIP)
	{
	double t = -rx1;
	rx1 = -rx2;
	rx2 = t;
	swapvalues(ry1, ry2);
	}*/

	if (rx1 >= -ry1)
	{
		if (rx1 > ry1) return false;	// left edge is off the right side
		if (ry1 == 0) return false;
		sx1 = xs_RoundToInt(CenterX + rx1 * CenterX / ry1);
	}
	else
	{
		if (rx2 < -ry2) return false;	// wall is off the left side
		if (rx1 - rx2 - ry2 + ry1 == 0) return false;	// wall does not intersect view volume
		sx1 = 0;
	}

	if (rx2 <= ry2)
	{
		if (rx2 < -ry2) return false;	// right edge is off the left side
		if (ry2 == 0) return false;
		sx2 = xs_RoundToInt(CenterX + rx2 * CenterX / ry2);
	}
	else
	{
		if (rx1 > ry1) return false;	// wall is off the right side
		if (ry2 - ry1 - rx2 + rx1 == 0) return false;	// wall does not intersect view volume
		sx2 = viewwidth;
	}

	// Find the first clippost that touches the source post
	//	(adjacent pixels are touching).

	// Does not cross a pixel.
	if (sx2 <= sx1)
		return false;

	return !IsSegmentCulled(sx1, sx2);
}

bool RenderPolyBsp::GetSegmentRangeForLine(double x1, double y1, double x2, double y2, int &sx1, int &sx2) const
{
	x1 = x1 - ViewPos.X;
	y1 = y1 - ViewPos.Y;
	x2 = x2 - ViewPos.X;
	y2 = y2 - ViewPos.Y;

	// Sitting on a line?
	if (y1 * (x1 - x2) + x1 * (y2 - y1) >= -EQUAL_EPSILON)
		return false;

	double rx1 = x1 * ViewSin - y1 * ViewCos;
	double rx2 = x2 * ViewSin - y2 * ViewCos;
	double ry1 = x1 * ViewTanCos + y1 * ViewTanSin;
	double ry2 = x2 * ViewTanCos + y2 * ViewTanSin;

	if (rx1 >= -ry1)
	{
		if (rx1 > ry1) return false;	// left edge is off the right side
		if (ry1 == 0) return false;
		sx1 = xs_RoundToInt(CenterX + rx1 * CenterX / ry1);
	}
	else
	{
		if (rx2 < -ry2) return false;	// wall is off the left side
		if (rx1 - rx2 - ry2 + ry1 == 0) return false;	// wall does not intersect view volume
		sx1 = 0;
	}

	if (rx2 <= ry2)
	{
		if (rx2 < -ry2) return false;	// right edge is off the left side
		if (ry2 == 0) return false;
		sx2 = xs_RoundToInt(CenterX + rx2 * CenterX / ry2);
	}
	else
	{
		if (rx1 > ry1) return false;	// wall is off the right side
		if (ry2 - ry1 - rx2 + rx1 == 0) return false;	// wall does not intersect view volume
		sx2 = viewwidth;
	}

	// Does not cross a pixel.
	if (sx2 <= sx1)
		return false;

	return true;
}

/////////////////////////////////////////////////////////////////////////////

void RenderPolyWall::Render(const TriMatrix &worldToClip)
{
	FTexture *tex = GetTexture();
	if (!tex)
		return;

	PolyWallTextureCoords texcoords(tex, Line, Texpart, TopZ, BottomZ, UnpeggedCeil);

	TriVertex *vertices = PolyVertexBuffer::GetVertices(4);
	if (!vertices)
		return;

	vertices[0].x = (float)v1.X;
	vertices[0].y = (float)v1.Y;
	vertices[0].z = (float)ceil1;
	vertices[0].w = 1.0f;
	vertices[0].varying[0] = (float)texcoords.u1;
	vertices[0].varying[1] = (float)texcoords.v1;

	vertices[1].x = (float)v2.X;
	vertices[1].y = (float)v2.Y;
	vertices[1].z = (float)ceil2;
	vertices[1].w = 1.0f;
	vertices[1].varying[0] = (float)texcoords.u2;
	vertices[1].varying[1] = (float)texcoords.v1;

	vertices[2].x = (float)v2.X;
	vertices[2].y = (float)v2.Y;
	vertices[2].z = (float)floor2;
	vertices[2].w = 1.0f;
	vertices[2].varying[0] = (float)texcoords.u2;
	vertices[2].varying[1] = (float)texcoords.v2;

	vertices[3].x = (float)v1.X;
	vertices[3].y = (float)v1.Y;
	vertices[3].z = (float)floor1;
	vertices[3].w = 1.0f;
	vertices[3].varying[0] = (float)texcoords.u1;
	vertices[3].varying[1] = (float)texcoords.v2;

	TriUniforms uniforms;
	uniforms.objectToClip = worldToClip;
	uniforms.light = (uint32_t)(GetLightLevel() / 255.0f * 256.0f);
	uniforms.flags = 0;
	PolyTriangleDrawer::draw(uniforms, vertices, 4, TriangleDrawMode::Fan, true, 0, viewwidth, 0, viewheight, tex);
}

FTexture *RenderPolyWall::GetTexture()
{
	FTexture *tex = TexMan(Line->sidedef->GetTexture(Texpart), true);
	if (tex == nullptr || tex->UseType == FTexture::TEX_Null)
		return nullptr;
	else
		return tex;
}

int RenderPolyWall::GetLightLevel()
{
	if (fixedlightlev >= 0 || fixedcolormap)
	{
		return 255;
	}
	else
	{
		bool foggy = false;
		int actualextralight = foggy ? 0 : extralight << 4;
		return Line->sidedef->GetLightLevel(foggy, Line->frontsector->lightlevel) + actualextralight;
	}
}

/*
float RenderPolyWall::GetLight(short x)
{
	if (fixedlightlev >= 0 || fixedcolormap)
		return 0.0f;
	else
		return (float)(r_WallVisibility / Coords.Z(x));
}
*/

/////////////////////////////////////////////////////////////////////////////

PolyWallTextureCoords::PolyWallTextureCoords(FTexture *tex, const seg_t *line, side_t::ETexpart texpart, double topz, double bottomz, double unpeggedceil)
{
	CalcU(tex, line, texpart);
	CalcV(tex, line, texpart, topz, bottomz, unpeggedceil);
}

void PolyWallTextureCoords::CalcU(FTexture *tex, const seg_t *line, side_t::ETexpart texpart)
{
	double lineLength = line->sidedef->TexelLength;
	double lineStart = 0.0;

	bool entireSegment = ((line->linedef->v1 == line->v1) && (line->linedef->v2 == line->v2) || (line->linedef->v2 == line->v1) && (line->linedef->v1 == line->v2));
	if (!entireSegment)
	{
		lineLength = (line->v2->fPos() - line->v1->fPos()).Length();
		lineStart = (line->v1->fPos() - line->linedef->v1->fPos()).Length();
	}

	int texWidth = tex->GetWidth();
	double uscale = line->sidedef->GetTextureXScale(texpart) * tex->Scale.X;
	u1 = lineStart + line->sidedef->GetTextureXOffset(texpart);
	u2 = u1 + lineLength;
	u1 *= uscale;
	u2 *= uscale;
	u1 /= texWidth;
	u2 /= texWidth;
}

void PolyWallTextureCoords::CalcV(FTexture *tex, const seg_t *line, side_t::ETexpart texpart, double topz, double bottomz, double unpeggedceil)
{
	double vscale = line->sidedef->GetTextureYScale(texpart) * tex->Scale.Y;

	double yoffset = line->sidedef->GetTextureYOffset(texpart);
	if (tex->bWorldPanning)
		yoffset *= vscale;

	switch (texpart)
	{
	default:
	case side_t::mid:
		CalcVMidPart(tex, line, topz, bottomz, vscale, yoffset);
		break;
	case side_t::top:
		CalcVTopPart(tex, line, topz, bottomz, vscale, yoffset);
		break;
	case side_t::bottom:
		CalcVBottomPart(tex, line, topz, bottomz, unpeggedceil, vscale, yoffset);
		break;
	}

	int texHeight = tex->GetHeight();
	v1 /= texHeight;
	v2 /= texHeight;
}

void PolyWallTextureCoords::CalcVTopPart(FTexture *tex, const seg_t *line, double topz, double bottomz, double vscale, double yoffset)
{
	bool pegged = (line->linedef->flags & ML_DONTPEGTOP) == 0;
	if (pegged) // bottom to top
	{
		int texHeight = tex->GetHeight();
		v1 = -yoffset;
		v2 = v1 + (topz - bottomz);
		v1 *= vscale;
		v2 *= vscale;
		v1 = texHeight - v1;
		v2 = texHeight - v2;
		std::swap(v1, v2);
	}
	else // top to bottom
	{
		v1 = yoffset;
		v2 = v1 + (topz - bottomz);
		v1 *= vscale;
		v2 *= vscale;
	}
}

void PolyWallTextureCoords::CalcVMidPart(FTexture *tex, const seg_t *line, double topz, double bottomz, double vscale, double yoffset)
{
	bool pegged = (line->linedef->flags & ML_DONTPEGBOTTOM) == 0;
	if (pegged) // top to bottom
	{
		v1 = yoffset;
		v2 = v1 + (topz - bottomz);
		v1 *= vscale;
		v2 *= vscale;
	}
	else // bottom to top
	{
		int texHeight = tex->GetHeight();
		v1 = yoffset;
		v2 = v1 + (topz - bottomz);
		v1 *= vscale;
		v2 *= vscale;
		v1 = texHeight - v1;
		v2 = texHeight - v2;
		std::swap(v1, v2);
	}
}

void PolyWallTextureCoords::CalcVBottomPart(FTexture *tex, const seg_t *line, double topz, double bottomz, double unpeggedceil, double vscale, double yoffset)
{
	bool pegged = (line->linedef->flags & ML_DONTPEGBOTTOM) == 0;
	if (pegged) // top to bottom
	{
		v1 = yoffset;
		v2 = v1 + (topz - bottomz);
		v1 *= vscale;
		v2 *= vscale;
	}
	else
	{
		v1 = yoffset + (unpeggedceil - topz);
		v2 = v1 + (topz - bottomz);
		v1 *= vscale;
		v2 *= vscale;
	}
}

/////////////////////////////////////////////////////////////////////////////

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
		DTA_ColormapStyle, usecolormapstyle ? &colormapstyle : NULL,
		TAG_DONE);
}

/////////////////////////////////////////////////////////////////////////////

namespace
{
	int NextBufferVertex = 0;
}

TriVertex *PolyVertexBuffer::GetVertices(int count)
{
	enum { VertexBufferSize = 16 * 1024 };
	static TriVertex Vertex[VertexBufferSize];

	if (NextBufferVertex + count > VertexBufferSize)
		return nullptr;
	TriVertex *v = Vertex + NextBufferVertex;
	NextBufferVertex += count;
	return v;
}

void PolyVertexBuffer::Clear()
{
	NextBufferVertex = 0;
}

/////////////////////////////////////////////////////////////////////////////

TriVertex PolySkyDome::SetVertex(float xx, float yy, float zz, float uu, float vv)
{
	TriVertex v;
	v.x = xx;
	v.y = yy;
	v.z = zz;
	v.w = 1.0f;
	v.varying[0] = uu;
	v.varying[1] = vv;
	return v;
}

TriVertex PolySkyDome::SetVertexXYZ(float xx, float yy, float zz, float uu, float vv)
{
	TriVertex v;
	v.x = xx;
	v.y = zz;
	v.z = yy;
	v.w = 1.0f;
	v.varying[0] = uu;
	v.varying[1] = vv;
	return v;
}

void PolySkyDome::SkyVertex(int r, int c, bool zflip)
{
	static const FAngle maxSideAngle = 60.f;
	static const float scale = 10000.;

	FAngle topAngle = (c / (float)mColumns * 360.f);
	FAngle sideAngle = maxSideAngle * (float)(mRows - r) / (float)mRows;
	float height = sideAngle.Sin();
	float realRadius = scale * sideAngle.Cos();
	FVector2 pos = topAngle.ToVector(realRadius);
	float z = (!zflip) ? scale * height : -scale * height;

	float u, v;
	//uint32_t color = r == 0 ? 0xffffff : 0xffffffff;

	// And the texture coordinates.
	if (!zflip)	// Flipped Y is for the lower hemisphere.
	{
		u = (-c / (float)mColumns);
		v = (r / (float)mRows);
	}
	else
	{
		u = (-c / (float)mColumns);
		v = 1.0f + ((mRows - r) / (float)mRows);
	}

	if (r != 4) z += 300;

	// And finally the vertex.
	TriVertex vert;
	vert = SetVertexXYZ(-pos.X, z - 1.f, pos.Y, u * 4.0f, v + 0.5f/*, color*/);
	mVertices.Push(vert);
}

void PolySkyDome::CreateSkyHemisphere(bool zflip)
{
	int r, c;

	mPrimStart.Push(mVertices.Size());

	for (c = 0; c < mColumns; c++)
	{
		SkyVertex(1, c, zflip);
	}

	// The total number of triangles per hemisphere can be calculated
	// as follows: rows * columns * 2 + 2 (for the top cap).
	for (r = 0; r < mRows; r++)
	{
		mPrimStart.Push(mVertices.Size());
		for (c = 0; c <= mColumns; c++)
		{
			SkyVertex(r + zflip, c, zflip);
			SkyVertex(r + 1 - zflip, c, zflip);
		}
	}
}

void PolySkyDome::CreateDome()
{
	mColumns = 128;
	mRows = 4;
	CreateSkyHemisphere(false);
	CreateSkyHemisphere(true);
	mPrimStart.Push(mVertices.Size());
}

void PolySkyDome::RenderRow(const TriUniforms &uniforms, FTexture *skytex, int row)
{
	PolyTriangleDrawer::draw(uniforms, &mVertices[mPrimStart[row]], mPrimStart[row + 1] - mPrimStart[row], TriangleDrawMode::Strip, false, 0, viewwidth, 0, viewheight, skytex);
}

void PolySkyDome::RenderCapColorRow(const TriUniforms &uniforms, FTexture *skytex, int row, bool bottomCap)
{
	uint32_t solid = skytex->GetSkyCapColor(bottomCap);
	if (!r_swtruecolor)
		solid = RGB32k.RGB[(RPART(solid) >> 3)][(GPART(solid) >> 3)][(BPART(solid) >> 3)];
	PolyTriangleDrawer::fill(uniforms, &mVertices[mPrimStart[row]], mPrimStart[row + 1] - mPrimStart[row], TriangleDrawMode::Fan, bottomCap, 0, viewwidth, 0, viewheight, solid);
}

void PolySkyDome::Render(const TriMatrix &worldToClip)
{
	FTextureID sky1tex, sky2tex;
	if ((level.flags & LEVEL_SWAPSKIES) && !(level.flags & LEVEL_DOUBLESKY))
		sky1tex = sky2texture;
	else
		sky1tex = sky1texture;
	sky2tex = sky2texture;

	FTexture *frontskytex = TexMan(sky1tex, true);
	FTexture *backskytex = nullptr;
	if (level.flags & LEVEL_DOUBLESKY)
		backskytex = TexMan(sky2tex, true);

	TriMatrix objectToWorld = TriMatrix::translate((float)ViewPos.X, (float)ViewPos.Y, (float)ViewPos.Z);

	TriUniforms uniforms;
	uniforms.objectToClip = worldToClip * objectToWorld;
	uniforms.light = 256;
	uniforms.flags = 0;

	int rc = mRows + 1;

	RenderCapColorRow(uniforms, frontskytex, 0, false);
	RenderCapColorRow(uniforms, frontskytex, rc, true);

	for (int i = 1; i <= mRows; i++)
	{
		RenderRow(uniforms, frontskytex, i);
		RenderRow(uniforms, frontskytex, rc + i);
	}
}
