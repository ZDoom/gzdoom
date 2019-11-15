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
		static void RenderDecals(RenderThread *thread, DrawSegment *draw_segment, seg_t *curline, const sector_t* lightsector, const short *walltop, const short *wallbottom, bool drawsegPass);

	private:
		static void Render(RenderThread *thread, DBaseDecal *first, DrawSegment *clipper, seg_t *curline, const sector_t* lightsector, const short *walltop, const short *wallbottom, bool drawsegPass);
	};
}
