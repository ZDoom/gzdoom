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

// arg0 = Visibility*4 for this skybox

IMPLEMENT_STATELESS_ACTOR (ASkyViewpoint, Any, 9080, 0)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOSECTOR|MF_NOGRAVITY)
	PROP_Flags3 (MF3_DONTSPLASH)
END_DEFAULTS

// If this actor has no TID, make it the default sky box
void ASkyViewpoint::BeginPlay ()
{
	Super::BeginPlay ();

	if (tid == 0)
	{
		int i;

		for (i = 0; i <numsectors; i++)
		{
			if (sectors[i].FloorSkyBox == NULL)
			{
				sectors[i].FloorSkyBox = this;
			}
			if (sectors[i].CeilingSkyBox == NULL)
			{
				sectors[i].CeilingSkyBox = this;
			}
		}
	}
}

void ASkyViewpoint::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	arc << bInSkybox << bAlways << Mate << PlaneAlpha;
}

void ASkyViewpoint::Destroy ()
{
	// remove all sector references to ourselves.
	for (int i = 0; i <numsectors; i++)
	{
		if (sectors[i].FloorSkyBox == this)
		{
			sectors[i].FloorSkyBox = NULL;
		}
		if (sectors[i].CeilingSkyBox == this)
		{
			sectors[i].CeilingSkyBox = NULL;
		}
	}
	Super::Destroy();
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
	DECLARE_STATELESS_ACTOR (ASkyPicker, AActor)
public:
	void PostBeginPlay ();
};

IMPLEMENT_STATELESS_ACTOR (ASkyPicker, Any, 9081, 0)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOSECTOR|MF_NOGRAVITY)
	PROP_Flags3 (MF3_DONTSPLASH)
END_DEFAULTS

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
			Sector->CeilingSkyBox = box;
		}
		if (0 == (args[1] & 1))
		{
			Sector->FloorSkyBox = box;
		}
	}
	Destroy ();
}

//---------------------------------------------------------------------------
// Stacked sectors.

// arg0 = opacity of plane; 0 = invisible, 255 = fully opaque

class AStackPoint : public ASkyViewpoint
{
	DECLARE_STATELESS_ACTOR (AStackPoint, ASkyViewpoint)
public:
	void BeginPlay ();
};

IMPLEMENT_ABSTRACT_ACTOR (AStackPoint)

void AStackPoint::BeginPlay ()
{
	// Skip SkyViewpoint's initialization
	AActor::BeginPlay ();

	bAlways = true;
}

//---------------------------------------------------------------------------
// Upper stacks go in the top sector. Lower stacks go in the bottom sector.

class AUpperStackLookOnly : public AStackPoint
{
	DECLARE_STATELESS_ACTOR (AUpperStackLookOnly, AStackPoint)
public:
	void PostBeginPlay ();
};

class ALowerStackLookOnly : public AStackPoint
{
	DECLARE_STATELESS_ACTOR (ALowerStackLookOnly, AStackPoint)
public:
	void PostBeginPlay ();
};

IMPLEMENT_STATELESS_ACTOR (AUpperStackLookOnly, Any, 9077, 0)
END_DEFAULTS

IMPLEMENT_STATELESS_ACTOR (ALowerStackLookOnly, Any, 9078, 0)
END_DEFAULTS

void AUpperStackLookOnly::PostBeginPlay ()
{
	Super::PostBeginPlay ();
	TActorIterator<ALowerStackLookOnly> it (tid);
	Sector->FloorSkyBox = it.Next();
	if (Sector->FloorSkyBox != NULL)
	{
		Sector->FloorSkyBox->Mate = this;
		Sector->FloorSkyBox->PlaneAlpha = Scale (args[0], OPAQUE, 255);
	}
}

void ALowerStackLookOnly::PostBeginPlay ()
{
	Super::PostBeginPlay ();
	TActorIterator<AUpperStackLookOnly> it (tid);
	Sector->CeilingSkyBox = it.Next();
	if (Sector->CeilingSkyBox != NULL)
	{
		Sector->CeilingSkyBox->Mate = this;
		Sector->CeilingSkyBox->PlaneAlpha = Scale (args[0], OPAQUE, 255);
	}
}

//---------------------------------------------------------------------------

class ASectorSilencer : public AActor
{
	DECLARE_STATELESS_ACTOR (ASectorSilencer, AActor)
public:
	void BeginPlay ();
	void Destroy ();
};

IMPLEMENT_STATELESS_ACTOR (ASectorSilencer, Any, 9082, 0)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY)
	PROP_RenderStyle (STYLE_None)
END_DEFAULTS

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
