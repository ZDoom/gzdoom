
#ifndef __GLC_DATA_H
#define __GLC_DATA_H

#include "doomtype.h"
#include "vectors.h"
#include "r_utility.h"

#include "r_defs.h"
#include "a_sharedglobal.h"
#include "c_cvars.h"

EXTERN_CVAR(Int, gl_weaponlight);

inline	int getExtraLight()
{
	return r_viewpoint.extralight * gl_weaponlight;
}


struct GLSectorStackPortal;

struct FPortal
{
	DVector2 mDisplacement;
	int plane;
	GLSectorStackPortal *glportal;	// for quick access to the render data. This is only valid during BSP traversal!

	GLSectorStackPortal *GetRenderState();
};

struct FGLLinePortal
{
	// defines the complete span of this portal, if this is of type PORTT_LINKED.
	vertex_t	*v1 = nullptr, *v2 = nullptr;	// vertices, from v1 to v2
	TArray<FLinePortal *> lines;
	int validcount = 0;
};

extern TArray<FPortal *> glSectorPortals;
extern TArray<FGLLinePortal*> linePortalToGL;

extern TArray<uint8_t> currentmapsection;

void gl_InitPortals();
void gl_BuildPortalCoverage(FPortalCoverage *coverage, subsector_t *subsector, const DVector2 &displacement);

#endif
