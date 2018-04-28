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
	bool clipPortal;
	FRotator mAngles;
	FVector2 mViewVector;
	AActor *mViewActor;

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

private:
    // For ProcessLowerMiniseg
    bool inview;
    subsector_t * viewsubsector;
    TArray<seg_t *> lowersegs;
    
    sector_t fakesec;    // this is a struct member because it gets used in recursively called functions so it cannot be put on the stack.
public:

	void ClearBuffers();

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

	virtual void FloodUpperGap(seg_t * seg) = 0;
	virtual void FloodLowerGap(seg_t * seg) = 0;
	virtual void ProcessLowerMinisegs(TArray<seg_t *> &lowersegs) = 0;
    virtual void AddSubsectorToPortal(FSectorPortalGroup *portal, subsector_t *sub) = 0;
    
    virtual void AddWall(GLWall *w) = 0;
	virtual void AddPortal(GLWall *w, int portaltype) = 0;
    virtual void AddMirrorSurface(GLWall *w) = 0;
	virtual void AddFlat(GLFlat *flat, bool fog) = 0;
	virtual void AddSprite(GLSprite *sprite, bool translucent) = 0;

    virtual GLDecal *AddDecal(bool onmirror) = 0;
	virtual std::pair<FFlatVertex *, unsigned int> AllocVertices(unsigned int count) = 0;

	virtual int ClipPoint(const DVector3 &pos) = 0;


};

