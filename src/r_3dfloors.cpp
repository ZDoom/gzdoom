/*
**	r_3dfloors.cpp
**	software 3D floors addon
**
**	by kgsws
*/

#include "templates.h"
#include "doomdef.h"
#include "p_local.h"
#include "c_dispatch.h"
#include "r_local.h"
#include "r_bsp.h"
#include "r_plane.h"

#include "r_3dfloors.h"

static void AddVisXPlane(vissubsector_t *vsub, sector_t *sec, F3DFloor *ffloor, F3DFloor::planeref *planeref, int orientation);

// external variables
int fake3D;
F3DFloor *fakeFloor;
fixed_t fakeHeight;
fixed_t fakeAlpha;
int fakeActive = 0;
fixed_t sclipBottom;
fixed_t sclipTop;
HeightLevel *height_top = NULL;
HeightLevel *height_cur = NULL;
int CurrentMirror = 0;
int CurrentSkybox = 0;

// private variables
int height_max = -1;
TArray<HeightStack> toplist;
ClipStack *clip_top = NULL;
ClipStack *clip_cur = NULL;

static FMemArena VisXPlaneArena;

void R_3D_DeleteHeights()
{
	height_cur = height_top;
	while(height_cur) {
		height_top = height_cur;
		height_cur = height_cur->next;
		M_Free(height_top);
	}
	height_max = -1;
	height_top = height_cur = NULL;
}

void R_3D_AddHeight(secplane_t *add, sector_t *sec)
{
	HeightLevel *near;
	HeightLevel *curr;
	fixed_t height;

	height = add->ZatPoint(viewx, viewy);
	if(height >= sec->CenterCeiling()) return;
	if(height <= sec->CenterFloor()) return;

	fakeActive = 1;

	if(height_max >= 0) {
		near = height_top;
		while(near && near->height < height) near = near->next;
		if(near) {
			if(near->height == height) return;
			curr = (HeightLevel*)M_Malloc(sizeof(HeightLevel));
			curr->height = height;
			curr->prev = near->prev;
			curr->next = near;
			if(near->prev) near->prev->next = curr;
			else height_top = curr;
			near->prev = curr;
		} else {
			curr = (HeightLevel*)M_Malloc(sizeof(HeightLevel));
			curr->height = height;
			curr->prev = height_cur;
			curr->next = NULL;
			height_cur->next = curr;
			height_cur = curr;
		}
	} else {
		height_top = height_cur = (HeightLevel*)M_Malloc(sizeof(HeightLevel));
		height_top->height = height;
		height_top->prev = NULL;
		height_top->next = NULL;
	}
	height_max++;
}

void R_3D_NewClip()
{
	ClipStack *curr;
//	extern short floorclip[MAXWIDTH];
//	extern short ceilingclip[MAXWIDTH];

	curr = (ClipStack*)M_Malloc(sizeof(ClipStack));
	curr->next = 0;
	memcpy(curr->floorclip, floorclip, sizeof(short) * MAXWIDTH);
	memcpy(curr->ceilingclip, ceilingclip, sizeof(short) * MAXWIDTH);
	curr->ffloor = fakeFloor;
	assert(fakeFloor->floorclip == NULL);
	assert(fakeFloor->ceilingclip == NULL);
	fakeFloor->floorclip = curr->floorclip;
	fakeFloor->ceilingclip = curr->ceilingclip;
	if(clip_top) {
		clip_cur->next = curr;
		clip_cur = curr;
	} else {
		clip_top = clip_cur = curr;
	}
}

void R_3D_ResetClip()
{
	clip_cur = clip_top;
	while(clip_cur) 
	{
		assert(clip_cur->ffloor->floorclip != NULL);
		assert(clip_cur->ffloor->ceilingclip != NULL);
		clip_cur->ffloor->ceilingclip = clip_cur->ffloor->floorclip = NULL;
		clip_top = clip_cur;
		clip_cur = clip_cur->next;
		M_Free(clip_top);
	}
	clip_cur = clip_top = NULL;
}

void R_3D_EnterSkybox()
{
	HeightStack current;

	current.height_top = height_top;
	current.height_cur = height_cur;
	current.height_max = height_max;

	toplist.Push(current);

	height_top = NULL;
	height_cur = NULL;
	height_max = -1;

	CurrentSkybox++;
}

void R_3D_LeaveSkybox()
{
	HeightStack current;

	current.height_top = NULL;
	current.height_cur = NULL;
	current.height_max = -1;

	toplist.Pop(current);

	height_top = current.height_top;
	height_cur = current.height_cur;
	height_max = current.height_max;

	CurrentSkybox--;
}

//=============================================================================
//
// R_3D_EnterSubsector
//
// Creates a new vissubsector_t and decides which extra floor planes should
// be drawn.
//
//=============================================================================

vissubsector_t *R_3D_EnterSubsector(subsector_t *sub)
{
	vissubsector_t *vsub = &VisSubsectors[VisSubsectors.Reserve(1)];
	fixed_t z;

	vsub->Planes = NULL;
	vsub->MinX = SHRT_MAX;
	vsub->MaxX = SHRT_MIN;
	vsub->uclip = (short *)VisXPlaneArena.Alloc(sizeof(short) * viewwidth * 2);
	vsub->dclip = vsub->uclip + viewwidth;
	memcpy(vsub->uclip, ceilingclip, sizeof(*ceilingclip)*viewwidth);
	memcpy(vsub->dclip, floorclip, sizeof(*floorclip)*viewwidth);

	if (sub->sector->e != NULL)
	{
		for (unsigned i = sub->sector->e->XFloor.ffloors.Size(); i-- > 0; )
		{
			F3DFloor *ffloor = sub->sector->e->XFloor.ffloors[i];

			if (ffloor->model == NULL) continue;
			if ((ffloor->flags & (FF_EXISTS | FF_RENDERPLANES)) != (FF_EXISTS | FF_RENDERPLANES)) continue;
			if (ffloor->alpha <= 0 && (ffloor->flags & (FF_TRANSLUCENT | FF_ADDITIVETRANS))) continue;
			// Check top of floor
			if ((ffloor->top.plane->a | ffloor->top.plane->b) == 0)
			{
				z = ffloor->top.plane->ZatPoint(viewx, viewy);
				if (viewz > z)
				{ // Above the top
					if (!(ffloor->flags & FF_INVERTPLANES) || (ffloor->flags & FF_BOTHPLANES))
					{
						AddVisXPlane(vsub, sub->sector, ffloor, &ffloor->top, sector_t::floor);
					}
				}
				else if (viewz < z)
				{ // Below the top
					if (ffloor->flags & (FF_INVERTPLANES | FF_BOTHPLANES))
					{
						AddVisXPlane(vsub, sub->sector, ffloor, &ffloor->top, sector_t::ceiling);
					}
				}
			}

			// Check bottom of floor
			if ((ffloor->bottom.plane->a | ffloor->top.plane->b) == 0)
			{
				z = ffloor->bottom.plane->ZatPoint(viewx, viewy);
				if (viewz > z)
				{ // Above the bottom
					if (ffloor->flags & (FF_INVERTPLANES | FF_BOTHPLANES))
					{
						AddVisXPlane(vsub, sub->sector, ffloor, &ffloor->bottom, sector_t::floor);
					}
				}
				else if (viewz < z)
				{ // Below the bottom
					if (!(ffloor->flags & FF_INVERTPLANES) || (ffloor->flags & FF_BOTHPLANES))
					{
						AddVisXPlane(vsub, sub->sector, ffloor, &ffloor->bottom, sector_t::ceiling);
					}
				}
			}
		}
	}
	return vsub;
}

//=============================================================================
//
// AddVisXPlane
//
// Attaches a new visible extra floor plane to a vissubsector.
//
//=============================================================================

static void AddVisXPlane(vissubsector_t *vsub, sector_t *sec, F3DFloor *ffloor, F3DFloor::planeref *planeref, int orientation)
{
	visxplane_t *xplane = R_NewVisXPlane();
	lightlist_t *light;

	xplane->PlaneRef = planeref;
	xplane->Orientation = orientation;
	xplane->FakeFloor = ffloor;

	light = P_GetPlaneLight(sec, planeref->plane, orientation == sector_t::ceiling);
	xplane->Colormap = light->extra_colormap;
	xplane->LightLevel = r_actualextralight + ((fixedlightlev >= 0) ? fixedlightlev : *light->p_lightlevel);

	xplane->Next = vsub->Planes;
	vsub->Planes = xplane;

	// If this is the first subsector drawn, that means we're inside of it, so
	// we need to initialize the near edge, since we can't rely on back-facing
	// walls to do it for us.
	if (VisSubsectors.Size() == 1)
	{
		clearbufshort(xplane->NearClip, viewwidth, orientation == sector_t::ceiling ? 0 : viewheight);
	}
}

//=============================================================================
//
// R_NewVisXPlane
//
// Creates a new visxplane_t with enough clipping room for the current
// viewwidth.
//
//=============================================================================

visxplane_t *R_NewVisXPlane()
{
	visxplane_t *xplane = (visxplane_t *)VisXPlaneArena.Alloc(sizeof(visxplane_t) +
		sizeof(short)*viewwidth * 2);

	xplane->Next = NULL;
	xplane->NearClip = (unsigned short *)((BYTE *)xplane + sizeof(visxplane_t));
	xplane->FarClip = xplane->NearClip + viewwidth;
	xplane->PlaneRef = NULL;
	xplane->LightLevel = 0;
	xplane->Orientation = -1;

	return xplane;
}

//=============================================================================
//
// R_ClearVisSubsectors
//
// Frees all vissubsectors and related data visxplanes.
//
//=============================================================================

void R_ClearVisSubsectors()
{
	VisSubsectors.Clear();
	VisXPlaneArena.FreeAll();
}

//=============================================================================
//
// R_3D_MarkPlanes
//
// Marks either the near or far edges of any extra planes in the subsector.
// Uses global variables set up by R_AddLine().
//
//=============================================================================

void R_3D_MarkPlanes(vissubsector_t *vsub, EMarkPlaneEdge edge)
{
	if (vsub->MinX > WallSX1)
	{
		vsub->MinX = WallSX1;
	}
	if (vsub->MaxX < WallSX2)
	{
		vsub->MaxX = WallSX2;
	}
	for (visxplane_t *xplane = vsub->Planes; xplane != NULL; xplane = xplane->Next)
	{
		WallMost((short *)(edge == MARK_NEAR ? xplane->NearClip : xplane->FarClip), *xplane->PlaneRef->plane);
	}
}
