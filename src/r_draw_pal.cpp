
#include "templates.h"
#include "doomtype.h"
#include "doomdef.h"
#include "r_defs.h"
#include "r_draw.h"
#include "r_main.h"
#include "r_things.h"
#include "v_video.h"
#include "r_draw_pal.h"

namespace swrenderer
{
	PalWall1Command::PalWall1Command()
	{
	}

	PalWall4Command::PalWall4Command()
	{
	}

	void DrawWall1PalCommand::Execute(DrawerThread *thread)
	{
	}

	void DrawWall4PalCommand::Execute(DrawerThread *thread)
	{
	}

	void DrawWallMasked1PalCommand::Execute(DrawerThread *thread)
	{
	}

	void DrawWallMasked4PalCommand::Execute(DrawerThread *thread)
	{
	}

	void DrawWallAdd1PalCommand::Execute(DrawerThread *thread)
	{
	}

	void DrawWallAdd4PalCommand::Execute(DrawerThread *thread)
	{
	}

	void DrawWallAddClamp1PalCommand::Execute(DrawerThread *thread)
	{
	}

	void DrawWallAddClamp4PalCommand::Execute(DrawerThread *thread)
	{
	}

	void DrawWallSubClamp1PalCommand::Execute(DrawerThread *thread)
	{
	}

	void DrawWallSubClamp4PalCommand::Execute(DrawerThread *thread)
	{
	}

	void DrawWallRevSubClamp1PalCommand::Execute(DrawerThread *thread)
	{
	}

	void DrawWallRevSubClamp4PalCommand::Execute(DrawerThread *thread)
	{
	}

	PalSkyCommand::PalSkyCommand(uint32_t solid_top, uint32_t solid_bottom)
	{
	}

	void DrawSingleSky1PalCommand::Execute(DrawerThread *thread)
	{
	}

	void DrawSingleSky4PalCommand::Execute(DrawerThread *thread)
	{
	}

	void DrawDoubleSky1PalCommand::Execute(DrawerThread *thread)
	{
	}

	void DrawDoubleSky4PalCommand::Execute(DrawerThread *thread)
	{
	}

	PalColumnCommand::PalColumnCommand()
	{
	}

	void DrawColumnPalCommand::Execute(DrawerThread *thread)
	{
	}

	void FillColumnPalCommand::Execute(DrawerThread *thread)
	{
	}

	void FillColumnAddPalCommand::Execute(DrawerThread *thread)
	{
	}

	void FillColumnAddClampPalCommand::Execute(DrawerThread *thread)
	{
	}

	void FillColumnSubClampPalCommand::Execute(DrawerThread *thread)
	{
	}

	void FillColumnRevSubClampPalCommand::Execute(DrawerThread *thread)
	{
	}

	void DrawColumnAddPalCommand::Execute(DrawerThread *thread)
	{
	}

	void DrawColumnTranslatedPalCommand::Execute(DrawerThread *thread)
	{
	}

	void DrawColumnTlatedAddPalCommand::Execute(DrawerThread *thread)
	{
	}

	void DrawColumnShadedPalCommand::Execute(DrawerThread *thread)
	{
	}

	void DrawColumnAddClampPalCommand::Execute(DrawerThread *thread)
	{
	}

	void DrawColumnAddClampTranslatedPalCommand::Execute(DrawerThread *thread)
	{
	}

	void DrawColumnSubClampPalCommand::Execute(DrawerThread *thread)
	{
	}

	void DrawColumnSubClampTranslatedPalCommand::Execute(DrawerThread *thread)
	{
	}

	void DrawColumnRevSubClampPalCommand::Execute(DrawerThread *thread)
	{
	}

	void DrawColumnRevSubClampTranslatedPalCommand::Execute(DrawerThread *thread)
	{
	}

	void DrawFuzzColumnPalCommand::Execute(DrawerThread *thread)
	{
	}

	void DrawSpanPalCommand::Execute(DrawerThread *thread)
	{
	}

	void DrawSpanMaskedPalCommand::Execute(DrawerThread *thread)
	{
	}

	void DrawSpanTranslucentPalCommand::Execute(DrawerThread *thread)
	{
	}

	void DrawSpanMaskedTranslucentPalCommand::Execute(DrawerThread *thread)
	{
	}

	void DrawSpanAddClampPalCommand::Execute(DrawerThread *thread)
	{
	}

	void DrawSpanMaskedAddClampPalCommand::Execute(DrawerThread *thread)
	{
	}

	void FillSpanPalCommand::Execute(DrawerThread *thread)
	{
	}

	DrawTiltedSpanPalCommand::DrawTiltedSpanPalCommand(int y, int x1, int x2, const FVector3 &plane_sz, const FVector3 &plane_su, const FVector3 &plane_sv, bool plane_shade, int planeshade, float planelightfloat, fixed_t pviewx, fixed_t pviewy)
	{
	}

	void DrawTiltedSpanPalCommand::Execute(DrawerThread *thread)
	{
	}

	DrawColoredSpanPalCommand::DrawColoredSpanPalCommand(int y, int x1, int x2)
	{
	}

	void DrawColoredSpanPalCommand::Execute(DrawerThread *thread)
	{
	}

	DrawSlabPalCommand::DrawSlabPalCommand(int dx, fixed_t v, int dy, fixed_t vi, const uint8_t *vptr, uint8_t *p, const uint8_t *colormap)
	{
	}

	void DrawSlabPalCommand::Execute(DrawerThread *thread)
	{
	}

	DrawFogBoundaryLinePalCommand::DrawFogBoundaryLinePalCommand(int y, int y2, int x1)
	{
	}

	void DrawFogBoundaryLinePalCommand::Execute(DrawerThread *thread)
	{
	}
}
