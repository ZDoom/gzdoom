#pragma once

#include "hw_mesh.h"
#include "r_defs.h"

struct FRenderViewpoint;
class HWDrawContext;

#if 0
class HWCachedSector
{
public:
	bool NeedsUpdate = true;

	sector_t* Sector = nullptr;
	secplane_t Floorplane;
	secplane_t Ceilingplane;
};
#endif

class HWMeshCache
{
public:
	void Clear();
	void Update(HWDrawContext* drawctx, FRenderViewpoint& vp);

#if 0
	TArray<HWCachedSector> Sectors;
#endif

	std::unique_ptr<Mesh> Opaque;
	std::unique_ptr<Mesh> Translucent;
	std::unique_ptr<Mesh> TranslucentDepthBiased;
};

extern HWMeshCache meshcache;
