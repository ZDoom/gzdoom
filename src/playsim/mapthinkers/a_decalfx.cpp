/*
** a_decalfx.h
** Decal animation thinkers
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

#include "decallib.h"
#include "a_decalfx.h"
#include "serializer_doom.h"
#include "serialize_obj.h"
#include "a_sharedglobal.h"
#include "g_levellocals.h"
#include "m_fixed.h"


IMPLEMENT_CLASS(DDecalThinker, false, true)

IMPLEMENT_POINTERS_START(DDecalThinker)
	IMPLEMENT_POINTER(TheDecal)
IMPLEMENT_POINTERS_END

void DDecalThinker::Serialize(FSerializer &arc)
{
	Super::Serialize (arc);
	arc("thedecal", TheDecal);
}

IMPLEMENT_CLASS(DDecalFader, false, false)

void DDecalFader::Serialize(FSerializer &arc)
{
	Super::Serialize (arc);
	arc("starttime", TimeToStartDecay)
		("endtime", TimeToEndDecay)
		("starttrans", StartTrans);
}

void DDecalFader::Tick ()
{
	if (TheDecal == nullptr)
	{
		Destroy ();
	}
	else
	{
		if (Level->maptime < TimeToStartDecay || Level->isFrozen())
		{
			return;
		}
		else if (Level->maptime >= TimeToEndDecay)
		{
			TheDecal->Expired();		// for impact decal bookkeeping.
			TheDecal->Destroy ();		// remove the decal
			Destroy ();					// remove myself
			return;
		}
		if (StartTrans == -1)
		{
			StartTrans = TheDecal->Alpha;
		}

		int distanceToEnd = TimeToEndDecay - Level->maptime;
		int fadeDistance = TimeToEndDecay - TimeToStartDecay;
		TheDecal->Alpha = StartTrans * distanceToEnd / fadeDistance;
	}
}

IMPLEMENT_CLASS(DDecalStretcher, false, false)

void DDecalStretcher::Serialize(FSerializer &arc)
{
	Super::Serialize (arc);
	arc("starttime", TimeToStart)
		("endtime", TimeToStop)
		("goalx", GoalX)
		("startx", StartX)
		("stretchx", bStretchX)
		("goaly", GoalY)
		("starty", StartY)
		("stretchy", bStretchY)
		("started", bStarted);
}

void DDecalStretcher::Tick ()
{
	if (TheDecal == nullptr)
	{
		Destroy ();
		return;
	}
	if (Level->maptime < TimeToStart || Level->isFrozen())
	{
		return;
	}
	if (Level->maptime >= TimeToStop)
	{
		if (bStretchX)
		{
			TheDecal->ScaleX = GoalX;
		}
		if (bStretchY)
		{
			TheDecal->ScaleY = GoalY;
		}
		Destroy ();
		return;
	}
	if (!bStarted)
	{
		bStarted = true;
		StartX = TheDecal->ScaleX;
		StartY = TheDecal->ScaleY;
	}

	int distance = Level->maptime - TimeToStart;
	int maxDistance = TimeToStop - TimeToStart;
	if (bStretchX)
	{
		TheDecal->ScaleX = StartX + (GoalX - StartX) * distance / maxDistance;
	}
	if (bStretchY)
	{
		TheDecal->ScaleY = StartY + (GoalY - StartY) * distance / maxDistance;
	}
}

IMPLEMENT_CLASS(DDecalSlider, false, false)

void DDecalSlider::Serialize(FSerializer &arc)
{
	Super::Serialize (arc);
	arc("starttime", TimeToStart)
		("endtime", TimeToStop)
		("disty", DistY)
		("starty", StartY)
		("started", bStarted);
}

void DDecalSlider::Tick ()
{
	if (TheDecal == nullptr)
	{
		Destroy ();
		return;
	}
	if (Level->maptime < TimeToStart || Level->isFrozen())
	{
		return;
	}
	if (!bStarted)
	{
		bStarted = true;
		/*StartX = TheDecal->LeftDistance;*/
		StartY = TheDecal->Z;
	}
	if (Level->maptime >= TimeToStop)
	{
		/*TheDecal->LeftDistance = StartX + DistX;*/
		TheDecal->Z = StartY + DistY;
		Destroy ();
		return;
	}

	int distance = Level->maptime - TimeToStart;
	int maxDistance = TimeToStop - TimeToStart;
	/*TheDecal->LeftDistance = StartX + DistX * distance / maxDistance);*/
	TheDecal->Z = StartY + DistY * distance / maxDistance;
}

IMPLEMENT_CLASS(DDecalColorer, false, false)

void DDecalColorer::Serialize(FSerializer &arc)
{
	Super::Serialize (arc);
	arc("starttime", TimeToStartDecay)
		("endtime", TimeToEndDecay)
		("startcolor", StartColor)
		("goalcolor", GoalColor);
}

void DDecalColorer::Tick ()
{
	if (TheDecal == nullptr || !(TheDecal->RenderStyle.Flags & STYLEF_ColorIsFixed))
	{
		Destroy ();
	}
	else
	{
		if (Level->maptime < TimeToStartDecay || Level->isFrozen())
		{
			return;
		}
		else if (Level->maptime >= TimeToEndDecay)
		{
			TheDecal->SetShade (GoalColor);
			Destroy ();					// remove myself
		}
		if (StartColor.a == 255)
		{
			StartColor = TheDecal->AlphaColor & 0xffffff;
			if (StartColor == GoalColor)
			{
				Destroy ();
				return;
			}
		}
		if (Level->maptime & 0)
		{ // Changing the shade can be expensive, so don't do it too often.
			return;
		}

		int distance = Level->maptime - TimeToStartDecay;
		int maxDistance = TimeToEndDecay - TimeToStartDecay;
		int r = StartColor.r + Scale (GoalColor.r - StartColor.r, distance, maxDistance);
		int g = StartColor.g + Scale (GoalColor.g - StartColor.g, distance, maxDistance);
		int b = StartColor.b + Scale (GoalColor.b - StartColor.b, distance, maxDistance);
		TheDecal->SetShade (r, g, b);
	}
}

