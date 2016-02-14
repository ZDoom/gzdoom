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
#include "d_net.h"
#include "colormatcher.h"
#include "v_palette.h"
#include "farchive.h"

static fixed_t DecalWidth, DecalLeft, DecalRight;
static fixed_t SpreadZ;
static const DBaseDecal *SpreadSource;
static const FDecalTemplate *SpreadTemplate;
static TArray<side_t *> SpreadStack;

static int ImpactCount;

CVAR (Bool, cl_spreaddecals, true, CVAR_ARCHIVE)

IMPLEMENT_POINTY_CLASS (DBaseDecal)
 DECLARE_POINTER(WallNext)
END_POINTERS

IMPLEMENT_CLASS (DImpactDecal)

DBaseDecal::DBaseDecal ()
: DThinker(STAT_DECAL),
  WallNext(0), WallPrev(0), LeftDistance(0), Z(0), ScaleX(FRACUNIT), ScaleY(FRACUNIT), Alpha(FRACUNIT),
  AlphaColor(0), Translation(0), RenderFlags(0)
{
	RenderStyle = STYLE_None;
	PicNum.SetInvalid();
}

DBaseDecal::DBaseDecal (fixed_t z)
: DThinker(STAT_DECAL),
  WallNext(0), WallPrev(0), LeftDistance(0), Z(z), ScaleX(FRACUNIT), ScaleY(FRACUNIT), Alpha(FRACUNIT),
  AlphaColor(0), Translation(0), RenderFlags(0)
{
	RenderStyle = STYLE_None;
	PicNum.SetInvalid();
}

DBaseDecal::DBaseDecal (int statnum, fixed_t z)
: DThinker(statnum),
  WallNext(0), WallPrev(0), LeftDistance(0), Z(z), ScaleX(FRACUNIT), ScaleY(FRACUNIT), Alpha(FRACUNIT),
  AlphaColor(0), Translation(0), RenderFlags(0)
{
	RenderStyle = STYLE_None;
	PicNum.SetInvalid();
}

DBaseDecal::DBaseDecal (const AActor *basis)
: DThinker(STAT_DECAL),
  WallNext(0), WallPrev(0), LeftDistance(0), Z(basis->Z()), ScaleX(basis->scaleX), ScaleY(basis->scaleY),
  Alpha(basis->alpha), AlphaColor(basis->fillcolor), Translation(basis->Translation), PicNum(basis->picnum),
  RenderFlags(basis->renderflags), RenderStyle(basis->RenderStyle)
{
}

DBaseDecal::DBaseDecal (const DBaseDecal *basis)
: DThinker(STAT_DECAL),
  WallNext(0), WallPrev(0), LeftDistance(basis->LeftDistance), Z(basis->Z), ScaleX(basis->ScaleX),
  ScaleY(basis->ScaleY), Alpha(basis->Alpha), AlphaColor(basis->AlphaColor), Translation(basis->Translation),
  PicNum(basis->PicNum), RenderFlags(basis->RenderFlags), RenderStyle(basis->RenderStyle)
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
	arc << LeftDistance
		<< Z
		<< ScaleX << ScaleY
		<< Alpha
		<< AlphaColor
		<< Translation
		<< PicNum
		<< RenderFlags
		<< RenderStyle
		<< Sector;
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

void DBaseDecal::GetXY (side_t *wall, fixed_t &ox, fixed_t &oy) const
{
	line_t *line = wall->linedef;
	vertex_t *v1, *v2;

	if (line->sidedef[0] == wall)
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

	ox = v1->x + MulScale30 (LeftDistance, dx);
	oy = v1->y + MulScale30 (LeftDistance, dy);
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
FTextureID DBaseDecal::StickToWall (side_t *wall, fixed_t x, fixed_t y, F3DFloor *ffloor)
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
	FTextureID tex;

	line = wall->linedef;
	if (line->sidedef[0] == wall)
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
			Z -= front->GetPlaneTexZ(sector_t::floor);
		else
			Z -= front->GetPlaneTexZ(sector_t::ceiling);
		tex = wall->GetTexture(side_t::mid);
	}
	else if (back->floorplane.ZatPoint (x, y) >= Z)
	{
		RenderFlags |= RF_RELLOWER|RF_CLIPLOWER;
		if (line->flags & ML_DONTPEGBOTTOM)
			Z -= front->GetPlaneTexZ(sector_t::ceiling);
		else
			Z -= back->GetPlaneTexZ(sector_t::floor);
		tex = wall->GetTexture(side_t::bottom);
	}
	else if (back->ceilingplane.ZatPoint (x, y) <= Z)
	{
		RenderFlags |= RF_RELUPPER|RF_CLIPUPPER;
		if (line->flags & ML_DONTPEGTOP)
			Z -= front->GetPlaneTexZ(sector_t::ceiling);
		else
			Z -= back->GetPlaneTexZ(sector_t::ceiling);
		tex = wall->GetTexture(side_t::top);
	}
	else if (ffloor) // this is a 3d-floor segment - do this only if we know which one!
	{
		Sector=ffloor->model;
		RenderFlags |= RF_RELMID|RF_CLIPMID;
		if (line->flags & ML_DONTPEGBOTTOM)
			Z -= Sector->GetPlaneTexZ(sector_t::floor);
		else
			Z -= Sector->GetPlaneTexZ(sector_t::ceiling);

		if (ffloor->flags & FF_UPPERTEXTURE)
		{
			tex = wall->GetTexture(side_t::top);
		}
		else if (ffloor->flags & FF_LOWERTEXTURE)
		{
			tex = wall->GetTexture(side_t::bottom);
		}
		else
		{
			tex = ffloor->master->sidedef[0]->GetTexture(side_t::mid);
		}
	}
	else return FNullTextureID();
	CalcFracPos (wall, x, y);

	FTexture *texture = TexMan[tex];

	if (texture == NULL || texture->bNoDecals)
	{
		return FNullTextureID();
	}

	return tex;
}

fixed_t DBaseDecal::GetRealZ (const side_t *wall) const
{
	const line_t *line = wall->linedef;
	const sector_t *front, *back;

	if (line->sidedef[0] == wall)
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
		return Z;
	case RF_RELUPPER:
		if (line->flags & ML_DONTPEGTOP)
		{
			return Z + front->GetPlaneTexZ(sector_t::ceiling);
		}
		else
		{
			return Z + back->GetPlaneTexZ(sector_t::ceiling);
		}
	case RF_RELLOWER:
		if (line->flags & ML_DONTPEGBOTTOM)
		{
			return Z + front->GetPlaneTexZ(sector_t::ceiling);
		}
		else
		{
			return Z + back->GetPlaneTexZ(sector_t::floor);
		}
	case RF_RELMID:
		if (line->flags & ML_DONTPEGBOTTOM)
		{
			return Z + front->GetPlaneTexZ(sector_t::floor);
		}
		else
		{
			return Z + front->GetPlaneTexZ(sector_t::ceiling);
		}
	}
}

void DBaseDecal::CalcFracPos (side_t *wall, fixed_t x, fixed_t y)
{
	line_t *line = wall->linedef;
	vertex_t *v1, *v2;

	if (line->sidedef[0] == wall)
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
		LeftDistance = SafeDivScale30 (x - v1->x, dx);
	}
	else if (dy != 0)
	{
		LeftDistance = SafeDivScale30 (y - v1->y, dy);
	}
	else
	{
		LeftDistance = 0;
	}
}

static void GetWallStuff (side_t *wall, vertex_t *&v1, fixed_t &ldx, fixed_t &ldy)
{
	line_t *line = wall->linedef;
	if (line->sidedef[0] == wall)
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
	return (fixed_t)sqrt ((double)dx*(double)dx+(double)dy*(double)dy);
}

static side_t *NextWall (const side_t *wall)
{
	line_t *line = wall->linedef;

	if (line->sidedef[0] == wall)
	{
		if (line->sidedef[1] != NULL)
		{
			return line->sidedef[1];
		}
	}
	else if (line->sidedef[1] == wall)
	{
		return line->sidedef[0];
	}
	return NULL;
}

void DBaseDecal::SpreadLeft (fixed_t r, vertex_t *v1, side_t *feelwall, F3DFloor *ffloor)
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
		SpreadSource->CloneSelf (SpreadTemplate, x, y, SpreadZ, feelwall, ffloor);
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
				SpreadLeft (startr, v2, nextwall, ffloor);
			}
		}
	}
}

void DBaseDecal::SpreadRight (fixed_t r, side_t *feelwall, fixed_t wallsize, F3DFloor *ffloor)
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
				SpreadRight (r, nextwall, wallsize, ffloor);
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
		SpreadSource->CloneSelf (SpreadTemplate, x, y, SpreadZ, feelwall, ffloor);
		SpreadStack.Push (feelwall);
	}
}

void DBaseDecal::Spread (const FDecalTemplate *tpl, side_t *wall, fixed_t x, fixed_t y, fixed_t z, F3DFloor * ffloor)
{
	FTexture *tex;
	vertex_t *v1;
	fixed_t rorg, ldx, ldy;

	GetWallStuff (wall, v1, ldx, ldy);
	rorg = Length (x - v1->x, y - v1->y);

	tex = TexMan[PicNum];
	int dwidth = tex->GetWidth ();

	DecalWidth = dwidth * ScaleX;
	DecalLeft = tex->LeftOffset * ScaleX;
	DecalRight = DecalWidth - DecalLeft;
	SpreadSource = this;
	SpreadTemplate = tpl;
	SpreadZ = z;

	// Try spreading left first
	SpreadLeft (rorg - DecalLeft, v1, wall, ffloor);
	SpreadStack.Clear ();

	// Then try spreading right
	SpreadRight (rorg + DecalRight, wall,
			Length (wall->linedef->dx, wall->linedef->dy), ffloor);
	SpreadStack.Clear ();
}

DBaseDecal *DBaseDecal::CloneSelf (const FDecalTemplate *tpl, fixed_t ix, fixed_t iy, fixed_t iz, side_t *wall, F3DFloor * ffloor) const
{
	DBaseDecal *decal = new DBaseDecal(iz);
	if (decal != NULL)
	{
		if (decal->StickToWall (wall, ix, iy, ffloor).isValid())
		{
			tpl->ApplyToDecal (decal, wall);
			decal->AlphaColor = AlphaColor;
			decal->RenderFlags = (decal->RenderFlags & RF_DECALMASK) |
								 (this->RenderFlags & ~RF_DECALMASK);
		}
		else
		{
			decal->Destroy();
			return NULL;
		}
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
			DThinker *thinker = DThinker::FirstThinker (STAT_AUTODECAL);
			if (thinker != NULL)
			{
				thinker->Destroy();
			}
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
	}
}

void DImpactDecal::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
}

DImpactDecal::DImpactDecal ()
: DBaseDecal (STAT_AUTODECAL, 0)
{
	ImpactCount++;
}

DImpactDecal::DImpactDecal (fixed_t z)
: DBaseDecal (STAT_AUTODECAL, z)
{
	ImpactCount++;
}

void DImpactDecal::CheckMax ()
{
	if (ImpactCount >= cl_maxdecals)
	{
		DThinker *thinker = DThinker::FirstThinker (STAT_AUTODECAL);
		if (thinker != NULL)
		{
			thinker->Destroy();
		}
	}
}

DImpactDecal *DImpactDecal::StaticCreate (const char *name, fixed_t x, fixed_t y, fixed_t z, side_t *wall, F3DFloor * ffloor, PalEntry color)
{
	if (cl_maxdecals > 0)
	{
		const FDecalTemplate *tpl = DecalLibrary.GetDecalByName (name);

		if (tpl != NULL && (tpl = tpl->GetDecal()) != NULL)
		{
			return StaticCreate (tpl, x, y, z, wall, ffloor, color);
		}
	}
	return NULL;
}

DImpactDecal *DImpactDecal::StaticCreate (const FDecalTemplate *tpl, fixed_t x, fixed_t y, fixed_t z, side_t *wall, F3DFloor * ffloor, PalEntry color)
{
	DImpactDecal *decal = NULL;
	if (tpl != NULL && cl_maxdecals > 0 && !(wall->Flags & WALLF_NOAUTODECALS))
	{
		if (tpl->LowerDecal)
		{
			int lowercolor;
			const FDecalTemplate * tpl_low = tpl->LowerDecal->GetDecal();

			// If the default color of the lower decal is the same as the main decal's
			// apply the custom color as well.
			if (tpl->ShadeColor != tpl_low->ShadeColor) lowercolor=0;
			else lowercolor = color;
			StaticCreate (tpl_low, x, y, z, wall, ffloor, lowercolor);
		}
		DImpactDecal::CheckMax();
		decal = new DImpactDecal (z);
		if (decal == NULL)
		{
			return NULL;
		}

		if (!decal->StickToWall (wall, x, y, ffloor).isValid())
		{
			return NULL;
		}

		tpl->ApplyToDecal (decal, wall);
		if (color != 0)
		{
			decal->SetShade (color.r, color.g, color.b);
		}

		if (!cl_spreaddecals || !decal->PicNum.isValid())
		{
			return decal;
		}

		// Spread decal to nearby walls if it does not all fit on this one
		decal->Spread (tpl, wall, x, y, z, ffloor);
	}
	return decal;
}

DBaseDecal *DImpactDecal::CloneSelf (const FDecalTemplate *tpl, fixed_t ix, fixed_t iy, fixed_t iz, side_t *wall, F3DFloor * ffloor) const
{
	if (wall->Flags & WALLF_NOAUTODECALS)
	{
		return NULL;
	}

	DImpactDecal::CheckMax();
	DImpactDecal *decal = new DImpactDecal(iz);
	if (decal != NULL)
	{
		if (decal->StickToWall (wall, ix, iy, ffloor).isValid())
		{
			tpl->ApplyToDecal (decal, wall);
			decal->AlphaColor = AlphaColor;
			decal->RenderFlags = (decal->RenderFlags & RF_DECALMASK) |
								 (this->RenderFlags & ~RF_DECALMASK);
		}
		else
		{
			decal->Destroy();
			return NULL;
		}
	}
	return decal;
}

void DImpactDecal::Destroy ()
{
	ImpactCount--;
	Super::Destroy ();
}

CCMD (countdecals)
{
	Printf ("%d impact decals\n", ImpactCount);
}

CCMD (countdecalsreal)
{
	TThinkerIterator<DImpactDecal> iterator (STAT_AUTODECAL);
	int count = 0;

	while (iterator.Next())
		count++;

	Printf ("Counted %d impact decals\n", count);
}

CCMD (spray)
{
	if (who == NULL || argv.argc() < 2)
	{
		Printf ("Usage: spray <decal>\n");
		return;
	}

	Net_WriteByte (DEM_SPRAY);
	Net_WriteString (argv[1]);
}

DBaseDecal *ShootDecal(const FDecalTemplate *tpl, AActor *basisactor, sector_t *sec, fixed_t x, fixed_t y, fixed_t z, angle_t angle, fixed_t tracedist, bool permanent)
{
	if (tpl == NULL || (tpl = tpl->GetDecal()) == NULL)
	{
		return NULL;
	}

	FTraceResults trace;
	DBaseDecal *decal;
	side_t *wall;

	angle >>= ANGLETOFINESHIFT;

	Trace(x, y, z, sec,
		finecosine[angle], finesine[angle], 0,
		tracedist, 0, 0, NULL, trace, TRACE_NoSky);

	if (trace.HitType == TRACE_HitWall)
	{
		if (permanent)
		{
			decal = new DBaseDecal(trace.Z);
			wall = trace.Line->sidedef[trace.Side];
			decal->StickToWall(wall, trace.X, trace.Y, trace.ffloor);
			tpl->ApplyToDecal(decal, wall);
			// Spread decal to nearby walls if it does not all fit on this one
			if (cl_spreaddecals)
			{
				decal->Spread(tpl, wall, trace.X, trace.Y, trace.Z, trace.ffloor);
			}
			return decal;
		}
		else
		{
			return DImpactDecal::StaticCreate(tpl,
				trace.X, trace.Y, trace.Z,
				trace.Line->sidedef[trace.Side], NULL);
		}
	}
	return NULL;
}

class ADecal : public AActor
{
	DECLARE_CLASS (ADecal, AActor);
public:
	void BeginPlay ();
};

IMPLEMENT_CLASS (ADecal)

void ADecal::BeginPlay ()
{
	const FDecalTemplate *tpl;

	Super::BeginPlay ();

	int decalid = args[0] + (args[1] << 8); // [KS] High byte for decals.

	// If no decal is specified, don't try to create one.
	if (decalid != 0 && (tpl = DecalLibrary.GetDecalByNum (decalid)) != 0)
	{
		if (!tpl->PicNum.Exists())
		{
			Printf("Decal actor at (%d,%d) does not have a valid texture\n", X()>>FRACBITS, Y()>>FRACBITS);
		}
		else
		{
			// Look for a wall within 64 units behind the actor. If none can be
			// found, then no decal is created, and this actor is destroyed
			// without effectively doing anything.
			if (NULL == ShootDecal(tpl, this, Sector, X(), Y(), Z(), angle + ANGLE_180, 64*FRACUNIT, true))
			{
				DPrintf ("Could not find a wall to stick decal to at (%d,%d)\n", X()>>FRACBITS, Y()>>FRACBITS);
			}
		}
	}
	else
	{
		DPrintf ("Decal actor at (%d,%d) does not have a good template\n", X()>>FRACBITS, Y()>>FRACBITS);
	}
	// This actor doesn't need to stick around anymore.
	Destroy();
}
