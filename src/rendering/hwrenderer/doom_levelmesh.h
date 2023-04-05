
#pragma once

#include "hw_levelmesh.h"
#include "tarray.h"
#include "vectors.h"
#include "r_defs.h"

struct FLevelLocals;

struct Surface
{
	SurfaceType type;
	int typeIndex;
	int numVerts;
	unsigned int startVertIndex;
	secplane_t plane;
	sector_t *controlSector;
	bool bSky;
};

class DoomLevelMesh : public hwrenderer::LevelMesh
{
public:
	DoomLevelMesh(FLevelLocals &doomMap);

	bool TraceSky(const FVector3& start, FVector3 direction, float dist)
	{
		FVector3 end = start + direction * dist;
		TraceHit hit = TriangleMeshShape::find_first_hit(Collision.get(), start, end);
		if (hit.fraction == 1.0f)
			return true;

		int surfaceIndex = MeshSurfaces[hit.triangle];
		const Surface& surface = Surfaces[surfaceIndex];
		return surface.bSky;
	}

	TArray<Surface> Surfaces;

private:
	void CreateSubsectorSurfaces(FLevelLocals &doomMap);
	void CreateCeilingSurface(FLevelLocals &doomMap, subsector_t *sub, sector_t *sector, int typeIndex, bool is3DFloor);
	void CreateFloorSurface(FLevelLocals &doomMap, subsector_t *sub, sector_t *sector, int typeIndex, bool is3DFloor);
	void CreateSideSurfaces(FLevelLocals &doomMap, side_t *side);

	static bool IsTopSideSky(sector_t* frontsector, sector_t* backsector, side_t* side);
	static bool IsTopSideVisible(side_t* side);
	static bool IsBottomSideVisible(side_t* side);
	static bool IsSkySector(sector_t* sector);
	static bool IsControlSector(sector_t* sector);

	static secplane_t ToPlane(const FVector3& pt1, const FVector3& pt2, const FVector3& pt3)
	{
		FVector3 n = ((pt2 - pt1) ^ (pt3 - pt2)).Unit();
		float d = pt1 | n;
		secplane_t p;
		p.set(n.X, n.Y, n.Z, d);
		return p;
	}

	static FVector2 ToFVector2(const DVector2& v) { return FVector2((float)v.X, (float)v.Y); }
	static FVector3 ToFVector3(const DVector3& v) { return FVector3((float)v.X, (float)v.Y, (float)v.Z); }
	static FVector4 ToFVector4(const DVector4& v) { return FVector4((float)v.X, (float)v.Y, (float)v.Z, (float)v.W); }

	static bool IsDegenerate(const FVector3 &v0, const FVector3 &v1, const FVector3 &v2);
};
