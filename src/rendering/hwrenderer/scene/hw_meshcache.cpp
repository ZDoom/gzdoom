
#include "hw_meshcache.h"
#include "hw_drawinfo.h"
#include "hw_drawstructs.h"
#include "hw_mesh.h"
#include "g_levellocals.h"

EXTERN_CVAR(Bool, gl_texture)
EXTERN_CVAR(Float, gl_mask_threshold)

HWMeshCache meshcache;

void HWMeshCache::Update(FRenderViewpoint& vp)
{
	auto level = vp.ViewLevel;
	unsigned int count = level->sectors.Size();
	Sectors.Resize(count);
	for (unsigned int i = 0; i < count; i++)
	{
		auto sector = &level->sectors[i];
		if (Sectors[i].Sector != sector) // Note: this is just a stupid way to detect we changed map. What is the proper way?
		{
			Sectors[i].Sector = sector;
			Sectors[i].Update(vp);
		}
	}
}

void HWCachedSector::Update(FRenderViewpoint& vp)
{
	HWDrawInfo* di = HWDrawInfo::StartDrawInfo(vp.ViewLevel, nullptr, vp, nullptr);
	di->MeshBuilding = true;

	// Add all the walls to the draw lists

	for (line_t* line : Sector->Lines)
	{
		side_t* side = (line->sidedef[0]->sector == Sector) ? line->sidedef[0] : line->sidedef[1];

		HWWall wall;
		wall.sub = Sector->subsectors[0];
		wall.Process(di, side->segs[0], Sector, (line->sidedef[0]->sector == Sector) ? line->backsector : line->frontsector);
	}

	// Convert draw lists to meshes

	MeshBuilder state;

	state.SetDepthMask(true);
	state.EnableFog(true);
	state.SetRenderStyle(STYLE_Source);

	di->drawlists[GLDL_PLAINWALLS].SortWalls();
	di->drawlists[GLDL_PLAINFLATS].SortFlats();
	di->drawlists[GLDL_MASKEDWALLS].SortWalls();
	di->drawlists[GLDL_MASKEDFLATS].SortFlats();
	di->drawlists[GLDL_MASKEDWALLSOFS].SortWalls();

	// Part 1: solid geometry. This is set up so that there are no transparent parts
	state.SetDepthFunc(DF_Less);
	state.AlphaFunc(Alpha_GEqual, 0.f);
	state.ClearDepthBias();
	state.EnableTexture(gl_texture);
	state.EnableBrightmap(true);
	di->drawlists[GLDL_PLAINWALLS].DrawWalls(di, state, false);
	di->drawlists[GLDL_PLAINFLATS].DrawFlats(di, state, false);
	Opaque = state.Create();

	// Part 2: masked geometry. This is set up so that only pixels with alpha>gl_mask_threshold will show
	state.AlphaFunc(Alpha_GEqual, gl_mask_threshold);
	di->drawlists[GLDL_MASKEDWALLS].DrawWalls(di, state, false);
	di->drawlists[GLDL_MASKEDFLATS].DrawFlats(di, state, false);
	Translucent = state.Create();

	// Part 3: masked geometry with polygon offset. This list is empty most of the time so only waste time on it when in use.
	if (di->drawlists[GLDL_MASKEDWALLSOFS].Size() > 0)
	{
		state.SetDepthBias(-1, -128);
		di->drawlists[GLDL_MASKEDWALLSOFS].DrawWalls(di, state, false);
		state.ClearDepthBias();
		TranslucentDepthBiased = state.Create();
	}
	else
	{
		TranslucentDepthBiased.reset();
	}

	di->MeshBuilding = false;
	di->EndDrawInfo();
}
