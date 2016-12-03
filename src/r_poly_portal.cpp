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
#include "p_maputl.h"
#include "sbar.h"
#include "r_data/r_translate.h"
#include "r_poly_portal.h"
#include "r_poly.h"
#include "gl/data/gl_data.h"

CVAR(Bool, r_debug_cull, 0, 0)
EXTERN_CVAR(Int, r_portal_recursions)

extern bool r_showviewer;

/////////////////////////////////////////////////////////////////////////////

RenderPolyPortal::RenderPolyPortal()
{
}

RenderPolyPortal::~RenderPolyPortal()
{
}

void RenderPolyPortal::SetViewpoint(const TriMatrix &worldToClip, const Vec4f &portalPlane, uint32_t stencilValue)
{
	WorldToClip = worldToClip;
	StencilValue = stencilValue;
	PortalPlane = portalPlane;
}

void RenderPolyPortal::Render(int portalDepth)
{
	ClearBuffers();
	Cull.CullScene(WorldToClip, PortalPlane);
	RenderSectors();
	RenderPortals(portalDepth);
}

void RenderPolyPortal::ClearBuffers()
{
	SeenSectors.clear();
	SubsectorDepths.clear();
	TranslucentObjects.clear();
	SectorPortals.clear();
	LinePortals.clear();
	NextSubsectorDepth = 0;
}

void RenderPolyPortal::RenderSectors()
{
	if (r_debug_cull)
	{
		for (auto it = Cull.PvsSectors.rbegin(); it != Cull.PvsSectors.rend(); ++it)
			RenderSubsector(*it);
	}
	else
	{
		for (auto it = Cull.PvsSectors.begin(); it != Cull.PvsSectors.end(); ++it)
			RenderSubsector(*it);
	}
}

void RenderPolyPortal::RenderSubsector(subsector_t *sub)
{
	sector_t *frontsector = sub->sector;
	frontsector->MoreFlags |= SECF_DRAWN;

	uint32_t subsectorDepth = NextSubsectorDepth++;

	if (sub->sector->CenterFloor() != sub->sector->CenterCeiling())
	{
		RenderPolyPlane::RenderPlanes(WorldToClip, PortalPlane, sub, subsectorDepth, StencilValue, Cull.MaxCeilingHeight, Cull.MinFloorHeight, SectorPortals);
	}

	for (uint32_t i = 0; i < sub->numlines; i++)
	{
		seg_t *line = &sub->firstline[i];
		if (line->sidedef == nullptr || !(line->sidedef->Flags & WALLF_POLYOBJ))
		{
			RenderLine(sub, line, frontsector, subsectorDepth);
		}
	}

	bool mainBSP = ((unsigned int)(sub - subsectors) < (unsigned int)numsubsectors);
	if (mainBSP)
	{
		int subsectorIndex = (int)(sub - subsectors);
		for (int i = ParticlesInSubsec[subsectorIndex]; i != NO_PARTICLE; i = Particles[i].snext)
		{
			particle_t *particle = Particles + i;
			TranslucentObjects.push_back({ particle, sub, subsectorDepth });
		}
	}

	SeenSectors.insert(sub->sector);
	SubsectorDepths[sub] = subsectorDepth;
}

void RenderPolyPortal::RenderSprite(AActor *thing, double sortDistance, const DVector2 &left, const DVector2 &right)
{
	if (numnodes == 0)
		RenderSprite(thing, sortDistance, left, right, 0.0, 1.0, subsectors);
	else
		RenderSprite(thing, sortDistance, left, right, 0.0, 1.0, nodes + numnodes - 1); // The head node is the last node output.
}

void RenderPolyPortal::RenderSprite(AActor *thing, double sortDistance, DVector2 left, DVector2 right, double t1, double t2, void *node)
{
	while (!((size_t)node & 1))  // Keep going until found a subsector
	{
		node_t *bsp = (node_t *)node;
		
		DVector2 planePos(FIXED2DBL(bsp->x), FIXED2DBL(bsp->y));
		DVector2 planeNormal = DVector2(FIXED2DBL(-bsp->dy), FIXED2DBL(bsp->dx));
		double planeD = planeNormal | planePos;
		
		int sideLeft = (left | planeNormal) > planeD;
		int sideRight = (right | planeNormal) > planeD;
		
		if (sideLeft != sideRight)
		{
			double dotLeft = planeNormal | left;
			double dotRight = planeNormal | right;
			double t = (planeD - dotLeft) / (dotRight - dotLeft);
			
			DVector2 mid = left * (1.0 - t) + right * t;
			double tmid = t1 * (1.0 - t) + t2 * t;
			
			RenderSprite(thing, sortDistance, mid, right, tmid, t2, bsp->children[sideRight]);
			right = mid;
			t2 = tmid;
		}
		node = bsp->children[sideLeft];
	}
	
	subsector_t *sub = (subsector_t *)((BYTE *)node - 1);
	
	auto it = SubsectorDepths.find(sub);
	if (it != SubsectorDepths.end())
		TranslucentObjects.push_back({ thing, sub, it->second, sortDistance, (float)t1, (float)t2 });
}

void RenderPolyPortal::RenderLine(subsector_t *sub, seg_t *line, sector_t *frontsector, uint32_t subsectorDepth)
{
	// Reject lines not facing viewer
	DVector2 pt1 = line->v1->fPos() - ViewPos;
	DVector2 pt2 = line->v2->fPos() - ViewPos;
	if (pt1.Y * (pt1.X - pt2.X) + pt1.X * (pt2.Y - pt1.Y) >= 0)
		return;

	// Cull wall if not visible
	int sx1, sx2;
	LineSegmentRange segmentRange = Cull.GetSegmentRangeForLine(line->v1->fX(), line->v1->fY(), line->v2->fX(), line->v2->fY(), sx1, sx2);
	if (segmentRange == LineSegmentRange::NotVisible || (segmentRange == LineSegmentRange::HasSegment && Cull.IsSegmentCulled(sx1, sx2)))
		return;

	// Tell automap we saw this
	if (!swrenderer::r_dontmaplines && line->linedef && segmentRange != LineSegmentRange::AlwaysVisible)
	{
		line->linedef->flags |= ML_MAPPED;
		sub->flags |= SSECF_DRAWN;
	}

	// Render 3D floor sides
	if (line->backsector && frontsector->e && line->backsector->e->XFloor.ffloors.Size())
	{
		for (unsigned int i = 0; i < line->backsector->e->XFloor.ffloors.Size(); i++)
		{
			F3DFloor *fakeFloor = line->backsector->e->XFloor.ffloors[i];
			if (!(fakeFloor->flags & FF_EXISTS)) continue;
			if (!(fakeFloor->flags & FF_RENDERPLANES)) continue;
			if (!fakeFloor->model) continue;
			RenderPolyWall::Render3DFloorLine(WorldToClip, PortalPlane, line, frontsector, subsectorDepth, StencilValue, fakeFloor, TranslucentObjects);
		}
	}

	// Render wall, and update culling info if its an occlusion blocker
	if (RenderPolyWall::RenderLine(WorldToClip, PortalPlane, line, frontsector, subsectorDepth, StencilValue, TranslucentObjects, LinePortals))
	{
		if (segmentRange == LineSegmentRange::HasSegment)
			Cull.MarkSegmentCulled(sx1, sx2);
	}
}

void RenderPolyPortal::RenderPortals(int portalDepth)
{
	if (portalDepth < r_portal_recursions)
	{
		for (auto &portal : SectorPortals)
			portal->Render(portalDepth + 1);

		for (auto &portal : LinePortals)
			portal->Render(portalDepth + 1);
	}
	else // Fill with black
	{
		PolyDrawArgs args;
		args.objectToClip = &WorldToClip;
		args.mode = TriangleDrawMode::Fan;
		args.uniforms.color = 0;
		args.uniforms.light = 256;
		args.uniforms.flags = TriUniforms::fixed_light;
		args.SetClipPlane(PortalPlane.x, PortalPlane.y, PortalPlane.z, PortalPlane.w);

		for (auto &portal : SectorPortals)
		{
			args.stenciltestvalue = portal->StencilValue;
			args.stencilwritevalue = portal->StencilValue + 1;
			for (const auto &verts : portal->Shape)
			{
				args.vinput = verts.Vertices;
				args.vcount = verts.Count;
				args.ccw = verts.Ccw;
				args.uniforms.subsectorDepth = verts.SubsectorDepth;
				PolyTriangleDrawer::draw(args, TriDrawVariant::FillNormal, TriBlendMode::Copy);
			}
		}

		for (auto &portal : LinePortals)
		{
			args.stenciltestvalue = portal->StencilValue;
			args.stencilwritevalue = portal->StencilValue + 1;
			for (const auto &verts : portal->Shape)
			{
				args.vinput = verts.Vertices;
				args.vcount = verts.Count;
				args.ccw = verts.Ccw;
				args.uniforms.subsectorDepth = verts.SubsectorDepth;
				PolyTriangleDrawer::draw(args, TriDrawVariant::FillNormal, TriBlendMode::Copy);
			}
		}
	}
}

void RenderPolyPortal::RenderTranslucent(int portalDepth)
{
	if (portalDepth < r_portal_recursions)
	{
		for (auto it = SectorPortals.rbegin(); it != SectorPortals.rend(); ++it)
		{
			auto &portal = *it;
			portal->RenderTranslucent(portalDepth + 1);
		
			PolyDrawArgs args;
			args.objectToClip = &WorldToClip;
			args.mode = TriangleDrawMode::Fan;
			args.stenciltestvalue = portal->StencilValue + 1;
			args.stencilwritevalue = StencilValue;
			args.SetClipPlane(PortalPlane.x, PortalPlane.y, PortalPlane.z, PortalPlane.w);
			for (const auto &verts : portal->Shape)
			{
				args.vinput = verts.Vertices;
				args.vcount = verts.Count;
				args.ccw = verts.Ccw;
				args.uniforms.subsectorDepth = verts.SubsectorDepth;
				PolyTriangleDrawer::draw(args, TriDrawVariant::StencilClose, TriBlendMode::Copy);
			}
		}

		for (auto it = LinePortals.rbegin(); it != LinePortals.rend(); ++it)
		{
			auto &portal = *it;
			portal->RenderTranslucent(portalDepth + 1);
		
			PolyDrawArgs args;
			args.objectToClip = &WorldToClip;
			args.mode = TriangleDrawMode::Fan;
			args.stenciltestvalue = portal->StencilValue + 1;
			args.stencilwritevalue = StencilValue;
			args.SetClipPlane(PortalPlane.x, PortalPlane.y, PortalPlane.z, PortalPlane.w);
			for (const auto &verts : portal->Shape)
			{
				args.vinput = verts.Vertices;
				args.vcount = verts.Count;
				args.ccw = verts.Ccw;
				args.uniforms.subsectorDepth = verts.SubsectorDepth;
				PolyTriangleDrawer::draw(args, TriDrawVariant::StencilClose, TriBlendMode::Copy);
			}
		}
	}

	for (sector_t *sector : SeenSectors)
	{
		for (AActor *thing = sector->thinglist; thing != nullptr; thing = thing->snext)
		{
			DVector2 left, right;
			if (!RenderPolySprite::GetLine(thing, left, right))
				continue;
			double distanceSquared = (thing->Pos() - ViewPos).LengthSquared();
			RenderSprite(thing, distanceSquared, left, right);
		}
	}

	std::stable_sort(TranslucentObjects.begin(), TranslucentObjects.end());

	for (auto it = TranslucentObjects.rbegin(); it != TranslucentObjects.rend(); ++it)
	{
		auto &obj = *it;
		if (obj.particle)
		{
			RenderPolyParticle spr;
			spr.Render(WorldToClip, PortalPlane, obj.particle, obj.sub, obj.subsectorDepth, StencilValue + 1);
		}
		else if (!obj.thing)
		{
			obj.wall.Render(WorldToClip, PortalPlane);
		}
		else if ((obj.thing->renderflags & RF_SPRITETYPEMASK) == RF_WALLSPRITE)
		{
			RenderPolyWallSprite wallspr;
			wallspr.Render(WorldToClip, PortalPlane, obj.thing, obj.sub, obj.subsectorDepth, StencilValue + 1);
		}
		else
		{
			RenderPolySprite spr;
			spr.Render(WorldToClip, PortalPlane, obj.thing, obj.sub, obj.subsectorDepth, StencilValue + 1, obj.SpriteLeft, obj.SpriteRight);
		}
	}
}

/////////////////////////////////////////////////////////////////////////////

PolyDrawSectorPortal::PolyDrawSectorPortal(FSectorPortal *portal, bool ceiling) : Portal(portal), Ceiling(ceiling)
{
	StencilValue = RenderPolyScene::Instance()->GetNextStencilValue();
}

void PolyDrawSectorPortal::Render(int portalDepth)
{
	if (Portal->mType == PORTS_HORIZON || Portal->mType == PORTS_PLANE)
		return;

	SaveGlobals();

	// To do: get this information from RenderPolyScene instead of duplicating the code..
	double radPitch = ViewPitch.Normalized180().Radians();
	double angx = cos(radPitch);
	double angy = sin(radPitch) * glset.pixelstretch;
	double alen = sqrt(angx*angx + angy*angy);
	float adjustedPitch = (float)asin(angy / alen);
	float adjustedViewAngle = (float)(ViewAngle - 90).Radians();
	float ratio = WidescreenRatio;
	float fovratio = (WidescreenRatio >= 1.3f) ? 1.333333f : ratio;
	float fovy = (float)(2 * DAngle::ToDegrees(atan(tan(FieldOfView.Radians() / 2) / fovratio)).Degrees);
	TriMatrix worldToView =
		TriMatrix::rotate(adjustedPitch, 1.0f, 0.0f, 0.0f) *
		TriMatrix::rotate(adjustedViewAngle, 0.0f, -1.0f, 0.0f) *
		TriMatrix::scale(1.0f, glset.pixelstretch, 1.0f) *
		TriMatrix::swapYZ() *
		TriMatrix::translate((float)-ViewPos.X, (float)-ViewPos.Y, (float)-ViewPos.Z);
	TriMatrix worldToClip = TriMatrix::perspective(fovy, ratio, 5.0f, 65535.0f) * worldToView;

	RenderPortal.SetViewpoint(worldToClip, Vec4f(0.0f, 0.0f, 0.0f, 1.0f), StencilValue);
	RenderPortal.Render(portalDepth);
	
	RestoreGlobals();
}

void PolyDrawSectorPortal::RenderTranslucent(int portalDepth)
{
	if (Portal->mType == PORTS_HORIZON || Portal->mType == PORTS_PLANE)
		return;

	SaveGlobals();
		
	RenderPortal.RenderTranslucent(portalDepth);

	RestoreGlobals();
}

void PolyDrawSectorPortal::SaveGlobals()
{
	savedextralight = extralight;
	savedpos = ViewPos;
	savedangle = ViewAngle;
	savedvisibility = swrenderer::R_GetVisibility();
	savedcamera = camera;
	savedsector = viewsector;

	if (Portal->mType == PORTS_SKYVIEWPOINT)
	{
		// Don't let gun flashes brighten the sky box
		ASkyViewpoint *sky = barrier_cast<ASkyViewpoint*>(Portal->mSkybox);
		extralight = 0;
		swrenderer::R_SetVisibility(sky->args[0] * 0.25f);
		ViewPos = sky->InterpolatedPosition(r_TicFracF);
		ViewAngle = savedangle + (sky->PrevAngles.Yaw + deltaangle(sky->PrevAngles.Yaw, sky->Angles.Yaw) * r_TicFracF);
	}
	else //if (Portal->mType == PORTS_STACKEDSECTORTHING || Portal->mType == PORTS_PORTAL || Portal->mType == PORTS_LINKEDPORTAL)
	{
		//extralight = pl->extralight;
		//swrenderer::R_SetVisibility(pl->visibility);
		ViewPos.X += Portal->mDisplacement.X;
		ViewPos.Y += Portal->mDisplacement.Y;
	}

	camera = nullptr;
	viewsector = Portal->mDestination;
	R_SetViewAngle();

	Portal->mFlags |= PORTSF_INSKYBOX;
	if (Portal->mPartner > 0) sectorPortals[Portal->mPartner].mFlags |= PORTSF_INSKYBOX;
}

void PolyDrawSectorPortal::RestoreGlobals()
{
	Portal->mFlags &= ~PORTSF_INSKYBOX;
	if (Portal->mPartner > 0) sectorPortals[Portal->mPartner].mFlags &= ~PORTSF_INSKYBOX;

	camera = savedcamera;
	viewsector = savedsector;
	ViewPos = savedpos;
	swrenderer::R_SetVisibility(savedvisibility);
	extralight = savedextralight;
	ViewAngle = savedangle;
	R_SetViewAngle();
}

/////////////////////////////////////////////////////////////////////////////

PolyDrawLinePortal::PolyDrawLinePortal(FLinePortal *portal) : Portal(portal)
{
	StencilValue = RenderPolyScene::Instance()->GetNextStencilValue();
}

PolyDrawLinePortal::PolyDrawLinePortal(line_t *mirror) : Mirror(mirror)
{
	StencilValue = RenderPolyScene::Instance()->GetNextStencilValue();
}

void PolyDrawLinePortal::Render(int portalDepth)
{
	SaveGlobals();

	// To do: get this information from RenderPolyScene instead of duplicating the code..
	double radPitch = ViewPitch.Normalized180().Radians();
	double angx = cos(radPitch);
	double angy = sin(radPitch) * glset.pixelstretch;
	double alen = sqrt(angx*angx + angy*angy);
	float adjustedPitch = (float)asin(angy / alen);
	float adjustedViewAngle = (float)(ViewAngle - 90).Radians();
	float ratio = WidescreenRatio;
	float fovratio = (WidescreenRatio >= 1.3f) ? 1.333333f : ratio;
	float fovy = (float)(2 * DAngle::ToDegrees(atan(tan(FieldOfView.Radians() / 2) / fovratio)).Degrees);
	TriMatrix worldToView =
		TriMatrix::rotate(adjustedPitch, 1.0f, 0.0f, 0.0f) *
		TriMatrix::rotate(adjustedViewAngle, 0.0f, -1.0f, 0.0f) *
		TriMatrix::scale(1.0f, glset.pixelstretch, 1.0f) *
		TriMatrix::swapYZ() *
		TriMatrix::translate((float)-ViewPos.X, (float)-ViewPos.Y, (float)-ViewPos.Z);
	TriMatrix worldToClip = TriMatrix::perspective(fovy, ratio, 5.0f, 65535.0f) * worldToView;

	// Calculate plane clipping
	DVector2 planePos = Portal->mDestination->v1->fPos();
	DVector2 planeNormal = (Portal->mDestination->v2->fPos() - Portal->mDestination->v1->fPos()).Rotated90CW();
	planeNormal.MakeUnit();
	double planeD = -(planeNormal | (planePos + planeNormal * 0.001));
	Vec4f portalPlane((float)planeNormal.X, (float)planeNormal.Y, 0.0f, (float)planeD);

	RenderPortal.SetViewpoint(worldToClip, portalPlane, StencilValue);
	RenderPortal.Render(portalDepth);

	RestoreGlobals();
}

void PolyDrawLinePortal::RenderTranslucent(int portalDepth)
{
	SaveGlobals();
	RenderPortal.RenderTranslucent(portalDepth);
	RestoreGlobals();
}

void PolyDrawLinePortal::SaveGlobals()
{
	savedextralight = extralight;
	savedpos = ViewPos;
	savedangle = ViewAngle;
	savedcamera = camera;
	savedsector = viewsector;
	savedvisibility = camera ? camera->renderflags & RF_INVISIBLE : ActorRenderFlags::FromInt(0);
	savedViewPath[0] = ViewPath[0];
	savedViewPath[1] = ViewPath[1];

	if (Mirror)
	{
		DAngle startang = ViewAngle;
		DVector3 startpos = ViewPos;

		vertex_t *v1 = Mirror->v1;

		// Reflect the current view behind the mirror.
		if (Mirror->Delta().X == 0)
		{ // vertical mirror
			ViewPos.X = v1->fX() - startpos.X + v1->fX();
		}
		else if (Mirror->Delta().Y == 0)
		{ // horizontal mirror
			ViewPos.Y = v1->fY() - startpos.Y + v1->fY();
		}
		else
		{ // any mirror
			vertex_t *v2 = Mirror->v2;

			double dx = v2->fX() - v1->fX();
			double dy = v2->fY() - v1->fY();
			double x1 = v1->fX();
			double y1 = v1->fY();
			double x = startpos.X;
			double y = startpos.Y;

			// the above two cases catch len == 0
			double r = ((x - x1)*dx + (y - y1)*dy) / (dx*dx + dy*dy);

			ViewPos.X = (x1 + r * dx) * 2 - x;
			ViewPos.Y = (y1 + r * dy) * 2 - y;
		}
		ViewAngle = Mirror->Delta().Angle() * 2 - startang;
	}
	else
	{
		auto src = Portal->mOrigin;
		auto dst = Portal->mDestination;

		P_TranslatePortalXY(src, ViewPos.X, ViewPos.Y);
		P_TranslatePortalZ(src, ViewPos.Z);
		P_TranslatePortalAngle(src, ViewAngle);
		P_TranslatePortalXY(src, ViewPath[0].X, ViewPath[0].Y);
		P_TranslatePortalXY(src, ViewPath[1].X, ViewPath[1].Y);

		if (!r_showviewer && camera && P_PointOnLineSidePrecise(ViewPath[0], dst) != P_PointOnLineSidePrecise(ViewPath[1], dst))
		{
			double distp = (ViewPath[0] - ViewPath[1]).Length();
			if (distp > EQUAL_EPSILON)
			{
				double dist1 = (ViewPos - ViewPath[0]).Length();
				double dist2 = (ViewPos - ViewPath[1]).Length();

				if (dist1 + dist2 < distp + 1)
				{
					camera->renderflags |= RF_INVISIBLE;
				}
			}
		}

		/*if (Portal->mirror)
		{
			if (MirrorFlags & RF_XFLIP) MirrorFlags &= ~RF_XFLIP;
			else MirrorFlags |= RF_XFLIP;
		}*/
	}

	camera = nullptr;
	//viewsector = R_PointInSubsector(ViewPos)->sector;
	R_SetViewAngle();
}

void PolyDrawLinePortal::RestoreGlobals()
{
	if (!savedvisibility && camera) camera->renderflags &= ~RF_INVISIBLE;
	camera = savedcamera;
	viewsector = savedsector;
	ViewPos = savedpos;
	extralight = savedextralight;
	ViewAngle = savedangle;
	ViewPath[0] = savedViewPath[0];
	ViewPath[1] = savedViewPath[1];
	R_SetViewAngle();
}
