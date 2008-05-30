#ifndef R_INTERPOLATE_H
#define R_INTERPOLATE_H

#include "doomtype.h"

// BUILD stuff for interpolating between frames, but modified (rather a lot)
#define INTERPOLATION_BUCKETS 107


#if 0 

class DInterpolation : public DObject
{
	DECLARE_ABSTRACT_CLASS(DInterpolation, DObject)
	HAS_OBJECT_POINTERS

	DInterpolation *Next;

protected:
	DInterpolation();

public:

	void Destroy();
	void CopyInterpToOld() = 0;
	void CopyBakToInterp() = 0;
	void DoAnInterpolation(fixed_t smoothratio) = 0;
	void Serialize(FArchive &arc);
};

class DSectorPlaneInterpolation : public DInterpolation
{
	DECLARE_CLASS(DSectorPlaneInterpolation, DInterpolation)

	sector_t *sector;
	fixed_t oldheight, oldtexz;
	fixed_t bakheight, baktexz;
	bool floor;
	TArray<DInterpolation *> attached;


public:

	DSectorPlaneInterpolation(sector_t *sector, int plane, bool attach);
	void Destroy();
	void CopyInterpToOld();
	void CopyBakToInterp();
	void DoAnInterpolation(fixed_t smoothratio);
	void Serialize(FArchive &arc);
	size_t PointerSubstitution (DObject *old, DObject *notOld);
	size_t PropagateMark();
};

class DSectorScrollInterpolation : public DInterpolation
{
	DECLARE_CLASS(DFloorScrollInterpolation, DInterpolation)

	sector_t *sector;
	fixed_t oldx, oldy;
	fixed_t bakx, baky;
	bool floor;

public:

	DSectorScrollInterpolation(sector_t *sector, int plane);
	void Destroy();
	void CopyInterpToOld();
	void CopyBakToInterp();
	void DoAnInterpolation(fixed_t smoothratio);
	void Serialize(FArchive &arc);
};

class DWallScrollInterpolation : public DInterpolation
{
	DECLARE_CLASS(DWallScrollInterpolation, DInterpolation)

	side_t *side;
	side_t::ETexpart part;
	fixed_t oldx, oldy;
	fixed_t bakx, baky;

public:

	DWallScrollInterpolation(side_t *side, side_t::ETexpart part);
	void Destroy();
	void CopyInterpToOld();
	void CopyBakToInterp();
	void DoAnInterpolation(fixed_t smoothratio);
	void Serialize(FArchive &arc);
};

class DPolyobjInterpolation : public DInterpolation
{
	DECLARE_CLASS(DPolyobjInterpolation, DInterpolation)

	FPolyobj *poly;
	vertex_t *oldverts, *bakverts;

public:

	DPolyobjInterpolation(FPolyobj *poly);
	void Destroy();
	void CopyInterpToOld();
	void CopyBakToInterp();
	void DoAnInterpolation(fixed_t smoothratio);
	void Serialize(FArchive &arc);
};

struct FInterpolator
{
	DInterpolation *curiposhash[INTERPOLATION_BUCKETS];

	int CountInterpolations ();
	int CountInterpolations (int *usedbuckets, int *minbucketfill, int *maxbucketfill);

private:

	size_t HashKey(FName type, void *interptr);
	DInterpolation *FindInterpolation(const PClass *, void *interptr, DInterpolation **&interp_p);

public:
	DInterpolation *FindInterpolation(const PClass *, void *interptr);
	void UpdateInterpolations();
	void AddInterpolation(DInterpolation *);
	void RemoveInterpolation(DInterpolation *);
	void DoInterpolations(fixed_t smoothratio);
	void RestoreInterpolations();
};


#endif

class FArchive;

enum EInterpType
{
	INTERP_SectorFloor,		// Pass a sector_t *
	INTERP_SectorCeiling,	// Pass a sector_t *
	INTERP_Vertex,			// Pass a vertex_t *
	INTERP_FloorPanning,	// Pass a sector_t *
	INTERP_CeilingPanning,	// Pass a sector_t *
	INTERP_WallPanning_Top,		// Pass a side_t *
	INTERP_WallPanning_Mid,
	INTERP_WallPanning_Bottom,
};

void updateinterpolations();
void setinterpolation(EInterpType, void *interptr, bool dolinks = true);
void stopinterpolation(EInterpType, void *interptr, bool dolinks = true);
void dointerpolations(fixed_t smoothratio);
void restoreinterpolations();
void clearinterpolations();
void SerializeInterpolations(FArchive &arc);

#endif

