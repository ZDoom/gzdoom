#pragma once

struct side_t;
class DBaseDecal;

namespace swrenderer
{
	struct DrawSegment;
	class ProjectedWallTexcoords;
	class SpriteDrawerArgs;

	class RenderDecal
	{
	public:
		static void RenderDecals(RenderThread *thread, side_t *wall, DrawSegment *draw_segment, seg_t *curline, const ProjectedWallLight &light, const short *walltop, const short *wallbottom, bool drawsegPass);

	private:
		static void Render(RenderThread *thread, side_t *wall, DBaseDecal *first, DrawSegment *clipper, seg_t *curline, const ProjectedWallLight &light, const short *walltop, const short *wallbottom, bool drawsegPass);
	};
}
