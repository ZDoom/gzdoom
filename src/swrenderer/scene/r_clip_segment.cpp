
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
#include "r_clip_segment.h"

namespace swrenderer
{
	cliprange_t *newend;
	cliprange_t solidsegs[MAXWIDTH/2+2];

	void R_ClearClipSegs(short left, short right)
	{
		solidsegs[0].first = -0x7fff;
		solidsegs[0].last = left;
		solidsegs[1].first = right;
		solidsegs[1].last = 0x7fff;
		newend = solidsegs+2;
	}
}
