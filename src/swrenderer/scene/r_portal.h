
#pragma once

#include "swrenderer/segments/r_portalsegment.h"

namespace swrenderer
{
	extern PortalDrawseg* CurrentPortal;
	extern int CurrentPortalUniq;
	extern bool CurrentPortalInSkybox;

	extern int stacked_extralight;
	extern double stacked_visibility;
	extern DVector3 stacked_viewpos;
	extern DAngle stacked_angle;

	void R_DrawPortals();
	void R_DrawWallPortals();
	void R_EnterPortal(PortalDrawseg* pds, int depth);
	void R_HighlightPortal(PortalDrawseg* pds);
}
