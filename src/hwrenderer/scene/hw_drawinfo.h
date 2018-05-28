#pragma once

#include <atomic>
#include "r_defs.h"


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
class IPortal;
class FFlatVertexGenerator;

//==========================================================================
//
// these are used to link faked planes due to missing textures to a sector
//
//==========================================================================
struct gl_subsectorrendernode
{
	gl_subsectorrendernode *	next;
	subsector_t *				sub;
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


struct HWDrawInfo
{
	virtual ~HWDrawInfo() {}

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
    
    int FixedColormap;
	std::atomic<int> spriteindex;
	IPortal *mClipPortal;
	FRotator mAngles;
	FVector2 mViewVector;
	AActor *mViewActor;
	IShadowMap *mShadowMap;
	Clipper *mClipper;

	TArray<MissingTextureInfo> MissingUpperTextures;
	TArray<MissingTextureInfo> MissingLowerTextures;

	TArray<MissingSegInfo> MissingUpperSegs;
	TArray<MissingSegInfo> MissingLowerSegs;

	TArray<SubsectorHackInfo> SubsectorHacks;

	TArray<gl_subsectorrendernode*> otherfloorplanes;
	TArray<gl_subsectorrendernode*> otherceilingplanes;

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
	FFlatVertexGenerator *mVBO;	// this class needs access because the sector vertex updating is part of BSP traversal.


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
public:
	void RenderBSPNode(void *node);

	void ClearBuffers();
	void SetViewArea();

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
	void DrawUnhandledMissingTextures();
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

	virtual void DrawWall(GLWall *wall, int pass) = 0;
	virtual void DrawFlat(GLFlat *flat, int pass, bool trans) = 0;
	virtual void DrawSprite(GLSprite *sprite, int pass) = 0;

	virtual void FloodUpperGap(seg_t * seg) = 0;
	virtual void FloodLowerGap(seg_t * seg) = 0;
	virtual void ProcessLowerMinisegs(TArray<seg_t *> &lowersegs) = 0;
    virtual void AddSubsectorToPortal(FSectorPortalGroup *portal, subsector_t *sub) = 0;
    
    virtual void AddWall(GLWall *w) = 0;
	virtual void AddPortal(GLWall *w, int portaltype) = 0;
    virtual void AddMirrorSurface(GLWall *w) = 0;
	virtual void AddFlat(GLFlat *flat, bool fog) = 0;
	virtual void AddSprite(GLSprite *sprite, bool translucent) = 0;
	virtual void AddHUDSprite(HUDSprite *huds) = 0;

	virtual int UploadLights(FDynLightData &data) = 0;

    virtual GLDecal *AddDecal(bool onmirror) = 0;
	virtual std::pair<FFlatVertex *, unsigned int> AllocVertices(unsigned int count) = 0;

};

