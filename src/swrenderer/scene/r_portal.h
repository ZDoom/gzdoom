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
	class RenderThread;
	struct VisiblePlane;

	class RenderPortal
	{
	public:
		RenderPortal(RenderThread *thread);
		
		void SetMainPortal();
		void CopyStackedViewParameters();
	
		void RenderPlanePortals();
		void RenderLinePortals();

		void AddLinePortal(line_t *linedef, int x1, int x2, const short *topclip, const short *bottomclip);

		RenderThread *Thread = nullptr;
	
		int WindowLeft = 0;
		int WindowRight = 0;
		uint16_t MirrorFlags = 0;

		PortalDrawseg* CurrentPortal = nullptr;
		int CurrentPortalUniq = 0;
		bool CurrentPortalInSkybox = false;

		// These are copies of the main parameters used when drawing stacked sectors.
		// When you change the main parameters, you should copy them here too *unless*
		// you are changing them to draw a stacked sector. Otherwise, stacked sectors
		// won't draw in skyboxes properly.
		int stacked_extralight = 0;
		double stacked_visibility = 0.0;
		DVector3 stacked_viewpos;
		DAngle stacked_angle;
		
		int numskyboxes = 0; // For ADD_STAT(skyboxes)

	private:
		void RenderLinePortal(PortalDrawseg* pds, int depth);
		void RenderLinePortalHighlight(PortalDrawseg* pds);
		
		TArray<DVector3> viewposStack;
		TArray<VisiblePlane *> visplaneStack;
		TArray<PortalDrawseg *> WallPortals;
	};
}
