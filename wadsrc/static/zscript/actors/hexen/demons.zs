
// Demon, type 1 (green, like D'Sparil's) -----------------------------------

class Demon1 : Actor
{
	Default
	{
		Health 250;
		Painchance 50;
		Speed 13;
		Radius 32;
		Height 64;
		Mass 220;
		Monster;
		+TELESTOMP
		+FLOORCLIP
		SeeSound "DemonSight";
		AttackSound "DemonAttack";
		PainSound "DemonPain";
		DeathSound "DemonDeath";
		ActiveSound "DemonActive";
		Obituary "$OB_DEMON1";
		HitObituary "$OB_DEMON1HIT";
		Tag "$FN_DEMON1";
	}
	
	const ChunkFlags = SXF_TRANSFERTRANSLATION | SXF_ABSOLUTEVELOCITY;
	
	States
	{
	Spawn:
		DEMN AA 10 A_Look;
		Loop;
	See:
		DEMN ABCD 4 A_Chase;
		Loop;
	Pain:
		DEMN E 4;
		DEMN E 4 A_Pain;
		Goto See;
	Melee:
		DEMN E 6 A_FaceTarget;
		DEMN F 8 A_FaceTarget;
		DEMN G 6 A_CustomMeleeAttack(random[DemonAttack1](1,8)*2);
		Goto See;
	Missile:
		DEMN E 5 A_FaceTarget;
		DEMN F 6 A_FaceTarget;
		DEMN G 5 A_SpawnProjectile("Demon1FX1", 62, 0);
		Goto See;
	Death:
		DEMN HI 6;
		DEMN J 6 A_Scream;
		DEMN K 6 A_NoBlocking;
		DEMN L 6 A_QueueCorpse;
		DEMN MNO 6;
		DEMN P -1;
		Stop;
	XDeath:
		DEMN H 6;
		DEMN I 6
		{
			static const class<Actor> chunks[] = { "Demon1Chunk1", "Demon1Chunk2", "Demon1Chunk3", "Demon1Chunk4", "Demon1Chunk5" };
			for(int i = 0; i < 5; i++)
				A_SpawnItemEx(chunks[i], 0,0,45, frandom[DemonChunks](1,4.984375)*cos(Angle+90), frandom[DemonChunks](1,4.984375)*sin(Angle+90), 8, 90, ChunkFlags);
		}
		Goto Death+2;
	Ice:
		DEMN Q 5 A_FreezeDeath;
		DEMN Q 1 A_FreezeDeathChunks;
		Wait;
	}
}

// Demon, type 1, mashed ----------------------------------------------------

class Demon1Mash : Demon1
{
	Default
	{
		+NOBLOOD
		+BLASTED
		-TELESTOMP
		+NOICEDEATH
		RenderStyle "Translucent";
		Alpha 0.4;
	}
	States
	{
	Death:
	XDeath:
	Ice:
		Stop;
	}
}

// Demon chunk, base class --------------------------------------------------

class DemonChunk : Actor
{
	Default
	{
		Radius 5;
		Height 5;
		+NOBLOCKMAP
		+DROPOFF
		+MISSILE
		+CORPSE
		+FLOORCLIP
		+NOTELEPORT
	}
}

// Demon, type 1, chunk 1 ---------------------------------------------------

class Demon1Chunk1 : DemonChunk
{
	States
	{
	Spawn:
		DEMA A 4;
		DEMA A 10 A_QueueCorpse;
		DEMA A 20;
		Wait;
	Death:
		DEMA A -1;
		Stop;
	}
}

// Demon, type 1, chunk 2 ---------------------------------------------------

class Demon1Chunk2 : DemonChunk
{
	States
	{
	Spawn:
		DEMB A 4;
		DEMB A 10 A_QueueCorpse;
		DEMB A 20;
		Wait;
	Death:
		DEMB A -1;
		Stop;
	}
}

class Demon1Chunk3 : DemonChunk
{
	States
	{
	Spawn:
		DEMC A 4;
		DEMC A 10 A_QueueCorpse;
		DEMC A 20;
		Wait;
	Death:
		DEMC A -1;
		Stop;
	}
}

// Demon, type 1, chunk 4 ---------------------------------------------------

class Demon1Chunk4 : DemonChunk
{
	States
	{
	Spawn:
		DEMD A 4;
		DEMD A 10 A_QueueCorpse;
		DEMD A 20;
		Wait;
	Death:
		DEMD A -1;
		Stop;
	}
}

// Demon, type 1, chunk 5 ---------------------------------------------------

class Demon1Chunk5 : DemonChunk
{
	States
	{
	Spawn:
		DEME A 4;
		DEME A 10 A_QueueCorpse;
		DEME A 20;
		Wait;
	Death:
		DEME A -1;
		Stop;
	}
}

// Demon, type 1, projectile ------------------------------------------------

class Demon1FX1 : Actor
{
	Default
	{
		Speed 15;
		Radius 10;
		Height 6;
		Damage 5;
		DamageType "Fire";
		Projectile;
		+SPAWNSOUNDSOURCE
		+ZDOOMTRANS
		RenderStyle "Add";
		SeeSound "DemonMissileFire";
		DeathSound "DemonMissileExplode";
	}
	States
	{
	Spawn:
		DMFX ABC 4 Bright;
		Loop;
	Death:
		DMFX DE 4 Bright;
		DMFX FGH 3 Bright;
		Stop;
	}
}

// Demon, type 2 (brown) ----------------------------------------------------

class Demon2 : Demon1
{
	Default
	{
		Obituary "$OB_DEMON2";
		HitObituary "$OB_DEMON2HIT";
		Species "Demon2";
	}
	States
	{
	Spawn:
		DEM2 AA 10 A_Look;
		Loop;
	See:
		DEM2 ABCD 4 A_Chase;
		Loop;
	Pain:
		DEM2 E 4;
		DEM2 E 4 A_Pain;
		Goto See;
	Melee:
		DEM2 E 6 A_FaceTarget;
		DEM2 F 8 A_FaceTarget;
		DEM2 G 6 A_CustomMeleeAttack(random[DemonAttack1](1,8)*2);
		Goto See;
	Missile:
		DEM2 E 5 A_FaceTarget;
		DEM2 F 6 A_FaceTarget;
		DEM2 G 5 A_SpawnProjectile("Demon2FX1", 62, 0);
		Goto See;
	Death:
		DEM2 HI 6;
		DEM2 J 6 A_Scream;
		DEM2 K 6 A_NoBlocking;
		DEM2 L 6 A_QueueCorpse;
		DEM2 MNO 6;
		DEM2 P -1;
		Stop;
	XDeath:
		DEM2 H 6;
		DEM2 I 6
		{
			static const class<Actor> chunks[] = { "Demon2Chunk1", "Demon2Chunk2", "Demon2Chunk3", "Demon2Chunk4", "Demon2Chunk5" };
			for(int i = 0; i < 5; i++)
				A_SpawnItemEx(chunks[i], 0,0,45, frandom[DemonChunks](1,4.984375)*cos(Angle+90), frandom[DemonChunks](1,4.984375)*sin(Angle+90), 8, 90, ChunkFlags);
		}
		Goto Death+2;
	}
}

// Demon, type 2, mashed ----------------------------------------------------

class Demon2Mash : Demon2
{
	Default
	{
		+NOBLOOD
		+BLASTED
		-TELESTOMP
		+NOICEDEATH
		RenderStyle "Translucent";
		Alpha 0.4;
	}
	States
	{
	Death:
	XDeath:
	Ice:
		Stop;
	}
}

// Demon, type 2, chunk 1 ---------------------------------------------------

class Demon2Chunk1 : DemonChunk
{
	States
	{
	Spawn:
		DMBA A 4;
		DMBA A 10 A_QueueCorpse;
		DMBA A 20;
		Wait;
	Death:
		DMBA A -1;
		Stop;
	}
}

// Demon, type 2, chunk 2 ---------------------------------------------------

class Demon2Chunk2 : DemonChunk
{
	States
	{
	Spawn:
		DMBB A 4;
		DMBB A 10 A_QueueCorpse;
		DMBB A 20;
		Wait;
	Death:
		DMBB A -1;
		Stop;
	}
}

// Demon, type 2, chunk 3 ---------------------------------------------------

class Demon2Chunk3 : DemonChunk
{
	States
	{
	Spawn:
		DMBC A 4;
		DMBC A 10 A_QueueCorpse;
		DMBC A 20;
		Wait;
	Death:
		DMBC A -1;
		Stop;
	}
}

// Demon, type 2, chunk 4 ---------------------------------------------------

class Demon2Chunk4 : DemonChunk
{
	States
	{
	Spawn:
		DMBD A 4;
		DMBD A 10 A_QueueCorpse;
		DMBD A 20;
		Wait;
	Death:
		DMBD A -1;
		Stop;
	}
}

// Demon, type 2, chunk 5 ---------------------------------------------------

class Demon2Chunk5 : DemonChunk
{
	States
	{
	Spawn:
		DMBE A 4;
		DMBE A 10 A_QueueCorpse;
		DMBE A 20;
		Wait;
	Death:
		DMBE A -1;
		Stop;
	}
}

// Demon, type 2, projectile ------------------------------------------------

class Demon2FX1 : Actor
{
	Default
	{
		Speed 15;
		Radius 10;
		Height 6;
		Damage 5;
		DamageType "Fire";
		Projectile;
		+SPAWNSOUNDSOURCE
		+ZDOOMTRANS
		RenderStyle "Add";
		SeeSound "DemonMissileFire";
		DeathSound "DemonMissileExplode";
	}
	States
	{
	Spawn:
		D2FX ABCDEF 4 Bright;
		Loop;
	Death:
		D2FX GHIJ 4 Bright;
		D2FX KL 3 Bright;
		Stop;
	}
}

