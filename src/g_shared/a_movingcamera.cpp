/*
** a_movingcamera.cpp
** Cameras that move and related neat stuff
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
#include "p_local.h"
#include "p_lnspec.h"
#include "doomstat.h"
#include "farchive.h"

/*
== InterpolationPoint: node along a camera's path
==
== args[0] = pitch
== args[1] = time (in octics) to get here from previous node
== args[2] = time (in octics) to stay here before moving to next node
== args[3] = low byte of next node's tid
== args[4] = high byte of next node's tid
*/

class AInterpolationPoint : public AActor
{
	DECLARE_CLASS (AInterpolationPoint, AActor)
	HAS_OBJECT_POINTERS
public:
	void BeginPlay ();
	void HandleSpawnFlags ();
	void Tick () {}		// Nodes do no thinking
	AInterpolationPoint *ScanForLoop ();
	void FormChain ();

	void Serialize (FArchive &arc);

	TObjPtr<AInterpolationPoint> Next;
};

IMPLEMENT_POINTY_CLASS (AInterpolationPoint)
 DECLARE_POINTER (Next)
END_POINTERS

void AInterpolationPoint::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	arc << Next;
}

void AInterpolationPoint::BeginPlay ()
{
	Super::BeginPlay ();
	Next = NULL;
}

void AInterpolationPoint::HandleSpawnFlags ()
{
	// Spawn flags mean nothing to an interpolation point
}

void AInterpolationPoint::FormChain ()
{
	if (flags & MF_AMBUSH)
		return;

	flags |= MF_AMBUSH;

	TActorIterator<AInterpolationPoint> iterator (args[3] + 256 * args[4]);
	Next = iterator.Next ();

	if (Next == this)	// Don't link to self
		Next = iterator.Next ();

	if (Next == NULL && (args[3] | args[4]))
		Printf ("Can't find target for camera node %d\n", tid);

	Angles.Pitch = (double)clamp<int>((signed char)args[0], -89, 89);

	if (Next != NULL)
		Next->FormChain ();
}

// Return the node (if any) where a path loops, relative to this one.
AInterpolationPoint *AInterpolationPoint::ScanForLoop ()
{
	AInterpolationPoint *node = this;
	while (node->Next && node->Next != this && node->special1 == 0)
	{
		node->special1 = 1;
		node = node->Next;
	}
	return node->Next == this ? node : NULL;
}

/*
== InterpolationSpecial: Holds a special to execute when a
==  PathFollower reaches an InterpolationPoint of the same TID.
*/

class AInterpolationSpecial : public AActor
{
	DECLARE_CLASS (AInterpolationSpecial, AActor)
public:
	void Tick () {}		// Does absolutely nothing itself
};

IMPLEMENT_CLASS (AInterpolationSpecial)

/*
== PathFollower: something that follows a camera path
==		Base class for some moving cameras
==
== args[0] = low byte of first node in path's tid
== args[1] = high byte of first node's tid
== args[2] = bit 0 = follow a linear path (rather than curved)
==			 bit 1 = adjust angle
==			 bit 2 = adjust pitch
==			 bit 3 = aim in direction of motion
==
== Also uses:
==	target = first node in path
==	lastenemy = node prior to first node (if looped)
*/

class APathFollower : public AActor
{
	DECLARE_CLASS (APathFollower, AActor)
	HAS_OBJECT_POINTERS
public:
	void BeginPlay ();
	void PostBeginPlay ();
	void Tick ();
	void Activate (AActor *activator);
	void Deactivate (AActor *activator);
protected:
	double Splerp (double p1, double p2, double p3, double p4);
	double Lerp (double p1, double p2);
	virtual bool Interpolate ();
	virtual void NewNode ();

	void Serialize (FArchive &arc);

	bool bActive, bJustStepped;
	TObjPtr<AInterpolationPoint> PrevNode, CurrNode;
	float Time;		// Runs from 0.0 to 1.0 between CurrNode and CurrNode->Next
	int HoldTime;
};

IMPLEMENT_POINTY_CLASS (APathFollower)
 DECLARE_POINTER (PrevNode)
 DECLARE_POINTER (CurrNode)
END_POINTERS

void APathFollower::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	arc << bActive << bJustStepped << PrevNode << CurrNode << Time << HoldTime;
}

// Interpolate between p2 and p3 along a Catmull-Rom spline
// http://research.microsoft.com/~hollasch/cgindex/curves/catmull-rom.html
double APathFollower::Splerp (double p1, double p2, double p3, double p4)
{
	double t = Time;
	double res = 2*p2;
	res += (p3 - p1) * Time;
	t *= Time;
	res += (2*p1 - 5*p2 + 4*p3 - p4) * t;
	t *= Time;
	res += (3*p2 - 3*p3 + p4 - p1) * t;
	return 0.5f * res;
}

// Linearly interpolate between p1 and p2
double APathFollower::Lerp (double p1, double p2)
{
	return p1 + Time * (p2 - p1);
}

void APathFollower::BeginPlay ()
{
	Super::BeginPlay ();
	PrevNode = CurrNode = NULL;
	bActive = false;
}

void APathFollower::PostBeginPlay ()
{
	// Find first node of path
	TActorIterator<AInterpolationPoint> iterator (args[0] + 256 * args[1]);
	AInterpolationPoint *node = iterator.Next ();
	AInterpolationPoint *prevnode;

	target = node;

	if (node == NULL)
	{
		Printf ("PathFollower %d: Can't find interpolation pt %d\n",
			tid, args[0] + 256 * args[1]);
		return;
	}

	// Verify the path has enough nodes
	node->FormChain ();
	if (args[2] & 1)
	{	// linear path; need 2 nodes
		if (node->Next == NULL)
		{
			Printf ("PathFollower %d: Path needs at least 2 nodes\n", tid);
			return;
		}
		lastenemy = NULL;
	}
	else
	{	// spline path; need 4 nodes
		if (node->Next == NULL ||
			node->Next->Next == NULL ||
			node->Next->Next->Next == NULL)
		{
			Printf ("PathFollower %d: Path needs at least 4 nodes\n", tid);
			return;
		}
		// If the first node is in a loop, we can start there.
		// Otherwise, we need to start at the second node in the path.
		prevnode = node->ScanForLoop ();
		if (prevnode == NULL || prevnode->Next != node)
		{
			lastenemy = target;
			target = node->Next;
		}
		else
		{
			lastenemy = prevnode;
		}
	}
}

void APathFollower::Deactivate (AActor *activator)
{
	bActive = false;
}

void APathFollower::Activate (AActor *activator)
{
	if (!bActive)
	{
		CurrNode = barrier_cast<AInterpolationPoint *>(target);
		PrevNode = barrier_cast<AInterpolationPoint *>(lastenemy);

		if (CurrNode != NULL)
		{
			NewNode ();
			SetOrigin (CurrNode->Pos(), false);
			Time = 0.f;
			HoldTime = 0;
			bJustStepped = true;
			bActive = true;
		}
	}
}

void APathFollower::Tick ()
{
	if (!bActive)
		return;

	if (bJustStepped)
	{
		bJustStepped = false;
		if (CurrNode->args[2])
		{
			HoldTime = level.time + CurrNode->args[2] * TICRATE / 8;
			SetXYZ(CurrNode->Pos());
		}
	}

	if (HoldTime > level.time)
		return;

	// Splines must have a previous node.
	if (PrevNode == NULL && !(args[2] & 1))
	{
		bActive = false;
		return;
	}

	// All paths must have a current node.
	if (CurrNode->Next == NULL)
	{
		bActive = false;
		return;
	}

	if (Interpolate ())
	{
		Time += float(8.f / ((double)CurrNode->args[1] * (double)TICRATE));
		if (Time > 1.f)
		{
			Time -= 1.f;
			bJustStepped = true;
			PrevNode = CurrNode;
			CurrNode = CurrNode->Next;
			if (CurrNode != NULL)
				NewNode ();
			if (CurrNode == NULL || CurrNode->Next == NULL)
				Deactivate (this);
			if ((args[2] & 1) == 0 && CurrNode->Next->Next == NULL)
				Deactivate (this);
		}
	}
}

void APathFollower::NewNode ()
{
	TActorIterator<AInterpolationSpecial> iterator (CurrNode->tid);
	AInterpolationSpecial *spec;

	while ( (spec = iterator.Next ()) )
	{
		P_ExecuteSpecial(spec->special, NULL, NULL, false, spec->args[0],
			spec->args[1], spec->args[2], spec->args[3], spec->args[4]);
	}
}

bool APathFollower::Interpolate ()
{
	DVector3 dpos(0, 0, 0);

	if ((args[2] & 8) && Time > 0.f)
	{
		dpos = Pos();
	}

	if (CurrNode->Next==NULL) return false;

	UnlinkFromWorld ();
	DVector3 newpos;
	if (args[2] & 1)
	{	// linear
		newpos.X = Lerp(CurrNode->X(), CurrNode->Next->X());
		newpos.Y = Lerp(CurrNode->Y(), CurrNode->Next->Y());
		newpos.Z = Lerp(CurrNode->Z(), CurrNode->Next->Z());
	}
	else
	{	// spline
		if (CurrNode->Next->Next==NULL) return false;

		newpos.X = Splerp(PrevNode->X(), CurrNode->X(), CurrNode->Next->X(), CurrNode->Next->Next->X());
		newpos.Y = Splerp(PrevNode->Y(), CurrNode->Y(), CurrNode->Next->Y(), CurrNode->Next->Next->Y());
		newpos.Z = Splerp(PrevNode->Z(), CurrNode->Z(), CurrNode->Next->Z(), CurrNode->Next->Next->Z());
	}
	SetXYZ(newpos);
	LinkToWorld ();

	if (args[2] & 6)
	{
		if (args[2] & 8)
		{
			if (args[2] & 1)
			{ // linear
				dpos.X = CurrNode->Next->X() - CurrNode->X();
				dpos.Y = CurrNode->Next->Y() - CurrNode->Y();
				dpos.Z = CurrNode->Next->Z() - CurrNode->Z();
			}
			else if (Time > 0.f)
			{ // spline
				dpos = newpos - dpos;
			}
			else
			{
				int realarg = args[2];
				args[2] &= ~(2|4|8);
				Time += 0.1f;
				dpos = newpos;
				Interpolate ();
				Time -= 0.1f;
				args[2] = realarg;
				dpos = newpos - dpos;
				newpos -= dpos;
				SetXYZ(newpos);
			}
			if (args[2] & 2)
			{ // adjust yaw
				Angles.Yaw = dpos.Angle();
			}
			if (args[2] & 4)
			{ // adjust pitch; use floats for precision
				double dist = dpos.XY().Length();
				Angles.Pitch = dist != 0.f ? VecToAngle(dist, -dpos.Z) : 0.;
			}
		}
		else
		{
			if (args[2] & 2)
			{ // interpolate angle
				DAngle angle1 = CurrNode->Angles.Yaw.Normalized180();
				DAngle angle2 = angle1 + deltaangle(angle1, CurrNode->Next->Angles.Yaw);
				Angles.Yaw = Lerp(angle1.Degrees, angle2.Degrees);
			}
			if (args[2] & 1)
			{ // linear
				if (args[2] & 4)
				{ // interpolate pitch
					Angles.Pitch = Lerp(CurrNode->Angles.Pitch.Degrees, CurrNode->Next->Angles.Pitch.Degrees);
				}
			}
			else
			{ // spline
				if (args[2] & 4)
				{ // interpolate pitch
					Angles.Pitch = Splerp(PrevNode->Angles.Pitch.Degrees, CurrNode->Angles.Pitch.Degrees,
						CurrNode->Next->Angles.Pitch.Degrees, CurrNode->Next->Next->Angles.Pitch.Degrees);
				}
			}
		}
	}

	return true;
}

/*
== ActorMover: Moves any actor along a camera path
==
== Same as PathFollower, except
== args[2], bit 7: make nonsolid
== args[3] = tid of thing to move
==
== also uses:
==	tracer = thing to move
*/

class AActorMover : public APathFollower
{
	DECLARE_CLASS (AActorMover, APathFollower)
public:
	void BeginPlay();
	void PostBeginPlay ();
	void Activate (AActor *activator);
	void Deactivate (AActor *activator);
protected:
	bool Interpolate ();
};

IMPLEMENT_CLASS (AActorMover)

void AActorMover::BeginPlay()
{
	ChangeStatNum(STAT_ACTORMOVER);
}

void AActorMover::PostBeginPlay ()
{
	Super::PostBeginPlay ();

	TActorIterator<AActor> iterator (args[3]);
	tracer = iterator.Next ();

	if (tracer == NULL)
	{
		Printf ("ActorMover %d: Can't find target %d\n", tid, args[3]);
	}
	else
	{
		special1 = tracer->flags;
		special2 = tracer->flags2;
	}
}

bool AActorMover::Interpolate ()
{
	if (tracer == NULL)
		return true;

	if (Super::Interpolate ())
	{
		double savedz = tracer->Z();
		tracer->SetZ(Z());
		if (!P_TryMove (tracer, Pos(), true))
		{
			tracer->SetZ(savedz);
			return false;
		}

		if (args[2] & 2)
			tracer->Angles.Yaw = Angles.Yaw;
		if (args[2] & 4)
			tracer->Angles.Pitch = Angles.Pitch;

		return true;
	}
	return false;
}

void AActorMover::Activate (AActor *activator)
{
	if (tracer == NULL || bActive)
		return;

	Super::Activate (activator);
	special1 = tracer->flags;
	special2 = tracer->flags2;
	tracer->flags |= MF_NOGRAVITY;
	if (args[2] & 128)
	{
		tracer->UnlinkFromWorld ();
		tracer->flags |= MF_NOBLOCKMAP;
		tracer->flags &= ~MF_SOLID;
		tracer->LinkToWorld ();
	}
	if (tracer->flags3 & MF3_ISMONSTER)
	{
		tracer->flags2 |= MF2_INVULNERABLE | MF2_DORMANT;
	}
	// Don't let the renderer interpolate between the actor's
	// old position and its new position.
	Interpolate ();
	tracer->ClearInterpolation();
}

void AActorMover::Deactivate (AActor *activator)
{
	if (bActive)
	{
		Super::Deactivate (activator);
		if (tracer != NULL)
		{
			tracer->UnlinkFromWorld ();
			tracer->flags = ActorFlags::FromInt (special1);
			tracer->LinkToWorld ();
			tracer->flags2 = ActorFlags2::FromInt (special2);
		}
	}
}

/*
== MovingCamera: Moves any actor along a camera path
==
== Same as PathFollower, except
== args[3] = tid of thing to look at (0 if none)
==
== Also uses:
==	tracer = thing to look at
*/

class AMovingCamera : public APathFollower
{
	DECLARE_CLASS (AMovingCamera, APathFollower)
	HAS_OBJECT_POINTERS
public:
	void PostBeginPlay ();

	void Serialize (FArchive &arc);
protected:
	bool Interpolate ();

	TObjPtr<AActor> Activator;
};

IMPLEMENT_POINTY_CLASS (AMovingCamera)
 DECLARE_POINTER (Activator)
END_POINTERS

void AMovingCamera::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	arc << Activator;
}

void AMovingCamera::PostBeginPlay ()
{
	Super::PostBeginPlay ();

	Activator = NULL;
	if (args[3] != 0)
	{
		TActorIterator<AActor> iterator (args[3]);
		tracer = iterator.Next ();
		if (tracer == NULL)
		{
			Printf ("MovingCamera %d: Can't find thing %d\n", tid, args[3]);
		}
	}
}

bool AMovingCamera::Interpolate ()
{
	if (tracer == NULL)
		return Super::Interpolate ();

	if (Super::Interpolate ())
	{
		Angles.Yaw = AngleTo(tracer, true);

		if (args[2] & 4)
		{ // Also aim camera's pitch;
			DVector3 diff = Pos() - tracer->PosPlusZ(tracer->Height / 2);
			double dist = diff.XY().Length();
			Angles.Pitch = dist != 0.f ? VecToAngle(dist, diff.Z) : 0.;
		}

		return true;
	}
	return false;
}
