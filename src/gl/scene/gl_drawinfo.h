#ifndef __GL_DRAWINFO_H
#define __GL_DRAWINFO_H

#include "hwrenderer/scene/hw_drawlist.h"
#include "hwrenderer/scene/hw_weapon.h"

#ifdef _MSC_VER
#pragma warning(disable:4244)
#endif

class GLSceneDrawer;

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

// more lists for handling of dynamic lights
enum DLDrawListType
{
	// These are organized so that the various multipass rendering modes have to be set as few times as possible
	GLLDL_WALLS_PLAIN,			// dynamic lights on normal walls
	GLLDL_WALLS_MASKED,			// dynamic lights on masked midtextures
	
	GLLDL_FLATS_PLAIN,			// dynamic lights on normal flats
	GLLDL_FLATS_MASKED,			// dynamic lights on masked flats
	
	GLLDL_WALLS_FOG,			// lights on fogged walls
	GLLDL_WALLS_FOGMASKED,		// lights on fogged masked midtextures
	
	GLLDL_FLATS_FOG,			// lights on fogged walls
	GLLDL_FLATS_FOGMASKED,		// lights on fogged masked midtextures
	
	GLLDL_TYPES,
};


enum Drawpasses
{
	GLPASS_ALL,			// Main pass with dynamic lights
	GLPASS_LIGHTSONLY,	// only collect dynamic lights
	GLPASS_DECALS,		// Draws a decal
	GLPASS_TRANSLUCENT,	// Draws translucent objects
	
	// these are only used with texture based dynamic lights
	GLPASS_BASE,		// untextured base for dynamic lights
	GLPASS_BASE_MASKED,	// same but with active texture
	GLPASS_LIGHTTEX,	// lighttexture pass
	GLPASS_TEXONLY,		// finishing texture pass
	GLPASS_LIGHTTEX_ADDITIVE,	// lighttexture pass (additive)
	GLPASS_LIGHTTEX_FOGGY,	// lighttexture pass on foggy surfaces (forces all lights to be additive)
	
};

struct FDrawInfo : public HWDrawInfo
{
	GLSceneDrawer *mDrawer;
	
	FDrawInfo * next;
	HWDrawList drawlists[GLDL_TYPES];
	TArray<HUDSprite> hudsprites;	// These may just be stored by value.
	TArray<GLDecal *> decals[2];	// the second slot is for mirrors which get rendered in a separate pass.
	HWDrawList *dldrawlists = NULL;	// only gets allocated when needed.
	
	FDrawInfo();
	~FDrawInfo();
	
	void AddWall(GLWall *wall) override;
    void AddMirrorSurface(GLWall *w) override;
	GLDecal *AddDecal(bool onmirror) override;
	void AddPortal(GLWall *w, int portaltype) override;
	void AddFlat(GLFlat *flat, bool fog) override;
	void AddSprite(GLSprite *sprite, bool translucent) override;
	void AddHUDSprite(HUDSprite *huds) override;

	std::pair<FFlatVertex *, unsigned int> AllocVertices(unsigned int count) override;
	int UploadLights(FDynLightData &data) override;

	// Legacy GL only. 
	bool PutWallCompat(GLWall *wall, int passflag);
	bool PutFlatCompat(GLFlat *flat, bool fog);
	void RenderFogBoundaryCompat(GLWall *wall);
	void RenderLightsCompat(GLWall *wall, int pass);
	void DrawSubsectorLights(GLFlat *flat, subsector_t * sub, int pass);
	void DrawLightsCompat(GLFlat *flat, int pass);


	void DrawDecal(GLDecal *gldecal);
	void DrawDecals();
	void DrawDecalsForMirror(GLWall *wall);

	void StartScene();
	void SetupFloodStencil(wallseg * ws);
	void ClearFloodStencil(wallseg * ws);
	void DrawFloodedPlane(wallseg * ws, float planez, sector_t * sec, bool ceiling);
	void FloodUpperGap(seg_t * seg) override;
	void FloodLowerGap(seg_t * seg) override;

	// Wall drawer
	void RenderWall(GLWall *wall, int textured);
	void RenderFogBoundary(GLWall *wall);
	void RenderMirrorSurface(GLWall *wall);
	void RenderTranslucentWall(GLWall *wall);
	void RenderTexturedWall(GLWall *wall, int rflags);
	void DrawWall(GLWall *wall, int pass) override;

	// Flat drawer
	void DrawFlat(GLFlat *flat, int pass, bool trans) override;	// trans only has meaning for GLPASS_LIGHTSONLY
	void DrawSkyboxSector(GLFlat *flat, int pass, bool processlights);
	void DrawSubsectors(GLFlat *flat, int pass, bool processlights, bool istrans);
	void ProcessLights(GLFlat *flat, bool istrans);
	void DrawSubsector(GLFlat *flat, subsector_t * sub);
	void SetupSubsectorLights(GLFlat *flat, int pass, subsector_t * sub, int *dli);
	void SetupSectorLights(GLFlat *flat, int pass, int *dli);

	// Sprite drawer
	void DrawSprite(GLSprite *sprite, int pass);
	void DrawPSprite(HUDSprite *huds);
	void DrawPlayerSprites(bool hudModelStep);

	void DoDrawSorted(HWDrawList *dl, SortNode * head);
	void DrawSorted(int listindex);

	// These two may be moved to the API independent part of the renderer later.
	void ProcessLowerMinisegs(TArray<seg_t *> &lowersegs) override;
	void AddSubsectorToPortal(FSectorPortalGroup *portal, subsector_t *sub) override;

	static FDrawInfo *StartDrawInfo(GLSceneDrawer *drawer);
	static void EndDrawInfo();
	
	gl_subsectorrendernode * GetOtherFloorPlanes(unsigned int sector)
	{
		if (sector<otherfloorplanes.Size()) return otherfloorplanes[sector];
		else return NULL;
	}
	
	gl_subsectorrendernode * GetOtherCeilingPlanes(unsigned int sector)
	{
		if (sector<otherceilingplanes.Size()) return otherceilingplanes[sector];
		else return NULL;
	}
};

class FDrawInfoList
{
	TDeletingArray<FDrawInfo *> mList;
	
public:
	
	FDrawInfo *GetNew();
	void Release(FDrawInfo *);
};


void gl_SetRenderStyle(FRenderStyle style, bool drawopaque, bool allowcolorblending);

#endif
