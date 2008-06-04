/*
** r_interpolate.cpp
**
** Movement interpolation
**
**---------------------------------------------------------------------------
** Copyright 2008 Christoph Oelckers
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
*/

#include "r_data.h"
#include "p_3dmidtex.h"
#include "stats.h"
#include "r_interpolate.h"
#include "p_local.h"

//==========================================================================
//
//
//
//==========================================================================

class DSectorPlaneInterpolation : public DInterpolation
{
	DECLARE_CLASS(DSectorPlaneInterpolation, DInterpolation)

	sector_t *sector;
	fixed_t oldheight, oldtexz;
	fixed_t bakheight, baktexz;
	bool ceiling;
	TArray<DInterpolation *> attached;


public:

	DSectorPlaneInterpolation() {}
	DSectorPlaneInterpolation(sector_t *sector, bool plane, bool attach);
	void Destroy();
	void UpdateInterpolation();
	void Restore();
	void Interpolate(fixed_t smoothratio);
	void Serialize(FArchive &arc);
	size_t PointerSubstitution (DObject *old, DObject *notOld);
	size_t PropagateMark();
};

//==========================================================================
//
//
//
//==========================================================================

class DSectorScrollInterpolation : public DInterpolation
{
	DECLARE_CLASS(DSectorScrollInterpolation, DInterpolation)

	sector_t *sector;
	fixed_t oldx, oldy;
	fixed_t bakx, baky;
	bool ceiling;

public:

	DSectorScrollInterpolation() {}
	DSectorScrollInterpolation(sector_t *sector, bool plane);
	void UpdateInterpolation();
	void Restore();
	void Interpolate(fixed_t smoothratio);
	void Serialize(FArchive &arc);
};


//==========================================================================
//
//
//
//==========================================================================

class DWallScrollInterpolation : public DInterpolation
{
	DECLARE_CLASS(DWallScrollInterpolation, DInterpolation)

	side_t *side;
	int part;
	fixed_t oldx, oldy;
	fixed_t bakx, baky;

public:

	DWallScrollInterpolation() {}
	DWallScrollInterpolation(side_t *side, int part);
	void UpdateInterpolation();
	void Restore();
	void Interpolate(fixed_t smoothratio);
	void Serialize(FArchive &arc);
};

//==========================================================================
//
//
//
//==========================================================================

class DPolyobjInterpolation : public DInterpolation
{
	DECLARE_CLASS(DPolyobjInterpolation, DInterpolation)

	FPolyObj *poly;
	TArray<fixed_t> oldverts, bakverts;

public:

	DPolyobjInterpolation() {}
	DPolyobjInterpolation(FPolyObj *poly);
	void UpdateInterpolation();
	void Restore();
	void Interpolate(fixed_t smoothratio);
	void Serialize(FArchive &arc);
};


//==========================================================================
//
//
//
//==========================================================================

IMPLEMENT_ABSTRACT_CLASS(DInterpolation)
IMPLEMENT_CLASS(DSectorPlaneInterpolation)
IMPLEMENT_CLASS(DSectorScrollInterpolation)
IMPLEMENT_CLASS(DWallScrollInterpolation)
IMPLEMENT_CLASS(DPolyobjInterpolation)

//==========================================================================
//
// Important note:
// The linked list of interpolations and the pointers in the interpolated
// objects are not processed by the garbage collector. This is intentional!
//
// If an interpolation is no longer owned by any thinker it should
// be destroyed even if the interpolator still has a link to it.
//
// When such an interpolation is destroyed by the garbage collector it
// will automatically be unlinked from the list.
//
//==========================================================================

FInterpolator interpolator;

//==========================================================================
//
//
//
//==========================================================================

int FInterpolator::CountInterpolations ()
{
	int count = 0;

	DInterpolation *probe = Head;
	while (probe != NULL)
	{
		count++;
		probe = probe->Next;
	}
	return count;
}

//==========================================================================
//
//
//
//==========================================================================

void FInterpolator::UpdateInterpolations()
{
	for (DInterpolation *probe = Head; probe != NULL; probe = probe->Next)
	{
		probe->UpdateInterpolation ();
	}
}

//==========================================================================
//
//
//
//==========================================================================

void FInterpolator::AddInterpolation(DInterpolation *interp)
{
	interp->Next = Head;
	if (Head != NULL) Head->Prev = &interp->Next;
	Head = interp;
	interp->Prev = &Head;
}

//==========================================================================
//
//
//
//==========================================================================

void FInterpolator::RemoveInterpolation(DInterpolation *interp)
{
	if (interp->Prev != NULL)
	{
		*interp->Prev = interp->Next;
		if (interp->Next != NULL) interp->Next->Prev = interp->Prev;
		interp->Next = NULL;
		interp->Prev = NULL;
	}
}

//==========================================================================
//
//
//
//==========================================================================

void FInterpolator::DoInterpolations(fixed_t smoothratio)
{
	if (smoothratio == FRACUNIT)
	{
		didInterp = false;
		return;
	}

	didInterp = true;

	for (DInterpolation *probe = Head; probe != NULL; probe = probe->Next)
	{
		probe->Interpolate(smoothratio);
	}
}

//==========================================================================
//
//
//
//==========================================================================

void FInterpolator::RestoreInterpolations()
{
	if (didInterp)
	{
		didInterp = false;
		for (DInterpolation *probe = Head; probe != NULL; probe = probe->Next)
		{
			probe->Restore();
		}
	}
}

//==========================================================================
//
//
//
//==========================================================================

void FInterpolator::ClearInterpolations()
{
	for (DInterpolation *probe = Head; probe != NULL; )
	{
		DInterpolation *next = probe->Next;
		probe->Destroy();
		probe = next;
	}
	Head = NULL;
}


//==========================================================================
//
//
//
//==========================================================================

//==========================================================================
//
//
//
//==========================================================================

DInterpolation::DInterpolation()
{
	Next = NULL;
	Prev = NULL;
	refcount = 0;
}

//==========================================================================
//
//
//
//==========================================================================

int DInterpolation::AddRef()
{
	return ++refcount;
}

//==========================================================================
//
//
//
//==========================================================================

int DInterpolation::DelRef()
{
	if (refcount > 0) --refcount;
	if (refcount <= 0 && !(ObjectFlags & OF_EuthanizeMe))
	{
		Destroy();
	}
	return refcount;
}

//==========================================================================
//
//
//
//==========================================================================

void DInterpolation::Destroy()
{
	interpolator.RemoveInterpolation(this);
	refcount = 0;
	Super::Destroy();
}

//==========================================================================
//
//
//
//==========================================================================

void DInterpolation::Serialize(FArchive &arc)
{
	Super::Serialize(arc);
	arc << refcount;
	if (arc.IsLoading())
	{
		interpolator.AddInterpolation(this);
	}
}

//==========================================================================
//
//
//
//==========================================================================

//==========================================================================
//
//
//
//==========================================================================

DSectorPlaneInterpolation::DSectorPlaneInterpolation(sector_t *_sector, bool _plane, bool attach)
{
	sector = _sector;
	ceiling = _plane;
	UpdateInterpolation ();

	if (attach)
	{
		P_Start3dMidtexInterpolations(attached, sector, ceiling);
		P_StartLinkedSectorInterpolations(attached, sector, ceiling);
	}
	interpolator.AddInterpolation(this);
}

//==========================================================================
//
//
//
//==========================================================================

void DSectorPlaneInterpolation::Destroy()
{
	for(unsigned i=0; i<attached.Size(); i++)
	{
		attached[i]->DelRef();
	}
	Super::Destroy();
}

//==========================================================================
//
//
//
//==========================================================================

void DSectorPlaneInterpolation::UpdateInterpolation()
{
	if (!ceiling)
	{
		oldheight = sector->floorplane.d;
		oldtexz = sector->floortexz;
	}
	else
	{
		oldheight = sector->ceilingplane.d;
		oldtexz = sector->ceilingtexz;
	}
}

//==========================================================================
//
//
//
//==========================================================================

void DSectorPlaneInterpolation::Restore()
{
	if (!ceiling)
	{
		sector->floorplane.d = bakheight;
		sector->floortexz = baktexz;
	}
	else
	{
		sector->ceilingplane.d = bakheight;
		sector->ceilingtexz = baktexz;
	}
}

//==========================================================================
//
//
//
//==========================================================================

void DSectorPlaneInterpolation::Interpolate(fixed_t smoothratio)
{
	fixed_t *pheight;
	fixed_t *ptexz;

	if (!ceiling)
	{
		pheight = &sector->floorplane.d;
		ptexz = &sector->floortexz;
	}
	else
	{
		pheight = &sector->ceilingplane.d;
		ptexz = &sector->ceilingtexz;
	}

	bakheight = *pheight;
	baktexz = *ptexz;

	*pheight = oldheight + FixedMul(bakheight - oldheight, smoothratio);
	*ptexz = oldtexz + FixedMul(baktexz - oldtexz, smoothratio);
}

//==========================================================================
//
//
//
//==========================================================================

void DSectorPlaneInterpolation::Serialize(FArchive &arc)
{
	Super::Serialize(arc);
	arc << sector << ceiling << oldheight << oldtexz << attached;
}

//==========================================================================
//
//
//
//==========================================================================

size_t DSectorPlaneInterpolation::PointerSubstitution (DObject *old, DObject *notOld)
{
	int subst = 0;
	for(unsigned i=0; i<attached.Size(); i++)
	{
		if (attached[i] == old) 
		{
			attached[i] = (DInterpolation*)notOld;
			subst++;
		}
	}
	return subst;
}

//==========================================================================
//
//
//
//==========================================================================

size_t DSectorPlaneInterpolation::PropagateMark()
{
	for(unsigned i=0; i<attached.Size(); i++)
	{
		GC::Mark(attached[i]);
	}
	return Super::PropagateMark();
}

//==========================================================================
//
//
//
//==========================================================================

//==========================================================================
//
//
//
//==========================================================================

DSectorScrollInterpolation::DSectorScrollInterpolation(sector_t *_sector, bool _plane)
{
	sector = _sector;
	ceiling = _plane;
	UpdateInterpolation ();
	interpolator.AddInterpolation(this);
}

//==========================================================================
//
//
//
//==========================================================================

void DSectorScrollInterpolation::UpdateInterpolation()
{
	if (!ceiling)
	{
		oldx = sector->floor_xoffs;
		oldy = sector->floor_yoffs;
	}
	else
	{
		oldx = sector->ceiling_xoffs;
		oldy = sector->ceiling_yoffs;
	}
}

//==========================================================================
//
//
//
//==========================================================================

void DSectorScrollInterpolation::Restore()
{
	if (!ceiling)
	{
		sector->floor_xoffs = bakx;
		sector->floor_yoffs = baky;
	}
	else
	{
		sector->ceiling_xoffs = bakx;
		sector->ceiling_yoffs = baky;
	}
}

//==========================================================================
//
//
//
//==========================================================================

void DSectorScrollInterpolation::Interpolate(fixed_t smoothratio)
{
	fixed_t *px;
	fixed_t *py;

	if (!ceiling)
	{
		px = &sector->floor_xoffs;
		py = &sector->floor_yoffs;
	}
	else
	{
		px = &sector->ceiling_xoffs;
		py = &sector->ceiling_yoffs;
	}

	bakx = *px;
	baky = *py;

	*px = oldx + FixedMul(bakx - oldx, smoothratio);
	*py = oldy + FixedMul(baky - oldy, smoothratio);
}

//==========================================================================
//
//
//
//==========================================================================

void DSectorScrollInterpolation::Serialize(FArchive &arc)
{
	Super::Serialize(arc);
	arc << sector << ceiling << oldx << oldy;
}


//==========================================================================
//
//
//
//==========================================================================

//==========================================================================
//
//
//
//==========================================================================

DWallScrollInterpolation::DWallScrollInterpolation(side_t *_side, int _part)
{
	side = _side;
	part = _part;
	UpdateInterpolation ();
	interpolator.AddInterpolation(this);
}

//==========================================================================
//
//
//
//==========================================================================

void DWallScrollInterpolation::UpdateInterpolation()
{
	oldx = side->GetTextureXOffset(part);
	oldy = side->GetTextureYOffset(part);
}

//==========================================================================
//
//
//
//==========================================================================

void DWallScrollInterpolation::Restore()
{
	side->SetTextureXOffset(part, bakx);
	side->SetTextureYOffset(part, baky);
}

//==========================================================================
//
//
//
//==========================================================================

void DWallScrollInterpolation::Interpolate(fixed_t smoothratio)
{
	bakx = side->GetTextureXOffset(part);
	baky = side->GetTextureYOffset(part);

	side->SetTextureXOffset(part, oldx + FixedMul(bakx - oldx, smoothratio));
	side->SetTextureYOffset(part, oldy + FixedMul(baky - oldy, smoothratio));
}

//==========================================================================
//
//
//
//==========================================================================

void DWallScrollInterpolation::Serialize(FArchive &arc)
{
	Super::Serialize(arc);
	arc << side << part << oldx << oldy;
}

//==========================================================================
//
//
//
//==========================================================================

//==========================================================================
//
//
//
//==========================================================================

DPolyobjInterpolation::DPolyobjInterpolation(FPolyObj *po)
{
	poly = po;
	oldverts.Resize(po->numvertices<<1);
	bakverts.Resize(po->numvertices<<1);
	UpdateInterpolation ();
	interpolator.AddInterpolation(this);
}

//==========================================================================
//
//
//
//==========================================================================

void DPolyobjInterpolation::UpdateInterpolation()
{
	for(int i = 0; i < poly->numvertices; i++)
	{
		oldverts[i*2  ] = poly->vertices[i]->x;
		oldverts[i*2+1] = poly->vertices[i]->y;
	}
}

//==========================================================================
//
//
//
//==========================================================================

void DPolyobjInterpolation::Restore()
{
	for(int i = 0; i < poly->numvertices; i++)
	{
		poly->vertices[i]->x = bakverts[i*2  ];
		poly->vertices[i]->y = bakverts[i*2+1];
	}
	//poly->Moved();
}

//==========================================================================
//
//
//
//==========================================================================

void DPolyobjInterpolation::Interpolate(fixed_t smoothratio)
{
	for(int i = 0; i < poly->numvertices; i++)
	{
		fixed_t *px = &poly->vertices[i]->x;
		fixed_t *py = &poly->vertices[i]->y;

		bakverts[i*2  ] = *px;
		bakverts[i*2+1] = *py;

		*px = oldverts[i*2  ] + FixedMul(bakverts[i*2  ] - oldverts[i*2  ], smoothratio);
		*py = oldverts[i*2+1] + FixedMul(bakverts[i*2+1] - oldverts[i*2+1], smoothratio);
	}
	//poly->Moved();
}

//==========================================================================
//
//
//
//==========================================================================

void DPolyobjInterpolation::Serialize(FArchive &arc)
{

	Super::Serialize(arc);
	int po = int(poly - polyobjs);
	arc << po << oldverts;
	poly = polyobjs + po;
	if (arc.IsLoading()) bakverts.Resize(oldverts.Size());
}


//==========================================================================
//
//
//
//==========================================================================

DInterpolation *side_t::SetInterpolation(int position)
{
	if (textures[position].interpolation == NULL)
	{
		textures[position].interpolation = new DWallScrollInterpolation(this, position);
		textures[position].interpolation->AddRef();
	}
	textures[position].interpolation->AddRef();
	GC::WriteBarrier(textures[position].interpolation);
	return textures[position].interpolation;
}

//==========================================================================
//
//
//
//==========================================================================

void side_t::StopInterpolation(int position)
{
	if (textures[position].interpolation != NULL)
	{
		textures[position].interpolation->DelRef();
	}
}

//==========================================================================
//
//
//
//==========================================================================

DInterpolation *sector_t::SetInterpolation(int position, bool attach)
{
	if (interpolations[position] == NULL)
	{
		DInterpolation *interp;
		switch (position)
		{
		case sector_t::CeilingMove:
			interp = new DSectorPlaneInterpolation(this, true, attach);
			break;

		case sector_t::FloorMove:
			interp = new DSectorPlaneInterpolation(this, false, attach);
			break;

		case sector_t::CeilingScroll:
			interp = new DSectorScrollInterpolation(this, true);
			break;

		case sector_t::FloorScroll:
			interp = new DSectorScrollInterpolation(this, false);
			break;

		default:
			return NULL;
		}
		interpolations[position] = interp;
	}
	interpolations[position]->AddRef();
	GC::WriteBarrier(interpolations[position]);
	return interpolations[position];
}

//==========================================================================
//
//
//
//==========================================================================

void sector_t::StopInterpolation(int position)
{
	if (interpolations[position] != NULL)
	{
		interpolations[position]->DelRef();
	}
}

//==========================================================================
//
//
//
//==========================================================================

DInterpolation *FPolyObj::SetInterpolation()
{
	if (interpolation != NULL)
	{
		interpolation->AddRef();
	}
	else
	{
		interpolation = new DPolyobjInterpolation(this);
		interpolation->AddRef();
	}
	GC::WriteBarrier(interpolation);
	return interpolation;
}

//==========================================================================
//
//
//
//==========================================================================

void FPolyObj::StopInterpolation()
{
	if (interpolation != NULL)
	{
		interpolation->DelRef();
	}
}


ADD_STAT (interpolations)
{
	FString out;
	out.Format ("%d interpolations", interpolator.CountInterpolations ());
	return out;
}


