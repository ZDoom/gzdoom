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

#include "p_3dmidtex.h"
#include "stats.h"
#include "r_data/r_interpolate.h"
#include "p_local.h"
#include "po_man.h"
#include "serializer.h"
#include "g_levellocals.h"

//==========================================================================
//
//
//
//==========================================================================

class DSectorPlaneInterpolation : public DInterpolation
{
	DECLARE_CLASS(DSectorPlaneInterpolation, DInterpolation)

	sector_t *sector;
	double oldheight, oldtexz;
	double bakheight, baktexz;
	bool ceiling;
	TArray<DInterpolation *> attached;


public:

	DSectorPlaneInterpolation() {}
	DSectorPlaneInterpolation(sector_t *sector, bool plane, bool attach);
	void UnlinkFromMap() override;
	void UpdateInterpolation();
	void Restore();
	void Interpolate(double smoothratio);
	
	virtual void Serialize(FSerializer &arc);
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
	double oldx, oldy;
	double bakx, baky;
	bool ceiling;

public:

	DSectorScrollInterpolation() {}
	DSectorScrollInterpolation(sector_t *sector, bool plane);
	void UnlinkFromMap() override;
	void UpdateInterpolation();
	void Restore();
	void Interpolate(double smoothratio);
	
	virtual void Serialize(FSerializer &arc);
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
	double oldx, oldy;
	double bakx, baky;

public:

	DWallScrollInterpolation() {}
	DWallScrollInterpolation(side_t *side, int part);
	void UnlinkFromMap() override;
	void UpdateInterpolation();
	void Restore();
	void Interpolate(double smoothratio);
	
	virtual void Serialize(FSerializer &arc);
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
	TArray<double> oldverts, bakverts;
	double oldcx, oldcy;
	double bakcx, bakcy;

public:

	DPolyobjInterpolation() {}
	DPolyobjInterpolation(FPolyObj *poly);
	void UnlinkFromMap() override;
	void UpdateInterpolation();
	void Restore();
	void Interpolate(double smoothratio);
	
	virtual void Serialize(FSerializer &arc);
};


//==========================================================================
//
//
//
//==========================================================================

IMPLEMENT_CLASS(DInterpolation, true, true)

IMPLEMENT_POINTERS_START(DInterpolation)
	IMPLEMENT_POINTER(Next)
	IMPLEMENT_POINTER(Prev)
IMPLEMENT_POINTERS_END

IMPLEMENT_CLASS(DSectorPlaneInterpolation, false, false)
IMPLEMENT_CLASS(DSectorScrollInterpolation, false, false)
IMPLEMENT_CLASS(DWallScrollInterpolation, false, false)
IMPLEMENT_CLASS(DPolyobjInterpolation, false, false)

//==========================================================================
//
//
//
//==========================================================================

int FInterpolator::CountInterpolations ()
{
	int count = 0;
	for (DInterpolation *probe = Head; probe != nullptr; probe = probe->Next)
	{
		count++;
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
	for (DInterpolation *probe = Head; probe != nullptr; probe = probe->Next)
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
	if (Head != nullptr) Head->Prev = interp;
	interp->Prev = nullptr;
	Head = interp;
}

//==========================================================================
//
//
//
//==========================================================================

void FInterpolator::RemoveInterpolation(DInterpolation *interp)
{
	if (Head == interp)
	{
		Head = interp->Next;
		if (Head != nullptr) Head->Prev = nullptr;
	}
	else
	{
		if (interp->Prev != nullptr) interp->Prev->Next = interp->Next;
		if (interp->Next != nullptr) interp->Next->Prev = interp->Prev;
	}
	interp->Next = nullptr;
	interp->Prev = nullptr;
}

//==========================================================================
//
//
//
//==========================================================================

void FInterpolator::DoInterpolations(double smoothratio)
{
	if (smoothratio >= 1.)
	{
		didInterp = false;
		return;
	}

	didInterp = true;

	DInterpolation *probe = Head;
	while (probe != nullptr)
	{
		DInterpolation *next = probe->Next;
		probe->Interpolate(smoothratio);
		probe = next;
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
		for (DInterpolation *probe = Head; probe != nullptr; probe = probe->Next)
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
	DInterpolation *probe = Head;
	Head = nullptr;

	while (probe != nullptr)
	{
		DInterpolation *next = probe->Next;
		probe->Next = probe->Prev = nullptr;
		probe->UnlinkFromMap();
		probe->Destroy();
		probe = next;
	}

}

FSerializer &Serialize(FSerializer &arc, const char *key, FInterpolator &rs, FInterpolator *def)
{
	if (arc.BeginObject(key))
	{
		arc("head", rs.Head)
			.EndObject();
	}
	return arc;
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

int DInterpolation::AddRef()
{
	return ++refcount;
}

//==========================================================================
//
//
//
//==========================================================================

int DInterpolation::DelRef(bool force)
{
	if (refcount > 0) --refcount;
	if (force && refcount == 0)
	{
		UnlinkFromMap();
		Destroy();
	}
	return refcount;
}

//==========================================================================
//
//
//
//==========================================================================

void DInterpolation::UnlinkFromMap()
{
	Level->interpolator.RemoveInterpolation(this);
	refcount = 0;
}

//==========================================================================
//
//
//
//==========================================================================

void DInterpolation::Serialize(FSerializer &arc)
{
	Super::Serialize(arc);
	arc("refcount", refcount)
		("next", Next)
		("prev", Prev)
		("level", Level);
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
: DInterpolation(_sector->Level)
{
	sector = _sector;
	ceiling = _plane;
	UpdateInterpolation ();

	if (attach)
	{
		P_Start3dMidtexInterpolations(attached, sector, ceiling);
		P_StartLinkedSectorInterpolations(attached, sector, ceiling);
	}
	sector->Level->interpolator.AddInterpolation(this);
}

//==========================================================================
//
//
//
//==========================================================================

void DSectorPlaneInterpolation::UnlinkFromMap()
{
	if (sector != nullptr)
	{
		if (ceiling)
		{
			sector->interpolations[sector_t::CeilingMove] = nullptr;
		}
		else
		{
			sector->interpolations[sector_t::FloorMove] = nullptr;
		}
		sector = nullptr;
	}
	for (unsigned i = 0; i < attached.Size(); i++)
	{
		attached[i]->DelRef();
	}
	attached.Reset();
	Super::UnlinkFromMap();
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
		oldheight = sector->floorplane.fD();
		oldtexz = sector->GetPlaneTexZ(sector_t::floor);
	}
	else
	{
		oldheight = sector->ceilingplane.fD();
		oldtexz = sector->GetPlaneTexZ(sector_t::ceiling);
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
		sector->floorplane.setD(bakheight);
		sector->SetPlaneTexZ(sector_t::floor, baktexz, true);
	}
	else
	{
		sector->ceilingplane.setD(bakheight);
		sector->SetPlaneTexZ(sector_t::ceiling, baktexz, true);
	}
	P_RecalculateAttached3DFloors(sector);
	sector->CheckPortalPlane(ceiling? sector_t::ceiling : sector_t::floor);
}

//==========================================================================
//
//
//
//==========================================================================

void DSectorPlaneInterpolation::Interpolate(double smoothratio)
{
	secplane_t *pplane;
	int pos;

	if (!ceiling)
	{
		pplane = &sector->floorplane;
		pos = sector_t::floor;
	}
	else
	{
		pplane = &sector->ceilingplane;
		pos = sector_t::ceiling;
	}

	bakheight = pplane->fD();
	baktexz = sector->GetPlaneTexZ(pos);

	if (refcount == 0 && oldheight == bakheight)
	{
		UnlinkFromMap();
		Destroy();
	}
	else
	{
		pplane->setD(oldheight + (bakheight - oldheight) * smoothratio);
		sector->SetPlaneTexZ(pos, oldtexz + (baktexz - oldtexz) * smoothratio, true);
		P_RecalculateAttached3DFloors(sector);
		sector->CheckPortalPlane(pos);
	}
}

//==========================================================================
//
//
//
//==========================================================================

void DSectorPlaneInterpolation::Serialize(FSerializer &arc)
{
	Super::Serialize(arc);
	arc("sector", sector)
		("ceiling", ceiling)
		("oldheight", oldheight)
		("oldtexz", oldtexz)
		("attached", attached);
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
: DInterpolation(_sector->Level)
{
	sector = _sector;
	ceiling = _plane;
	UpdateInterpolation ();
	sector->Level->interpolator.AddInterpolation(this);
}

//==========================================================================
//
//
//
//==========================================================================

void DSectorScrollInterpolation::UnlinkFromMap()
{
	if (sector != nullptr)
	{
		if (ceiling)
		{
			sector->interpolations[sector_t::CeilingScroll] = nullptr;
		}
		else
		{
			sector->interpolations[sector_t::FloorScroll] = nullptr;
		}
		sector = nullptr;
	}
	Super::UnlinkFromMap();
}

//==========================================================================
//
//
//
//==========================================================================

void DSectorScrollInterpolation::UpdateInterpolation()
{
	oldx = sector->GetXOffset(ceiling);
	oldy = sector->GetYOffset(ceiling, false);
}

//==========================================================================
//
//
//
//==========================================================================

void DSectorScrollInterpolation::Restore()
{
	sector->SetXOffset(ceiling, bakx);
	sector->SetYOffset(ceiling, baky);
}

//==========================================================================
//
//
//
//==========================================================================

void DSectorScrollInterpolation::Interpolate(double smoothratio)
{
	bakx = sector->GetXOffset(ceiling);
	baky = sector->GetYOffset(ceiling, false);

	if (refcount == 0 && oldx == bakx && oldy == baky)
	{
		UnlinkFromMap();
		Destroy();
	}
	else
	{
		sector->SetXOffset(ceiling, oldx + (bakx - oldx) * smoothratio);
		sector->SetYOffset(ceiling, oldy + (baky - oldy) * smoothratio);
	}
}

//==========================================================================
//
//
//
//==========================================================================

void DSectorScrollInterpolation::Serialize(FSerializer &arc)
{
	Super::Serialize(arc);
	arc("sector", sector)
		("ceiling", ceiling)
		("oldx", oldx)
		("oldy", oldy);
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
: DInterpolation(_side->GetLevel())
{
	side = _side;
	part = _part;
	UpdateInterpolation ();
	side->GetLevel()->interpolator.AddInterpolation(this);
}

//==========================================================================
//
//
//
//==========================================================================

void DWallScrollInterpolation::UnlinkFromMap()
{
	if (side != nullptr)
	{
		side->textures[part].interpolation = nullptr;
		side = nullptr;
	}
	Super::UnlinkFromMap();
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

void DWallScrollInterpolation::Interpolate(double smoothratio)
{
	bakx = side->GetTextureXOffset(part);
	baky = side->GetTextureYOffset(part);

	if (refcount == 0 && oldx == bakx && oldy == baky)
	{
		UnlinkFromMap();
		Destroy();
	}
	else
	{
		side->SetTextureXOffset(part, oldx + (bakx - oldx) * smoothratio);
		side->SetTextureYOffset(part, oldy + (baky - oldy) * smoothratio);
	}
}

//==========================================================================
//
//
//
//==========================================================================

void DWallScrollInterpolation::Serialize(FSerializer &arc)
{
	Super::Serialize(arc);
	arc("side", side)
		("part", part)
		("oldx", oldx)
		("oldy", oldy);
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
: DInterpolation(po->Level)
{
	poly = po;
	oldverts.Resize(po->Vertices.Size() << 1);
	bakverts.Resize(po->Vertices.Size() << 1);
	UpdateInterpolation ();
	po->Level->interpolator.AddInterpolation(this);
}

//==========================================================================
//
//
//
//==========================================================================

void DPolyobjInterpolation::UnlinkFromMap()
{
	if (poly != nullptr)
	{
		poly->interpolation = nullptr;
	}
	Super::UnlinkFromMap();
}

//==========================================================================
//
//
//
//==========================================================================

void DPolyobjInterpolation::UpdateInterpolation()
{
	for(unsigned int i = 0; i < poly->Vertices.Size(); i++)
	{
		oldverts[i*2  ] = poly->Vertices[i]->fX();
		oldverts[i*2+1] = poly->Vertices[i]->fY();
	}
	oldcx = poly->CenterSpot.pos.X;
	oldcy = poly->CenterSpot.pos.Y;
}

//==========================================================================
//
//
//
//==========================================================================

void DPolyobjInterpolation::Restore()
{
	for(unsigned int i = 0; i < poly->Vertices.Size(); i++)
	{
		poly->Vertices[i]->set(bakverts[i*2  ], bakverts[i*2+1]);
	}
	poly->CenterSpot.pos.X = bakcx;
	poly->CenterSpot.pos.Y = bakcy;
	poly->ClearSubsectorLinks();
}

//==========================================================================
//
//
//
//==========================================================================

void DPolyobjInterpolation::Interpolate(double smoothratio)
{
	bool changed = false;
	for(unsigned int i = 0; i < poly->Vertices.Size(); i++)
	{
		bakverts[i*2  ] = poly->Vertices[i]->fX();
		bakverts[i*2+1] = poly->Vertices[i]->fY();

		if (bakverts[i * 2] != oldverts[i * 2] || bakverts[i * 2 + 1] != oldverts[i * 2 + 1])
		{
			changed = true;
			poly->Vertices[i]->set(
				oldverts[i * 2] + (bakverts[i * 2] - oldverts[i * 2]) * smoothratio,
				oldverts[i * 2 + 1] + (bakverts[i * 2 + 1] - oldverts[i * 2 + 1]) * smoothratio);
		}
	}
	if (refcount == 0 && !changed)
	{
		UnlinkFromMap();
		Destroy();
	}
	else
	{
		bakcx = poly->CenterSpot.pos.X;
		bakcy = poly->CenterSpot.pos.Y;
		poly->CenterSpot.pos.X = bakcx + (bakcx - oldcx) * smoothratio;
		poly->CenterSpot.pos.Y = bakcy + (bakcy - oldcy) * smoothratio;

		poly->ClearSubsectorLinks();
	}
}

//==========================================================================
//
//
//
//==========================================================================

void DPolyobjInterpolation::Serialize(FSerializer &arc)
{
	Super::Serialize(arc);
	arc("poly", poly)
		("oldverts", oldverts)
		("oldcx", oldcx)
		("oldcy", oldcy);
	if (arc.isReading()) bakverts.Resize(oldverts.Size());
}


//==========================================================================
//
//
//
//==========================================================================

DInterpolation *side_t::SetInterpolation(int position)
{
	if (textures[position].interpolation == nullptr)
	{
		textures[position].interpolation = Create<DWallScrollInterpolation>(this, position);
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
	if (textures[position].interpolation != nullptr)
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
	if (interpolations[position] == nullptr)
	{
		DInterpolation *interp;
		switch (position)
		{
		case sector_t::CeilingMove:
			interp = Create<DSectorPlaneInterpolation>(this, true, attach);
			break;

		case sector_t::FloorMove:
			interp = Create<DSectorPlaneInterpolation>(this, false, attach);
			break;

		case sector_t::CeilingScroll:
			interp = Create<DSectorScrollInterpolation>(this, true);
			break;

		case sector_t::FloorScroll:
			interp = Create<DSectorScrollInterpolation>(this, false);
			break;

		default:
			return nullptr;
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

DInterpolation *FPolyObj::SetInterpolation()
{
	if (interpolation != nullptr)
	{
		interpolation->AddRef();
	}
	else
	{
		interpolation = Create<DPolyobjInterpolation>(this);
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
	if (interpolation != nullptr)
	{
		interpolation->DelRef();
	}
}


