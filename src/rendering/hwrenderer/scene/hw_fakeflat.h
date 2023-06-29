#pragma once

class HWDrawContext;

enum area_t : int
{
	area_normal,
	area_below,
	area_above,
	area_default
};

// Global functions.
bool hw_CheckClip(side_t * sidedef, sector_t * frontsector, sector_t * backsector);
void hw_ClearFakeFlat(HWDrawContext* drawctx);
sector_t * hw_FakeFlat(HWDrawContext* drawctx, sector_t * sec, area_t in_area, bool back, sector_t *localcopy = nullptr);
area_t hw_CheckViewArea(vertex_t *v1, vertex_t *v2, sector_t *frontsector, sector_t *backsector);

