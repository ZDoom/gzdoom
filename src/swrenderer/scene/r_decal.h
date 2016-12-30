
#pragma once

struct side_t;
class DBaseDecal;

namespace swrenderer
{
	struct drawseg_t;

	void R_RenderDecals(side_t *wall, drawseg_t *draw_segment);
	void R_RenderDecal(side_t *wall, DBaseDecal *first, drawseg_t *clipper, int pass);
}
