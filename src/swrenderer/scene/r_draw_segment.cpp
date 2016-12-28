
#include <stdlib.h>
#include "templates.h"
#include "doomdef.h"
#include "m_bbox.h"
#include "i_system.h"
#include "p_lnspec.h"
#include "p_setup.h"
#include "swrenderer/r_main.h"
#include "r_plane.h"
#include "swrenderer/drawers/r_draw.h"
#include "r_things.h"
#include "r_3dfloors.h"
#include "a_sharedglobal.h"
#include "g_level.h"
#include "p_effect.h"
#include "doomstat.h"
#include "r_state.h"
#include "r_bsp.h"
#include "r_segs.h"
#include "v_palette.h"
#include "r_sky.h"
#include "po_man.h"
#include "r_data/colormaps.h"
#include "r_draw_segment.h"

namespace swrenderer
{
	size_t MaxDrawSegs;
	drawseg_t *drawsegs;
	drawseg_t *firstdrawseg;
	drawseg_t *ds_p;

	size_t FirstInterestingDrawseg;
	TArray<size_t> InterestingDrawsegs;

	void R_ClearDrawSegs()
	{
		if (drawsegs == NULL)
		{
			MaxDrawSegs = 256; // [RH] Default. Increased as needed.
			firstdrawseg = drawsegs = (drawseg_t *)M_Malloc (MaxDrawSegs * sizeof(drawseg_t));
		}
		FirstInterestingDrawseg = 0;
		InterestingDrawsegs.Clear ();
		ds_p = drawsegs;
	}

}
