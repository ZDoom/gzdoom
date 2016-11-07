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

namespace
{
	short cliptop[MAXWIDTH], clipbottom[MAXWIDTH];
}

/////////////////////////////////////////////////////////////////////////////

void RenderPolyBsp::Render()
{
	PolyVertexBuffer::Clear();

	for (int i = 0; i < viewwidth; i++)
	{
		cliptop[i] = 0;
		clipbottom[i] = viewheight;
	}

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
	worldToClip = TriMatrix::perspective(fovy, ratio, 5.0, 65535.0f) * worldToView;

	// Y shearing like the Doom renderer:
	//worldToClip = TriMatrix::viewToClip() * TriMatrix::worldToView();

	// Cull front to back (ok, so we dont cull yet, but we should during this!):
	if (numnodes == 0)
		PvsSectors.push_back(subsectors); // RenderSubsector(subsectors);
	else
		RenderNode(nodes + numnodes - 1);	// The head node is the last node output.

	// Render back to front (we don't have a zbuffer at the moment, sniff!):
	for (auto it = PvsSectors.rbegin(); it != PvsSectors.rend(); ++it)
		RenderSubsector(*it);

	RenderPlayerSprites();
	RenderScreenSprites(); // To do: should be called by FSoftwareRenderer::DrawRemainingPlayerSprites instead of here
}

void RenderPolyBsp::RenderScreenSprites()
{
	for (auto &sprite : ScreenSprites)
		sprite.Render();
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

	TriVertex *floorVertices = PolyVertexBuffer::GetVertices(sub->numlines);
	TriVertex *ceilVertices = PolyVertexBuffer::GetVertices(sub->numlines);
	if (floorVertices == nullptr || ceilVertices == nullptr)
		return;

	for (uint32_t i = 0; i < sub->numlines; i++)
	{
		seg_t *line = &sub->firstline[i];
		int j = sub->numlines - 1 - i;
		ceilVertices[j].x = (float)line->v1->fPos().X;
		ceilVertices[j].y = (float)line->v1->fPos().Y;
		ceilVertices[j].z = (float)frontsector->ceilingplane.ZatPoint(line->v1);
		ceilVertices[j].w = 1.0f;
		ceilVertices[j].varying[0] = ceilVertices[j].x / 64.0f;
		ceilVertices[j].varying[1] = ceilVertices[j].y / 64.0f;
		ceilVertices[j].varying[2] = 1.0f;

		floorVertices[i].x = (float)line->v1->fPos().X;
		floorVertices[i].y = (float)line->v1->fPos().Y;
		floorVertices[i].z = (float)frontsector->floorplane.ZatPoint(line->v1);
		floorVertices[i].w = 1.0f;
		floorVertices[i].varying[0] = floorVertices[i].x / 64.0f;
		floorVertices[i].varying[1] = floorVertices[i].y / 64.0f;
		floorVertices[i].varying[2] = 1.0f;
	}

	FTexture *floortex = TexMan(frontsector->GetTexture(sector_t::floor));
	if (floortex->UseType != FTexture::TEX_Null)
		TriangleDrawer::draw(worldToClip, floorVertices, sub->numlines, TriangleDrawMode::Fan, true, 0, viewwidth, cliptop, clipbottom, floortex);

	FTexture *ceiltex = TexMan(frontsector->GetTexture(sector_t::ceiling));
	if (ceiltex->UseType != FTexture::TEX_Null)
		TriangleDrawer::draw(worldToClip, ceilVertices, sub->numlines, TriangleDrawMode::Fan, true, 0, viewwidth, cliptop, clipbottom, ceiltex);

	/*for (AActor *thing = sub->sector->thinglist; thing != nullptr; thing = thing->snext)
	{
		if ((thing->renderflags & RF_SPRITETYPEMASK) == RF_WALLSPRITE)
			AddWallSprite(thing);
		else
			AddSprite(thing);
	}*/
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

		bool bothSkyCeiling = false;// frontsector->GetTexture(sector_t::ceiling) == skyflatnum && backsector->GetTexture(sector_t::ceiling) == skyflatnum;
		bool bothSkyFloor = false;// frontsector->GetTexture(sector_t::floor) == skyflatnum && backsector->GetTexture(sector_t::floor) == skyflatnum;

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
	PvsSectors.push_back((subsector_t *)((BYTE *)node - 1));
	//RenderSubsector((subsector_t *)((BYTE *)node - 1));
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
	vertices[0].varying[2] = 1.0f;

	vertices[1].x = (float)v2.X;
	vertices[1].y = (float)v2.Y;
	vertices[1].z = (float)ceil2;
	vertices[1].w = 1.0f;
	vertices[1].varying[0] = (float)texcoords.u2;
	vertices[1].varying[1] = (float)texcoords.v1;
	vertices[1].varying[2] = 1.0f;

	vertices[2].x = (float)v2.X;
	vertices[2].y = (float)v2.Y;
	vertices[2].z = (float)floor2;
	vertices[2].w = 1.0f;
	vertices[2].varying[0] = (float)texcoords.u2;
	vertices[2].varying[1] = (float)texcoords.v2;
	vertices[2].varying[2] = 1.0f;

	vertices[3].x = (float)v1.X;
	vertices[3].y = (float)v1.Y;
	vertices[3].z = (float)floor1;
	vertices[3].w = 1.0f;
	vertices[3].varying[0] = (float)texcoords.u1;
	vertices[3].varying[1] = (float)texcoords.v2;
	vertices[3].varying[2] = 1.0f;

	TriangleDrawer::draw(worldToClip, vertices, 4, TriangleDrawMode::Fan, true, 0, viewwidth, cliptop, clipbottom, tex);
}

FTexture *RenderPolyWall::GetTexture()
{
	FTexture *tex = TexMan(Line->sidedef->GetTexture(Texpart), true);
	if (tex == nullptr || tex->UseType == FTexture::TEX_Null)
		return nullptr;
	else
		return tex;
}

int RenderPolyWall::GetShade()
{
	if (fixedlightlev >= 0 || fixedcolormap)
	{
		return 0;
	}
	else
	{
		bool foggy = false;
		int actualextralight = foggy ? 0 : extralight << 4;
		int shade = LIGHT2SHADE(Line->sidedef->GetLightLevel(foggy, Line->frontsector->lightlevel) + actualextralight);
		return shade;
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
