#include "actor.h"
#include "info.h"
#include "a_sharedglobal.h"
#include "r_main.h"
#include "p_local.h"
#include "vectors.h"

/*
== SecurityCamera
==
== args[0] = pitch
== args[1] = amount camera turns to either side of its initial position
==			 (in degrees)
== args[2] = octics to complete one cycle
*/

class ASecurityCamera : public AActor
{
	DECLARE_STATELESS_ACTOR (ASecurityCamera, AActor)
public:
	void PostBeginPlay ();
	void RunThink ();
	angle_t AngleIncrements ();

	void Serialize (FArchive &arc);
protected:
	angle_t Center;
	angle_t Acc;
	angle_t Delta;
	angle_t Range;
};

IMPLEMENT_STATELESS_ACTOR (ASecurityCamera, Any, 9025, 0)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY)
	PROP_RenderStyle (STYLE_None)
END_DEFAULTS

void ASecurityCamera::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	arc << Center << Acc << Delta << Range;
}

angle_t ASecurityCamera::AngleIncrements ()
{
	return ANGLE_1;
}

void ASecurityCamera::PostBeginPlay ()
{
	Super::PostBeginPlay ();
	Center = angle;
	if (args[2])
		Delta = ANGLE_MAX / (args[2] * TICRATE / 8);
	else
		Delta = 0;
	if (args[1])
		Delta /= 2;
	Acc = 0;
	pitch = (signed int)((char)args[0]) * ANGLE_1;
	if (pitch <= -ANGLE_90)
		pitch = -ANGLE_90 + ANGLE_1;
	else if (pitch >= ANGLE_90)
		pitch = ANGLE_90 - ANGLE_1;
	Range = (angle_t)((float)args[1] * 536870912.f / 45.f);
}

void ASecurityCamera::RunThink ()
{
	Acc += Delta;
	if (Range)
		angle = Center + FixedMul (Range, finesine[Acc >> ANGLETOFINESHIFT]);
	else if (Delta)
		angle = Acc;
}

/*
== AimingCamera
==
== args[0] = pitch
== args[1] = max turn (in degrees)
== args[2] = max pitch turn (in degrees)
== args[3] = tid of thing to look at
==
== Also uses:
==	tracer: thing to look at
*/

class AAimingCamera : public ASecurityCamera
{
	DECLARE_STATELESS_ACTOR (AAimingCamera, ASecurityCamera)
public:
	void PostBeginPlay ();
	void RunThink ();

	void Serialize (FArchive &arc);
protected:
	int MaxPitchChange;
};

IMPLEMENT_STATELESS_ACTOR (AAimingCamera, Any, 9073, 0)
END_DEFAULTS

void AAimingCamera::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	arc << MaxPitchChange;
}

void AAimingCamera::PostBeginPlay ()
{
	int changepitch = args[2];

	args[2] = 0;
	Super::PostBeginPlay ();
	MaxPitchChange = (int)((float)changepitch * 536870912.f / 45.f / (float)TICRATE);
	Range /= TICRATE;

	TActorIterator<AActor> iterator (args[3]);
	tracer = iterator.Next ();
	if (tracer == NULL)
	{
		Printf ("AimingCamera %d: Can't find thing %d\n", tid, args[3]);
	}
}

void AAimingCamera::RunThink ()
{
	if (tracer != NULL)
	{
		angle_t delta;
		int dir = P_FaceMobj (this, tracer, &delta);
		if (delta > Range)
		{
			delta = Range;
		}
		if (dir)
		{
			angle += delta;
		}
		else
		{
			angle -= delta;
		}
		if (MaxPitchChange)
		{ // Aim camera's pitch; use floats for precision
			float dx = FIXED2FLOAT(x - tracer->x);
			float dy = FIXED2FLOAT(y - tracer->y);
			float dz = FIXED2FLOAT(z - tracer->z - tracer->height/2);
			float dist = (float)sqrt (dx*dx + dy*dy);
			float ang = dist != 0.f ? (float)atan2 (dz, dist) : 0;
			int desiredpitch = (angle_t)(ang * 2147483648.f / PI);
			if (abs (desiredpitch - pitch) < MaxPitchChange)
			{
				pitch = desiredpitch;
			}
			else if (desiredpitch < pitch)
			{
				pitch -= MaxPitchChange;
			}
			else
			{
				pitch += MaxPitchChange;
			}
		}
	}
}
