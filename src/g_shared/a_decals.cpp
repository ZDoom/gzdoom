/*
** a_decals.cpp
** Implements the actor that represents decals in the level
**
**---------------------------------------------------------------------------
** Copyright 1998-2001 Randy Heit
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
#include "a_sharedglobal.h"
#include "r_defs.h"
#include "p_local.h"
#include "v_video.h"
#include "p_trace.h"
#include "decallib.h"
#include "statnums.h"
#include "c_dispatch.h"

// Decals overload snext and sprev to keep a list of decals attached to a wall.
// They also overload floorclip to be the fractional distance from the
// left edge of the side. This distance is stored as a 2.30 fixed pt number.

IMPLEMENT_STATELESS_ACTOR (ADecal, Any, 9200, 0)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOSECTOR|MF_NOGRAVITY)
END_DEFAULTS

void ADecal::Destroy ()
{
	Remove ();
	Super::Destroy ();
}

void ADecal::Remove ()
{
	AActor **prev = sprev;
	AActor *next = snext;
	if (prev && (*prev = next))
		next->sprev = prev;
	sprev = NULL;
	snext = NULL;
}

void ADecal::SerializeChain (FArchive &arc, ADecal **first)
{
	DWORD numInChain;
	AActor *fresh;
	AActor **firstptr = (AActor **)first;

	if (arc.IsLoading ())
	{
		numInChain = arc.ReadCount ();
		
		while (numInChain--)
		{
			arc << fresh;
			*firstptr = fresh;
			fresh->sprev = firstptr;
			firstptr = &fresh->snext;
		}
	}
	else
	{
		numInChain = 0;
		fresh = *firstptr;
		while (fresh != NULL)
		{
			fresh = fresh->snext;
			++numInChain;
		}
		arc.WriteCount (numInChain);
		fresh = *firstptr;
		while (numInChain--)
		{
			arc << fresh;
			fresh = fresh->snext;
		}
	}
}

void ADecal::MoveChain (ADecal *first, fixed_t x, fixed_t y)
{
	while (first != NULL)
	{
		first->x += x;
		first->y += y;
		first = (ADecal *)first->snext;
	}
}

void ADecal::FixForSide (side_t *wall)
{
	AActor *decal = wall->BoundActors;
	line_t *line = &lines[wall->linenum];
	int wallnum = wall - sides;
	vertex_t *v1, *v2;

	if (line->sidenum[0] == wallnum)
	{
		v1 = line->v1;
		v2 = line->v2;
	}
	else
	{
		v1 = line->v2;
		v2 = line->v1;
	}

	fixed_t dx = v2->x - v1->x;
	fixed_t dy = v2->y - v1->y;

	while (decal != NULL)
	{
		decal->x = v1->x + MulScale2 (decal->floorclip, dx);
		decal->y = v1->y + MulScale2 (decal->floorclip, dy);
		decal = decal->snext;
	}
}

void ADecal::BeginPlay ()
{
	Super::BeginPlay ();

	// Decals do not think.
	ChangeStatNum (STAT_DECAL);

	// Find a wall to attach to, and set renderflags to keep
	// the decal at its current height. If the decal cannot find a wall
	// within 64 units, it destroys itself.
	//
	// Subclasses can set special1 if they don't want this sticky logic.

	if (special1 == 0)
	{
		DoTrace ();
	}

	if (args[0] != 0)
	{
		const FDecal *decal = DecalLibrary.GetDecalByNum (args[0]);
		if (decal != NULL)
		{
			decal->ApplyToActor (this);
		}
	}
}

void ADecal::DoTrace ()
{
	FTraceResults trace;

	Trace (x, y, z, Sector,
		finecosine[(angle+ANGLE_180)>>ANGLETOFINESHIFT],
		finesine[(angle+ANGLE_180)>>ANGLETOFINESHIFT], 0,
		64*FRACUNIT, 0, 0, NULL, trace, TRACE_NoSky);

	if (trace.HitType == TRACE_HitWall)
	{
		x = trace.X;
		y = trace.Y;
		StickToWall (sides + trace.Line->sidenum[trace.Side]);
	}
	else
	{
		Destroy ();
	}
}

// Returns the texture the decal stuck to.
int ADecal::StickToWall (side_t *wall)
{
	// Stick the decal at the end of the chain so it appears on top
	AActor *next, **prev;

	prev = (AActor **)&wall->BoundActors;
	while (*prev != NULL)
	{
		next = *prev;
		prev = &next->snext;
	}

	*prev = this;
	snext = NULL;
	sprev = prev;
/*
	snext = wall->BoundActors;
	sprev = &wall->BoundActors;
	if (snext)
		snext->sprev = &snext;
	wall->BoundActors = this;
*/
	sector_t *front, *back;
	line_t *line;
	int tex;

	line = &lines[wall->linenum];
	if (line->sidenum[0] == wall - sides)
	{
		front = line->frontsector;
		back = line->backsector;
	}
	else
	{
		front = line->backsector;
		back = line->frontsector;
	}
	if (back == NULL)
	{
		renderflags |= RF_RELMID;
		if (line->flags & ML_DONTPEGBOTTOM)
			z -= front->floortexz;
		else
			z -= front->ceilingtexz;
		tex = wall->midtexture;
	}
	else if (back->floorplane.ZatPoint (x, y) >= z)
	{
		renderflags |= RF_RELLOWER|RF_CLIPLOWER;
		if (line->flags & ML_DONTPEGBOTTOM)
			z -= front->ceilingtexz;
		else
			z -= back->floortexz;
		tex = wall->bottomtexture;
	}
	else
	{
		renderflags |= RF_RELUPPER|RF_CLIPUPPER;
		if (line->flags & ML_DONTPEGTOP)
			z -= front->ceilingtexz;
		else
			z -= back->ceilingtexz;
		tex = wall->toptexture;
	}

	CalcFracPos (wall);

	// Face the decal away from the wall
	angle = R_PointToAngle2 (0, 0, line->dx, line->dy);
	if (line->frontsector == front)
	{
		angle -= ANGLE_90;
	}
	else
	{
		angle += ANGLE_90;
	}

	return tex;
}

fixed_t ADecal::GetRealZ (const side_t *wall) const
{
	const line_t *line = &lines[wall->linenum];
	const sector_t *front, *back;

	if (line->sidenum[0] == wall - sides)
	{
		front = line->frontsector;
		back = line->backsector;
	}
	else
	{
		front = line->backsector;
		back = line->frontsector;
	}
	if (back == NULL)
	{
		back = front;
	}

	switch (renderflags & RF_RELMASK)
	{
	default:
		return z;
	case RF_RELUPPER:
		if (curline->linedef->flags & ML_DONTPEGTOP)
		{
			return z + front->ceilingtexz;
		}
		else
		{
			return z + back->ceilingtexz;
		}
	case RF_RELLOWER:
		if (curline->linedef->flags & ML_DONTPEGBOTTOM)
		{
			return z + front->ceilingtexz;
		}
		else
		{
			return z + back->floortexz;
		}
	case RF_RELMID:
		if (curline->linedef->flags & ML_DONTPEGBOTTOM)
		{
			return z + front->floortexz;
		}
		else
		{
			return z + front->ceilingtexz;
		}
	}
}

void ADecal::Relocate (fixed_t ix, fixed_t iy, fixed_t iz)
{
	Remove ();
	x = ix;
	y = iy;
	z = iz;
	DoTrace ();
}

void ADecal::CalcFracPos (side_t *wall)
{
	line_t *line = &lines[wall->linenum];
	int wallnum = wall - sides;
	vertex_t *v1, *v2;

	if (line->sidenum[0] == wallnum)
	{
		v1 = line->v1;
		v2 = line->v2;
	}
	else
	{
		v1 = line->v2;
		v2 = line->v1;
	}

	fixed_t dx = v2->x - v1->x;
	fixed_t dy = v2->y - v1->y;

	if (abs(dx) > abs(dy))
	{
		floorclip = SafeDivScale2 (x - v1->x, dx);
	}
	else if (dy != 0)
	{
		floorclip = SafeDivScale2 (y - v1->y, dy);
	}
	else
	{
		floorclip = 0;
	}
}

static int ImpactCount;
static AActor *FirstImpact;	// (but not the Second or Third Impact :-)
static AActor *LastImpact;

CVAR (Bool, cl_spreaddecals, true, CVAR_ARCHIVE)

CUSTOM_CVAR (Int, cl_maxdecals, 1024, CVAR_ARCHIVE)
{
	if (self < 0)
	{
		self = 0;
	}
	else
	{
		while (ImpactCount > self)
		{
			FirstImpact->Destroy ();
		}
	}
}

// Uses: target points to previous impact decal
//		 tracer points to next impact decal
//
// Note that this means we can't simply serialize an impact decal as-is
// because doing so when many are present in a level could result in
// a lot of recursion and we would run out of stack. Not nice. So instead,
// the save game code calls AImpactDecal::SerializeAll to serialize a
// list of impact decals.

IMPLEMENT_STATELESS_ACTOR (AImpactDecal, Any, -1, 0)
END_DEFAULTS

void AImpactDecal::SerializeTime (FArchive &arc)
{
	if (arc.IsLoading ())
	{
		ImpactCount = 0;
		FirstImpact = LastImpact = NULL;
	}
}

void AImpactDecal::Serialize (FArchive &arc)
{
	if (arc.IsStoring ())
	{
		// NULL the next pointer so that the serializer will not follow it
		// and possibly run out of stack space. NULLing target isn't
		// required; it just makes the archive smaller.
		AActor *saved1 = tracer, *saved2 = target;
		tracer = NULL;
		target = NULL;
		Super::Serialize (arc);
		tracer = saved1;
		target = saved2;
	}
	else
	{
		Super::Serialize (arc);

		ImpactCount++;
		target = LastImpact;
		if (target != NULL)
		{
			target->tracer = this;
		}
		else
		{
			FirstImpact = this;
		}
		LastImpact = this;
	}
}

void AImpactDecal::BeginPlay ()
{
	special1 = 1;	// Don't want ADecal to find a wall to stick to
	Super::BeginPlay ();

	target = LastImpact;
	if (target != NULL)
	{
		target->tracer = this;
	}
	else
	{
		FirstImpact = this;
	}
	LastImpact = this;
}

AImpactDecal *AImpactDecal::StaticCreate (const char *name, fixed_t x, fixed_t y, fixed_t z, side_t *wall)
{
	if (cl_maxdecals > 0)
	{
		const FDecal *decal = DecalLibrary.GetDecalByName (name);

		if (decal != NULL)
		{
			return StaticCreate (decal, x, y, z, wall);
		}
	}
	return NULL;
}

static void GetWallStuff (side_t *wall, vertex_t *&v1, fixed_t &ldx, fixed_t &ldy)
{
	line_t *line = &lines[wall->linenum];
	if (line->sidenum[0] == wall - sides)
	{
		v1 = line->v1;
		ldx = line->dx;
		ldy = line->dy;
	}
	else
	{
		v1 = line->v2;
		ldx = -line->dx;
		ldy = -line->dy;
	}
}

static fixed_t Length (fixed_t dx, fixed_t dy)
{
	return (fixed_t)sqrtf ((float)dx*(float)dx+(float)dy*(float)dy);
}

static side_t *NextWall (const side_t *wall)
{
	line_t *line = &lines[wall->linenum];
	int wallnum = wall - sides;

	if (line->sidenum[0] == wallnum)
	{
		if (line->sidenum[1] != NO_INDEX)
		{
			return sides + line->sidenum[1];
		}
	}
	else if (line->sidenum[1] == wallnum)
	{
		return sides + line->sidenum[0];
	}
	return NULL;
}

static fixed_t DecalWidth, DecalLeft, DecalRight;
static fixed_t SpreadZ;
static const AImpactDecal *SpreadSource;
static const FDecal *SpreadDecal;
static TArray<side_t *> SpreadStack;

void AImpactDecal::SpreadLeft (fixed_t r, vertex_t *v1, side_t *feelwall)
{
	fixed_t ldx, ldy;

	SpreadStack.Push (feelwall);

	while (r < 0 && feelwall->LeftSide != NO_INDEX)
	{
		fixed_t startr = r;

		fixed_t x = v1->x;
		fixed_t y = v1->y;

		feelwall = &sides[feelwall->LeftSide];
		GetWallStuff (feelwall, v1, ldx, ldy);
		fixed_t wallsize = Length (ldx, ldy);
		r += DecalLeft;
		x += Scale (r, ldx, wallsize);
		y += Scale (r, ldy, wallsize);
		r = wallsize + startr;
		SpreadSource->CloneSelf (SpreadDecal, x, y, SpreadZ, feelwall);
		SpreadStack.Push (feelwall);

		side_t *nextwall = NextWall (feelwall);
		if (nextwall != NULL && nextwall->LeftSide != NO_INDEX)
		{
			size_t i;

			for (i = SpreadStack.Size(); i-- > 0; )
			{
				if (SpreadStack[i] == nextwall)
					break;
			}
			if (i == (size_t)~0)
			{
				vertex_t *v2;

				GetWallStuff (nextwall, v2, ldx, ldy);
				SpreadLeft (startr, v2, nextwall);
			}
		}
	}
}

void AImpactDecal::SpreadRight (fixed_t r, side_t *feelwall, fixed_t wallsize)
{
	vertex_t *v1;
	fixed_t x, y, ldx, ldy;

	SpreadStack.Push (feelwall);

	while (r > wallsize && feelwall->RightSide != NO_INDEX)
	{
		feelwall = &sides[feelwall->RightSide];

		side_t *nextwall = NextWall (feelwall);
		if (nextwall != NULL && nextwall->LeftSide != NO_INDEX)
		{
			size_t i;

			for (i = SpreadStack.Size(); i-- > 0; )
			{
				if (SpreadStack[i] == nextwall)
					break;
			}
			if (i == (size_t)~0)
			{
				SpreadRight (r, nextwall, wallsize);
			}
		}

		r = DecalWidth - r + wallsize - DecalLeft;
		GetWallStuff (feelwall, v1, ldx, ldy);
		x = v1->x;
		y = v1->y;
		wallsize = Length (ldx, ldy);
		x -= Scale (r, ldx, wallsize);
		y -= Scale (r, ldy, wallsize);
		r = DecalRight - r;
		SpreadSource->CloneSelf (SpreadDecal, x, y, SpreadZ, feelwall);
		SpreadStack.Push (feelwall);
	}
}

AImpactDecal *AImpactDecal::StaticCreate (const FDecal *decal, fixed_t x, fixed_t y, fixed_t z, side_t *wall)
{
	AImpactDecal *actor = NULL;
	if (decal != NULL && cl_maxdecals > 0 &&
		!(wall->Flags & WALLF_NOAUTODECALS))
	{
		if (decal->LowerDecal)
		{
			StaticCreate (decal->LowerDecal->GetDecal(), x, y, z, wall);
		}
		if (ImpactCount >= cl_maxdecals)
		{
			FirstImpact->Destroy ();
		}
		ImpactCount++;
		actor = Spawn<AImpactDecal> (x, y, z);

		if (actor == NULL)
			return NULL;

		int stickypic = actor->StickToWall (wall);
		FTexture *tex = TexMan(stickypic);

		if (tex != NULL && tex->bNoDecals)
		{
			actor->Destroy ();
			return NULL;
		}

		decal->ApplyToActor (actor);

		if (!cl_spreaddecals || actor->picnum == 0xffff)
			return actor;

		// Spread decal to nearby walls if it does not all fit on this one
		vertex_t *v1;
		fixed_t rorg, ldx, ldy;
		int xscale, yscale;

		GetWallStuff (wall, v1, ldx, ldy);
		rorg = Length (x - v1->x, y - v1->y);

		tex = TexMan[actor->picnum];
		int dwidth = tex->GetWidth ();

		xscale = (actor->xscale + 1) << (FRACBITS - 6);
		yscale = (actor->yscale + 1) << (FRACBITS - 6);

		DecalWidth = dwidth * xscale;
		DecalLeft = tex->LeftOffset * xscale;
		DecalRight = DecalWidth - DecalLeft;
		SpreadSource = actor;
		SpreadDecal = decal;
		SpreadZ = z;


		// Try spreading left first
		SpreadLeft (rorg - DecalLeft, v1, wall);
		SpreadStack.Clear ();

		// Then try spreading right
		SpreadRight (rorg + DecalRight, wall,
			Length (lines[wall->linenum].dx, lines[wall->linenum].dy));
		SpreadStack.Clear ();
	}
	return actor;
}

AImpactDecal *AImpactDecal::CloneSelf (const FDecal *decal, fixed_t ix, fixed_t iy, fixed_t iz, side_t *wall) const
{
	AImpactDecal *actor = NULL;
	// [SO] We used to have an 'else' here -- but that means we create an actor without increasing the
	//		count!!!
	// [RH] Moved this before the destroy, just so it can't go negative temporarily.
	ImpactCount++;
	if (ImpactCount >= cl_maxdecals)
	{
		FirstImpact->Destroy ();
	}
	actor = Spawn<AImpactDecal> (ix, iy, iz);
	if (actor != NULL)
	{
		actor->StickToWall (wall);
		decal->ApplyToActor (actor);
		actor->renderflags = (actor->renderflags & RF_DECALMASK) |
							 (this->renderflags & ~RF_DECALMASK);
	}
	return actor;
}

void AImpactDecal::Destroy ()
{
	if (target != NULL)
	{
		target->tracer = tracer;
	}
	if (tracer != NULL)
	{
		tracer->target = target;
	}
	if (LastImpact == this)
	{
		LastImpact = target;
	}
	if (FirstImpact == this)
	{
		FirstImpact = tracer;
	}

	ImpactCount--;
	Super::Destroy ();
}

CCMD (countdecals)
{
	Printf ("%d impact decals\n", ImpactCount);
}

CCMD (countdecalsreal)
{
	TThinkerIterator<AImpactDecal> iterator (STAT_DECAL);
	int count = 0;

	while (iterator.Next())
		count++;

	Printf ("Counted %d impact decals\n", count);
}

CCMD (spray)
{
	if (argv.argc() < 2)
	{
		Printf ("Usage: spray <decal>\n");
		return;
	}

	FTraceResults trace;

	angle_t ang = m_Instigator->angle  >> ANGLETOFINESHIFT;
	angle_t pitch = (angle_t)(m_Instigator->pitch) >> ANGLETOFINESHIFT;
	fixed_t vx = FixedMul (finecosine[pitch], finecosine[ang]);
	fixed_t vy = FixedMul (finecosine[pitch], finesine[ang]);
	fixed_t vz = -finesine[pitch];

	if (Trace (m_Instigator->x, m_Instigator->y,
		m_Instigator->z + m_Instigator->height - (m_Instigator->height>>2),
		m_Instigator->Sector,
		vx, vy, vz, 172*FRACUNIT, 0, ML_BLOCKEVERYTHING, m_Instigator,
		trace, TRACE_NoSky))
	{
		if (trace.HitType == TRACE_HitWall)
		{
			AImpactDecal::StaticCreate (argv[1],
				trace.X, trace.Y, trace.Z,
				sides + trace.Line->sidenum[trace.Side]);
		}
	}
}
