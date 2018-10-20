#ifndef __GL_DRAWINFO_H
#define __GL_DRAWINFO_H

#include "gl/renderer/gl_lightdata.h"
#include "hwrenderer/scene/hw_drawlist.h"
#include "hwrenderer/scene/hw_weapon.h"
#include "hwrenderer/scene/hw_viewpointuniforms.h"

#include "gl/renderer/gl_renderstate.h"	// temporary

#ifdef _MSC_VER
#pragma warning(disable:4244)
#endif

enum DrawListType
{
	GLDL_PLAINWALLS,
	GLDL_PLAINFLATS,
	GLDL_MASKEDWALLS,
	GLDL_MASKEDFLATS,
	GLDL_MASKEDWALLSOFS,
	GLDL_MODELS,
	
	GLDL_TRANSLUCENT,
	GLDL_TRANSLUCENTBORDER,
	
	GLDL_TYPES,
};

enum Drawpasses
{
	GLPASS_ALL,			// Main pass with dynamic lights
	GLPASS_DECALS,		// Draws a decal
	GLPASS_TRANSLUCENT,	// Draws translucent objects
};

struct FDrawInfo : public HWDrawInfo
{
	HWDrawList drawlists[GLDL_TYPES];
	TArray<HUDSprite> hudsprites;	// These may just be stored by value.
	TArray<GLDecal *> decals[2];	// the second slot is for mirrors which get rendered in a separate pass.
	int vpIndex;
	
	void ApplyVPUniforms() override;

	void AddWall(GLWall *wall) override;
    void AddMirrorSurface(GLWall *w) override;
	GLDecal *AddDecal(bool onmirror) override;
	void AddPortal(GLWall *w, int portaltype) override;
	void AddFlat(GLFlat *flat, bool fog) override;
	void AddSprite(GLSprite *sprite, bool translucent) override;
	void AddHUDSprite(HUDSprite *huds) override;

	std::pair<FFlatVertex *, unsigned int> AllocVertices(unsigned int count) override;
	int UploadLights(FDynLightData &data) override;

	void Draw(EDrawType dt, FRenderState &state, int index, int count, bool apply = true);
	void DrawIndexed(EDrawType dt, FRenderState &state, int index, int count, bool apply = true);

	void DrawDecal(GLDecal *gldecal);
	void DrawDecals();
	void DrawDecalsForMirror(GLWall *wall);

	void StartScene();
	void SetupFloodStencil(int vindex);
	void ClearFloodStencil(int vindex);
	void DrawFloodedPlane(wallseg * ws, float planez, sector_t * sec, bool ceiling);
	void FloodUpperGap(seg_t * seg);
	void FloodLowerGap(seg_t * seg);

	// Wall drawer
	void RenderWall(GLWall *wall, int textured);
	void RenderFogBoundary(GLWall *wall);
	void RenderMirrorSurface(GLWall *wall);
	void RenderTranslucentWall(GLWall *wall);
	void RenderTexturedWall(GLWall *wall, int rflags);
	void DrawWall(GLWall *wall, int pass) override;

	// Sprite drawer
	void DrawSprite(GLSprite *sprite, int pass);
	void DrawPSprite(HUDSprite *huds);
	void DrawPlayerSprites(bool hudModelStep);

	void DoDrawSorted(HWDrawList *dl, SortNode * head);
	void DrawSorted(int listindex);

	// These two may be moved to the API independent part of the renderer later.
	void AddSubsectorToPortal(FSectorPortalGroup *portal, subsector_t *sub) override;
    
    void CreateScene();
    void RenderScene(int recursion);
    void RenderTranslucent();
    void DrawScene(int drawmode);
    void ProcessScene(bool toscreen = false);
    void EndDrawScene(sector_t * viewsector);
    void DrawEndScene2D(sector_t * viewsector);
	bool SetDepthClamp(bool on) override;

	static FDrawInfo *StartDrawInfo(FRenderViewpoint &parentvp, HWViewpointUniforms *uniforms);
	FDrawInfo *EndDrawInfo();
	
	void SetColor(int light, int rellight, const FColormap &cm, float alpha, bool weapon = false)
	{
		gl_RenderState.SetColor(light, rellight, isFullbrightScene(), cm, alpha, weapon);
	}

	void SetFog(int lightlevel, int rellight, const FColormap *cmap, bool isadditive)
	{
		gl_RenderState.SetFog(lightlevel, rellight, isFullbrightScene(), cmap, isadditive);
	}

};


void gl_SetRenderStyle(FRenderStyle style, bool drawopaque, bool allowcolorblending);

#endif
