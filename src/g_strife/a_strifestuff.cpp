#include "actor.h"
#include "gi.h"
#include "m_random.h"
#include "s_sound.h"
#include "d_player.h"
#include "a_action.h"
#include "p_local.h"
#include "a_doomglobal.h"
#include "a_strifeglobal.h"
#include "p_enemy.h"
#include "p_lnspec.h"

// Notes so I don't forget them:
// Strife does some extra stuff in A_Explode if a player caused the explosion.
// See the instructions @ 21249.

/* These mobjinfos have been converted:

	  1 StrifePlayer

     27 MaulerPuff

	 31 Tank1
	 32 Tank2
	 33 Tank3
	 34 Tank4
	 35 Tank5
	 36 Tank6

	 51 RocketTrail
	 52 Reaver (incomplete)

	 76 EntityNest
	 77 EntityPod

	 83 Zap6

	 88 Zap5 (incomplete)

	 99 MiniMissile

    102 ElectricBolt
	103 PoisonBolt

	106 HEGrenade
	107 PhosphorousGrenade

	109 PhosphorousFire
	110 MaulerTorpedo
	111 MaulerTorpedoWave
	112 FlameMissile
	113 FastFlameMissile
	114 ElectricBoltAmmo
	115 StrifePuff
    116 StrifeSpark

	121 KlaxonWarningLight (incomplete)

	124 Computer (incomplete)

	131 WaterBottle
	132 Mug

	177 HEGrenadeRounds
	178 PhosphorusGrenadeRounds
	179 ClipOfBullets
	180 BoxOfBullets
	181 MiniMissiles
	182 CrateOfMissiles
	183 EnergyPod
	184 EnergyPack
	185 PoisonBolts
	186 ElectricBolts
	187 AmmoSatchel
	188 AssaultGun
	189 AssaultGunStanding
	190 FlameThrower

	192 MiniMissileLauncher
	193 Mauler
	194 ElectricCrossbow

	201 PowerCrystal (incomplete)

	203 WoodenBarrel
	204 ExplosiveBarrel2
	205 TargetPractice
	206 LightSilverFluorescent
	207 LightBrownFluorescent
	208 LightGoldFluorescent
	209 LightGlobe
	210 PillarTechno
	211 PillarAztec
	212 PillarAztecDamaged
	213 PillarAztecRuined
	214 PillarHugeTech
	215 PillarAlienPower
	216 SStalactiteBig
	217 SStalactiteSmall
	218 SStalagmiteBig
	219 CavePillarTop
	220 CavePillarBottom
	221 SStalagmiteSmall
	222 Candle
	223 StrifeCandelabra
	224 WaterDropOnFloor
	225 WaterfallSplash
	226 WaterDrip
	227 WaterFountain
	228 HeartsInTank
	229 TeleportSwirl
	230 DeadCrusader (incomplete)
	231 DeadStrifePlayer
	232 DeadPeasant
	233 DeadAcolyte
	234 DeadReaver
	235 DeadRebel
	236 SacrificedGuy
	237 PileOfGuts
	238 StrifeBurningBarrel
	239 BurningBowl
	240 BurningBrazier
	241 SmallTorchLit
	242 SmallTorchUnlit
	243 CeilingChain
	244 CageLight
	245 Statue
	246 StatueRuined
	247 MediumTorch
	248 OutsideLamp
	249 PoleLantern
	250 SRock1
	251 SRock2
	252 SRock3
	253 SRock4
	254 StickInWater
	255 Rubble1
	256 Rubble2
	257 Rubble3
	258 Rubble4
	259 Rubble5
	260 Rubble6
	261 Rubble7
	262 Rubble8
	263 SurgeryCrab
	264 LargeTorch
	265 HugeTorch
	266 PalmTree
	267 BigTree2
	268 PottedTree
	269 TreeStub
	270 ShortBush
	271 TallBush
	272 ChimneyStack
	273 BarricadeColumn
	274 Pot
	275 Pitcher
	276 Stool
	277 MetalPot
	278 Tub
	279 Anvil
	280 TechLampSilver
	281 TechLampBrass
	282 Tray
	283 AmmoFiller
	284 SigilBanner
	285 RebelBoots
	286 RebelHelmet
	287 RebelShirt
	288 PowerCoupling

	290 AlienBubbleColumn
	291 AlienFloorBubble
	292 AlienCeilingBubble
	293 AlienAspClimber
	294 AlienSpiderLight
	295 Meat
	296 Junk
*/	

static FRandom pr_gibtosser ("GibTosser");
static FRandom pr_bang4cloud ("Bang4Cloud");
static FRandom pr_lightout ("LightOut");
static FRandom pr_reaverattack ("ReaverAttack");

void A_TossGib (AActor *);
void A_LoopActiveSound (AActor *);
void A_FLoopActiveSound (AActor *);
void A_Countdown (AActor *);
void A_Bang4Cloud (AActor *);
void A_LightGoesOut (AActor *);
void A_XXScream (AActor *);

// Tank 1 (Huge) ------------------------------------------------------------

class ATank1 : public AActor
{
	DECLARE_ACTOR (ATank1, AActor)
};

FState ATank1::States[] =
{
	S_NORMAL (TNK1, 'A', 15, NULL, &States[1]),
	S_NORMAL (TNK1, 'B', 11, NULL, &States[2]),
	S_NORMAL (TNK1, 'C', 40, NULL, &States[0])
};

IMPLEMENT_ACTOR (ATank1, Strife, 209, 0)
	PROP_SpawnState (0)
	PROP_RadiusFixed (16)
	PROP_HeightFixed (192)
	PROP_Flags (MF_SOLID)
END_DEFAULTS

// Tank 2 (Huge) ------------------------------------------------------------

class ATank2 : public AActor
{
	DECLARE_ACTOR (ATank2, AActor)
};

FState ATank2::States[] =
{
	S_NORMAL (TNK2, 'A', 15, NULL, &States[1]),
	S_NORMAL (TNK2, 'B', 11, NULL, &States[2]),
	S_NORMAL (TNK2, 'C', 40, NULL, &States[0])
};

IMPLEMENT_ACTOR (ATank2, Strife, 210, 0)
	PROP_SpawnState (0)
	PROP_RadiusFixed (16)
	PROP_HeightFixed (192)
	PROP_Flags (MF_SOLID)
END_DEFAULTS

// Tank 3 (Huge) ------------------------------------------------------------

class ATank3 : public AActor
{
	DECLARE_ACTOR (ATank3, AActor)
};

FState ATank3::States[] =
{
	S_NORMAL (TNK3, 'A', 15, NULL, &States[1]),
	S_NORMAL (TNK3, 'B', 11, NULL, &States[2]),
	S_NORMAL (TNK3, 'C', 40, NULL, &States[0])
};

IMPLEMENT_ACTOR (ATank3, Strife, 211, 0)
	PROP_SpawnState (0)
	PROP_RadiusFixed (16)
	PROP_HeightFixed (192)
	PROP_Flags (MF_SOLID)
END_DEFAULTS

// Tank 4 -------------------------------------------------------------------

class ATank4 : public AActor
{
	DECLARE_ACTOR (ATank4, AActor)
};

FState ATank4::States[] =
{
	S_NORMAL (TNK4, 'A', 15, NULL, &States[1]),
	S_NORMAL (TNK4, 'B', 11, NULL, &States[2]),
	S_NORMAL (TNK4, 'C', 40, NULL, &States[0])
};

IMPLEMENT_ACTOR (ATank4, Strife, 213, 0)
	PROP_SpawnState (0)
	PROP_RadiusFixed (16)
	PROP_HeightFixed (56)
	PROP_Flags (MF_SOLID)
END_DEFAULTS

// Tank 5 -------------------------------------------------------------------

class ATank5 : public AActor
{
	DECLARE_ACTOR (ATank5, AActor)
};

FState ATank5::States[] =
{
	S_NORMAL (TNK5, 'A', 15, NULL, &States[1]),
	S_NORMAL (TNK5, 'B', 11, NULL, &States[2]),
	S_NORMAL (TNK5, 'C', 40, NULL, &States[0])
};

IMPLEMENT_ACTOR (ATank5, Strife, 214, 0)
	PROP_SpawnState (0)
	PROP_RadiusFixed (16)
	PROP_HeightFixed (56)
	PROP_Flags (MF_SOLID)
END_DEFAULTS

// Tank 6 -------------------------------------------------------------------

class ATank6 : public AActor
{
	DECLARE_ACTOR (ATank6, AActor)
};

FState ATank6::States[] =
{
	S_NORMAL (TNK6, 'A', 15, NULL, &States[1]),
	S_NORMAL (TNK6, 'B', 11, NULL, &States[2]),
	S_NORMAL (TNK6, 'C', 40, NULL, &States[0])
};

IMPLEMENT_ACTOR (ATank6, Strife, 229, 0)
	PROP_SpawnState (0)
	PROP_RadiusFixed (16)
	PROP_HeightFixed (56)
	PROP_Flags (MF_SOLID)
END_DEFAULTS

// Reaver -------------------------------------------------------------------

void A_ReaverMelee (AActor *self);
void A_ReaverRanged (AActor *self);

void A_21230 (AActor *) {}

class AReaver : public AActor
{
	DECLARE_ACTOR (AReaver, AActor)
public:
	void GetExplodeParms (int &damage, int &dist, bool &hurtSource)
	{
		damage = dist = 32;
	}
};

FState AReaver::States[] =
{
#define S_REAVER_STND (0)
	S_NORMAL (ROB1, 'A',   10, A_Look,				&States[S_REAVER_STND]),
	// Needless duplication of the previous state removed

#define S_REAVER_RUN (S_REAVER_STND+1)
	S_NORMAL (ROB1, 'B',	3, A_Chase,				&States[S_REAVER_RUN+1]),
	S_NORMAL (ROB1, 'B',	3, A_Chase,				&States[S_REAVER_RUN+2]),
	S_NORMAL (ROB1, 'C',	3, A_Chase,				&States[S_REAVER_RUN+3]),
	S_NORMAL (ROB1, 'C',	3, A_Chase,				&States[S_REAVER_RUN+4]),
	S_NORMAL (ROB1, 'D',	3, A_Chase,				&States[S_REAVER_RUN+5]),
	S_NORMAL (ROB1, 'D',	3, A_Chase,				&States[S_REAVER_RUN+6]),
	S_NORMAL (ROB1, 'E',	3, A_Chase,				&States[S_REAVER_RUN+7]),
	S_NORMAL (ROB1, 'E',	3, A_Chase,				&States[S_REAVER_RUN]),

#define S_REAVER_MELEE (S_REAVER_RUN+8)
	S_NORMAL (ROB1, 'H',	6, A_FaceTarget,		&States[S_REAVER_MELEE+1]),
	S_NORMAL (ROB1, 'I',	8, A_ReaverMelee,		&States[S_REAVER_MELEE+2]),
	S_NORMAL (ROB1, 'H',	6, NULL,				&States[S_REAVER_RUN]),

#define S_REAVER_MISSILE (S_REAVER_MELEE+3)
	S_NORMAL (ROB1, 'F',	8, A_FaceTarget,		&States[S_REAVER_MISSILE+1]),
	S_BRIGHT (ROB1, 'G',   11, A_ReaverRanged,		&States[S_REAVER_RUN]),

#define S_REAVER_PAIN (S_REAVER_MISSILE+2)
	S_NORMAL (ROB1, 'A',	2, NULL,				&States[S_REAVER_PAIN+1]),
	S_NORMAL (ROB1, 'A',	2, A_Pain,				&States[S_REAVER_RUN]),

#define S_REAVER_DEATH (S_REAVER_PAIN+2)
	S_BRIGHT (ROB1, 'J',	6, NULL,				&States[S_REAVER_DEATH+1]),
	S_BRIGHT (ROB1, 'K',	6, A_Scream,			&States[S_REAVER_DEATH+2]),
	S_BRIGHT (ROB1, 'L',	5, NULL,				&States[S_REAVER_DEATH+3]),
	S_BRIGHT (ROB1, 'M',	5, A_NoBlocking,		&States[S_REAVER_DEATH+4]),
	S_BRIGHT (ROB1, 'N',	5, NULL,				&States[S_REAVER_DEATH+5]),
	S_BRIGHT (ROB1, 'O',	5, NULL,				&States[S_REAVER_DEATH+6]),
	S_BRIGHT (ROB1, 'P',	5, NULL,				&States[S_REAVER_DEATH+7]),
	S_BRIGHT (ROB1, 'Q',	6, A_Explode,			&States[S_REAVER_DEATH+8]),
	S_NORMAL (ROB1, 'R',   -1, NULL,				NULL),

#define S_REAVER_XDEATH (S_REAVER_DEATH+9)
	S_BRIGHT (ROB1, 'L',	5, A_TossGib,			&States[S_REAVER_XDEATH+1]),
	S_BRIGHT (ROB1, 'M',	5, A_XXScream,			&States[S_REAVER_XDEATH+2]),
	S_BRIGHT (ROB1, 'N',	5, A_TossGib,			&States[S_REAVER_XDEATH+3]),
	S_BRIGHT (ROB1, 'O',	5, A_NoBlocking,		&States[S_REAVER_XDEATH+4]),
	S_BRIGHT (ROB1, 'P',	5, A_TossGib,			&States[S_REAVER_XDEATH+5]),
	S_BRIGHT (ROB1, 'Q',	6, A_Explode,			&States[S_REAVER_XDEATH+6]),
	S_NORMAL (ROB1, 'R',   -1, NULL,				NULL),
};

IMPLEMENT_ACTOR (AReaver, Strife, 3001, 0)
	PROP_SpawnHealth (150)
	PROP_PainChance (128)
	PROP_SpeedFixed (12)
	PROP_RadiusFixed (20)
	PROP_HeightFixed (60)
	PROP_Flags (MF_SOLID|MF_SHOOTABLE|MF_STRIFEx8000|MF_NOBLOOD|MF_COUNTKILL)
	PROP_Flags2 (MF2_MCROSS|MF2_PASSMOBJ|MF2_PUSHWALL|MF2_FLOORCLIP)

	PROP_Mass (500)
	PROP_SpawnState (S_REAVER_STND)
	PROP_SeeState (S_REAVER_RUN)
	PROP_PainState (S_REAVER_PAIN)
	PROP_MeleeState (S_REAVER_MELEE)
	PROP_MissileState (S_REAVER_MISSILE)
	PROP_DeathState (S_REAVER_DEATH)
	PROP_XDeathState (S_REAVER_XDEATH)

	PROP_SeeSound ("reaver/sight")
	PROP_PainSound ("reaver/pain")
	PROP_DeathSound ("reaver/death")
	PROP_ActiveSound ("reaver/active")
END_DEFAULTS

void A_ReaverMelee (AActor *self)
{
	if (self->target != NULL)
	{
		A_FaceTarget (self);

		if (P_CheckMeleeRange (self))
		{
			int damage;

			S_Sound (self, CHAN_WEAPON, "reaver/blade", 1, ATTN_NORM);
			damage = ((pr_reaverattack() & 7) + 1) * 3;
			P_DamageMobj (self->target, self, self, damage, MOD_HIT);
			P_TraceBleed (damage, self->target, self);
		}
	}
}

void A_ReaverRanged (AActor *self)
{
	if (self->target != NULL)
	{
		angle_t bangle;
		int pitch;

		PuffType = RUNTIME_CLASS(AStrifePuff);
		A_FaceTarget (self);
		S_Sound (self, CHAN_WEAPON, "reaver/attack", 1, ATTN_NORM);
		bangle = self->angle;
		pitch = P_AimLineAttack (self, bangle, MISSILERANGE);

		for (int i = 0; i < 3; ++i)
		{
			angle_t angle = bangle + (pr_reaverattack.Random2() << 20);
			int damage = ((pr_reaverattack() & 7) + 1) * 3;
			P_LineAttack (self, angle, MISSILERANGE, pitch, damage);
		}
	}
}

// Entity Nest --------------------------------------------------------------

class AEntityNest : public AActor
{
	DECLARE_ACTOR (AEntityNest, AActor)
};

FState AEntityNest::States[] =
{
	S_NORMAL (NEST, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (AEntityNest, Strife, 26, 0)
	PROP_SpawnState (0)
	PROP_RadiusFixed (84)
	PROP_HeightFixed (47)
	PROP_Flags (MF_SOLID|MF_NOTDMATCH)
END_DEFAULTS

// Entity Pod ---------------------------------------------------------------

void A_SpawnEntity (AActor *);

class AEntityPod : public AActor
{
	DECLARE_ACTOR (AEntityPod, AActor)
};

FState AEntityPod::States[] =
{
	S_NORMAL (PODD, 'A',  60, A_Look,			&States[0]),

	S_NORMAL (PODD, 'A', 360, NULL,				&States[2]),
	S_NORMAL (PODD, 'B',   9, A_NoBlocking,		&States[3]),
	S_NORMAL (PODD, 'C',   9, NULL,				&States[4]),
	S_NORMAL (PODD, 'D',   9, A_SpawnEntity,	&States[5]),
	S_NORMAL (PODD, 'E',  -1, NULL,				NULL)
};

IMPLEMENT_ACTOR (AEntityPod, Strife, 198, 0)
	PROP_SpawnState (0)
	PROP_SeeState (1)
	PROP_RadiusFixed (25)
	PROP_HeightFixed (91)
	PROP_Flags (MF_SOLID|MF_NOTDMATCH)
	PROP_SeeSound ("misc/gibbed")
END_DEFAULTS

void A_SpawnEntity (AActor *self)
{
#if 0
	AActor *entity = Spawn<AEntity> (self->x, self->y, self->z + 70*FRACUNIT);
	if (entity != NULL)
	{
		entity->momz = 5*FRACUNIT;
		// And then it records the entity's spawn location at
		// 2F860, 2F864, 2F868
	}
#endif
}

// "Zap 6" ------------------------------------------------------------------

class AZap6 : public AActor
{
	DECLARE_ACTOR (AZap6, AActor)
};

FState AZap6::States[] =
{
	S_BRIGHT (ZAP6, 'A', 5, NULL, &States[1]),
	S_BRIGHT (ZAP6, 'B', 5, NULL, &States[2]),
	S_BRIGHT (ZAP6, 'C', 5, NULL, NULL)
};

IMPLEMENT_ACTOR (AZap6, Strife, -1, 0)
	PROP_SpawnState (0)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY|MF_DROPOFF)
	PROP_RenderStyle (STYLE_Add)
END_DEFAULTS

// "Zap 5" ------------------------------------------------------------------

void A_201fc (AActor *) {}

class AZap5 : public AActor
{
	DECLARE_ACTOR (AZap5, AActor)
};

FState AZap5::States[] =
{
	S_BRIGHT (ZAP5, 'A', 4, A_Countdown, &States[1]),
	S_BRIGHT (ZAP5, 'B', 4, A_201fc, &States[2]),
	S_BRIGHT (ZAP5, 'C', 4, A_Countdown, &States[3]),
	S_BRIGHT (ZAP5, 'D', 4, A_Countdown, &States[0]),
};

IMPLEMENT_ACTOR (AZap5, Strife, -1, 0)
	PROP_SpawnState (0)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY|MF_DROPOFF)
	PROP_RenderStyle (STYLE_Add)
	PROP_Alpha (HX_SHADOW)
END_DEFAULTS

// Klaxon Warning Light -----------------------------------------------------

void A_1f608 (AActor *) {}
void A_21c0c (AActor *) {}

class AKlaxonWarningLight : public AActor
{
	DECLARE_ACTOR (AKlaxonWarningLight, AActor)
};

FState AKlaxonWarningLight::States[] =
{
	S_NORMAL (KLAX, 'A',  5, A_1f608,	&States[0]),

	S_NORMAL (KLAX, 'B',  6, A_21c0c,	&States[2]),
	S_NORMAL (KLAX, 'C', 60, NULL,		&States[1])
};

IMPLEMENT_ACTOR (AKlaxonWarningLight, Strife, 24, 0)
	PROP_SpawnState (0)
	PROP_SeeState (1)
	PROP_ReactionTime (60)
	PROP_Flags (MF_NOBLOCKMAP|MF_AMBUSH|MF_SPAWNCEILING|MF_NOGRAVITY)
	PROP_Flags4 (MF4_FIXMAPTHINGPOS)
END_DEFAULTS

// Computer -----------------------------------------------------------------

void A_2100c (AActor *self)
{
	// This function does a lot more than just this
	if (self->DeathSound != 0)
	{
		S_SoundID (self, CHAN_VOICE, self->DeathSound, 1.0, ATTN_NORM);
		S_SoundID (self, 6, self->DeathSound, 1.0, ATTN_NORM);
	}
}

class AComputer : public AActor
{
	DECLARE_ACTOR (AComputer, AActor)
};

FState AComputer::States[] =
{
	S_BRIGHT (SECR, 'A', 4, NULL,				&States[1]),
	S_BRIGHT (SECR, 'B', 4, NULL,				&States[2]),
	S_BRIGHT (SECR, 'C', 4, NULL,				&States[3]),
	S_BRIGHT (SECR, 'D', 4, NULL,				&States[0]),

	S_BRIGHT (SECR, 'E', 5, A_Bang4Cloud,		&States[5]),
	S_BRIGHT (SECR, 'F', 5, A_NoBlocking,		&States[6]),
	S_BRIGHT (SECR, 'G', 5, A_2100c,			&States[7]),
	S_BRIGHT (SECR, 'H', 5, A_TossGib,			&States[8]),
	S_BRIGHT (SECR, 'I', 5, A_Bang4Cloud,		&States[9]),
	S_NORMAL (SECR, 'J', 5, NULL,				&States[10]),
	S_NORMAL (SECR, 'K', 5, A_Bang4Cloud,		&States[11]),
	S_NORMAL (SECR, 'L', 5, NULL,				&States[12]),
	S_NORMAL (SECR, 'M', 5, A_Bang4Cloud,		&States[13]),
	S_NORMAL (SECR, 'N', 5, NULL,				&States[14]),
	S_NORMAL (SECR, 'O', 5, A_Bang4Cloud,		&States[15]),
	S_NORMAL (SECR, 'P',-1, NULL,				NULL)
};

IMPLEMENT_ACTOR (AComputer, Strife, 182, 0)
	PROP_SpawnState (0)
	PROP_SpawnHealth (80)
	PROP_DeathState (4)
	PROP_SpeedFixed (27)		// was a byte in Strife
	PROP_RadiusFixed (26)
	PROP_HeightFixed (128)
	PROP_MassLong (100000)
	PROP_Flags (MF_SOLID|MF_SHOOTABLE|MF_STRIFEx400|MF_STRIFEx8000|MF_NOBLOOD)
	PROP_DeathSound ("misc/explosion")
END_DEFAULTS

// A Cloud use for varius explosions ----------------------------------------
// This actor has no direct equivalent in strife. To create this, Strife
// spawned a spark and then changed its state to that of this explosion
// cloud. Weird.

class ABang4Cloud : public AActor
{
	DECLARE_ACTOR (ABang4Cloud, AActor);
public:
	void BeginPlay ()
	{
		momz = FRACUNIT;
	}
};

FState ABang4Cloud::States[] =
{
	S_BRIGHT (BNG4, 'B', 3, NULL,		&States[1]),
	S_BRIGHT (BNG4, 'C', 3, NULL,		&States[2]),
	S_BRIGHT (BNG4, 'D', 3, NULL,		&States[3]),
	S_BRIGHT (BNG4, 'E', 3, NULL,		&States[4]),
	S_BRIGHT (BNG4, 'F', 3, NULL,		&States[5]),
	S_BRIGHT (BNG4, 'G', 3, NULL,		&States[6]),
	S_BRIGHT (BNG4, 'H', 3, NULL,		&States[7]),
	S_BRIGHT (BNG4, 'I', 3, NULL,		&States[8]),
	S_BRIGHT (BNG4, 'J', 3, NULL,		&States[9]),
	S_BRIGHT (BNG4, 'K', 3, NULL,		&States[10]),
	S_BRIGHT (BNG4, 'L', 3, NULL,		&States[11]),
	S_BRIGHT (BNG4, 'M', 3, NULL,		&States[12]),
	S_BRIGHT (BNG4, 'N', 3, NULL,		NULL)
};

IMPLEMENT_ACTOR (ABang4Cloud, Strife, -1, 0)
	PROP_SpawnState (0)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY)
	PROP_RenderStyle (STYLE_Add)
END_DEFAULTS

void A_Bang4Cloud (AActor *self)
{
	fixed_t spawnx, spawny;

	spawnx = self->x + (pr_bang4cloud.Random2() & 3) * 10240;
	spawny = self->y + (pr_bang4cloud.Random2() & 3) * 10240;

	Spawn<ABang4Cloud> (spawnx, spawny, self->z);
}

// Water Bottle -------------------------------------------------------------

class AWaterBottle : public AActor
{
	DECLARE_ACTOR (AWaterBottle, AActor)
};

FState AWaterBottle::States[] =
{
	S_NORMAL (WATR, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (AWaterBottle, Strife, 2014, 0)
	PROP_SpawnState (0)
END_DEFAULTS

// Mug ----------------------------------------------------------------------

class AMug : public AActor
{
	DECLARE_ACTOR (AMug, AActor)
};

FState AMug::States[] =
{
	S_NORMAL (MUGG, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (AMug, Strife, 164, 0)
	PROP_SpawnState (0)
END_DEFAULTS

// Power Crystal ------------------------------------------------------------

void A_21100 (AActor *);
void A_21124 (AActor *);

class APowerCrystal : public AActor
{
	DECLARE_ACTOR (APowerCrystal, AActor)
};

FState APowerCrystal::States[] =
{
	S_BRIGHT (CRYS, 'A', 16, A_LoopActiveSound,		&States[1]),
	S_BRIGHT (CRYS, 'B',  5, A_LoopActiveSound,		&States[2]),
	S_BRIGHT (CRYS, 'C',  4, A_LoopActiveSound,		&States[3]),
	S_BRIGHT (CRYS, 'D',  4, A_LoopActiveSound,		&States[4]),
	S_BRIGHT (CRYS, 'E',  4, A_LoopActiveSound,		&States[5]),
	S_BRIGHT (CRYS, 'F',  4, A_LoopActiveSound,		&States[0]),

	S_BRIGHT (BOOM, 'A',  1, A_21124,				&States[7]),
	S_BRIGHT (BOOM, 'B',  3, A_2100c,				&States[8]),
	S_BRIGHT (BOOM, 'C',  2, A_LightGoesOut,		&States[9]),
	S_BRIGHT (BOOM, 'D',  3, A_Bang4Cloud,			&States[10]),
	S_BRIGHT (BOOM, 'E',  3, NULL,					&States[11]),
	S_BRIGHT (BOOM, 'F',  3, NULL,					&States[12]),
	S_BRIGHT (BOOM, 'G',  3, A_Bang4Cloud,			&States[13]),
	S_BRIGHT (BOOM, 'H',  1, A_21124,				&States[14]),
	S_BRIGHT (BOOM, 'I',  3, NULL,					&States[15]),
	S_BRIGHT (BOOM, 'J',  3, A_Bang4Cloud,			&States[16]),
	S_BRIGHT (BOOM, 'K',  3, A_Bang4Cloud,			&States[17]),
	S_BRIGHT (BOOM, 'L',  3, A_Bang4Cloud,			&States[18]),
	S_BRIGHT (BOOM, 'M',  3, NULL,					&States[19]),
	S_BRIGHT (BOOM, 'N',  3, NULL,					&States[20]),
	S_BRIGHT (BOOM, 'O',  3, A_Bang4Cloud,			&States[21]),
	S_BRIGHT (BOOM, 'P',  3, NULL,					&States[22]),
	S_BRIGHT (BOOM, 'Q',  3, NULL,					&States[23]),
	S_BRIGHT (BOOM, 'R',  3, NULL,					&States[24]),
	S_BRIGHT (BOOM, 'S',  3, NULL,					&States[25]),
	S_BRIGHT (BOOM, 'T',  3, NULL,					&States[26]),
	S_BRIGHT (BOOM, 'U',  3, A_21100,				&States[27]),
	S_BRIGHT (BOOM, 'V',  3, NULL,					&States[28]),
	S_BRIGHT (BOOM, 'W',  3, NULL,					&States[29]),
	S_BRIGHT (BOOM, 'X',  3, NULL,					&States[30]),
	S_BRIGHT (BOOM, 'Y',  3, NULL,					NULL)
};

IMPLEMENT_ACTOR (APowerCrystal, Strife, 92, 0)
	PROP_SpawnState (0)
	PROP_SpawnHealth (50)
	PROP_DeathState (6)
	PROP_SpeedFixed (14)		// Was a byte in Strife
	PROP_RadiusFixed (20)		// This size seems odd, but that's what the mobjinfo says
	PROP_HeightFixed (16)
	PROP_MassLong (99999999)
	PROP_Flags (MF_SOLID|MF_SHOOTABLE|MF_NOGRAVITY|MF_STRIFEx400|MF_NOBLOOD)
	PROP_DeathSound ("misc/explosion")
	PROP_ActiveSound ("misc/reactor")
END_DEFAULTS

void A_21100 (AActor *self)
{
	if (self->target != NULL && self->target->player != NULL)
	{
		// Store 0 in the dword at offset 0x307 in the player struct
	}
}

void A_21124 (AActor *self)
{
	//sys25768 (self, self->target, 512);
	if (self->target != NULL && self->target->player != NULL)
	{
		// Store 5 in the dword at offset 0x307 in the player struct
	}

	// Strife didn't do this next part, but it looks good
	self->RenderStyle = STYLE_Add;
}

// Wooden Barrel ------------------------------------------------------------

class AWoodenBarrel : public AActor
{
	DECLARE_ACTOR (AWoodenBarrel, AActor)
};

FState AWoodenBarrel::States[] =
{
	S_NORMAL (BARW, 'A', -1, NULL, NULL),

	S_NORMAL (BARW, 'B', 2, A_Scream,			&States[2]),
	S_NORMAL (BARW, 'C', 2, NULL,				&States[3]),
	S_NORMAL (BARW, 'D', 2, A_NoBlocking,		&States[4]),
	S_NORMAL (BARW, 'E', 2, NULL,				&States[5]),
	S_NORMAL (BARW, 'F', 2, NULL,				&States[6]),
	S_NORMAL (BARW, 'G', 2, NULL,				&States[7]),
	S_NORMAL (BARW, 'H', -1, NULL,				NULL)
};

IMPLEMENT_ACTOR (AWoodenBarrel, Strife, 82, 0)
	PROP_SpawnState (0)
	PROP_SpawnHealth (10)
	PROP_DeathState (1)
	PROP_RadiusFixed (10)
	PROP_HeightFixed (32)
	PROP_Flags (MF_SOLID|MF_SHOOTABLE|MF_STRIFEx8000|MF_NOBLOOD)
	PROP_DeathSound ("woodenbarrel/death")
END_DEFAULTS

// Strife's explosive barrel ------------------------------------------------

class AExplosiveBarrel2 : public AExplosiveBarrel
{
	DECLARE_ACTOR (AExplosiveBarrel2, AExplosiveBarrel)
public:
	void GetExplodeParms (int &damage, int &dist, bool &hurtSource)
	{
		damage = dist = 64;
	}
};

FState AExplosiveBarrel2::States[] =
{
	S_NORMAL (BART, 'A', -1, NULL, NULL),

	S_BRIGHT (BART, 'B',  2, A_Scream,		&States[2]),
	S_BRIGHT (BART, 'C',  2, NULL,			&States[3]),
	S_BRIGHT (BART, 'D',  2, NULL,			&States[4]),
	S_BRIGHT (BART, 'E',  2, A_NoBlocking,	&States[5]),
	S_BRIGHT (BART, 'F',  2, A_Explode,		&States[6]),
	S_BRIGHT (BART, 'G',  2, NULL,			&States[7]),
	S_BRIGHT (BART, 'H',  2, NULL,			&States[8]),
	S_BRIGHT (BART, 'I',  2, NULL,			&States[9]),
	S_BRIGHT (BART, 'J',  2, NULL,			&States[10]),
	S_BRIGHT (BART, 'K',  3, NULL,			&States[11]),
	S_BRIGHT (BART, 'L', -1, NULL,			NULL),
	
};

IMPLEMENT_ACTOR (AExplosiveBarrel2, Strife, 94, 0)
	PROP_SpawnState (0)
	PROP_SpawnHealth (30)
	PROP_DeathState (1)
	PROP_RadiusFixed (10)
	PROP_HeightFixed (32)
	//PROP_Flags (MF_SOLID|MF_SHOOTABLE|MF_STRIFEx8000|MF_NOBLOOD)
END_DEFAULTS

// Target Practice -----------------------------------------------------------

class ATargetPractice : public AActor
{
	DECLARE_ACTOR (ATargetPractice, AActor)
};

void A_20e10 (AActor *self)
{
	sector_t *sec = self->Sector;

	if (self->z == sec->floorplane.ZatPoint (self->x, self->y))
	{
		if ((sec->special & 0xFF) == Damage_InstantDeath)
		{
			P_DamageMobj (self, NULL, NULL, 999);
		}
		else if ((sec->special & 0xFF) == Scroll_StrifeCurrent)
		{
			int anglespeed = sec->tag - 100;
			fixed_t speed = (anglespeed % 10) << (FRACBITS - 4);
			angle_t finean = (anglespeed / 10) << (32-3);
			finean >>= ANGLETOFINESHIFT;
			self->momx += FixedMul (speed, finecosine[finean]);
			self->momy += FixedMul (speed, finesine[finean]);
		}
	}
}

FState ATargetPractice::States[] =
{
	S_NORMAL (HOGN, 'A', 2, A_20e10, &States[0]),

	S_NORMAL (HOGN, 'B', 1, A_20e10, &States[2]),
	S_NORMAL (HOGN, 'C', 1, A_Pain, &States[0])
};

IMPLEMENT_ACTOR (ATargetPractice, Strife, 208, 0)
	PROP_SpawnState (0)
	PROP_SpawnHealthLong (99999999)
	PROP_PainState (1)
	PROP_PainChance (255)
	PROP_RadiusFixed (10)
	PROP_HeightFixed (72)
	PROP_MassLong (9999999)
	PROP_Flags (MF_SOLID|MF_SHOOTABLE|MF_STRIFEx8000|MF_NOBLOOD)
	PROP_PainSound ("misc/metalhit")
END_DEFAULTS

// Light (Silver, Fluorescent) ----------------------------------------------

class ALightSilverFluorescent : public AActor
{
	DECLARE_ACTOR (ALightSilverFluorescent, AActor)
};

FState ALightSilverFluorescent::States[] =
{
	S_BRIGHT (LITS, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (ALightSilverFluorescent, Strife, 95, 0)
	PROP_SpawnState (0)
	PROP_RadiusFixed (4)
	PROP_HeightFixed (16)
	PROP_Flags (MF_NOBLOCKMAP)
	PROP_Flags4 (MF4_FIXMAPTHINGPOS)
END_DEFAULTS

// Light (Brown, Fluorescent) -----------------------------------------------

class ALightBrownFluorescent : public AActor
{
	DECLARE_ACTOR (ALightBrownFluorescent, AActor)
};

FState ALightBrownFluorescent::States[] =
{
	S_BRIGHT (LITB, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (ALightBrownFluorescent, Strife, 96, 0)
	PROP_SpawnState (0)
	PROP_RadiusFixed (4)
	PROP_HeightFixed (16)
	PROP_Flags (MF_NOBLOCKMAP)
	PROP_Flags4 (MF4_FIXMAPTHINGPOS)
END_DEFAULTS

// Light (Gold, Fluorescent) ------------------------------------------------

class ALightGoldFluorescent : public AActor
{
	DECLARE_ACTOR (ALightGoldFluorescent, AActor)
};

FState ALightGoldFluorescent::States[] =
{
	S_BRIGHT (LITG, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (ALightGoldFluorescent, Strife, 97, 0)
	PROP_SpawnState (0)
	PROP_RadiusFixed (4)
	PROP_HeightFixed (16)
	PROP_Flags (MF_NOBLOCKMAP)
	PROP_Flags4 (MF4_FIXMAPTHINGPOS)
END_DEFAULTS

// Light Globe --------------------------------------------------------------

class ALightGlobe : public AActor
{
	DECLARE_ACTOR (ALightGlobe, AActor)
};

FState ALightGlobe::States[] =
{
	S_BRIGHT (LITE, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (ALightGlobe, Strife, 2028, 0)
	PROP_SpawnState (0)
	PROP_RadiusFixed (16)
	PROP_HeightFixed (16)
	PROP_Flags (MF_SOLID)
END_DEFAULTS

// Techno Pillar ------------------------------------------------------------

class APillarTechno : public AActor
{
	DECLARE_ACTOR (APillarTechno, AActor)
};

FState APillarTechno::States[] =
{
	S_NORMAL (MONI, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (APillarTechno, Strife, 48, 0)
	PROP_SpawnState (0)
	PROP_RadiusFixed (20)
	PROP_HeightFixed (128)
	PROP_Flags (MF_SOLID)
END_DEFAULTS

// Aztec Pillar -------------------------------------------------------------

class APillarAztec : public AActor
{
	DECLARE_ACTOR (APillarAztec, AActor)
};

FState APillarAztec::States[] =
{
	S_NORMAL (STEL, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (APillarAztec, Strife, 54, 0)
	PROP_SpawnState (0)
	PROP_RadiusFixed (16)
	PROP_HeightFixed (128)
	PROP_Flags (MF_SOLID)
END_DEFAULTS

// Damaged Aztec Pillar -----------------------------------------------------

class APillarAztecDamaged : public AActor
{
	DECLARE_ACTOR (APillarAztecDamaged, AActor)
};

FState APillarAztecDamaged::States[] =
{
	S_NORMAL (STLA, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (APillarAztecDamaged, Strife, 55, 0)
	PROP_SpawnState (0)
	PROP_RadiusFixed (16)
	PROP_HeightFixed (80)
	PROP_Flags (MF_SOLID)
END_DEFAULTS

// Ruined Aztec Pillar ------------------------------------------------------

class APillarAztecRuined : public AActor
{
	DECLARE_ACTOR (APillarAztecRuined, AActor)
};

FState APillarAztecRuined::States[] =
{
	S_NORMAL (STLE, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (APillarAztecRuined, Strife, 56, 0)
	PROP_SpawnState (0)
	PROP_RadiusFixed (16)
	PROP_HeightFixed (40)
	PROP_Flags (MF_SOLID)
END_DEFAULTS

// Huge Tech Pillar ---------------------------------------------------------

class APillarHugeTech : public AActor
{
	DECLARE_ACTOR (APillarHugeTech, AActor)
};

FState APillarHugeTech::States[] =
{
	S_NORMAL (HUGE, 'A', 4, NULL, &States[1]),
	S_NORMAL (HUGE, 'B', 4, NULL, &States[2]),
	S_NORMAL (HUGE, 'C', 4, NULL, &States[3]),
	S_NORMAL (HUGE, 'D', 4, NULL, &States[0])
};

IMPLEMENT_ACTOR (APillarHugeTech, Strife, 57, 0)
	PROP_SpawnState (0)
	PROP_RadiusFixed (24)
	PROP_HeightFixed (192)
	PROP_Flags (MF_SOLID)
END_DEFAULTS

// Alien Power Crystal in a Pillar ------------------------------------------

class APillarAlienPower : public AActor
{
	DECLARE_ACTOR (APillarAlienPower, AActor)
};

FState APillarAlienPower::States[] =
{
	S_NORMAL (APOW, 'A', 4, A_LoopActiveSound, &States[0])
};

IMPLEMENT_ACTOR (APillarAlienPower, Strife, 227, 0)
	PROP_SpawnState (0)
	PROP_RadiusFixed (24)
	PROP_HeightFixed (192)
	PROP_Flags (MF_SOLID)
	PROP_ActiveSound ("ambient/alien2")
END_DEFAULTS

// SStalactiteBig -----------------------------------------------------------

class ASStalactiteBig : public AActor
{
	DECLARE_ACTOR (ASStalactiteBig, AActor)
};

FState ASStalactiteBig::States[] =
{
	S_NORMAL (STLG, 'C', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (ASStalactiteBig, Strife, 98, 0)
	PROP_SpawnState (0)
	PROP_RadiusFixed (16)
	PROP_HeightFixed (54)
	PROP_Flags (MF_SOLID|MF_SPAWNCEILING|MF_NOGRAVITY)
END_DEFAULTS

// SStalactiteSmall ---------------------------------------------------------

class ASStalactiteSmall : public AActor
{
	DECLARE_ACTOR (ASStalactiteSmall, AActor)
};

FState ASStalactiteSmall::States[] =
{
	S_NORMAL (STLG, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (ASStalactiteSmall, Strife, 161, 0)
	PROP_SpawnState (0)
	PROP_RadiusFixed (16)
	PROP_HeightFixed (40)
	PROP_Flags (MF_SOLID|MF_SPAWNCEILING|MF_NOGRAVITY)
END_DEFAULTS

// SStalagmiteBig -----------------------------------------------------------

class ASStalagmiteBig : public AActor
{
	DECLARE_ACTOR (ASStalagmiteBig, AActor)
};

FState ASStalagmiteBig::States[] =
{
	S_NORMAL (STLG, 'B', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (ASStalagmiteBig, Strife, 160, 0)
	PROP_SpawnState (0)
	PROP_RadiusFixed (16)
	PROP_HeightFixed (40)
	PROP_Flags (MF_SOLID)
END_DEFAULTS

// Cave Pillar Top ----------------------------------------------------------

class ACavePillarTop : public AActor
{
	DECLARE_ACTOR (ACavePillarTop, AActor)
};

FState ACavePillarTop::States[] =
{
	S_NORMAL (STLG, 'D', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (ACavePillarTop, Strife, 159, 0)
	PROP_SpawnState (0)
	PROP_RadiusFixed (16)
	PROP_HeightFixed (128)
	PROP_Flags (MF_SOLID|MF_SPAWNCEILING|MF_NOGRAVITY)
END_DEFAULTS

// Cave Pillar Bottom -------------------------------------------------------

class ACavePillarBottom : public AActor
{
	DECLARE_ACTOR (ACavePillarBottom, AActor)
};

FState ACavePillarBottom::States[] =
{
	S_NORMAL (STLG, 'E', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (ACavePillarBottom, Strife, 162, 0)
	PROP_SpawnState (0)
	PROP_RadiusFixed (16)
	PROP_HeightFixed (128)
	PROP_Flags (MF_SOLID)
END_DEFAULTS

// SStalagmiteSmall ---------------------------------------------------------

class ASStalagmiteSmall : public AActor
{
	DECLARE_ACTOR (ASStalagmiteSmall, AActor)
};

FState ASStalagmiteSmall::States[] =
{
	S_NORMAL (STLG, 'F', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (ASStalagmiteSmall, Strife, 163, 0)
	PROP_SpawnState (0)
	PROP_RadiusFixed (16)
	PROP_HeightFixed (25)
	PROP_Flags (MF_SOLID)
END_DEFAULTS

// Candle -------------------------------------------------------------------

class ACandle : public AActor
{
	DECLARE_ACTOR (ACandle, AActor)
};

FState ACandle::States[] =
{
	S_BRIGHT (CNDL, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (ACandle, Strife, 34, 0)
	PROP_SpawnState (0)
END_DEFAULTS

// StrifeCandelabra ---------------------------------------------------------

class AStrifeCandelabra : public AActor
{
	DECLARE_ACTOR (AStrifeCandelabra, AActor)
};

FState AStrifeCandelabra::States[] =
{
	S_BRIGHT (CLBR, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (AStrifeCandelabra, Strife, 35, 0)
	PROP_SpawnState (0)
	PROP_RadiusFixed (16)
	PROP_HeightFixed (40)
	PROP_Flags (MF_SOLID)
END_DEFAULTS

// Floor Water Drop ---------------------------------------------------------

class AWaterDropOnFloor : public AActor
{
	DECLARE_ACTOR (AWaterDropOnFloor, AActor)
};

FState AWaterDropOnFloor::States[] =
{
	S_NORMAL (DRIP, 'A', 6, A_FLoopActiveSound,	&States[1]),
	S_NORMAL (DRIP, 'B', 4, NULL,				&States[2]),
	S_NORMAL (DRIP, 'C', 4, NULL,				&States[3]),
	S_NORMAL (DRIP, 'D', 4, A_FLoopActiveSound,	&States[4]),
	S_NORMAL (DRIP, 'E', 4, NULL,				&States[5]),
	S_NORMAL (DRIP, 'F', 4, NULL,				&States[6]),
	S_NORMAL (DRIP, 'G', 4, A_FLoopActiveSound,	&States[7]),
	S_NORMAL (DRIP, 'H', 4, NULL,				&States[0])
};

IMPLEMENT_ACTOR (AWaterDropOnFloor, Strife, 103, 0)
	PROP_SpawnState (0)
	PROP_Flags (MF_NOBLOCKMAP)
	PROP_ActiveSound ("world/waterdrip")
END_DEFAULTS

// Waterfall Splash ---------------------------------------------------------
// Why is this not visible in Strife?

class AWaterfallSplash : public AActor
{
	DECLARE_ACTOR (AWaterfallSplash, AActor)
};

FState AWaterfallSplash::States[] =
{
	S_NORMAL (SPLH, 'A', 4, NULL,				&States[1]),
	S_NORMAL (SPLH, 'B', 4, NULL,				&States[2]),
	S_NORMAL (SPLH, 'C', 4, NULL,				&States[3]),
	S_NORMAL (SPLH, 'D', 8, NULL,				&States[4]),
	S_NORMAL (SPLH, 'E', 4, NULL,				&States[5]),
	S_NORMAL (SPLH, 'F', 4, NULL,				&States[6]),
	S_NORMAL (SPLH, 'G', 4, NULL,				&States[7]),
	S_NORMAL (SPLH, 'H', 4, A_LoopActiveSound,	&States[0]),
};

IMPLEMENT_ACTOR (AWaterfallSplash, Strife, 104, 0)
	PROP_SpawnState (0)
	PROP_Flags (MF_NOBLOCKMAP)
	PROP_ActiveSound ("world/waterfall")
END_DEFAULTS

// Ceiling Water Drip -------------------------------------------------------

class AWaterDrip : public AActor
{
	DECLARE_ACTOR (AWaterDrip, AActor)
};

FState AWaterDrip::States[] =
{
	S_NORMAL (CDRP, 'A', 10, NULL, &States[1]),
	S_NORMAL (CDRP, 'B',  8, NULL, &States[2]),
	S_NORMAL (CDRP, 'C',  8, NULL, &States[3]),
	S_NORMAL (CDRP, 'D',  8, NULL, &States[0])
};

IMPLEMENT_ACTOR (AWaterDrip, Strife, 53, 0)
	PROP_SpawnState (0)
	PROP_HeightFixed (1)
	PROP_Flags (MF_NOBLOCKMAP|MF_SPAWNCEILING|MF_NOGRAVITY)
END_DEFAULTS

// WaterFountain ------------------------------------------------------------

class AWaterFountain : public AActor
{
	DECLARE_ACTOR (AWaterFountain, AActor)
};

FState AWaterFountain::States[] =
{
	S_NORMAL (WTFT, 'A', 4, NULL,				&States[1]),
	S_NORMAL (WTFT, 'B', 4, NULL,				&States[2]),
	S_NORMAL (WTFT, 'C', 4, NULL,				&States[3]),
	S_NORMAL (WTFT, 'D', 4, A_LoopActiveSound,	&States[0]),
};

IMPLEMENT_ACTOR (AWaterFountain, Strife, 112, 0)
	PROP_SpawnState (0)
	PROP_Flags (MF_NOBLOCKMAP)
	PROP_ActiveSound ("world/watersplash")
END_DEFAULTS

// Hearts in Tank -----------------------------------------------------------

class AHeartsInTank : public AActor
{
	DECLARE_ACTOR (AHeartsInTank, AActor)
};

FState AHeartsInTank::States[] =
{
	S_BRIGHT (HERT, 'A', 4, NULL, &States[1]),
	S_BRIGHT (HERT, 'B', 4, NULL, &States[2]),
	S_BRIGHT (HERT, 'C', 4, NULL, &States[0])
};

IMPLEMENT_ACTOR (AHeartsInTank, Strife, 113, 0)
	PROP_SpawnState (0)
	PROP_RadiusFixed (16)
	PROP_HeightFixed (56)
	PROP_Flags (MF_SOLID)
END_DEFAULTS

// Teleport Swirl -----------------------------------------------------------

class ATeleportSwirl : public AActor
{
	DECLARE_ACTOR (ATeleportSwirl, AActor)
};

FState ATeleportSwirl::States[] =
{
	S_BRIGHT (TELP, 'A', 3, NULL, &States[1]),
	S_BRIGHT (TELP, 'B', 3, NULL, &States[2]),
	S_BRIGHT (TELP, 'C', 3, NULL, &States[3]),
	S_BRIGHT (TELP, 'D', 3, NULL, &States[0])
};

IMPLEMENT_ACTOR (ATeleportSwirl, Strife, 23, 0)
	PROP_SpawnState (0)
	PROP_Flags (MF_NOBLOCKMAP)
	PROP_RenderStyle (STYLE_Add)
	PROP_Alpha (HX_SHADOW)
END_DEFAULTS

// Dead Crusader ------------------------------------------------------------

void A_21598() {}

class ADeadCrusader : public AActor
{
	DECLARE_ACTOR (ADeadCrusader, AActor)
};

FState ADeadCrusader::States[] =
{
	S_NORMAL (ROB2, 'N', 4, A_TossGib,	&States[1]),
	S_NORMAL (ROB2, 'O', 4, A_Explode,	&States[2]),
	S_NORMAL (ROB2, 'P',-1, A_21598,	NULL)
};

IMPLEMENT_ACTOR (ADeadCrusader, Strife, 22, 0)
	PROP_SpawnState (0)
END_DEFAULTS

// Dead Player --------------------------------------------------------------
// Strife's disappeared. This one doesn't.

class ADeadStrifePlayer : public AActor
{
	DECLARE_ACTOR (ADeadStrifePlayer, AActor)
};

FState ADeadStrifePlayer::States[] =
{
	S_NORMAL (PLAY, 'P', 700, NULL, &States[1]),
	S_NORMAL (RGIB, 'H',  -1, NULL, NULL)
};

IMPLEMENT_ACTOR (ADeadStrifePlayer, Strife, 15, 0)
	PROP_SpawnState (0)
END_DEFAULTS

// Dead Peasant -------------------------------------------------------------
// Unlike Strife's, this one does not turn into gibs and disappear.

class ADeadPeasant : public AActor
{
	DECLARE_ACTOR (ADeadPeasant, AActor)
};

FState ADeadPeasant::States[] =
{
	S_NORMAL (PEAS, 'N', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (ADeadPeasant, Strife, 18, 0)
	PROP_SpawnState (0)
END_DEFAULTS

// Dead Acolyte -------------------------------------------------------------
// Unlike Strife's, this one does not turn into gibs and disappear.

class ADeadAcolyte : public AActor
{
	DECLARE_ACTOR (ADeadAcolyte, AActor)
};

FState ADeadAcolyte::States[] =
{
	S_NORMAL (AGRD, 'N', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (ADeadAcolyte, Strife, 21, 0)
	PROP_SpawnState (0)
END_DEFAULTS

// Dead Reaver --------------------------------------------------------------

class ADeadReaver : public AActor
{
	DECLARE_ACTOR (ADeadReaver, AActor)
};

FState ADeadReaver::States[] =
{
	S_NORMAL (ROB1, 'R', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (ADeadReaver, Strife, 20, 0)
	PROP_SpawnState (0)
END_DEFAULTS

// Dead Rebel ---------------------------------------------------------------

class ADeadRebel : public AActor
{
	DECLARE_ACTOR (ADeadRebel, AActor)
};

FState ADeadRebel::States[] =
{
	S_NORMAL (HMN1, 'N', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (ADeadRebel, Strife, 19, 0)
	PROP_SpawnState (0)
END_DEFAULTS

// Sacrificed Guy -----------------------------------------------------------

class ASacrificedGuy : public AActor
{
	DECLARE_ACTOR (ASacrificedGuy, AActor)
};

FState ASacrificedGuy::States[] =
{
	S_NORMAL (SACR, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (ASacrificedGuy, Strife, 212, 0)
	PROP_SpawnState (0)
END_DEFAULTS

// Pile of Guts -------------------------------------------------------------

class APileOfGuts : public AActor
{
	DECLARE_ACTOR (APileOfGuts, AActor)
};

FState APileOfGuts::States[] =
{
	S_NORMAL (DEAD, 'A', -1, NULL, NULL)
};

// Strife used a doomednum, which is the same as the Aztec Pillar. Since
// the pillar came first in the mobjinfo list, you could not spawn this
// in a map. Pity.
IMPLEMENT_ACTOR (APileOfGuts, Strife, -1, 0)
	PROP_SpawnState (0)
END_DEFAULTS

// Burning Barrel -----------------------------------------------------------

class AStrifeBurningBarrel : public AActor
{
	DECLARE_ACTOR (AStrifeBurningBarrel, AActor)
};

FState AStrifeBurningBarrel::States[] =
{
	S_BRIGHT (BARL, 'A', 4, NULL, &States[1]),
	S_BRIGHT (BARL, 'B', 4, NULL, &States[2]),
	S_BRIGHT (BARL, 'C', 4, NULL, &States[3]),
	S_BRIGHT (BARL, 'D', 4, NULL, &States[0]),
};

IMPLEMENT_ACTOR (AStrifeBurningBarrel, Strife, 70, 0)
	PROP_SpawnState (0)
	PROP_RadiusFixed (16)
	PROP_HeightFixed (48)
	PROP_Flags (MF_SOLID)
END_DEFAULTS

// Burning Bowl -----------------------------------------------------------

class ABurningBowl : public AActor
{
	DECLARE_ACTOR (ABurningBowl, AActor)
};

FState ABurningBowl::States[] =
{
	S_BRIGHT (BOWL, 'A', 4, NULL, &States[1]),
	S_BRIGHT (BOWL, 'B', 4, NULL, &States[2]),
	S_BRIGHT (BOWL, 'C', 4, NULL, &States[3]),
	S_BRIGHT (BOWL, 'D', 4, NULL, &States[0]),
};

IMPLEMENT_ACTOR (ABurningBowl, Strife, 105, 0)
	PROP_SpawnState (0)
	PROP_RadiusFixed (16)
	PROP_HeightFixed (16)
	PROP_Flags (MF_SOLID)
	PROP_ActiveSound ("world/smallfire")
END_DEFAULTS

// Burning Brazier -----------------------------------------------------------

class ABurningBrazier : public AActor
{
	DECLARE_ACTOR (ABurningBrazier, AActor)
};

FState ABurningBrazier::States[] =
{
	S_BRIGHT (BRAZ, 'A', 4, NULL, &States[1]),
	S_BRIGHT (BRAZ, 'B', 4, NULL, &States[2]),
	S_BRIGHT (BRAZ, 'C', 4, NULL, &States[3]),
	S_BRIGHT (BRAZ, 'D', 4, NULL, &States[0]),
};

IMPLEMENT_ACTOR (ABurningBrazier, Strife, 106, 0)
	PROP_SpawnState (0)
	PROP_RadiusFixed (10)
	PROP_HeightFixed (32)
	PROP_Flags (MF_SOLID)
	PROP_ActiveSound ("world/smallfire")
END_DEFAULTS

// Small Torch (Lit) --------------------------------------------------------

class ASmallTorchLit : public AActor
{
	DECLARE_ACTOR (ASmallTorchLit, AActor)
};

FState ASmallTorchLit::States[] =
{
	S_BRIGHT (TRCH, 'A', 4, NULL, &States[1]),
	S_BRIGHT (TRCH, 'B', 4, NULL, &States[2]),
	S_BRIGHT (TRCH, 'C', 4, NULL, &States[3]),
	S_BRIGHT (TRCH, 'D', 4, NULL, &States[0])
};

IMPLEMENT_ACTOR (ASmallTorchLit, Strife, 107, 0)
	PROP_SpawnState (0)
	PROP_Radius (4)			// This is intentionally not PROP_RadiusFixed
	PROP_HeightFixed (16)
	PROP_Flags (MF_NOBLOCKMAP)
	PROP_Flags4 (MF4_FIXMAPTHINGPOS)

	// It doesn't have any action functions, so how does it use this sound?
	PROP_ActiveSound ("world/smallfire")
END_DEFAULTS

// Small Torch (Unlit) --------------------------------------------------------

class ASmallTorchUnlit : public AActor
{
	DECLARE_ACTOR (ASmallTorchUnlit, AActor)
};

FState ASmallTorchUnlit::States[] =
{
	S_NORMAL (TRHO, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (ASmallTorchUnlit, Strife, 108, 0)
	PROP_SpawnState (0)
	PROP_Radius (4)			// This is intentionally not PROP_RadiusFixed
	PROP_HeightFixed (16)
	PROP_Flags (MF_NOBLOCKMAP)
	PROP_Flags4 (MF4_FIXMAPTHINGPOS)
END_DEFAULTS

// Ceiling Chain ------------------------------------------------------------

class ACeilingChain : public AActor
{
	DECLARE_ACTOR (ACeilingChain, AActor)
};

FState ACeilingChain::States[] =
{
	S_NORMAL (CHAN, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (ACeilingChain, Strife, 109, 0)
	PROP_SpawnState (0)
	PROP_RadiusFixed (20)
	PROP_HeightFixed (93)
	PROP_Flags (MF_NOBLOCKMAP|MF_SPAWNCEILING|MF_NOGRAVITY)
END_DEFAULTS

// Cage Light ---------------------------------------------------------------

class ACageLight : public AActor
{
	DECLARE_ACTOR (ACageLight, AActor)
};

FState ACageLight::States[] =
{
	// No, it's not bright even though it's a light.
	S_NORMAL (CAGE, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (ACageLight, Strife, 28, 0)
	PROP_SpawnState (0)
	PROP_HeightFixed (3)
	PROP_Flags (MF_NOBLOCKMAP|MF_SPAWNCEILING|MF_NOGRAVITY)
END_DEFAULTS

// Statue -------------------------------------------------------------------

class AStatue : public AActor
{
	DECLARE_ACTOR (AStatue, AActor)
};

FState AStatue::States[] =
{
	S_NORMAL (STAT, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (AStatue, Strife, 110, 0)
	PROP_SpawnState (0)
	PROP_RadiusFixed (20)
	PROP_HeightFixed (64)
	PROP_Flags (MF_SOLID)
END_DEFAULTS

// Ruined Statue ------------------------------------------------------------

class AStatueRuined : public AActor
{
	DECLARE_ACTOR (AStatueRuined, AActor)
};

FState AStatueRuined::States[] =
{
	S_NORMAL (DSTA, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (AStatueRuined, Strife, 44, 0)
	PROP_SpawnState (0)
	PROP_RadiusFixed (20)
	PROP_HeightFixed (56)
	PROP_Flags (MF_SOLID)
END_DEFAULTS

// Medium Torch -------------------------------------------------------------

class AMediumTorch : public AActor
{
	DECLARE_ACTOR (AMediumTorch, AActor)
};

FState AMediumTorch::States[] =
{
	S_BRIGHT (LTRH, 'A', 4, NULL, &States[1]),
	S_BRIGHT (LTRH, 'B', 4, NULL, &States[2]),
	S_BRIGHT (LTRH, 'C', 4, NULL, &States[3]),
	S_BRIGHT (LTRH, 'D', 4, NULL, &States[0]),
};

IMPLEMENT_ACTOR (AMediumTorch, Strife, 111, 0)
	PROP_SpawnState (0)
	PROP_RadiusFixed (4)
	PROP_HeightFixed (72)
	PROP_Flags (MF_SOLID)
END_DEFAULTS

// Outside Lamp -------------------------------------------------------------

class AOutsideLamp : public AActor
{
	DECLARE_ACTOR (AOutsideLamp, AActor)
};

FState AOutsideLamp::States[] =
{
	// No, it's not bright.
	S_NORMAL (LAMP, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (AOutsideLamp, Strife, 43, 0)
	PROP_SpawnState (0)
	PROP_RadiusFixed (3)
	PROP_HeightFixed (80)
	PROP_Flags (MF_SOLID)
END_DEFAULTS

// Pole Lantern -------------------------------------------------------------

class APoleLantern : public AActor
{
	DECLARE_ACTOR (APoleLantern, AActor)
};

FState APoleLantern::States[] =
{
	// No, it's not bright.
	S_NORMAL (LANT, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (APoleLantern, Strife, 46, 0)
	PROP_SpawnState (0)
	PROP_RadiusFixed (3)
	PROP_HeightFixed (80)
	PROP_Flags (MF_SOLID)
END_DEFAULTS

// Rock 1 -------------------------------------------------------------------

class ASRock1 : public AActor
{
	DECLARE_ACTOR (ASRock1, AActor)
};

FState ASRock1::States[] =
{
	S_NORMAL (ROK1, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (ASRock1, Strife, 99, 0)
	PROP_SpawnState (0)
	PROP_Flags (MF_NOBLOCKMAP)
END_DEFAULTS

// Rock 2 -------------------------------------------------------------------

class ASRock2 : public AActor
{
	DECLARE_ACTOR (ASRock2, AActor)
};

FState ASRock2::States[] =
{
	S_NORMAL (ROK2, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (ASRock2, Strife, 100, 0)
	PROP_SpawnState (0)
	PROP_Flags (MF_NOBLOCKMAP)
END_DEFAULTS

// Rock 3 -------------------------------------------------------------------

class ASRock3 : public AActor
{
	DECLARE_ACTOR (ASRock3, AActor)
};

FState ASRock3::States[] =
{
	S_NORMAL (ROK3, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (ASRock3, Strife, 101, 0)
	PROP_SpawnState (0)
	PROP_Flags (MF_NOBLOCKMAP)
END_DEFAULTS

// Rock 4 -------------------------------------------------------------------

class ASRock4 : public AActor
{
	DECLARE_ACTOR (ASRock4, AActor)
};

FState ASRock4::States[] =
{
	S_NORMAL (ROK4, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (ASRock4, Strife, 102, 0)
	PROP_SpawnState (0)
	PROP_Flags (MF_NOBLOCKMAP)
END_DEFAULTS

// Stick in Water -----------------------------------------------------------

class AStickInWater : public AActor
{
	DECLARE_ACTOR (AStickInWater, AActor)
};

FState AStickInWater::States[] =
{
	S_NORMAL (LOGG, 'A', 5, A_LoopActiveSound, &States[1]),
	S_NORMAL (LOGG, 'B', 5, A_LoopActiveSound, &States[2]),
	S_NORMAL (LOGG, 'C', 5, A_LoopActiveSound, &States[3]),
	S_NORMAL (LOGG, 'D', 5, A_LoopActiveSound, &States[0])
};

IMPLEMENT_ACTOR (AStickInWater, Strife, 215, 0)
	PROP_SpawnState (0)
	PROP_Flags (MF_NOBLOCKMAP)
	PROP_ActiveSound ("world/river")
END_DEFAULTS

// Rubble 1 -----------------------------------------------------------------

class ARubble1 : public AActor
{
	DECLARE_ACTOR (ARubble1, AActor)
};

FState ARubble1::States[] =
{
	S_NORMAL (RUB1, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (ARubble1, Strife, 29, 0)
	PROP_SpawnState (0)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOCLIP)
END_DEFAULTS

// Rubble 2 -----------------------------------------------------------------

class ARubble2 : public AActor
{
	DECLARE_ACTOR (ARubble2, AActor)
};

FState ARubble2::States[] =
{
	S_NORMAL (RUB2, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (ARubble2, Strife, 30, 0)
	PROP_SpawnState (0)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOCLIP)
END_DEFAULTS

// Rubble 3 -----------------------------------------------------------------

class ARubble3 : public AActor
{
	DECLARE_ACTOR (ARubble3, AActor)
};

FState ARubble3::States[] =
{
	S_NORMAL (RUB3, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (ARubble3, Strife, 31, 0)
	PROP_SpawnState (0)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOCLIP)
END_DEFAULTS

// Rubble 4 -----------------------------------------------------------------

class ARubble4 : public AActor
{
	DECLARE_ACTOR (ARubble4, AActor)
};

FState ARubble4::States[] =
{
	S_NORMAL (RUB4, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (ARubble4, Strife, 32, 0)
	PROP_SpawnState (0)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOCLIP)
END_DEFAULTS

// Rubble 5 -----------------------------------------------------------------

class ARubble5 : public AActor
{
	DECLARE_ACTOR (ARubble5, AActor)
};

FState ARubble5::States[] =
{
	S_NORMAL (RUB5, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (ARubble5, Strife, 36, 0)
	PROP_SpawnState (0)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOCLIP)
END_DEFAULTS

// Rubble 6 -----------------------------------------------------------------

class ARubble6 : public AActor
{
	DECLARE_ACTOR (ARubble6, AActor)
};

FState ARubble6::States[] =
{
	S_NORMAL (RUB6, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (ARubble6, Strife, 37, 0)
	PROP_SpawnState (0)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOCLIP)
END_DEFAULTS

// Rubble 7 -----------------------------------------------------------------

class ARubble7 : public AActor
{
	DECLARE_ACTOR (ARubble7, AActor)
};

FState ARubble7::States[] =
{
	S_NORMAL (RUB7, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (ARubble7, Strife, 41, 0)
	PROP_SpawnState (0)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOCLIP)
END_DEFAULTS

// Rubble 8 -----------------------------------------------------------------

class ARubble8 : public AActor
{
	DECLARE_ACTOR (ARubble8, AActor)
};

FState ARubble8::States[] =
{
	S_NORMAL (RUB8, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (ARubble8, Strife, 42, 0)
	PROP_SpawnState (0)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOCLIP)
END_DEFAULTS

// Surgery Crab -------------------------------------------------------------

class ASurgeryCrab : public AActor
{
	DECLARE_ACTOR (ASurgeryCrab, AActor)
};

FState ASurgeryCrab::States[] =
{
	S_NORMAL (CRAB, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (ASurgeryCrab, Strife, 117, 0)
	PROP_SpawnState (0)
	PROP_Flags (MF_SOLID|MF_SPAWNCEILING|MF_NOGRAVITY)
	PROP_RadiusFixed (20)
	PROP_HeightFixed (16)
END_DEFAULTS

// Large Torch --------------------------------------------------------------

class ALargeTorch : public AActor
{
	DECLARE_ACTOR (ALargeTorch, AActor)
};

FState ALargeTorch::States[] =
{
	S_BRIGHT (LMPC, 'A', 4, NULL, &States[1]),
	S_BRIGHT (LMPC, 'B', 4, NULL, &States[2]),
	S_BRIGHT (LMPC, 'C', 4, NULL, &States[3]),
	S_BRIGHT (LMPC, 'D', 4, NULL, &States[0])
};

IMPLEMENT_ACTOR (ALargeTorch, Strife, 47, 0)
	PROP_SpawnState (0)
	PROP_RadiusFixed (10)
	PROP_HeightFixed (72)
	PROP_Flags (MF_SOLID)
	PROP_ActiveSound ("world/smallfire")
END_DEFAULTS

// Huge Torch --------------------------------------------------------------

class AHugeTorch : public AActor
{
	DECLARE_ACTOR (AHugeTorch, AActor)
};

FState AHugeTorch::States[] =
{
	S_BRIGHT (LOGS, 'A', 4, NULL, &States[1]),
	S_BRIGHT (LOGS, 'B', 4, NULL, &States[2]),
	S_BRIGHT (LOGS, 'C', 4, NULL, &States[3]),
	S_BRIGHT (LOGS, 'D', 4, NULL, &States[0])
};

IMPLEMENT_ACTOR (AHugeTorch, Strife, 50, 0)
	PROP_SpawnState (0)
	PROP_RadiusFixed (10)
	PROP_HeightFixed (80)
	PROP_Flags (MF_SOLID)
	PROP_ActiveSound ("world/smallfire")
END_DEFAULTS

// Palm Tree ----------------------------------------------------------------

class APalmTree : public AActor
{
	DECLARE_ACTOR (APalmTree, AActor)
};

FState APalmTree::States[] =
{
	S_NORMAL (TREE, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (APalmTree, Strife, 51, 0)
	PROP_SpawnState (0)
	PROP_RadiusFixed (15)
	PROP_HeightFixed (109)
	PROP_Flags (MF_SOLID)
END_DEFAULTS

// Big Tree ----------------------------------------------------------------

class ABigTree2 : public AActor
{
	DECLARE_ACTOR (ABigTree2, AActor)
};

FState ABigTree2::States[] =
{
	S_NORMAL (TREE, 'B', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (ABigTree2, Strife, 202, 0)
	PROP_SpawnState (0)
	PROP_RadiusFixed (15)
	PROP_HeightFixed (109)
	PROP_Flags (MF_SOLID)
END_DEFAULTS

// Potted Tree ----------------------------------------------------------------

class APottedTree : public AActor
{
	DECLARE_ACTOR (APottedTree, AActor)
};

FState APottedTree::States[] =
{
	S_NORMAL (TREE, 'C', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (APottedTree, Strife, 203, 0)
	PROP_SpawnState (0)
	PROP_RadiusFixed (15)
	PROP_HeightFixed (64)
	PROP_Flags (MF_SOLID)
END_DEFAULTS

// Tree Stub ----------------------------------------------------------------

class ATreeStub : public AActor
{
	DECLARE_ACTOR (ATreeStub, AActor)
};

FState ATreeStub::States[] =
{
	S_NORMAL (TRE1, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (ATreeStub, Strife, 33, 0)
	PROP_SpawnState (0)
	PROP_RadiusFixed (15)
	PROP_HeightFixed (80)
	PROP_Flags (MF_SOLID)
END_DEFAULTS

// Short Bush ---------------------------------------------------------------

class AShortBush : public AActor
{
	DECLARE_ACTOR (AShortBush, AActor)
};

FState AShortBush::States[] =
{
	S_NORMAL (BUSH, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (AShortBush, Strife, 60, 0)
	PROP_SpawnState (0)
	PROP_RadiusFixed (15)
	PROP_HeightFixed (40)
	PROP_Flags (MF_SOLID)
END_DEFAULTS

// Tall Bush ---------------------------------------------------------------

class ATallBush : public AActor
{
	DECLARE_ACTOR (ATallBush, AActor)
};

FState ATallBush::States[] =
{
	S_NORMAL (SHRB, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (ATallBush, Strife, 62, 0)
	PROP_SpawnState (0)
	PROP_RadiusFixed (20)
	PROP_HeightFixed (64)
	PROP_Flags (MF_SOLID)
END_DEFAULTS

// Chimney Stack ------------------------------------------------------------

class AChimneyStack : public AActor
{
	DECLARE_ACTOR (AChimneyStack, AActor)
};

FState AChimneyStack::States[] =
{
	S_NORMAL (STAK, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (AChimneyStack, Strife, 63, 0)
	PROP_SpawnState (0)
	PROP_RadiusFixed (20)
	PROP_HeightFixed (64)	// This height does not fit the sprite
	PROP_Flags (MF_SOLID)
END_DEFAULTS

// Barricade Column ---------------------------------------------------------

class ABarricadeColumn : public AActor
{
	DECLARE_ACTOR (ABarricadeColumn, AActor)
};

FState ABarricadeColumn::States[] =
{
	S_NORMAL (BAR1, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (ABarricadeColumn, Strife, 69, 0)
	PROP_SpawnState (0)
	PROP_RadiusFixed (16)
	PROP_HeightFixed (128)
	PROP_Flags (MF_SOLID)
END_DEFAULTS

// Pot ----------------------------------------------------------------------

class APot : public AActor
{
	DECLARE_ACTOR (APot, AActor)
};

FState APot::States[] =
{
	S_NORMAL (VASE, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (APot, Strife, 165, 0)
	PROP_SpawnState (0)
	PROP_RadiusFixed (12)
	PROP_HeightFixed (24)
	PROP_Flags (MF_SOLID)
END_DEFAULTS

// Pitcher ------------------------------------------------------------------

class APitcher : public AActor
{
	DECLARE_ACTOR (APitcher, AActor)
};

FState APitcher::States[] =
{
	S_NORMAL (VASE, 'B', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (APitcher, Strife, 188, 0)
	PROP_SpawnState (0)
	PROP_RadiusFixed (12)
	PROP_HeightFixed (32)
	PROP_Flags (MF_SOLID)
END_DEFAULTS

// Stool --------------------------------------------------------------------

class AStool : public AActor
{
	DECLARE_ACTOR (AStool, AActor)
};

FState AStool::States[] =
{
	S_NORMAL (STOL, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (AStool, Strife, 189, 0)
	PROP_SpawnState (0)
	PROP_RadiusFixed (6)
	PROP_HeightFixed (24)
	PROP_Flags (MF_SOLID)
END_DEFAULTS

// Metal Pot ----------------------------------------------------------------

class AMetalPot : public AActor
{
	DECLARE_ACTOR (AMetalPot, AActor)
};

FState AMetalPot::States[] =
{
	S_NORMAL (POT1, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (AMetalPot, Strife, 190, 0)
	PROP_SpawnState (0)
	PROP_Flags (MF_NOBLOCKMAP)
END_DEFAULTS

// Tub ----------------------------------------------------------------------

class ATub : public AActor
{
	DECLARE_ACTOR (ATub, AActor)
};

FState ATub::States[] =
{
	S_NORMAL (TUB1, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (ATub, Strife, 191, 0)
	PROP_SpawnState (0)
	PROP_Flags (MF_NOBLOCKMAP)
END_DEFAULTS

// Anvil --------------------------------------------------------------------

class AAnvil : public AActor
{
	DECLARE_ACTOR (AAnvil, AActor)
};

FState AAnvil::States[] =
{
	S_NORMAL (ANVL, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (AAnvil, Strife, 194, 0)
	PROP_SpawnState (0)
	PROP_RadiusFixed (16)
	PROP_HeightFixed (32)
	PROP_Flags (MF_SOLID)
END_DEFAULTS

// Silver Tech Lamp ----------------------------------------------------------

class ATechLampSilver : public AActor
{
	DECLARE_ACTOR (ATechLampSilver, AActor)
};

FState ATechLampSilver::States[] =
{
	S_NORMAL (TLMP, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (ATechLampSilver, Strife, 196, 0)
	PROP_SpawnState (0)
	PROP_RadiusFixed (11)
	PROP_HeightFixed (64)
	PROP_Flags (MF_SOLID)
END_DEFAULTS

// Brass Tech Lamp ----------------------------------------------------------

class ATechLampBrass : public AActor
{
	DECLARE_ACTOR (ATechLampBrass, AActor)
};

FState ATechLampBrass::States[] =
{
	S_NORMAL (TLMP, 'B', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (ATechLampBrass, Strife, 197, 0)
	PROP_SpawnState (0)
	PROP_RadiusFixed (8)
	PROP_HeightFixed (64)
	PROP_Flags (MF_SOLID)
END_DEFAULTS

// Tray --------------------------------------------------------------------

class ATray : public AActor
{
	DECLARE_ACTOR (ATray, AActor)
};

FState ATray::States[] =
{
	S_NORMAL (TRAY, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (ATray, Strife, 68, 0)
	PROP_SpawnState (0)
	PROP_RadiusFixed (24)
	PROP_HeightFixed (40)
	PROP_Flags (MF_SOLID)
END_DEFAULTS

// AmmoFiller ---------------------------------------------------------------

class AAmmoFiller : public AActor
{
	DECLARE_ACTOR (AAmmoFiller, AActor)
};

FState AAmmoFiller::States[] =
{
	S_NORMAL (AFED, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (AAmmoFiller, Strife, 228, 0)
	PROP_SpawnState (0)
	PROP_RadiusFixed (12)
	PROP_HeightFixed (24)
	PROP_Flags (MF_SOLID)
END_DEFAULTS

// Sigil Banner -------------------------------------------------------------

class ASigilBanner : public AActor
{
	DECLARE_ACTOR (ASigilBanner, AActor)
};

FState ASigilBanner::States[] =
{
	S_NORMAL (SBAN, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (ASigilBanner, Strife, 216, 0)
	PROP_SpawnState (0)
	PROP_RadiusFixed (24)
	PROP_HeightFixed (96)
	PROP_Flags (MF_NOBLOCKMAP)	// I take it this was once solid, yes?
END_DEFAULTS

// RebelBoots ---------------------------------------------------------------

class ARebelBoots : public AActor
{
	DECLARE_ACTOR (ARebelBoots, AActor)
};

FState ARebelBoots::States[] =
{
	S_NORMAL (BOTR, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (ARebelBoots, Strife, 217, 0)
	PROP_SpawnState (0)
	PROP_Flags (MF_NOBLOCKMAP)
END_DEFAULTS

// RebelHelmet --------------------------------------------------------------

class ARebelHelmet : public AActor
{
	DECLARE_ACTOR (ARebelHelmet, AActor)
};

FState ARebelHelmet::States[] =
{
	S_NORMAL (HATR, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (ARebelHelmet, Strife, 218, 0)
	PROP_SpawnState (0)
	PROP_Flags (MF_NOBLOCKMAP)
END_DEFAULTS

// RebelShirt ---------------------------------------------------------------

class ARebelShirt : public AActor
{
	DECLARE_ACTOR (ARebelShirt, AActor)
};

FState ARebelShirt::States[] =
{
	S_NORMAL (TOPR, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (ARebelShirt, Strife, 219, 0)
	PROP_SpawnState (0)
	PROP_Flags (MF_NOBLOCKMAP)
END_DEFAULTS

// Power Coupling -----------------------------------------------------------

class APowerCoupling : public AActor
{
	DECLARE_ACTOR (APowerCoupling, AActor)
};

FState APowerCoupling::States[] =
{
	S_NORMAL (COUP, 'A', 5, NULL, &States[1]),
	S_NORMAL (COUP, 'B', 5, NULL, &States[0])
};

IMPLEMENT_ACTOR (APowerCoupling, Strife, 220, 0)
	PROP_SpawnState (0)
	PROP_SpawnHealth (40)
	PROP_SpeedFixed (6)		// Was a byte in Strife
	PROP_RadiusFixed (17)
	PROP_HeightFixed (64)
	PROP_MassLong (999999)
	PROP_Flags (MF_SOLID|MF_SHOOTABLE|MF_STRIFEx400|MF_STRIFEx8000|MF_DROPPED|MF_NOBLOOD|MF_NOTDMATCH)
	// Why no death state?
END_DEFAULTS

// Alien Bubble Column ------------------------------------------------------

class AAlienBubbleColumn : public AActor
{
	DECLARE_ACTOR (AAlienBubbleColumn, AActor)
};

FState AAlienBubbleColumn::States[] =
{
	S_NORMAL (BUBB, 'A', 4, A_LoopActiveSound, &States[0])
};

IMPLEMENT_ACTOR (AAlienBubbleColumn, Strife, 221, 0)
	PROP_SpawnState (0)
	PROP_RadiusFixed (16)
	PROP_HeightFixed (128)
	PROP_Flags (MF_SOLID)
	PROP_ActiveSound ("ambient/alien5")
END_DEFAULTS

// Alien Floor Bubble -------------------------------------------------------

class AAlienFloorBubble : public AActor
{
	DECLARE_ACTOR (AAlienFloorBubble, AActor)
};

FState AAlienFloorBubble::States[] =
{
	S_NORMAL (BUBF, 'A', 4, A_LoopActiveSound, &States[0])
};

IMPLEMENT_ACTOR (AAlienFloorBubble, Strife, 222, 0)
	PROP_SpawnState (0)
	PROP_RadiusFixed (16)
	PROP_HeightFixed (72)
	PROP_Flags (MF_SOLID)
	PROP_ActiveSound ("ambient/alien6")
END_DEFAULTS

// Alien Ceiling Bubble -----------------------------------------------------

class AAlienCeilingBubble : public AActor
{
	DECLARE_ACTOR (AAlienCeilingBubble, AActor)
};

FState AAlienCeilingBubble::States[] =
{
	S_NORMAL (BUBC, 'A', 4, A_LoopActiveSound, &States[0])
};

IMPLEMENT_ACTOR (AAlienCeilingBubble, Strife, 223, 0)
	PROP_SpawnState (0)
	PROP_RadiusFixed (16)
	PROP_HeightFixed (72)
	PROP_Flags (MF_SOLID|MF_SPAWNCEILING|MF_NOGRAVITY)
	PROP_ActiveSound ("ambient/alien4")
END_DEFAULTS

// Alien Asp Climber --------------------------------------------------------

class AAlienAspClimber : public AActor
{
	DECLARE_ACTOR (AAlienAspClimber, AActor)
};

FState AAlienAspClimber::States[] =
{
	S_NORMAL (ASPR, 'A', 4, A_LoopActiveSound, &States[0])
};

IMPLEMENT_ACTOR (AAlienAspClimber, Strife, 224, 0)
	PROP_SpawnState (0)
	PROP_RadiusFixed (16)
	PROP_HeightFixed (128)
	PROP_Flags (MF_SOLID)
	PROP_ActiveSound ("ambient/alien3")
END_DEFAULTS

// Alien Spider Light -------------------------------------------------------

class AAlienSpiderLight : public AActor
{
	DECLARE_ACTOR (AAlienSpiderLight, AActor)
};

FState AAlienSpiderLight::States[] =
{
	S_NORMAL (SPDL, 'A', 5, A_LoopActiveSound, &States[1]),
	S_NORMAL (SPDL, 'B', 5, A_LoopActiveSound, &States[2]),
	S_NORMAL (SPDL, 'C', 5, A_LoopActiveSound, &States[0])
};

IMPLEMENT_ACTOR (AAlienSpiderLight, Strife, 225, 0)
	PROP_SpawnState (0)
	PROP_RadiusFixed (32)
	PROP_HeightFixed (56)
	PROP_Flags (MF_SOLID)
	PROP_ActiveSound ("ambient/alien1")
END_DEFAULTS

// Gibs for things that bleed -----------------------------------------------

class AMeat : public AActor
{
	DECLARE_ACTOR (AMeat, AActor)
public:
	void BeginPlay ()
	{
		// Strife used mod 19, but there are 20 states. Hmm.
		SetState (SpawnState + pr_gibtosser() % 20);
	}
};

FState AMeat::States[] =
{
	S_NORMAL (MEAT, 'A', 700, NULL, NULL),
	S_NORMAL (MEAT, 'B', 700, NULL, NULL),
	S_NORMAL (MEAT, 'C', 700, NULL, NULL),
	S_NORMAL (MEAT, 'D', 700, NULL, NULL),
	S_NORMAL (MEAT, 'E', 700, NULL, NULL),
	S_NORMAL (MEAT, 'F', 700, NULL, NULL),
	S_NORMAL (MEAT, 'G', 700, NULL, NULL),
	S_NORMAL (MEAT, 'H', 700, NULL, NULL),
	S_NORMAL (MEAT, 'I', 700, NULL, NULL),
	S_NORMAL (MEAT, 'J', 700, NULL, NULL),
	S_NORMAL (MEAT, 'K', 700, NULL, NULL),
	S_NORMAL (MEAT, 'L', 700, NULL, NULL),
	S_NORMAL (MEAT, 'M', 700, NULL, NULL),
	S_NORMAL (MEAT, 'N', 700, NULL, NULL),
	S_NORMAL (MEAT, 'O', 700, NULL, NULL),
	S_NORMAL (MEAT, 'P', 700, NULL, NULL),
	S_NORMAL (MEAT, 'Q', 700, NULL, NULL),
	S_NORMAL (MEAT, 'R', 700, NULL, NULL),
	S_NORMAL (MEAT, 'S', 700, NULL, NULL),
	S_NORMAL (MEAT, 'T', 700, NULL, NULL)
};

IMPLEMENT_ACTOR (AMeat, Any, -1, 0)
	PROP_SpawnState (0)
	PROP_Flags (MF_NOCLIP|MF_NOBLOCKMAP)
END_DEFAULTS

// Gibs for things that don't bleed -----------------------------------------

class AJunk : public AMeat
{
	DECLARE_ACTOR (AJunk, AMeat)
};

FState AJunk::States[] =
{
	S_NORMAL (JUNK, 'A', 700, NULL, NULL),
	S_NORMAL (JUNK, 'B', 700, NULL, NULL),
	S_NORMAL (JUNK, 'C', 700, NULL, NULL),
	S_NORMAL (JUNK, 'D', 700, NULL, NULL),
	S_NORMAL (JUNK, 'E', 700, NULL, NULL),
	S_NORMAL (JUNK, 'F', 700, NULL, NULL),
	S_NORMAL (JUNK, 'G', 700, NULL, NULL),
	S_NORMAL (JUNK, 'H', 700, NULL, NULL),
	S_NORMAL (JUNK, 'I', 700, NULL, NULL),
	S_NORMAL (JUNK, 'J', 700, NULL, NULL),
	S_NORMAL (JUNK, 'K', 700, NULL, NULL),
	S_NORMAL (JUNK, 'L', 700, NULL, NULL),
	S_NORMAL (JUNK, 'M', 700, NULL, NULL),
	S_NORMAL (JUNK, 'N', 700, NULL, NULL),
	S_NORMAL (JUNK, 'O', 700, NULL, NULL),
	S_NORMAL (JUNK, 'P', 700, NULL, NULL),
	S_NORMAL (JUNK, 'Q', 700, NULL, NULL),
	S_NORMAL (JUNK, 'R', 700, NULL, NULL),
	S_NORMAL (JUNK, 'S', 700, NULL, NULL),
	S_NORMAL (JUNK, 'T', 700, NULL, NULL)
};

IMPLEMENT_ACTOR (AJunk, Any, -1, 0)
	PROP_SpawnState (0)
	PROP_Flags (MF_NOCLIP|MF_NOBLOCKMAP)
END_DEFAULTS

//==========================================================================
//
// A_TossGib
//
//==========================================================================

void A_TossGib (AActor *self)
{
	const TypeInfo *gibtype = (self->flags & MF_NOBLOOD) ? RUNTIME_CLASS(AJunk) : RUNTIME_CLASS(AMeat);
	AActor *gib = Spawn (gibtype, self->x, self->y, self->z + 24*FRACUNIT);
	angle_t an;
	int speed;

	if (gib == NULL)
	{
		return;
	}

	an = pr_gibtosser() << 24;
	gib->angle = an;
	speed = pr_gibtosser() & 15;
	gib->momx = speed * finecosine[an >> ANGLETOFINESHIFT];
	gib->momy = speed * finesine[an >> ANGLETOFINESHIFT];
	gib->momz = (pr_gibtosser() & 15) << FRACBITS;
}

//============================================================================

void A_FLoopActiveSound (AActor *self)
{
	if (self->ActiveSound != 0 && !(level.time & 7))
	{
		S_SoundID (self, CHAN_VOICE, self->ActiveSound, 1, ATTN_NORM);
	}
}

void A_Countdown (AActor *self)
{
	if (--self->reactiontime <= 0)
	{
		P_ExplodeMissile (self, NULL);
		self->flags &= ~MF_SKULLFLY;
	}
}

void A_LightGoesOut (AActor *self)
{
	AActor *foo;
	sector_t *sec = self->Sector;
	vertex_t *spot;
	fixed_t newheight;

	sec->lightlevel = 0;

	newheight = sec->FindLowestFloorSurrounding (&spot);
	sec->floorplane.d = sec->floorplane.PointToDist (spot, newheight);

	for (int i = 0; i < 8; ++i)
	{
		foo = Spawn<ARubble1> (self->x, self->y, self->z);
		if (foo != NULL)
		{
			int t = pr_lightout() & 15;
			foo->momx = (t - (pr_lightout() & 7)) << FRACBITS;
			foo->momy = (pr_lightout.Random2() & 7) << FRACBITS;
			foo->momz = (7 + (pr_lightout() & 3)) << FRACBITS;
		}
	}
}

void A_LoopActiveSound (AActor *self)
{
	if (self->ActiveSound != 0 && !S_IsActorPlayingSomething (self, CHAN_VOICE))
	{
		S_LoopedSoundID (self, CHAN_VOICE, self->ActiveSound, 1, ATTN_NORM);
	}
}
