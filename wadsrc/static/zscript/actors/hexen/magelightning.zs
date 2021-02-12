
// The Mage's Lightning Arc of Death ----------------------------------------

class MWeapLightning : MageWeapon
{
	Default
	{
		+NOGRAVITY
		Weapon.SelectionOrder 1100;
		Weapon.AmmoUse1 5;
		Weapon.AmmoGive1 25;
		Weapon.KickBack 0;
		Weapon.YAdjust 20;
		Weapon.AmmoType1 "Mana2";
		Inventory.PickupMessage "$TXT_WEAPON_M3";
		Tag "$TAG_MWEAPLIGHTNING";
	}

	States
	{
	Spawn:
		WMLG ABCDEFGH 4 Bright;
		Loop;
	Select:
		MLNG A 1 Bright A_Raise;
		Loop;
	Deselect:
		MLNG A 1 Bright A_Lower;
		Loop;
	Ready:
		MLNG AAAAA 1 Bright A_WeaponReady;
		MLNG A 1 Bright A_LightningReady;
		MLNG BBBBBB 1 Bright A_WeaponReady;
		MLNG CCCCC 1 Bright A_WeaponReady;
		MLNG C 1 Bright A_LightningReady;
		MLNG BBBBBB 1 Bright A_WeaponReady;
		Loop;
	Fire:
		MLNG DE 3 Bright;
		MLNG F 4 Bright A_MLightningAttack;
		MLNG G 4 Bright;
		MLNG HI 3 Bright;
		MLNG I 6 Bright Offset (0, 199);
		MLNG C 2 Bright Offset (0, 55);
		MLNG B 2 Bright Offset (0, 50);
		MLNG B 2 Bright Offset (0, 45);
		MLNG B 2 Bright Offset (0, 40);
		Goto Ready;
	}
	
	//============================================================================
	//
	// A_LightningReady
	//
	//============================================================================

	action void A_LightningReady()
	{
		A_WeaponReady();
		if (random[LightningReady]() < 160)
		{
			A_StartSound ("MageLightningReady", CHAN_WEAPON);
		}
	}

	//============================================================================
	//
	// A_MLightningAttack
	//
	//============================================================================

	action void A_MLightningAttack(class<Actor> floor = "LightningFloor", class<Actor> ceiling = "LightningCeiling")
	{
		LightningFloor fmo = LightningFloor(SpawnPlayerMissile (floor));
		LightningCeiling cmo = LightningCeiling(SpawnPlayerMissile (ceiling));
		if (fmo)
		{
			fmo.special1 = 0;
			fmo.lastenemy = cmo;
			fmo.A_LightningZap();
		}
		if (cmo)
		{
			cmo.tracer = NULL;
			cmo.lastenemy = fmo;
			cmo.A_LightningZap();
		}
		A_StartSound ("MageLightningFire", CHAN_BODY);

		if (player != NULL)
		{
			Weapon weapon = player.ReadyWeapon;
			if (weapon != NULL)
			{
				weapon.DepleteAmmo (weapon.bAltFire);
			}
		}
	}

	
}

// Ceiling Lightning --------------------------------------------------------

class Lightning : Actor
{
	Default
	{
		MissileType "LightningZap";
		AttackSound "MageLightningZap";
		ActiveSound "MageLightningContinuous";
		Obituary "$OB_MPMWEAPLIGHTNING";
	}
	
	override int SpecialMissileHit (Actor thing)
	{
		if (thing.bShootable && thing != target)
		{
			if (thing.Mass < LARGE_MASS)
			{
				thing.Vel.X += Vel.X / 16;
				thing.Vel.Y += Vel.Y / 16;
			}
			if ((!thing.player && !thing.bBoss) || !(Level.maptime & 1))
			{
				thing.DamageMobj(self, target, 3, 'Electric');
				A_StartSound(AttackSound, CHAN_WEAPON, CHANF_NOSTOP, 1);
				if (thing.bIsMonster && random[LightningHit]() < 64)
				{
					thing.Howl ();
				}
			}
			health--;
			if (health <= 0 || thing.health <= 0)
			{
				return 0;
			}
			if (bFloorHugger)
			{
				if (lastenemy && ! lastenemy.tracer)
				{
					lastenemy.tracer = thing;
				}
			}
			else if (!tracer)
			{
				tracer = thing;
			}
		}
		return 1; // lightning zaps through all sprites
	}
	
}

class LightningCeiling : Lightning
{
	const ZAGSPEED = 1;

	Default
	{
		Health 144;
		Speed 25;
		Radius 16;
		Height 40;
		Damage 8;
		Projectile;
		+CEILINGHUGGER
		+ZDOOMTRANS
		RenderStyle "Add";
	}

	States
	{
	Spawn:
		MLFX A 2 Bright A_LightningZap;
		MLFX BCD 2 Bright A_LightningClip;
		Loop;
	Death:
		MLF2 A 2 Bright A_LightningRemove;
		MLF2 BCDEKLM 3 Bright;
		ACLO E 35;
		MLF2 NO 3 Bright;
		MLF2 P 4 Bright;
		MLF2 QP 3 Bright;
		MLF2 Q 4 Bright;
		MLF2 P 3 Bright;
		MLF2 O 3 Bright;
		MLF2 P 3 Bright;
		MLF2 P 1 Bright A_HideThing;
		ACLO E 1050;
		Stop;
	}
	
	//============================================================================
	//
	// A_LightningClip
	//
	//============================================================================

	void A_LightningClip()
	{
		Actor cMo;
		Actor target = NULL;
		int zigZag;

		if (bFloorHugger)
		{
			if (lastenemy == NULL)
			{
				return;
			}
			SetZ(floorz);
			target = lastenemy.tracer;
		}
		else if (bCeilingHugger)
		{
			SetZ(ceilingz - Height);
			target = tracer;
		}
		if (bFloorHugger)
		{ // floor lightning zig-zags, and forces the ceiling lightning to mimic
			cMo = lastenemy;
			zigZag = random[LightningClip]();
			if((zigZag > 128 && special1 < 2) || special1 < -2)
			{
				Thrust(ZAGSPEED, angle + 90);
				if(cMo)
				{
					cMo.Thrust(ZAGSPEED, angle + 90);
				}
				special1++;
			}
			else
			{
				Thrust(ZAGSPEED,angle - 90);
				if(cMo)
				{
					cMo.Thrust(ZAGSPEED, angle - 90);
				}
				special1--;
			}
		}
		if(target)
		{
			if(target.health <= 0)
			{
				ExplodeMissile();
			}
			else
			{
				angle = AngleTo(target);
				VelFromAngle(Speed / 2);
			}
		}
	}


	//============================================================================
	//
	// A_LightningZap
	//
	//============================================================================

	void A_LightningZap()
	{
		Class<Actor> lightning = MissileName;
		if (lightning == NULL) lightning = "LightningZap";

		A_LightningClip();

		health -= 8;
		if (health <= 0)
		{
			SetStateLabel ("Death");
			return;
		}
		double deltaX = (random[LightningZap]() - 128) * radius / 256;
		double deltaY = (random[LightningZap]() - 128) * radius / 256;
		double deltaZ = (bFloorHugger) ? 10 : -10;

		Actor mo = Spawn(lightning, Vec3Offset(deltaX, deltaY, deltaZ), ALLOW_REPLACE);
		if (mo)
		{
			mo.lastenemy = self;
			mo.Vel.X = Vel.X;
			mo.Vel.Y = Vel.Y;
			mo.Vel.Z = (bFloorHugger) ? 20 : -20;
			mo.target = target;
		}
		if (bFloorHugger && random[LightningZap]() < 160)
		{
			A_StartSound (ActiveSound, CHAN_BODY);
		}
	}

	//============================================================================
	//
	// A_LightningRemove
	//
	//============================================================================

	void A_LightningRemove()
	{
		Actor mo = lastenemy;
		if (mo)
		{
			bNoTarget = true;	// tell A_ZapMimic that we are dead. The original code did a state pointer compare which is not safe.
			mo.lastenemy = NULL;
			mo.ExplodeMissile ();
		}
	}
	
}

// Floor Lightning ----------------------------------------------------------

class LightningFloor : LightningCeiling
{
	Default
	{
		-CEILINGHUGGER
		+FLOORHUGGER
		+ZDOOMTRANS
		RenderStyle "Add";
	}

	States
	{
	Spawn:
		MLFX E 2 Bright A_LightningZap;
		MLFX FGH 2 Bright A_LightningClip;
		Loop;
	Death:
		MLF2 F 2 Bright A_LightningRemove;
		MLF2 GHIJKLM 3 Bright;
		ACLO E 20;
		MLF2 NO 3 Bright;
		MLF2 P 4 Bright;
		MLF2 QP 3 Bright;
		MLF2 Q 4 Bright A_LastZap;
		MLF2 POP 3 Bright;
		MLF2 P 1 Bright A_HideThing;
		Goto Super::Death + 19;
	}
	
	//============================================================================
	//
	// A_LastZap
	//
	//============================================================================

	void A_LastZap()
	{
		Class<Actor> lightning = MissileName;
		if (lightning == NULL) lightning = "LightningZap";
		
		Actor mo = Spawn(lightning, self.Pos, ALLOW_REPLACE);
		if (mo)
		{
			mo.SetStateLabel ("Death");
			mo.Vel.Z = 40;
			mo.SetDamage(0);
		}
	}
}

// Lightning Zap ------------------------------------------------------------

class LightningZap : Actor
{
	Default
	{
		Radius 15;
		Height 35;
		Damage 2;
		Projectile;
		-ACTIVATEIMPACT
		-ACTIVATEPCROSS
		+ZDOOMTRANS
		RenderStyle "Add";
		Obituary "$OB_MPMWEAPLIGHTNING";
	}

	States
	{
	Spawn:
		MLFX IJKLM 2 Bright A_ZapMimic;
		Loop;
	Death:
		MLFX NOPQRSTU 2 Bright;
		Stop;
	}
	
	override int SpecialMissileHit (Actor thing)
	{
		Actor lmo;

		if (thing.bShootable && thing != target)
		{			
			lmo = lastenemy;
			if (lmo)
			{
				if (lmo.bFloorHugger)
				{
					if (lmo.lastenemy && !lmo.lastenemy.tracer)
					{
						lmo.lastenemy.tracer = thing;
					}
				}
				else if (!lmo.tracer)
				{
					lmo.tracer = thing;
				}
				if (!(Level.maptime&3))
				{
					lmo.health--;
				}
			}
		}
		return -1;
	}
	
	//============================================================================
	//
	// A_ZapMimic
	//
	//============================================================================

	void A_ZapMimic()
	{
		Actor mo = lastenemy;
		if (mo)
		{
			if (mo.bNoTarget)
			{
				ExplodeMissile ();
			}
			else
			{
				Vel.X = mo.Vel.X;
				Vel.Y = mo.Vel.Y;
			}
		}
	}

	
}
