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

IMPLEMENT_CLASS (ASectorAction)

ASectorAction::ASectorAction (bool activatedByUse) :
	ActivatedByUse (activatedByUse) {}

bool ASectorAction::IsActivatedByUse() const
{
	return ActivatedByUse;
}

void ASectorAction::Destroy ()
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
	if (probe != NULL)
	{
		*prev.act = probe->tracer;
	}

	Super::Destroy ();
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

bool ASectorAction::TriggerAction(AActor *triggerer, int activationType)
{
	if (DoTriggerAction(triggerer, activationType))
	{
		if (flags4 & MF4_STANDSTILL)
		{
			Destroy();
		}
	}
	return false;
}

bool ASectorAction::DoTriggerAction (AActor *triggerer, int activationType)
{
	if (tracer != NULL)
		return barrier_cast<ASectorAction *>(tracer)->DoTriggerAction (triggerer, activationType);
	else
		return false;
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

// Triggered when entering sector -------------------------------------------

class ASecActEnter : public ASectorAction
{
	DECLARE_CLASS (ASecActEnter, ASectorAction)
public:
	bool DoTriggerAction (AActor *triggerer, int activationType);
};

IMPLEMENT_CLASS (ASecActEnter)


bool ASecActEnter::DoTriggerAction (AActor *triggerer, int activationType)
{
	bool didit = (activationType & SECSPAC_Enter) ? CheckTrigger (triggerer) : false;
	return didit | Super::DoTriggerAction (triggerer, activationType);
}

// Triggered when leaving sector --------------------------------------------

class ASecActExit : public ASectorAction
{
	DECLARE_CLASS (ASecActExit, ASectorAction)
public:
	bool DoTriggerAction (AActor *triggerer, int activationType);
};

IMPLEMENT_CLASS (ASecActExit)


bool ASecActExit::DoTriggerAction (AActor *triggerer, int activationType)
{
	bool didit = (activationType & SECSPAC_Exit) ? CheckTrigger (triggerer) : false;
	return didit | Super::DoTriggerAction (triggerer, activationType);
}

// Triggered when hitting sector's floor ------------------------------------

class ASecActHitFloor : public ASectorAction
{
	DECLARE_CLASS (ASecActHitFloor, ASectorAction)
public:
	bool DoTriggerAction (AActor *triggerer, int activationType);
};

// Skull Tag uses 9999 for a special that is triggered whenever
// the player is on the sector's floor. I think this is more useful.
IMPLEMENT_CLASS (ASecActHitFloor)


bool ASecActHitFloor::DoTriggerAction (AActor *triggerer, int activationType)
{
	bool didit = (activationType & SECSPAC_HitFloor) ? CheckTrigger (triggerer) : false;
	return didit | Super::DoTriggerAction (triggerer, activationType);
}

// Triggered when hitting sector's ceiling ----------------------------------

class ASecActHitCeil : public ASectorAction
{
	DECLARE_CLASS (ASecActHitCeil, ASectorAction)
public:
	bool DoTriggerAction (AActor *triggerer, int activationType);
};

IMPLEMENT_CLASS (ASecActHitCeil)


bool ASecActHitCeil::DoTriggerAction (AActor *triggerer, int activationType)
{
	bool didit = (activationType & SECSPAC_HitCeiling) ? CheckTrigger (triggerer) : false;
	return didit | Super::DoTriggerAction (triggerer, activationType);
}

// Triggered when using inside sector ---------------------------------------

class ASecActUse : public ASectorAction
{
	DECLARE_CLASS (ASecActUse, ASectorAction)
public:
	ASecActUse() : ASectorAction (true) {}
	bool DoTriggerAction (AActor *triggerer, int activationType);
};

IMPLEMENT_CLASS (ASecActUse)


bool ASecActUse::DoTriggerAction (AActor *triggerer, int activationType)
{
	bool didit = (activationType & SECSPAC_Use) ? CheckTrigger (triggerer) : false;
	return didit | Super::DoTriggerAction (triggerer, activationType);
}

// Triggered when using a sector's wall -------------------------------------

class ASecActUseWall : public ASectorAction
{
	DECLARE_CLASS (ASecActUseWall, ASectorAction)
public:
	ASecActUseWall() : ASectorAction (true) {}
	bool DoTriggerAction (AActor *triggerer, int activationType);
};

IMPLEMENT_CLASS (ASecActUseWall)


bool ASecActUseWall::DoTriggerAction (AActor *triggerer, int activationType)
{
	bool didit = (activationType & SECSPAC_UseWall) ? CheckTrigger (triggerer) : false;
	return didit | Super::DoTriggerAction (triggerer, activationType);
}

// Triggered when eyes go below fake floor ----------------------------------

class ASecActEyesDive : public ASectorAction
{
	DECLARE_CLASS (ASecActEyesDive, ASectorAction)
public:
	bool DoTriggerAction (AActor *triggerer, int activationType);
};

IMPLEMENT_CLASS (ASecActEyesDive)


bool ASecActEyesDive::DoTriggerAction (AActor *triggerer, int activationType)
{
	bool didit = (activationType & SECSPAC_EyesDive) ? CheckTrigger (triggerer) : false;
	return didit | Super::DoTriggerAction (triggerer, activationType);
}

// Triggered when eyes go above fake floor ----------------------------------

class ASecActEyesSurface : public ASectorAction
{
	DECLARE_CLASS (ASecActEyesSurface, ASectorAction)
public:
	bool DoTriggerAction (AActor *triggerer, int activationType);
};

IMPLEMENT_CLASS (ASecActEyesSurface)


bool ASecActEyesSurface::DoTriggerAction (AActor *triggerer, int activationType)
{
	bool didit = (activationType & SECSPAC_EyesSurface) ? CheckTrigger (triggerer) : false;
	return didit | Super::DoTriggerAction (triggerer, activationType);
}

// Triggered when eyes go below fake floor ----------------------------------

class ASecActEyesBelowC : public ASectorAction
{
	DECLARE_CLASS (ASecActEyesBelowC, ASectorAction)
public:
	bool DoTriggerAction (AActor *triggerer, int activationType);
};

IMPLEMENT_CLASS (ASecActEyesBelowC)


bool ASecActEyesBelowC::DoTriggerAction (AActor *triggerer, int activationType)
{
	bool didit = (activationType & SECSPAC_EyesBelowC) ? CheckTrigger (triggerer) : false;
	return didit | Super::DoTriggerAction (triggerer, activationType);
}

// Triggered when eyes go above fake floor ----------------------------------

class ASecActEyesAboveC : public ASectorAction
{
	DECLARE_CLASS (ASecActEyesAboveC, ASectorAction)
public:
	bool DoTriggerAction (AActor *triggerer, int activationType);
};

IMPLEMENT_CLASS (ASecActEyesAboveC)


bool ASecActEyesAboveC::DoTriggerAction (AActor *triggerer, int activationType)
{
	bool didit = (activationType & SECSPAC_EyesAboveC) ? CheckTrigger (triggerer) : false;
	return didit | Super::DoTriggerAction (triggerer, activationType);
}

// Triggered when eyes go below fake floor ----------------------------------

class ASecActHitFakeFloor : public ASectorAction
{
	DECLARE_CLASS (ASecActHitFakeFloor, ASectorAction)
public:
	bool DoTriggerAction (AActor *triggerer, int activationType);
};

IMPLEMENT_CLASS (ASecActHitFakeFloor)


bool ASecActHitFakeFloor::DoTriggerAction (AActor *triggerer, int activationType)
{
	bool didit = (activationType & SECSPAC_HitFakeFloor) ? CheckTrigger (triggerer) : false;
	return didit | Super::DoTriggerAction (triggerer, activationType);
}
