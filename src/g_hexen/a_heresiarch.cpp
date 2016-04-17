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
//		specialf1		Angle of ball 1 (all others relative to that)
//		StopBall		which ball to stop at in stop mode (MT_???)
//		args[0]			Defense time
//		args[1]			Number of full rotations since stopping mode
//		args[2]			Target orbit speed for acceleration/deceleration
//		args[3]			Movement mode (see SORC_ macros)
//		args[4]			Current ball orbit speed
//	Sorcerer Ball Variables
//		specialf1		Previous angle of ball (for woosh)
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

#define BALL1_ANGLEOFFSET	0.
#define BALL2_ANGLEOFFSET	120.
#define BALL3_ANGLEOFFSET	240.

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
	DAngle BallAngle;

	void Serialize (FArchive &arc);
	void Die (AActor *source, AActor *inflictor, int dmgflags);
};

IMPLEMENT_CLASS (AHeresiarch)

void AHeresiarch::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	arc << StopBall << BallAngle;
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
	DAngle AngleOffset;
	DAngle OldAngle;

	void Serialize (FArchive &arc)
	{
		Super::Serialize (arc);
		arc << AngleOffset << OldAngle;
	}

	bool SpecialBlastHandling (AActor *source, double strength)
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
	PARAM_ACTION_PROLOGUE_TYPE(AHeresiarch);

	AActor *mo;

	self->SpawnState += 2;		// [RH] Don't spawn balls again
	A_SlowBalls(self);
	self->args[0] = 0;								// Currently no defense
	self->args[3] = SORC_NORMAL;
	self->args[4] = SORCBALL_INITIAL_SPEED;		// Initial orbit speed
	self->BallAngle = 1.;

	DVector3 pos = self->PosPlusZ(-self->Floorclip + self->Height);
	
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
	return 0;
}


//============================================================================
//
// A_SorcBallOrbit
//
// - actor is ball
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_SorcBallOrbit)
{
	PARAM_ACTION_PROLOGUE_TYPE(ASorcBall);

	// [RH] If no parent, then die instead of crashing
	if (self->target == NULL)
	{
		self->SetState (self->FindState(NAME_Pain));
		return 0;
	}

	int mode = self->target->args[3];
	AHeresiarch *parent = barrier_cast<AHeresiarch *>(self->target);
	double dist = parent->radius - (self->radius*2);

#if 0
	// This cannot happen anymore because this is defined locally in SorcBall
	if (!self->IsKindOf (RUNTIME_CLASS(ASorcBall)))
	{
		I_Error ("Corrupted sorcerer:\nTried to use a %s", self->GetClass()->TypeName.GetChars());
	}
#endif

	if (self->target->health <= 0)
	{
		self->SetState (self->FindState(NAME_Pain));
		return 0;
	}

	DAngle prevangle = self->OldAngle;
	DAngle baseangle = parent->BallAngle;
	DAngle angle = baseangle + self->AngleOffset;

	self->Angles.Yaw = angle;

	switch (mode)
	{
	case SORC_NORMAL:			// Balls rotating normally
		self->SorcUpdateBallAngle ();
		break;

	case SORC_DECELERATE:		// Balls decelerating
		A_DecelBalls(self);
		self->SorcUpdateBallAngle ();
		break;

	case SORC_ACCELERATE:		// Balls accelerating
		A_AccelBalls(self);
		self->SorcUpdateBallAngle ();
		break;

	case SORC_STOPPING:			// Balls stopping
		if ((parent->StopBall == self->GetClass()) &&
			 (parent->args[1] > SORCBALL_SPEED_ROTATIONS) &&
			 absangle(angle, parent->Angles.Yaw) < 42.1875)
		{
			// Can stop now
			self->target->args[3] = SORC_FIRESPELL;
			self->target->args[4] = 0;
			// Set angle so self angle == sorcerer angle
			parent->BallAngle = parent->Angles.Yaw - self->AngleOffset;
		}
		else
		{
			self->SorcUpdateBallAngle ();
		}
		break;

	case SORC_FIRESPELL:			// Casting spell
		if (parent->StopBall == self->GetClass())
		{
			// Put sorcerer into special throw spell anim
			if (parent->health > 0)
				parent->SetState (parent->FindState("Attack1"));

			self->DoFireSpell ();
		}
		break;

	case SORC_FIRING_SPELL:
		if (parent->StopBall == self->GetClass())
		{
			if (self->special2-- <= 0)
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
				A_SorcOffense2(self);
			}
		}
		break;

	case SORC_STOPPED:			// Balls stopped
	default:
		break;
	}

	if ( angle.BAMs() < prevangle.BAMs() && (parent->args[4]==SORCBALL_TERMINAL_SPEED))
	{
		parent->args[1]++;			// Bump rotation counter
		// Completed full rotation - make woosh sound
		S_Sound (self, CHAN_BODY, "SorcererBallWoosh", 1, ATTN_NORM);
	}
	self->OldAngle = angle;		// Set previous angle

	DVector3 pos = parent->Vec3Angle(dist, angle, -parent->Floorclip + parent->Height);
	self->SetOrigin (pos, true);
	self->floorz = parent->floorz;
	self->ceilingz = parent->ceilingz;
	return 0;
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
	PARAM_ACTION_PROLOGUE;

	self->args[3] = SORC_ACCELERATE;				// speed mode
	self->args[2] = SORCBALL_TERMINAL_SPEED;		// target speed
	return 0;
}


//============================================================================
//
// A_SlowBalls
//
// Set balls to slow mode - actor is sorcerer
//
//============================================================================

void A_SlowBalls(AActor *self)
{
	self->args[3] = SORC_DECELERATE;				// slow mode
	self->args[2] = SORCBALL_INITIAL_SPEED;		// target speed
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
	AHeresiarch *self = static_cast<AHeresiarch *> (scary);
	int chance = pr_heresiarch();
	self->args[3] = SORC_STOPPING;				// stopping mode
	self->args[1] = 0;							// Reset rotation counter

	if ((self->args[0] <= 0) && (chance < 200))
	{
		self->StopBall = RUNTIME_CLASS(ASorcBall2);	// Blue
	}
	else if((self->health < (self->SpawnHealth() >> 1)) &&
			(chance < 200))
	{
		self->StopBall = RUNTIME_CLASS(ASorcBall3);	// Green
	}
	else
	{
		self->StopBall = RUNTIME_CLASS(ASorcBall1);	// Yellow
	}
}

//============================================================================
//
// A_AccelBalls
//
// Increase ball orbit speed - actor is ball
//
//============================================================================

void A_AccelBalls(AActor *self)
{
	AActor *sorc = self->target;

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

void A_DecelBalls(AActor *self)
{
	AActor *sorc = self->target;

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
	barrier_cast<AHeresiarch*>(target)->BallAngle += target->args[4];
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

	mo = Spawn("SorcFX2", PosPlusZ(parent->Floorclip + SORC_DEFENSE_HEIGHT), ALLOW_REPLACE);
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
	DAngle ang1, ang2;
	AActor *parent = target;

	ang1 = Angles.Yaw.Degrees - 45;
	ang2 = Angles.Yaw.Degrees + 45;
	PClassActor *cls = PClass::FindActor("SorcFX3");
	if (health < (SpawnHealth()/3))
	{	// Spawn 2 at a time
		mo = P_SpawnMissileAngle(parent, cls, ang1, 4.);
		if (mo) mo->target = parent;
		mo = P_SpawnMissileAngle(parent, cls, ang2, 4.);
		if (mo) mo->target = parent;
	}			
	else
	{
		if (pr_heresiarch() < 128)
			ang1 = ang2;
		mo = P_SpawnMissileAngle(parent, cls, ang1, 4.);
		if (mo) mo->target = parent;
	}
}


/*
void A_SpawnReinforcements(AActor *actor)
{
	AActor *parent = self->target;
	AActor *mo;
	DAngle ang;

	ang = P_Random();
	mo = P_SpawnMissileAngle(actor, MT_SORCFX3, ang, 5.);
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
	DAngle ang1, ang2;
	AActor *parent = target;

	ang1 = Angles.Yaw.Degrees + 70;
	ang2 = Angles.Yaw.Degrees - 70;
	PClassActor *cls = PClass::FindActor("SorcFX1");
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

void A_SorcOffense2(AActor *self)
{
	DAngle ang1;
	AActor *mo;
	double delta;
	int index;
	AActor *parent = self->target;
	AActor *dest = parent->target;
	double dist;

	// [RH] If no enemy, then don't try to shoot.
	if (dest == NULL)
	{
		return;
	}

	index = self->args[4];
	self->args[4] = (self->args[4] + 15) & 255;
	delta = DAngle(index * (360 / 256.f)).Sin() * SORCFX4_SPREAD_ANGLE;

	ang1 = self->Angles.Yaw + delta;
	mo = P_SpawnMissileAngle(parent, PClass::FindActor("SorcFX4"), ang1, 0);
	if (mo)
	{
		mo->special2 = 35*5/2;		// 5 seconds
		dist = mo->DistanceBySpeed(dest, mo->Speed);
		mo->Vel.Z = (dest->Z() - mo->Z()) / dist;
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
	PARAM_ACTION_PROLOGUE;

	self->args[3] = SORC_ACCELERATE;
	self->args[2] = SORCBALL_INITIAL_SPEED;
	return 0;
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
	PARAM_ACTION_PROLOGUE;
	int speed = (int)self->Speed;
	DAngle rangle;
	AActor *mo;
	int ix;

	DVector3 pos = self->Vec3Angle(5., self->Angles.Yaw, -self->Floorclip + self->Height / 2. );
	for (ix=0; ix<5; ix++)
	{
		mo = Spawn("SorcSpark1", pos, ALLOW_REPLACE);
		if (mo)
		{
			rangle = self->Angles.Yaw + (pr_heresiarch() % 5) * (4096 / 360.);
			mo->Vel.X = (pr_heresiarch() % speed) * rangle.Cos();
			mo->Vel.Y = (pr_heresiarch() % speed) * rangle.Sin();
			mo->Vel.Z = 2;
		}
	}
	return 0;
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
	PARAM_ACTION_PROLOGUE;

	A_DoBounceCheck (self, "SorcererHeadScream");
	P_SeekerMissile(self, 2, 6);
	return 0;
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
//		specialf1		current angle
//		special2
//		args[0]		0 = CW,  1 = CCW
//		args[1]		
//============================================================================

// Split ball in two
DEFINE_ACTION_FUNCTION(AActor, A_SorcFX2Split)
{
	PARAM_ACTION_PROLOGUE;

	AActor *mo;

	mo = Spawn(self->GetClass(), self->Pos(), NO_REPLACE);
	if (mo)
	{
		mo->target = self->target;
		mo->args[0] = 0;									// CW
		mo->specialf1 = self->Angles.Yaw.Degrees;			// Set angle
		mo->SetState (mo->FindState("Orbit"));
	}
	mo = Spawn(self->GetClass(), self->Pos(), NO_REPLACE);
	if (mo)
	{
		mo->target = self->target;
		mo->args[0] = 1;									// CCW
		mo->specialf1 = self->Angles.Yaw.Degrees;			// Set angle
		mo->SetState (mo->FindState("Orbit"));
	}
	self->Destroy ();
	return 0;
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
	PARAM_ACTION_PROLOGUE;

	DAngle angle;
	DVector3 pos;
	AActor *parent = self->target;

	// [RH] If no parent, then disappear
	if (parent == NULL)
	{
		self->Destroy();
		return 0;
	}

	double dist = parent->radius;

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
		self->specialf1 += 10;
		angle = self->specialf1;
		pos = parent->Vec3Angle(dist, angle, parent->Floorclip + SORC_DEFENSE_HEIGHT);
		pos.Z += 15 * angle.Cos();
		// Spawn trailer
		Spawn("SorcFX2T1", pos, ALLOW_REPLACE);
	}
	else							// Clock wise
	{
		self->specialf1 -= 10;
		angle = self->specialf1;
		pos = parent->Vec3Angle(dist, angle, parent->Floorclip + SORC_DEFENSE_HEIGHT);
		pos.Z += 20 * angle.Sin();
		// Spawn trailer
		Spawn("SorcFX2T1", pos, ALLOW_REPLACE);
	}

	self->SetOrigin (pos, true);
	self->floorz = parent->floorz;
	self->ceilingz = parent->ceilingz;
	return 0;
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
	PARAM_ACTION_PROLOGUE;

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
	return 0;
}

//============================================================================
//
// A_SorcererBishopEntry
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_SorcererBishopEntry)
{
	PARAM_ACTION_PROLOGUE;

	Spawn("SorcFX3Explosion", self->Pos(), ALLOW_REPLACE);
	S_Sound (self, CHAN_VOICE, self->SeeSound, 1, ATTN_NORM);
	return 0;
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
	PARAM_ACTION_PROLOGUE;

	if (self->special2-- <= 0)
	{
		self->SetState (self->FindState(NAME_Death));
	}
	return 0;
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
	PARAM_ACTION_PROLOGUE;

	S_Sound (self, CHAN_BODY, "SorcererBallPop", 1, ATTN_NONE);
	self->flags &= ~MF_NOGRAVITY;
	self->Gravity = 1. / 8;;

	self->Vel.X = ((pr_heresiarch()%10)-5);
	self->Vel.Y = ((pr_heresiarch()%10)-5);
	self->Vel.Z = (2+(pr_heresiarch()%3));
	self->args[4] = BOUNCE_TIME_UNIT;	// Bounce time unit
	self->args[3] = 5;					// Bounce time in seconds
	return 0;
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
	PARAM_ACTION_PROLOGUE;

	A_DoBounceCheck (self, "SorcererBigBallExplode");
	return 0;
}
