// The barrel of green goop ------------------------------------------------

class ExplosiveBarrel : Actor
{
	Default
	{
		Health 20;
		Radius 10;
		Height 42;
		+SOLID
		+SHOOTABLE
		+NOBLOOD
		+ACTIVATEMCROSS
		+DONTGIB
		+NOICEDEATH
		+OLDRADIUSDMG
		DeathSound "world/barrelx";
		Obituary "$OB_BARREL";
	}
	States
	{
	Spawn:
		BAR1 AB 6;
		Loop;
	Death:
		BEXP A 5 BRIGHT;
		BEXP B 5 BRIGHT A_Scream;
		BEXP C 5 BRIGHT;
		BEXP D 10 BRIGHT A_Explode;
		BEXP E 10 BRIGHT;
		TNT1 A 1050 BRIGHT A_BarrelDestroy;
		TNT1 A 5 A_Respawn;
		Wait;
	}
}

extend class Actor
{
	void A_BarrelDestroy()
	{
		if (sv_barrelrespawn)
		{
			Height = Default.Height;
			bInvisible = true;
			bSolid = false;
		}
		else
		{
			Destroy();
		}
	}
}

// Bullet puff -------------------------------------------------------------

class BulletPuff : Actor
{
	Default
	{
		+NOBLOCKMAP
		+NOGRAVITY
		+ALLOWPARTICLES
		+RANDOMIZE
		+ZDOOMTRANS
		RenderStyle "Translucent";
		Alpha 0.5;
		VSpeed 1;
		Mass 5;
	}
	States
	{
	Spawn:
		PUFF A 4 Bright;
		PUFF B 4;
	Melee:
		PUFF CD 4;
		Stop;
	}
}
	
// Container for an unused state -------------------------------------------

/* Doom defined the states S_STALAG, S_DEADTORSO, and S_DEADBOTTOM but never
 * actually used them. For compatibility with DeHackEd patches, they still
 * need to be kept around. This class serves that purpose.
 */

class DoomUnusedStates : Actor
{
	States
	{
	Label1:
		SMT2 A -1;
		stop;
	Label2:
		PLAY N -1;
		stop;
		PLAY S -1;
		stop;
	TNT: // MBF compatibility
		TNT1 A -1;
		Loop;
	}
}

// MBF Beta emulation items

class EvilSceptre : ScoreItem
{
	Default
	{
		Inventory.PickupMessage "$BETA_BONUS3";
	}
	States
	{
	Spawn:
		BON3 A 6;
		Loop;
	}
}

class UnholyBible : ScoreItem
{
	Default
	{
		Inventory.PickupMessage "$BETA_BONUS4";
	}
	States
	{
	Spawn:
		BON4 A 6;
		Loop;
	}
}
