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
#include "st_stuff.h"
#include "r_data/r_translate.h"
#include "r_data/r_interpolate.h"
#include "poly_renderer.h"
#include "d_net.h"
#include "po_man.h"
#include "st_stuff.h"
#include "g_levellocals.h"
#include "p_effect.h"
#include "polyrenderer/scene/poly_light.h"
#include "swrenderer/scene/r_scene.h"
#include "swrenderer/drawers/r_draw_rgba.h"
#include "swrenderer/viewport/r_viewport.h"
#include "swrenderer/r_swcolormaps.h"

EXTERN_CVAR(Int, screenblocks)
EXTERN_CVAR(Float, r_visibility)

/////////////////////////////////////////////////////////////////////////////

PolyRenderer *PolyRenderer::Instance()
{
	static PolyRenderer scene;
	return &scene;
}

PolyRenderer::PolyRenderer()
{
}

void PolyRenderer::RenderView(player_t *player, DCanvas *target)
{
	using namespace swrenderer;
	
	R_ExecuteSetViewSize(Viewpoint, Viewwindow);

	RenderTarget = target;
	RenderToCanvas = false;

	RenderActorView(player->mo, false);

	Threads.MainThread()->FlushDrawQueue();
	PolyDrawerWaitCycles.Clock();
	DrawerThreads::WaitForWorkers();
	PolyDrawerWaitCycles.Unclock();
}

void PolyRenderer::RenderViewToCanvas(AActor *actor, DCanvas *canvas, int x, int y, int width, int height, bool dontmaplines)
{
	// Save a bunch of silly globals:
	auto savedViewpoint = Viewpoint;
	auto savedViewwindow = Viewwindow;
	auto savedviewwindowx = viewwindowx;
	auto savedviewwindowy = viewwindowy;
	auto savedviewwidth = viewwidth;
	auto savedviewheight = viewheight;
	auto savedviewactive = viewactive;
	auto savedRenderTarget = RenderTarget;

	// Setup the view:
	RenderTarget = canvas;
	RenderToCanvas = true;
	R_SetWindow(Viewpoint, Viewwindow, 12, width, height, height, true);
	viewwindowx = x;
	viewwindowy = y;
	viewactive = true;
	
	// Render:
	RenderActorView(actor, dontmaplines);
	Threads.MainThread()->FlushDrawQueue();
	DrawerThreads::WaitForWorkers();

	RenderToCanvas = false;

	// Restore silly globals:
	Viewpoint = savedViewpoint;
	Viewwindow = savedViewwindow;
	viewwindowx = savedviewwindowx;
	viewwindowy = savedviewwindowy;
	viewwidth = savedviewwidth;
	viewheight = savedviewheight;
	viewactive = savedviewactive;
	RenderTarget = savedRenderTarget;
}

void PolyRenderer::RenderActorView(AActor *actor, bool dontmaplines)
{
	PolyTotalBatches = 0;
	PolyTotalTriangles = 0;
	PolyTotalDrawCalls = 0;
	PolyCullCycles.Reset();
	PolyOpaqueCycles.Reset();
	PolyMaskedCycles.Reset();
	PolyDrawerWaitCycles.Reset();

	NetUpdate();
	
	DontMapLines = dontmaplines;
	
	R_SetupFrame(Viewpoint, Viewwindow, actor);
	P_FindParticleSubsectors();
	PO_LinkToSubsectors();

	static bool firstcall = true;
	if (firstcall)
	{
		swrenderer::R_InitFuzzTable(RenderTarget->GetPitch());
		firstcall = false;
	}

	swrenderer::R_UpdateFuzzPosFrameStart();

	if (APART(R_OldBlend)) NormalLight.Maps = realcolormaps.Maps;
	else NormalLight.Maps = realcolormaps.Maps + NUMCOLORMAPS * 256 * R_OldBlend;

	Light.SetVisibility(Viewwindow, r_visibility);

	PolyCameraLight::Instance()->SetCamera(Viewpoint, RenderTarget, actor);
	//Viewport->SetupFreelook();

	ActorRenderFlags savedflags = 0;
	if (Viewpoint.camera)
	{
		savedflags = Viewpoint.camera->renderflags;

		// Never draw the player unless in chasecam mode
		if (!Viewpoint.showviewer)
			Viewpoint.camera->renderflags |= RF_INVISIBLE;
	}

	ScreenTriangle::FuzzStart = (ScreenTriangle::FuzzStart + 14) % FUZZTABLE;

	ClearBuffers();
	SetSceneViewport();

	PolyPortalViewpoint mainViewpoint = SetupPerspectiveMatrix();
	mainViewpoint.StencilValue = GetNextStencilValue();
	Scene.CurrentViewpoint = &mainViewpoint;
	Scene.Render(&mainViewpoint);
	PlayerSprites.Render(Threads.MainThread());
	Scene.CurrentViewpoint = nullptr;

	if (Viewpoint.camera)
		Viewpoint.camera->renderflags = savedflags;
	interpolator.RestoreInterpolations ();
	
	NetUpdate();
}

void PolyRenderer::RenderRemainingPlayerSprites()
{
	PlayerSprites.RenderRemainingSprites();
}

void PolyRenderer::ClearBuffers()
{
	Threads.Clear();
	PolyTriangleDrawer::ClearBuffers(RenderTarget);
	NextStencilValue = 0;
	Threads.MainThread()->SectorPortals.clear();
	Threads.MainThread()->LinePortals.clear();
	Threads.MainThread()->TranslucentObjects.clear();
}

void PolyRenderer::SetSceneViewport()
{
	using namespace swrenderer;
	
	if (!RenderToCanvas) // Rendering to screen
	{
		int height;
		if (screenblocks >= 10)
			height = SCREENHEIGHT;
		else
			height = (screenblocks*SCREENHEIGHT / 10) & ~7;

		int bottom = SCREENHEIGHT - (height + viewwindowy - ((height - viewheight) / 2));
		PolyTriangleDrawer::SetViewport(Threads.MainThread()->DrawQueue, viewwindowx, SCREENHEIGHT - bottom - height, viewwidth, height, RenderTarget);
	}
	else // Rendering to camera texture
	{
		PolyTriangleDrawer::SetViewport(Threads.MainThread()->DrawQueue, 0, 0, RenderTarget->GetWidth(), RenderTarget->GetHeight(), RenderTarget);
	}
}

PolyPortalViewpoint PolyRenderer::SetupPerspectiveMatrix(bool mirror)
{
	// We have to scale the pitch to account for the pixel stretching, because the playsim doesn't know about this and treats it as 1:1.
	double radPitch = Viewpoint.Angles.Pitch.Normalized180().Radians();
	double angx = cos(radPitch);
	double angy = sin(radPitch) * level.info->pixelstretch;
	double alen = sqrt(angx*angx + angy*angy);
	float adjustedPitch = (float)asin(angy / alen);
	float adjustedViewAngle = (float)(Viewpoint.Angles.Yaw - 90).Radians();

	float ratio = Viewwindow.WidescreenRatio;
	float fovratio = (Viewwindow.WidescreenRatio >= 1.3f) ? 1.333333f : ratio;

	float fovy = (float)(2 * DAngle::ToDegrees(atan(tan(Viewpoint.FieldOfView.Radians() / 2) / fovratio)).Degrees);

	PolyPortalViewpoint portalViewpoint;

	portalViewpoint.WorldToView =
		Mat4f::Rotate((float)Viewpoint.Angles.Roll.Radians(), 0.0f, 0.0f, 1.0f) *
		Mat4f::Rotate(adjustedPitch, 1.0f, 0.0f, 0.0f) *
		Mat4f::Rotate(adjustedViewAngle, 0.0f, -1.0f, 0.0f) *
		Mat4f::Scale(1.0f, level.info->pixelstretch, 1.0f) *
		Mat4f::SwapYZ() *
		Mat4f::Translate((float)-Viewpoint.Pos.X, (float)-Viewpoint.Pos.Y, (float)-Viewpoint.Pos.Z);

	portalViewpoint.Mirror = mirror;

	if (mirror)
		portalViewpoint.WorldToView = Mat4f::Scale(-1.0f, 1.0f, 1.0f) * portalViewpoint.WorldToView;

	portalViewpoint.WorldToClip = Mat4f::Perspective(fovy, ratio, 5.0f, 65535.0f, Handedness::Right, ClipZRange::NegativePositiveW) * portalViewpoint.WorldToView;

	return portalViewpoint;
}

cycle_t PolyCullCycles, PolyOpaqueCycles, PolyMaskedCycles, PolyDrawerWaitCycles;
int PolyTotalBatches, PolyTotalTriangles, PolyTotalDrawCalls;

ADD_STAT(polyfps)
{
	FString out;
	out.Format("frame=%04.1f ms  cull=%04.1f ms  opaque=%04.1f ms  masked=%04.1f ms  drawers=%04.1f ms",
		FrameCycles.TimeMS(), PolyCullCycles.TimeMS(), PolyOpaqueCycles.TimeMS(), PolyMaskedCycles.TimeMS(), PolyDrawerWaitCycles.TimeMS());
	out.AppendFormat("\nbatches drawn: %d  triangles drawn: %d  drawcalls: %d", PolyTotalBatches, PolyTotalTriangles, PolyTotalDrawCalls);
	return out;
}
