
// The Mage's Wand ----------------------------------------------------------

class MWeapWand : MageWeapon
{
	Default
	{
		Weapon.SelectionOrder 3600;
		Weapon.KickBack 0;
		Weapon.YAdjust 9;
		Tag "$TAG_MWEAPWAND";
	}
	States
	{
	Select:
		MWND A 1 A_Raise;
		Loop;
	Deselect:
		MWND A 1 A_Lower;
		Loop;
	Ready:
		MWND A 1 A_WeaponReady;
		Loop;
	Fire:
		MWND A 6;
		MWND B 6 Bright Offset (0, 48) A_FireProjectile ("MageWandMissile");
		MWND A 3 Offset (0, 40);
		MWND A 3 Offset (0, 36) A_ReFire;
		Goto Ready;
	}
}

// Wand Smoke ---------------------------------------------------------------

class MageWandSmoke : Actor
{
	Default
	{
	+NOBLOCKMAP +NOGRAVITY +SHADOW
	+NOTELEPORT +CANNOTPUSH +NODAMAGETHRUST
	RenderStyle "Translucent";
	Alpha 0.6;
	}
	States
	{
	Spawn:
		MWND CDCD 4;
		Stop;
	}
}

// Wand Missile -------------------------------------------------------------

class MageWandMissile : FastProjectile
{
	Default
	{
	Speed 184;
	Radius 12;
	Height 8;
	Damage 2;
	+RIPPER +CANNOTPUSH +NODAMAGETHRUST
	+SPAWNSOUNDSOURCE
	MissileType "MageWandSmoke";
	SeeSound "MageWandFire";
	Obituary "$OB_MPMWEAPWAND";
	}
	States
	{
	Spawn:
		MWND CD 4 Bright;
		Loop;
	Death:
		MWND E 4 Bright;
		MWND F 3 Bright;
		MWND G 4 Bright;
		MWND H 3 Bright;
		MWND I 4 Bright;
		Stop;
	}
}
