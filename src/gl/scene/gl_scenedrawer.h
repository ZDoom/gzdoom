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

	void SetViewMatrix(const FRotator &angles, float vx, float vy, float vz, bool mirror, bool planemirror);
	void SetupView(FRenderViewpoint &vp, float vx, float vy, float vz, DAngle va, bool mirror, bool planemirror);
	void SetProjection(VSMatrix matrix);
	void Set3DViewport(bool mainview);

	sector_t *RenderViewpoint(FRenderViewpoint &mainvp, AActor * camera, IntRect * bounds, float fov, float ratio, float fovratio, bool mainview, bool toscreen);
	sector_t *RenderView(player_t *player);
	void WriteSavePic(player_t *player, FileWriter *file, int width, int height);
};
