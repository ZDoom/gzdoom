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
		static void RenderDecals(RenderThread *thread, side_t *wall, DrawSegment *draw_segment, int lightlevel, float lightleft, float lightstep, seg_t *curline, const FWallCoords &wallC, bool foggy, FDynamicColormap *basecolormap, const short *walltop, const short *wallbottom, bool drawsegPass);

	private:
		static void Render(RenderThread *thread, side_t *wall, DBaseDecal *first, DrawSegment *clipper, int lightlevel, float lightleft, float lightstep, seg_t *curline, const FWallCoords &wallC, bool foggy, FDynamicColormap *basecolormap, const short *walltop, const short *wallbottom, bool drawsegPass);
		static void DrawColumn(RenderThread *thread, SpriteDrawerArgs &drawerargs, int x, FSoftwareTexture *WallSpriteTile, const ProjectedWallTexcoords &walltexcoords, double texturemid, float maskedScaleY, bool sprflipvert, const short *mfloorclip, const short *mceilingclip, FRenderStyle style);
	};
}
