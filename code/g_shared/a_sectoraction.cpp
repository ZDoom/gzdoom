#include "r_defs.h"
#include "p_lnspec.h"

IMPLEMENT_STATELESS_ACTOR (ASectorAction, Any, -1, 0)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOSECTOR|MF_NOGRAVITY)
END_DEFAULTS

void ASectorAction::Destroy ()
{
	// Remove ourself from this sector's list of actions
	sector_t *sec = subsector->sector;
	AActor *probe = sec->SecActTarget;
	AActor **prev = reinterpret_cast<AActor **>(&sec->SecActTarget);

	while (probe && probe != this)
	{
		prev = &probe->tracer;
		probe = probe->tracer;
	}
	if (probe != NULL)
	{
		*prev = probe->tracer;
	}

	Super::Destroy ();
}

void ASectorAction::BeginPlay ()
{
	Super::BeginPlay ();

	// Add ourself to this sector's list of actions
	sector_t *sec = subsector->sector;
	tracer = sec->SecActTarget;
	sec->SecActTarget = this;
}

void ASectorAction::Activate (AActor *source)
{
	flags2 &= ~MF2_DORMANT;	// Projectiles cannot trigger
}

void ASectorAction::Deactivate (AActor *source)
{
	flags2 |= MF2_DORMANT;	// Projectiles can trigger
}

bool ASectorAction::TriggerAction (AActor *triggerer, int activationType)
{
	if (tracer != NULL)
		return static_cast<ASectorAction *>(tracer)->TriggerAction (triggerer, activationType);
	else
		return false;
}

bool ASectorAction::CheckTrigger (AActor *triggerer) const
{
	if (special &&
		(triggerer->player ||
		 ((flags & MF_AMBUSH) && (triggerer->flags2 & MF2_MCROSS)) ||
		 ((flags2 & MF2_DORMANT) && (triggerer->flags2 & MF2_PCROSS))))
	{
		return LineSpecials[special] (NULL, triggerer, args[0], args[1],
			args[2], args[3], args[4]);
	}
	return false;
}

// Triggered when entering sector

class ASecActEnter : public ASectorAction
{
	DECLARE_STATELESS_ACTOR (ASecActEnter, ASectorAction)
public:
	bool TriggerAction (AActor *triggerer, int activationType);
};

IMPLEMENT_STATELESS_ACTOR (ASecActEnter, Any, 9998, 0)
END_DEFAULTS

bool ASecActEnter::TriggerAction (AActor *triggerer, int activationType)
{
	bool didit = (activationType & SECSPAC_Enter) ? CheckTrigger (triggerer) : false;
	return didit | Super::TriggerAction (triggerer, activationType);
}

// Triggered when leaving sector

class ASecActExit : public ASectorAction
{
	DECLARE_STATELESS_ACTOR (ASecActExit, ASectorAction)
public:
	bool TriggerAction (AActor *triggerer, int activationType);
};

IMPLEMENT_STATELESS_ACTOR (ASecActExit, Any, 9997, 0)
END_DEFAULTS

bool ASecActExit::TriggerAction (AActor *triggerer, int activationType)
{
	bool didit = (activationType & SECSPAC_Exit) ? CheckTrigger (triggerer) : false;
	return didit | Super::TriggerAction (triggerer, activationType);
}

// Triggered when hitting sector's floor

class ASecActHitFloor : public ASectorAction
{
	DECLARE_STATELESS_ACTOR (ASecActHitFloor, ASectorAction)
public:
	bool TriggerAction (AActor *triggerer, int activationType);
};

// Skull Tag uses 9999 for a special that is triggered whenever
// the player is on the sector's floor. I think this is more useful.
IMPLEMENT_STATELESS_ACTOR (ASecActHitFloor, Any, 9999, 0)
END_DEFAULTS

bool ASecActHitFloor::TriggerAction (AActor *triggerer, int activationType)
{
	bool didit = (activationType & SECSPAC_HitFloor) ? CheckTrigger (triggerer) : false;
	return didit | Super::TriggerAction (triggerer, activationType);
}

// Triggered when hitting sector's ceiling

class ASecActHitCeil : public ASectorAction
{
	DECLARE_STATELESS_ACTOR (ASecActHitCeil, ASectorAction)
public:
	bool TriggerAction (AActor *triggerer, int activationType);
};

IMPLEMENT_STATELESS_ACTOR (ASecActHitCeil, Any, 9996, 0)
END_DEFAULTS

bool ASecActHitCeil::TriggerAction (AActor *triggerer, int activationType)
{
	bool didit = (activationType & SECSPAC_HitCeiling) ? CheckTrigger (triggerer) : false;
	return didit | Super::TriggerAction (triggerer, activationType);
}

// Triggered when using inside sector

class ASecActUse : public ASectorAction
{
	DECLARE_STATELESS_ACTOR (ASecActUse, ASectorAction)
public:
	bool TriggerAction (AActor *triggerer, int activationType);
};

IMPLEMENT_STATELESS_ACTOR (ASecActUse, Any, 9995, 0)
END_DEFAULTS

bool ASecActUse::TriggerAction (AActor *triggerer, int activationType)
{
	bool didit = (activationType & SECSPAC_Use) ? CheckTrigger (triggerer) : false;
	return didit | Super::TriggerAction (triggerer, activationType);
}

// Triggered when using a sector's wall

class ASecActUseWall : public ASectorAction
{
	DECLARE_STATELESS_ACTOR (ASecActUseWall, ASectorAction)
public:
	bool TriggerAction (AActor *triggerer, int activationType);
};

IMPLEMENT_STATELESS_ACTOR (ASecActUseWall, Any, 9994, 0)
END_DEFAULTS

bool ASecActUseWall::TriggerAction (AActor *triggerer, int activationType)
{
	bool didit = (activationType & SECSPAC_UseWall) ? CheckTrigger (triggerer) : false;
	return didit | Super::TriggerAction (triggerer, activationType);
}
