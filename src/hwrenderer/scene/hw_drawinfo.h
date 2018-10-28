#pragma once

#include <atomic>
#include "vectors.h"
#include "r_defs.h"
#include "r_utility.h"
#include "hw_viewpointuniforms.h"
#include "v_video.h"
#include "hw_weapon.h"
#include "hw_drawlist.h"

enum EDrawMode
{
	DM_MAINVIEW,
	DM_OFFSCREEN,
	DM_PORTAL,
	DM_SKYPORTAL
};


enum EDrawType
{
	DT_Points = 0,
	DT_Lines = 1,
	DT_Triangles = 2,
	DT_TriangleFan = 3,
	DT_TriangleStrip = 4
};

enum EDepthFunc
{
	DF_Less,
	DF_LEqual,
	DF_Always
};

enum EStencilFlags
{
	SF_AllOn = 0,
	SF_ColorMaskOff = 1,
	SF_DepthMaskOff = 2,
	SF_DepthTestOff = 4,
	SF_DepthClear = 8
};

enum EStencilOp
{
	SOP_Keep = 0,
	SOP_Increment = 1,
	SOP_Decrement = 2
};

enum ECull
{
	Cull_None,
	Cull_CCW,
	Cull_CW
};

struct FSectorPortalGroup;
struct FLinePortalSpan;
struct FFlatVertex;
class GLWall;
class GLFlat;
class GLSprite;
struct GLDecal;
class IShadowMap;
struct particle_t;
struct FDynLightData;
struct HUDSprite;
class Clipper;
class HWPortal;
class FFlatVertexBuffer;
class IRenderQueue;
class HWScenePortalBase;
class FRenderState;

//==========================================================================
//
// these are used to link faked planes due to missing textures to a sector
//
//==========================================================================
struct gl_subsectorrendernode
{
	gl_subsectorrendernode *	next;
	subsector_t *				sub;
	int							lightindex;
	int							vertexindex;
};

struct gl_floodrendernode
{
	gl_floodrendernode * next;
	seg_t *seg;
	int vertexindex;
	// This will use the light list of the originating sector.
};

enum area_t : int;

enum SectorRenderFlags
{
    // This is used to merge several subsectors into a single draw item
    SSRF_RENDERFLOOR = 1,
    SSRF_RENDERCEILING = 2,
    SSRF_RENDER3DPLANES = 4,
    SSRF_RENDERALL = 7,
    SSRF_PROCESSED = 8,
    SSRF_SEEN = 16,
};

enum EPortalClip
{
	PClip_InFront,
	PClip_Inside,
	PClip_Behind,
};

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


struct HWDrawInfo
{
	virtual ~HWDrawInfo() 
	{
		ClearBuffers();
	}

	struct wallseg
	{
		float x1, y1, z1, x2, y2, z2;
	};

	struct MissingTextureInfo
	{
		seg_t * seg;
		subsector_t * sub;
		float Planez;
		float Planezfront;
	};

	struct MissingSegInfo
	{
		seg_t * seg;
		int MTI_Index;    // tells us which MissingTextureInfo represents this seg.
	};

	struct SubsectorHackInfo
	{
		subsector_t * sub;
		uint8_t flags;
	};

	enum EFullbrightFlags
	{
		Fullbright = 1,
		Nightvision = 2,
		StealthVision = 4
	};

	bool isFullbrightScene() const { return !!(FullbrightFlags & Fullbright); }
	bool isNightvision() const { return !!(FullbrightFlags & Nightvision); }
	bool isStealthVision() const { return !!(FullbrightFlags & StealthVision); }
    
	HWDrawList drawlists[GLDL_TYPES];
	int vpIndex;

	HWDrawInfo * outer = nullptr;
	int FullbrightFlags;
	std::atomic<int> spriteindex;
	HWScenePortalBase *mClipPortal;
	HWPortal *mCurrentPortal;
	//FRotator mAngles;
	IShadowMap *mShadowMap;
	Clipper *mClipper;
	FRenderViewpoint Viewpoint;
	HWViewpointUniforms VPUniforms;	// per-viewpoint uniform state
	TArray<HWPortal *> Portals;
	TArray<GLDecal *> Decals[2];	// the second slot is for mirrors which get rendered in a separate pass.
	TArray<HUDSprite> hudsprites;	// These may just be stored by value.

	TArray<MissingTextureInfo> MissingUpperTextures;
	TArray<MissingTextureInfo> MissingLowerTextures;

	TArray<MissingSegInfo> MissingUpperSegs;
	TArray<MissingSegInfo> MissingLowerSegs;

	TArray<SubsectorHackInfo> SubsectorHacks;

	TArray<gl_subsectorrendernode*> otherfloorplanes;
	TArray<gl_subsectorrendernode*> otherceilingplanes;
	TArray<gl_floodrendernode*> floodfloorsegs;
	TArray<gl_floodrendernode*> floodceilingsegs;

	TArray<sector_t *> CeilingStacks;
	TArray<sector_t *> FloorStacks;

	TArray<subsector_t *> HandledSubsectors;

	TArray<uint8_t> sectorrenderflags;
	TArray<uint8_t> ss_renderflags;
	TArray<uint8_t> no_renderflags;

	// This is needed by the BSP traverser.
	BitArray CurrentMapSections;	// this cannot be a single number, because a group of portals with the same displacement may link different sections.
	area_t	in_area;
	fixed_t viewx, viewy;	// since the nodes are still fixed point, keeping the view position  also fixed point for node traversal is faster.


private:
    // For ProcessLowerMiniseg
    bool inview;
    subsector_t * viewsubsector;
    TArray<seg_t *> lowersegs;

	subsector_t *currentsubsector;	// used by the line processing code.
	sector_t *currentsector;

    sector_t fakesec;    // this is a struct member because it gets used in recursively called functions so it cannot be put on the stack.


	void UnclipSubsector(subsector_t *sub);
	void AddLine(seg_t *seg, bool portalclip);
	void PolySubsector(subsector_t * sub);
	void RenderPolyBSPNode(void *node);
	void AddPolyobjs(subsector_t *sub);
	void AddLines(subsector_t * sub, sector_t * sector);
	void AddSpecialPortalLines(subsector_t * sub, sector_t * sector, line_t *line);
	void RenderThings(subsector_t * sub, sector_t * sector);
	void DoSubsector(subsector_t * sub);
	int SetupLightsForOtherPlane(subsector_t * sub, FDynLightData &lightdata, const secplane_t *plane);
	int CreateOtherPlaneVertices(subsector_t *sub, const secplane_t *plane);
	void DrawPSprite(HUDSprite *huds, FRenderState &state);
public:

	gl_subsectorrendernode * GetOtherFloorPlanes(unsigned int sector)
	{
		if (sector<otherfloorplanes.Size()) return otherfloorplanes[sector];
		else return nullptr;
	}

	gl_subsectorrendernode * GetOtherCeilingPlanes(unsigned int sector)
	{
		if (sector<otherceilingplanes.Size()) return otherceilingplanes[sector];
		else return nullptr;
	}

	gl_floodrendernode * GetFloodFloorSegs(unsigned int sector)
	{
		if (sector<floodfloorsegs.Size()) return floodfloorsegs[sector];
		else return nullptr;
	}

	gl_floodrendernode * GetFloodCeilingSegs(unsigned int sector)
	{
		if (sector<floodceilingsegs.Size()) return floodceilingsegs[sector];
		else return nullptr;
	}



	void SetCameraPos(const DVector3 &pos)
	{
		VPUniforms.mCameraPos = { (float)pos.X, (float)pos.Z, (float)pos.Y, 0.f };
	}

	void SetClipHeight(float h, float d)
	{
		VPUniforms.mClipHeight = h;
		VPUniforms.mClipHeightDirection = d;
		VPUniforms.mClipLine.X = -1000001.f;
	}

	void SetClipLine(line_t *line)
	{
		VPUniforms.mClipLine = { (float)line->v1->fX(), (float)line->v1->fY(), (float)line->Delta().X, (float)line->Delta().Y };
		VPUniforms.mClipHeight = 0;
	}

	bool ClipLineShouldBeActive()
	{
		return (screen->hwcaps & RFL_NO_CLIP_PLANES) && VPUniforms.mClipLine.X > -1000000.f;
	}

	HWPortal * FindPortal(const void * src);
	void RenderBSPNode(void *node);

	static HWDrawInfo *StartDrawInfo(FRenderViewpoint &parentvp, HWViewpointUniforms *uniforms);
	void StartScene(FRenderViewpoint &parentvp, HWViewpointUniforms *uniforms);
	void ClearBuffers();
	HWDrawInfo *EndDrawInfo();
	void SetViewArea();
	int SetFullbrightFlags(player_t *player);

	void RenderScene(FRenderState &state);
	void RenderTranslucent(FRenderState &state);
	void RenderPortal(HWPortal *p, FRenderState &state, bool usestencil);

	bool DoOneSectorUpper(subsector_t * subsec, float planez, area_t in_area);
	bool DoOneSectorLower(subsector_t * subsec, float planez, area_t in_area);
	bool DoFakeBridge(subsector_t * subsec, float planez, area_t in_area);
	bool DoFakeCeilingBridge(subsector_t * subsec, float planez, area_t in_area);

	bool CheckAnchorFloor(subsector_t * sub);
	bool CollectSubsectorsFloor(subsector_t * sub, sector_t * anchor);
	bool CheckAnchorCeiling(subsector_t * sub);
	bool CollectSubsectorsCeiling(subsector_t * sub, sector_t * anchor);
	void CollectSectorStacksCeiling(subsector_t * sub, sector_t * anchor, area_t in_area);
	void CollectSectorStacksFloor(subsector_t * sub, sector_t * anchor, area_t in_area);

	void AddUpperMissingTexture(side_t * side, subsector_t *sub, float backheight);
	void AddLowerMissingTexture(side_t * side, subsector_t *sub, float backheight);
	void HandleMissingTextures(area_t in_area);
	void PrepareUnhandledMissingTextures();
	void PrepareUpperGap(seg_t * seg);
	void PrepareLowerGap(seg_t * seg);
	void CreateFloodPoly(wallseg * ws, FFlatVertex *vertices, float planez, sector_t * sec, bool ceiling);
	void CreateFloodStencilPoly(wallseg * ws, FFlatVertex *vertices);

	void AddHackedSubsector(subsector_t * sub);
	void HandleHackedSubsectors();
	void AddFloorStack(sector_t * sec);
	void AddCeilingStack(sector_t * sec);
	void ProcessSectorStacks(area_t in_area);

	void ProcessActorsInPortal(FLinePortalSpan *glport, area_t in_area);

	void AddOtherFloorPlane(int sector, gl_subsectorrendernode * node);
	void AddOtherCeilingPlane(int sector, gl_subsectorrendernode * node);

	void GetDynSpriteLight(AActor *self, float x, float y, float z, FLightNode *node, int portalgroup, float *out);
	void GetDynSpriteLight(AActor *thing, particle_t *particle, float *out);

	void PreparePlayerSprites(sector_t * viewsector, area_t in_area);
	void PrepareTargeterSprites();

	void UpdateCurrentMapSection();
	void SetViewMatrix(const FRotator &angles, float vx, float vy, float vz, bool mirror, bool planemirror);
	void SetupView(FRenderState &state, float vx, float vy, float vz, bool mirror, bool planemirror);
	angle_t FrustumAngle();

	void DrawDecals(FRenderState &state, TArray<GLDecal *> &decals);
	void DrawPlayerSprites(bool hudModelStep, FRenderState &state);

	void ProcessLowerMinisegs(TArray<seg_t *> &lowersegs);
    void AddSubsectorToPortal(FSectorPortalGroup *portal, subsector_t *sub);
    
    void AddWall(GLWall *w);
    void AddMirrorSurface(GLWall *w);
	void AddFlat(GLFlat *flat, bool fog);
	void AddSprite(GLSprite *sprite, bool translucent);


    GLDecal *AddDecal(bool onmirror);

	virtual void DrawScene(int drawmode) = 0;
};

