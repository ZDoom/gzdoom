/*
 ** a_lighttransfer.cpp
 **
 **---------------------------------------------------------------------------
 ** Copyright 1998-2016 Randy Heit
 ** Copyright 2003-2018 Christoph Oelckers
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

#include "p_spec.h"
#include "a_lighttransfer.h"
#include "serializer_doom.h"
#include "g_levellocals.h"

//
// SPECIAL SPAWNING
//

IMPLEMENT_CLASS(DLightTransfer, false, false)

void DLightTransfer::Serialize(FSerializer &arc)
{
	Super::Serialize (arc);
	arc("lastlight", LastLight)
		("source", Source)
		("targettag", TargetTag)
		("copyfloor", CopyFloor);
}

void DLightTransfer::Construct(sector_t *srcSec, int target, bool copyFloor)
{
	int secnum;

	Source = srcSec;
	TargetTag = target;
	CopyFloor = copyFloor;
	DoTransfer (LastLight = srcSec->lightlevel, target, copyFloor);

	if (copyFloor)
	{
		auto itr = Level->GetSectorTagIterator(target);
		while ((secnum = itr.Next()) >= 0)
			Level->sectors[secnum].ChangeFlags(sector_t::floor, 0, PLANEF_ABSLIGHTING);
	}
	else
	{
		auto itr = Level->GetSectorTagIterator(target);
		while ((secnum = itr.Next()) >= 0)
			Level->sectors[secnum].ChangeFlags(sector_t::ceiling, 0, PLANEF_ABSLIGHTING);
	}
}

void DLightTransfer::Tick ()
{
	int light = Source->lightlevel;

	if (light != LastLight)
	{
		LastLight = light;
		DoTransfer (light, TargetTag, CopyFloor);
	}
}

void DLightTransfer::DoTransfer (int llevel, int target, bool floor)
{
	int secnum;

	if (floor)
	{
		auto itr = Level->GetSectorTagIterator(target);
		while ((secnum = itr.Next()) >= 0)
			Level->sectors[secnum].SetPlaneLight(sector_t::floor, llevel);
	}
	else
	{
		auto itr = Level->GetSectorTagIterator(target);
		while ((secnum = itr.Next()) >= 0)
			Level->sectors[secnum].SetPlaneLight(sector_t::ceiling, llevel);
	}
}


IMPLEMENT_CLASS(DWallLightTransfer, false, false)

void DWallLightTransfer::Serialize(FSerializer &arc)
{
	Super::Serialize (arc);
	arc("lastlight", LastLight)
		("source", Source)
		("targetid", TargetID)
		("flags", Flags);
}

void DWallLightTransfer::Construct(sector_t *srcSec, int target, uint8_t flags)
{
	int linenum;
	int wallflags;

	Source = srcSec;
	TargetID = target;
	Flags = flags;
	DoTransfer (LastLight = srcSec->GetLightLevel(), target, Flags);

	if (!(flags & WLF_NOFAKECONTRAST))
	{
		wallflags = WALLF_ABSLIGHTING;
	}
	else
	{
		wallflags = WALLF_ABSLIGHTING | WALLF_NOFAKECONTRAST;
	}

	auto itr = Level->GetLineIdIterator(target);
	while ((linenum = itr.Next()) >= 0)
	{
		if (flags & WLF_SIDE1 && Level->lines[linenum].sidedef[0] != NULL)
		{
			Level->lines[linenum].sidedef[0]->Flags |= wallflags;
		}

		if (flags & WLF_SIDE2 && Level->lines[linenum].sidedef[1] != NULL)
		{
			Level->lines[linenum].sidedef[1]->Flags |= wallflags;
		}
	}
}

void DWallLightTransfer::Tick ()
{
	short light = sector_t::ClampLight(Source->lightlevel);

	if (light != LastLight)
	{
		LastLight = light;
		DoTransfer (light, TargetID, Flags);
	}
}

void DWallLightTransfer::DoTransfer (short lightlevel, int target, uint8_t flags)
{
	int linenum;

	auto itr = Level->GetLineIdIterator(target);
	while ((linenum = itr.Next()) >= 0)
	{
		line_t *line = &Level->lines[linenum];

		if (flags & WLF_SIDE1 && line->sidedef[0] != NULL)
		{
			line->sidedef[0]->SetLight(lightlevel);
		}

		if (flags & WLF_SIDE2 && line->sidedef[1] != NULL)
		{
			line->sidedef[1]->SetLight(lightlevel);
		}
	}
}

