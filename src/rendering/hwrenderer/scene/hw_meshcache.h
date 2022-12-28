#pragma once

#include "hw_mesh.h"
#include "r_defs.h"

struct FRenderViewpoint;

class HWCachedSector
{
public:
	bool NeedsUpdate = true;

	sector_t* Sector = nullptr;
	secplane_t Floorplane;
	secplane_t Ceilingplane;

	std::unique_ptr<Mesh> Opaque;
	std::unique_ptr<Mesh> Translucent;
	std::unique_ptr<Mesh> TranslucentDepthBiased;

	void Update(FRenderViewpoint& vp);
};

class HWMeshCache
{
public:
	void Clear();
	void Update(FRenderViewpoint& vp);

	TArray<HWCachedSector> Sectors;

private:
	unsigned int nextRefresh = 0;
};

extern HWMeshCache meshcache;
