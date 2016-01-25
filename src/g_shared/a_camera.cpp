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
	angle_t Center;
	angle_t Acc;
	angle_t Delta;
	angle_t Range;
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

void ASecurityCamera::Tick ()
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
	DECLARE_CLASS (AAimingCamera, ASecurityCamera)
public:
	void PostBeginPlay ();
	void Tick ();

	void Serialize (FArchive &arc);
protected:
	int MaxPitchChange;
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
	MaxPitchChange = (int)((float)changepitch * 536870912.f / 45.f / (float)TICRATE);
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
			fixedvec2 fv3 = tracer->Vec2To(this);
			TVector2<double> vect(fv3.x, fv3.y);
			double dz = Z() - tracer->Z() - tracer->height/2;
			double dist = vect.Length();
			double ang = dist != 0.f ? atan2 (dz, dist) : 0;
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
