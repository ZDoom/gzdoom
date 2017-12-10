
class SectorAction : Actor
{
	// self class uses health to define the activation type.
	enum EActivation
	{
		SECSPAC_Enter		= 1,
		SECSPAC_Exit		= 2,
		SECSPAC_HitFloor	= 4,
		SECSPAC_HitCeiling	= 8,
		SECSPAC_Use			= 16,
		SECSPAC_UseWall		= 32,
		SECSPAC_EyesDive	= 64,
		SECSPAC_EyesSurface = 128,
		SECSPAC_EyesBelowC	= 256,
		SECSPAC_EyesAboveC	= 512,
		SECSPAC_HitFakeFloor= 1024,
	};

	default
	{
		+NOBLOCKMAP
		+NOSECTOR
		+NOGRAVITY
		+DONTSPLASH
	}
	
	override void OnDestroy ()
	{
		if (CurSector != null)
		{
			// Remove ourself from self CurSector's list of actions
			if (CurSector.SecActTarget == self)
			{
				CurSector.SecActTarget = SectorAction(tracer);
			}
			else
			{
				Actor probe = CurSector.SecActTarget;
				if (null != probe)
				{
					while (probe.tracer != self && probe.tracer != null)
					{
						probe = probe.tracer;
					}
					if (probe.tracer == self)
					{
						probe.tracer = tracer;
					}
				}
			}
		}
		Super.OnDestroy();
	}

	override void BeginPlay ()
	{
		Super.BeginPlay ();

		// Add ourself to self CurSector's list of actions
		tracer = CurSector.SecActTarget;
		CurSector.SecActTarget = self;
	}

	override void Activate (Actor source)
	{
		bDormant = false;	// Projectiles cannot trigger
	}

	override void Deactivate (Actor source)
	{
		bDormant = true;	// Projectiles can trigger
	}

	virtual bool TriggerAction (Actor triggerer, int activationType)
	{
		if ((activationType & health) && CanTrigger(triggerer))
		{
			return triggerer.A_CallSpecial(special, args[0], args[1], args[2], args[3], args[4]);
		}
		return false;
	}

	virtual bool CanTrigger (Actor triggerer)
	{
		return special &&
			((triggerer.player && !bFriendly) ||
			(bAmbush && triggerer.bActivateMCross) ||
			(bDormant && triggerer.bActivatePCross));
	}
}

// Triggered when entering CurSector -------------------------------------------

class SecActEnter : SectorAction
{
	Default
	{
		Health SECSPAC_Enter;
	}
}

// Triggered when leaving CurSector --------------------------------------------

class SecActExit : SectorAction
{
	Default
	{
		Health SECSPAC_Exit;
	}
}

// Triggered when hitting CurSector's floor ------------------------------------

class SecActHitFloor : SectorAction
{
	Default
	{
		Health SECSPAC_HitFloor;
	}
}

// Triggered when hitting CurSector's ceiling ----------------------------------

class SecActHitCeil : SectorAction
{
	Default
	{
		Health SECSPAC_HitCeiling;
	}
}

// Triggered when using inside CurSector ---------------------------------------

class SecActUse : SectorAction
{
	Default
	{
		Health SECSPAC_Use;
	}
}

// Triggered when using a CurSector's wall -------------------------------------

class SecActUseWall : SectorAction
{
	Default
	{
		Health SECSPAC_UseWall;
	}
}

// Triggered when eyes go below fake floor ----------------------------------

class SecActEyesDive : SectorAction
{
	Default
	{
		Health SECSPAC_EyesDive;
	}
}

// Triggered when eyes go above fake floor ----------------------------------

class SecActEyesSurface : SectorAction
{
	Default
	{
		Health SECSPAC_EyesSurface;
	}
}

// Triggered when eyes go below fake ceiling ----------------------------------

class SecActEyesBelowC : SectorAction
{
	Default
	{
		Health SECSPAC_EyesBelowC;
	}
}

// Triggered when eyes go above fake ceiling ----------------------------------

class SecActEyesAboveC : SectorAction
{
	Default
	{
		Health SECSPAC_EyesAboveC;
	}
}

// Triggered when hitting fake floor ----------------------------------

class SecActHitFakeFloor : SectorAction
{
	Default
	{
		Health SECSPAC_HitFakeFloor;
	}
}

//==========================================================================
//
// Music changer. Uses the sector action class to do its job
//
//==========================================================================

class MusicChanger : SectorAction
{
	override bool TriggerAction (Actor triggerer, int activationType)
	{
		if (activationType & SECSPAC_Enter && triggerer.player != null)
		{
			if (triggerer.player.MUSINFOactor != self)
			{
				triggerer.player.MUSINFOactor = self;
				triggerer.player.MUSINFOtics = 30;
			}
		}
		return false;
	}
	 
	override void PostBeginPlay()
	{
		// The music changer should consider itself activated if the player
		// spawns in its sector as well as if it enters the sector during a P_TryMove.
		Super.PostBeginPlay();
		for (int i = 0; i < MAXPLAYERS; ++i)
		{
			if (playeringame[i] && players[i].mo && players[i].mo.CurSector == self.CurSector)
			{
				TriggerAction(players[i].mo, SECSPAC_Enter);
			}
		}
	}
}


