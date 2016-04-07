/*
** a_camera.cpp
** Implements the Duke Nukem 3D-ish security camera
**
**---------------------------------------------------------------------------
** Copyright 1998-2006 Randy Heit
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
*/

#include "actor.h"
#include "info.h"
#include "a_sharedglobal.h"
#include "p_local.h"
#include "farchive.h"
#include "math/cmath.h"

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
	DECLARE_CLASS (ASecurityCamera, AActor)
public:
	void PostBeginPlay ();
	void Tick ();

	void Serialize (FArchive &arc);
protected:
	DAngle Center;
	DAngle Acc;
	DAngle Delta;
	DAngle Range;
};

IMPLEMENT_CLASS (ASecurityCamera)

void ASecurityCamera::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	arc << Center << Acc << Delta << Range;
}

void ASecurityCamera::PostBeginPlay ()
{
	Super::PostBeginPlay ();
	Center = Angles.Yaw;
	if (args[2])
		Delta = 360. / (args[2] * TICRATE / 8);
	else
		Delta = 0.;
	if (args[1])
		Delta /= 2;
	Acc = 0.;
	Angles.Pitch = (double)clamp<int>((signed char)args[0], -89, 89);
	Range = (double)args[1];
}

void ASecurityCamera::Tick ()
{
	Acc += Delta;
	if (Range != 0)
		Angles.Yaw = Center + Range * Acc.Sin();
	else if (Delta != 0)
		Angles.Yaw = Acc;
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
	DECLARE_CLASS (AAimingCamera, ASecurityCamera)
public:
	void PostBeginPlay ();
	void Tick ();

	void Serialize (FArchive &arc);
protected:
	DAngle MaxPitchChange;
};

IMPLEMENT_CLASS (AAimingCamera)

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
	MaxPitchChange = double(changepitch / TICRATE);
	Range /= TICRATE;

	TActorIterator<AActor> iterator (args[3]);
	tracer = iterator.Next ();
	if (tracer == NULL)
	{
		//Printf ("AimingCamera %d: Can't find TID %d\n", tid, args[3]);
	}
	else
	{ // Don't try for a new target upon losing this one.
		args[3] = 0;
	}
}

void AAimingCamera::Tick ()
{
	if (tracer == NULL && args[3] != 0)
	{ // Recheck, in case something with this TID was created since the last time.
		TActorIterator<AActor> iterator (args[3]);
		tracer = iterator.Next ();
	}
	if (tracer != NULL)
	{
		DAngle delta;
		int dir = P_FaceMobj (this, tracer, &delta);
		if (delta > Range)
		{
			delta = Range;
		}
		if (dir)
		{
			Angles.Yaw += delta;
		}
		else
		{
			Angles.Yaw -= delta;
		}
		if (MaxPitchChange != 0)
		{ // Aim camera's pitch; use floats for precision
			DVector2 vect = tracer->Vec2To(this);
			double dz = Z() - tracer->Z() - tracer->Height/2;
			double dist = vect.Length();
			DAngle desiredPitch = dist != 0.f ? VecToAngle(dist, dz) : 0.;
			DAngle diff = deltaangle(Angles.Pitch, desiredPitch);
			if (fabs (diff) < MaxPitchChange)
			{
				Angles.Pitch = desiredPitch;
			}
			else if (diff < 0)
			{
				Angles.Pitch -= MaxPitchChange;
			}
			else
			{
				Angles.Pitch += MaxPitchChange;
			}
		}
	}
}
