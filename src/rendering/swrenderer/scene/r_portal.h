//-----------------------------------------------------------------------------
//
// Copyright 1993-1996 id Software
// Copyright 1999-2016 Randy Heit
// Copyright 2016 Magnus Norddahl
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/
//
//-----------------------------------------------------------------------------

#pragma once

#include "swrenderer/segments/r_portalsegment.h"
#include <set>

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

		// [Nash] this is set when first entering a mirror portal, and is unset when leaving the final mirror portal recursion
		// Used for the RF2_INVISIBLEINMIRRORS and RF2_ONLYVISIBLEINMIRRORS features
		bool IsInMirrorRecursively = false;

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
		DRotator stacked_angle;
		
		int numskyboxes = 0; // For ADD_STAT(skyboxes)

		void SetInSkyBox(FSectorPortal *portal) { SectorPortalsInSkyBox.insert(portal); }
		void ClearInSkyBox(FSectorPortal *portal) { SectorPortalsInSkyBox.erase(portal); }
		bool InSkyBox(FSectorPortal *portal) const { return SectorPortalsInSkyBox.find(portal) != SectorPortalsInSkyBox.end(); }

	private:
		void RenderLinePortal(PortalDrawseg* pds, int depth);
		void RenderLinePortalHighlight(PortalDrawseg* pds);
		
		TArray<DVector3> viewposStack;
		TArray<VisiblePlane *> visplaneStack;
		TArray<PortalDrawseg *> WallPortals;

		std::set<FSectorPortal *> SectorPortalsInSkyBox; // Instead of portal->mFlags & PORTSF_INSKYBOX
	};
}
