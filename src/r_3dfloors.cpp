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
#include "r_segs.h"

#include "r_3dfloors.h"

static visxplane_t *AddVisXPlane(vissubsector_t *vsub, sector_t *sec, F3DFloor *ffloor, F3DFloor::planeref *planeref, int orientation, bool visible);
static void R_3D_AddVisWalls(F3DFloor *ffloor, visxplane_t *top, fixed_t topz, visxplane_t *bot, fixed_t botz, visseg_t *segs, EMarkPlaneEdge edge);

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
// R_NewVisSubsector
//
//=============================================================================

vissubsector_t *R_NewVisSubsector(subsector_t *sub)
{
	vissubsector_t *vsub = &VisSubsectors[VisSubsectors.Reserve(1)];

	vsub->Planes = NULL;
	vsub->MinX = SHRT_MAX;
	vsub->MaxX = SHRT_MIN;
	vsub->uclip = -1;
	vsub->dclip = -1;
	vsub->FarSegs = NULL;
	vsub->NearSegs = NULL;

	return vsub;
}

//=============================================================================
//
// R_3D_DontNeedVisSubsector
//
// Frees a vissubsector that was just allocated.
//
//=============================================================================

void R_3D_DontNeedVisSubsector(vissubsector_t *vsub)
{
	assert(VisSubsectors.Size() > 0 && vsub == &VisSubsectors.Last());

	VisSubsectors.Pop();
}

//=============================================================================
//
// R_3D_SetSubsectorUDClip
//
// Records the top and bottom clipping arrays for this subsector.
//
//=============================================================================

static void R_3D_SetSubsectorUDClip(vissubsector_t *vsub)
{
	assert(vsub->MinX != SHRT_MAX && vsub->MaxX != SHRT_MIN);
	assert(vsub->uclip == -1 && vsub->dclip == -1);

	int x = vsub->MinX;
	int width = vsub->MaxX - x;

	vsub->uclip = R_NewOpening(width);
	vsub->dclip = R_NewOpening(width);

	memcpy(openings + vsub->uclip, ceilingclip + x, sizeof(*ceilingclip)*width);
	memcpy(openings + vsub->dclip, floorclip + x, sizeof(*floorclip)*width);
}


//=============================================================================
//
// R_3D_SetupVisSubsector
//
// Decides which extra floor planes and walls should be drawn.
//
//=============================================================================

void R_3D_SetupVisSubsector(vissubsector_t *vsub, subsector_t *sub)
{
	R_3D_SetSubsectorUDClip(vsub);

	if (sub->sector->e != NULL)
	{
		for (unsigned i = sub->sector->e->XFloor.ffloors.Size(); i-- > 0; )
		{
			F3DFloor *ffloor = sub->sector->e->XFloor.ffloors[i];
			visxplane_t *top, *bot;
			bool pvisible, wvisible, visside;
			fixed_t topz, botz;

			if (ffloor->model == NULL) continue;
			if (!(ffloor->flags & FF_EXISTS)) continue;

			pvisible = (ffloor->flags & FF_RENDERPLANES) &&
				!(ffloor->alpha <= 0 && (ffloor->flags & (FF_TRANSLUCENT | FF_ADDITIVETRANS)));
			wvisible = (ffloor->flags & FF_RENDERSIDES) &&
				!(ffloor->alpha <= 0 && (ffloor->flags & (FF_TRANSLUCENT | FF_ADDITIVETRANS)));

			if (!pvisible && !wvisible)
			{ // Skip invisible planes if they have no visible walls attached.
				continue;
			}

			top = bot = NULL;

			// Check top of floor
			topz = ffloor->top.plane->ZatPoint(viewx, viewy);
			if (viewz > topz)
			{ // Above the top
				visside = !(ffloor->flags & FF_INVERTPLANES) || (ffloor->flags & FF_BOTHPLANES);
				if (wvisible || visside)
				{
					top = AddVisXPlane(vsub, sub->sector, ffloor, &ffloor->top, sector_t::floor,
						pvisible & visside);
				}
			}
			else if (viewz < topz)
			{ // Below the top
				visside = !!(ffloor->flags & (FF_INVERTPLANES | FF_BOTHPLANES));
				if (wvisible || visside)
				{
					top = AddVisXPlane(vsub, sub->sector, ffloor, &ffloor->top, sector_t::ceiling,
						pvisible & visside);
				}
			}
			else if (wvisible)
			{ // Right at eye level
				top = AddVisXPlane(vsub, sub->sector, ffloor, &ffloor->top, sector_t::floor, false);
			}

			// Check bottom of floor
			botz = ffloor->bottom.plane->ZatPoint(viewx, viewy);
			if (viewz > botz)
			{ // Above the bottom
				visside = !!(ffloor->flags & (FF_INVERTPLANES | FF_BOTHPLANES));
				if (wvisible || visside)
				{
					bot = AddVisXPlane(vsub, sub->sector, ffloor, &ffloor->bottom, sector_t::floor,
						pvisible & visside);
				}
			}
			else if (viewz < botz)
			{ // Below the bottom
				visside = !(ffloor->flags & FF_INVERTPLANES) || (ffloor->flags & FF_BOTHPLANES);
				if (wvisible || visside)
				{
					bot = AddVisXPlane(vsub, sub->sector, ffloor, &ffloor->bottom, sector_t::ceiling,
						pvisible & visside);
				}
			}
			else if (wvisible)
			{ // Right at eye level
				bot = AddVisXPlane(vsub, sub->sector, ffloor, &ffloor->bottom, sector_t::ceiling, false);
			}

			if (wvisible)
			{
				assert(top != NULL && bot != NULL);
				R_3D_AddVisWalls(ffloor, top, topz, bot, botz, vsub->FarSegs, MARK_FAR);
				R_3D_AddVisWalls(ffloor, top, topz, bot, botz, vsub->NearSegs, MARK_NEAR);
			}
		}
	}
}

//=============================================================================
//
// AddVisXPlane
//
// Attaches a new visible extra floor plane to a vissubsector.
//
//=============================================================================

static visxplane_t *AddVisXPlane(vissubsector_t *vsub, sector_t *sec, F3DFloor *ffloor, F3DFloor::planeref *planeref, int orientation, bool visible)
{
	visxplane_t *xplane = R_NewVisXPlane();
	lightlist_t *light;

	xplane->PlaneRef = planeref;
	xplane->Orientation = orientation;
	xplane->FakeFloor = ffloor;
	xplane->Plane = *planeref->plane;
	xplane->bVisible = visible;
	if ((orientation == sector_t::ceiling && xplane->Plane.c > 0) ||
		(orientation == sector_t::floor && xplane->Plane.c < 0))
	{
		xplane->Plane.FlipVert();
	}

	light = P_GetPlaneLight(sec, planeref->plane, orientation == sector_t::ceiling);
	xplane->Colormap = light->extra_colormap;
	xplane->LightLevel = r_actualextralight + ((fixedlightlev >= 0) ? fixedlightlev : *light->p_lightlevel);

	xplane->Next = vsub->Planes;
	vsub->Planes = xplane;

	// Initialize with current floor/ceilingclip.
	memcpy(openings + xplane->UClip, ceilingclip, sizeof(short)*viewwidth);
	memcpy(openings + xplane->DClip, floorclip, sizeof(short)*viewwidth);

	return xplane;
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
	visxplane_t *xplane = (visxplane_t *)VisXPlaneArena.Alloc(sizeof(visxplane_t));

	xplane->Next = NULL;
	xplane->UClip = R_NewOpening(viewwidth);
	xplane->DClip = R_NewOpening(viewwidth);
	xplane->NearWalls = NULL;
	xplane->FarWalls = NULL;
	xplane->LightLevel = 0;
	xplane->Orientation = -1;

	return xplane;
}

//=============================================================================
//
// R_NewVisSeg
//
//=============================================================================

visseg_t *R_NewVisSeg()
{
	visseg_t *vseg = (visseg_t *)VisXPlaneArena.Alloc(sizeof(visseg_t));

	vseg->Next = NULL;
	vseg->Seg = NULL;

	return vseg;
}

//=============================================================================
//
// R_NewVisXWall
//
// Creates a new visxwall_t.
//
//=============================================================================

visxwall_t *R_NewVisXWall()
{
	visxwall_t *xwall = (visxwall_t *)VisXPlaneArena.Alloc(sizeof(visxwall_t));
	xwall->Next = NULL;
	xwall->UClip = INT_MIN;
	xwall->DClip = INT_MIN;
	return xwall;
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
// R_3D_AddVisWalls
//
// Adds all viswalls visible between two visxplanes, for either the far or
// near end of the subsector.
//
//=============================================================================

static void R_3D_AddVisWalls(F3DFloor *ffloor,
	visxplane_t *top, fixed_t topz,
	visxplane_t *bot, fixed_t botz,
	visseg_t *vseg, EMarkPlaneEdge edge)
{
	assert(ffloor->flags & FF_RENDERSIDES);

	visxplane_t *attachplane;

	if (edge == MARK_NEAR)		// exterior walls
	{
		if ((ffloor->flags & (FF_INVERTSIDES | FF_ALLSIDES)) == FF_INVERTSIDES)
		{ // Only interior sides are visible for this extra floor.
			return;
		}
		// If we are above the extra floor, we want to attach it to the top plane.
		// Otherwise, we attatch it to the bottom plane.
		attachplane = (viewz >= topz) ? top : bot;
	}
	else						// interior walls
	{
		if ((ffloor->flags & (FF_INVERTSIDES | FF_ALLSIDES)) == 0)
		{ // Only exterior sides are visible for this extra floor.
			return;
		}
		// If we are below the extra floor, we want to attach it to the top plane.
		// Otherwise, we attach it to the bottom plane.
		attachplane = (viewz < topz) ? top : bot;
	}

	for (; vseg != NULL; vseg = vseg->Next)
	{
		seg_t *seg = vseg->Seg;

		// Does this particular seg get a wall attached to it?
		if (edge == MARK_NEAR && seg->backsector == NULL)
		{ // Not if there is no back side (and this is an exterior wall).
			continue;
		}
		if (seg->backsector != NULL && seg->backsector->tag == seg->frontsector->tag)
		{ // Not if both sides have the same sector tag.
			continue;
		}

		// Store it.
		visxwall_t *vwall = R_NewVisXWall();
		vwall->VisSeg = vseg;
		vwall->LightingSector = (edge == MARK_FAR) ? ffloor->model : seg->backsector;

		// If below the top plane, its uclip is the front edge and its dclip is the back edge.
		// If above the top plane, its dclip is the front edge and its uclip is the back edge.
		vwall->UClip = ((top->Orientation == sector_t::ceiling) ^ (edge == MARK_FAR)) ? top->UClip : top->DClip;
		vwall->UClip += vseg->TMap.SX1;

		// The bottom plane is the same.
		vwall->DClip = ((bot->Orientation == sector_t::ceiling) ^ (edge == MARK_FAR)) ? bot->UClip : bot->DClip;
		vwall->DClip += vseg->TMap.SX1;

		if (edge == MARK_NEAR)
		{
			vwall->Next = attachplane->NearWalls;
			attachplane->NearWalls = vwall;
		}
		else
		{
			vwall->Next = attachplane->FarWalls;
			attachplane->FarWalls = vwall;
		}
	}
}

//=============================================================================
//
// R_3D_MarkPlanes
//
// Marks either the near or far edges of any extra planes in the subsector.
// Uses global variables set up by R_AddLine().
//
//=============================================================================

void R_3D_MarkPlanes(vissubsector_t *vsub, visseg_t *vseg, vertex_t *v1, vertex_t *v2)
{
	seg_t *seg = vseg->Seg;
	const FWallTexMapParm *tmap = &vseg->TMap;

	assert((v1 == seg->v1 && v2 == seg->v2) || (v2 == seg->v1 && v1 == seg->v2));

	EMarkPlaneEdge edge = (seg->v1 == v1) ? MARK_FAR : MARK_NEAR;

	for (visxplane_t *xplane = vsub->Planes; xplane != NULL; xplane = xplane->Next)
	{
		short most[MAXWIDTH], *in;
		short *out, *uclip, *dclip;

		WallMost(most, tmap, xplane->Plane, v1, v2);

		// Clip to existing bounds.
		uclip = openings + xplane->UClip;
		dclip = openings + xplane->DClip;
		if (xplane->Orientation == sector_t::ceiling)
		{ // For a ceiling, the near edge is the top, and the far edge is the bottom.
			out = edge == MARK_NEAR ? uclip : dclip;
		}
		else
		{ // For a floor, the near edge is the bottom, and the far edge is the top.
			out = edge == MARK_NEAR ? dclip : uclip;
		}
		out += tmap->SX1;
		in = most + tmap->SX1;
		uclip += tmap->SX1;
		dclip += tmap->SX1;
		for (int i = tmap->SX2 - tmap->SX1; i > 0; --i)
		{
			*out++ = clamp<short>(*in++, *uclip++, *dclip++);
		}
	}
}
