#pragma once

enum area_t : int
{
	area_normal,
	area_below,
	area_above,
	area_default
};


// Global functions. Make them members of GLRenderer later?
bool hw_CheckClip(side_t * sidedef, sector_t * frontsector, sector_t * backsector);
sector_t * hw_FakeFlat(sector_t * sec, sector_t * dest, area_t in_area, bool back);
area_t hw_CheckViewArea(vertex_t *v1, vertex_t *v2, sector_t *frontsector, sector_t *backsector);

