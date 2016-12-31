
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
#include "swrenderer/segments/r_portalsegment.h"

namespace swrenderer
{
	TArray<PortalDrawseg> WallPortals(1000);	// note: this array needs to go away as reallocation can cause crashes.
}
