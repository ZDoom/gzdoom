/*
** a_decals.cpp
** Implements the actor that represents decals in the level
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
#include "a_sharedglobal.h"
#include "r_defs.h"
#include "p_local.h"
#include "v_video.h"
#include "p_trace.h"
#include "decallib.h"
#include "statnums.h"
#include "c_dispatch.h"

static fixed_t DecalWidth, DecalLeft, DecalRight;
static fixed_t SpreadZ;
static const DBaseDecal *SpreadSource;
static const FDecalTemplate *SpreadTemplate;
static TArray<side_t *> SpreadStack;

static int ImpactCount;
static DImpactDecal *FirstImpact;	// (but not the Second or Third Impact :-)
static DImpactDecal *LastImpact;

CVAR (Bool, cl_spreaddecals, true, CVAR_ARCHIVE)

// They also overload floorclip to be the fractional distance from the
// left edge of the side. This distance is stored as a 2.30 fixed pt number.

IMPLEMENT_CLASS (DBaseDecal)
IMPLEMENT_CLASS (DImpactDecal)

DBaseDecal::DBaseDecal ()
: DThinker(STAT_DECAL),
  WallNext(0), WallPrev(0), x(0), y(0), z(0), AlphaColor(0), Translation(0), PicNum(0xFFFF),
  RenderFlags(0), XScale(8), YScale(8), RenderStyle(0), LeftDistance(0), Alpha(0)
{
}

DBaseDecal::DBaseDecal (fixed_t x, fixed_t y, fixed_t z)
: DThinker(STAT_DECAL),
  WallNext(0), WallPrev(0), x(x), y(y), z(z), AlphaColor(0), Translation(0), PicNum(0xFFFF),
  RenderFlags(0), XScale(8), YScale(8), RenderStyle(0), LeftDistance(0), Alpha(0)
{
}

DBaseDecal::DBaseDecal (const AActor *basis)
: DThinker(STAT_DECAL),
  WallNext(0), WallPrev(0), x(basis->x), y(basis->y), z(basis->z), AlphaColor(basis->alphacolor),
  Translation(basis->Translation), PicNum(basis->picnum), RenderFlags(basis->renderflags),
  XScale(basis->xscale), YScale(basis->yscale), RenderStyle(basis->RenderStyle), LeftDistance(0),
  Alpha(basis->alpha)
{
}

DBaseDecal::DBaseDecal (const DBaseDecal *basis)
: DThinker(STAT_DECAL),
  WallNext(0), WallPrev(0), x(basis->x), y(basis->y), z(basis->z), AlphaColor(basis->AlphaColor),
  Translation(basis->Translation), PicNum(basis->PicNum), RenderFlags(basis->RenderFlags),
  XScale(basis->XScale), YScale(basis->YScale), RenderStyle(basis->RenderStyle), LeftDistance(0),
  Alpha(basis->Alpha)
{
}

void DBaseDecal::Destroy ()
{
	Remove ();
	Super::Destroy ();
}

void DBaseDecal::Remove ()
{
	DBaseDecal **prev = WallPrev;
	DBaseDecal *next = WallNext;
	if (prev && (*prev = next))
		next->WallPrev = prev;
	WallPrev = NULL;
	WallNext = NULL;
}

void DBaseDecal::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	arc << x << y << z
		<< AlphaColor
		<< Translation
		<< PicNum
		<< RenderFlags
		<< XScale << YScale
		<< RenderStyle
		<< LeftDistance
		<< Alpha;
}

void DBaseDecal::SerializeChain (FArchive &arc, DBaseDecal **first)
{
	DWORD numInChain;
	DBaseDecal *fresh;
	DBaseDecal **firstptr = first;

	if (arc.IsLoading ())
	{
		numInChain = arc.ReadCount ();
		
		while (numInChain--)
		{
			arc << fresh;
			*firstptr = fresh;
			fresh->WallPrev = firstptr;
			firstptr = &fresh->WallNext;
		}
	}
	else
	{
		numInChain = 0;
		fresh = *firstptr;
		while (fresh != NULL)
		{
			fresh = fresh->WallNext;
			++numInChain;
		}
		arc.WriteCount (numInChain);
		fresh = *firstptr;
		while (numInChain--)
		{
			arc << fresh;
			fresh = fresh->WallNext;
		}
	}
}

void DBaseDecal::MoveChain (DBaseDecal *first, fixed_t x, fixed_t y)
{
	while (first != NULL)
	{
		first->x += x;
		first->y += y;
		first = (DBaseDecal *)first->WallNext;
	}
}

void DBaseDecal::FixForSide (side_t *wall)
{
	DBaseDecal *decal = wall->AttachedDecals;
	line_t *line = &lines[wall->linenum];
	int wallnum = int(wall - sides);
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
		decal->x = v1->x + MulScale2 (decal->LeftDistance, dx);
		decal->y = v1->y + MulScale2 (decal->LeftDistance, dy);
		decal = decal->WallNext;
	}
}

void DBaseDecal::SetShade (DWORD rgb)
{
	PalEntry *entry = (PalEntry *)&rgb;
	AlphaColor = rgb | (ColorMatcher.Pick (entry->r, entry->g, entry->b) << 24);
}

void DBaseDecal::SetShade (int r, int g, int b)
{
	AlphaColor = MAKEARGB(ColorMatcher.Pick (r, g, b), r, g, b);
}

// Returns the texture the decal stuck to.
int DBaseDecal::StickToWall (side_t *wall)
{
	// Stick the decal at the end of the chain so it appears on top
	DBaseDecal *next, **prev;

	prev = &wall->AttachedDecals;
	while (*prev != NULL)
	{
		next = *prev;
		prev = &next->WallNext;
	}

	*prev = this;
	WallNext = NULL;
	WallPrev = prev;
/*
	WallNext = wall->AttachedDecals;
	WallPrev = &wall->AttachedDecals;
	if (WallNext)
		WallNext->WallPrev = &WallNext;
	wall->AttachedDecals = this;
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
		RenderFlags |= RF_RELMID;
		if (line->flags & ML_DONTPEGBOTTOM)
			z -= front->floortexz;
		else
			z -= front->ceilingtexz;
		tex = wall->midtexture;
	}
	else if (back->floorplane.ZatPoint (x, y) >= z)
	{
		RenderFlags |= RF_RELLOWER|RF_CLIPLOWER;
		if (line->flags & ML_DONTPEGBOTTOM)
			z -= front->ceilingtexz;
		else
			z -= back->floortexz;
		tex = wall->bottomtexture;
	}
	else
	{
		RenderFlags |= RF_RELUPPER|RF_CLIPUPPER;
		if (line->flags & ML_DONTPEGTOP)
			z -= front->ceilingtexz;
		else
			z -= back->ceilingtexz;
		tex = wall->toptexture;
	}

	CalcFracPos (wall);

	return tex;
}

fixed_t DBaseDecal::GetRealZ (const side_t *wall) const
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

	switch (RenderFlags & RF_RELMASK)
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

void DBaseDecal::CalcFracPos (side_t *wall)
{
	line_t *line = &lines[wall->linenum];
	int wallnum = int(wall - sides);
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
		LeftDistance = SafeDivScale2 (x - v1->x, dx);
	}
	else if (dy != 0)
	{
		LeftDistance = SafeDivScale2 (y - v1->y, dy);
	}
	else
	{
		LeftDistance = 0;
	}
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
	int wallnum = int(wall - sides);

	if (line->sidenum[0] == wallnum)
	{
		if (line->sidenum[1] != NO_SIDE)
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

void DBaseDecal::SpreadLeft (fixed_t r, vertex_t *v1, side_t *feelwall)
{
	fixed_t ldx, ldy;

	SpreadStack.Push (feelwall);

	while (r < 0 && feelwall->LeftSide != NO_SIDE)
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
		SpreadSource->CloneSelf (SpreadTemplate, x, y, SpreadZ, feelwall);
		SpreadStack.Push (feelwall);

		side_t *nextwall = NextWall (feelwall);
		if (nextwall != NULL && nextwall->LeftSide != NO_SIDE)
		{
			int i;

			for (i = SpreadStack.Size(); i-- > 0; )
			{
				if (SpreadStack[i] == nextwall)
					break;
			}
			if (i == -1)
			{
				vertex_t *v2;

				GetWallStuff (nextwall, v2, ldx, ldy);
				SpreadLeft (startr, v2, nextwall);
			}
		}
	}
}

void DBaseDecal::SpreadRight (fixed_t r, side_t *feelwall, fixed_t wallsize)
{
	vertex_t *v1;
	fixed_t x, y, ldx, ldy;

	SpreadStack.Push (feelwall);

	while (r > wallsize && feelwall->RightSide != NO_SIDE)
	{
		feelwall = &sides[feelwall->RightSide];

		side_t *nextwall = NextWall (feelwall);
		if (nextwall != NULL && nextwall->LeftSide != NO_SIDE)
		{
			int i;

			for (i = SpreadStack.Size(); i-- > 0; )
			{
				if (SpreadStack[i] == nextwall)
					break;
			}
			if (i == -1)
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
		SpreadSource->CloneSelf (SpreadTemplate, x, y, SpreadZ, feelwall);
		SpreadStack.Push (feelwall);
	}
}

void DBaseDecal::Spread (const FDecalTemplate *tpl, side_t *wall)
{
	FTexture *tex;
	vertex_t *v1;
	fixed_t rorg, ldx, ldy;
	int xscale, yscale;

	GetWallStuff (wall, v1, ldx, ldy);
	rorg = Length (x - v1->x, y - v1->y);

	tex = TexMan[PicNum];
	int dwidth = tex->GetWidth ();

	xscale = (XScale + 1) << (FRACBITS - 6);
	yscale = (YScale + 1) << (FRACBITS - 6);

	DecalWidth = dwidth * xscale;
	DecalLeft = tex->LeftOffset * xscale;
	DecalRight = DecalWidth - DecalLeft;
	SpreadSource = this;
	SpreadTemplate = tpl;
	SpreadZ = z;


	// Try spreading left first
	SpreadLeft (rorg - DecalLeft, v1, wall);
	SpreadStack.Clear ();

	// Then try spreading right
	SpreadRight (rorg + DecalRight, wall,
		Length (lines[wall->linenum].dx, lines[wall->linenum].dy));
	SpreadStack.Clear ();
}

DBaseDecal *DBaseDecal::CloneSelf (const FDecalTemplate *tpl, fixed_t ix, fixed_t iy, fixed_t iz, side_t *wall) const
{
	DBaseDecal *decal = new DBaseDecal(ix, iy, iz);
	if (decal != NULL)
	{
		decal->StickToWall (wall);
		tpl->ApplyToDecal (decal);
		decal->AlphaColor = AlphaColor;
		decal->RenderFlags = (decal->RenderFlags & RF_DECALMASK) |
							 (this->RenderFlags & ~RF_DECALMASK);
	}
	return decal;
}

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
// the save game code calls DImpactDecal::SerializeAll to serialize a
// list of impact decals.

void DImpactDecal::SerializeTime (FArchive &arc)
{
	if (arc.IsLoading ())
	{
		ImpactCount = 0;
		FirstImpact = LastImpact = NULL;
	}
}

void DImpactDecal::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
}

DImpactDecal::DImpactDecal ()
: ImpactNext(0), ImpactPrev(0)
{
	Link();
}

DImpactDecal::DImpactDecal (fixed_t x, fixed_t y, fixed_t z)
: DBaseDecal (x, y, z),
  ImpactNext(0), ImpactPrev(0)
{
	Link();
}

void DImpactDecal::Link ()
{
	ImpactCount++;
	ImpactPrev = LastImpact;
	if (ImpactPrev != NULL)
	{
		ImpactPrev->ImpactNext = this;
	}
	else
	{
		FirstImpact = this;
	}
	LastImpact = this;
}

DImpactDecal *DImpactDecal::StaticCreate (const char *name, fixed_t x, fixed_t y, fixed_t z, side_t *wall, PalEntry color)
{
	if (cl_maxdecals > 0)
	{
		const FDecalTemplate *tpl = DecalLibrary.GetDecalByName (name);

		if (tpl != NULL && (tpl = tpl->GetDecal()) != NULL)
		{
			return StaticCreate (tpl, x, y, z, wall, color);
		}
	}
	return NULL;
}

DImpactDecal *DImpactDecal::StaticCreate (const FDecalTemplate *tpl, fixed_t x, fixed_t y, fixed_t z, side_t *wall, PalEntry color)
{
	DImpactDecal *decal = NULL;
	if (tpl != NULL && cl_maxdecals > 0 && !(wall->Flags & WALLF_NOAUTODECALS))
	{
		if (tpl->LowerDecal)
		{
			StaticCreate (tpl->LowerDecal->GetDecal(), x, y, z, wall);
		}
		if (ImpactCount >= cl_maxdecals)
		{
			FirstImpact->Destroy ();
		}

		decal = new DImpactDecal (x, y, z);

		int stickypic = decal->StickToWall (wall);
		FTexture *tex = TexMan[stickypic];

		if (tex != NULL && tex->bNoDecals)
		{
			return NULL;
		}

		if (decal == NULL)
		{
			return NULL;
		}

		tpl->ApplyToDecal (decal);
		if (color != 0)
		{
			decal->SetShade (color.r, color.g, color.b);
		}

		if (!cl_spreaddecals || decal->PicNum == 0xffff)
		{
			return decal;
		}

		// Spread decal to nearby walls if it does not all fit on this one
		decal->Spread (tpl, wall);
	}
	return decal;
}

DBaseDecal *DImpactDecal::CloneSelf (const FDecalTemplate *tpl, fixed_t ix, fixed_t iy, fixed_t iz, side_t *wall) const
{
	if (ImpactCount >= cl_maxdecals)
	{
		FirstImpact->Destroy ();
	}
	DImpactDecal *decal = new DImpactDecal(ix, iy, iz);
	if (decal != NULL)
	{
		decal->StickToWall (wall);
		tpl->ApplyToDecal (decal);
		decal->AlphaColor = AlphaColor;
		decal->RenderFlags = (decal->RenderFlags & RF_DECALMASK) |
							 (this->RenderFlags & ~RF_DECALMASK);
	}
	return decal;
}

void DImpactDecal::Destroy ()
{
	if (ImpactPrev != NULL)
	{
		ImpactPrev->ImpactNext = ImpactNext;
	}
	if (ImpactNext != NULL)
	{
		ImpactNext->ImpactPrev = ImpactPrev;
	}
	if (LastImpact == this)
	{
		LastImpact = ImpactPrev;
	}
	if (FirstImpact == this)
	{
		FirstImpact = ImpactNext;
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
	TThinkerIterator<DImpactDecal> iterator (STAT_DECAL);
	int count = 0;

	while (iterator.Next())
		count++;

	Printf ("Counted %d impact decals\n", count);
}

CCMD (spray)
{
	if (argv.argc() < 2 || m_Instigator == NULL)
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
			DImpactDecal::StaticCreate (argv[1],
				trace.X, trace.Y, trace.Z,
				sides + trace.Line->sidenum[trace.Side]);
		}
	}
}

class ADecal : public AActor
{
	DECLARE_STATELESS_ACTOR (ADecal, AActor);
public:
	void BeginPlay ();
	bool DoTrace (DBaseDecal *decal, angle_t angle, sector_t *sec);
};

IMPLEMENT_STATELESS_ACTOR (ADecal, Any, 9200, 0)
END_DEFAULTS

void ADecal::BeginPlay ()
{
	Super::BeginPlay ();

	// Find a wall to attach to, and set RenderFlags to keep
	// the decal at its current height. If the decal cannot find a wall
	// within 64 units, it destroys itself.
	//
	// Subclasses can set special1 if they don't want this sticky logic.

	DBaseDecal *decal = new DBaseDecal (this);

	if (!DoTrace (decal, angle, Sector))
	{
		decal->Destroy();
		return;
	}

	if (args[0] != 0)
	{
		const FDecalTemplate *tpl = DecalLibrary.GetDecalByNum (args[0]);
		if (tpl != NULL)
		{
			tpl->ApplyToDecal (decal);
		}
	}
}

bool ADecal::DoTrace (DBaseDecal *decal, angle_t angle, sector_t *sec)
{
	FTraceResults trace;

	Trace (x, y, z, sec,
		finecosine[(angle+ANGLE_180)>>ANGLETOFINESHIFT],
		finesine[(angle+ANGLE_180)>>ANGLETOFINESHIFT], 0,
		64*FRACUNIT, 0, 0, NULL, trace, TRACE_NoSky);

	if (trace.HitType == TRACE_HitWall)
	{
		decal->x = trace.X;
		decal->y = trace.Y;
		decal->StickToWall (sides + trace.Line->sidenum[trace.Side]);
		return true;
	}
	return false;
}
