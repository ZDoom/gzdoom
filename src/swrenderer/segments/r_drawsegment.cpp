
#include <stdlib.h>
#include "templates.h"
#include "doomdef.h"
#include "m_bbox.h"
#include "i_system.h"
#include "p_lnspec.h"
#include "p_setup.h"
#include "swrenderer/r_main.h"
#include "swrenderer/scene/r_plane.h"
#include "swrenderer/drawers/r_draw.h"
#include "swrenderer/scene/r_things.h"
#include "swrenderer/scene/r_3dfloors.h"
#include "a_sharedglobal.h"
#include "g_level.h"
#include "p_effect.h"
#include "doomstat.h"
#include "r_state.h"
#include "swrenderer/scene/r_bsp.h"
#include "swrenderer/scene/r_segs.h"
#include "v_palette.h"
#include "r_sky.h"
#include "po_man.h"
#include "r_data/colormaps.h"
#include "swrenderer/segments/r_drawsegment.h"

namespace swrenderer
{
	drawseg_t *firstdrawseg;
	drawseg_t *ds_p;
	drawseg_t *drawsegs;

	size_t FirstInterestingDrawseg;
	TArray<size_t> InterestingDrawsegs;

	namespace
	{
		size_t MaxDrawSegs;
	}

	void R_FreeDrawSegs()
	{
		if (drawsegs != nullptr)
		{
			M_Free(drawsegs);
			drawsegs = nullptr;
		}
	}

	void R_ClearDrawSegs()
	{
		if (drawsegs == nullptr)
		{
			MaxDrawSegs = 256; // [RH] Default. Increased as needed.
			firstdrawseg = drawsegs = (drawseg_t *)M_Malloc (MaxDrawSegs * sizeof(drawseg_t));
		}
		FirstInterestingDrawseg = 0;
		InterestingDrawsegs.Clear ();
		ds_p = drawsegs;
	}

	drawseg_t *R_AddDrawSegment()
	{
		if (ds_p == &drawsegs[MaxDrawSegs])
		{ // [RH] Grab some more drawsegs
			size_t newdrawsegs = MaxDrawSegs ? MaxDrawSegs * 2 : 32;
			ptrdiff_t firstofs = firstdrawseg - drawsegs;
			drawsegs = (drawseg_t *)M_Realloc(drawsegs, newdrawsegs * sizeof(drawseg_t));
			firstdrawseg = drawsegs + firstofs;
			ds_p = drawsegs + MaxDrawSegs;
			MaxDrawSegs = newdrawsegs;
			DPrintf(DMSG_NOTIFY, "MaxDrawSegs increased to %zu\n", MaxDrawSegs);
		}

		return ds_p++;
	}
}
