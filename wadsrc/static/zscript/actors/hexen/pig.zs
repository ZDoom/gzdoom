

// Snout puff ---------------------------------------------------------------

class SnoutPuff : Actor
{
	Default
	{
		+NOBLOCKMAP
		+NOGRAVITY
		Renderstyle "Translucent";
		Alpha 0.6;
	}

	States
	{
	Spawn:
		FHFX STUVW 4;
		Stop;
	}
}


// Snout --------------------------------------------------------------------

class Snout : Weapon
{
	Default
	{
		Weapon.SelectionOrder 10000;
		+WEAPON.DONTBOB
		+WEAPON.MELEEWEAPON
		Weapon.Kickback 150;
		Weapon.YAdjust 10;
	}

	States
	{
	Ready:
		WPIG A 1 A_WeaponReady;
		Loop;
	Deselect:
		WPIG A 1 A_Lower;
		Loop;
	Select:
		WPIG A 1 A_Raise;
	Fire:
		WPIG A 4 A_SnoutAttack;
		WPIG B 8 A_SnoutAttack;
		Goto Ready;
	Grunt:
		WPIG B 8;
		Goto Ready;
	}
	
	//============================================================================
	//
	// A_SnoutAttack
	//
	//============================================================================

	action void A_SnoutAttack ()
	{
		FTranslatedLineTarget t;

		if (player == null)
		{
			return;
		}

		int damage = random[SnoutAttack](3, 6);
		double ang = angle;
		double slope = AimLineAttack(ang, DEFMELEERANGE);
		Actor puff = LineAttack(ang, DEFMELEERANGE, slope, damage, 'Melee', "SnoutPuff", true, t);
		A_PlaySound("PigActive", CHAN_VOICE);
		if(t.linetarget)
		{
			AdjustPlayerAngle(t);
			if(puff != null)
			{ // Bit something
				A_PlaySound("PigAttack", CHAN_VOICE);
			}
		}
	}
	
}


// Pig player ---------------------------------------------------------------

class PigPlayer : PlayerPawn
{
	Default
	{
		Health 30;
		ReactionTime 0;
		PainChance 255;
		Radius 16;
		Height 24;
		Speed 1;
		+WINDTHRUST
		+NOSKIN
		-PICKUP
		PainSound "PigPain";
		DeathSound "PigDeath";
		Player.JumpZ 6;
		Player.Viewheight 28;
		Player.ForwardMove 0.96, 0.98;
		Player.SideMove 0.95833333, 0.975;
		Player.SpawnClass "Pig";
		Player.SoundClass "Pig";
		Player.DisplayName "Pig";
		Player.MorphWeapon "Snout";
	}

	States
	{
	Spawn:
		PIGY A -1;
		Stop;
	See:
		PIGY ABCD 3;
		Loop;
	Pain:
		PIGY D 4 A_PigPain;
		Goto Spawn;
	Melee:
	Missile:
		PIGY A 12;
		Goto Spawn;
	Death:
		PIGY E 4 A_Scream;
		PIGY F 3 A_NoBlocking;
		PIGY G 4;
		PIGY H 3;
		PIGY IJK 4;
		PIGY L -1;
		Stop;
	Ice:
		PIGY M 5 A_FreezeDeath;
		PIGY M 1 A_FreezeDeathChunks;
		Wait;
	}
	

	override void MorphPlayerThink ()
	{
		if (player.morphTics & 15)
		{
			return;
		}
		if(Vel.X == 0 && Vel.Y == 0 && random[PigPlayerThink]() < 64)
		{ // Snout sniff
			if (player.ReadyWeapon != null)
			{
				player.SetPsprite(PSP_WEAPON, player.ReadyWeapon.FindState('Grunt'));
			}
			A_PlaySound ("PigActive1", CHAN_VOICE); // snort
			return;
		}
		if (random[PigPlayerThink]() < 48)
		{
			A_PlaySound ("PigActive", CHAN_VOICE); // snort
		}
	}

	
}
	


// Pig (non-player) ---------------------------------------------------------

class Pig : MorphedMonster
{
	Default
	{
		Health 25;
		Painchance 128;
		Speed 10;
		Radius 12;
		Height 22;
		Mass 60;
		Monster;
		-COUNTKILL
		+WINDTHRUST
		+DONTMORPH
		SeeSound "PigActive1";
		PainSound "PigPain";
		DeathSound "PigDeath";
		ActiveSound "PigActive1";
	}

	States
	{
	Spawn:
		PIGY B 10 A_Look;
		Loop;
	See:
		PIGY ABCD 3 A_Chase;
		Loop;
	Pain:
		PIGY D 4 A_PigPain;
		Goto See;
	Melee:
		PIGY A 5 A_FaceTarget;
		PIGY A 10 A_CustomMeleeAttack(random[PigAttack](2,3), "PigAttack");
		Goto See;
	Death:
		PIGY E 4 A_Scream;
		PIGY F 3 A_NoBlocking;
		PIGY G 4 A_QueueCorpse;
		PIGY H 3;
		PIGY IJK 4;
		PIGY L -1;
		Stop;
	Ice:
		PIGY M 5 A_FreezeDeath;
		PIGY M 1 A_FreezeDeathChunks;
		Wait;
	}
}


extend class Actor
{
	//============================================================================
	//
	// A_PigPain
	//
	//============================================================================

	void A_PigPain ()
	{
		A_Pain();
		if (pos.z <= floorz)
		{
			Vel.Z = 3.5;
		}
	}
}