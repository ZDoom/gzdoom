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
#include "gl/data/gl_data.h"
#include "d_net.h"
#include "po_man.h"
#include "st_stuff.h"
#include "swrenderer/scene/r_scene.h"
#include "swrenderer/scene/r_light.h"
#include "swrenderer/drawers/r_draw_rgba.h"
#include "swrenderer/viewport/r_viewport.h"

EXTERN_CVAR(Bool, r_shadercolormaps)
EXTERN_CVAR(Int, screenblocks)
void InitGLRMapinfoData();
extern bool r_showviewer;

/////////////////////////////////////////////////////////////////////////////

PolyRenderer *PolyRenderer::Instance()
{
	static PolyRenderer scene;
	return &scene;
}

PolyRenderer::PolyRenderer() : Thread(nullptr)
{
}

void PolyRenderer::RenderView(player_t *player)
{
	using namespace swrenderer;
	
	auto viewport = RenderViewport::Instance();

	viewport->RenderTarget = screen;

	int width = SCREENWIDTH;
	int height = SCREENHEIGHT;
	int stHeight = gST_Y;
	float trueratio;
	ActiveRatio(width, height, &trueratio);
	viewport->SetViewport(width, height, trueratio);

	RenderActorView(player->mo, false);

	// Apply special colormap if the target cannot do it
	CameraLight *cameraLight = CameraLight::Instance();
	if (cameraLight->ShaderColormap() && viewport->RenderTarget->IsBgra() && !(r_shadercolormaps && screen->Accel2D))
	{
		Thread.DrawQueue->Push<ApplySpecialColormapRGBACommand>(cameraLight->ShaderColormap(), screen);
	}
	
	DrawerThreads::Execute({ Thread.DrawQueue });
}

void PolyRenderer::RenderViewToCanvas(AActor *actor, DCanvas *canvas, int x, int y, int width, int height, bool dontmaplines)
{
	auto viewport = swrenderer::RenderViewport::Instance();
	
	const bool savedviewactive = viewactive;

	viewwidth = width;
	viewport->RenderTarget = canvas;
	R_SetWindow(12, width, height, height, true);
	viewport->SetViewport(width, height, WidescreenRatio);
	viewwindowx = x;
	viewwindowy = y;
	viewactive = true;
	
	canvas->Lock(true);
	
	RenderActorView(actor, dontmaplines);
	DrawerThreads::Execute({ Thread.DrawQueue });
	
	canvas->Unlock();

	viewport->RenderTarget = screen;
	R_ExecuteSetViewSize();
	float trueratio;
	ActiveRatio(width, height, &trueratio);
	viewport->SetViewport(width, height, WidescreenRatio);
	viewactive = savedviewactive;
}

void PolyRenderer::RenderActorView(AActor *actor, bool dontmaplines)
{
	NetUpdate();
	
	DontMapLines = dontmaplines;
	
	P_FindParticleSubsectors();
	PO_LinkToSubsectors();
	R_SetupFrame(actor);
	swrenderer::CameraLight::Instance()->SetCamera(actor);
	swrenderer::RenderViewport::Instance()->SetupFreelook();

	ActorRenderFlags savedflags = camera->renderflags;
	// Never draw the player unless in chasecam mode
	if (!r_showviewer)
		camera->renderflags |= RF_INVISIBLE;

	ClearBuffers();
	SetSceneViewport();
	SetupPerspectiveMatrix();
	MainPortal.SetViewpoint(WorldToClip, Vec4f(0.0f, 0.0f, 0.0f, 1.0f), GetNextStencilValue());
	MainPortal.Render(0);
	Skydome.Render(WorldToClip);
	MainPortal.RenderTranslucent(0);
	PlayerSprites.Render();

	camera->renderflags = savedflags;
	interpolator.RestoreInterpolations ();
	
	NetUpdate();
}

void PolyRenderer::RenderRemainingPlayerSprites()
{
	PlayerSprites.RenderRemainingSprites();
}

void PolyRenderer::ClearBuffers()
{
	PolyVertexBuffer::Clear();
	auto viewport = swrenderer::RenderViewport::Instance();
	PolyStencilBuffer::Instance()->Clear(viewport->RenderTarget->GetWidth(), viewport->RenderTarget->GetHeight(), 0);
	PolySubsectorGBuffer::Instance()->Resize(viewport->RenderTarget->GetPitch(), viewport->RenderTarget->GetHeight());
	NextStencilValue = 0;
	SeenLinePortals.clear();
	SeenMirrors.clear();
}

void PolyRenderer::SetSceneViewport()
{
	using namespace swrenderer;
	
	auto viewport = RenderViewport::Instance();

	if (viewport->RenderTarget == screen) // Rendering to screen
	{
		int height;
		if (screenblocks >= 10)
			height = SCREENHEIGHT;
		else
			height = (screenblocks*SCREENHEIGHT / 10) & ~7;

		int bottom = SCREENHEIGHT - (height + viewwindowy - ((height - viewheight) / 2));
		PolyTriangleDrawer::set_viewport(viewwindowx, SCREENHEIGHT - bottom - height, viewwidth, height, viewport->RenderTarget);
	}
	else // Rendering to camera texture
	{
		PolyTriangleDrawer::set_viewport(0, 0, viewport->RenderTarget->GetWidth(), viewport->RenderTarget->GetHeight(), viewport->RenderTarget);
	}
}

void PolyRenderer::SetupPerspectiveMatrix()
{
	static bool bDidSetup = false;

	if (!bDidSetup)
	{
		InitGLRMapinfoData();
		bDidSetup = true;
	}

	// Code provided courtesy of Graf Zahl. Now we just have to plug it into the viewmatrix code...
	// We have to scale the pitch to account for the pixel stretching, because the playsim doesn't know about this and treats it as 1:1.
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

	WorldToClip = TriMatrix::perspective(fovy, ratio, 5.0f, 65535.0f) * worldToView;
}

bool PolyRenderer::InsertSeenLinePortal(FLinePortal *portal)
{
	return SeenLinePortals.insert(portal).second;
}

bool PolyRenderer::InsertSeenMirror(line_t *mirrorLine)
{
	return SeenMirrors.insert(mirrorLine).second;
}
