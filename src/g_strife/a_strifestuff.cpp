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
#include "c_console.h"

// Notes so I don't forget them:
// Strife does some extra stuff in A_Explode if a player caused the explosion. (probably NoiseAlert)
// See the instructions @ 21249.
//
// Strife's FLOATSPEED is 5 and not 4.
//
// In P_CheckMissileRange, mobjtypes 53,54,55,56,57,58 shift the distance right 4 bits (some, but not all the acolytes)
//						   mobjtypes 61,63,91 shift it right 1 bit
//
// When shooting missiles at something, if MF_SHADOW is set, the angle is adjusted with the formula:
//		angle += pr_spawnmissile.Random2() << 21
// When MF_STRIFEx4000000 is set, the angle is adjusted similarly:
//		angle += pr_spawnmissile.Random2() << 22
// Note that these numbers are different from those used by all the other Doom engine games.

// Strife seems to use the mass of armor for something, since it's not the default value.

/* These mobjinfos have been converted:

	  0 ForceFieldGuard
	  1 StrifePlayer
	  2 WeaponSmith
	  3 BarKeep
	  4 Armorer
	  5 Medic
	  6 Peasant1
	  7 Peasant2
	  8 Peasant3
	  9 Peasant4
     10 Peasant5
	 11 Peasant6
	 12 Peasant7
	 13 Peasant8
	 14 Peasant9
	 15 Peasant10
	 16 Peasant11
	 17 Peasant12
	 18 Peasant13
	 19 Peasant14
	 20 Peasant15
	 21 Peasant16
	 22 Peasant17
	 23 Peasant18
	 24 Peasant19
	 25 Peasant20
	 26 Peasant21
     27 Peasant22
	 28 Zombie
	 29 AcolyteToBe
	 30 ZombieSpawner
	 31 Tank1
	 32 Tank2
	 33 Tank3
	 34 Tank4
	 35 Tank5
	 36 Tank6
	 37 KneelingGuy
	 38 Beggar1
	 39 Beggar2
	 40 Beggar3
	 41 Beggar4
	 42 Beggar5
	 43 Rebel1
	 44 Rebel2
	 45 Rebel3
	 46 Rebel4
	 47 Rebel5
	 48 Rebel6
	 49 Macil1
	 50 Macil2
	 51 RocketTrail
	 52 Reaver
	 53 AcolyteTan
	 54 AcolyteRed
	 55 AcolyteRust
	 56 AcolyteGray
	 57 AcolyteDGreen
	 58 AcolyteGold
	 59 AcolyteLGreen
	 60 AcolyteBlue
	 61 AcolyteShadow
	 62 Templar
	 63 Crusader
	 64 StrifeBishop
	 65 Oracle
	 66 Loremaster (aka Priest)
	 67 AlienSpectre1
	 68 AlienChunkSmall
	 69 AlienChunkLarge
	 70 AlienSpectre2
	 71 AlienSpectre3
	 72 AlienSpectre4
	 73 AlienSpectre5
	 74 EntityBoss
	 75 EntitySecond
	 76 EntityNest
	 77 EntityPod
	 78 SpectralLightningH1
	 79 SpectralLightningH2
	 80 SpectralLightningBall1
	 81 SpectralLightningBall2
	 82 SpectralLightningH3
	 83 SpectralLightningHTail
	 84 SpectralLightningBigBall1
	 85 SpectralLightningBigBall2
	 86 SpectralLightningV1
	 87 SpectralLightningV2
	 88 SpectralLightningSpot
	 89 SpectralLightningBigV1
	 90 SpectralLightningBigV2
	 91 Sentinel
	 92 Stalker
	 93 Inquisitor
	 94 InquisitorArm
	 95 Programmer
	 96 ProgrammerBase
	 97 LoreShot
	 98 LoreShot2
	 99 MiniMissile
    100 CrusaderMissile
	101 BishopMissile
    102 ElectricBolt
	103 PoisonBolt
	104 SentinelFX1
	105 SentinelFX2
	106 HEGrenade
	107 PhosphorousGrenade
	108 InquisitorShot
	109 PhosphorousFire
	110 MaulerTorpedo
	111 MaulerTorpedoWave
	112 FlameMissile
	113 FastFlameMissile
	114 MaulerPuff
	115 StrifePuff
    116 StrifeSpark
	117 Blood
	118 TeleportFog
	119 ItemFog
	120 --- Doomednum is 14, which makes it a teleport destination. Don't know why it's in the mobjinfo table.
	121 KlaxonWarningLight
	122 CeilingTurret
	123 Piston
	124 Computer
	125 MedPatch
	126 MedicalKit
	127 SurgeryKit
	128 DegninOre
	129 MetalArmor
	130 LeatherArmor
	131 WaterBottle
	132 Mug
	133 BaseKey
	134 GovsKey
	135 Passcard
	136 IDBadge
	137 PrisonKey
	138 SeveredHand
	139 Power1Key
	140 Power2Key
	141 Power3Key
	142 GoldKey
	143 IDCard
	144 SilverKey
	145 OracleKey
	146 MilitaryID
	147 OrderKey
	148 WarehouseKey
	149 BrassKey
	150 RedCrystalKey
	151 BlueCrystalKey
	152 ChapelKey
	153 CatacombKey
	154 SecurityKey
	155 CoreKey
	156 MaulerKey
	157 FactoryKey
	158 MineKey
	159 NewKey5
	160 ShadowArmor
	161 EnvironmentalSuit
	162 GuardUniform
	163 OfficersUniform
	164 StrifeMap
	165 Scanner
	166
	167 Targeter
	168 Coin
	169 Gold10
	170 Gold25
	171 Gold50
	172 Gold300
	173 BeldinsRing
	174 OfferingChalice
	175 Ear
	176 Communicator
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
	191 FlameThrowerParts
	192 MiniMissileLauncher
	193 Mauler
	194 StrifeCrossbow
	195 StrifeGrenadeLauncher
	196 Sigil1
	197 Sigil2
	198 Sigil3
	199 Sigil4
	200 Sigil5
	201 PowerCrystal
	202 RatBuddy
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
	230 DeadCrusader
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
	289 BrokenPowerCoupling
	290 AlienBubbleColumn
	291 AlienFloorBubble
	292 AlienCeilingBubble
	293 AlienAspClimber
	294 AlienSpiderLight
	295 Meat
	296 Junk
	297 FireDroplet
	298 AmmoFillup
	299 HealthFillup
	300 Info
	301 RaiseAlarm
	302 OpenDoor222
	303 CloseDoor222
	304 PrisonPass
	305 OpenDoor224
	306 UpgradeStamina
	307 UpgradeAccuracy
	308 InterrogatorReport (seems to be unused)
	309 HealthTraining
	310 GunTraining
	311 OraclePass
	312 QuestItem1
	313 QuestItem2
	314 QuestItem3
	315 QuestItem4
	316 QuestItem5
	317 QuestItem6
	318 QuestItem7
	319 QuestItem8
	320 QuestItem9
	321 QuestItem10
	322 QuestItem11
	323 QuestItem12
	324 QuestItem13
	325 QuestItem14
	326 QuestItem15
	327 QuestItem16
	328 QuestItem17
	329 QuestItem18
	330 QuestItem19
	331 QuestItem20
	332 QuestItem21
	333 QuestItem22
	334 QuestItem23
	335 QuestItem24
	336 QuestItem25
	337 QuestItem26
	338 QuestItem27
	339 QuestItem28
	340 QuestItem29
	341 QuestItem30
	342 QuestItem31
	343 SlideshowStarter
*/	

static FRandom pr_gibtosser ("GibTosser");

void A_TossGib (AActor *);
void A_LoopActiveSound (AActor *);
void A_FLoopActiveSound (AActor *);
void A_Countdown (AActor *);
void A_XXScream (AActor *);
void A_SentinelRefire (AActor *);

// Force Field Guard --------------------------------------------------------

void A_RemoveForceField (AActor *);

class AForceFieldGuard : public AActor
{
	DECLARE_ACTOR (AForceFieldGuard, AActor)
public:
	int TakeSpecialDamage (AActor *inflictor, AActor *source, int damage);
};

FState AForceFieldGuard::States[] =
{
	S_NORMAL (TOKN, 'A', -1, NULL,					NULL),
	S_NORMAL (XPRK, 'A',  1, A_RemoveForceField,	NULL)
};

IMPLEMENT_ACTOR (AForceFieldGuard, Strife, 25, 0)
	PROP_StrifeType (0)
	PROP_SpawnHealth (10)
	PROP_SpawnState (0)
	PROP_DeathState (1)
	PROP_RadiusFixed (2)
	PROP_HeightFixed (1)
	PROP_Mass (10000)
	PROP_Flags (MF_SHOOTABLE|MF_NOSECTOR)
	PROP_Flags4 (MF4_INCOMBAT)
END_DEFAULTS

int AForceFieldGuard::TakeSpecialDamage (AActor *inflictor, AActor *source, int damage)
{
	if (inflictor == NULL || !inflictor->IsKindOf (RUNTIME_CLASS(ADegninOre)))
	{
		return -1;
	}
	return health;
}

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
	PROP_StrifeType (31)
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
	PROP_StrifeType (32)
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
	PROP_StrifeType (33)
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
	PROP_StrifeType (34)
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
	PROP_StrifeType (35)
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
	PROP_StrifeType (36)
END_DEFAULTS

// Kneeling Guy -------------------------------------------------------------

void A_SetShadow (AActor *self)
{
	self->flags |= MF_STRIFEx8000000|MF_SHADOW;
	self->RenderStyle = STYLE_Translucent;
	self->alpha = HR_SHADOW;
}

void A_ClearShadow (AActor *self)
{
	self->flags &= ~(MF_STRIFEx8000000|MF_SHADOW);
	self->RenderStyle = STYLE_Normal;
	self->alpha = OPAQUE;
}

static FRandom pr_gethurt ("HurtMe!");

void A_GetHurt (AActor *self)
{
	self->flags4 |= MF4_INCOMBAT;
	if ((pr_gethurt() % 5) == 0)
	{
		S_SoundID (self, CHAN_VOICE, self->PainSound, 1, ATTN_NORM);
		self->health--;
	}
	if (self->health <= 0)
	{
		self->Die (self->target, self->target);
	}
}

class AKneelingGuy : public AActor
{
	DECLARE_ACTOR (AKneelingGuy, AActor)
};

FState AKneelingGuy::States[] =
{
#define S_KNEEL	0
	S_NORMAL (NEAL, 'A',   15, A_LoopActiveSound,	&States[S_KNEEL+1]),
	S_NORMAL (NEAL, 'B',   40, A_LoopActiveSound,	&States[S_KNEEL]),

#define S_KNEEL_PAIN (S_KNEEL+2)
	S_NORMAL (NEAL, 'C',	5, A_SetShadow,			&States[S_KNEEL_PAIN+1]),
	S_NORMAL (NEAL, 'B',	4, A_Pain,				&States[S_KNEEL_PAIN+2]),
	S_NORMAL (NEAL, 'C',	5, A_ClearShadow,		&States[S_KNEEL]),

#define S_KNEEL_HURT (S_KNEEL_PAIN+3)
	S_NORMAL (NEAL, 'B',	6, NULL,				&States[S_KNEEL_HURT+1]),
	S_NORMAL (NEAL, 'C',   13, A_GetHurt,			&States[S_KNEEL_HURT]),

#define S_KNEEL_DIE (S_KNEEL_HURT+2)
	S_NORMAL (NEAL, 'D',	5, NULL,				&States[S_KNEEL_DIE+1]),
	S_NORMAL (NEAL, 'E',	5, A_Scream,			&States[S_KNEEL_DIE+2]),
	S_NORMAL (NEAL, 'F',	6, NULL,				&States[S_KNEEL_DIE+3]),
	S_NORMAL (NEAL, 'G',	5, A_NoBlocking,		&States[S_KNEEL_DIE+4]),
	S_NORMAL (NEAL,	'H',	5, NULL,				&States[S_KNEEL_DIE+5]),
	S_NORMAL (NEAL, 'I',	6, NULL,				&States[S_KNEEL_DIE+6]),
	S_NORMAL (NEAL, 'J',   -1, NULL,				NULL)
};

IMPLEMENT_ACTOR (AKneelingGuy, Strife, 204, 0)
	PROP_SpawnState (S_KNEEL)
	PROP_SeeState (S_KNEEL)
	PROP_PainState (S_KNEEL_PAIN)
	PROP_WoundState (S_KNEEL_HURT)
	PROP_DeathState (S_KNEEL_DIE)

	PROP_SpawnHealth (51)
	PROP_PainChance (255)
	PROP_RadiusFixed (6)
	PROP_HeightFixed (6)
	PROP_MassLong (50000)
	PROP_Flags (MF_SOLID|MF_SHOOTABLE|MF_NOBLOOD|MF_COUNTKILL)
	PROP_Flags4 (MF4_INCOMBAT)
	PROP_MinMissileChance (150)
	PROP_StrifeType (37)

	PROP_PainSound ("misc/static")
	PROP_DeathSound ("misc/static")
	PROP_ActiveSound ("misc/chant")
END_DEFAULTS

// Klaxon Warning Light -----------------------------------------------------

void A_TurretLook (AActor *self)
{
	AActor *target;

	self->threshold = 0;
	target = self->LastHeard;
	if (target != NULL &&
		target->health > 0 &&
		target->flags & MF_SHOOTABLE &&
		(self->flags & MF_FRIENDLY) != (target->flags & MF_FRIENDLY))
	{
		self->target = target;
		if ((self->flags & MF_AMBUSH) && !P_CheckSight (self, target))
		{
			return;
		}
		if (self->SeeSound != 0)
		{
			S_SoundID (self, CHAN_VOICE, self->SeeSound, 1, ATTN_NORM);
		}
		self->LastHeard = NULL;
		self->threshold = 10;
		self->SetState (self->SeeState);
	}
}

void A_KlaxonBlare (AActor *self)
{
	if (--self->reactiontime < 0)
	{
		self->target = NULL;
		self->reactiontime = self->GetDefault()->reactiontime;
		A_TurretLook (self);
		if (self->target == NULL)
		{
			self->SetState (self->SpawnState);
		}
		else
		{
			self->reactiontime = 50;
		}
	}
	if (self->reactiontime == 2)
	{
		for (AActor *actor = self->Sector->thinglist; actor != NULL; actor = actor->snext)
		{
			actor->LastHeard = NULL;
		}
	}
	else if (self->reactiontime > 50)
	{
		S_Sound (self, CHAN_VOICE, "misc/alarm", 1, ATTN_NORM);
	}
}

class AKlaxonWarningLight : public AActor
{
	DECLARE_ACTOR (AKlaxonWarningLight, AActor)
};

FState AKlaxonWarningLight::States[] =
{
	S_NORMAL (KLAX, 'A',  5, A_TurretLook,	&States[0]),

	S_NORMAL (KLAX, 'B',  6, A_KlaxonBlare,	&States[2]),
	S_NORMAL (KLAX, 'C', 60, NULL,			&States[1])
};

IMPLEMENT_ACTOR (AKlaxonWarningLight, Strife, 24, 0)
	PROP_SpawnState (0)
	PROP_SeeState (1)
	PROP_ReactionTime (60)
	PROP_Flags (MF_NOBLOCKMAP|MF_AMBUSH|MF_SPAWNCEILING|MF_NOGRAVITY)
	PROP_Flags4 (MF4_FIXMAPTHINGPOS|MF4_NOSPLASHALERT|MF4_SYNCHRONIZED)
	PROP_StrifeType (121)
END_DEFAULTS

// CeilingTurret ------------------------------------------------------------

void A_ShootGun (AActor *);

class ACeilingTurret : public AActor
{
	DECLARE_ACTOR (ACeilingTurret, AActor)
};

FState ACeilingTurret::States[] =
{
	S_NORMAL (TURT, 'A',  5, A_TurretLook,		&States[0]),

	S_NORMAL (TURT, 'A',  2, A_Chase,			&States[1]),

	S_NORMAL (TURT, 'B',  4, A_ShootGun,		&States[3]),
	S_NORMAL (TURT, 'D',  3, A_SentinelRefire,	&States[4]),
	S_NORMAL (TURT, 'A',  4, A_SentinelRefire,  &States[2]),

	S_BRIGHT (BALL, 'A',  6, A_Scream,			&States[6]),
	S_BRIGHT (BALL, 'B',  6, NULL,				&States[7]),
	S_BRIGHT (BALL, 'C',  6, NULL,				&States[8]),
	S_BRIGHT (BALL, 'D',  6, NULL,				&States[9]),
	S_BRIGHT (BALL, 'E',  6, NULL,				&States[10]),
	S_NORMAL (TURT, 'C', -1, NULL,				NULL)
};

IMPLEMENT_ACTOR (ACeilingTurret, Strife, 27, 0)
	PROP_StrifeType (122)
	PROP_SpawnHealth (125)
	PROP_SpawnState (0)
	PROP_SeeState (1)
	PROP_PainState (2)
	PROP_MissileState (2)
	PROP_DeathState (5)
	PROP_SpeedFixed (0)
	PROP_PainChance (0)
	PROP_MassLong (10000000)
	PROP_Flags (MF_SHOOTABLE|MF_AMBUSH|MF_SPAWNCEILING|MF_NOGRAVITY|
				MF_NOBLOOD|MF_COUNTKILL)
	PROP_Flags4 (MF4_NOSPLASHALERT|MF4_DONTFALL)
	PROP_MinMissileChance (150)
	PROP_DeathSound ("turret/death")
END_DEFAULTS

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
	PROP_StrifeType (131)
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
	PROP_StrifeType (132)
END_DEFAULTS

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
	PROP_Flags (MF_SOLID|MF_SHOOTABLE|MF_NOBLOOD)
	PROP_Flags4 (MF4_INCOMBAT)
	PROP_StrifeType (203)
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
	S_NORMAL (BART, 'L', -1, NULL,			NULL),
	
};

IMPLEMENT_ACTOR (AExplosiveBarrel2, Strife, 94, 0)
	PROP_SpawnState (0)
	PROP_SpawnHealth (30)
	PROP_DeathState (1)
	PROP_RadiusFixed (10)
	PROP_HeightFixed (32)
	PROP_StrifeType (204)
	//PROP_Flags (MF_SOLID|MF_SHOOTABLE|MF_NOBLOOD)
	PROP_Flags4 (MF4_INCOMBAT)
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
	PROP_Flags (MF_SOLID|MF_SHOOTABLE|MF_NOBLOOD)
	PROP_Flags4 (MF4_INCOMBAT)
	PROP_StrifeType (205)
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
	PROP_StrifeType (206)
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
	PROP_StrifeType (207)
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
	PROP_StrifeType (208)
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
	PROP_StrifeType (209)
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
	PROP_StrifeType (210)
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
	PROP_StrifeType (211)
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
	PROP_StrifeType (212)
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
	PROP_StrifeType (213)
END_DEFAULTS

// Huge Tech Pillar ---------------------------------------------------------

class APillarHugeTech : public AActor
{
	DECLARE_ACTOR (APillarHugeTech, AActor)
};

// This was defined while compiling on Linux.
// I don't know where it came from.
#ifdef HUGE
#undef HUGE
#endif

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
	PROP_StrifeType (214)
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
	PROP_StrifeType (215)
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
	PROP_StrifeType (216)
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
	PROP_StrifeType (217)
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
	PROP_StrifeType (218)
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
	PROP_StrifeType (219)
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
	PROP_StrifeType (220)
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
	PROP_StrifeType (221)
END_DEFAULTS

// Candle -------------------------------------------------------------------

class ACandle : public AActor
{
	DECLARE_ACTOR (ACandle, AActor)
};

FState ACandle::States[] =
{
	S_BRIGHT (KNDL, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (ACandle, Strife, 34, 0)
	PROP_SpawnState (0)
	PROP_StrifeType (222)
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
	PROP_StrifeType (223)
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
	PROP_StrifeType (224)
	PROP_ActiveSound ("world/waterdrips")
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
	PROP_StrifeType (225)
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
	PROP_StrifeType (226)
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
	PROP_StrifeType (227)
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
	PROP_StrifeType (228)
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
	PROP_Alpha (TRANSLUC25)
	PROP_StrifeType (229)
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
	PROP_StrifeType (231)
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
	PROP_StrifeType (232)
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
	PROP_StrifeType (233)
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
	PROP_StrifeType (234)
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
	PROP_StrifeType (235)
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
	PROP_StrifeType (236)
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
	PROP_StrifeType (237)
END_DEFAULTS

// Burning Barrel -----------------------------------------------------------

class AStrifeBurningBarrel : public AActor
{
	DECLARE_ACTOR (AStrifeBurningBarrel, AActor)
};

FState AStrifeBurningBarrel::States[] =
{
	S_BRIGHT (BBAR, 'A', 4, NULL, &States[1]),
	S_BRIGHT (BBAR, 'B', 4, NULL, &States[2]),
	S_BRIGHT (BBAR, 'C', 4, NULL, &States[3]),
	S_BRIGHT (BBAR, 'D', 4, NULL, &States[0]),
};

IMPLEMENT_ACTOR (AStrifeBurningBarrel, Strife, 70, 0)
	PROP_SpawnState (0)
	PROP_RadiusFixed (16)
	PROP_HeightFixed (48)
	PROP_Flags (MF_SOLID)
	PROP_StrifeType (238)
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
	PROP_StrifeType (239)
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
	PROP_StrifeType (240)
	PROP_ActiveSound ("world/smallfire")
END_DEFAULTS

// Small Torch (Lit) --------------------------------------------------------

class ASmallTorchLit : public AActor
{
	DECLARE_ACTOR (ASmallTorchLit, AActor)
};

FState ASmallTorchLit::States[] =
{
	S_BRIGHT (TRHL, 'A', 4, NULL, &States[1]),
	S_BRIGHT (TRHL, 'B', 4, NULL, &States[2]),
	S_BRIGHT (TRHL, 'C', 4, NULL, &States[3]),
	S_BRIGHT (TRHL, 'D', 4, NULL, &States[0])
};

IMPLEMENT_ACTOR (ASmallTorchLit, Strife, 107, 0)
	PROP_SpawnState (0)
	PROP_Radius (4)			// This is intentionally not PROP_RadiusFixed
	PROP_HeightFixed (16)
	PROP_Flags (MF_NOBLOCKMAP)
	PROP_Flags4 (MF4_FIXMAPTHINGPOS)
	PROP_StrifeType (241)

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
	PROP_StrifeType (242)
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
	PROP_StrifeType (243)
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
	PROP_StrifeType (244)
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
	PROP_StrifeType (245)
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
	PROP_StrifeType (246)
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
	PROP_StrifeType (247)
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
	PROP_StrifeType (248)
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
	PROP_StrifeType (249)
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
	PROP_StrifeType (250)
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
	PROP_StrifeType (251)
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
	PROP_StrifeType (252)
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
	PROP_StrifeType (253)
END_DEFAULTS

// Stick in Water -----------------------------------------------------------

class AStickInWater : public AActor
{
	DECLARE_ACTOR (AStickInWater, AActor)
};

FState AStickInWater::States[] =
{
	S_NORMAL (LOGW, 'A', 5, A_LoopActiveSound, &States[1]),
	S_NORMAL (LOGW, 'B', 5, A_LoopActiveSound, &States[2]),
	S_NORMAL (LOGW, 'C', 5, A_LoopActiveSound, &States[3]),
	S_NORMAL (LOGW, 'D', 5, A_LoopActiveSound, &States[0])
};

IMPLEMENT_ACTOR (AStickInWater, Strife, 215, 0)
	PROP_SpawnState (0)
	PROP_Flags (MF_NOBLOCKMAP)
	PROP_StrifeType (254)
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
	PROP_StrifeType (255)
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
	PROP_StrifeType (256)
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
	PROP_StrifeType (257)
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
	PROP_StrifeType (258)
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
	PROP_StrifeType (259)
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
	PROP_StrifeType (260)
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
	PROP_StrifeType (261)
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
	PROP_StrifeType (262)
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
	PROP_StrifeType (263)
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
	PROP_StrifeType (264)
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
	PROP_StrifeType (265)
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
	PROP_StrifeType (266)
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
	PROP_StrifeType (267)
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
	PROP_StrifeType (268)
END_DEFAULTS

// Tree Stub ----------------------------------------------------------------

class ATreeStub : public AActor
{
	DECLARE_ACTOR (ATreeStub, AActor)
};

FState ATreeStub::States[] =
{
	S_NORMAL (TRET, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (ATreeStub, Strife, 33, 0)
	PROP_SpawnState (0)
	PROP_RadiusFixed (15)
	PROP_HeightFixed (80)
	PROP_Flags (MF_SOLID)
	PROP_StrifeType (269)
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
	PROP_StrifeType (270)
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
	PROP_StrifeType (271)
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
	PROP_StrifeType (272)
END_DEFAULTS

// Barricade Column ---------------------------------------------------------

class ABarricadeColumn : public AActor
{
	DECLARE_ACTOR (ABarricadeColumn, AActor)
};

FState ABarricadeColumn::States[] =
{
	S_NORMAL (BARC, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (ABarricadeColumn, Strife, 69, 0)
	PROP_SpawnState (0)
	PROP_RadiusFixed (16)
	PROP_HeightFixed (128)
	PROP_Flags (MF_SOLID)
	PROP_StrifeType (273)
END_DEFAULTS

// Pot ----------------------------------------------------------------------

class APot : public AActor
{
	DECLARE_ACTOR (APot, AActor)
};

FState APot::States[] =
{
	S_NORMAL (VAZE, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (APot, Strife, 165, 0)
	PROP_SpawnState (0)
	PROP_RadiusFixed (12)
	PROP_HeightFixed (24)
	PROP_Flags (MF_SOLID)
	PROP_StrifeType (274)
END_DEFAULTS

// Pitcher ------------------------------------------------------------------

class APitcher : public AActor
{
	DECLARE_ACTOR (APitcher, AActor)
};

FState APitcher::States[] =
{
	S_NORMAL (VAZE, 'B', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (APitcher, Strife, 188, 0)
	PROP_SpawnState (0)
	PROP_RadiusFixed (12)
	PROP_HeightFixed (32)
	PROP_Flags (MF_SOLID)
	PROP_StrifeType (275)
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
	PROP_StrifeType (276)
END_DEFAULTS

// Metal Pot ----------------------------------------------------------------

class AMetalPot : public AActor
{
	DECLARE_ACTOR (AMetalPot, AActor)
};

FState AMetalPot::States[] =
{
	S_NORMAL (MPOT, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (AMetalPot, Strife, 190, 0)
	PROP_SpawnState (0)
	PROP_Flags (MF_NOBLOCKMAP)
	PROP_StrifeType (277)
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
	PROP_StrifeType (278)
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
	PROP_StrifeType (279)
END_DEFAULTS

// Silver Tech Lamp ----------------------------------------------------------

class ATechLampSilver : public AActor
{
	DECLARE_ACTOR (ATechLampSilver, AActor)
};

FState ATechLampSilver::States[] =
{
	S_NORMAL (TECH, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (ATechLampSilver, Strife, 196, 0)
	PROP_SpawnState (0)
	PROP_RadiusFixed (11)
	PROP_HeightFixed (64)
	PROP_Flags (MF_SOLID)
	PROP_StrifeType (280)
END_DEFAULTS

// Brass Tech Lamp ----------------------------------------------------------

class ATechLampBrass : public AActor
{
	DECLARE_ACTOR (ATechLampBrass, AActor)
};

FState ATechLampBrass::States[] =
{
	S_NORMAL (TECH, 'B', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (ATechLampBrass, Strife, 197, 0)
	PROP_SpawnState (0)
	PROP_RadiusFixed (8)
	PROP_HeightFixed (64)
	PROP_Flags (MF_SOLID)
	PROP_StrifeType (281)
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
	PROP_StrifeType (282)
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
	PROP_StrifeType (283)
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
	PROP_StrifeType (284)
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
	PROP_StrifeType (285)
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
	PROP_StrifeType (286)
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
	PROP_StrifeType (287)
END_DEFAULTS

// Power Coupling -----------------------------------------------------------

class APowerCoupling : public AActor
{
	DECLARE_ACTOR (APowerCoupling, AActor)
public:
	void Die (AActor *source, AActor *inflictor);
};

FState APowerCoupling::States[] =
{
	S_NORMAL (COUP, 'A', 5, NULL, &States[1]),
	S_NORMAL (COUP, 'B', 5, NULL, &States[0]),
};

IMPLEMENT_ACTOR (APowerCoupling, Strife, 220, 0)
	PROP_SpawnState (0)
	PROP_SpawnHealth (40)
	PROP_RadiusFixed (17)
	PROP_HeightFixed (64)
	PROP_MassLong (999999)
	PROP_Flags (MF_SOLID|MF_SHOOTABLE|MF_DROPPED|MF_NOBLOOD|MF_NOTDMATCH)
	PROP_Flags4 (MF4_INCOMBAT)
	PROP_StrifeType (288)
END_DEFAULTS

void APowerCoupling::Die (AActor *source, AActor *inflictor)
{
	Super::Die (source, inflictor);

	int i;

	for (i = 0; i < MAXPLAYERS; ++i)
		if (playeringame[i] && players[i].health > 0)
			break;

	if (i == MAXPLAYERS)
		return;

	// [RH] In case the player broke it with the dagger, alert the guards now.
	if (LastHeard != source)
	{
		P_NoiseAlert (source, this);
	}
	EV_DoDoor (DDoor::doorClose, NULL, players[i].mo, 225, 2*FRACUNIT, 0, 0, 0);
	EV_DoFloor (DFloor::floorLowerToHighest, NULL, 44, FRACUNIT, 0, 0, 0);
	players[i].mo->GiveInventoryType (QuestItemClasses[5]);
	S_Sound (CHAN_VOICE, "svox/voc13", 1, ATTN_NORM);
	players[i].SetLogNumber (13);
	P_DropItem (this, "BrokenPowerCoupling", -1, 256);
}

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
	PROP_StrifeType (290)
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
	PROP_StrifeType (291)
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
	PROP_StrifeType (292)
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
	PROP_StrifeType (293)
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
	PROP_StrifeType (294)
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

void A_LoopActiveSound (AActor *self)
{
	if (self->ActiveSound != 0 && !S_IsActorPlayingSomething (self, CHAN_VOICE))
	{
		S_LoopedSoundID (self, CHAN_VOICE, self->ActiveSound, 1, ATTN_NORM);
	}
}

// Fire Droplet -------------------------------------------------------------

class AFireDroplet : public AActor
{
	DECLARE_ACTOR (AFireDroplet, AActor)
};

// [RH] I think these should be bright, even though they weren't in Strife.
FState AFireDroplet::States[] =
{
	S_BRIGHT (FFOT, 'A', 9, NULL, &States[1]),
	S_BRIGHT (FFOT, 'B', 9, NULL, &States[2]),
	S_BRIGHT (FFOT, 'C', 9, NULL, &States[3]),
	S_BRIGHT (FFOT, 'D', 9, NULL, NULL)
};

IMPLEMENT_ACTOR (AFireDroplet, Strife, -1, 0)
	PROP_StrifeType (297)
	PROP_SpawnState (0)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOCLIP)
END_DEFAULTS

// Humanoid Base Class ------------------------------------------------------

void A_ItBurnsItBurns (AActor *);
void A_DropFire (AActor *);

FState AStrifeHumanoid::States[] =
{
#define S_FIREHANDS 0
	S_BRIGHT (WAVE, 'A', 3, NULL,				&States[S_FIREHANDS+1]),
	S_BRIGHT (WAVE, 'B', 3, NULL,				&States[S_FIREHANDS+2]),
	S_BRIGHT (WAVE, 'C', 3, NULL,				&States[S_FIREHANDS+3]),
	S_BRIGHT (WAVE, 'D', 3, NULL,				&States[S_FIREHANDS]),

	// [RH] These weren't bright in Strife, but I think they should be.
	// (After all, they are now a light source.)
#define S_HUMAN_BURNDEATH (S_FIREHANDS+4)
	S_BRIGHT (BURN, 'A', 3, A_ItBurnsItBurns,	&States[S_HUMAN_BURNDEATH+1]),
	S_BRIGHT (BURN, 'B', 3, A_DropFire,			&States[S_HUMAN_BURNDEATH+2]),
	S_BRIGHT (BURN, 'C', 3, A_Wander,			&States[S_HUMAN_BURNDEATH+3]),
	S_BRIGHT (BURN, 'D', 3, A_NoBlocking,		&States[S_HUMAN_BURNDEATH+4]),
	S_BRIGHT (BURN, 'E', 5, A_DropFire,			&States[S_HUMAN_BURNDEATH+5]),
	S_BRIGHT (BURN, 'F', 5, A_Wander,			&States[S_HUMAN_BURNDEATH+6]),
	S_BRIGHT (BURN, 'G', 5, A_Wander,			&States[S_HUMAN_BURNDEATH+7]),
	S_BRIGHT (BURN, 'H', 5, A_Wander,			&States[S_HUMAN_BURNDEATH+8]),
	S_BRIGHT (BURN, 'I', 5, A_DropFire,			&States[S_HUMAN_BURNDEATH+9]),
	S_BRIGHT (BURN, 'J', 5, A_Wander,			&States[S_HUMAN_BURNDEATH+10]),
	S_BRIGHT (BURN, 'K', 5, A_Wander,			&States[S_HUMAN_BURNDEATH+11]),
	S_BRIGHT (BURN, 'L', 5, A_Wander,			&States[S_HUMAN_BURNDEATH+12]),
	S_BRIGHT (BURN, 'M', 3, A_DropFire,			&States[S_HUMAN_BURNDEATH+13]),
	S_BRIGHT (BURN, 'N', 3, NULL,				&States[S_HUMAN_BURNDEATH+14]),
	S_BRIGHT (BURN, 'O', 5, NULL,				&States[S_HUMAN_BURNDEATH+15]),
	S_BRIGHT (BURN, 'P', 5, NULL,				&States[S_HUMAN_BURNDEATH+16]),
	S_BRIGHT (BURN, 'Q', 5, NULL,				&States[S_HUMAN_BURNDEATH+17]),
	S_BRIGHT (BURN, 'P', 5, NULL,				&States[S_HUMAN_BURNDEATH+18]),
	S_BRIGHT (BURN, 'Q', 5, NULL,				&States[S_HUMAN_BURNDEATH+19]),
	S_BRIGHT (BURN, 'R', 7, NULL,				&States[S_HUMAN_BURNDEATH+20]),
	S_BRIGHT (BURN, 'S', 7, NULL,				&States[S_HUMAN_BURNDEATH+21]),
	S_BRIGHT (BURN, 'T', 7, NULL,				&States[S_HUMAN_BURNDEATH+22]),
	S_BRIGHT (BURN, 'U', 7, NULL,				&States[S_HUMAN_BURNDEATH+23]),
	S_BRIGHT (BURN, 'V',700,NULL,				NULL),

#define S_HUMAN_ZAPDEATH (S_HUMAN_BURNDEATH+24)
	S_NORMAL (DISR, 'A', 5, NULL,				&States[S_HUMAN_ZAPDEATH+1]),
	S_NORMAL (DISR, 'B', 5, NULL,				&States[S_HUMAN_ZAPDEATH+2]),
	S_NORMAL (DISR, 'C', 5, NULL,				&States[S_HUMAN_ZAPDEATH+3]),
	S_NORMAL (DISR, 'D', 5, A_NoBlocking,		&States[S_HUMAN_ZAPDEATH+4]),
	S_NORMAL (DISR, 'E', 5, NULL,				&States[S_HUMAN_ZAPDEATH+5]),
	S_NORMAL (DISR, 'F', 5, NULL,				&States[S_HUMAN_ZAPDEATH+6]),
	S_NORMAL (DISR, 'G', 4, NULL,				&States[S_HUMAN_ZAPDEATH+7]),
	S_NORMAL (DISR, 'H', 4, NULL,				&States[S_HUMAN_ZAPDEATH+8]),
	S_NORMAL (DISR, 'I', 4, NULL,				&States[S_HUMAN_ZAPDEATH+9]),
	S_NORMAL (DISR, 'J', 4, NULL,				&States[S_HUMAN_ZAPDEATH+10]),
	S_NORMAL (MEAT, 'D',700,NULL,				NULL)
};

IMPLEMENT_ACTOR (AStrifeHumanoid, Any, -1, 0)
	PROP_BDeathState (S_HUMAN_BURNDEATH)
	PROP_EDeathState (S_HUMAN_ZAPDEATH)
END_DEFAULTS

void A_ItBurnsItBurns (AActor *self)
{
	int burnsound = S_FindSound ("human/imonfire");
	if (burnsound != 0)
	{
		self->DeathSound = burnsound;
	}
	A_Scream (self);
	if (self->player != NULL)
	{
		P_SetPsprite (self->player, ps_weapon, &AStrifeHumanoid::States[S_FIREHANDS]);
		P_SetPsprite (self->player, ps_flash, NULL);
	}
}

void A_DropFire (AActor *self)
{
	AActor *drop = Spawn<AFireDroplet> (self->x, self->y, self->z + 24*FRACUNIT);
	drop->momz = -FRACUNIT;
	P_RadiusAttack (self, self, 64, 64, false, 0);
}
