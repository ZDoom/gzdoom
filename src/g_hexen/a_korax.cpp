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

/*
#include "actor.h"
#include "info.h"
#include "p_local.h"
#include "p_spec.h"
#include "s_sound.h"
#include "a_action.h"
#include "m_random.h"
#include "i_system.h"
#include "thingdef/thingdef.h"
#include "g_level.h"
*/

const int KORAX_SPIRIT_LIFETIME = 5*TICRATE/5;	// 5 seconds
const int KORAX_COMMAND_HEIGHT	= 120;
const int KORAX_COMMAND_OFFSET	= 27;

const int KORAX_TID					= 245;
const int KORAX_FIRST_TELEPORT_TID	= 248;
const int KORAX_TELEPORT_TID		= 249;

const int KORAX_DELTAANGLE			= 85;
const int KORAX_ARM_EXTENSION_SHORT	= 40;
const int KORAX_ARM_EXTENSION_LONG	= 55;

const int KORAX_ARM1_HEIGHT			= 108;
const int KORAX_ARM2_HEIGHT			= 82;
const int KORAX_ARM3_HEIGHT			= 54;
const int KORAX_ARM4_HEIGHT			= 104;
const int KORAX_ARM5_HEIGHT			= 86;
const int KORAX_ARM6_HEIGHT			= 53;

const double KORAX_BOLT_HEIGHT		= 48.;
const int KORAX_BOLT_LIFETIME		= 3;



static FRandom pr_koraxchase ("KoraxChase");
static FRandom pr_kspiritinit ("KSpiritInit");
static FRandom pr_koraxdecide ("KoraxDecide");
static FRandom pr_koraxmissile ("KoraxMissile");
static FRandom pr_koraxcommand ("KoraxCommand");
static FRandom pr_kspiritweave ("KSpiritWeave");
static FRandom pr_kspiritseek ("KSpiritSeek");
static FRandom pr_kspiritroam ("KSpiritRoam");
static FRandom pr_kmissile ("SKoraxMissile");

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

void KoraxFire (AActor *actor, PClassActor *type, int arm);
void KSpiritInit (AActor *spirit, AActor *korax);
AActor *P_SpawnKoraxMissile (const DVector3 &pos, AActor *source, AActor *dest, PClassActor *type);

extern void SpawnSpiritTail (AActor *spirit);

//============================================================================
//
// A_KoraxChase
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_KoraxChase)
{
	PARAM_ACTION_PROLOGUE;

	AActor *spot;

	if ((!self->special2) && (self->health <= (self->SpawnHealth()/2)))
	{
		FActorIterator iterator (KORAX_FIRST_TELEPORT_TID);
		spot = iterator.Next ();
		if (spot != NULL)
		{
			P_Teleport (self, spot->PosAtZ(ONFLOORZ), spot->Angles.Yaw, TELF_SOURCEFOG | TELF_DESTFOG);
		}

		P_StartScript (self, NULL, 249, NULL, NULL, 0, 0);
		self->special2 = 1;	// Don't run again

		return 0;
	}

	if (self->target == NULL)
	{
		return 0;
	}
	if (pr_koraxchase()<30)
	{
		self->SetState (self->MissileState);
	}
	else if (pr_koraxchase()<30)
	{
		S_Sound (self, CHAN_VOICE, "KoraxActive", 1, ATTN_NONE);
	}

	// Teleport away
	if (self->health < (self->SpawnHealth()>>1))
	{
		if (pr_koraxchase()<10)
		{
			FActorIterator iterator (KORAX_TELEPORT_TID);

			if (self->tracer != NULL)
			{	// Find the previous teleport destination
				do
				{
					spot = iterator.Next ();
				} while (spot != NULL && spot != self->tracer);
			}

			// Go to the next teleport destination
			spot = iterator.Next ();
			self->tracer = spot;
			if (spot)
			{
				P_Teleport (self, spot->PosAtZ(ONFLOORZ), spot->Angles.Yaw, TELF_SOURCEFOG | TELF_DESTFOG);
			}
		}
	}
	return 0;
}

//============================================================================
//
// A_KoraxBonePop
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_KoraxBonePop)
{
	PARAM_ACTION_PROLOGUE;

	AActor *mo;
	int i;

	// Spawn 6 spirits equalangularly
	for (i = 0; i < 6; ++i)
	{
		mo = P_SpawnMissileAngle (self, PClass::FindActor("KoraxSpirit"), DAngle(60.*i), 5.);
		if (mo)
		{
			KSpiritInit (mo, self);
		}
	}

	P_StartScript (self, NULL, 255, NULL, NULL, 0, 0);		// Death script
	return 0;
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
	spirit->WeaveIndexZ = 32 + (pr_kspiritinit() & 7);	// Float bob index
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

DEFINE_ACTION_FUNCTION(AActor, A_KoraxDecide)
{
	PARAM_ACTION_PROLOGUE;

	if (pr_koraxdecide()<220)
	{
		self->SetState (self->FindState("Attack"));
	}
	else
	{
		self->SetState (self->FindState("Command"));
	}
	return 0;
}

//============================================================================
//
// A_KoraxMissile
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_KoraxMissile)
{
	PARAM_ACTION_PROLOGUE;

	static const struct { const char *type, *sound; } choices[6] =
	{
		{ "WraithFX1", "WraithMissileFire" },
		{ "Demon1FX1", "DemonMissileFire" },
		{ "Demon2FX1", "DemonMissileFire" },
		{ "FireDemonMissile", "FireDemonAttack" },
		{ "CentaurFX", "CentaurLeaderAttack" },
		{ "SerpentFX", "CentaurLeaderAttack" }
	};

	int type = pr_koraxmissile() % 6;
	int i;
	PClassActor *info;

	S_Sound(self, CHAN_VOICE, "KoraxAttack", 1, ATTN_NORM);

	info = PClass::FindActor(choices[type].type);
	if (info == NULL)
	{
		I_Error("Unknown Korax missile: %s\n", choices[type].type);
	}

	// Fire all 6 missiles at once
	S_Sound(self, CHAN_WEAPON, choices[type].sound, 1, ATTN_NONE);
	for (i = 0; i < 6; ++i)
	{
		KoraxFire(self, info, i);
	}
	return 0;
}

//============================================================================
//
// A_KoraxCommand
//
// Call action code scripts (250-254)
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_KoraxCommand)
{
	PARAM_ACTION_PROLOGUE;
	DAngle ang;
	int numcommands;

	S_Sound (self, CHAN_VOICE, "KoraxCommand", 1, ATTN_NORM);

	// Shoot stream of lightning to ceiling
	ang = self->Angles.Yaw - 90;
	DVector3 pos = self->Vec3Angle(KORAX_COMMAND_OFFSET, ang, KORAX_COMMAND_HEIGHT);
	Spawn("KoraxBolt", pos, ALLOW_REPLACE);

	if (self->health <= (self->SpawnHealth() >> 1))
	{
		numcommands = 5;
	}
	else
	{
		numcommands = 4;
	}

	P_StartScript (self, NULL, 250+(pr_koraxcommand()%numcommands), NULL, NULL, 0, 0);
	return 0;
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

void KoraxFire (AActor *actor, PClassActor *type, int arm)
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
	static const int armheight[6] =
	{
		KORAX_ARM1_HEIGHT,
		KORAX_ARM2_HEIGHT,
		KORAX_ARM3_HEIGHT,
		KORAX_ARM4_HEIGHT,
		KORAX_ARM5_HEIGHT,
		KORAX_ARM6_HEIGHT
	};

	DAngle ang = actor->Angles.Yaw + (arm < 3 ? -KORAX_DELTAANGLE : KORAX_DELTAANGLE);
	DVector3 pos = actor->Vec3Angle(extension[arm], ang, armheight[arm] - actor->Floorclip);
	P_SpawnKoraxMissile (pos, actor, actor->target, type);
}

//============================================================================
//
// A_KSpiritSeeker
//
//============================================================================

static void A_KSpiritSeeker (AActor *actor, DAngle thresh, DAngle turnMax)
{
	int dir;
	DAngle delta;
	AActor *target;
	double newZ;
	double deltaZ;

	target = actor->tracer;
	if (target == NULL)
	{
		return;
	}
	dir = P_FaceMobj (actor, target, &delta);
	if (delta > thresh)
	{
		delta /= 2;
		if(delta > turnMax)
		{
			delta = turnMax;
		}
	}
	if(dir)
	{ // Turn clockwise
		actor->Angles.Yaw += delta;
	}
	else
	{ // Turn counter clockwise
		actor->Angles.Yaw -= delta;
	}
	actor->VelFromAngle();

	if (!(level.time&15) 
		|| actor->Z() > target->Z() + target->GetDefault()->Height
		|| actor->Top() < target->Z())
	{
		newZ = target->Z() + pr_kspiritseek() * target->GetDefault()->Height / 256;
		deltaZ = newZ-actor->Z();

		if (fabs(deltaZ) > 15)
		{
			if(deltaZ > 0)
			{
				deltaZ = 15;
			}
			else
			{
				deltaZ = -15;
			}
		}
		actor->Vel.Z = deltaZ + actor->DistanceBySpeed(target, actor->Speed);
	}
	return;
}

//============================================================================
//
// A_KSpiritRoam
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_KSpiritRoam)
{
	PARAM_ACTION_PROLOGUE;

	if (self->health-- <= 0)
	{
		S_Sound (self, CHAN_VOICE, "SpiritDie", 1, ATTN_NORM);
		self->SetState (self->FindState("Death"));
	}
	else
	{
		if (self->tracer)
		{
			A_KSpiritSeeker(self, (double)self->args[0], self->args[0] * 2.);
		}
		int xyspeed = (pr_kspiritweave() % 5);
		int zspeed = (pr_kspiritweave() % 5);
		A_Weave(self, xyspeed, zspeed, 4., 2.);

		if (pr_kspiritroam()<50)
		{
			S_Sound (self, CHAN_VOICE, "SpiritActive", 1, ATTN_NONE);
		}
	}
	return 0;
}

//============================================================================
//
// A_KBolt
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_KBolt)
{
	PARAM_ACTION_PROLOGUE;

	// Countdown lifetime
	if (self->special1-- <= 0)
	{
		self->Destroy ();
	}
	return 0;
}

//============================================================================
//
// A_KBoltRaise
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_KBoltRaise)
{
	PARAM_ACTION_PROLOGUE;

	AActor *mo;

	// Spawn a child upward
	double z = self->Z() + KORAX_BOLT_HEIGHT;

	if ((z + KORAX_BOLT_HEIGHT) < self->ceilingz)
	{
		mo = Spawn("KoraxBolt", self->PosAtZ(z), ALLOW_REPLACE);
		if (mo)
		{
			mo->special1 = KORAX_BOLT_LIFETIME;
		}
	}
	else
	{
		// Maybe cap it off here
	}
	return 0;
}

//============================================================================
//
// P_SpawnKoraxMissile
//
//============================================================================

AActor *P_SpawnKoraxMissile (const DVector3 &pos, AActor *source, AActor *dest, PClassActor *type)
{
	AActor *th;
	DAngle an;
	double dist;

	th = Spawn (type, pos, ALLOW_REPLACE);
	th->target = source; // Originator
	an = th->AngleTo(dest);
	if (dest->flags & MF_SHADOW)
	{ // Invisible target
		an += pr_kmissile.Random2() * (45/256.);
	}
	th->Angles.Yaw = an;
	th->VelFromAngle();
	dist = dest->DistanceBySpeed(th, th->Speed);
	th->Vel.Z = (dest->Z() - pos.Z + 30) / dist;
	return (P_CheckMissileSpawn(th, source->radius) ? th : NULL);
}
