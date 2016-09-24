
#include <stdlib.h>
#include "templates.h"
#include "doomdef.h"
#include "sbar.h"
#include "r_data/r_translate.h"
#include "r_swrenderer2.h"
#include "r_draw.h"
#include "r_plane.h" // for yslope
#include "r_sky.h" // for skyflatnum
#include "r_things.h" // for pspritexscale

EXTERN_CVAR(Bool, r_drawplayersprites)
EXTERN_CVAR(Bool, r_deathcamera)
EXTERN_CVAR(Bool, st_scale)

DVector3 ViewPosTransform::WorldToEye(const DVector3 &worldPoint) const
{
	double tr_x = worldPoint.X - ViewPos.X;
	double tr_y = worldPoint.Y - ViewPos.Y;
	double tr_z = worldPoint.Z - ViewPos.Z;
	double tx = tr_x * ViewSin - tr_y * ViewCos;
	double tz = tr_x * ViewTanCos + tr_y * ViewTanSin;
	return DVector3(tx, tr_z, tz);
}

DVector3 ViewPosTransform::EyeToViewport(const DVector3 &eyePoint) const
{
	double rcp_z = 1.0 / eyePoint.Z;
	return DVector3(eyePoint.X * rcp_z, eyePoint.Y * rcp_z, rcp_z);
}

DVector3 ViewPosTransform::ViewportToScreen(const DVector3 &viewportPoint) const
{
	return DVector3(CenterX + viewportPoint.X * CenterX, CenterY - viewportPoint.Y * InvZtoScale, viewportPoint.Z);
}

double ViewPosTransform::ScreenXToEye(int x, double z) const
{
	return (x + 0.5 - CenterX) / CenterX * z;
}

double ViewPosTransform::ScreenYToEye(int y, double z) const
{
	return -(y + 0.5 - CenterY) / InvZtoScale * z;
}

/////////////////////////////////////////////////////////////////////////////

WallCoords::WallCoords(const ViewPosTransform &transform, const DVector2 &v1, const DVector2 &v2, double ceil1, double floor1, double ceil2, double floor2) : Transform(transform)
{
	// Transform wall to eye space
	DVector3 top1 = transform.WorldToEye(DVector3(v1, ceil1));
	DVector3 top2 = transform.WorldToEye(DVector3(v2, ceil2));
	DVector3 bottom1 = transform.WorldToEye(DVector3(v1, floor1));
	DVector3 bottom2 = transform.WorldToEye(DVector3(v2, floor2));

	double clipNearZ = transform.NearZ();

	// Is entire wall behind znear clipping plane? If so, wall is culled
	if ((top1.Z < clipNearZ && top2.Z < clipNearZ))
		return;

	PlaneNormal = (top2 - top1) ^ (top1 - bottom1);
	PlaneD = -(PlaneNormal | top1);

	// Clip wall to znear clipping plane
	if (top1.Z < clipNearZ)
	{
		double t = (clipNearZ - top1.Z) / (top2.Z - top1.Z);
		top1 = Mix(top1, top2, t);
		bottom1 = Mix(bottom1, bottom2, t);

		VaryingXScale = 1.0 - t;
		VaryingXOffset = t;
	}
	else if (top2.Z < clipNearZ)
	{
		double t = (clipNearZ - top1.Z) / (top2.Z - top1.Z);
		top2 = Mix(top1, top2, t);
		bottom2 = Mix(bottom1, bottom2, t);
		VaryingXScale = t;
		VaryingXOffset = 0.0;
	}

	NearZ = MIN(top1.Z, top2.Z);
	FarZ = MAX(top1.Z, top2.Z);

	// Transform to screen coordinates
	ScreenTopLeft = transform.EyeToScreen(top1);
	ScreenTopRight = transform.EyeToScreen(top2);
	ScreenBottomLeft = transform.EyeToScreen(bottom1);
	ScreenBottomRight = transform.EyeToScreen(bottom2);
	if (ScreenTopLeft.X > ScreenTopRight.X)
	{
		std::swap(ScreenTopLeft, ScreenTopRight);
		std::swap(ScreenBottomLeft, ScreenBottomRight);
	}

	ScreenX1 = xs_RoundToInt(MAX(ScreenTopLeft.X, 0.0));
	ScreenX2 = xs_RoundToInt(MIN(ScreenTopRight.X, (double)viewwidth));
	ScreenY1 = xs_RoundToInt(MAX(MIN(ScreenTopLeft.Y, ScreenTopRight.Y), 0.0));
	ScreenY2 = xs_RoundToInt(MIN(MAX(ScreenBottomLeft.Y, ScreenBottomRight.Y), (double)viewheight));

	// Cull if nothing of the wall is visible
	if (ScreenX2 <= ScreenX1 || ScreenY2 <= ScreenY1)
		return;

	RcpDeltaScreenX = 1.0 / (ScreenTopRight.X - ScreenTopLeft.X);
	Culled = false;
}

DVector3 WallCoords::Mix(const DVector3 &a, const DVector3 &b, double t)
{
	double invt = 1.0 - t;
	return DVector3(a.X * invt + b.X * t, a.Y * invt + b.Y * t, a.Z * invt + b.Z * t);
}

double WallCoords::Mix(double a, double b, double t)
{
	return a * (1.0 - t) + b * t;
}

short WallCoords::Y1(int x) const
{
	double t = (x + 0.5 - ScreenTopLeft.X) * RcpDeltaScreenX;
	return (short)MAX(xs_RoundToInt(Mix(ScreenTopLeft.Y, ScreenTopRight.Y, t)), 0);
}

short WallCoords::Y2(int x) const
{
	double t = (x + 0.5 - ScreenBottomLeft.X) * RcpDeltaScreenX;
	return (short)MIN(xs_RoundToInt(Mix(ScreenBottomLeft.Y, ScreenBottomRight.Y, t)), viewheight);
}

double WallCoords::Z(int x) const
{
	double t = (x + 0.5 - ScreenTopLeft.X) * RcpDeltaScreenX;
	double rcp_z = Mix(ScreenTopLeft.Z, ScreenTopRight.Z, t);
	return 1.0 / rcp_z;
}

double WallCoords::VaryingX(int x, double start, double end) const
{
	double t = (x + 0.5 - ScreenTopLeft.X) * RcpDeltaScreenX;
	double rcp_z = Mix(ScreenTopLeft.Z, ScreenTopRight.Z, t);
	double t2 = VaryingXOffset + t / rcp_z * ScreenTopRight.Z * VaryingXScale;
	return Mix(start, end, t2);
}

double WallCoords::VaryingY(int x, int y, double start, double end) const
{
	double t = (x + 0.5 - ScreenTopLeft.X) * RcpDeltaScreenX;
	double y1 = Mix(ScreenTopLeft.Y, ScreenTopRight.Y, t);
	double y2 = Mix(ScreenBottomLeft.Y, ScreenBottomRight.Y, t);
	if (y1 == y2 || y1 == -y2)
		return start;
	double t2 = (y + 0.5 - y1) / (y2 - y1);
	return Mix(start, end, t2);
}

/////////////////////////////////////////////////////////////////////////////

void RenderBsp::Render()
{
	Clip.Clear(0, viewwidth);
	Planes.Clear();
	VisibleSprites.clear();
	ScreenSprites.clear();

	if (numnodes == 0)
		RenderSubsector(subsectors);
	else
		RenderNode(nodes + numnodes - 1);	// The head node is the last node output.

	Planes.Render();

	RenderMaskedObjects();
	RenderPlayerSprites();
	RenderScreenSprites(); // To do: should be called by FSoftwareRenderer::DrawRemainingPlayerSprites instead of here
}

void RenderBsp::RenderScreenSprites()
{
	for (auto &sprite : ScreenSprites)
		sprite.Render();
}

void RenderBsp::RenderMaskedObjects()
{
	Clip.DrawMaskedWall = [&](short x1, short x2, int drawIndex, const short *clipTop, const short *clipBottom)
	{
		VisibleMaskedWalls[drawIndex].RenderMasked(x1, x2, clipTop, clipBottom);
	};

	std::stable_sort(VisibleSprites.begin(), VisibleSprites.end(), [](const auto &a, const auto &b) { return a.EyePos.Z > b.EyePos.Z; });

	for (auto &sprite : VisibleSprites)
		sprite.Render(&Clip);

	Clip.RenderMaskedWalls();
}

void RenderBsp::RenderPlayerSprites()
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

void RenderBsp::RenderPlayerSprite(DPSprite *sprite, AActor *owner, float bobx, float boby, double wx, double wy, double ticfrac)
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
			ScreenSprite screenSprite;
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

bool RenderBsp::IsThingCulled(AActor *thing)
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

void RenderBsp::AddSprite(AActor *thing)
{
	if (IsThingCulled(thing))
		return;

	DVector3 pos = thing->InterpolatedPosition(r_TicFracF);
	pos.Z += thing->GetBobOffset(r_TicFracF);

	DVector3 eyePos = Transform.WorldToEye(pos);

	// thing is behind view plane?
	if (eyePos.Z < Transform.NearZ())
		return;

	// too far off the side?
	if (fabs(eyePos.X / 64) > eyePos.Z)
		return;

	VisibleSprites.push_back({ thing, eyePos });
}

void RenderBsp::AddWallSprite(AActor *thing)
{
	if (IsThingCulled(thing))
		return;
}

void RenderBsp::RenderSubsector(subsector_t *sub)
{
	sector_t *frontsector = sub->sector;
	frontsector->MoreFlags |= SECF_DRAWN;

	for (AActor *thing = sub->sector->thinglist; thing != nullptr; thing = thing->snext)
	{
		if ((thing->renderflags & RF_SPRITETYPEMASK) == RF_WALLSPRITE)
			AddWallSprite(thing);
		else
			AddSprite(thing);
	}

	for (uint32_t i = 0; i < sub->numlines; i++)
	{
		seg_t *line = &sub->firstline[i];
		if (line->sidedef == NULL || !(line->sidedef->Flags & WALLF_POLYOBJ))
			AddLine(line, frontsector);
	}
}

void RenderBsp::AddLine(seg_t *line, sector_t *frontsector)
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

	WallCoords entireWall(Transform, line->v1->fPos(), line->v2->fPos(), frontceilz1, frontfloorz1, frontceilz2, frontfloorz2);
	if (entireWall.Culled)
		return;

	VisiblePlaneKey ceilingPlaneKey(frontsector->GetTexture(sector_t::ceiling), frontsector->ColorMap, frontsector->lightlevel, frontsector->ceilingplane, frontsector->planes[sector_t::ceiling].xform);
	VisiblePlaneKey floorPlaneKey(frontsector->GetTexture(sector_t::floor), frontsector->ColorMap, frontsector->lightlevel, frontsector->floorplane, frontsector->planes[sector_t::floor].xform);

	RenderWall wall;
	wall.Line = line;
	wall.Colormap = frontsector->ColorMap;
	wall.Masked = false;

	if (line->backsector == nullptr)
	{
		Planes.MarkCeilingPlane(ceilingPlaneKey, Clip, entireWall);
		Planes.MarkFloorPlane(floorPlaneKey, Clip, entireWall);

		wall.Coords = entireWall;
		wall.TopZ = frontceilz1;
		wall.BottomZ = frontfloorz1;
		wall.UnpeggedCeil = frontceilz1;
		wall.Texpart = side_t::mid;
		wall.Render(Clip);

		Clip.MarkSegmentCulled(entireWall, -1);
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

		int maskedWallIndex = -1;

		if ((topceilz1 > topfloorz1 || topceilz2 > topfloorz2) && !bothSkyCeiling && line->sidedef)
		{
			WallCoords topwall(Transform, line->v1->fPos(), line->v2->fPos(), topceilz1, topfloorz1, topceilz2, topfloorz2);
			if (!topwall.Culled)
			{
				wall.Coords = topwall;
				wall.TopZ = topceilz1;
				wall.BottomZ = topfloorz1;
				wall.UnpeggedCeil = topceilz1;
				wall.Texpart = side_t::top;
				wall.Render(Clip);
			}
		}

		if ((bottomfloorz1 < bottomceilz1 || bottomfloorz2 < bottomceilz2) && !bothSkyFloor && line->sidedef)
		{
			WallCoords bottomwall(Transform, line->v1->fPos(), line->v2->fPos(), bottomceilz1, bottomfloorz2, bottomceilz2, bottomfloorz2);
			if (!bottomwall.Culled)
			{
				wall.Coords = bottomwall;
				wall.TopZ = bottomceilz1;
				wall.BottomZ = bottomfloorz2;
				wall.UnpeggedCeil = topceilz1;
				wall.Texpart = side_t::bottom;
				wall.Render(Clip);
			}
		}

		WallCoords midwall(Transform, line->v1->fPos(), line->v2->fPos(), middleceilz1, middlefloorz1, middleceilz2, middlefloorz2);
		if (!midwall.Culled && line->sidedef)
		{
			FTexture *midtex = TexMan(line->sidedef->GetTexture(side_t::mid), true);
			if (midtex && midtex->UseType != FTexture::TEX_Null)
			{
				DVector3 v1 = Transform.WorldToEye({ line->v1->fPos(), 0.0 });
				DVector3 v2 = Transform.WorldToEye({ line->v2->fPos(), 0.0 });
				wall.Coords = midwall;
				wall.TopZ = middleceilz1;
				wall.BottomZ = middlefloorz1;
				wall.UnpeggedCeil = topceilz1;
				wall.Texpart = side_t::mid;
				wall.Masked = true;

				maskedWallIndex = (int)VisibleMaskedWalls.size();
				VisibleMaskedWalls.push_back(wall);
			}
		}

		if (!bothSkyCeiling && !bothSkyFloor)
		{
			Planes.MarkCeilingPlane(ceilingPlaneKey, Clip, entireWall);
			Planes.MarkFloorPlane(floorPlaneKey, Clip, entireWall);
			if (!midwall.Culled)
				Clip.ClipVertical(midwall, maskedWallIndex);
			else
				Clip.MarkSegmentCulled(entireWall, maskedWallIndex);
		}
		else if (bothSkyCeiling)
		{
			Planes.MarkFloorPlane(floorPlaneKey, Clip, entireWall);
			if (!midwall.Culled)
				Clip.ClipBottom(midwall, maskedWallIndex);
			else
				Clip.MarkSegmentCulled(entireWall, maskedWallIndex);
		}
		else if (bothSkyFloor)
		{
			Planes.MarkCeilingPlane(ceilingPlaneKey, Clip, entireWall);
			if (!midwall.Culled)
				Clip.ClipTop(midwall, maskedWallIndex);
			else
				Clip.MarkSegmentCulled(entireWall, maskedWallIndex);
		}
	}
}

void RenderBsp::RenderNode(void *node)
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
	RenderSubsector((subsector_t *)((BYTE *)node - 1));
}

int RenderBsp::PointOnSide(const DVector2 &pos, const node_t *node)
{
	return DMulScale32(FLOAT2FIXED(pos.Y) - node->y, node->dx, node->x - FLOAT2FIXED(pos.X), node->dy) > 0;
}

bool RenderBsp::CheckBBox(float *bspcoord)
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

	return !Clip.IsSegmentCulled(sx1, sx2);
}

/////////////////////////////////////////////////////////////////////////////

WallTextureCoords::WallTextureCoords(FTexture *tex, const seg_t *line, side_t::ETexpart texpart, double topz, double bottomz, double unpeggedceil)
{
	CalcU(tex, line, texpart);
	CalcV(tex, line, texpart, topz, bottomz, unpeggedceil);
}

void WallTextureCoords::CalcU(FTexture *tex, const seg_t *line, side_t::ETexpart texpart)
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

void WallTextureCoords::CalcV(FTexture *tex, const seg_t *line, side_t::ETexpart texpart, double topz, double bottomz, double unpeggedceil)
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

void WallTextureCoords::CalcVTopPart(FTexture *tex, const seg_t *line, double topz, double bottomz, double vscale, double yoffset)
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

void WallTextureCoords::CalcVMidPart(FTexture *tex, const seg_t *line, double topz, double bottomz, double vscale, double yoffset)
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

void WallTextureCoords::CalcVBottomPart(FTexture *tex, const seg_t *line, double topz, double bottomz, double unpeggedceil, double vscale, double yoffset)
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

void RenderClipBuffer::Clear(short left, short right)
{
	SolidSegments.clear();
	SolidSegments.reserve(MAXWIDTH / 2 + 2);
	SolidSegments.push_back({ -0x7fff, left });
	SolidSegments.push_back({ right, 0x7fff });

	DrawSegments.clear();
	ClipValues.clear();

	for (int x = left; x < right; x++)
	{
		Top[x] = 0;
		Bottom[x] = viewheight;
	}
}

bool RenderClipBuffer::IsSegmentCulled(short x1, short x2) const
{
	int next = 0;
	while (SolidSegments[next].X2 <= x2)
		next++;
	return (x1 >= SolidSegments[next].X1 && x2 <= SolidSegments[next].X2);
}

void RenderClipBuffer::MarkSegmentCulled(const WallCoords &wallCoords, int drawIndex)
{
	if (wallCoords.Culled)
		return;

	VisibleSegmentsIterator it(*this, wallCoords.ScreenX1, wallCoords.ScreenX2);
	while (it.Step())
	{
		for (short x = it.X1; x < it.X2; x++)
		{
			Bottom[x] = Top[x];
		}

		AddDrawSegment(it.X1, it.X2, wallCoords, true, true, drawIndex);
	}

	short x1 = wallCoords.ScreenX1;
	short x2 = wallCoords.ScreenX2;

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

void RenderClipBuffer::ClipVertical(const WallCoords &wallCoords, int drawIndex)
{
	if (wallCoords.Culled)
		return;

	VisibleSegmentsIterator it(*this, wallCoords.ScreenX1, wallCoords.ScreenX2);
	while (it.Step())
	{
		for (short x = it.X1; x < it.X2; x++)
		{
			Top[x] = MAX(wallCoords.Y1(x), Top[x]);
			Bottom[x] = MIN(wallCoords.Y2(x), Bottom[x]);
		}
		AddDrawSegment(it.X1, it.X2, wallCoords, true, true, drawIndex);
	}
}

void RenderClipBuffer::ClipTop(const WallCoords &wallCoords, int drawIndex)
{
	if (wallCoords.Culled)
		return;

	VisibleSegmentsIterator it(*this, wallCoords.ScreenX1, wallCoords.ScreenX2);
	while (it.Step())
	{
		for (short x = it.X1; x < it.X2; x++)
		{
			Top[x] = MAX(wallCoords.Y1(x), Top[x]);
		}
		AddDrawSegment(it.X1, it.X2, wallCoords, true, false, drawIndex);
	}
}

void RenderClipBuffer::ClipBottom(const WallCoords &wallCoords, int drawIndex)
{
	if (wallCoords.Culled)
		return;

	VisibleSegmentsIterator it(*this, wallCoords.ScreenX1, wallCoords.ScreenX2);
	while (it.Step())
	{
		for (short x = it.X1; x < it.X2; x++)
		{
			Bottom[x] = MIN(wallCoords.Y2(x), Bottom[x]);
		}
		AddDrawSegment(it.X1, it.X2, wallCoords, false, true, drawIndex);
	}
}

void RenderClipBuffer::AddDrawSegment(short x1, short x2, const WallCoords &wall, bool clipTop, bool clipBottom, int drawIndex)
{
	if (drawIndex != -1) // DrawMaskedWall needs both clipping ranges
	{
		clipTop = true;
		clipBottom = true;
	}

	DrawSegment segment;
	segment.X1 = x1;
	segment.X2 = x2;
	segment.ClipOffset = (int)ClipValues.size();
	segment.ClipTop = clipTop;
	segment.ClipBottom = clipBottom;
	segment.PlaneNormal = wall.PlaneNormal;
	segment.PlaneD = wall.PlaneD;
	segment.NearZ = wall.NearZ;
	segment.FarZ = wall.FarZ;
	segment.DrawIndex = drawIndex;

	if (clipTop)
	{
		ClipValues.insert(ClipValues.end(), Top + x1, Top + x2);
	}

	if (clipBottom)
	{
		ClipValues.insert(ClipValues.end(), Bottom + x1, Bottom + x2);
	}

	DrawSegments.push_back(segment);
}

void RenderClipBuffer::SetupSpriteClip(short x1, short x2, const DVector3 &pos, bool wallSprite)
{
	for (int i = x1; i < x2; i++)
	{
		Top[i] = 0;
		Bottom[i] = viewheight;
	}

	for (auto it = DrawSegments.crbegin(); it != DrawSegments.crend(); ++it)
	{
		const auto &segment = *it;

		int r1 = MAX<int>(segment.X1, x1);
		int r2 = MIN<int>(segment.X2, x2);
		if (r2 <= r1)
			continue;

		short *clipTop = ClipValues.data() + segment.ClipOffset;
		short *clipBottom = segment.ClipTop ? clipTop + (segment.X2 - segment.X1) : clipTop;

		double side = (pos | segment.PlaneNormal) + segment.PlaneD;
		bool segBehindSprite;
		if (!wallSprite)
			segBehindSprite = (segment.NearZ >= pos.Z) || (segment.FarZ >= pos.Z && side <= 0.0);
		else
			segBehindSprite = side <= 0.0;

		if (segBehindSprite)
		{
			if (segment.DrawIndex != -1 && DrawMaskedWall)
				DrawMaskedWall(r1, r2, segment.DrawIndex, clipTop + (r1 - segment.X1), clipBottom + (r1 - segment.X1));

			if (segment.ClipTop)
			{
				for (int i = r1 - segment.X1; i < r2 - segment.X1; i++)
					clipTop[i] = 0;
			}

			if (segment.ClipBottom)
			{
				for (int i = r1 - segment.X1; i < r2 - segment.X1; i++)
					clipBottom[i] = 0;
			}
		}
		else
		{
			if (segment.ClipTop)
			{
				for (int x = r1; x < r2; x++)
					Top[x] = MAX(clipTop[x - segment.X1], Top[x]);
			}

			if (segment.ClipBottom)
			{
				for (int x = r1; x < r2; x++)
					Bottom[x] = MIN(clipBottom[x - segment.X1], Bottom[x]);
			}
		}
	}
}

void RenderClipBuffer::RenderMaskedWalls()
{
	for (int i = 0; i < viewwidth; i++)
	{
		Top[i] = 0;
		Bottom[i] = viewheight;
	}

	for (auto it = DrawSegments.crbegin(); it != DrawSegments.crend(); ++it)
	{
		const auto &segment = *it;
		if (segment.DrawIndex != -1 && DrawMaskedWall)
		{
			short *clipTop = ClipValues.data() + segment.ClipOffset;
			short *clipBottom = segment.ClipTop ? clipTop + (segment.X2 - segment.X1) : clipTop;
			DrawMaskedWall(segment.X1, segment.X2, segment.DrawIndex, clipTop, clipBottom);
		}
	}
}

/////////////////////////////////////////////////////////////////////////////

VisibleSegmentsIterator::VisibleSegmentsIterator(const RenderClipBuffer &buffer, short startx, short endx) : SolidSegments(buffer.SolidSegments), endx(endx)
{
	X1 = startx;
	X2 = startx;
}

bool VisibleSegmentsIterator::Step()
{
	if (next == 0)
	{
		while (SolidSegments[next].X2 <= X1)
			next++;
		if (SolidSegments[next].X1 <= X1)
			X1 = SolidSegments[next++].X2;
		X2 = MIN(SolidSegments[next].X1, endx);
	}
	else if (X2 == SolidSegments[next].X1 && next + 1 != SolidSegments.size())
	{
		X1 = SolidSegments[next++].X2;
		X2 = MIN(SolidSegments[next].X1, endx);
	}
	else
	{
		X1 = X2;
	}

	return X1 < X2;
}

/////////////////////////////////////////////////////////////////////////////

RenderVisiblePlane::RenderVisiblePlane(VisiblePlane *plane, FTexture *tex)
{
	const auto &key = plane->Key;

	double xscale = key.Transform.xScale * tex->Scale.X;
	double yscale = key.Transform.yScale * tex->Scale.Y;

	double planeang = (key.Transform.Angle + key.Transform.baseAngle).Radians();
	double cosine = cos(planeang);
	double sine = sin(planeang);
	viewx = (key.Transform.xOffs + ViewPos.X * cosine - ViewPos.Y * sine) * xscale;
	viewy = (key.Transform.yOffs - ViewPos.X * sine - ViewPos.Y * cosine) * yscale;

	// left to right mapping
	planeang += (ViewAngle - 90).Radians();

	// Scale will be unit scale at FocalLengthX (normally SCREENWIDTH/2) distance
	double xstep = cos(planeang) / FocalLengthX;
	double ystep = -sin(planeang) / FocalLengthX;

	// [RH] flip for mirrors
	/*if (MirrorFlags & RF_XFLIP)
	{
	xstep = -xstep;
	ystep = -ystep;
	}*/

	planeang += M_PI / 2;
	cosine = cos(planeang);
	sine = -sin(planeang);
	double x = plane->Right - centerx - 0.5;
	double rightxfrac = xscale * (cosine + x * xstep);
	double rightyfrac = yscale * (sine + x * ystep);
	x = plane->Left - centerx - 0.5;
	double leftxfrac = xscale * (cosine + x * xstep);
	double leftyfrac = yscale * (sine + x * ystep);

	basexfrac = rightxfrac;
	baseyfrac = rightyfrac;
	xstepscale = (rightxfrac - leftxfrac) / (plane->Right - plane->Left);
	ystepscale = (rightyfrac - leftyfrac) / (plane->Right - plane->Left);

	planeheight = fabs(key.Plane.Zat0() - ViewPos.Z);
}

void RenderVisiblePlane::Step()
{
	basexfrac -= xstepscale;
	baseyfrac -= ystepscale;
}

/////////////////////////////////////////////////////////////////////////////

void RenderPlanes::Render()
{
	for (int i = 0; i < NumBuckets; i++)
	{
		VisiblePlane *plane = PlaneBuckets[i].get();
		while (plane)
		{
			RenderPlane(plane);
			plane = plane->Next.get();
		}
	}
}

void RenderPlanes::RenderPlane(VisiblePlane *plane)
{
	FTexture *tex = TexMan(plane->Key.Picnum);
	if (tex->UseType == FTexture::TEX_Null)
		return;

	RenderVisiblePlane render(plane, tex);

	short spanend[MAXHEIGHT];
	int x = plane->Right - 1;
	int t2 = plane->Top[x];
	int b2 = plane->Bottom[x];

	if (b2 > t2)
	{
		clearbufshort(spanend + t2, b2 - t2, x);
	}

	for (--x; x >= plane->Left; --x)
	{
		int t1 = plane->Top[x];
		int b1 = plane->Bottom[x];
		const int xr = x + 1;
		int stop;

		// Draw any spans that have just closed
		stop = MIN(t1, b2);
		while (t2 < stop)
		{
			int y = t2++;
			RenderSpan(y, xr, spanend[y], plane->Key, tex, render);
		}
		stop = MAX(b1, t2);
		while (b2 > stop)
		{
			int y = --b2;
			RenderSpan(y, xr, spanend[y], plane->Key, tex, render);
		}

		// Mark any spans that have just opened
		stop = MIN(t2, b1);
		while (t1 < stop)
		{
			spanend[t1++] = x;
		}
		stop = MAX(b2, t2);
		while (b1 > stop)
		{
			spanend[--b1] = x;
		}

		t2 = plane->Top[x];
		b2 = plane->Bottom[x];
		render.Step();
	}
	// Draw any spans that are still open
	while (t2 < b2)
	{
		int y = --b2;
		RenderSpan(y, plane->Left, spanend[y], plane->Key, tex, render);
	}
}

void RenderPlanes::RenderSpan(int y, int x1, int x2, const VisiblePlaneKey &key, FTexture *tex, const RenderVisiblePlane &renderInfo)
{
	if (key.Picnum != skyflatnum)
	{
		double distance = renderInfo.planeheight * yslope[y];

		double u = distance * renderInfo.basexfrac + renderInfo.viewx;
		double v = distance * renderInfo.baseyfrac + renderInfo.viewy;
		double uscale = distance * renderInfo.xstepscale;
		double vscale = distance * renderInfo.ystepscale;

		double vis = r_FloorVisibility / renderInfo.planeheight;

		if (fixedlightlev >= 0)
			R_SetDSColorMapLight(key.ColorMap, 0, FIXEDLIGHT2SHADE(fixedlightlev));
		else if (fixedcolormap)
			R_SetDSColorMapLight(fixedcolormap, 0, 0);
		else
			R_SetDSColorMapLight(key.ColorMap, (float)(vis * fabs(CenterY - y)), LIGHT2SHADE(key.LightLevel));

		ds_source = (const BYTE *)tex->GetPixelsBgra();
		ds_source_mipmapped = false;
		ds_xbits = tex->WidthBits;
		ds_ybits = tex->HeightBits;
		ds_xfrac = (uint32_t)(u * (1 << (32 - ds_xbits)));
		ds_yfrac = (uint32_t)(v * (1 << (32 - ds_ybits)));
		ds_xstep = (uint32_t)(uscale * (1 << (32 - ds_xbits)));
		ds_ystep = (uint32_t)(vscale * (1 << (32 - ds_ybits)));
		ds_y = y;
		ds_x1 = x1;
		ds_x2 = x2;
		R_DrawSpan();
	}
	else
	{
		tex = TexMan(sky1texture, true);

		double xangle1 = ((0.5 - x1 / (double)viewwidth) * FocalTangent * 90.0);
		double xangle2 = ((0.5 - x2 / (double)viewwidth) * FocalTangent * 90.0);

		double u1 = sky1pos + (ViewAngle.Degrees + xangle1) * sky1cyl / 360.0;
		double u2 = sky1pos + (ViewAngle.Degrees + xangle2) * sky1cyl / 360.0;
		double u = u1;
		double v = (y - CenterY) * skyiscale + skytexturemid * tex->Scale.Y;
		double uscale = (u2 - u1) / (x2 - x1);
		double vscale = 0.0;

		if (fixedlightlev >= 0)
			R_SetDSColorMapLight(key.ColorMap, 0, FIXEDLIGHT2SHADE(fixedlightlev));
		else if (fixedcolormap)
			R_SetDSColorMapLight(fixedcolormap, 0, 0);
		else
			R_SetDSColorMapLight(key.ColorMap, 0, 0);

		ds_source = (const BYTE *)tex->GetPixelsBgra();
		ds_source_mipmapped = false;
		ds_xbits = tex->WidthBits;
		ds_ybits = tex->HeightBits;
		ds_xfrac = (uint32_t)(u * (1 << (32 - ds_xbits)));
		ds_yfrac = (uint32_t)(v * (1 << (32 - ds_ybits)));
		ds_xstep = (uint32_t)(uscale * (1 << (32 - ds_xbits)));
		ds_ystep = (uint32_t)(vscale * (1 << (32 - ds_ybits)));
		ds_y = y;
		ds_x1 = x1;
		ds_x2 = x2;
		R_DrawSpan();
	}
}

void RenderPlanes::Clear()
{
	for (int i = 0; i < NumBuckets; i++)
	{
		std::unique_ptr<VisiblePlane> plane = std::move(PlaneBuckets[i]);
		while (plane)
		{
			std::unique_ptr<VisiblePlane> next = std::move(plane->Next);
			FreePlanes.push_back(std::move(plane));
			plane = std::move(next);
		}
	}
}

void RenderPlanes::MarkCeilingPlane(const VisiblePlaneKey &key, const RenderClipBuffer &clip, const WallCoords &wallCoords)
{
	VisibleSegmentsIterator it(clip, wallCoords.ScreenX1, wallCoords.ScreenX2);
	while (it.Step())
	{
		VisiblePlane *plane = GetPlaneWithUnsetRange(key, it.X1, it.X2);

		for (short x = it.X1; x < it.X2; x++)
		{
			short walltop = MAX(wallCoords.Y1(x), clip.Top[x]);
			short top = clip.Top[x];
			short bottom = MIN(walltop, clip.Bottom[x]);
			if (top < bottom)
			{
				plane->Top[x] = top;
				plane->Bottom[x] = bottom;
			}
		}
	}
}

void RenderPlanes::MarkFloorPlane(const VisiblePlaneKey &key, const RenderClipBuffer &clip, const WallCoords &wallCoords)
{
	VisibleSegmentsIterator it(clip, wallCoords.ScreenX1, wallCoords.ScreenX2);
	while (it.Step())
	{
		VisiblePlane *plane = GetPlaneWithUnsetRange(key, it.X1, it.X2);

		for (short x = it.X1; x < it.X2; x++)
		{
			short wallbottom = MIN(wallCoords.Y2(x), clip.Bottom[x]);
			short top = MAX(wallbottom, clip.Top[x]);
			short bottom = clip.Bottom[x];
			if (top < bottom)
			{
				plane->Top[x] = top;
				plane->Bottom[x] = bottom;
			}
		}
	}
}

VisiblePlane *RenderPlanes::GetPlaneWithUnsetRange(const VisiblePlaneKey &key, int start, int stop)
{
	VisiblePlane *plane = GetPlane(key);

	int intrl, intrh;
	int unionl, unionh;

	if (start < plane->Left)
	{
		intrl = plane->Left;
		unionl = start;
	}
	else
	{
		unionl = plane->Left;
		intrl = start;
	}

	if (stop > plane->Right)
	{
		intrh = plane->Right;
		unionh = stop;
	}
	else
	{
		unionh = plane->Right;
		intrh = stop;
	}

	// Verify that the entire range has unset values
	int x = intrl;
	while (x < intrh && plane->Top[x] == VisiblePlane::UnsetValue)
		x++;

	if (x >= intrh) // They do. Use the current plane
	{
		plane->Left = unionl;
		plane->Right = unionh;
		return plane;
	}
	else // Create new plane and make sure it is found first
	{
		auto &bucket = PlaneBuckets[Hash(key)];
		std::unique_ptr<VisiblePlane> newPlane = AllocPlane(key);
		newPlane->Left = start;
		newPlane->Right = stop;
		newPlane->Next = std::move(bucket);
		bucket = std::move(newPlane);
		return bucket.get();
	}
}

VisiblePlane *RenderPlanes::GetPlane(const VisiblePlaneKey &key)
{
	auto &bucket = PlaneBuckets[Hash(key)];
	VisiblePlane *plane = bucket.get();

	while (plane != nullptr)
	{
		if (plane->Key == key)
			return plane;
		plane = plane->Next.get();
	}

	std::unique_ptr<VisiblePlane> new_plane = AllocPlane(key);
	new_plane->Next = std::move(bucket);
	bucket = std::move(new_plane);
	return bucket.get();
}

std::unique_ptr<VisiblePlane> RenderPlanes::AllocPlane(const VisiblePlaneKey &key)
{
	if (!FreePlanes.empty())
	{
		std::unique_ptr<VisiblePlane> plane = std::move(FreePlanes.back());
		FreePlanes.pop_back();
		plane->Clear(key);
		return std::move(plane);
	}
	else
	{
		return std::make_unique<VisiblePlane>(key);
	}
}

/////////////////////////////////////////////////////////////////////////////

void RenderWall::Render(const RenderClipBuffer &clip)
{
	FTexture *tex = GetTexture();
	if (!tex)
		return;
	int texWidth = tex->GetWidth();
	int texHeight = tex->GetHeight();

	WallTextureCoords texcoords(tex, Line, Texpart, TopZ, BottomZ, UnpeggedCeil);

	VisibleSegmentsIterator it(clip, Coords.ScreenX1, Coords.ScreenX2);
	while (it.Step())
	{
		for (short x = it.X1; x < it.X2; x++)
		{
			short y1 = MAX(Coords.Y1(x), clip.Top[x]);
			short y2 = MIN(Coords.Y2(x), clip.Bottom[x]);
			if (y1 >= y2)
				continue;

			double u = Coords.VaryingX(x, texcoords.u1, texcoords.u2);
			double v1 = Coords.VaryingY(x, y1, texcoords.v1, texcoords.v2);
			double v2 = Coords.VaryingY(x, y2, texcoords.v1, texcoords.v2);

			R_SetColorMapLight(Colormap, GetLight(x), GetShade());

			dc_source = (const BYTE *)tex->GetColumnBgra((int)(u * texWidth), nullptr);
			dc_source2 = nullptr;
			dc_textureheight = texHeight;
			dc_texturefrac = (uint32_t)(v1 * 0xffffffff);
			dc_iscale = (uint32_t)((v2 - v1) / (y2 - y1) * 0xffffffff);
			dc_dest = dc_destorg + (ylookup[y1] + x) * 4;
			dc_count = y2 - y1;
			dovline1();
		}
	}
}

void RenderWall::RenderMasked(short x1, short x2, const short *clipTop, const short *clipBottom)
{
	FTexture *tex = GetTexture();
	if (!tex)
		return;
	int texWidth = tex->GetWidth();
	int texHeight = tex->GetHeight();

	WallTextureCoords texcoords(tex, Line, Texpart, TopZ, BottomZ, UnpeggedCeil);

	for (short x = x1; x < x2; x++)
	{
		short y1 = MAX(Coords.Y1(x), clipTop[x - x1]);
		short y2 = MIN(Coords.Y2(x), clipBottom[x - x1]);
		if (y1 >= y2)
			continue;

		double u = Coords.VaryingX(x, texcoords.u1, texcoords.u2);
		double v1 = Coords.VaryingY(x, y1, texcoords.v1, texcoords.v2);
		double v2 = Coords.VaryingY(x, y2, texcoords.v1, texcoords.v2);

		R_SetColorMapLight(Colormap, GetLight(x), GetShade());

		dc_source = (const BYTE *)tex->GetColumnBgra((int)(u * texWidth), nullptr);
		dc_source2 = nullptr;
		dc_textureheight = texHeight;
		dc_texturefrac = (uint32_t)(v1 * 0xffffffff);
		dc_iscale = (uint32_t)((v2 - v1) / (y2 - y1) * 0xffffffff);
		dc_dest = dc_destorg + (ylookup[y1] + x) * 4;
		dc_count = y2 - y1;
		domvline1();
	}
}

FTexture *RenderWall::GetTexture()
{
	FTexture *tex = TexMan(Line->sidedef->GetTexture(Texpart), true);
	if (tex == nullptr || tex->UseType == FTexture::TEX_Null)
		return nullptr;
	else
		return tex;
}

int RenderWall::GetShade()
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

float RenderWall::GetLight(short x)
{
	if (fixedlightlev >= 0 || fixedcolormap)
		return 0.0f;
	else
		return (float)(r_WallVisibility / Coords.Z(x));
}

/////////////////////////////////////////////////////////////////////////////

VisibleSprite::VisibleSprite(AActor *actor, const DVector3 &eyePos) : Actor(actor), EyePos(eyePos)
{
}

void VisibleSprite::Render(RenderClipBuffer *clip)
{
	//if (MirrorFlags & RF_XFLIP)
	//	tx = -tx;

	bool flipTextureX = false;
	FTexture *tex = GetSpriteTexture(Actor, flipTextureX);
	DVector2 spriteScale = Actor->Scale;

	const double thingxscalemul = spriteScale.X / tex->Scale.X;

	double xscale = CenterX / EyePos.Z;
	double yscale = spriteScale.Y / tex->Scale.Y;// spriteScale.Y / tex->Scale.Y * InvZtoScale / EyePos.Z;

	double tx;
	if (flipTextureX)
	{
		tx = EyePos.X - (tex->GetWidth() - tex->LeftOffset - 1) * thingxscalemul;
	}
	else
	{
		tx = EyePos.X - tex->LeftOffset * thingxscalemul;
	}

	double texturemid = tex->TopOffset + (EyePos.Y - Actor->Floorclip) / yscale;
	double y = CenterY - texturemid * (InvZtoScale * yscale / EyePos.Z);

	int x1 = centerx + xs_RoundToInt(tx * xscale);
	int x2 = centerx + xs_RoundToInt((tx + tex->GetWidth() * thingxscalemul) * xscale);
	int y1 = xs_RoundToInt(y);
	int y2 = xs_RoundToInt(y + (InvZtoScale * yscale / EyePos.Z) * tex->GetHeight());

	xscale = spriteScale.X * xscale / tex->Scale.X;

	int clipped_x1 = clamp(x1, 0, viewwidth - 1);
	int clipped_x2 = clamp(x2, 0, viewwidth - 1);
	int clipped_y1 = clamp(y1, 0, viewheight - 1);
	int clipped_y2 = clamp(y2, 0, viewheight - 1);
	if (clipped_x1 >= clipped_x2 || clipped_y1 >= clipped_y2)
		return;

	clip->SetupSpriteClip(clipped_x1, clipped_x2, EyePos, false);

	uint32_t texwidth = tex->GetWidth();
	uint32_t texheight = tex->GetHeight();

	visstyle_t visstyle = GetSpriteVisStyle(Actor, EyePos.Z);
	// Rumor has it that AlterWeaponSprite needs to be called with visstyle passed in somewhere around here..
	R_SetColorMapLight(visstyle.BaseColormap, 0, visstyle.ColormapNum << FRACBITS);

	for (int x = clipped_x1; x < clipped_x2; x++)
	{
		short top = MAX<int>(clipped_y1, clip->Top[x]);
		short bottom = MIN<int>(clipped_y2, clip->Bottom[x]);
		if (top < bottom)
		{
			float u = (x - x1) / (float)(x2 - x1);
			float v = (top - y1) / (float)(y2 - y1);
			if (flipTextureX)
				u = 1.0f - u;
			u = u - floor(u);

			dc_source = (const BYTE *)tex->GetColumnBgra((int)(u * texwidth), nullptr);
			dc_source2 = nullptr;
			dc_textureheight = texheight;
			dc_texturefrac = (uint32_t)(v * 0xffffffff);
			dc_iscale = 0xffffffff / (y2 - y1);
			dc_dest = dc_destorg + (ylookup[top] + x) * 4;
			dc_count = bottom - top;
			domvline1();
		}
	}
}

visstyle_t VisibleSprite::GetSpriteVisStyle(AActor *thing, double z)
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

FTexture *VisibleSprite::GetSpriteTexture(AActor *thing, /*out*/ bool &flipX)
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

/////////////////////////////////////////////////////////////////////////////

void ScreenSprite::Render()
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
