#include "actor.h"
#include "a_sharedglobal.h"
#include "r_defs.h"
#include "p_local.h"
#include "v_video.h"
#include "p_trace.h"
#include "decallib.h"
#include "statnums.h"

IMPLEMENT_STATELESS_ACTOR (ADecal, Any, 9200, 0)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOSECTOR|MF_NOGRAVITY)
END_DEFAULTS

void ADecal::Destroy ()
{
	AActor **prev = sprev;
	AActor *next = snext;
	if (prev && (*prev = next))
		next->sprev = prev;
	sprev = NULL;
	snext = NULL;
	Super::Destroy ();
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
		FTraceResults trace;

		Trace (x, y, z, subsector->sector,
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

	if (args[0] != 0)
	{
		const FDecal *decal = DecalLibrary.GetDecalByNum (args[0]);
		if (decal != NULL)
		{
			decal->ApplyToActor (this);
		}
	}
}

void ADecal::StickToWall (side_t *wall)
{
	snext = wall->BoundActors;
	sprev = &wall->BoundActors;
	if (snext)
		snext->sprev = &snext;
	wall->BoundActors = this;

	sector_t *front, *back;
	line_t *line;

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
	}
	else if (back->floorplane.ZatPoint (x, y) >= z)
	{
		renderflags |= RF_RELLOWER|RF_CLIPLOWER;
		if (line->flags & ML_DONTPEGBOTTOM)
			z -= front->ceilingtexz;
		else
			z -= back->floortexz;
	}
	else
	{
		renderflags |= RF_RELUPPER|RF_CLIPUPPER;
		if (line->flags & ML_DONTPEGTOP)
			z -= front->ceilingtexz;
		else
			z -= back->ceilingtexz;
	}
}

static int ImpactCount;
static AActor *FirstImpact;	// (but not the Second or Third Impact :-)
static AActor *LastImpact;

CUSTOM_CVAR (Int, cl_maxdecals, 1024, CVAR_ARCHIVE)
{
	if (*var < 0)
	{
		var = 0;
	}
	else
	{
		while (ImpactCount > *var)
		{
			FirstImpact->Destroy ();
		}
	}
}

// Uses: target points to previous impact decal
//		 tracer points to next impact decal

IMPLEMENT_STATELESS_ACTOR (AImpactDecal, Any, -1, 0)
END_DEFAULTS

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
	AImpactDecal *actor = NULL;
	if (*cl_maxdecals > 0)
	{
		const FDecal *decal = DecalLibrary.GetDecalByName (name);

		if (decal != NULL)
		{
			if (ImpactCount >= *cl_maxdecals)
			{
				FirstImpact->Destroy ();
			}
			ImpactCount++;
			actor = Spawn<AImpactDecal> (x, y, z);
			actor->StickToWall (wall);
			decal->ApplyToActor (actor);
		}
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