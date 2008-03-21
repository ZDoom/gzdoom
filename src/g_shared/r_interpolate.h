#ifndef R_INTERPOLATE_H
#define R_INTERPOLATE_H

#include "doomtype.h"

// BUILD stuff for interpolating between frames, but modified (rather a lot)
#define INTERPOLATION_BUCKETS 107


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

struct FActiveInterpolation
{
	FActiveInterpolation *Next;
	void *Address;
	EInterpType Type;

	friend FArchive &operator << (FArchive &arc, FActiveInterpolation *&interp);

	static int CountInterpolations ();
	static int CountInterpolations (int *usedbuckets, int *minbucketfill, int *maxbucketfill);

private:
	fixed_t oldipos[2], bakipos[2];

	void CopyInterpToOld();
	void CopyBakToInterp();
	void DoAnInterpolation(fixed_t smoothratio);

	static size_t HashKey(EInterpType type, void *interptr);
	static FActiveInterpolation *FindInterpolation(EInterpType, void *interptr, FActiveInterpolation **&interp_p);

	friend void updateinterpolations();
	friend void setinterpolation(EInterpType, void *interptr, bool dolinks);
	friend void stopinterpolation(EInterpType, void *interptr, bool dolinks);
	friend void dointerpolations(fixed_t smoothratio);
	friend void restoreinterpolations();
	friend void clearinterpolations();
	friend void SerializeInterpolations(FArchive &arc);

	static FActiveInterpolation *curiposhash[INTERPOLATION_BUCKETS];
};

void updateinterpolations();
void setinterpolation(EInterpType, void *interptr, bool dolinks = true);
void stopinterpolation(EInterpType, void *interptr, bool dolinks = true);
void dointerpolations(fixed_t smoothratio);
void restoreinterpolations();
void clearinterpolations();
void SerializeInterpolations(FArchive &arc);

#endif