#pragma once

#include "r_defs.h"
#include "m_fixed.h"
#include "gl_clipper.h"

class GLSceneDrawer
{
	fixed_t viewx, viewy;	// since the nodes are still fixed point, keeping the view position  also fixed point for node traversal is faster.
	
	subsector_t *currentsubsector;	// used by the line processing code.
	sector_t *currentsector;
	
	void UnclipSubsector(subsector_t *sub);
	void AddLine (seg_t *seg, bool portalclip);
	void PolySubsector(subsector_t * sub);
	void RenderPolyBSPNode (void *node);
	void AddPolyobjs(subsector_t *sub);
	void AddLines(subsector_t * sub, sector_t * sector);
	void AddSpecialPortalLines(subsector_t * sub, sector_t * sector, line_t *line);
	void RenderThings(subsector_t * sub, sector_t * sector);
	void DoSubsector(subsector_t * sub);
	void RenderBSPNode(void *node);

public:
	Clipper clipper;
	
	void CreateScene();

	void InitClipper(angle_t a1, angle_t a2)
	{
		clipper.SafeAddClipRangeRealAngles(a1, a2);
	}

	void SetView()
	{
		viewx = FLOAT2FIXED(ViewPos.X);
		viewy = FLOAT2FIXED(ViewPos.Y);
	}
};
