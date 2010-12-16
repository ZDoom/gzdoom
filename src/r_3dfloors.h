#ifndef __SOFT_FAKE3D_H
#define __SOFT_FAKE3D_H

#include "p_3dfloors.h"

// special types

struct HeightLevel
{
	fixed_t height;
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
	ClipStack *next;
};

// external varialbes

/*
	fake3D flags:
		BSP stage:
		1 - fake floor, mark seg as FAKE
		2 - fake ceiling, mark seg as FAKE
		4 - R_AddLine with fake backsector, mark seg as FAKE
		sorting stage:
		1 - clip bottom
		2 - clip top
		4 - refresh clip info
		8 - rendering from down to up (floors)
*/
extern int fake3D;
extern F3DFloor *fakeFloor;
extern fixed_t fakeHeight;
extern fixed_t fakeAlpha;
extern int fakeActive;
extern fixed_t sclipBottom;
extern fixed_t sclipTop;
extern HeightLevel *height_top;
extern HeightLevel *height_cur;
extern int CurrentMirror;
extern int CurrentSkybox;

// functions
void R_3D_DeleteHeights();
void R_3D_AddHeight(secplane_t *add, sector_t *sec);
void R_3D_NewClip();
void R_3D_ResetClip();
void R_3D_EnterSkybox();
void R_3D_LeaveSkybox();

#endif
