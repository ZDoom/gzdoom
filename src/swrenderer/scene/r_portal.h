
#pragma once

#include "swrenderer/segments/r_portalsegment.h"

namespace swrenderer
{
	extern PortalDrawseg* CurrentPortal;
	extern int CurrentPortalUniq;
	extern bool CurrentPortalInSkybox;

	void R_DrawPortals();
	void R_DrawWallPortals();
	void R_EnterPortal(PortalDrawseg* pds, int depth);
	void R_HighlightPortal(PortalDrawseg* pds);
}
