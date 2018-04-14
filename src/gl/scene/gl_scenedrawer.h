#pragma once

#include "r_defs.h"
#include "m_fixed.h"
#include "gl_clipper.h"
#include "gl_portal.h"
#include "gl/renderer/gl_lightdata.h"
#include "gl/renderer/gl_renderer.h"
#include "r_utility.h"
#include "c_cvars.h"

EXTERN_CVAR(Int, gl_weaponlight);

inline	int getExtraLight()
{
	return r_viewpoint.extralight * gl_weaponlight;
}


class GLSceneDrawer
{
	fixed_t viewx, viewy;	// since the nodes are still fixed point, keeping the view position  also fixed point for node traversal is faster.
	
	subsector_t *currentsubsector;	// used by the line processing code.
	sector_t *currentsector;

	TMap<DPSprite*, int> weapondynlightindex;

	void SetupWeaponLight();

	void RenderMultipassStuff();
	
	void UnclipSubsector(subsector_t *sub);
	void AddLine (seg_t *seg, bool portalclip);
	void PolySubsector(subsector_t * sub);
	void RenderPolyBSPNode (void *node);
	void AddPolyobjs(subsector_t *sub);
	void AddLines(subsector_t * sub, sector_t * sector);
	void AddSpecialPortalLines(subsector_t * sub, sector_t * sector, line_t *line);
	void RenderThings(subsector_t * sub, sector_t * sector);
	void DoSubsector(subsector_t * sub);
	void RenderBSPNode(void *node);

	void RenderScene(int recursion);
	void RenderTranslucent();

	void CreateScene();

public:
	GLSceneDrawer()
	{
		GLPortal::drawer = this;
	}

	Clipper clipper;
	int		FixedColormap;
	area_t	in_area;
	BitArray CurrentMapSections;	// this cannot be a single number, because a group of portals with the same displacement may link different sections.

	angle_t FrustumAngle();
	void SetViewMatrix(float vx, float vy, float vz, bool mirror, bool planemirror);
	void SetViewArea();
	void SetupView(float vx, float vy, float vz, DAngle va, bool mirror, bool planemirror);
	void SetViewAngle(DAngle viewangle);
	void SetProjection(VSMatrix matrix);
	void Set3DViewport(bool mainview);
	void Reset3DViewport();
	void SetFixedColormap(player_t *player);
	void DrawScene(int drawmode);
	void ProcessScene(bool toscreen = false);
	void DrawBlend(sector_t * viewsector);
	void EndDrawScene(sector_t * viewsector);
	void DrawEndScene2D(sector_t * viewsector);
	void RenderActorsInPortal(FLinePortalSpan *glport);

	void CheckViewArea(vertex_t *v1, vertex_t *v2, sector_t *frontsector, sector_t *backsector);

	sector_t *RenderViewpoint(AActor * camera, GL_IRECT * bounds, float fov, float ratio, float fovratio, bool mainview, bool toscreen);
	void RenderView(player_t *player);
	void WriteSavePic(player_t *player, FileWriter *file, int width, int height);

	void DrawPSprite(player_t * player, DPSprite *psp, float sx, float sy, bool hudModelStep, int OverrideShader, bool alphatexture);
	void DrawPlayerSprites(sector_t * viewsector, bool hudModelStep);
	void DrawTargeterSprites();
	
	void InitClipper(angle_t a1, angle_t a2)
	{
		clipper.Clear();
		clipper.SafeAddClipRangeRealAngles(a1, a2);
	}

	void SetView()
	{
		viewx = FLOAT2FIXED(r_viewpoint.Pos.X);
		viewy = FLOAT2FIXED(r_viewpoint.Pos.Y);
	}

	void SetColor(int light, int rellight, const FColormap &cm, float alpha, bool weapon = false)
	{
		gl_SetColor(light, rellight, FixedColormap != CM_DEFAULT, cm, alpha, weapon);
	}

	bool CheckFog(sector_t *frontsector, sector_t *backsector)
	{
		if (FixedColormap != CM_DEFAULT) return false;
		return gl_CheckFog(frontsector, backsector);
	}

	void SetFog(int lightlevel, int rellight, const FColormap *cmap, bool isadditive)
	{
		gl_SetFog(lightlevel, rellight, FixedColormap != CM_DEFAULT, cmap, isadditive);
	}

	inline bool isFullbright(PalEntry color, int lightlevel)
	{
		return FixedColormap != CM_DEFAULT || (gl_isWhite(color) && lightlevel == 255);
	}
};
