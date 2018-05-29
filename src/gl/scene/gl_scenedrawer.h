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
	TMap<DPSprite*, int> weapondynlightindex;

	void RenderMultipassStuff(FDrawInfo *di);
	
	void RenderScene(FDrawInfo *di, int recursion);
	void RenderTranslucent(FDrawInfo *di);

	void CreateScene(FDrawInfo *di);

public:
	GLSceneDrawer()
	{
		GLPortal::drawer = this;
	}

	int		FixedColormap;

	angle_t FrustumAngle();
	void SetViewMatrix(float vx, float vy, float vz, bool mirror, bool planemirror);
	void SetupView(float vx, float vy, float vz, DAngle va, bool mirror, bool planemirror);
	void SetViewAngle(DAngle viewangle);
	void SetProjection(VSMatrix matrix);
	void Set3DViewport(bool mainview);
	void Reset3DViewport();
	void SetFixedColormap(player_t *player);
	void DrawScene(FDrawInfo *di, int drawmode);
	void ProcessScene(FDrawInfo *di, bool toscreen = false);
	void EndDrawScene(FDrawInfo *di, sector_t * viewsector);
	void DrawEndScene2D(FDrawInfo *di, sector_t * viewsector);

	sector_t *RenderViewpoint(AActor * camera, IntRect * bounds, float fov, float ratio, float fovratio, bool mainview, bool toscreen);
	sector_t *RenderView(player_t *player);
	void WriteSavePic(player_t *player, FileWriter *file, int width, int height);

	void SetColor(int light, int rellight, const FColormap &cm, float alpha, bool weapon = false)
	{
		gl_SetColor(light, rellight, FixedColormap != CM_DEFAULT, cm, alpha, weapon);
	}

	bool CheckFog(sector_t *frontsector, sector_t *backsector)
	{
		if (FixedColormap != CM_DEFAULT) return false;
		return hw_CheckFog(frontsector, backsector);
	}

	void SetFog(int lightlevel, int rellight, const FColormap *cmap, bool isadditive)
	{
		gl_SetFog(lightlevel, rellight, FixedColormap != CM_DEFAULT, cmap, isadditive);
	}
};
