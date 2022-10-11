
// Poison Bag (Flechette used by Cleric) ------------------------------------

class PoisonBag : Actor
{
	Default
	{
		Radius 5;
		Height 5;
		+NOBLOCKMAP +NOGRAVITY
	}

	States
	{
	Spawn:
		PSBG A 18 Bright;
		PSBG B 4 Bright;
		PSBG C 3;
		PSBG C 1 A_PoisonBagInit;
		Stop;
	}
	
	//===========================================================================
	//
	// A_PoisonBagInit
	//
	//===========================================================================

	void A_PoisonBagInit()
	{
		Actor mo = Spawn("PoisonCloud", pos + (0, 0, 28), ALLOW_REPLACE);
		if (mo)
		{
			mo.target = target;
		}
	}
}

// Fire Bomb (Flechette used by Mage) ---------------------------------------

class FireBomb : Actor
{
	Default
	{
		DamageType "Fire";
		+NOGRAVITY
		+FOILINVUL
		RenderStyle "Translucent";
		Alpha 0.6;
		DeathSound "FlechetteExplode";
	}

	States
	{
	Spawn:
		PSBG A 20;
		PSBG AA 10;
		PSBG B 4;
		PSBG C 4 A_Scream;
		XPL1 A 4 Bright A_TimeBomb;
		XPL1 BCDEF 4 Bright;
		Stop;
	}

	void A_TimeBomb()
	{
		AddZ(32, false);
		A_SetRenderStyle(1., STYLE_Add);
		A_Explode();
	}
}

// Throwing Bomb (Flechette used by Fighter) --------------------------------

class ThrowingBomb : Actor
{
	Default
	{
		Health 48;
		Speed 12;
		Radius 8;
		Height 10;
		DamageType "Fire";
		+NOBLOCKMAP +DROPOFF +MISSILE
		BounceType "HexenCompat";
		SeeSound "FlechetteBounce";
		DeathSound "FlechetteExplode";
	}

	States
	{
	Spawn:
		THRW A 4 A_CheckThrowBomb;
		THRW BCDE 3 A_CheckThrowBomb;
		THRW F 3 A_CheckThrowBomb2;
		Loop;
	Tail:
		THRW G 6 A_CheckThrowBomb;
		THRW F 4 A_CheckThrowBomb;
		THRW H 6 A_CheckThrowBomb;
		THRW F 4 A_CheckThrowBomb;
		THRW G 6 A_CheckThrowBomb;
		THRW F 3 A_CheckThrowBomb;
		Wait;
	Death:
		CFCF Q 4 Bright A_NoGravity;
		CFCF R 3 Bright A_Scream;
		CFCF S 4 Bright A_Explode;
		CFCF T 3 Bright;
		CFCF U 4 Bright;
		CFCF W 3 Bright;
		CFCF X 4 Bright;
		CFCF Z 3 Bright;
		Stop;
	}
	
	//===========================================================================
	//
	// A_CheckThrowBomb
	//
	//===========================================================================

	void A_CheckThrowBomb()
	{
		if (--health <= 0)
		{
			SetStateLabel("Death");
		}
	}

	//===========================================================================
	//
	// A_CheckThrowBomb2
	//
	//===========================================================================

	void A_CheckThrowBomb2()
	{
		// [RH] Check using actual velocity, although the vel.z < 2 check still stands
		if (Vel.Z < 2 && Vel.Length() < 1.5)
		{
			SetStateLabel("Tail");
			SetZ(floorz);
			Vel.Z = 0;
			ClearBounce();
			bMissile = false;
		}
		A_CheckThrowBomb();
	}
	
}

// Poison Bag Artifact (Flechette) ------------------------------------------

class ArtiPoisonBag : Inventory
{
	Default
	{
		+FLOATBOB
		Inventory.DefMaxAmount;
		Inventory.PickupFlash "PickupFlash";
		+INVENTORY.INVBAR +INVENTORY.FANCYPICKUPSOUND
		Inventory.Icon "ARTIPSBG";
		Inventory.PickupSound "misc/p_pkup";
		Inventory.PickupMessage "$TXT_ARTIPOISONBAG";
		Tag "$TAG_ARTIPOISONBAG";
	}
	States
	{
	Spawn:
		PSBG A -1;
		Stop;
	}
	
	//============================================================================
	//
	// AArtiPoisonBag :: BeginPlay
	//
	//============================================================================

	override void BeginPlay ()
	{
		Super.BeginPlay ();
		// If a subclass's specific icon is not defined, let it use the base class's.
		if (!Icon.isValid())
		{
			Icon = GetDefaultByType("ArtiPoisonBag").Icon;
		}
	}

	//============================================================================
	//
	// GetFlechetteType
	//
	//============================================================================

	private class<Actor> GetFlechetteType(Actor other)
	{
		class<Actor> spawntype = null;
		PlayerPawn pp = PlayerPawn(other);
		if (pp)
		{
			spawntype = pp.FlechetteType;
		}
		if (spawntype == null)
		{
			// default fallback if nothing valid defined.
			spawntype = "ArtiPoisonBag3";
		}
		return spawntype;
	}

	//============================================================================
	//
	// AArtiPoisonBag :: HandlePickup
	//
	//============================================================================

	override bool HandlePickup (Inventory item)
	{
		// Only do special handling when picking up the base class
		if (item.GetClass() != "ArtiPoisonBag")
		{
			return Super.HandlePickup (item);
		}

		if (GetClass() == GetFlechetteType(Owner))
		{
			if (Amount < MaxAmount || sv_unlimited_pickup)
			{
				Amount += item.Amount;
				if (Amount > MaxAmount && !sv_unlimited_pickup)
				{
					Amount = MaxAmount;
				}
				item.bPickupGood = true;
			}
			return true;
		}
		return false;
	}

	//============================================================================
	//
	// AArtiPoisonBag :: CreateCopy
	//
	//============================================================================

	override Inventory CreateCopy (Actor other)
	{
		// Only the base class gets special handling
		if (GetClass() != "ArtiPoisonBag")
		{
			return Super.CreateCopy (other);
		}

		class<Actor> spawntype = GetFlechetteType(other);
		let copy = Inventory(Spawn (spawntype));
		if (copy != null)
		{
			copy.Amount = Amount;
			copy.MaxAmount = MaxAmount;
			GoAwayAndDie ();
		}
		return copy;
	}
}

// Poison Bag 1 (The Cleric's) ----------------------------------------------

class ArtiPoisonBag1 : ArtiPoisonBag
{
	Default
	{
		Inventory.Icon "ARTIPSB1";
		Tag "$TAG_ARTIPOISONBAG1";
	}
	
	override bool Use (bool pickup)
	{
		Actor mo = Spawn("PoisonBag", Owner.Vec3Offset(
			16 * cos(Owner.angle),
			24 * sin(Owner.angle),
			-Owner.Floorclip + 8), ALLOW_REPLACE);
		if (mo)
		{
			mo.target = Owner;
			return true;
		}
		return false;
	}
	
}

// Poison Bag 2 (The Mage's) ------------------------------------------------

class ArtiPoisonBag2 : ArtiPoisonBag
{
	Default
	{
		Inventory.Icon "ARTIPSB2";
		Tag "$TAG_ARTIPOISONBAG2";
	}
	
	override bool Use (bool pickup)
	{
		Actor mo = Spawn("FireBomb", Owner.Vec3Offset(
			16 * cos(Owner.angle),
			24 * sin(Owner.angle),
			-Owner.Floorclip + 8), ALLOW_REPLACE);
		if (mo)
		{
			mo.target = Owner;
			return true;
		}
		return false;
	}
	
}

// Poison Bag 3 (The Fighter's) ---------------------------------------------

class ArtiPoisonBag3 : ArtiPoisonBag
{
	Default
	{
		Inventory.Icon "ARTIPSB3";
		Tag "$TAG_ARTIPOISONBAG3";
	}
	
	override bool Use (bool pickup)
	{
		Actor mo = Spawn("ThrowingBomb", Owner.Pos + (0,0,35. - Owner.Floorclip + (Owner.player? Owner.player.crouchoffset : 0)), ALLOW_REPLACE);
		if (mo)
		{
			mo.angle = Owner.angle + (random[PoisonBag](-4, 3) * (360./256.));

			/* Original flight code from Hexen
			 * mo.momz = 4*F.RACUNIT+((player.lookdir)<<(F.RACBITS-4));
			 * mo.z += player.lookdir<<(F.RACBITS-4);
			 * P_ThrustMobj(mo, mo.ang, mo.info.speed);
			 * mo.momx += player.mo.momx>>1;
			 * mo.momy += player.mo.momy>>1;
			 */

			// When looking straight ahead, it uses a z velocity of 4 while the xy velocity
			// is as set by the projectile. To accommodate self with a proper trajectory, we
			// aim the projectile ~20 degrees higher than we're looking at and increase the
			// speed we fire at accordingly.
			double modpitch = clamp(-Owner.Pitch + 20, -89., 89.);
			double ang = mo.angle;
			double speed = (mo.Speed, 4.).Length();
			double xyscale = speed * cos(modpitch);

			mo.Vel.Z = speed * sin(modpitch);
			mo.Vel.X = xyscale * cos(ang) + Owner.Vel.X / 2;
			mo.Vel.Y = xyscale * sin(ang) + Owner.Vel.Y / 2;
			mo.AddZ(mo.Speed * sin(modpitch));

			mo.target = Owner;
			mo.tics -= random[PoisonBag](0, 3);
			mo.CheckMissileSpawn(Owner.radius);
			return true;
		}
		return false;
	}
	
	
}

// Poison Bag 4 (Custom Giver) ----------------------------------------------

class ArtiPoisonBagGiver : ArtiPoisonBag
{
	Default
	{
		Inventory.Icon "ARTIPSB4";
	}
	
	override bool Use (bool pickup)
	{
		Class<Actor> missiletype = MissileName;
		if (missiletype != null)
		{
			Actor mo = Spawn (missiletype, Owner.Pos, ALLOW_REPLACE);
			if (mo != null)
			{
				Inventory inv = Inventory(mo);
				if (inv && inv.CallTryPickup(Owner)) return true;
				mo.Destroy();	// Destroy if not inventory or couldn't be picked up
			}
		}
		return false;
	}
	
}

// Poison Bag 5 (Custom Thrower) --------------------------------------------

class ArtiPoisonBagShooter : ArtiPoisonBag
{
	Default
	{
		Inventory.Icon "ARTIPSB5";
	}
	
	override bool Use (bool pickup)
	{
		Class<Actor> missiletype = MissileName;
		if (missiletype != null)
		{
			Actor mo = Owner.SpawnPlayerMissile(missiletype);
			if (mo != null)
			{
				// automatic handling of seeker missiles
				if (mo.bSeekerMissile)
				{
					mo.tracer = Owner.target;
				}
				return true;
			}
		}
		return false;
	}
	
}

// Poison Cloud -------------------------------------------------------------

class PoisonCloud : Actor
{
	Default
	{
		Radius 20;
		Height 30;
		Mass 0x7fffffff;
		+NOBLOCKMAP +NOGRAVITY +DROPOFF
		+NODAMAGETHRUST
		+DONTSPLASH +FOILINVUL +CANBLAST +BLOODLESSIMPACT +BLOCKEDBYSOLIDACTORS +FORCEZERORADIUSDMG +OLDRADIUSDMG
		RenderStyle "Translucent";
		Alpha 0.6;
		DeathSound "PoisonShroomDeath";
		DamageType "PoisonCloud";
	}

	States
	{
	Spawn:
		PSBG D 1;
		PSBG D 1 A_Scream;
		PSBG DEEEFFFGGGHHHII 2 A_PoisonBagDamage;
		PSBG I 2 A_PoisonBagDamage;
		PSBG I 1 A_PoisonBagCheck;
		Goto Spawn + 3;
	Death:
		PSBG HG 7;
		PSBG FD 6;
		Stop;
	}
	
	//===========================================================================
	//
	// 
	//
	//===========================================================================

	override void BeginPlay ()
	{
		Vel.X = MinVel; // missile objects must move to impact other objects
		special1 = random[PoisonCloud](24, 31);
		special2 = 0;
	}
	
	//===========================================================================
	//
	// 
	//
	//===========================================================================

	override int DoSpecialDamage (Actor victim, int damage, Name damagetype)
	{
		if (victim.player)
		{
			bool mate = (target != null && victim.player != target.player && victim.IsTeammate (target));
			bool dopoison;
			
			if (!mate)
			{
				dopoison = victim.player.poisoncount < 4;
			}
			else
			{
				dopoison = victim.player.poisoncount < (int)(4. * level.teamdamage);
			}

			if (dopoison)
			{
				damage = random[PoisonCloud](15, 30);
				if (mate)
				{
					damage = (int)(damage * level.teamdamage);
				}
				// Handle passive damage modifiers (e.g. PowerProtection)
				damage = victim.GetModifiedDamage(damagetype, damage, true);
				// Modify with damage factors
				damage = victim.ApplyDamageFactor(damagetype, damage);
				if (damage > 0)
				{
					victim.player.PoisonDamage (self, random[PoisonCloud](15, 30), false); // Don't play painsound

					// If successful, play the poison sound.
					if (victim.player.PoisonPlayer (self, self.target, 50))
						victim.A_StartSound ("*poison", CHAN_VOICE);
				}
			}	
			return -1;
		}
		else if (!victim.bIsMonster)
		{ // only damage monsters/players with the poison cloud
			return -1;
		}
		return damage;
	}
	
	//===========================================================================
	//
	// A_PoisonBagCheck
	//
	//===========================================================================

	void A_PoisonBagCheck()
	{
		if (--special1 <= 0)
		{
			SetStateLabel("Death");
		}
	}

	//===========================================================================
	//
	// A_PoisonBagDamage
	//
	//===========================================================================

	void A_PoisonBagDamage()
	{
		A_Explode(4, 40);
		AddZ(BobSin(special2) / 16);
		special2 = (special2 + 1) & 63;
	}
}

// Poison Shroom ------------------------------------------------------------

class ZPoisonShroom : PoisonBag
{
	Default
	{
		Radius 6;
		Height 20;
		PainChance 255;
		Health 30;
		Mass 0x7fffffff;
		+SHOOTABLE
		+SOLID
		+NOBLOOD
		+NOICEDEATH
		-NOBLOCKMAP
		-NOGRAVITY
		PainSound "PoisonShroomPain";
		DeathSound "PoisonShroomDeath";
	}

	States
	{
	Spawn:
		SHRM A 5 A_PoisonShroom;
		Goto Pain+1;
	Pain:
		SHRM A 6;
		SHRM B 8 A_Pain;
		Goto Spawn;
	Death:
		SHRM CD 5;
		SHRM E 5 A_PoisonBagInit;
		SHRM F -1;
		Stop;
	}
	
	//===========================================================================
	//
	// A_PoisonShroom
	//
	//===========================================================================

	void A_PoisonShroom()
	{
		tics = 128 + (random[PoisonShroom]() << 1);
	}
}

