
#pragma once

#include "hw_levelmesh.h"
#include "tarray.h"
#include "vectors.h"
#include "r_defs.h"
#include "bounds.h"
#include <dp_rect_pack.h>

typedef dp::rect_pack::RectPacker<int> RectPacker;

struct FLevelLocals;

struct DoomLevelMeshSurface : public LevelMeshSurface
{
	subsector_t* Subsector;
	side_t* Side;
	sector_t* ControlSector;
	uint32_t LightmapNum; // To do: same as atlasPageIndex. Delete one of them!
	float* TexCoords;
};

class DoomLevelMesh : public LevelMesh
{
public:
	DoomLevelMesh(FLevelLocals &doomMap);

	bool TraceSky(const FVector3& start, FVector3 direction, float dist)
	{
		FVector3 end = start + direction * dist;
		TraceHit hit = TriangleMeshShape::find_first_hit(Collision.get(), start, end);
		if (hit.fraction == 1.0f)
			return true;

		int surfaceIndex = MeshSurfaceIndexes[hit.triangle];
		return Surfaces[surfaceIndex].bSky;
	}

	void UpdateLightLists() override;

	LevelMeshSurface* GetSurface(int index) override { return &Surfaces[index]; }
	int GetSurfaceCount() override { return Surfaces.Size(); }

	TArray<DoomLevelMeshSurface> Surfaces;

	TArray<FVector2> LightmapUvs;

	static_assert(alignof(FVector2) == alignof(float[2]) && sizeof(FVector2) == sizeof(float) * 2);

	void DumpMesh(const FString& objFilename, const FString& mtlFilename) const;

	void SetupLightmapUvs();

private:
	void CreateSubsectorSurfaces(FLevelLocals &doomMap);
	void CreateCeilingSurface(FLevelLocals &doomMap, subsector_t *sub, sector_t *sector, int typeIndex, bool is3DFloor);
	void CreateFloorSurface(FLevelLocals &doomMap, subsector_t *sub, sector_t *sector, int typeIndex, bool is3DFloor);
	void CreateSideSurfaces(FLevelLocals &doomMap, side_t *side);

	void BindLightmapSurfacesToGeometry(FLevelLocals& doomMap);
	void SetSubsectorLightmap(DoomLevelMeshSurface* surface);
	void SetSideLightmap(DoomLevelMeshSurface* surface);

	void CreateLightList(DoomLevelMeshSurface* surface, FLightNode* lighthead, int portalgroup);

	static bool IsTopSideSky(sector_t* frontsector, sector_t* backsector, side_t* side);
	static bool IsTopSideVisible(side_t* side);
	static bool IsBottomSideVisible(side_t* side);
	static bool IsSkySector(sector_t* sector, int plane);
	static bool IsControlSector(sector_t* sector);

	static FVector4 ToPlane(const FVector3& pt1, const FVector3& pt2, const FVector3& pt3)
	{
		FVector3 n = ((pt2 - pt1) ^ (pt3 - pt2)).Unit();
		float d = pt1 | n;
		return FVector4(n.X, n.Y, n.Z, d);
	}

	static FVector2 ToFVector2(const DVector2& v) { return FVector2((float)v.X, (float)v.Y); }
	static FVector3 ToFVector3(const DVector3& v) { return FVector3((float)v.X, (float)v.Y, (float)v.Z); }
	static FVector4 ToFVector4(const DVector4& v) { return FVector4((float)v.X, (float)v.Y, (float)v.Z, (float)v.W); }

	static bool IsDegenerate(const FVector3 &v0, const FVector3 &v1, const FVector3 &v2);

	// WIP internal lightmapper

	enum PlaneAxis
	{
		AXIS_YZ = 0,
		AXIS_XZ,
		AXIS_XY
	};

	static PlaneAxis BestAxis(const FVector4& p);
	BBox GetBoundsFromSurface(const LevelMeshSurface& surface) const;

	inline int AllocUvs(int amount) { return LightmapUvs.Reserve(amount); }

	void BuildSurfaceParams(int lightMapTextureWidth, int lightMapTextureHeight, LevelMeshSurface& surface);
	void FinishSurface(int lightmapTextureWidth, int lightmapTextureHeight, RectPacker& packer, LevelMeshSurface& surface);
};
