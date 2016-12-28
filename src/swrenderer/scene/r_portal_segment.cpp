
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
#include "r_portal_segment.h"

namespace swrenderer
{
	PortalDrawseg *CurrentPortal = nullptr;
	int CurrentPortalUniq = 0;
	bool CurrentPortalInSkybox = false;
	
	TArray<PortalDrawseg> WallPortals(1000);	// note: this array needs to go away as reallocation can cause crashes.
}
