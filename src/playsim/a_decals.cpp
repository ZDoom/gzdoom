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
#include "p_trace.h"
#include "decallib.h"
#include "c_dispatch.h"
#include "d_net.h"
#include "serializer_doom.h"
#include "doomdata.h"
#include "g_levellocals.h"
#include "vm.h"
#include "texturemanager.h"

EXTERN_CVAR (Bool, cl_spreaddecals)
EXTERN_CVAR (Int, cl_maxdecals)


//----------------------------------------------------------------------------
//
//
//
//----------------------------------------------------------------------------

struct SpreadInfo
{
	double DecalWidth, DecalLeft, DecalRight;
	double SpreadZ;
	const DBaseDecal *SpreadSource;
	const FDecalTemplate *SpreadTemplate;
	TArray<side_t *> SpreadStack;
};


//----------------------------------------------------------------------------
//
//
//
//----------------------------------------------------------------------------

IMPLEMENT_CLASS(DBaseDecal, false, true)

IMPLEMENT_POINTERS_START(DBaseDecal)
	IMPLEMENT_POINTER(WallPrev)
	IMPLEMENT_POINTER(WallNext)
IMPLEMENT_POINTERS_END

IMPLEMENT_CLASS(DImpactDecal, false, false)

//----------------------------------------------------------------------------
//
//
//
//----------------------------------------------------------------------------

void DBaseDecal::Construct(double z)
{
	Z = z;
	RenderStyle = STYLE_None;
	PicNum.SetInvalid();
}

//----------------------------------------------------------------------------
//
//
//
//----------------------------------------------------------------------------

void DBaseDecal::Construct(const AActor *basis)
{
	Z = basis->Z();
	ScaleX = basis->Scale.X;
	ScaleY = basis->Scale.Y;
	Alpha = basis->Alpha;
	AlphaColor = basis->fillcolor;
	Translation = basis->Translation;
	PicNum = basis->picnum;
	RenderFlags = basis->renderflags;
	RenderStyle = basis->RenderStyle;
}

//----------------------------------------------------------------------------
//
//
//
//----------------------------------------------------------------------------

void DBaseDecal::Construct(const DBaseDecal *basis)
{
	LeftDistance = basis->LeftDistance;
	Z = basis->Z;
	ScaleX = basis->ScaleX;
	ScaleY = basis->ScaleY;
	Alpha = basis->Alpha;
	AlphaColor = basis->AlphaColor;
	Translation = basis->Translation;
	PicNum = basis->PicNum;
	RenderFlags = basis->RenderFlags;
	RenderStyle = basis->RenderStyle;

}

//----------------------------------------------------------------------------
//
//
//
//----------------------------------------------------------------------------

void DBaseDecal::OnDestroy ()
{
	Remove ();
	Super::OnDestroy();
}

//----------------------------------------------------------------------------
//
//
//
//----------------------------------------------------------------------------

void DBaseDecal::Remove ()
{
	if (WallPrev == nullptr)
	{
		if (Side != nullptr) Side->AttachedDecals = WallNext;
	}
	else WallPrev->WallNext = WallNext;

	if (WallNext != nullptr) WallNext->WallPrev = WallPrev;

	WallPrev = nullptr;
	WallNext = nullptr;
}

//----------------------------------------------------------------------------
//
//
//
//----------------------------------------------------------------------------

void DBaseDecal::Serialize(FSerializer &arc)
{
	Super::Serialize (arc);
	arc("wallprev", WallPrev)
		("wallnext", WallNext)
		("leftdistance", LeftDistance)
		("z", Z)
		("scalex", ScaleX)
		("scaley", ScaleY)
		("alpha", Alpha)
		("alphacolor", AlphaColor)
		("translation", Translation)
		("picnum", PicNum)
		("renderflags", RenderFlags)
		("renderstyle", RenderStyle)
		("side", Side)
		("sector", Sector);
}

//----------------------------------------------------------------------------
//
//
//
//----------------------------------------------------------------------------

void DBaseDecal::GetXY (side_t *wall, double &ox, double &oy) const
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

	double dx = v2->fX() - v1->fX();
	double dy = v2->fY() - v1->fY();

	ox = v1->fX() + LeftDistance * dx;
	oy = v1->fY() + LeftDistance * dy;
}

//----------------------------------------------------------------------------
//
//
//
//----------------------------------------------------------------------------

void DBaseDecal::SetShade (uint32_t rgb)
{
	PalEntry *entry = (PalEntry *)&rgb;
	AlphaColor = rgb | (ColorMatcher.Pick (entry->r, entry->g, entry->b) << 24);
}

//----------------------------------------------------------------------------
//
//
//
//----------------------------------------------------------------------------

void DBaseDecal::SetShade (int r, int g, int b)
{
	AlphaColor = MAKEARGB(ColorMatcher.Pick (r, g, b), r, g, b);
}

//----------------------------------------------------------------------------
//
//
//
//----------------------------------------------------------------------------

void DBaseDecal::SetTranslation(uint32_t trans)
{
	Translation = trans;
}

//----------------------------------------------------------------------------
//
// Returns the texture the decal stuck to.
//
//----------------------------------------------------------------------------

FTextureID DBaseDecal::StickToWall (side_t *wall, double x, double y, F3DFloor *ffloor)
{
	Side = wall;
	WallPrev = wall->AttachedDecals;

	while (WallPrev != nullptr && WallPrev->WallNext != nullptr)
	{
		WallPrev = WallPrev->WallNext;
	}
	if (WallPrev != nullptr) WallPrev->WallNext = this;
	else wall->AttachedDecals = this;
	WallNext = nullptr;


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

	auto texture = TexMan.GetGameTexture(tex);

	if (texture == NULL || texture->allowNoDecals())
	{
		return FNullTextureID();
	}

	return tex;
}

//----------------------------------------------------------------------------
//
//
//
//----------------------------------------------------------------------------

double DBaseDecal::GetRealZ (const side_t *wall) const
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

//----------------------------------------------------------------------------
//
//
//
//----------------------------------------------------------------------------

void DBaseDecal::CalcFracPos (side_t *wall, double x, double y)
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

	double dx = v2->fX() - v1->fX();
	double dy = v2->fY() - v1->fY();

	if (fabs(dx) > fabs(dy))
	{
		LeftDistance = (x - v1->fX()) / dx;
	}
	else if (dy != 0)
	{
		LeftDistance = (y - v1->fY()) / dy;
	}
	else
	{
		LeftDistance = 0;
	}
}

//----------------------------------------------------------------------------
//
//
//
//----------------------------------------------------------------------------

static void GetWallStuff (side_t *wall, vertex_t *&v1, double &ldx, double &ldy)
{
	line_t *line = wall->linedef;
	if (line->sidedef[0] == wall)
	{
		v1 = line->v1;
		ldx = line->Delta().X;
		ldy = line->Delta().Y;
	}
	else
	{
		v1 = line->v2;
		ldx = -line->Delta().X;
		ldy = -line->Delta().Y;
	}
}

//----------------------------------------------------------------------------
//
//
//
//----------------------------------------------------------------------------

static double Length (double dx, double dy)
{
	return DVector2(dx, dy).Length();
}

//----------------------------------------------------------------------------
//
//
//
//----------------------------------------------------------------------------

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

//----------------------------------------------------------------------------
//
//
//
//----------------------------------------------------------------------------

void DBaseDecal::SpreadLeft (double r, vertex_t *v1, side_t *feelwall, F3DFloor *ffloor, SpreadInfo *spread)
{
	double ldx, ldy;

	spread->SpreadStack.Push (feelwall);

	while (r < 0 && feelwall->LeftSide != NO_SIDE)
	{
		double startr = r;

		double x = v1->fX();
		double y = v1->fY();

		feelwall = &feelwall->GetLevel()->sides[feelwall->LeftSide];
		GetWallStuff (feelwall, v1, ldx, ldy);
		double wallsize = Length (ldx, ldy);
		r += spread->DecalLeft;
		x += r*ldx / wallsize;
		y += r*ldy / wallsize;
		r = wallsize + startr;
		spread->SpreadSource->CloneSelf (spread->SpreadTemplate, x, y, spread->SpreadZ, feelwall, ffloor);
		spread->SpreadStack.Push (feelwall);

		side_t *nextwall = NextWall (feelwall);
		if (nextwall != NULL && nextwall->LeftSide != NO_SIDE)
		{
			int i;

			for (i = spread->SpreadStack.Size(); i-- > 0; )
			{
				if (spread->SpreadStack[i] == nextwall)
					break;
			}
			if (i == -1)
			{
				vertex_t *v2;

				GetWallStuff (nextwall, v2, ldx, ldy);
				SpreadLeft (startr, v2, nextwall, ffloor, spread);
			}
		}
	}
}

//----------------------------------------------------------------------------
//
//
//
//----------------------------------------------------------------------------

void DBaseDecal::SpreadRight (double r, side_t *feelwall, double wallsize, F3DFloor *ffloor, SpreadInfo *spread)
{
	vertex_t *v1;
	double x, y, ldx, ldy;

	spread->SpreadStack.Push (feelwall);

	while (r > wallsize && feelwall->RightSide != NO_SIDE)
	{
		feelwall = &feelwall->GetLevel()->sides[feelwall->RightSide];

		side_t *nextwall = NextWall (feelwall);
		if (nextwall != NULL && nextwall->LeftSide != NO_SIDE)
		{
			int i;

			for (i = spread->SpreadStack.Size(); i-- > 0; )
			{
				if (spread->SpreadStack[i] == nextwall)
					break;
			}
			if (i == -1)
			{
				SpreadRight (r, nextwall, wallsize, ffloor, spread);
			}
		}

		r = spread->DecalWidth - r + wallsize - spread->DecalLeft;
		GetWallStuff (feelwall, v1, ldx, ldy);
		x = v1->fX();
		y = v1->fY();
		wallsize = Length (ldx, ldy);
		x -= r*ldx / wallsize;
		y -= r*ldy / wallsize;
		r = spread->DecalRight - r;
		spread->SpreadSource->CloneSelf (spread->SpreadTemplate, x, y, spread->SpreadZ, feelwall, ffloor);
		spread->SpreadStack.Push (feelwall);
	}
}

//----------------------------------------------------------------------------
//
//
//
//----------------------------------------------------------------------------

void DBaseDecal::Spread (const FDecalTemplate *tpl, side_t *wall, double x, double y, double z, F3DFloor * ffloor)
{
	SpreadInfo spread;
	FGameTexture *tex;
	vertex_t *v1;
	double rorg, ldx, ldy;

	GetWallStuff (wall, v1, ldx, ldy);
	rorg = Length (x - v1->fX(), y - v1->fY());

	if ((tex = TexMan.GetGameTexture(PicNum)) == NULL)
	{
		return;
	}

	double dwidth = tex->GetDisplayWidth ();

	spread.DecalWidth = dwidth * ScaleX;
	spread.DecalLeft = tex->GetDisplayLeftOffset() * ScaleX;
	spread.DecalRight = spread.DecalWidth - spread.DecalLeft;
	spread.SpreadSource = this;
	spread.SpreadTemplate = tpl;
	spread.SpreadZ = z;

	// Try spreading left first
	SpreadLeft (rorg - spread.DecalLeft, v1, wall, ffloor, &spread);
	spread.SpreadStack.Clear ();

	// Then try spreading right
	SpreadRight (rorg + spread.DecalRight, wall,
			Length (wall->linedef->Delta().X, wall->linedef->Delta().Y), ffloor, &spread);
}

//----------------------------------------------------------------------------
//
//
//
//----------------------------------------------------------------------------

DBaseDecal *DBaseDecal::CloneSelf (const FDecalTemplate *tpl, double ix, double iy, double iz, side_t *wall, F3DFloor * ffloor) const
{
	DBaseDecal *decal = Level->CreateThinker<DBaseDecal>(iz);
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

//----------------------------------------------------------------------------
//
//
//
//----------------------------------------------------------------------------

void DImpactDecal::CheckMax ()
{
	static int SpawnCounter;

	if (++Level->ImpactDecalCount >= cl_maxdecals)
	{
		DThinker *thinker = Level->FirstThinker (STAT_AUTODECAL);
		if (thinker != NULL)
		{
			thinker->Destroy();
			Level->ImpactDecalCount--;
		}
	}
}

//----------------------------------------------------------------------------
//
//
//
//----------------------------------------------------------------------------

void DImpactDecal::Expired()
{
	Level->ImpactDecalCount--;
}

//----------------------------------------------------------------------------
//
//
//
//----------------------------------------------------------------------------

DBaseDecal* DImpactDecal::StaticCreate (FLevelLocals *Level, const char *name, const DVector3 &pos, side_t *wall, F3DFloor * ffloor, PalEntry color, uint32_t bloodTranslation)
{
	if (cl_maxdecals > 0)
	{
		const FDecalTemplate *tpl = DecalLibrary.GetDecalByName (name);

		if (tpl != NULL && (tpl = tpl->GetDecal()) != NULL)
		{
			return StaticCreate (Level, tpl, pos, wall, ffloor, color, bloodTranslation);
		}
	}
	return nullptr;
}

//----------------------------------------------------------------------------
//
//
//
//----------------------------------------------------------------------------

DBaseDecal* DImpactDecal::StaticCreate (FLevelLocals *Level, const FDecalTemplate *tpl, const DVector3 &pos, side_t *wall, F3DFloor * ffloor, PalEntry color, uint32_t bloodTranslation, bool permanent)
{
	DBaseDecal *decal = NULL;
	if (tpl != NULL && ((cl_maxdecals > 0 && !(wall->Flags & WALLF_NOAUTODECALS)) || permanent))
	{
		if (tpl->LowerDecal)
		{
			int lowercolor;
			const FDecalTemplate * tpl_low = tpl->LowerDecal->GetDecal();

			// If the default color of the lower decal is the same as the main decal's
			// apply the custom color as well.
			if (tpl->ShadeColor != tpl_low->ShadeColor) lowercolor=0;
			else lowercolor = color;

			uint32_t lowerTrans = (bloodTranslation != 0 ? bloodTranslation : 0);

			StaticCreate (Level, tpl_low, pos, wall, ffloor, lowercolor, lowerTrans, permanent);
		}
		if (!permanent) decal = Level->CreateThinker<DImpactDecal>(pos.Z);
		else decal = Level->CreateThinker<DBaseDecal>(pos.Z);
		if (decal == NULL)
		{
			return NULL;
		}

		if (!decal->StickToWall (wall, pos.X, pos.Y, ffloor).isValid())
		{
			decal->Destroy();
			return NULL;
		}
		if (!permanent) static_cast<DImpactDecal*>(decal)->CheckMax();

		tpl->ApplyToDecal (decal, wall);
		if (color != 0)
		{
			decal->SetShade (color.r, color.g, color.b);
		}

		// [Nash] opaque blood
		if (bloodTranslation != 0 && tpl->ShadeColor == 0 && tpl->opaqueBlood)
		{
			decal->SetTranslation(bloodTranslation);
			decal->RenderStyle = STYLE_Normal;
		}

		if (!cl_spreaddecals || !decal->PicNum.isValid())
		{
			return decal;
		}

		// Spread decal to nearby walls if it does not all fit on this one
		decal->Spread (tpl, wall, pos.X, pos.Y, pos.Z, ffloor);
	}
	return decal;
}

//----------------------------------------------------------------------------
//
//
//
//----------------------------------------------------------------------------

DBaseDecal *DImpactDecal::CloneSelf (const FDecalTemplate *tpl, double ix, double iy, double iz, side_t *wall, F3DFloor * ffloor) const
{
	if (wall->Flags & WALLF_NOAUTODECALS)
	{
		return NULL;
	}

	DImpactDecal *decal = Level->CreateThinker<DImpactDecal>(iz);
	if (decal != NULL)
	{
		if (decal->StickToWall (wall, ix, iy, ffloor).isValid())
		{
			decal->CheckMax();
			tpl->ApplyToDecal (decal, wall);
			decal->AlphaColor = AlphaColor;

			// [Nash] opaque blood
			if (tpl->ShadeColor == 0 && tpl->opaqueBlood)
			{
				decal->SetTranslation(Translation);
				decal->RenderStyle = STYLE_Normal;
			}

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

//----------------------------------------------------------------------------
//
//
//
//----------------------------------------------------------------------------

void SprayDecal(AActor *shooter, const char *name, double distance, DVector3 offset, DVector3 direction, bool useBloodColor, uint32_t decalColor)
{
	//just in case
	if (!shooter)
		return;

	FTraceResults trace;
	DVector3 off(0, 0, 0), dir(0, 0, 0);

	//use vanilla offset only if "custom" equal to zero
	if (offset.isZero() )
		off = shooter->PosPlusZ(shooter->Height / 2);

	else
		off = shooter->Pos() + offset;

	//same for direction
	if (direction.isZero() )
	{
		DAngle ang = shooter->Angles.Yaw;
		DAngle pitch = shooter->Angles.Pitch;
		double c = pitch.Cos();
		dir = DVector3(c * ang.Cos(), c * ang.Sin(), -pitch.Sin());
	}
	
	else
		dir = direction;

	uint32_t bloodTrans = useBloodColor ? shooter->BloodTranslation : 0;
	PalEntry entry = !useBloodColor ? (PalEntry)decalColor : shooter->BloodColor;

	if (Trace(off, shooter->Sector, dir, distance, 0, ML_BLOCKEVERYTHING, shooter, trace, TRACE_NoSky))
	{
		if (trace.HitType == TRACE_HitWall)
		{
			DImpactDecal::StaticCreate(shooter->Level, name, trace.HitPos, trace.Line->sidedef[trace.Side], NULL, entry, bloodTrans);
		}
	}
}

//----------------------------------------------------------------------------
//
//
//
//----------------------------------------------------------------------------

DBaseDecal *ShootDecal(FLevelLocals *Level, const FDecalTemplate *tpl, sector_t *sec, double x, double y, double z, DAngle angle, double tracedist, bool permanent)
{
	if (tpl == NULL || (tpl = tpl->GetDecal()) == NULL)
	{
		return NULL;
	}

	FTraceResults trace;

	Trace(DVector3(x,y,z), sec, DVector3(angle.ToVector(), 0), tracedist, 0, 0, NULL, trace, TRACE_NoSky);

	if (trace.HitType == TRACE_HitWall)
	{
		return DImpactDecal::StaticCreate(Level, tpl, trace.HitPos, trace.Line->sidedef[trace.Side], trace.ffloor, 0, 0, permanent);
	}
	return NULL;
}

//----------------------------------------------------------------------------
//
//
//
//----------------------------------------------------------------------------

static void SpawnDecal(AActor *self)
{
	const FDecalTemplate *tpl = nullptr;
	
	if (self->args[0] < 0)
	{
		FName name = ENamedName(-self->args[0]);
		tpl = DecalLibrary.GetDecalByName(name.GetChars());
	}
	else
	{
		int decalid = self->args[0] + (self->args[1] << 8); // [KS] High byte for decals.
		tpl = DecalLibrary.GetDecalByNum(decalid);
	}
	
	// If no decal is specified, don't try to create one.
	if (tpl != nullptr)
	{
		if (!tpl->PicNum.Exists())
		{
			Printf("Decal actor at (%f,%f) does not have a valid texture\n", self->X(), self->Y());
		}
		else
		{
			// Look for a wall within 64 units behind the actor. If none can be
			// found, then no decal is created, and this actor is destroyed
			// without effectively doing anything.
			if (!ShootDecal(self->Level, tpl, self->Sector, self->X(), self->Y(), self->Z(), self->Angles.Yaw + DAngle::fromDeg(180), 64., true))
			{
				DPrintf (DMSG_WARNING, "Could not find a wall to stick decal to at (%f,%f)\n", self->X(), self->Y());
			}
		}
	}
	else
	{
		DPrintf (DMSG_ERROR, "Decal actor at (%f,%f) does not have a good template\n", self->X(), self->Y());
	}
}

DEFINE_ACTION_FUNCTION_NATIVE(ADecal, SpawnDecal, SpawnDecal)
{
	PARAM_SELF_PROLOGUE(AActor);
	SpawnDecal(self);
	return 0;
}
