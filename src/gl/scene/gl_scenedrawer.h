#pragma once

#include "r_defs.h"
#include "m_fixed.h"
#include "hwrenderer/scene/hw_clipper.h"
#include "gl_portal.h"
#include "gl/renderer/gl_lightdata.h"
#include "gl/renderer/gl_renderer.h"
#include "r_utility.h"
#include "c_cvars.h"

struct HUDSprite;

class GLSceneDrawer
{
public:
	GLSceneDrawer()
	{
		GLPortal::drawer = this;
	}

	void SetupView(FRenderViewpoint &vp, float vx, float vy, float vz, DAngle va, bool mirror, bool planemirror);

	sector_t *RenderView(player_t *player);
};
