/*
** a_skies.cpp
** Skybox-related actors
**
**---------------------------------------------------------------------------
** Copyright 1998-2006 Randy Heit
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

#include "actor.h"
#include "a_sharedglobal.h"
#include "p_local.h"
#include "p_lnspec.h"
#include "farchive.h"
#include "r_sky.h"
#include "portal.h"

// arg0 = Visibility*4 for this skybox

IMPLEMENT_CLASS (ASkyViewpoint)

// If this actor has no TID, make it the default sky box
void ASkyViewpoint::BeginPlay ()
{
	Super::BeginPlay ();

	if (tid == 0 && sectorPortals[0].mSkybox == nullptr)
	{
		sectorPortals[0].mSkybox = this;
		sectorPortals[0].mDestination = Sector;
	}
}

void ASkyViewpoint::Destroy ()
{
	// remove all sector references to ourselves.
	for (auto &s : sectorPortals)
	{
		if (s.mSkybox == this)
		{
			s.mSkybox = 0;
			// This is necessary to entirely disable EE-style skyboxes
			// if their viewpoint gets deleted.
			s.mFlags |= PORTSF_SKYFLATONLY;
		}
	}

	Super::Destroy();
}

IMPLEMENT_CLASS (ASkyCamCompat)

void ASkyCamCompat::BeginPlay()
{
	// Do not call the SkyViewpoint's super method because it would trash our setup
	AActor::BeginPlay();
}

//---------------------------------------------------------------------------

// arg0 = tid of matching SkyViewpoint
// A value of 0 means to use a regular stretched texture, in case
// there is a default SkyViewpoint in the level.
//
// arg1 = 0: set both floor and ceiling skybox
//		= 1: set only ceiling skybox
//		= 2: set only floor skybox

class ASkyPicker : public AActor
{
	DECLARE_CLASS (ASkyPicker, AActor)
public:
	void PostBeginPlay ();
};

IMPLEMENT_CLASS (ASkyPicker)

void ASkyPicker::PostBeginPlay ()
{
	ASkyViewpoint *box;
	Super::PostBeginPlay ();

	if (args[0] == 0)
	{
		box = NULL;
	}
	else
	{
		TActorIterator<ASkyViewpoint> iterator (args[0]);
		box = iterator.Next ();
	}

	if (box == NULL && args[0] != 0)
	{
		Printf ("Can't find SkyViewpoint %d for sector %td\n", args[0], Sector - sectors);
	}
	else
	{
		int boxindex = P_GetSkyboxPortal(box);
		// Do not override special portal types, only regular skies.
		if (0 == (args[1] & 2))
		{
			if (Sector->GetPortalType(sector_t::ceiling) == PORTS_SKYVIEWPOINT)
				Sector->Portals[sector_t::ceiling] = boxindex;
		}
		if (0 == (args[1] & 1))
		{
			if (Sector->GetPortalType(sector_t::floor) == PORTS_SKYVIEWPOINT)
				Sector->Portals[sector_t::floor] = boxindex;
		}
	}
	Destroy ();
}

//---------------------------------------------------------------------------
// Stacked sectors.

// arg0 = opacity of plane; 0 = invisible, 255 = fully opaque

IMPLEMENT_CLASS (AStackPoint)

void AStackPoint::BeginPlay ()
{
	// Skip SkyViewpoint's initialization
	AActor::BeginPlay ();
}

//---------------------------------------------------------------------------

class ASectorSilencer : public AActor
{
	DECLARE_CLASS (ASectorSilencer, AActor)
public:
	void BeginPlay ();
	void Destroy ();
};

IMPLEMENT_CLASS (ASectorSilencer)

void ASectorSilencer::BeginPlay ()
{
	Super::BeginPlay ();
	Sector->Flags |= SECF_SILENT;
}

void ASectorSilencer::Destroy ()
{
	Sector->Flags &= ~SECF_SILENT;
	Super::Destroy ();
}

class ASectorFlagSetter : public AActor
{
	DECLARE_CLASS (ASectorFlagSetter, AActor)
public:
	void BeginPlay ();
};

IMPLEMENT_CLASS (ASectorFlagSetter)

void ASectorFlagSetter::BeginPlay ()
{
	Super::BeginPlay ();
	Sector->Flags |= args[0];
}

