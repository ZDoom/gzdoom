#include "actor.h"
#include "g_level.h"
#include "gi.h"
#include "m_random.h"
#include "s_sound.h"
#include "d_player.h"
#include "a_action.h"
#include "p_local.h"
#include "a_strifeglobal.h"
#include "p_enemy.h"
#include "p_lnspec.h"
#include "c_console.h"
#include "thingdef/thingdef.h"
#include "doomstat.h"
#include "gstrings.h"
#include "a_keys.h"
#include "a_sharedglobal.h"
#include "templates.h"
#include "d_event.h"
#include "v_font.h"
#include "farchive.h"

// Include all the other Strife stuff here to reduce compile time
#include "a_acolyte.cpp"
#include "a_spectral.cpp"
#include "a_alienspectres.cpp"
#include "a_coin.cpp"
#include "a_crusader.cpp"
#include "a_entityboss.cpp"
#include "a_inquisitor.cpp"
#include "a_loremaster.cpp"
//#include "a_macil.cpp"
#include "a_oracle.cpp"
#include "a_programmer.cpp"
#include "a_reaver.cpp"
#include "a_rebels.cpp"
#include "a_sentinel.cpp"
#include "a_stalker.cpp"
#include "a_strifeitems.cpp"
#include "a_strifeweapons.cpp"
#include "a_templar.cpp"
#include "a_thingstoblowup.cpp"

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
	120 teleport destination
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

// Force Field Guard --------------------------------------------------------

void A_RemoveForceField (AActor *);

class AForceFieldGuard : public AActor
{
	DECLARE_CLASS (AForceFieldGuard, AActor)
public:
	int TakeSpecialDamage (AActor *inflictor, AActor *source, int damage, FName damagetype);
};

IMPLEMENT_CLASS (AForceFieldGuard)

int AForceFieldGuard::TakeSpecialDamage (AActor *inflictor, AActor *source, int damage, FName damagetype)
{
	if (inflictor == NULL || !inflictor->IsKindOf (RUNTIME_CLASS(ADegninOre)))
	{
		return -1;
	}
	return health;
}

// Kneeling Guy -------------------------------------------------------------

DEFINE_ACTION_FUNCTION(AActor, A_SetShadow)
{
	self->flags |= MF_STRIFEx8000000|MF_SHADOW;
	self->RenderStyle = STYLE_Translucent;
	self->alpha = HR_SHADOW;
}

DEFINE_ACTION_FUNCTION(AActor, A_ClearShadow)
{
	self->flags &= ~(MF_STRIFEx8000000|MF_SHADOW);
	self->RenderStyle = STYLE_Normal;
	self->alpha = OPAQUE;
}

static FRandom pr_gethurt ("HurtMe!");

DEFINE_ACTION_FUNCTION(AActor, A_GetHurt)
{
	self->flags4 |= MF4_INCOMBAT;
	if ((pr_gethurt() % 5) == 0)
	{
		S_Sound (self, CHAN_VOICE, self->PainSound, 1, ATTN_NORM);
		self->health--;
	}
	if (self->health <= 0)
	{
		self->Die (self->target, self->target);
	}
}

// Klaxon Warning Light -----------------------------------------------------

DEFINE_ACTION_FUNCTION(AActor, A_TurretLook)
{
	AActor *target;

	if (self->flags5 & MF5_INCONVERSATION)
		return;

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
			S_Sound (self, CHAN_VOICE, self->SeeSound, 1, ATTN_NORM);
		}
		self->LastHeard = NULL;
		self->threshold = 10;
		self->SetState (self->SeeState);
	}
}

DEFINE_ACTION_FUNCTION(AActor, A_KlaxonBlare)
{
	if (--self->reactiontime < 0)
	{
		self->target = NULL;
		self->reactiontime = self->GetDefault()->reactiontime;
		CALL_ACTION(A_TurretLook, self);
		if (self->target == NULL)
		{
			self->SetIdle();
		}
		else
		{
			self->reactiontime = 50;
		}
	}
	if (self->reactiontime == 2)
	{
		// [RH] Unalert monsters near the alarm and not just those in the same sector as it.
		P_NoiseAlert (NULL, self, false);
	}
	else if (self->reactiontime > 50)
	{
		S_Sound (self, CHAN_VOICE, "misc/alarm", 1, ATTN_NORM);
	}
}

// Power Coupling -----------------------------------------------------------

class APowerCoupling : public AActor
{
	DECLARE_CLASS (APowerCoupling, AActor)
public:
	void Die (AActor *source, AActor *inflictor, int dmgflags);
};

IMPLEMENT_CLASS (APowerCoupling)

void APowerCoupling::Die (AActor *source, AActor *inflictor, int dmgflags)
{
	Super::Die (source, inflictor, dmgflags);

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
	EV_DoFloor (DFloor::floorLowerToHighest, NULL, 44, FRACUNIT, 0, -1, 0, false);
	players[i].mo->GiveInventoryType (QuestItemClasses[5]);
	S_Sound (CHAN_VOICE, "svox/voc13", 1, ATTN_NORM);
	players[i].SetLogNumber (13);
	P_DropItem (this, PClass::FindClass("BrokenPowerCoupling"), -1, 256);
	Destroy ();
}

// Gibs for things that bleed -----------------------------------------------

class AMeat : public AActor
{
	DECLARE_CLASS (AMeat, AActor)
public:
	void BeginPlay ()
	{
		// Strife used mod 19, but there are 20 states. Hmm.
		SetState (SpawnState + pr_gibtosser() % 20);
	}
};

IMPLEMENT_CLASS (AMeat)

//==========================================================================
//
// A_TossGib
//
//==========================================================================

DEFINE_ACTION_FUNCTION(AActor, A_TossGib)
{
	const char *gibtype = (self->flags & MF_NOBLOOD) ? "Junk" : "Meat";
	AActor *gib = Spawn (gibtype, self->PosPlusZ(24*FRACUNIT), ALLOW_REPLACE);
	angle_t an;
	int speed;

	if (gib == NULL)
	{
		return;
	}

	an = pr_gibtosser() << 24;
	gib->angle = an;
	speed = pr_gibtosser() & 15;
	gib->velx = speed * finecosine[an >> ANGLETOFINESHIFT];
	gib->vely = speed * finesine[an >> ANGLETOFINESHIFT];
	gib->velz = (pr_gibtosser() & 15) << FRACBITS;
}

//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_FLoopActiveSound)
{
	if (self->ActiveSound != 0 && !(level.time & 7))
	{
		S_Sound (self, CHAN_VOICE, self->ActiveSound, 1, ATTN_NORM);
	}
}

DEFINE_ACTION_FUNCTION(AActor, A_Countdown)
{
	if (--self->reactiontime <= 0)
	{
		P_ExplodeMissile (self, NULL, NULL);
		self->flags &= ~MF_SKULLFLY;
	}
}

DEFINE_ACTION_FUNCTION(AActor, A_LoopActiveSound)
{
	if (self->ActiveSound != 0 && !S_IsActorPlayingSomething (self, CHAN_VOICE, -1))
	{
		S_Sound (self, CHAN_VOICE|CHAN_LOOP, self->ActiveSound, 1, ATTN_NORM);
	}
}

DEFINE_ACTION_FUNCTION(AActor, A_CheckTerrain)
{
	sector_t *sec = self->Sector;

	if (self->Z() == sec->floorplane.ZatPoint(self))
	{
		if (sec->special == Damage_InstantDeath)
		{
			P_DamageMobj (self, NULL, NULL, 999, NAME_InstantDeath);
		}
		else if (sec->special == Scroll_StrifeCurrent)
		{
			int anglespeed = tagManager.GetFirstSectorTag(sec) - 100;
			fixed_t speed = (anglespeed % 10) << (FRACBITS - 4);
			angle_t finean = (anglespeed / 10) << (32-3);
			finean >>= ANGLETOFINESHIFT;
			self->velx += FixedMul (speed, finecosine[finean]);
			self->vely += FixedMul (speed, finesine[finean]);
		}
	}
}

//============================================================================
//
// A_ClearSoundTarget
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_ClearSoundTarget)
{
	AActor *actor;

	self->Sector->SoundTarget = NULL;
	for (actor = self->Sector->thinglist; actor != NULL; actor = actor->snext)
	{
		actor->LastHeard = NULL;
	}
}


DEFINE_ACTION_FUNCTION(AActor, A_ItBurnsItBurns)
{
	S_Sound (self, CHAN_VOICE, "human/imonfire", 1, ATTN_NORM);

	if (self->player != NULL && self->player->mo == self)
	{
		P_SetPsprite (self->player, ps_weapon, self->FindState("FireHands"));
		P_SetPsprite (self->player, ps_flash, NULL);
		self->player->ReadyWeapon = NULL;
		self->player->PendingWeapon = WP_NOCHANGE;
		self->player->playerstate = PST_LIVE;
		self->player->extralight = 3;
	}
}

DEFINE_ACTION_FUNCTION(AActor, A_DropFire)
{
	AActor *drop = Spawn("FireDroplet", self->PosPlusZ(24*FRACUNIT), ALLOW_REPLACE);
	drop->velz = -FRACUNIT;
	P_RadiusAttack (self, self, 64, 64, NAME_Fire, 0);
}

DEFINE_ACTION_FUNCTION(AActor, A_CrispyPlayer)
{
	if (self->player != NULL && self->player->mo == self)
	{
		self->player->playerstate = PST_DEAD;
		P_SetPsprite (self->player, ps_weapon,
			self->player->psprites[ps_weapon].state +
			(self->FindState("FireHandsLower") - self->FindState("FireHands")));
	}
}

DEFINE_ACTION_FUNCTION(AActor, A_HandLower)
{
	if (self->player != NULL)
	{
		pspdef_t *psp = &self->player->psprites[ps_weapon];
		psp->sy += FRACUNIT*9;
		if (psp->sy > WEAPONBOTTOM*2)
		{
			P_SetPsprite (self->player, ps_weapon, NULL);
		}
		if (self->player->extralight > 0) self->player->extralight--;
	}
}

