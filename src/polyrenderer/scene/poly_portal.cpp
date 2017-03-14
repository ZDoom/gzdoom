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
#include "g_levellocals.h"
#include "r_data/r_translate.h"
#include "poly_portal.h"
#include "polyrenderer/poly_renderer.h"
#include "swrenderer/scene/r_light.h"
#include "gl/data/gl_data.h"

/////////////////////////////////////////////////////////////////////////////

PolyDrawSectorPortal::PolyDrawSectorPortal(FSectorPortal *portal, bool ceiling) : Portal(portal), Ceiling(ceiling)
{
	StencilValue = PolyRenderer::Instance()->GetNextStencilValue();
}

void PolyDrawSectorPortal::Render(int portalDepth)
{
	if (Portal->mType == PORTS_HORIZON || Portal->mType == PORTS_PLANE)
		return;

	SaveGlobals();

	// To do: get this information from PolyRenderer instead of duplicating the code..
	const auto &viewpoint = PolyRenderer::Instance()->Thread.Viewport->viewpoint;
	const auto &viewwindow = PolyRenderer::Instance()->Thread.Viewport->viewwindow;
	double radPitch = viewpoint.Angles.Pitch.Normalized180().Radians();
	double angx = cos(radPitch);
	double angy = sin(radPitch) * level.info->pixelstretch;
	double alen = sqrt(angx*angx + angy*angy);
	float adjustedPitch = (float)asin(angy / alen);
	float adjustedViewAngle = (float)(viewpoint.Angles.Yaw - 90).Radians();
	float ratio = viewwindow.WidescreenRatio;
	float fovratio = (viewwindow.WidescreenRatio >= 1.3f) ? 1.333333f : ratio;
	float fovy = (float)(2 * DAngle::ToDegrees(atan(tan(viewpoint.FieldOfView.Radians() / 2) / fovratio)).Degrees);
	TriMatrix worldToView =
		TriMatrix::rotate(adjustedPitch, 1.0f, 0.0f, 0.0f) *
		TriMatrix::rotate(adjustedViewAngle, 0.0f, -1.0f, 0.0f) *
		TriMatrix::scale(1.0f, level.info->pixelstretch, 1.0f) *
		TriMatrix::swapYZ() *
		TriMatrix::translate((float)-viewpoint.Pos.X, (float)-viewpoint.Pos.Y, (float)-viewpoint.Pos.Z);
	TriMatrix worldToClip = TriMatrix::perspective(fovy, ratio, 5.0f, 65535.0f) * worldToView;

	RenderPortal.SetViewpoint(worldToClip, PortalPlane, StencilValue);
	RenderPortal.SetPortalSegments(Segments);
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
	auto &viewpoint = PolyRenderer::Instance()->Thread.Viewport->viewpoint;
	const auto &viewwindow = PolyRenderer::Instance()->Thread.Viewport->viewwindow;

	savedextralight = viewpoint.extralight;
	savedpos = viewpoint.Pos;
	savedangles = viewpoint.Angles;
	savedvisibility = PolyRenderer::Instance()->Thread.Light->GetVisibility();
	savedcamera = viewpoint.camera;
	savedsector = viewpoint.sector;

	if (Portal->mType == PORTS_SKYVIEWPOINT)
	{
		// Don't let gun flashes brighten the sky box
		AActor *sky = Portal->mSkybox;
		viewpoint.extralight = 0;
		PolyRenderer::Instance()->Thread.Light->SetVisibility(PolyRenderer::Instance()->Thread.Viewport.get(), sky->args[0] * 0.25f);
		viewpoint.Pos = sky->InterpolatedPosition(viewpoint.TicFrac);
		viewpoint.Angles.Yaw = savedangles.Yaw + (sky->PrevAngles.Yaw + deltaangle(sky->PrevAngles.Yaw, sky->Angles.Yaw) * viewpoint.TicFrac);
	}
	else //if (Portal->mType == PORTS_STACKEDSECTORTHING || Portal->mType == PORTS_PORTAL || Portal->mType == PORTS_LINKEDPORTAL)
	{
		//extralight = pl->extralight;
		//swrenderer::R_SetVisibility(pl->visibility);
		viewpoint.Pos.X += Portal->mDisplacement.X;
		viewpoint.Pos.Y += Portal->mDisplacement.Y;
	}

	viewpoint.camera = nullptr;
	viewpoint.sector = Portal->mDestination;
	R_SetViewAngle(viewpoint, viewwindow);

	Portal->mFlags |= PORTSF_INSKYBOX;
	if (Portal->mPartner > 0) level.sectorPortals[Portal->mPartner].mFlags |= PORTSF_INSKYBOX;
}

void PolyDrawSectorPortal::RestoreGlobals()
{
	Portal->mFlags &= ~PORTSF_INSKYBOX;
	if (Portal->mPartner > 0) level.sectorPortals[Portal->mPartner].mFlags &= ~PORTSF_INSKYBOX;

	auto &viewpoint = PolyRenderer::Instance()->Thread.Viewport->viewpoint;
	const auto &viewwindow = PolyRenderer::Instance()->Thread.Viewport->viewwindow;

	viewpoint.camera = savedcamera;
	viewpoint.sector = savedsector;
	viewpoint.Pos = savedpos;
	PolyRenderer::Instance()->Thread.Light->SetVisibility(PolyRenderer::Instance()->Thread.Viewport.get(), savedvisibility);
	viewpoint.extralight = savedextralight;
	viewpoint.Angles = savedangles;
	R_SetViewAngle(viewpoint, viewwindow);
}

/////////////////////////////////////////////////////////////////////////////

PolyDrawLinePortal::PolyDrawLinePortal(FLinePortal *portal) : Portal(portal)
{
	StencilValue = PolyRenderer::Instance()->GetNextStencilValue();
}

PolyDrawLinePortal::PolyDrawLinePortal(line_t *mirror) : Mirror(mirror)
{
	StencilValue = PolyRenderer::Instance()->GetNextStencilValue();
}

void PolyDrawLinePortal::Render(int portalDepth)
{
	SaveGlobals();

	// To do: get this information from PolyRenderer instead of duplicating the code..
	const auto &viewpoint = PolyRenderer::Instance()->Thread.Viewport->viewpoint;
	const auto &viewwindow = PolyRenderer::Instance()->Thread.Viewport->viewwindow;
	double radPitch = viewpoint.Angles.Pitch.Normalized180().Radians();
	double angx = cos(radPitch);
	double angy = sin(radPitch) * level.info->pixelstretch;
	double alen = sqrt(angx*angx + angy*angy);
	float adjustedPitch = (float)asin(angy / alen);
	float adjustedViewAngle = (float)(viewpoint.Angles.Yaw - 90).Radians();
	float ratio = viewwindow.WidescreenRatio;
	float fovratio = (viewwindow.WidescreenRatio >= 1.3f) ? 1.333333f : ratio;
	float fovy = (float)(2 * DAngle::ToDegrees(atan(tan(viewpoint.FieldOfView.Radians() / 2) / fovratio)).Degrees);
	TriMatrix worldToView =
		TriMatrix::rotate(adjustedPitch, 1.0f, 0.0f, 0.0f) *
		TriMatrix::rotate(adjustedViewAngle, 0.0f, -1.0f, 0.0f) *
		TriMatrix::scale(1.0f, level.info->pixelstretch, 1.0f) *
		TriMatrix::swapYZ() *
		TriMatrix::translate((float)-viewpoint.Pos.X, (float)-viewpoint.Pos.Y, (float)-viewpoint.Pos.Z);
	if (Mirror)
		worldToView = TriMatrix::scale(-1.0f, 1.0f, 1.0f) * worldToView;
	TriMatrix worldToClip = TriMatrix::perspective(fovy, ratio, 5.0f, 65535.0f) * worldToView;

	// Calculate plane clipping
	line_t *clipLine = Portal ? Portal->mDestination : Mirror;
	DVector2 planePos = clipLine->v1->fPos();
	DVector2 planeNormal = (clipLine->v2->fPos() - clipLine->v1->fPos()).Rotated90CW();
	planeNormal.MakeUnit();
	double planeD = -(planeNormal | (planePos + planeNormal * 0.001));
	Vec4f portalPlane((float)planeNormal.X, (float)planeNormal.Y, 0.0f, (float)planeD);

	RenderPortal.SetViewpoint(worldToClip, portalPlane, StencilValue);
	RenderPortal.SetPortalSegments(Segments);
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
	auto &viewpoint = PolyRenderer::Instance()->Thread.Viewport->viewpoint;
	const auto &viewwindow = PolyRenderer::Instance()->Thread.Viewport->viewwindow;

	savedextralight = viewpoint.extralight;
	savedpos = viewpoint.Pos;
	savedangles = viewpoint.Angles;
	savedcamera = viewpoint.camera;
	savedsector = viewpoint.sector;
	savedinvisibility = viewpoint.camera ? (viewpoint.camera->renderflags & RF_INVISIBLE) == RF_INVISIBLE : false;
	savedViewPath[0] = viewpoint.Path[0];
	savedViewPath[1] = viewpoint.Path[1];

	if (Mirror)
	{
		DAngle startang = viewpoint.Angles.Yaw;
		DVector3 startpos = viewpoint.Pos;

		vertex_t *v1 = Mirror->v1;

		// Reflect the current view behind the mirror.
		if (Mirror->Delta().X == 0)
		{ // vertical mirror
			viewpoint.Pos.X = v1->fX() - startpos.X + v1->fX();
		}
		else if (Mirror->Delta().Y == 0)
		{ // horizontal mirror
			viewpoint.Pos.Y = v1->fY() - startpos.Y + v1->fY();
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

			viewpoint.Pos.X = (x1 + r * dx) * 2 - x;
			viewpoint.Pos.Y = (y1 + r * dy) * 2 - y;
		}
		viewpoint.Angles.Yaw = Mirror->Delta().Angle() * 2 - startang;

		if (viewpoint.camera)
			viewpoint.camera->renderflags &= ~RF_INVISIBLE;
	}
	else
	{
		auto src = Portal->mOrigin;
		auto dst = Portal->mDestination;

		P_TranslatePortalXY(src, viewpoint.Pos.X, viewpoint.Pos.Y);
		P_TranslatePortalZ(src, viewpoint.Pos.Z);
		P_TranslatePortalAngle(src, viewpoint.Angles.Yaw);
		P_TranslatePortalXY(src, viewpoint.Path[0].X, viewpoint.Path[0].Y);
		P_TranslatePortalXY(src, viewpoint.Path[1].X, viewpoint.Path[1].Y);

		if (!viewpoint.showviewer && viewpoint.camera && P_PointOnLineSidePrecise(viewpoint.Path[0], dst) != P_PointOnLineSidePrecise(viewpoint.Path[1], dst))
		{
			double distp = (viewpoint.Path[0] - viewpoint.Path[1]).Length();
			if (distp > EQUAL_EPSILON)
			{
				double dist1 = (viewpoint.Pos - viewpoint.Path[0]).Length();
				double dist2 = (viewpoint.Pos - viewpoint.Path[1]).Length();

				if (dist1 + dist2 < distp + 1)
				{
					viewpoint.camera->renderflags |= RF_INVISIBLE;
				}
			}
		}
	}

	//camera = nullptr;
	//viewsector = R_PointInSubsector(ViewPos)->sector;
	R_SetViewAngle(viewpoint, viewwindow);

	if (Mirror)
		PolyTriangleDrawer::toggle_mirror();
}

void PolyDrawLinePortal::RestoreGlobals()
{
	auto &viewpoint = PolyRenderer::Instance()->Thread.Viewport->viewpoint;
	const auto &viewwindow = PolyRenderer::Instance()->Thread.Viewport->viewwindow;
	if (viewpoint.camera)
	{
		if (savedinvisibility)
			viewpoint.camera->renderflags |= RF_INVISIBLE;
		else
			viewpoint.camera->renderflags &= ~RF_INVISIBLE;
	}
	viewpoint.camera = savedcamera;
	viewpoint.sector = savedsector;
	viewpoint.Pos = savedpos;
	viewpoint.extralight = savedextralight;
	viewpoint.Angles = savedangles;
	viewpoint.Path[0] = savedViewPath[0];
	viewpoint.Path[1] = savedViewPath[1];
	R_SetViewAngle(viewpoint, viewwindow);

	if (Mirror)
		PolyTriangleDrawer::toggle_mirror();
}
