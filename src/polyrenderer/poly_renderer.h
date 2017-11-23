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

#pragma once

#include <vector>
#include <memory>
#include <algorithm>
#include <functional>
#include "doomdata.h"
#include "r_utility.h"
#include "scene/poly_portal.h"
#include "scene/poly_playersprite.h"
#include "scene/poly_sky.h"
#include "scene/poly_light.h"
#include "swrenderer/r_memory.h"
#include "poly_renderthread.h"

class AActor;
class DCanvas;
class DrawerCommandQueue;
typedef std::shared_ptr<DrawerCommandQueue> DrawerCommandQueuePtr;

class PolyRenderer
{
public:
	PolyRenderer();
	
	void RenderView(player_t *player);
	void RenderViewToCanvas(AActor *actor, DCanvas *canvas, int x, int y, int width, int height, bool dontmaplines);
	void RenderRemainingPlayerSprites();

	static PolyRenderer *Instance();
	
	uint32_t GetNextStencilValue() { uint32_t value = NextStencilValue; NextStencilValue += 2; return value; }

	bool DontMapLines = false;
	
	PolyRenderThreads Threads;
	DCanvas *RenderTarget = nullptr;
	FViewWindow Viewwindow;
	FRenderViewpoint Viewpoint;
	PolyLightVisibility Light;

	TriMatrix WorldToView;
	TriMatrix WorldToClip;

private:
	void RenderActorView(AActor *actor, bool dontmaplines);
	void ClearBuffers();
	void SetSceneViewport();
	void SetupPerspectiveMatrix();

	RenderPolyScene MainPortal;
	PolySkyDome Skydome;
	RenderPolyPlayerSprites PlayerSprites;
	uint32_t NextStencilValue = 0;
};
