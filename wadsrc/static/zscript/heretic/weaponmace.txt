// The mace itself ----------------------------------------------------------

class Mace : HereticWeapon
{
	Default
	{
		Weapon.SelectionOrder 1400;
		Weapon.AmmoUse 1;
		Weapon.AmmoGive1 50;
		Weapon.YAdjust 15;
		Weapon.AmmoType "MaceAmmo";
		Weapon.SisterWeapon "MacePowered";
		Inventory.PickupMessage "$TXT_WPNMACE";
		Tag "$TAG_MACE";
	}

	States
	{
	Spawn:
		WMCE A -1;
		Stop;
	Ready:
		MACE A 1 A_WeaponReady;
		Loop;
	Deselect:
		MACE A 1 A_Lower;
		Loop;
	Select:
		MACE A 1 A_Raise;
		Loop;
	Fire:
		MACE B 4;
	Hold:
		MACE CDEF 3 A_FireMacePL1;
		MACE C 4 A_ReFire;
		MACE DEFB 4;
		Goto Ready;
	}
	
	//----------------------------------------------------------------------------
	//
	// PROC A_FireMacePL1
	//
	//----------------------------------------------------------------------------

	action void A_FireMacePL1()
	{
		if (player == null)
		{
			return;
		}

		Weapon weapon = player.ReadyWeapon;
		if (weapon != null)
		{
			if (!weapon.DepleteAmmo (weapon.bAltFire))
				return;
		}

		if (random[MaceAtk]() < 28)
		{
			Actor ball = Spawn("MaceFX2", Pos + (0, 0, 28 - Floorclip), ALLOW_REPLACE);
			if (ball != null)
			{
				ball.Vel.Z = 2 - clamp(tan(pitch), -5, 5);
				ball.target = self;
				ball.angle = self.angle;
				ball.AddZ(ball.Vel.Z);
				ball.VelFromAngle();
				ball.Vel += Vel.xy / 2;
				ball.A_PlaySound ("weapons/maceshoot", CHAN_BODY);
				ball.CheckMissileSpawn (radius);
			}
		}
		else
		{
			player.GetPSprite(PSP_WEAPON).x = random[MaceAtk](-2, 1);
			player.GetPSprite(PSP_WEAPON).y = WEAPONTOP + random[MaceAtk](0, 3);
			Actor ball = SpawnPlayerMissile("MaceFX1", angle + (random[MaceAtk](-4, 3) * (360. / 256)));
			if (ball)
			{
				ball.special1 = 16; // tics till dropoff
			}
		}
	}
}

class MacePowered : Mace
{
	Default
	{
		+WEAPON.POWERED_UP
		Weapon.AmmoUse 5;
		Weapon.AmmoGive 0;
		Weapon.SisterWeapon "Mace";
		Tag "$TAG_MACEP";
	}

	States
	{
	Fire:
	Hold:	
		MACE B 4;
		MACE D 4 A_FireMacePL2;
		MACE B 4;
		MACE A 8 A_ReFire;
		Goto Ready;
	}
	
	//----------------------------------------------------------------------------
	//
	// PROC A_FireMacePL2
	//
	//----------------------------------------------------------------------------

	action void A_FireMacePL2()
	{
		FTranslatedLineTarget t;

		if (player == null)
		{
			return;
		}

		Weapon weapon = player.ReadyWeapon;
		if (weapon != null)
		{
			if (!weapon.DepleteAmmo (weapon.bAltFire))
				return;
		}
		Actor mo = SpawnPlayerMissile ("MaceFX4", angle, pLineTarget:t);
		if (mo)
		{
			mo.Vel.xy += Vel.xy;
			mo.Vel.Z = 2 - clamp(tan(pitch), -5, 5);
			if (t.linetarget && !t.unlinked)
			{
				mo.tracer = t.linetarget;
			}
		}
		A_PlaySound ("weapons/maceshoot", CHAN_WEAPON);
	}
}

// Mace FX1 -----------------------------------------------------------------

class MaceFX1 : Actor
{
	const MAGIC_JUNK = 1234;
	
	Default
	{
		Radius 8;
		Height 6;
		Speed 20;
		Damage 2;
		Projectile;
		+THRUGHOST
		BounceType "HereticCompat";
		SeeSound "weapons/maceshoot";
		Obituary "$OB_MPMACE";
	}

	States
	{
	Spawn:
		FX02 AB 4 A_MacePL1Check;
		Loop;
	Death:
		FX02 F 4 BRIGHT A_MaceBallImpact;
		FX02 GHIJ 4 BRIGHT;
		Stop;
	}
	
	//----------------------------------------------------------------------------
	//
	// PROC A_MacePL1Check
	//
	//----------------------------------------------------------------------------

	void A_MacePL1Check()
	{
		if (special1 == 0) return;
		special1 -= 4;
		if (special1 > 0) return;
		special1 = 0;
		bNoGravity = false;
		Gravity = 1. / 8;
		// [RH] Avoid some precision loss by scaling the velocity directly
		double velscale = 7 / Vel.XY.Length();
		Vel.XY *= velscale;
		Vel.Z *= 0.5;
	}

	//----------------------------------------------------------------------------
	//
	// PROC A_MaceBallImpact
	//
	//----------------------------------------------------------------------------

	void A_MaceBallImpact()
	{
		if ((health != MAGIC_JUNK) && bInFloat)
		{ // Bounce
			health = MAGIC_JUNK;
			Vel.Z *= 0.75;
			bBounceOnFloors = bBounceOnCeilings = false;
			SetState (SpawnState);
			A_PlaySound ("weapons/macebounce", CHAN_BODY);
		}
		else
		{ // Explode
			Vel = (0,0,0);
			bNoGravity = true;
			Gravity = 1;
			A_PlaySound ("weapons/macehit", CHAN_BODY);
		}
	}
}

// Mace FX2 -----------------------------------------------------------------

class MaceFX2 : MaceFX1
{
	Default
	{
		Speed 10;
		Damage 6;
		Gravity 0.125;
		-NOGRAVITY
		SeeSound "";
	}

	States
	{
	Spawn:
		FX02 CD 4;
		Loop;
	Death:
		FX02 F 4 A_MaceBallImpact2;
		goto Super::Death+1;
	}
	
	//----------------------------------------------------------------------------
	//
	// PROC A_MaceBallImpact2
	//
	//----------------------------------------------------------------------------

	void A_MaceBallImpact2()
	{
		if ((pos.Z <= floorz) && HitFloor ())
		{ // Landed in some sort of liquid
			Destroy ();
			return;
		}
		if (bInFloat)
		{
			if (Vel.Z >= 2)
			{
				// Bounce
				Vel.Z *= 0.75;
				SetState (SpawnState);

				Actor tiny = Spawn("MaceFX3", Pos, ALLOW_REPLACE);
				if (tiny != null)
				{
					tiny.target = target;
					tiny.angle = angle + 90.;
					tiny.VelFromAngle(Vel.Z - 1.);
					tiny.Vel += (Vel.XY * .5, Vel.Z);
					tiny.CheckMissileSpawn (radius);
				}

				tiny = Spawn("MaceFX3", Pos, ALLOW_REPLACE);
				if (tiny != null)
				{
					tiny.target = target;
					tiny.angle = angle - 90.;
					tiny.VelFromAngle(Vel.Z - 1.);
					tiny.Vel += (Vel.XY * .5, Vel.Z);
					tiny.CheckMissileSpawn (radius);
				}
				return;
			}
		}
		Vel = (0,0,0);
		bNoGravity = true;
		bBounceOnFloors = bBounceOnCeilings = false;
		Gravity = 1;
	}
}

// Mace FX3 -----------------------------------------------------------------

class MaceFX3 : MaceFX1
{
	Default
	{
		Speed 7;
		Damage 4;
		-NOGRAVITY;
		Gravity 0.125;
	}

	States
	{
	Spawn:
		FX02 AB 4;
		Loop;
	}
}


// Mace FX4 -----------------------------------------------------------------

class MaceFX4 : Actor
{
	Default
	{
		Radius 8;
		Height 6;
		Speed 7;
		Damage 18;
		Gravity 0.125;
		Projectile;
		-NOGRAVITY
		+TELESTOMP
		+THRUGHOST
		-NOTELEPORT
		BounceType "HereticCompat";
		SeeSound "";
		Obituary "$OB_MPPMACE";
	}

	States
	{
	Spawn:
		FX02 E 99;
		Loop;
	Death:
		FX02 C 4 A_DeathBallImpact;
		FX02 GHIJ 4 BRIGHT;
		Stop;
	}
	
	//---------------------------------------------------------------------------
	//
	// FUNC P_AutoUseChaosDevice
	//
	//---------------------------------------------------------------------------

	private bool AutoUseChaosDevice (PlayerInfo player)
	{
		Inventory arti = player.mo.FindInventory("ArtiTeleport");

		if (arti != null)
		{
			player.mo.UseInventory (arti);
			player.health = player.mo.health = (player.health+1)/2;
			return true;
		}
		return false;
	}
	
	//----------------------------------------------------------------------------
	//
	// PROC DoSpecialDamage
	//
	//----------------------------------------------------------------------------

	override int DoSpecialDamage (Actor target, int damage, Name damagetype)
	{
		if (target.bBoss || target.bDontSquash || target.IsTeammate (self.target))
		{ // Don't allow cheap boss kills and don't instagib teammates
			return damage;
		}
		else if (target.player)
		{ // Player specific checks
			if (target.player.mo.bInvulnerable)
			{ // Can't hurt invulnerable players
				return -1;
			}
			if (AutoUseChaosDevice (target.player))
			{ // Player was saved using chaos device
				return -1;
			}
		}
		return TELEFRAG_DAMAGE; // Something's gonna die
	}

	//----------------------------------------------------------------------------
	//
	// PROC A_DeathBallImpact
	//
	//----------------------------------------------------------------------------

	void A_DeathBallImpact()
	{
		FTranslatedLineTarget t;

		if ((pos.Z <= floorz) && HitFloor ())
		{ // Landed in some sort of liquid
			Destroy ();
			return;
		}
		if (bInFloat)
		{
			if (Vel.Z >= 2)
			{
				// Bounce
				bool newAngle = false;
				double ang = 0;
				if (tracer)
				{
					if (!tracer.bShootable)
					{ // Target died
						tracer = null;
					}
					else
					{ // Seek
						ang = AngleTo(tracer);
						newAngle = true;
					}
				}
				else
				{ // Find new target
					ang = 0.;
					for (int i = 0; i < 16; i++)
					{
						AimLineAttack (ang, 640., t, 0., ALF_NOFRIENDS|ALF_PORTALRESTRICT, null, target);
						if (t.linetarget && target != t.linetarget)
						{
							tracer = t.linetarget;
							ang = t.angleFromSource;
							newAngle = true;
							break;
						}
						ang += 22.5;
					}
				}
				if (newAngle)
				{
					angle = ang;
					VelFromAngle();
				}
				SetState (SpawnState);
				A_PlaySound ("weapons/macestop", CHAN_BODY);
				return;
			}
		}
		Vel = (0,0,0);
		bNoGravity = true;
		Gravity = 1;
		A_PlaySound ("weapons/maceexplode", CHAN_BODY);
	}
}


// Mace spawn spot ----------------------------------------------------------

class MaceSpawner : SpecialSpot
{
	Default
	{
		+NOSECTOR
		+NOBLOCKMAP
	}

	States
	{
	Spawn:
		TNT1 A 1;
		TNT1 A -1 A_SpawnSingleItem("Mace", 64, 64, 0);
		Stop;
	}
}
