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

// arg0 = Visibility*4 for this skybox

IMPLEMENT_POINTY_CLASS (ASkyViewpoint)
	DECLARE_POINTER(Mate)
END_POINTERS

// If this actor has no TID, make it the default sky box
void ASkyViewpoint::BeginPlay ()
{
	Super::BeginPlay ();

	if (tid == 0 && level.DefaultSkybox == NULL)
	{
		level.DefaultSkybox = this;
	}
}

void ASkyViewpoint::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	arc << bInSkybox << bAlways << Mate;
}

void ASkyViewpoint::Destroy ()
{
	// remove all sector references to ourselves.
	for (int i = 0; i <numsectors; i++)
	{
		if (sectors[i].SkyBoxes[sector_t::floor] == this)
		{
			sectors[i].SkyBoxes[sector_t::floor] = NULL;
		}
		if (sectors[i].SkyBoxes[sector_t::ceiling] == this)
		{
			sectors[i].SkyBoxes[sector_t::ceiling] = NULL;
		}
	}
	if (level.DefaultSkybox == this)
	{
		level.DefaultSkybox = NULL;
	}
	Super::Destroy();
}

IMPLEMENT_CLASS (ASkyCamCompat)


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
		Printf ("Can't find SkyViewpoint %d for sector %td\n",
			args[0], Sector - sectors);
	}
	else
	{
		if (0 == (args[1] & 2))
		{
			Sector->SkyBoxes[sector_t::ceiling] = box;
			if (box == NULL) Sector->MoreFlags |= SECF_NOCEILINGSKYBOX;	// sector should ignore the level's default skybox
		}
		if (0 == (args[1] & 1))
		{
			Sector->SkyBoxes[sector_t::floor] = box;
			if (box == NULL) Sector->MoreFlags |= SECF_NOFLOORSKYBOX;	// sector should ignore the level's default skybox
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

	bAlways = true;
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

