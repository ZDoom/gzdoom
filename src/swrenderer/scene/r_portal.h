
#pragma once

#include "r_portal_segment.h"

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
