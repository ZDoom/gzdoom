#pragma once

#include "hw_mesh.h"
#include "r_defs.h"

struct FRenderViewpoint;

class HWCachedSector
{
public:
	sector_t* Sector = nullptr;

	std::unique_ptr<Mesh> Opaque;
	std::unique_ptr<Mesh> Translucent;
	std::unique_ptr<Mesh> TranslucentDepthBiased;

	void Update(FRenderViewpoint& vp);
};

class HWMeshCache
{
public:
	void Update(FRenderViewpoint& vp);

	TArray<HWCachedSector> Sectors;

private:
};

extern HWMeshCache meshcache;
