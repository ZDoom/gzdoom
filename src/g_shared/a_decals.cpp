#include "actor.h"
#include "a_sharedglobal.h"
#include "r_defs.h"
#include "p_local.h"
#include "v_video.h"
#include "p_trace.h"
#include "decallib.h"
#include "statnums.h"
#include "z_zone.h"
#include "c_dispatch.h"

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

void ADecal::SerializeChain (FArchive &arc, AActor **firstptr)
{
	DWORD numInChain;
	AActor *fresh;

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
	AActor *next, **prev;

	prev = &wall->BoundActors;
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
		if (line->sidenum[1] != -1)
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
static side_t *SpreadWall;

void AImpactDecal::SpreadLeft (fixed_t r, vertex_t *v1, side_t *feelwall)
{
	fixed_t ldx, ldy;

	while (r < 0 && feelwall->LeftSide != -1)
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
/*
		side_t *nextwall = NextWall (feelwall);
		if (nextwall != NULL && nextwall->LeftSide != -1 && nextwall != SpreadWall)
		{
			vertex_t *v2;

			//nextwall = &sides[nextwall->LeftSide];
			GetWallStuff (nextwall, v2, ldx, ldy);
			SpreadLeft (startr, v2, nextwall);
		}
*/
	}
}

void AImpactDecal::SpreadRight (fixed_t r, side_t *feelwall, fixed_t wallsize)
{
	vertex_t *v1;
	fixed_t x, y, ldx, ldy;

	while (r > wallsize && feelwall->RightSide != -1)
	{
		r = DecalWidth - r + wallsize - DecalLeft;
		feelwall = &sides[feelwall->RightSide];
		GetWallStuff (feelwall, v1, ldx, ldy);
		x = v1->x;
		y = v1->y;
		wallsize = Length (ldx, ldy);
		x -= Scale (r, ldx, wallsize);
		y -= Scale (r, ldy, wallsize);
		r = DecalRight - r;
		SpreadSource->CloneSelf (SpreadDecal, x, y, SpreadZ, feelwall);
	}
}

AImpactDecal *AImpactDecal::StaticCreate (const FDecal *decal, fixed_t x, fixed_t y, fixed_t z, side_t *wall)
{
	AImpactDecal *actor = NULL;
	if (decal != NULL && cl_maxdecals > 0 && !(wall->Flags & WALLF_NOAUTODECALS))
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
			return actor;

		actor->StickToWall (wall);
		decal->ApplyToActor (actor);

		if (!cl_spreaddecals || actor->picnum == 0xffff)
			return actor;

		// Spread decal to nearby walls if it does not all fit on this one
		vertex_t *v1;
		fixed_t rorg, ldx, ldy;
		int xscale, yscale;

		GetWallStuff (wall, v1, ldx, ldy);
		rorg = Length (x - v1->x, y - v1->y);

		if (TileSizes[actor->picnum].Width == 0xffff)
		{
			R_CacheTileNum (actor->picnum, PU_CACHE);
		}

		xscale = (actor->xscale + 1) << (FRACBITS - 6);
		yscale = (actor->yscale + 1) << (FRACBITS - 6);

		DecalWidth = TileSizes[actor->picnum].Width * xscale;
		DecalLeft = TileSizes[actor->picnum].LeftOffset * xscale;
		DecalRight = DecalWidth - DecalLeft;
		SpreadSource = actor;
		SpreadDecal = decal;
		SpreadZ = z;
		SpreadWall = wall;

		// Try spreading left first
		SpreadLeft (rorg - DecalLeft, v1, wall);

		// Then try spreading right
		SpreadRight (rorg + DecalRight, wall,
			Length (lines[wall->linenum].dx, lines[wall->linenum].dy));
	}
	return actor;
}

AImpactDecal *AImpactDecal::CloneSelf (const FDecal *decal, fixed_t ix, fixed_t iy, fixed_t iz, side_t *wall) const
{
	AImpactDecal *actor = NULL;
	if (ImpactCount >= cl_maxdecals)
	{
		FirstImpact->Destroy ();
	}
	else
	{
		ImpactCount++;
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
		m_Instigator->subsector->sector,
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
