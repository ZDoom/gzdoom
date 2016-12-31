
#pragma once

#include <stddef.h>
#include "r_defs.h"

class ASkyViewpoint;
class ADynamicLight;
struct FLightNode;
struct FDynamicColormap;
struct FSectorPortal;

namespace swrenderer
{
	struct visplane_light
	{
		ADynamicLight *lightsource;
		visplane_light *next;
	};

	struct visplane_t
	{
		visplane_t *next;		// Next visplane in hash chain -- killough

		FDynamicColormap *colormap;		// [RH] Support multiple colormaps
		FSectorPortal *portal;			// [RH] Support sky boxes
		visplane_light *lights;

		FTransform	xform;
		secplane_t	height;
		FTextureID	picnum;
		int			lightlevel;
		int			left, right;
		int			sky;

		// [RH] This set of variables copies information from the time when the
		// visplane is created. They are only used by stacks so that you can
		// have stacked sectors inside a skybox. If the visplane is not for a
		// stack, then they are unused.
		int			extralight;
		double		visibility;
		DVector3	viewpos;
		DAngle		viewangle;
		fixed_t		Alpha;
		bool		Additive;

		// kg3D - keep track of mirror and skybox owner
		int CurrentSkybox;
		int CurrentPortalUniq; // mirror counter, counts all of them
		int MirrorFlags; // this is not related to CurrentMirror

		unsigned short *bottom;			// [RH] bottom and top arrays are dynamically
		unsigned short pad;				//		allocated immediately after the
		unsigned short top[];			//		visplane.
	};

	#define MAXVISPLANES 128    /* must be a power of 2 */
	#define visplane_hash(picnum,lightlevel,height) ((unsigned)((picnum)*3+(lightlevel)+(FLOAT2FIXED((height).fD()))*7) & (MAXVISPLANES-1))

	extern visplane_t *visplanes[MAXVISPLANES + 1];
	extern visplane_t *freetail;
	extern visplane_t **freehead;

	void R_DeinitPlanes();
	visplane_t *new_visplane(unsigned hash);
	void R_PlaneInitData();
}
