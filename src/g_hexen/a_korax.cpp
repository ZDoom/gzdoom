//===========================================================================
// Korax Variables
//	tracer		last teleport destination
//	special2	set if "below half" script not yet run
//
// Korax Scripts (reserved)
//	249		Tell scripts that we are below half health
//	250-254	Control scripts (254 is only used when less than half health)
//	255		Death script
//
// Korax TIDs (reserved)
//	245		Reserved for Korax himself
//  248		Initial teleport destination
//	249		Teleport destination
//	250-254	For use in respective control scripts
//	255		For use in death script (spawn spots)
//===========================================================================

#include "actor.h"
#include "info.h"
#include "p_local.h"
#include "p_spec.h"
#include "s_sound.h"
#include "p_enemy.h"
#include "a_action.h"
#include "m_random.h"
#include "i_system.h"

const int KORAX_SPIRIT_LIFETIME = 5*TICRATE/5;	// 5 seconds
const int KORAX_COMMAND_HEIGHT	= 120;
const int KORAX_COMMAND_OFFSET	= 27;

const int KORAX_TID					= 245;
const int KORAX_FIRST_TELEPORT_TID	= 248;
const int KORAX_TELEPORT_TID		= 249;

const int KORAX_DELTAANGLE			= 85*ANGLE_1;
const int KORAX_ARM_EXTENSION_SHORT	= 40;
const int KORAX_ARM_EXTENSION_LONG	= 55;

const int KORAX_ARM1_HEIGHT			= 108*FRACUNIT;
const int KORAX_ARM2_HEIGHT			= 82*FRACUNIT;
const int KORAX_ARM3_HEIGHT			= 54*FRACUNIT;
const int KORAX_ARM4_HEIGHT			= 104*FRACUNIT;
const int KORAX_ARM5_HEIGHT			= 86*FRACUNIT;
const int KORAX_ARM6_HEIGHT			= 53*FRACUNIT;

const int KORAX_BOLT_HEIGHT			= 48*FRACUNIT;
const int KORAX_BOLT_LIFETIME		= 3;



static FRandom pr_koraxchase ("KoraxChase");
static FRandom pr_kspiritinit ("KSpiritInit");
static FRandom pr_koraxdecide ("KoraxDecide");
static FRandom pr_koraxmissile ("KoraxMissile");
static FRandom pr_koraxcommand ("KoraxCommand");
static FRandom pr_kspiritweave ("KSpiritWeave");
static FRandom pr_kspiritseek ("KSpiritSeek");
static FRandom pr_kspiritroam ("KSpiritRoam");
static FRandom pr_missile ("SKoraxMissile");

void A_KoraxChase (AActor *);
void A_KoraxStep (AActor *);
void A_KoraxStep2 (AActor *);
void A_KoraxDecide (AActor *);
void A_KoraxBonePop (AActor *);
void A_KoraxMissile (AActor *);
void A_KoraxCommand (AActor *);
void A_KSpiritRoam (AActor *);
void A_KBolt (AActor *);
void A_KBoltRaise (AActor *);

void KoraxFire (AActor *actor, const PClass *type, int arm);
void KSpiritInit (AActor *spirit, AActor *korax);
AActor *P_SpawnKoraxMissile (fixed_t x, fixed_t y, fixed_t z,
	AActor *source, AActor *dest, const PClass *type);

extern void SpawnSpiritTail (AActor *spirit);

// Korax --------------------------------------------------------------------

class AKorax : public AActor
{
	DECLARE_ACTOR (AKorax, AActor)
};

FState AKorax::States[] =
{
#define S_KORAX_LOOK1 0
	S_NORMAL (KORX, 'A',	5, A_Look				    , &States[S_KORAX_LOOK1]),

#define S_KORAX_CHASE2 (S_KORAX_LOOK1+1)
	S_NORMAL (KORX, 'A',	3, A_KoraxChase			    , &States[S_KORAX_CHASE2+1]),
	S_NORMAL (KORX, 'A',	3, A_KoraxChase			    , &States[S_KORAX_CHASE2+2]),
	S_NORMAL (KORX, 'A',	3, A_KoraxChase			    , &States[S_KORAX_CHASE2+3]),
	S_NORMAL (KORX, 'B',	3, A_KoraxStep			    , &States[S_KORAX_CHASE2+4]),
	S_NORMAL (KORX, 'B',	3, A_KoraxChase			    , &States[S_KORAX_CHASE2+5]),
	S_NORMAL (KORX, 'B',	3, A_KoraxChase			    , &States[S_KORAX_CHASE2+6]),
	S_NORMAL (KORX, 'B',	3, A_KoraxChase			    , &States[S_KORAX_CHASE2+7]),
	S_NORMAL (KORX, 'C',	3, A_KoraxStep2			    , &States[S_KORAX_CHASE2+8]),
	S_NORMAL (KORX, 'C',	3, A_KoraxChase			    , &States[S_KORAX_CHASE2+9]),
	S_NORMAL (KORX, 'C',	3, A_KoraxChase			    , &States[S_KORAX_CHASE2+10]),
	S_NORMAL (KORX, 'C',	3, A_KoraxChase			    , &States[S_KORAX_CHASE2+11]),
	S_NORMAL (KORX, 'D',	3, A_KoraxStep			    , &States[S_KORAX_CHASE2+12]),
	S_NORMAL (KORX, 'D',	3, A_KoraxChase			    , &States[S_KORAX_CHASE2+13]),
	S_NORMAL (KORX, 'D',	3, A_KoraxChase			    , &States[S_KORAX_CHASE2+14]),
	S_NORMAL (KORX, 'D',	3, A_KoraxChase			    , &States[S_KORAX_CHASE2+15]),
	S_NORMAL (KORX, 'A',	3, A_KoraxStep2			    , &States[S_KORAX_CHASE2]),

#define S_KORAX_PAIN1 (S_KORAX_CHASE2+16)
	S_NORMAL (KORX, 'H',	5, A_Pain				    , &States[S_KORAX_PAIN1+1]),
	S_NORMAL (KORX, 'H',	5, NULL					    , &States[S_KORAX_CHASE2]),

#define S_KORAX_ATTACK1 (S_KORAX_PAIN1+2)
	S_BRIGHT (KORX, 'E',	2, A_FaceTarget			    , &States[S_KORAX_ATTACK1+1]),
	S_BRIGHT (KORX, 'E',	5, A_KoraxDecide		    , &States[S_KORAX_ATTACK1+1]),

#define S_KORAX_DEATH1 (S_KORAX_ATTACK1+2)
	S_NORMAL (KORX, 'I',	5, NULL					    , &States[S_KORAX_DEATH1+1]),
	S_NORMAL (KORX, 'J',	5, A_FaceTarget			    , &States[S_KORAX_DEATH1+2]),
	S_NORMAL (KORX, 'K',	5, A_Scream				    , &States[S_KORAX_DEATH1+3]),
	S_NORMAL (KORX, 'L',	5, NULL					    , &States[S_KORAX_DEATH1+4]),
	S_NORMAL (KORX, 'M',	5, NULL					    , &States[S_KORAX_DEATH1+5]),
	S_NORMAL (KORX, 'N',	5, NULL					    , &States[S_KORAX_DEATH1+6]),
	S_NORMAL (KORX, 'O',	5, NULL					    , &States[S_KORAX_DEATH1+7]),
	S_NORMAL (KORX, 'P',	5, NULL					    , &States[S_KORAX_DEATH1+8]),
	S_NORMAL (KORX, 'Q',   10, NULL					    , &States[S_KORAX_DEATH1+9]),
	S_NORMAL (KORX, 'R',	5, A_KoraxBonePop		    , &States[S_KORAX_DEATH1+10]),
	S_NORMAL (KORX, 'S',	5, A_NoBlocking			    , &States[S_KORAX_DEATH1+11]),
	S_NORMAL (KORX, 'T',	5, NULL					    , &States[S_KORAX_DEATH1+12]),
	S_NORMAL (KORX, 'U',	5, NULL					    , &States[S_KORAX_DEATH1+13]),
	S_NORMAL (KORX, 'V',   -1, NULL					    , NULL),

#define S_KORAX_MISSILE1 (S_KORAX_DEATH1+14)
	S_BRIGHT (KORX, 'E',	4, A_FaceTarget			    , &States[S_KORAX_MISSILE1+1]),
	S_BRIGHT (KORX, 'F',	8, A_KoraxMissile		    , &States[S_KORAX_MISSILE1+2]),
	S_BRIGHT (KORX, 'E',	8, NULL					    , &States[S_KORAX_CHASE2]),

#define S_KORAX_COMMAND1 (S_KORAX_MISSILE1+3)
	S_BRIGHT (KORX, 'E',	5, A_FaceTarget			    , &States[S_KORAX_COMMAND1+1]),
	S_BRIGHT (KORX, 'W',   10, A_FaceTarget			    , &States[S_KORAX_COMMAND1+2]),
	S_BRIGHT (KORX, 'G',   15, A_KoraxCommand		    , &States[S_KORAX_COMMAND1+3]),
	S_BRIGHT (KORX, 'W',   10, NULL					    , &States[S_KORAX_COMMAND1+4]),
	S_BRIGHT (KORX, 'E',	5, NULL					    , &States[S_KORAX_CHASE2]),

};

IMPLEMENT_ACTOR (AKorax, Hexen, 10200, 0)
	PROP_SpawnHealth (5000)
	PROP_PainChance (20)
	PROP_SpeedFixed (10)
	PROP_RadiusFixed (65)
	PROP_HeightFixed (115)
	PROP_Mass (2000)
	PROP_Damage (15)
	PROP_Flags (MF_SOLID|MF_SHOOTABLE|MF_COUNTKILL)
	PROP_Flags2 (MF2_FLOORCLIP|MF2_BOSS|MF2_TELESTOMP|MF2_PUSHWALL|MF2_MCROSS)
	PROP_Flags3 (MF3_DONTMORPH|MF3_NOTARGET)
	PROP_Flags4 (MF4_NOICEDEATH)

	PROP_SpawnState (S_KORAX_LOOK1)
	PROP_SeeState (S_KORAX_CHASE2)
	PROP_PainState (S_KORAX_PAIN1)
	PROP_MissileState (S_KORAX_ATTACK1)
	PROP_DeathState (S_KORAX_DEATH1)

	PROP_SeeSound ("KoraxSight")
	PROP_AttackSound ("KoraxAttack")
	PROP_PainSound ("KoraxPain")
	PROP_DeathSound ("KoraxDeath")
	PROP_ActiveSound ("KoraxActive")
	PROP_Obituary ("$OB_KORAX")
END_DEFAULTS

// Korax Spirit -------------------------------------------------------------

class AKoraxSpirit : public AActor
{
	DECLARE_ACTOR (AKoraxSpirit, AActor)
};

FState AKoraxSpirit::States[] =
{
	S_NORMAL (SPIR, 'A',	5, A_KSpiritRoam		    , &States[1]),
	S_NORMAL (SPIR, 'B',	5, A_KSpiritRoam		    , &States[0]),

#define S_KSPIRIT_DEATH (2)
	S_NORMAL (SPIR, 'D',	5, NULL					    , &States[S_KSPIRIT_DEATH+1]),
	S_NORMAL (SPIR, 'E',	5, NULL					    , &States[S_KSPIRIT_DEATH+2]),
	S_NORMAL (SPIR, 'F',	5, NULL					    , &States[S_KSPIRIT_DEATH+3]),
	S_NORMAL (SPIR, 'G',	5, NULL					    , &States[S_KSPIRIT_DEATH+4]),
	S_NORMAL (SPIR, 'H',	5, NULL					    , &States[S_KSPIRIT_DEATH+5]),
	S_NORMAL (SPIR, 'I',	5, NULL					    , NULL),
};

IMPLEMENT_ACTOR (AKoraxSpirit, Hexen, -1, 0)
	PROP_SpeedFixed (8)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY|MF_DROPOFF|MF_NOCLIP|MF_MISSILE)
	PROP_Flags2 (MF2_NOTELEPORT)
	PROP_RenderStyle (STYLE_Translucent)
	PROP_Alpha (HX_ALTSHADOW)
	PROP_SpawnState (0)
END_DEFAULTS

// Korax Bolt ---------------------------------------------------------------

class AKoraxBolt : public AActor
{
	DECLARE_ACTOR (AKoraxBolt, AActor)
};

FState AKoraxBolt::States[] =
{
#define S_KBOLT1 0
	S_BRIGHT (MLFX, 'I',	2, NULL					    , &States[S_KBOLT1+1]),
	S_BRIGHT (MLFX, 'J',	2, A_KBoltRaise			    , &States[S_KBOLT1+2]),
	S_BRIGHT (MLFX, 'I',	2, A_KBolt				    , &States[S_KBOLT1+3]),
	S_BRIGHT (MLFX, 'J',	2, A_KBolt				    , &States[S_KBOLT1+4]),
	S_BRIGHT (MLFX, 'K',	2, A_KBolt				    , &States[S_KBOLT1+5]),
	S_BRIGHT (MLFX, 'L',	2, A_KBolt				    , &States[S_KBOLT1+6]),
	S_BRIGHT (MLFX, 'M',	2, A_KBolt				    , &States[S_KBOLT1+2]),
};

IMPLEMENT_ACTOR (AKoraxBolt, Hexen, -1, 0)
	PROP_RadiusFixed (15)
	PROP_HeightFixed (35)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY|MF_DROPOFF|MF_MISSILE)
	PROP_Flags2 (MF2_NOTELEPORT)
	PROP_RenderStyle (STYLE_Add)

	PROP_SpawnState (S_KBOLT1)
END_DEFAULTS

//============================================================================

//============================================================================
//
// A_KoraxChase
//
//============================================================================

void A_KoraxChase (AActor *actor)
{
	AActor *spot;

	if ((!actor->special2) && (actor->health <= (actor->GetDefault()->health/2)))
	{
		FActorIterator iterator (KORAX_FIRST_TELEPORT_TID);
		spot = iterator.Next ();
		if (spot != NULL)
		{
			P_Teleport (actor, spot->x, spot->y, ONFLOORZ, spot->angle, true, true, false);
		}

		P_StartScript (actor, NULL, 249, NULL, 0, 0, 0, 0, 0, false);
		actor->special2 = 1;	// Don't run again

		return;
	}

	if (!actor->target) return;
	if (pr_koraxchase()<30)
	{
		actor->SetState (actor->MissileState);
	}
	else if (pr_koraxchase()<30)
	{
		S_Sound (actor, CHAN_VOICE, "KoraxActive", 1, ATTN_NONE);
	}

	// Teleport away
	if (actor->health < (actor->GetDefault()->health>>1))
	{
		if (pr_koraxchase()<10)
		{
			FActorIterator iterator (KORAX_TELEPORT_TID);

			if (actor->tracer != NULL)
			{	// Find the previous teleport destination
				do
				{
					spot = iterator.Next ();
				} while (spot != NULL && spot != actor->tracer);
			}

			// Go to the next teleport destination
			spot = iterator.Next ();
			actor->tracer = spot;
			if (spot)
			{
				P_Teleport (actor, spot->x, spot->y, ONFLOORZ, spot->angle, true, true, false);
			}
		}
	}
}

//============================================================================
//
// A_KoraxStep
//
//============================================================================

void A_KoraxStep (AActor *actor)
{
	A_Chase (actor);
}

//============================================================================
//
// A_KoraxStep2
//
//============================================================================

void A_KoraxStep2 (AActor *actor)
{
	S_Sound (actor, CHAN_BODY, "KoraxStep", 1, ATTN_NONE);
	A_Chase (actor);
}

//============================================================================
//
// A_KoraxBonePop
//
//============================================================================

void A_KoraxBonePop (AActor *actor)
{
	AActor *mo;
	int i;

	// Spawn 6 spirits equalangularly
	for (i = 0; i < 6; ++i)
	{
		mo = P_SpawnMissileAngle (actor, RUNTIME_CLASS(AKoraxSpirit), ANGLE_60*i, 5*FRACUNIT);
		if (mo) KSpiritInit (mo, actor);
	}

	P_StartScript (actor, NULL, 255, NULL, 0, 0, 0, 0, false, false);		// Death script
}

//============================================================================
//
// KSpiritInit
//
//============================================================================

void KSpiritInit (AActor *spirit, AActor *korax)
{
	spirit->health = KORAX_SPIRIT_LIFETIME;

	spirit->tracer = korax;						// Swarm around korax
	spirit->special2 = 32+(pr_kspiritinit()&7);	// Float bob index
	spirit->args[0] = 10; 						// initial turn value
	spirit->args[1] = 0; 						// initial look angle

	// Spawn a tail for spirit
	SpawnSpiritTail (spirit);
}

//============================================================================
//
// A_KoraxDecide
//
//============================================================================

void A_KoraxDecide (AActor *actor)
{
	if (pr_koraxdecide()<220)
	{
		actor->SetState (&AKorax::States[S_KORAX_MISSILE1]);
	}
	else
	{
		actor->SetState (&AKorax::States[S_KORAX_COMMAND1]);
	}
}

//============================================================================
//
// A_KoraxMissile
//
//============================================================================

void A_KoraxMissile (AActor *actor)
{
	static const struct { const char *type, *sound; } choices[6] =
	{
		{ "WraithFX1", "WraithMissileFire" },
		{ "Demon1FX1", "DemonMissileFire" },
		{ "Demon2FX1", "DemonMissileFire" },
		{ "FireDemonMissile", "FireDemonAttack" },
		{ "CentaurFX", "CentaurLeaderAttack" },
		{ "SerpentFX", "CentaurLeaderAttack" }
	};

	int type = pr_koraxmissile()%6;
	int i;
	const PClass *info;

	S_Sound (actor, CHAN_VOICE, "KoraxAttack", 1, ATTN_NORM);

	info = PClass::FindClass (choices[type].type);
	if (info == NULL)
	{
		I_Error ("Unknown Korax missile: %s\n", choices[type].type);
	}

	// Fire all 6 missiles at once
	S_Sound (actor, CHAN_WEAPON, choices[type].sound, 1, ATTN_NONE);
	for (i = 0; i < 6; ++i)
	{
		KoraxFire (actor, info, i);
	}
}

//============================================================================
//
// A_KoraxCommand
//
// Call action code scripts (250-254)
//
//============================================================================

void A_KoraxCommand (AActor *actor)
{
	fixed_t x,y,z;
	angle_t ang;
	int numcommands;

	S_Sound (actor, CHAN_VOICE, "KoraxCommand", 1, ATTN_NORM);

	// Shoot stream of lightning to ceiling
	ang = (actor->angle - ANGLE_90) >> ANGLETOFINESHIFT;
	x = actor->x + KORAX_COMMAND_OFFSET * finecosine[ang];
	y = actor->y + KORAX_COMMAND_OFFSET * finesine[ang];
	z = actor->z + KORAX_COMMAND_HEIGHT*FRACUNIT;
	Spawn<AKoraxBolt> (x, y, z, ALLOW_REPLACE);

	if (actor->health <= (actor->GetDefault()->health >> 1))
	{
		numcommands = 5;
	}
	else
	{
		numcommands = 4;
	}

	P_StartScript (actor, NULL, 250+(pr_koraxcommand()%numcommands),
		NULL, 0, 0, 0, 0, false, false);
}

//============================================================================
//
// KoraxFire
//
// Arm projectiles
//		arm positions numbered:
//			1	top left
//			2	middle left
//			3	lower left
//			4	top right
//			5	middle right
//			6	lower right
//
//============================================================================

void KoraxFire (AActor *actor, const PClass *type, int arm)
{
	static const int extension[6] =
	{
		KORAX_ARM_EXTENSION_SHORT,
		KORAX_ARM_EXTENSION_LONG,
		KORAX_ARM_EXTENSION_LONG,
		KORAX_ARM_EXTENSION_SHORT,
		KORAX_ARM_EXTENSION_LONG,
		KORAX_ARM_EXTENSION_LONG
	};
	static const fixed_t armheight[6] =
	{
		KORAX_ARM1_HEIGHT,
		KORAX_ARM2_HEIGHT,
		KORAX_ARM3_HEIGHT,
		KORAX_ARM4_HEIGHT,
		KORAX_ARM5_HEIGHT,
		KORAX_ARM6_HEIGHT
	};

	angle_t ang;
	fixed_t x,y,z;

	ang = (actor->angle + (arm < 3 ? -KORAX_DELTAANGLE : KORAX_DELTAANGLE))
		>> ANGLETOFINESHIFT;
	x = actor->x + extension[arm] * finecosine[ang];
	y = actor->y + extension[arm] * finesine[ang];
	z = actor->z - actor->floorclip + armheight[arm];
	P_SpawnKoraxMissile (x, y, z, actor, actor->target, type);
}

//============================================================================
//
// A_KSpiritWeave
//
//============================================================================

void A_KSpiritWeave (AActor *actor)
{
	fixed_t newX, newY;
	int weaveXY, weaveZ;
	int angle;

	weaveXY = actor->special2>>16;
	weaveZ = actor->special2&0xFFFF;
	angle = (actor->angle+ANG90)>>ANGLETOFINESHIFT;
	newX = actor->x-FixedMul(finecosine[angle], 
		FloatBobOffsets[weaveXY]<<2);
	newY = actor->y-FixedMul(finesine[angle],
		FloatBobOffsets[weaveXY]<<2);
	weaveXY = (weaveXY+(pr_kspiritweave()%5))&63;
	newX += FixedMul(finecosine[angle], 
		FloatBobOffsets[weaveXY]<<2);
	newY += FixedMul(finesine[angle], 
		FloatBobOffsets[weaveXY]<<2);
	P_TryMove(actor, newX, newY, true);
	actor->z -= FloatBobOffsets[weaveZ]<<1;
	weaveZ = (weaveZ+(pr_kspiritweave()%5))&63;
	actor->z += FloatBobOffsets[weaveZ]<<1;	
	actor->special2 = weaveZ+(weaveXY<<16);
}

//============================================================================
//
// A_KSpiritSeeker
//
//============================================================================

void A_KSpiritSeeker (AActor *actor, angle_t thresh, angle_t turnMax)
{
	int dir;
	int dist;
	angle_t delta;
	angle_t angle;
	AActor *target;
	fixed_t newZ;
	fixed_t deltaZ;

	target = actor->tracer;
	if (target == NULL)
	{
		return;
	}
	dir = P_FaceMobj (actor, target, &delta);
	if (delta > thresh)
	{
		delta >>= 1;
		if(delta > turnMax)
		{
			delta = turnMax;
		}
	}
	if(dir)
	{ // Turn clockwise
		actor->angle += delta;
	}
	else
	{ // Turn counter clockwise
		actor->angle -= delta;
	}
	angle = actor->angle>>ANGLETOFINESHIFT;
	actor->momx = FixedMul (actor->Speed, finecosine[angle]);
	actor->momy = FixedMul (actor->Speed, finesine[angle]);

	if (!(level.time&15) 
		|| actor->z > target->z+(target->GetDefault()->height)
		|| actor->z+actor->height < target->z)
	{
		newZ = target->z+((pr_kspiritseek()*target->GetDefault()->height)>>8);
		deltaZ = newZ-actor->z;
		if (abs(deltaZ) > 15*FRACUNIT)
		{
			if(deltaZ > 0)
			{
				deltaZ = 15*FRACUNIT;
			}
			else
			{
				deltaZ = -15*FRACUNIT;
			}
		}
		dist = P_AproxDistance (target->x-actor->x, target->y-actor->y);
		dist = dist/actor->Speed;
		if (dist < 1)
		{
			dist = 1;
		}
		actor->momz = deltaZ/dist;
	}
	return;
}

//============================================================================
//
// A_KSpiritRoam
//
//============================================================================

void A_KSpiritRoam (AActor *actor)
{
	if (actor->health-- <= 0)
	{
		S_Sound (actor, CHAN_VOICE, "SpiritDie", 1, ATTN_NORM);
		actor->SetState (&AKoraxSpirit::States[S_KSPIRIT_DEATH]);
	}
	else
	{
		if (actor->tracer)
		{
			A_KSpiritSeeker (actor, actor->args[0]*ANGLE_1,
							 actor->args[0]*ANGLE_1*2);
		}
		A_KSpiritWeave (actor);
		if (pr_kspiritroam()<50)
		{
			S_Sound (actor, CHAN_VOICE, "SpiritActive", 1, ATTN_NONE);
		}
	}
}

//============================================================================
//
// A_KBolt
//
//============================================================================

void A_KBolt (AActor *actor)
{
	// Countdown lifetime
	if (actor->special1-- <= 0)
	{
		actor->Destroy ();
	}
}

//============================================================================
//
// A_KBoltRaise
//
//============================================================================

void A_KBoltRaise (AActor *actor)
{
	AActor *mo;
	fixed_t z;

	// Spawn a child upward
	z = actor->z + KORAX_BOLT_HEIGHT;

	if ((z + KORAX_BOLT_HEIGHT) < actor->ceilingz)
	{
		mo = Spawn<AKoraxBolt> (actor->x, actor->y, z, ALLOW_REPLACE);
		if (mo)
		{
			mo->special1 = KORAX_BOLT_LIFETIME;
		}
	}
	else
	{
		// Maybe cap it off here
	}
}

//============================================================================
//
// P_SpawnKoraxMissile
//
//============================================================================

AActor *P_SpawnKoraxMissile (fixed_t x, fixed_t y, fixed_t z,
	AActor *source, AActor *dest, const PClass *type)
{
	AActor *th;
	angle_t an;
	int dist;

	z -= source->floorclip;
	th = Spawn (type, x, y, z, ALLOW_REPLACE);
	th->target = source; // Originator
	an = R_PointToAngle2(x, y, dest->x, dest->y);
	if (dest->flags & MF_SHADOW)
	{ // Invisible target
		an += pr_missile.Random2()<<21;
	}
	th->angle = an;
	an >>= ANGLETOFINESHIFT;
	th->momx = FixedMul (th->Speed, finecosine[an]);
	th->momy = FixedMul (th->Speed, finesine[an]);
	dist = P_AproxDistance (dest->x - x, dest->y - y);
	dist = dist/th->Speed;
	if (dist < 1)
	{
		dist = 1;
	}
	th->momz = (dest->z-z+(30*FRACUNIT))/dist;
	return (P_CheckMissileSpawn(th) ? th : NULL);
}
