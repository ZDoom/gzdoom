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

extern bool r_showviewer;

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
	savedextralight = extralight;
	savedpos = ViewPos;
	savedangle = ViewAngle;
	savedvisibility = swrenderer::LightVisibility::Instance()->GetVisibility();
	savedcamera = camera;
	savedsector = viewsector;

	if (Portal->mType == PORTS_SKYVIEWPOINT)
	{
		// Don't let gun flashes brighten the sky box
		AActor *sky = Portal->mSkybox;
		extralight = 0;
		swrenderer::LightVisibility::Instance()->SetVisibility(sky->args[0] * 0.25f);
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
	if (Portal->mPartner > 0) level.sectorPortals[Portal->mPartner].mFlags |= PORTSF_INSKYBOX;
}

void PolyDrawSectorPortal::RestoreGlobals()
{
	Portal->mFlags &= ~PORTSF_INSKYBOX;
	if (Portal->mPartner > 0) level.sectorPortals[Portal->mPartner].mFlags &= ~PORTSF_INSKYBOX;

	camera = savedcamera;
	viewsector = savedsector;
	ViewPos = savedpos;
	swrenderer::LightVisibility::Instance()->SetVisibility(savedvisibility);
	extralight = savedextralight;
	ViewAngle = savedangle;
	R_SetViewAngle();
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
	savedextralight = extralight;
	savedpos = ViewPos;
	savedangle = ViewAngle;
	savedcamera = camera;
	savedsector = viewsector;
	savedinvisibility = camera ? (camera->renderflags & RF_INVISIBLE) == RF_INVISIBLE : false;
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

		if (camera)
			camera->renderflags &= ~RF_INVISIBLE;
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
	}

	//camera = nullptr;
	//viewsector = R_PointInSubsector(ViewPos)->sector;
	R_SetViewAngle();

	if (Mirror)
		PolyTriangleDrawer::toggle_mirror();
}

void PolyDrawLinePortal::RestoreGlobals()
{
	if (camera)
	{
		if (savedinvisibility)
			camera->renderflags |= RF_INVISIBLE;
		else
			camera->renderflags &= ~RF_INVISIBLE;
	}
	camera = savedcamera;
	viewsector = savedsector;
	ViewPos = savedpos;
	extralight = savedextralight;
	ViewAngle = savedangle;
	ViewPath[0] = savedViewPath[0];
	ViewPath[1] = savedViewPath[1];
	R_SetViewAngle();

	if (Mirror)
		PolyTriangleDrawer::toggle_mirror();
}
