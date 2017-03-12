#pragma once

#include "r_defs.h"
#include "m_fixed.h"
#include "gl_clipper.h"
#include "gl_portal.h"

class GLSceneDrawer
{
	fixed_t viewx, viewy;	// since the nodes are still fixed point, keeping the view position  also fixed point for node traversal is faster.
	
	subsector_t *currentsubsector;	// used by the line processing code.
	sector_t *currentsector;

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

	sector_t *RenderViewpoint(AActor * camera, GL_IRECT * bounds, float fov, float ratio, float fovratio, bool mainview, bool toscreen);
	void RenderView(player_t *player);
	void WriteSavePic(player_t *player, FileWriter *file, int width, int height);

	void InitClipper(angle_t a1, angle_t a2)
	{
		clipper.SafeAddClipRangeRealAngles(a1, a2);
	}

	void SetView()
	{
		viewx = FLOAT2FIXED(r_viewpoint.Pos.X);
		viewy = FLOAT2FIXED(r_viewpoint.Pos.Y);
	}
};
