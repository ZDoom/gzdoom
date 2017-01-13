/*
** a_sectoraction.cpp
** Actors that hold specials to be executed upon conditions in a sector
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

#include "r_defs.h"
#include "p_lnspec.h"

// The base class for sector actions ----------------------------------------

IMPLEMENT_CLASS(ASectorAction, false, false)

ASectorAction::ASectorAction (bool activatedByUse) :
	ActivatedByUse (activatedByUse) {}

bool ASectorAction::IsActivatedByUse() const
{
	return ActivatedByUse;
}

void ASectorAction::OnDestroy ()
{
	if (Sector != nullptr)
	{
		// Remove ourself from this sector's list of actions
		AActor *probe = Sector->SecActTarget;
		union
		{
			AActor **act;
			ASectorAction **secact;
		} prev;
		prev.secact = &Sector->SecActTarget;

		while (probe && probe != this)
		{
			prev.act = &probe->tracer;
			probe = probe->tracer;
		}
		if (probe != nullptr)
		{
			*prev.act = probe->tracer;
		}
		Sector = nullptr;
	}

	Super::OnDestroy();
}

void ASectorAction::BeginPlay ()
{
	Super::BeginPlay ();

	// Add ourself to this sector's list of actions
	tracer = Sector->SecActTarget;
	Sector->SecActTarget = this;
}

void ASectorAction::Activate (AActor *source)
{
	flags2 &= ~MF2_DORMANT;	// Projectiles cannot trigger
}

void ASectorAction::Deactivate (AActor *source)
{
	flags2 |= MF2_DORMANT;	// Projectiles can trigger
}

bool ASectorAction::DoTriggerAction (AActor *triggerer, int activationType)
{
	return (activationType & health) ? CheckTrigger(triggerer) : false;
}

bool ASectorAction::CanTrigger (AActor *triggerer) const
{
	return special &&
		((triggerer->player && !(flags & MF_FRIENDLY)) ||
		((flags & MF_AMBUSH) && (triggerer->flags2 & MF2_MCROSS)) ||
		((flags2 & MF2_DORMANT) && (triggerer->flags2 & MF2_PCROSS)));
}

bool ASectorAction::CheckTrigger (AActor *triggerer) const
{
	if (CanTrigger(triggerer))
	{
		bool res = !!P_ExecuteSpecial(special, NULL, triggerer, false, args[0], args[1],
			args[2], args[3], args[4]);
		return res;
	}
	return false;
}

