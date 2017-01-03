//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// The source is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
// for more details.
//

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
