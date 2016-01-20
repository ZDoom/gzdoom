/*
#include "actor.h"
#include "info.h"
#include "p_local.h"
#include "s_sound.h"
#include "a_action.h"
#include "m_random.h"
#include "a_hexenglobal.h"
#include "i_system.h"
#include "thingdef/thingdef.h"
#include "g_level.h"
*/

//============================================================================
//
//	Sorcerer stuff
//
// Sorcerer Variables
//		special1		Angle of ball 1 (all others relative to that)
//		StopBall		which ball to stop at in stop mode (MT_???)
//		args[0]			Defense time
//		args[1]			Number of full rotations since stopping mode
//		args[2]			Target orbit speed for acceleration/deceleration
//		args[3]			Movement mode (see SORC_ macros)
//		args[4]			Current ball orbit speed
//	Sorcerer Ball Variables
//		special1		Previous angle of ball (for woosh)
//		special2		Countdown of rapid fire (FX4)
//============================================================================

#define SORCBALL_INITIAL_SPEED 		7
#define SORCBALL_TERMINAL_SPEED		25
#define SORCBALL_SPEED_ROTATIONS 	5
#define SORC_DEFENSE_TIME			255
#define SORC_DEFENSE_HEIGHT			45
#define BOUNCE_TIME_UNIT			(35/2)
#define SORCFX4_RAPIDFIRE_TIME		(6*3)		// 3 seconds
#define SORCFX4_SPREAD_ANGLE		20

#define SORC_DECELERATE		0
#define SORC_ACCELERATE 	1
#define SORC_STOPPING		2
#define SORC_FIRESPELL		3
#define SORC_STOPPED		4
#define SORC_NORMAL			5
#define SORC_FIRING_SPELL	6

#define BALL1_ANGLEOFFSET	0
#define BALL2_ANGLEOFFSET	(ANGLE_MAX/3)
#define BALL3_ANGLEOFFSET	((ANGLE_MAX/3)*2)

void A_SlowBalls (AActor *actor);
void A_StopBalls (AActor *actor);
void A_AccelBalls (AActor *actor);
void A_DecelBalls (AActor *actor);
void A_SorcOffense2 (AActor *actor);
void A_DoBounceCheck (AActor *actor, const char *sound);

static FRandom pr_heresiarch ("Heresiarch");

// The Heresiarch him/itself ------------------------------------------------

class AHeresiarch : public AActor
{
	DECLARE_CLASS (AHeresiarch, AActor)
public:
	const PClass *StopBall;

	void Serialize (FArchive &arc);
	void Die (AActor *source, AActor *inflictor, int dmgflags);
};

IMPLEMENT_CLASS (AHeresiarch)

void AHeresiarch::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	arc << StopBall;
}

void AHeresiarch::Die (AActor *source, AActor *inflictor, int dmgflags)
{
	// The heresiarch just executes a script instead of a special upon death
	int script = special;
	special = 0;

	Super::Die (source, inflictor, dmgflags);

	if (script != 0)
	{
		P_StartScript (this, NULL, script, level.MapName, NULL, 0, 0);
	}
}

// Base class for the balls flying around the Heresiarch's head -------------

class ASorcBall : public AActor
{
	DECLARE_CLASS (ASorcBall, AActor)
public:
	virtual void DoFireSpell ();
	virtual void SorcUpdateBallAngle ();
	virtual void CastSorcererSpell ();
	angle_t AngleOffset;

	void Serialize (FArchive &arc)
	{
		Super::Serialize (arc);
		arc << AngleOffset;
	}

	bool SpecialBlastHandling (AActor *source, fixed_t strength)
	{ // don't blast sorcerer balls
		return false;
	}
};

IMPLEMENT_CLASS (ASorcBall)

// First ball (purple) - fires projectiles ----------------------------------

class ASorcBall1 : public ASorcBall
{
	DECLARE_CLASS (ASorcBall1, ASorcBall)
public:
	void BeginPlay ()
	{
		Super::BeginPlay ();
		AngleOffset = BALL1_ANGLEOFFSET;
	}
	virtual void DoFireSpell ();
	virtual void SorcUpdateBallAngle ();
	virtual void CastSorcererSpell ();
};

IMPLEMENT_CLASS (ASorcBall1)

// Second ball (blue) - generates the shield --------------------------------

class ASorcBall2 : public ASorcBall
{
	DECLARE_CLASS (ASorcBall2, ASorcBall)
public:
	void BeginPlay ()
	{
		Super::BeginPlay ();
		AngleOffset = BALL2_ANGLEOFFSET;
	}
	virtual void CastSorcererSpell ();
};

IMPLEMENT_CLASS (ASorcBall2)

// Third ball (green) - summons Bishops -------------------------------------

class ASorcBall3 : public ASorcBall
{
	DECLARE_CLASS (ASorcBall3, ASorcBall)
public:
	void BeginPlay ()
	{
		Super::BeginPlay ();
		AngleOffset = BALL3_ANGLEOFFSET;
	}
	virtual void CastSorcererSpell ();
};

IMPLEMENT_CLASS (ASorcBall3)

// Sorcerer spell 1 (The burning, bouncing head thing) ----------------------

/*
class ASorcFX1 : public AActor
{
	DECLARE_CLASS (ASorcFX1, AActor)
public:
	bool FloorBounceMissile (secplane_t &plane)
	{
		fixed_t orgvelz = velz;

		if (!Super::FloorBounceMissile (plane))
		{
			velz = -orgvelz;		// no energy absorbed
			return false;
		}
		return true;
	}
};
IMPLEMENT_CLASS (ASorcFX1)
*/

//============================================================================
//
// SorcBall::DoFireSpell
//
//============================================================================

void ASorcBall::DoFireSpell ()
{
	CastSorcererSpell ();
	target->args[3] = SORC_STOPPED;
}

//============================================================================
//
// SorcBall1::DoFireSpell
//
//============================================================================

void ASorcBall1::DoFireSpell ()
{
	if (pr_heresiarch() < 200)
	{
		S_Sound (target, CHAN_VOICE, "SorcererSpellCast", 1, ATTN_NONE);
		special2 = SORCFX4_RAPIDFIRE_TIME;
		args[4] = 128;
		target->args[3] = SORC_FIRING_SPELL;
	}
	else
	{
		Super::DoFireSpell ();
	}
}

//============================================================================
//
// A_SorcSpinBalls
//
// Spawn spinning balls above head - actor is sorcerer
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_SorcSpinBalls)
{
	AActor *mo;

	self->SpawnState += 2;		// [RH] Don't spawn balls again
	A_SlowBalls(self);
	self->args[0] = 0;								// Currently no defense
	self->args[3] = SORC_NORMAL;
	self->args[4] = SORCBALL_INITIAL_SPEED;		// Initial orbit speed
	self->special1 = ANGLE_1;

	fixedvec3 pos = self->PosPlusZ(-self->floorclip + self->height);
	
	mo = Spawn("SorcBall1", pos, NO_REPLACE);
	if (mo)
	{
		mo->target = self;
		mo->special2 = SORCFX4_RAPIDFIRE_TIME;
	}
	mo = Spawn("SorcBall2", pos, NO_REPLACE);
	if (mo) mo->target = self;
	mo = Spawn("SorcBall3", pos, NO_REPLACE);
	if (mo) mo->target = self;
}


//============================================================================
//
// A_SorcBallOrbit
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_SorcBallOrbit)
{
	// [RH] If no parent, then die instead of crashing
	if (self->target == NULL)
	{
		self->SetState (self->FindState(NAME_Pain));
		return;
	}

	ASorcBall *actor;
	angle_t angle, baseangle;
	int mode = self->target->args[3];
	AHeresiarch *parent = barrier_cast<AHeresiarch *>(self->target);
	int dist = parent->radius - (self->radius<<1);
	angle_t prevangle = self->special1;

	if (!self->IsKindOf (RUNTIME_CLASS(ASorcBall)))
	{
		I_Error ("Corrupted sorcerer:\nTried to use a %s", RUNTIME_TYPE(self)->TypeName.GetChars());
	}
	actor = static_cast<ASorcBall *> (self);

	if (actor->target->health <= 0)
	{
		actor->SetState (actor->FindState(NAME_Pain));
		return;
	}

	baseangle = (angle_t)parent->special1;
	angle = baseangle + actor->AngleOffset;
	actor->angle = angle;
	angle >>= ANGLETOFINESHIFT;

	switch (mode)
	{
	case SORC_NORMAL:			// Balls rotating normally
		actor->SorcUpdateBallAngle ();
		break;

	case SORC_DECELERATE:		// Balls decelerating
		A_DecelBalls(actor);
		actor->SorcUpdateBallAngle ();
		break;

	case SORC_ACCELERATE:		// Balls accelerating
		A_AccelBalls(actor);
		actor->SorcUpdateBallAngle ();
		break;

	case SORC_STOPPING:			// Balls stopping
		if ((parent->StopBall == RUNTIME_TYPE(actor)) &&
			 (parent->args[1] > SORCBALL_SPEED_ROTATIONS) &&
			 (absangle(angle - (parent->angle>>ANGLETOFINESHIFT)) < (30<<5)))
		{
			// Can stop now
			actor->target->args[3] = SORC_FIRESPELL;
			actor->target->args[4] = 0;
			// Set angle so self angle == sorcerer angle
			parent->special1 = (int)(parent->angle - actor->AngleOffset);
		}
		else
		{
			actor->SorcUpdateBallAngle ();
		}
		break;

	case SORC_FIRESPELL:			// Casting spell
		if (parent->StopBall == RUNTIME_TYPE(actor))
		{
			// Put sorcerer into special throw spell anim
			if (parent->health > 0)
				parent->SetState (parent->FindState("Attack1"));

			actor->DoFireSpell ();
		}
		break;

	case SORC_FIRING_SPELL:
		if (parent->StopBall == RUNTIME_TYPE(actor))
		{
			if (actor->special2-- <= 0)
			{
				// Done rapid firing 
				parent->args[3] = SORC_STOPPED;
				// Back to orbit balls
				if (parent->health > 0)
					parent->SetState (parent->FindState("Attack2"));
			}
			else
			{
				// Do rapid fire spell
				A_SorcOffense2(actor);
			}
		}
		break;

	case SORC_STOPPED:			// Balls stopped
	default:
		break;
	}

	if ((angle < prevangle) && (parent->args[4]==SORCBALL_TERMINAL_SPEED))
	{
		parent->args[1]++;			// Bump rotation counter
		// Completed full rotation - make woosh sound
		S_Sound (actor, CHAN_BODY, "SorcererBallWoosh", 1, ATTN_NORM);
	}
	actor->special1 = angle;		// Set previous angle

	fixedvec3 pos = parent->Vec3Offset(
		FixedMul(dist, finecosine[angle]),
		FixedMul(dist, finesine[angle]),
		-parent->floorclip + parent->height);
	actor->SetOrigin (pos, true);
	actor->floorz = parent->floorz;
	actor->ceilingz = parent->ceilingz;
}

//============================================================================
//
// A_SpeedBalls
//
// Set balls to speed mode - self is sorcerer
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_SpeedBalls)
{
	self->args[3] = SORC_ACCELERATE;				// speed mode
	self->args[2] = SORCBALL_TERMINAL_SPEED;		// target speed
}


//============================================================================
//
// A_SlowBalls
//
// Set balls to slow mode - actor is sorcerer
//
//============================================================================

void A_SlowBalls(AActor *actor)
{
	actor->args[3] = SORC_DECELERATE;				// slow mode
	actor->args[2] = SORCBALL_INITIAL_SPEED;		// target speed
}

//============================================================================
//
// A_StopBalls
//
// Instant stop when rotation gets to ball in special2
//		self is sorcerer
//
//============================================================================

void A_StopBalls(AActor *scary)
{
	AHeresiarch *actor = static_cast<AHeresiarch *> (scary);
	int chance = pr_heresiarch();
	actor->args[3] = SORC_STOPPING;				// stopping mode
	actor->args[1] = 0;							// Reset rotation counter

	if ((actor->args[0] <= 0) && (chance < 200))
	{
		actor->StopBall = RUNTIME_CLASS(ASorcBall2);	// Blue
	}
	else if((actor->health < (actor->SpawnHealth() >> 1)) &&
			(chance < 200))
	{
		actor->StopBall = RUNTIME_CLASS(ASorcBall3);	// Green
	}
	else
	{
		actor->StopBall = RUNTIME_CLASS(ASorcBall1);	// Yellow
	}
}

//============================================================================
//
// A_AccelBalls
//
// Increase ball orbit speed - actor is ball
//
//============================================================================

void A_AccelBalls(AActor *actor)
{
	AActor *sorc = actor->target;

	if (sorc->args[4] < sorc->args[2])
	{
		sorc->args[4]++;
	}
	else
	{
		sorc->args[3] = SORC_NORMAL;
		if (sorc->args[4] >= SORCBALL_TERMINAL_SPEED)
		{
			// Reached terminal velocity - stop balls
			A_StopBalls(sorc);
		}
	}
}

//============================================================================
//
// A_DecelBalls
//
// Decrease ball orbit speed - actor is ball
//
//============================================================================

void A_DecelBalls(AActor *actor)
{
	AActor *sorc = actor->target;

	if (sorc->args[4] > sorc->args[2])
	{
		sorc->args[4]--;
	}
	else
	{
		sorc->args[3] = SORC_NORMAL;
	}
}

//============================================================================
//
// ASorcBall1::SorcUpdateBallAngle
//
// Update angle if first ball
//============================================================================

void ASorcBall1::SorcUpdateBallAngle ()
{
	target->special1 += ANGLE_1*target->args[4];
}

//============================================================================
//
// ASorcBall::SorcUpdateBallAngle
//
//============================================================================

void ASorcBall::SorcUpdateBallAngle ()
{
}

//============================================================================
//
// ASorcBall::CastSorcererSpell
//
// Make noise and change the parent sorcerer's animation
//
//============================================================================

void ASorcBall::CastSorcererSpell ()
{
	S_Sound (target, CHAN_VOICE, "SorcererSpellCast", 1, ATTN_NONE);

	// Put sorcerer into throw spell animation
	if (target->health > 0)
		target->SetState (target->FindState("Attack2"));
}

//============================================================================
//
// ASorcBall2::CastSorcererSpell
//
// Defensive
//
//============================================================================

void ASorcBall2::CastSorcererSpell ()
{
	Super::CastSorcererSpell ();

	AActor *parent = target;
	AActor *mo;

	mo = Spawn("SorcFX2", PosPlusZ(-parent->floorclip + SORC_DEFENSE_HEIGHT*FRACUNIT), ALLOW_REPLACE);
	parent->flags2 |= MF2_REFLECTIVE|MF2_INVULNERABLE;
	parent->args[0] = SORC_DEFENSE_TIME;
	if (mo) mo->target = parent;
}

//============================================================================
//
// ASorcBall3::CastSorcererSpell
//
// Reinforcements
//
//============================================================================

void ASorcBall3::CastSorcererSpell ()
{
	Super::CastSorcererSpell ();

	AActor *mo;
	angle_t ang1, ang2;
	AActor *parent = target;

	ang1 = angle - ANGLE_45;
	ang2 = angle + ANGLE_45;
	const PClass *cls = PClass::FindClass("SorcFX3");
	if (health < (SpawnHealth()/3))
	{	// Spawn 2 at a time
		mo = P_SpawnMissileAngle(parent, cls, ang1, 4*FRACUNIT);
		if (mo) mo->target = parent;
		mo = P_SpawnMissileAngle(parent, cls, ang2, 4*FRACUNIT);
		if (mo) mo->target = parent;
	}			
	else
	{
		if (pr_heresiarch() < 128)
			ang1 = ang2;
		mo = P_SpawnMissileAngle(parent, cls, ang1, 4*FRACUNIT);
		if (mo) mo->target = parent;
	}
}


/*
void A_SpawnReinforcements(AActor *actor)
{
	AActor *parent = actor->target;
	AActor *mo;
	angle_t ang;

	ang = ANGLE_1 * P_Random();
	mo = P_SpawnMissileAngle(actor, MT_SORCFX3, ang, 5*FRACUNIT);
	if (mo) mo->target = parent;
}
*/

//============================================================================
//
// SorcBall1::CastSorcererSpell
//
// Offensive
//
//============================================================================

void ASorcBall1::CastSorcererSpell ()
{
	Super::CastSorcererSpell ();

	AActor *mo;
	angle_t ang1, ang2;
	AActor *parent = target;

	ang1 = angle + ANGLE_1*70;
	ang2 = angle - ANGLE_1*70;
	const PClass *cls = PClass::FindClass("SorcFX1");
	mo = P_SpawnMissileAngle (parent, cls, ang1, 0);
	if (mo)
	{
		mo->target = parent;
		mo->tracer = parent->target;
		mo->args[4] = BOUNCE_TIME_UNIT;
		mo->args[3] = 15;				// Bounce time in seconds
	}
	mo = P_SpawnMissileAngle (parent, cls, ang2, 0);
	if (mo)
	{
		mo->target = parent;
		mo->tracer = parent->target;
		mo->args[4] = BOUNCE_TIME_UNIT;
		mo->args[3] = 15;				// Bounce time in seconds
	}
}

//============================================================================
//
// A_SorcOffense2
//
// Actor is ball
//
//============================================================================

void A_SorcOffense2(AActor *actor)
{
	angle_t ang1;
	AActor *mo;
	int delta, index;
	AActor *parent = actor->target;
	AActor *dest = parent->target;
	int dist;

	// [RH] If no enemy, then don't try to shoot.
	if (dest == NULL)
	{
		return;
	}

	index = actor->args[4] << 5;
	actor->args[4] = (actor->args[4] + 15) & 255;
	delta = (finesine[index])*SORCFX4_SPREAD_ANGLE;
	delta = (delta>>FRACBITS)*ANGLE_1;
	ang1 = actor->angle + delta;
	mo = P_SpawnMissileAngle(parent, PClass::FindClass("SorcFX4"), ang1, 0);
	if (mo)
	{
		mo->special2 = 35*5/2;		// 5 seconds
		dist = mo->AproxDistance(dest) / mo->Speed;
		if(dist < 1) dist = 1;
		mo->velz = (dest->Z() - mo->Z()) / dist;
	}
}

//============================================================================
//
// A_SorcBossAttack
//
// Resume ball spinning
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_SorcBossAttack)
{
	self->args[3] = SORC_ACCELERATE;
	self->args[2] = SORCBALL_INITIAL_SPEED;
}

//============================================================================
//
// A_SpawnFizzle
//
// spell cast magic fizzle
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_SpawnFizzle)
{
	fixed_t dist = 5*FRACUNIT;
	fixed_t speed = self->Speed;
	angle_t rangle;
	AActor *mo;
	int ix;

	fixedvec3 pos = self->Vec3Angle(dist, self->angle, -self->floorclip + (self->height >> 1));
	for (ix=0; ix<5; ix++)
	{
		mo = Spawn("SorcSpark1", pos, ALLOW_REPLACE);
		if (mo)
		{
			rangle = (self->angle >> ANGLETOFINESHIFT) + ((pr_heresiarch()%5) << 1);
			mo->velx = FixedMul(pr_heresiarch()%speed, finecosine[rangle]);
			mo->vely = FixedMul(pr_heresiarch()%speed, finesine[rangle]);
			mo->velz = FRACUNIT*2;
		}
	}
}


//============================================================================
//
// A_SorcFX1Seek
//
// Yellow spell - offense
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_SorcFX1Seek)
{
	A_DoBounceCheck (self, "SorcererHeadScream");
	P_SeekerMissile (self,ANGLE_1*2,ANGLE_1*6);
}


//============================================================================
//
// A_SorcFX2Split
//
// Blue spell - defense
//
//============================================================================
//
// FX2 Variables
//		special1		current angle
//		special2
//		args[0]		0 = CW,  1 = CCW
//		args[1]		
//============================================================================

// Split ball in two
DEFINE_ACTION_FUNCTION(AActor, A_SorcFX2Split)
{
	AActor *mo;

	mo = Spawn(self->GetClass(), self->Pos(), NO_REPLACE);
	if (mo)
	{
		mo->target = self->target;
		mo->args[0] = 0;									// CW
		mo->special1 = self->angle;					// Set angle
		mo->SetState (mo->FindState("Orbit"));
	}
	mo = Spawn(self->GetClass(), self->Pos(), NO_REPLACE);
	if (mo)
	{
		mo->target = self->target;
		mo->args[0] = 1;									// CCW
		mo->special1 = self->angle;					// Set angle
		mo->SetState (mo->FindState("Orbit"));
	}
	self->Destroy ();
}

//============================================================================
//
// A_SorcFX2Orbit
//
// Orbit FX2 about sorcerer
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_SorcFX2Orbit)
{
	angle_t angle;
	fixedvec3 pos;
	AActor *parent = self->target;

	// [RH] If no parent, then disappear
	if (parent == NULL)
	{
		self->Destroy();
		return;
	}

	fixed_t dist = parent->radius;

	if ((parent->health <= 0) ||		// Sorcerer is dead
		(!parent->args[0]))				// Time expired
	{
		self->SetState (self->FindState(NAME_Death));
		parent->args[0] = 0;
		parent->flags2 &= ~MF2_REFLECTIVE;
		parent->flags2 &= ~MF2_INVULNERABLE;
	}

	if (self->args[0] && (parent->args[0]-- <= 0))		// Time expired
	{
		self->SetState (self->FindState(NAME_Death));
		parent->args[0] = 0;
		parent->flags2 &= ~MF2_REFLECTIVE;
	}

	// Move to new position based on angle
	if (self->args[0])		// Counter clock-wise
	{
		self->special1 += ANGLE_1*10;
		angle = ((angle_t)self->special1) >> ANGLETOFINESHIFT;
		pos = parent->Vec3Offset(
			FixedMul(dist, finecosine[angle]),
			FixedMul(dist, finesine[angle]),
			parent->floorclip + SORC_DEFENSE_HEIGHT*FRACUNIT);
		pos.z += FixedMul(15*FRACUNIT,finecosine[angle]);
		// Spawn trailer
		Spawn("SorcFX2T1", pos, ALLOW_REPLACE);
	}
	else							// Clock wise
	{
		self->special1 -= ANGLE_1*10;
		angle = ((angle_t)self->special1) >> ANGLETOFINESHIFT;
		pos = parent->Vec3Offset(
			FixedMul(dist, finecosine[angle]),
			FixedMul(dist, finesine[angle]),
			parent->floorclip + SORC_DEFENSE_HEIGHT*FRACUNIT);
		pos.z += FixedMul(20*FRACUNIT,finesine[angle]);
		// Spawn trailer
		Spawn("SorcFX2T1", pos, ALLOW_REPLACE);
	}

	self->SetOrigin (pos, true);
	self->floorz = parent->floorz;
	self->ceilingz = parent->ceilingz;
}

//============================================================================
//
// A_SpawnBishop
//
// Green spell - spawn bishops
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_SpawnBishop)
{
	AActor *mo;
	mo = Spawn("Bishop", self->Pos(), ALLOW_REPLACE);
	if (mo)
	{
		if (!P_TestMobjLocation(mo))
		{
			mo->ClearCounters();
			mo->Destroy ();
		}
		else if (self->target != NULL)
		{ // [RH] Make the new bishops inherit the Heriarch's target
			mo->CopyFriendliness (self->target, true);
			mo->master = self->target;
		}
	}
	self->Destroy ();
}

//============================================================================
//
// A_SorcererBishopEntry
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_SorcererBishopEntry)
{
	Spawn("SorcFX3Explosion", self->Pos(), ALLOW_REPLACE);
	S_Sound (self, CHAN_VOICE, self->SeeSound, 1, ATTN_NORM);
}

//============================================================================
//
// A_SorcFX4Check
//
// FX4 - rapid fire balls
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_SorcFX4Check)
{
	if (self->special2-- <= 0)
	{
		self->SetState (self->FindState(NAME_Death));
	}
}

//============================================================================
//
// A_SorcBallPop
//
// Ball death - bounce away in a random direction
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_SorcBallPop)
{
	S_Sound (self, CHAN_BODY, "SorcererBallPop", 1, ATTN_NONE);
	self->flags &= ~MF_NOGRAVITY;
	self->gravity = FRACUNIT/8;
	self->velx = ((pr_heresiarch()%10)-5) << FRACBITS;
	self->vely = ((pr_heresiarch()%10)-5) << FRACBITS;
	self->velz = (2+(pr_heresiarch()%3)) << FRACBITS;
	self->special2 = 4*FRACUNIT;		// Initial bounce factor
	self->args[4] = BOUNCE_TIME_UNIT;	// Bounce time unit
	self->args[3] = 5;					// Bounce time in seconds
}

//============================================================================
//
// A_DoBounceCheck
//
//============================================================================

void A_DoBounceCheck (AActor *self, const char *sound)
{
	if (self->args[4]-- <= 0)
	{
		if (self->args[3]-- <= 0)
		{
			self->SetState (self->FindState(NAME_Death));
			S_Sound (self, CHAN_BODY, sound, 1, ATTN_NONE);
		}
		else
		{
			self->args[4] = BOUNCE_TIME_UNIT;
		}
	}
}

//============================================================================
//
// A_BounceCheck
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_BounceCheck)
{
	A_DoBounceCheck (self, "SorcererBigBallExplode");
}
