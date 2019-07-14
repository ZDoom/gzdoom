
// Loremaster (aka Priest) --------------------------------------------------

class Loremaster : Actor
{
	Default
	{	
		Health 800;
		Speed 10;
		Radius 15;
		Height 56;
		FloatSpeed 5;
		Monster;
		+FLOAT
		+NOBLOOD
		+NOGRAVITY
		+NOTDMATCH
		+FLOORCLIP
		+NOBLOCKMONST
		+INCOMBAT
		+LOOKALLAROUND
		+NOICEDEATH
		+NEVERRESPAWN
		DamageFactor "Fire", 0.5;
		MinMissileChance 150;
		Tag "$TAG_PRIEST";
		SeeSound "loremaster/sight";
		AttackSound "loremaster/attack";
		PainSound "loremaster/pain";
		DeathSound "loremaster/death";
		ActiveSound "loremaster/active";
		Obituary "$OB_LOREMASTER";
		DropItem "Junk";
	}
	States
	{
	Spawn:
		PRST A 10 A_Look;
		PRST B 10 A_SentinelBob;
		Loop;
	See:
		PRST A 4 A_Chase;
		PRST A 4 A_SentinelBob;
		PRST B 4 A_Chase;
		PRST B 4 A_SentinelBob;
		PRST C 4 A_Chase;
		PRST C 4 A_SentinelBob;
		PRST D 4 A_Chase;
		PRST D 4 A_SentinelBob;
		Loop;
	Melee:
		PRST E 4 A_FaceTarget;
		PRST F 4 A_CustomMeleeAttack((random[SpectreMelee](0,255)&9)*5);
		PRST E 4 A_SentinelBob;
		Goto See;
	Missile:
		PRST E 4 A_FaceTarget;
		PRST F 4 A_SpawnProjectile("LoreShot", 32, 0);
		PRST E 4 A_SentinelBob;
		Goto See;
	Death:
		PDED A 6;
		PDED B 6 A_Scream;
		PDED C 6;
		PDED D 6 A_Fall;
		PDED E 6;
		PDED FGHIJIJIJKL 5;
		PDED MNOP 4;
		PDED Q 4 A_SpawnItemEx("AlienSpectre5", 0, 0, 0, 0, 0, random[spectrespawn](0,255)*0.0078125, 0, SXF_NOCHECKPOSITION);
		PDED RS 4;
		PDED T -1;
		Stop;
	}
}


// Loremaster Projectile ----------------------------------------------------

class LoreShot : Actor
{
	Default
	{
		Speed 20;
		Height 14;
		Radius 10;
		Projectile;
		+STRIFEDAMAGE
		Damage 2;
		MaxStepHeight 4;
		SeeSound "loremaster/chain";
		ActiveSound "loremaster/swish";
	}

	States
	{
	Spawn:
		OCLW A 2 A_LoremasterChain;
		Loop;
	Death:
		OCLW A 6;
		Stop;
	}
	
	override int DoSpecialDamage (Actor victim, int damage, Name damagetype)
	{
		
		if (victim != NULL && target != NULL && !victim.bDontThrust)
		{
			Vector3 thrust = victim.Vec3To(target);
			victim.Vel += thrust.Unit() * (255. * 50 / max(victim.Mass, 1));
		}
		return damage;
	}

	void A_LoremasterChain ()
	{
		A_PlaySound ("loremaster/active", CHAN_BODY);
		Spawn("LoreShot2", Pos, ALLOW_REPLACE);
		Spawn("LoreShot2", Vec3Offset(-Vel.x/2., -Vel.y/2., -Vel.z/2.), ALLOW_REPLACE);
		Spawn("LoreShot2", Vec3Offset(-Vel.x, -Vel.y, -Vel.z), ALLOW_REPLACE);
	}
	
}

// Loremaster Subprojectile -------------------------------------------------

class LoreShot2 : Actor
{
	Default
	{
		+NOBLOCKMAP
		+NOGRAVITY
	}
	States
	{
	Spawn:
		TEND A 20;
		Stop;
	}
}

