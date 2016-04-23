#ifndef SOFT_FAKE3D_H
#define SOFT_FAKE3D_H

#include "p_3dfloors.h"

// special types

struct HeightLevel
{
	double height;
	struct HeightLevel *prev;
	struct HeightLevel *next;
};

struct HeightStack
{
	HeightLevel *height_top;
	HeightLevel *height_cur;
	int height_max;
};

struct ClipStack
{
	short floorclip[MAXWIDTH];
	short ceilingclip[MAXWIDTH];
	F3DFloor *ffloor;
	ClipStack *next;
};

// external varialbes

// fake3D flags:
enum
{
	// BSP stage:
	FAKE3D_FAKEFLOOR		= 1,	// fake floor, mark seg as FAKE
	FAKE3D_FAKECEILING		= 2,	// fake ceiling, mark seg as FAKE
	FAKE3D_FAKEBACK			= 4,	// R_AddLine with fake backsector, mark seg as FAKE
	FAKE3D_FAKEMASK			= 7,
	FAKE3D_CLIPBOTFRONT		= 8,	// use front sector clipping info (bottom)
	FAKE3D_CLIPTOPFRONT		= 16,	// use front sector clipping info (top)

	// sorting stage:
	FAKE3D_CLIPBOTTOM		= 1,	// clip bottom
	FAKE3D_CLIPTOP			= 2,	// clip top
	FAKE3D_REFRESHCLIP		= 4,	// refresh clip info
	FAKE3D_DOWN2UP			= 8,	// rendering from down to up (floors)
};

extern int fake3D;
extern F3DFloor *fakeFloor;
extern fixed_t fakeAlpha;
extern int fakeActive;
extern double sclipBottom;
extern double sclipTop;
extern HeightLevel *height_top;
extern HeightLevel *height_cur;
extern int CurrentMirror;
extern int CurrentSkybox;
EXTERN_CVAR(Int, r_3dfloors);

// functions
void R_3D_DeleteHeights();
void R_3D_AddHeight(secplane_t *add, sector_t *sec);
void R_3D_NewClip();
void R_3D_ResetClip();
void R_3D_EnterSkybox();
void R_3D_LeaveSkybox();

#endif
