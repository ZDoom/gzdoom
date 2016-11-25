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
#include "r_data/r_translate.h"
#include "r_poly.h"
#include "gl/data/gl_data.h"

EXTERN_CVAR(Int, screenblocks)
void InitGLRMapinfoData();

/////////////////////////////////////////////////////////////////////////////

RenderPolyScene *RenderPolyScene::Instance()
{
	static RenderPolyScene scene;
	return &scene;
}

void RenderPolyScene::Render()
{
	ClearBuffers();
	SetSceneViewport();
	SetupPerspectiveMatrix();
	MainPortal.SetViewpoint(WorldToClip, GetNextStencilValue());
	MainPortal.Render(0);
	Skydome.Render(WorldToClip);
	MainPortal.RenderTranslucent(0);
	PlayerSprites.Render();

	DrawerCommandQueue::WaitForWorkers();
}

void RenderPolyScene::RenderRemainingPlayerSprites()
{
	PlayerSprites.RenderRemainingSprites();
}

void RenderPolyScene::ClearBuffers()
{
	PolyVertexBuffer::Clear();
	PolyStencilBuffer::Instance()->Clear(RenderTarget->GetWidth(), RenderTarget->GetHeight(), 0);
	PolySubsectorGBuffer::Instance()->Resize(RenderTarget->GetPitch(), RenderTarget->GetHeight());
	NextStencilValue = 0;
}

void RenderPolyScene::SetSceneViewport()
{
	if (RenderTarget == screen) // Rendering to screen
	{
		int height;
		if (screenblocks >= 10)
			height = SCREENHEIGHT;
		else
			height = (screenblocks*SCREENHEIGHT / 10) & ~7;

		int bottom = SCREENHEIGHT - (height + viewwindowy - ((height - viewheight) / 2));
		PolyTriangleDrawer::set_viewport(viewwindowx, SCREENHEIGHT - bottom - height, viewwidth, height, RenderTarget);
	}
	else // Rendering to camera texture
	{
		PolyTriangleDrawer::set_viewport(0, 0, RenderTarget->GetWidth(), RenderTarget->GetHeight(), RenderTarget);
	}
}

void RenderPolyScene::SetupPerspectiveMatrix()
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
