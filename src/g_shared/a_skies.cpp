/*
** a_skies.cpp
** Skybox-related actors
**
**---------------------------------------------------------------------------
** Copyright 1998-2001 Randy Heit
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
			if (sectors[i].SkyBox == NULL)
			{
				sectors[i].SkyBox = this;
			}
		}
	}
}

//---------------------------------------------------------------------------

// arg0 = tid of matching SkyViewpoint
// A value of 0 means to use a regular stretched texture, in case
// there is a default SkyViewpoint in the level.
class ASkyPicker : public AActor
{
	DECLARE_STATELESS_ACTOR (ASkyPicker, AActor)
public:
	void PostBeginPlay ();
};

IMPLEMENT_STATELESS_ACTOR (ASkyPicker, Any, 9081, 0)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOSECTOR|MF_NOGRAVITY)
END_DEFAULTS

void ASkyPicker::PostBeginPlay ()
{
	Super::PostBeginPlay ();

	if (args[0] == 0)
	{
		Sector->SkyBox = NULL;
	}
	else
	{
		TActorIterator<ASkyViewpoint> iterator (args[0]);
		ASkyViewpoint *box = iterator.Next ();

		if (box != NULL)
		{
			Sector->SkyBox = box;
		}
		else
		{
			Printf ("Can't find SkyViewpoint %d for sector %d\n",
				args[0], Sector - sectors);
		}
	}
	Destroy ();
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
	Sector->MoreFlags |= SECF_SILENT;
}

void ASectorSilencer::Destroy ()
{
	Sector->MoreFlags &= ~SECF_SILENT;
	Super::Destroy ();
}
