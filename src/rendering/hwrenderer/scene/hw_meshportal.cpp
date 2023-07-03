
#include "hw_meshportal.h"
#include "p_local.h"
#include "p_lnspec.h"
#include "a_dynlight.h"
#include "a_sharedglobal.h"
#include "r_defs.h"
#include "r_sky.h"
#include "r_utility.h"
#include "p_maputl.h"
#include "doomdata.h"
#include "g_levellocals.h"
#include "actorinlines.h"
#include "texturemanager.h"
#include "hw_dynlightdata.h"
#include "hw_material.h"
#include "hw_cvars.h"
#include "hw_clock.h"
#include "hw_lighting.h"
#include "hw_drawcontext.h"
#include "hw_drawinfo.h"
#include "hw_renderstate.h"

void HWPortalWall::Process(HWDrawInfo* di, FRenderState& state, seg_t* seg, sector_t* frontsector, sector_t* backsector)
{
	// The portal code wants a HWWall struct.

	if (IsPortal(di, state, seg, frontsector, backsector))
	{
		HWWall wall;
		wall.sub = seg->Subsector;
		wall.Process(di, state, seg, frontsector, backsector);
	}
}

bool HWPortalWall::IsPortal(HWDrawInfo* di, FRenderState& state, seg_t* seg, sector_t* frontsector, sector_t* backsector)
{
	// Note: This is incomplete. It assumes a completely valid map. Good luck adding the full range of checks!

	if (seg->linedef->special == Line_Mirror && gl_mirrors)
		return true;

	if (seg->linedef->special == Line_Horizon)
		return true;

	if (seg->linedef->isVisualPortal() && seg->sidedef == seg->linedef->sidedef[0])
		return true;

	if (seg->linedef->GetTransferredPortal())
		return true;

	for (int plane : { sector_t::floor, sector_t::ceiling })
	{
		FSectorPortal* sportal = frontsector->ValidatePortal(plane);
		if (frontsector->ValidatePortal(plane) && !(sportal->mFlags & PORTSF_INSKYBOX)) // no recursions
			return true;
		if (frontsector->GetTexture(plane) == skyflatnum)
			return true;
		if (frontsector->GetReflect(plane) > 0)
			return true;
	}

	return false;
}
