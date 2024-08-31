/*
**
**
**---------------------------------------------------------------------------
** Copyright 2018 ZZYZX
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
#include "p_spec.h"
#include "g_levellocals.h"
#include "p_destructible.h"
#include "v_text.h"
#include "actor.h"
#include "p_trace.h"
#include "p_lnspec.h"
#include "p_local.h"
#include "p_maputl.h"
#include "c_cvars.h"
#include "serializer.h"
#include "vm.h"
#include "events.h"

//==========================================================================
//
// [ZZ] Geometry damage logic callbacks
//
//==========================================================================
void P_SetHealthGroupHealth(FHealthGroup* grp, int health)
{
	if (!grp) return;

	grp->health = health;

	// now set health of all linked objects to the same as this one
	for (unsigned i = 0; i < grp->lines.Size(); i++)
	{
		line_t* lline = grp->lines[i];
		lline->health = health;
	}
	//
	for (unsigned i = 0; i < grp->sectors.Size(); i++)
	{
		sector_t* lsector = grp->sectors[i];
		if (lsector->healthceilinggroup == grp->id)
			lsector->healthceiling = health;
		if (lsector->healthfloorgroup == grp->id)
			lsector->healthfloor = health;
		if (lsector->health3dgroup == grp->id)
			lsector->health3d = health;
	}
}

void P_SetHealthGroupHealth(FLevelLocals *Level, int id, int health)
{
	P_SetHealthGroupHealth(P_GetHealthGroup(Level, id), health);
}

void P_DamageHealthGroup(FHealthGroup* grp, void* object, AActor* source, int damage, FName damagetype, int side, int part, DVector3 position, bool isradius)
{
	if (!grp) return;
	int group = grp->id;

	// now set health of all linked objects to the same as this one
	for (unsigned i = 0; i < grp->lines.Size(); i++)
	{
		line_t* lline = grp->lines[i];
		if (lline == object)
			continue;
		lline->health = grp->health + damage;
		P_DamageLinedef(lline, source, damage, damagetype, side, position, isradius, false);
	}
	//
	for (unsigned i = 0; i < grp->sectors.Size(); i++)
	{
		sector_t* lsector = grp->sectors[i];
		
		if (lsector->healthceilinggroup == group && (lsector != object || part != SECPART_Ceiling))
		{
			lsector->healthceiling = grp->health + damage;
			P_DamageSector(lsector, source, damage, damagetype, SECPART_Ceiling, position, isradius, false);
		}
		
		if (lsector->healthfloorgroup == group && (lsector != object || part != SECPART_Floor))
		{
			lsector->healthfloor = grp->health + damage;
			P_DamageSector(lsector, source, damage, damagetype, SECPART_Floor, position, isradius, false);
		}

		if (lsector->health3dgroup == group && (lsector != object || part != SECPART_3D))
		{
			lsector->health3d = grp->health + damage;
			P_DamageSector(lsector, source, damage, damagetype, SECPART_3D, position, isradius, false);
		}
	}
}

void P_DamageLinedef(line_t* line, AActor* source, int damage, FName damagetype, int side, DVector3 position, bool isradius, bool dogroups)
{
	if (damage < 0) damage = 0;

	if (dogroups)
	{
		damage = line->GetLevel()->localEventManager->WorldLineDamaged(line, source, damage, damagetype, side, position, isradius);
		if (damage < 0) damage = 0;
	}

	if (!damage) return;

	line->health -= damage;
	if (line->health < 0) line->health = 0;
	auto Level = line->GetLevel();

	// callbacks here
	// first off, call special if needed
	if ((line->activation & SPAC_Damage) || ((line->activation & SPAC_Death) && !line->health))
	{
		int activateon = SPAC_Damage;
		if (!line->health) activateon |= SPAC_Death;
		P_ActivateLine(line, source, side, activateon&line->activation, &position);
	}

	if (dogroups && line->healthgroup)
	{
		FHealthGroup* grp = P_GetHealthGroup(Level, line->healthgroup);
		if (grp)
			grp->health = line->health;
		P_DamageHealthGroup(grp, line, source, damage, damagetype, side, -1, position, isradius);
	}

	//Printf("P_DamageLinedef: %d damage (type=%s, source=%p), new health = %d\n", damage, damagetype.GetChars(), source, line->health);
}

void P_DamageSector(sector_t* sector, AActor* source, int damage, FName damagetype, int part, DVector3 position, bool isradius, bool dogroups)
{
	if (damage < 0) damage = 0;

	if (dogroups)
	{
		damage = sector->Level->localEventManager->WorldSectorDamaged(sector, source, damage, damagetype, part, position, isradius);
		if (damage < 0) damage = 0;
	}

	if (!damage) return;

	auto Level = sector->Level;
	int* sectorhealth;
	int group;
	int dmg;
	int dth;
	switch (part)
	{
	case SECPART_Ceiling:
		sectorhealth = &sector->healthceiling;
		group = sector->healthceilinggroup;
		dmg = SECSPAC_DamageCeiling;
		dth = SECSPAC_DeathCeiling;
		break;
	case SECPART_Floor:
		sectorhealth = &sector->healthfloor;
		group = sector->healthfloorgroup;
		dmg = SECSPAC_DamageFloor;
		dth = SECSPAC_DeathFloor;
		break;
	case SECPART_3D:
		sectorhealth = &sector->health3d;
		group = sector->health3dgroup;
		dmg = SECSPAC_Damage3D;
		dth = SECSPAC_Death3D;
		break;
	default:
		return;
	}

	int newhealth = *sectorhealth - damage;
	if (newhealth < 0) newhealth = 0;
	
	*sectorhealth = newhealth;

	// callbacks here
	if (sector->SecActTarget)
	{
		sector->TriggerSectorActions(source, dmg);
		if (newhealth == 0)
			sector->TriggerSectorActions(source, dth);
	}

	if (dogroups && group)
	{
		FHealthGroup* grp = P_GetHealthGroup(Level, group);
		if (grp)
			grp->health = newhealth;
		P_DamageHealthGroup(grp, sector, source, damage, damagetype, 0, part, position, isradius);
	}

	//Printf("P_DamageSector: %d damage (type=%s, position=%s, source=%p), new health = %d\n", damage, damagetype.GetChars(), (part == SECPART_Ceiling) ? "ceiling" : "floor", source, newhealth);
}


//===========================================================================
//
//
//
//===========================================================================

FHealthGroup* P_GetHealthGroup(FLevelLocals *Level, int id)
{
	FHealthGroup* grp = Level->healthGroups.CheckKey(id);
	return grp;
}

FHealthGroup* P_GetHealthGroupOrNew(FLevelLocals *Level, int id, int health)
{
	FHealthGroup* grp = Level->healthGroups.CheckKey(id);
	if (!grp)
	{
		FHealthGroup newgrp;
		newgrp.id = id;
		newgrp.health = health;
		grp = &(Level->healthGroups.Insert(id, newgrp));
	}
	return grp;
}

void P_InitHealthGroups(FLevelLocals *Level)
{
	Level->healthGroups.Clear();
	TArray<int> groupsInError;

	for (unsigned i = 0; i < Level->lines.Size(); i++)
	{
		line_t* lline = &Level->lines[i];
		if (lline->healthgroup > 0)
		{
			FHealthGroup* grp = P_GetHealthGroupOrNew(Level, lline->healthgroup, lline->health);
			if (grp->health != lline->health)
			{
				Printf(TEXTCOLOR_RED "Warning: line %d in health group %d has different starting health (line health = %d, group health = %d)\n", i, lline->healthgroup, lline->health, grp->health);
				groupsInError.Push(lline->healthgroup);
				if (lline->health > grp->health)
					grp->health = lline->health;
			}
			grp->lines.Push(lline);
		}
	}
	for (unsigned i = 0; i < Level->sectors.Size(); i++)
	{
		sector_t* lsector = &Level->sectors[i];
		if (lsector->healthceilinggroup > 0)
		{
			FHealthGroup* grp = P_GetHealthGroupOrNew(Level, lsector->healthceilinggroup, lsector->healthceiling);
			if (grp->health != lsector->healthceiling)
			{
				Printf(TEXTCOLOR_RED "Warning: sector ceiling %d in health group %d has different starting health (ceiling health = %d, group health = %d)\n", i, lsector->healthceilinggroup, lsector->healthceiling, grp->health);
				groupsInError.Push(lsector->healthceilinggroup);
				if (lsector->healthceiling > grp->health)
					grp->health = lsector->healthceiling;
			}
			grp->sectors.Push(lsector);
		}
		if (lsector->healthfloorgroup > 0)
		{
			FHealthGroup* grp = P_GetHealthGroupOrNew(Level, lsector->healthfloorgroup, lsector->healthfloor);
			if (grp->health != lsector->healthfloor)
			{
				Printf(TEXTCOLOR_RED "Warning: sector floor %d in health group %d has different starting health (floor health = %d, group health = %d)\n", i, lsector->healthfloorgroup, lsector->healthfloor, grp->health);
				groupsInError.Push(lsector->healthfloorgroup);
				if (lsector->healthfloor > grp->health)
					grp->health = lsector->healthfloor;
			}
			if (lsector->healthceilinggroup != lsector->healthfloorgroup)
				grp->sectors.Push(lsector);
		}
	}
	for (unsigned i = 0; i < groupsInError.Size(); i++)
	{
		FHealthGroup* grp = P_GetHealthGroup(Level, groupsInError[i]);
		Printf(TEXTCOLOR_GOLD "Health group %d is using the highest found health value of %d", groupsInError[i], grp->health);
	}
}

//==========================================================================
//
// P_GeometryLineAttack
//
// Applies hitscan damage to geometry (lines and sectors with health>0)
//==========================================================================

void P_GeometryLineAttack(FTraceResults& trace, AActor* thing, int damage, FName damageType)
{
	// [ZZ] hitscan geometry damage logic
	//      

	// check 3d floor, but still allow the wall to take generic damage
	if (trace.HitType == TRACE_HitWall && trace.Tier == TIER_FFloor)
	{
		if (trace.ffloor && trace.ffloor->model && trace.ffloor->model->health3d)
			P_DamageSector(trace.ffloor->model, thing, damage, damageType, SECPART_3D, trace.HitPos, false, true);
	}
	
	if (trace.HitType == TRACE_HitWall && P_CheckLinedefVulnerable(trace.Line, trace.Side))
	{
		if (trace.Tier == TIER_Lower || trace.Tier == TIER_Upper) // process back sector health if any
		{
			sector_t* backsector = (trace.Line->sidedef[!trace.Side] ? trace.Line->sidedef[!trace.Side]->sector : nullptr);
			int sectorhealth = 0;
			if (backsector && trace.Tier == TIER_Lower && backsector->healthfloor > 0 && P_CheckLinedefVulnerable(trace.Line, trace.Side, SECPART_Floor))
				sectorhealth = backsector->healthfloor;
			if (backsector && trace.Tier == TIER_Upper && backsector->healthceiling > 0 && P_CheckLinedefVulnerable(trace.Line, trace.Side, SECPART_Ceiling))
				sectorhealth = backsector->healthceiling;
			if (sectorhealth > 0)
			{
				P_DamageSector(backsector, thing, damage, damageType, (trace.Tier == TIER_Upper) ? SECPART_Ceiling : SECPART_Floor, trace.HitPos, false, true);
			}
		}
		// always process linedef health if any
		if (trace.Line->health > 0)
		{
			P_DamageLinedef(trace.Line, thing, damage, damageType, trace.Side, trace.HitPos, false, true);
		}
		// fake floors are not handled
	}
	else if (trace.HitType == TRACE_HitFloor || trace.HitType == TRACE_HitCeiling)
	{
		// check for 3d floors. if a 3d floor was hit, it'll block any interaction with the sector planes at the same point, if present.
		// i.e. if there are 3d floors at the same height as the sector's real floor/ceiling, and they blocked the shot, then it won't damage.
		bool hit3dfloors = false;
		sector_t* sector = trace.Sector;
		for (auto f : sector->e->XFloor.ffloors)
		{
			if (!(f->flags & FF_EXISTS)) continue;
			if (!(f->flags & FF_SOLID) || (f->flags & FF_SHOOTTHROUGH)) continue;

			if (!f->model) continue;
			if (trace.HitType == TRACE_HitFloor && fabs(f->top.plane->ZatPoint(trace.HitPos.XY())-trace.HitPos.Z) <= EQUAL_EPSILON)
			{
				if (f->model->health3d)
					P_DamageSector(f->model, thing, damage, damageType, SECPART_3D, trace.HitPos, false, true);
				hit3dfloors = true;
			}
			else if (trace.HitType == TRACE_HitCeiling && fabs(f->bottom.plane->ZatPoint(trace.HitPos.XY())-trace.HitPos.Z) <= EQUAL_EPSILON)
			{
				if (f->model->health3d)
					P_DamageSector(f->model, thing, damage, damageType, SECPART_3D, trace.HitPos, false, true);
				hit3dfloors = true;
			}
		}

		if (hit3dfloors)
			return;

		int sectorhealth = 0;
		if (trace.HitType == TRACE_HitFloor && trace.Sector->healthfloor > 0 && P_CheckSectorVulnerable(trace.Sector, SECPART_Floor))
			sectorhealth = trace.Sector->healthfloor;
		if (trace.HitType == TRACE_HitCeiling && trace.Sector->healthceiling > 0 && P_CheckSectorVulnerable(trace.Sector, SECPART_Ceiling))
			sectorhealth = trace.Sector->healthceiling;
		if (sectorhealth > 0)
		{
			P_DamageSector(trace.Sector, thing, damage, damageType, (trace.HitType == TRACE_HitCeiling) ? SECPART_Ceiling : SECPART_Floor, trace.HitPos, false, true);
		}
	}
}

//==========================================================================
//
// P_GeometryRadiusAttack
//
// Applies radius damage to destructible geometry (lines and sectors with health>0)
//==========================================================================

struct pgra_data_t
{
	DVector3 pos;
	line_t* line;
	sector_t* sector;
	int secpart;
};

// we use this horizontally for 2D wall distance, and in a bit fancier way for 3D wall distance
static DVector2 PGRA_ClosestPointOnLine2D(DVector2 x, DVector2 p1, DVector2 p2)
{
	DVector2 p2p1 = (p2 - p1);
	DVector2 xp1 = (x - p1);
	double r = p2p1 | xp1;
	double len = p2p1.Length();
	r /= len;

	if (r < 0)
		return p1;
	if (r > len)
		return p2;

	return p1 + p2p1.Unit() * r;
}

static bool PGRA_CheckExplosionBlocked(DVector3 pt, DVector3 check, sector_t* checksector)
{
	// simple solid geometry sight check between "check" and "pt"
	// expected - Trace hits nothing
	check.Z += EQUAL_EPSILON; // this is so that floor under the rocket doesn't block explosion
	DVector3 ptVec = (pt - check);
	double ptDst = ptVec.Length() - 0.5;
	ptVec.MakeUnit();
	FTraceResults res;
	bool isblocked = Trace(check, checksector, ptVec, ptDst, 0, ML_BLOCKEVERYTHING, nullptr, res);
	return isblocked;
}

static void PGRA_InsertIfCloser(TMap<int, pgra_data_t>& damageGroupPos, int group, DVector3 pt, DVector3 check, sector_t* checksector, sector_t* sector, line_t* line, int secpart)
{
	if (PGRA_CheckExplosionBlocked(pt, check, checksector))
		return;

	pgra_data_t* existing = damageGroupPos.CheckKey(group);
	// not present or distance is closer
	if (!existing || (((*existing).pos - check).Length() > (pt - check).Length()))
	{
		if (!existing) existing = &damageGroupPos.Insert(group, pgra_data_t());
		existing->pos = pt;
		existing->line = line;
		existing->sector = sector;
		existing->secpart = secpart;
	}
}

EXTERN_CVAR(Float, splashfactor);

void P_GeometryRadiusAttack(AActor* bombspot, AActor* bombsource, int bombdamage, double bombdistance, FName damagetype, double fulldamagedistance)
{
	TMap<int, pgra_data_t> damageGroupPos;

	double bombdistancefloat = 1.0 / (bombdistance - fulldamagedistance);

	// now, this is not entirely correct... but sector actions still _do_ require a valid source actor to trigger anything
	if (!bombspot)
		return;
	if (!bombsource)
		bombsource = bombspot;

	// check current sector floor and ceiling. this is the only *sector* checked, otherwise only use lines
	// a bit imprecise, but should work
	sector_t* srcsector = bombspot->Sector;

	if (srcsector->healthceiling > 0)
	{
		double dstceiling = srcsector->ceilingplane.Normal() | (bombspot->Pos() + srcsector->ceilingplane.Normal()*srcsector->ceilingplane.D);
		DVector3 spotTo = bombspot->Pos() - srcsector->ceilingplane.Normal() * dstceiling;
		int grp = srcsector->healthceilinggroup;
		if (grp <= 0)
			grp = 0x80000000 | (srcsector->sectornum & 0x0FFFFFFF);
		PGRA_InsertIfCloser(damageGroupPos, grp, spotTo, bombspot->Pos(), srcsector, srcsector, nullptr, SECPART_Ceiling);
	}

	if (srcsector->healthfloor > 0)
	{
		double dstfloor = srcsector->floorplane.Normal() | (bombspot->Pos() + srcsector->floorplane.Normal()*srcsector->floorplane.D);
		DVector3 spotTo = bombspot->Pos() - srcsector->floorplane.Normal() * dstfloor;
		int grp = srcsector->healthfloorgroup;
		if (grp <= 0)
			grp = 0x40000000 | (srcsector->sectornum & 0x0FFFFFFF);
		PGRA_InsertIfCloser(damageGroupPos, grp, spotTo, bombspot->Pos(), srcsector, srcsector, nullptr, SECPART_Floor);
	}

	for (auto f : srcsector->e->XFloor.ffloors)
	{
		if (!(f->flags & FF_EXISTS)) continue;
		if (!(f->flags & FF_SOLID)) continue;

		if (!f->model || !f->model->health3d) continue;

		double ff_top = f->top.plane->ZatPoint(bombspot->Pos());
		double ff_bottom = f->bottom.plane->ZatPoint(bombspot->Pos());
		if (ff_top < ff_bottom) // ignore eldritch geometry
			continue;

		int grp = f->model->health3dgroup;
		if (grp <= 0)
			grp = 0x20000000 | (f->model->sectornum & 0x0FFFFFFF);

		DVector3 spotTo;
		
		if (bombspot->Z() < ff_bottom) // use bottom plane
		{
			double dst = f->bottom.plane->Normal() | (bombspot->Pos() + f->bottom.plane->Normal()*f->bottom.plane->D);
			spotTo = bombspot->Pos() - f->bottom.plane->Normal() * dst;
		}
		else if (bombspot->Z() > ff_top) // use top plane
		{
			double dst = f->top.plane->Normal() | (bombspot->Pos() + f->top.plane->Normal()*f->top.plane->D);
			spotTo = bombspot->Pos() - f->top.plane->Normal() * dst;
		}
		else // explosion right inside the floor. do 100% damage
		{
			spotTo = bombspot->Pos();
		}

		PGRA_InsertIfCloser(damageGroupPos, grp, spotTo, bombspot->Pos(), srcsector, f->model, nullptr, SECPART_3D);
	}

	// enumerate all lines around
	FBoundingBox bombbox(bombspot->X(), bombspot->Y(), bombdistance);
	FBlockLinesIterator it(bombspot->Level, bombbox);
	line_t* ln;
	int vc = validcount;
	TArray<line_t*> lines;
	while ((ln = it.Next())) // iterator and Trace both use validcount and interfere with each other
		lines.Push(ln);
	for (unsigned i = 0; i < lines.Size(); i++)
	{
		ln = lines[i];
		DVector2 pos2d = bombspot->Pos().XY();
		int sd = P_PointOnLineSide(pos2d, ln);

		side_t* side = ln->sidedef[sd];
		if (!side) continue;
		sector_t* sector = side->sector;
		side_t* otherside = ln->sidedef[!sd];
		sector_t* othersector = otherside ? otherside->sector : nullptr;
		if (!ln->health && (!othersector || (!othersector->healthfloor && !othersector->healthceiling && !othersector->e->XFloor.ffloors.Size())))
			continue; // non-interactive geometry

		DVector2 to2d = PGRA_ClosestPointOnLine2D(bombspot->Pos().XY(), side->V1()->p, side->V2()->p);
		// this gives us x/y
		double distto2d = (to2d - pos2d).Length();
		double z_top1, z_top2, z_bottom1, z_bottom2; // here, z_top1 is closest to the ceiling, and z_bottom1 is closest to the floor.
		z_top1 = sector->ceilingplane.ZatPoint(to2d);
		z_top2 = othersector ? othersector->ceilingplane.ZatPoint(to2d) : z_top1;
		z_bottom1 = sector->floorplane.ZatPoint(to2d);
		z_bottom2 = othersector ? othersector->floorplane.ZatPoint(to2d) : z_bottom1;
		DVector3 to3d_fullheight(to2d.X, to2d.Y, clamp(bombspot->Z(), z_bottom1, z_top1));

		if (ln->health && P_CheckLinedefVulnerable(ln, sd))
		{
			bool cantdamage = false;
			bool linefullheight = !othersector || !!(ln->flags & (ML_BLOCKEVERYTHING));
			// decide specific position to affect on a line.
			if (!linefullheight)
			{
				z_top2 = othersector->ceilingplane.ZatPoint(to2d);
				z_bottom2 = othersector->floorplane.ZatPoint(to2d);
				double bombz = bombspot->Z();
				if (z_top2 > z_top1)
					z_top2 = z_top1;
				if (z_bottom2 < z_bottom1)
					z_bottom2 = z_bottom1;
				if (bombz <= z_top2 && bombz >= z_bottom2) // between top and bottom opening
					to3d_fullheight.Z = (z_top2 - bombz < bombz - z_bottom2) ? z_top2 : z_bottom2;
				else if (bombz < z_bottom2 && bombz >= z_bottom1)
					to3d_fullheight.Z = clamp(bombz, z_bottom1, z_bottom2);
				else if (bombz > z_top2 && bombz <= z_top1)
					to3d_fullheight.Z = clamp(bombz, z_top2, z_top1);
				else cantdamage = true;
			}

			if (!cantdamage)
			{
				if (ln->healthgroup)
				{
					PGRA_InsertIfCloser(damageGroupPos, ln->healthgroup, to3d_fullheight, bombspot->Pos(), srcsector, nullptr, ln, -1);
				}
				else if (!PGRA_CheckExplosionBlocked(to3d_fullheight, bombspot->Pos(), srcsector))
				{
					// otherwise just damage line
					double dst = (to3d_fullheight - bombspot->Pos()).Length();
					int damage = 0;
					if (dst < bombdistance)
					{
						dst = clamp<double>(dst - fulldamagedistance, 0.0, dst);
						damage = (int)((double)bombdamage * (1.0 - dst * bombdistancefloat));
						if (bombsource == bombspot)
							damage = (int)(damage * splashfactor);
					}
					P_DamageLinedef(ln, bombsource, damage, damagetype, sd, to3d_fullheight, true, true);
				}
			}
		}

		if (othersector && othersector->healthceiling && P_CheckLinedefVulnerable(ln, sd, SECPART_Ceiling))
		{
			if (z_top2 < z_top1) // we have front side to hit against
			{
				DVector3 to3d_upper(to2d.X, to2d.Y, clamp(bombspot->Z(), z_top2, z_top1));
				int grp = othersector->healthceilinggroup;
				if (grp <= 0)
					grp = 0x80000000 | (othersector->sectornum & 0x0FFFFFFF);
				PGRA_InsertIfCloser(damageGroupPos, grp, to3d_upper, bombspot->Pos(), srcsector, othersector, nullptr, SECPART_Ceiling);
			}
		}

		if (othersector && othersector->healthfloor && P_CheckLinedefVulnerable(ln, sd, SECPART_Floor))
		{
			if (z_bottom2 > z_bottom1) // we have front side to hit against
			{
				DVector3 to3d_lower(to2d.X, to2d.Y, clamp(bombspot->Z(), z_bottom1, z_bottom2));
				int grp = othersector->healthfloorgroup;
				if (grp <= 0)
					grp = 0x40000000 | (othersector->sectornum & 0x0FFFFFFF);
				PGRA_InsertIfCloser(damageGroupPos, grp, to3d_lower, bombspot->Pos(), srcsector, othersector, nullptr, SECPART_Floor);
			}
		}

		// check 3d floors
		if (othersector)
		{
			for (auto f : othersector->e->XFloor.ffloors)
			{
				if (!(f->flags & FF_EXISTS)) continue;
				if (!(f->flags & FF_SOLID)) continue;

				if (!f->model || !f->model->health3d) continue;

				// 3d floors over real ceiling, or under real floor, are ignored
				double z_ff_top = clamp(f->top.plane->ZatPoint(to2d), z_bottom2, z_top2);
				double z_ff_bottom = clamp(f->bottom.plane->ZatPoint(to2d), z_bottom2, z_top2);
				if (z_ff_top < z_ff_bottom)
					continue; // also ignore eldritch geometry

				DVector3 to3d_ffloor(to2d.X, to2d.Y, clamp(bombspot->Z(), z_ff_bottom, z_ff_top));
				int grp = f->model->health3dgroup;
				if (grp <= 0)
					grp = 0x20000000 | (f->model->sectornum & 0x0FFFFFFF);
				PGRA_InsertIfCloser(damageGroupPos, grp, to3d_ffloor, bombspot->Pos(), srcsector, f->model, nullptr, SECPART_3D);
			}
		}

	}

	// damage health groups and sectors.
	// if group & 0x80000000, this is sector ceiling without health group
	// if grpup & 0x40000000, this is sector floor without health group
	// otherwise this is some object in health group
	TMap<int, pgra_data_t>::ConstIterator damageGroupIt(damageGroupPos);
	TMap<int, pgra_data_t>::ConstPair* damageGroupPair;
	while (damageGroupIt.NextPair(damageGroupPair))
	{
		DVector3 pos = damageGroupPair->Value.pos;
		double dst = (pos - bombspot->Pos()).Length();
		int damage = 0;
		if (dst < bombdistance)
		{
			dst = clamp<double>(dst - fulldamagedistance, 0.0, dst);
			damage = (int)((double)bombdamage * (1.0 - dst * bombdistancefloat));
			if (bombsource == bombspot)
				damage = (int)(damage * splashfactor);
		}

		// for debug
		//P_SpawnParticle(damageGroupPair->Value.pos, DVector3(), DVector3(), PalEntry(0xFF, 0x00, 0x00), 1.0, 35, 5.0, 0.0, 0.0, 0);

		int grp = damageGroupPair->Key;
		if (grp & 0x80000000) // sector ceiling
		{
			assert(damageGroupPair->Value.sector != nullptr);
			P_DamageSector(damageGroupPair->Value.sector, bombsource, damage, damagetype, SECPART_Ceiling, pos, true, true);
		}
		else if (grp & 0x40000000) // sector floor
		{
			assert(damageGroupPair->Value.sector != nullptr);
			P_DamageSector(damageGroupPair->Value.sector, bombsource, damage, damagetype, SECPART_Floor, pos, true, true);
		}
		else if (grp & 0x20000000) // sector 3d
		{
			assert(damageGroupPair->Value.sector != nullptr);
			P_DamageSector(damageGroupPair->Value.sector, bombsource, damage, damagetype, SECPART_3D, pos, true, true);
		}
		else
		{
			assert((damageGroupPair->Value.sector != nullptr) != (damageGroupPair->Value.line != nullptr));
			if (damageGroupPair->Value.line != nullptr)
				P_DamageLinedef(damageGroupPair->Value.line, bombsource, damage, damagetype, P_PointOnLineSide(pos.XY(), damageGroupPair->Value.line), pos, true, true);
			else P_DamageSector(damageGroupPair->Value.sector, bombsource, damage, damagetype, damageGroupPair->Value.secpart, pos, true, true);
		}
	}
}

//==========================================================================
//
// P_ProjectileHitLinedef
//
// Called if P_ExplodeMissile was called against a wall.
//==========================================================================

bool P_ProjectileHitLinedef(AActor* mo, line_t* line)
{
	bool washit = false;
	// detect 3d floor hit
	if (mo->Blocking3DFloor)
	{
		if (mo->Blocking3DFloor->health3d > 0)
		{
			P_DamageSector(mo->Blocking3DFloor, mo, mo->GetMissileDamage((mo->flags4 & MF4_STRIFEDAMAGE) ? 3 : 7, 1), mo->DamageType, SECPART_3D, mo->Pos(), false, true);
			washit = true;
		}
	}

	int wside = P_PointOnLineSide(mo->Pos().XY(), line);
	int oside = !wside;
	side_t* otherside = line->sidedef[oside];
	// check if hit upper or lower part
	if (otherside)
	{
		sector_t* othersector = otherside->sector;
		// find closest pos from line to MO.
		// this logic is so that steep slopes work correctly (value at the line is used, instead of value below the rocket actor)
		DVector2 moRelPos = line->v1->p - mo->Pos().XY();
		DVector2 lineNormal = line->delta.Rotated90CW().Unit();
		double moRelDst = lineNormal | moRelPos;
		DVector2 moPos = mo->Pos().XY() - lineNormal*fabs(moRelDst);

		double otherfloorz = othersector->floorplane.ZatPoint(moPos);
		double otherceilingz = othersector->ceilingplane.ZatPoint(moPos);
		double zbottom = mo->Pos().Z;
		double ztop = mo->Pos().Z + mo->Height;
		if (zbottom < (otherfloorz + EQUAL_EPSILON) && othersector->healthfloor > 0 && P_CheckLinedefVulnerable(line, wside, SECPART_Floor))
		{
			P_DamageSector(othersector, mo, mo->GetMissileDamage((mo->flags4 & MF4_STRIFEDAMAGE) ? 3 : 7, 1), mo->DamageType, SECPART_Floor, mo->Pos(), false, true);
			washit = true;
		}
		if (ztop > (otherceilingz - EQUAL_EPSILON) && othersector->healthceiling > 0 && P_CheckLinedefVulnerable(line, wside, SECPART_Ceiling))
		{
			P_DamageSector(othersector, mo, mo->GetMissileDamage((mo->flags4 & MF4_STRIFEDAMAGE) ? 3 : 7, 1), mo->DamageType, SECPART_Ceiling, mo->Pos(), false, true);
			washit = true;
		}
	}

	if (line->health > 0 && P_CheckLinedefVulnerable(line, wside))
	{
		P_DamageLinedef(line, mo, mo->GetMissileDamage((mo->flags4 & MF4_STRIFEDAMAGE) ? 3 : 7, 1), mo->DamageType, wside, mo->Pos(), false, true);
		washit = true;
	}

	return washit;
}

// part = -1 means "detect from blocking"
bool P_ProjectileHitPlane(AActor* mo, int part)
{
	if (part < 0)
	{
		if (mo->BlockingCeiling)
			part = SECPART_Ceiling;
		else if (mo->BlockingFloor)
			part = SECPART_Floor;
	}

	// detect 3d floor hit
	if (mo->Blocking3DFloor)
	{
		if (mo->Blocking3DFloor->health3d > 0)
		{
			P_DamageSector(mo->Blocking3DFloor, mo, mo->GetMissileDamage((mo->flags4 & MF4_STRIFEDAMAGE) ? 3 : 7, 1), mo->DamageType, SECPART_3D, mo->Pos(), false, true);
			return true;
		}
		
		return false;
	}

	if (part == SECPART_Floor && mo->Sector->healthfloor > 0 && P_CheckSectorVulnerable(mo->Sector, SECPART_Floor))
	{
		P_DamageSector(mo->Sector, mo, mo->GetMissileDamage((mo->flags4 & MF4_STRIFEDAMAGE) ? 3 : 7, 1), mo->DamageType, SECPART_Floor, mo->Pos(), false, true);
		return true;
	}
	else if (part == SECPART_Ceiling && mo->Sector->healthceiling > 0 && P_CheckSectorVulnerable(mo->Sector, SECPART_Ceiling))
	{
		P_DamageSector(mo->Sector, mo, mo->GetMissileDamage((mo->flags4 & MF4_STRIFEDAMAGE) ? 3 : 7, 1), mo->DamageType, SECPART_Ceiling, mo->Pos(), false, true);
		return true;
	}

	return false;
}

//==========================================================================
//
// P_CheckLinedefVulnerable
//
// If sectorpart is <0: returns if linedef is damageable directly
// If sectorpart is valid (SECPART_ enum): returns if sector on sidedef is 
//  damageable through this line at specified sectorpart
//==========================================================================

bool P_CheckLinedefVulnerable(line_t* line, int side, int sectorpart)
{
	if (line->special == Line_Horizon)
		return false;
	side_t* sidedef = line->sidedef[side];
	if (!sidedef)
		return false;
	return true;
}

//==========================================================================
//
// P_CheckSectorVulnerable
//
// Returns true if sector's floor or ceiling is directly damageable
//==========================================================================

bool P_CheckSectorVulnerable(sector_t* sector, int part)
{
	if (part == SECPART_3D)
		return true;
	FTextureID texture = sector->GetTexture((part == SECPART_Ceiling) ? sector_t::ceiling : sector_t::floor);
	secplane_t* plane = (part == SECPART_Ceiling) ? &sector->ceilingplane : &sector->floorplane;
	if (texture == skyflatnum)
		return false;
	return true;
}

///

FSerializer &Serialize(FSerializer &arc, const char *key, FHealthGroup& g, FHealthGroup *def)
{
	if (arc.BeginObject(key))
	{
		arc("id", g.id)
		   ("health", g.health)
			.EndObject();
	}
	return arc;
}

void P_SerializeHealthGroups(FLevelLocals *Level, FSerializer& arc)
{
	// todo : stuff
	if (arc.BeginArray("healthgroups"))
	{
		if (arc.isReading())
		{
			TArray<FHealthGroup> readGroups;
			int sz = arc.ArraySize();
			for (int i = 0; i < sz; i++)
			{
				FHealthGroup grp;
				arc(nullptr, grp);
				FHealthGroup* existinggrp = P_GetHealthGroup(Level, grp.id);
				if (!existinggrp)
					continue;
				existinggrp->health = grp.health;
			}
		}
		else
		{
			TMap<int, FHealthGroup>::ConstIterator it(Level->healthGroups);
			TMap<int, FHealthGroup>::ConstPair* pair;
			while (it.NextPair(pair))
			{
				FHealthGroup grp = pair->Value;
				arc(nullptr, grp);
			}
		}

		arc.EndArray();
	}
}

// ===================== zscript interface =====================
// 
// =============================================================

DEFINE_FIELD_X(HealthGroup, FHealthGroup, id)
DEFINE_FIELD_X(HealthGroup, FHealthGroup, health)
DEFINE_FIELD_X(HealthGroup, FHealthGroup, sectors)
DEFINE_FIELD_X(HealthGroup, FHealthGroup, lines)

DEFINE_ACTION_FUNCTION(FLevelLocals, FindHealthGroup)
{
	PARAM_SELF_STRUCT_PROLOGUE(FLevelLocals);
	PARAM_INT(id);
	FHealthGroup* grp = P_GetHealthGroup(self, id);
	ACTION_RETURN_POINTER(grp);
}

DEFINE_ACTION_FUNCTION(FHealthGroup, SetHealth)
{
	PARAM_SELF_STRUCT_PROLOGUE(FHealthGroup);
	PARAM_INT(health);
	P_SetHealthGroupHealth(self, health);
	return 0;
}

DEFINE_ACTION_FUNCTION(FDestructible, DamageSector)
{
	PARAM_PROLOGUE;
	PARAM_POINTER(sec, sector_t);
	PARAM_OBJECT(source, AActor);
	PARAM_INT(damage);
	PARAM_NAME(damagetype);
	PARAM_INT(part);
	PARAM_FLOAT(position_x);
	PARAM_FLOAT(position_y);
	PARAM_FLOAT(position_z);
	PARAM_BOOL(isradius);
	P_DamageSector(sec, source, damage, damagetype, part, DVector3(position_x, position_y, position_z), isradius, true);
	return 0;
}

DEFINE_ACTION_FUNCTION(FDestructible, DamageLinedef)
{
	PARAM_PROLOGUE;
	PARAM_POINTER(def, line_t);
	PARAM_OBJECT(source, AActor);
	PARAM_INT(damage);
	PARAM_NAME(damagetype);
	PARAM_INT(side);
	PARAM_FLOAT(position_x);
	PARAM_FLOAT(position_y);
	PARAM_FLOAT(position_z);
	PARAM_BOOL(isradius);
	P_DamageLinedef(def, source, damage, damagetype, side, DVector3(position_x, position_y, position_z), isradius, true);
	return 0;
}

DEFINE_ACTION_FUNCTION(FDestructible, GeometryLineAttack)
{
	PARAM_PROLOGUE;
	PARAM_POINTER(trace, FTraceResults);
	PARAM_OBJECT(thing, AActor);
	PARAM_INT(damage);
	PARAM_NAME(damagetype);
	P_GeometryLineAttack(*trace, thing, damage, damagetype);
	return 0;
}

DEFINE_ACTION_FUNCTION(FDestructible, GeometryRadiusAttack)
{
	PARAM_PROLOGUE;
	PARAM_OBJECT(bombspot, AActor);
	PARAM_OBJECT(bombsource, AActor);
	PARAM_INT(bombdamage);
	PARAM_FLOAT(bombdistance);
	PARAM_NAME(damagetype);
	PARAM_FLOAT(fulldamagedistance);
	P_GeometryRadiusAttack(bombspot, bombsource, bombdamage, bombdistance, damagetype, fulldamagedistance);
	return 0;
}

DEFINE_ACTION_FUNCTION(FDestructible, ProjectileHitLinedef)
{
	PARAM_PROLOGUE;
	PARAM_OBJECT(projectile, AActor);
	PARAM_POINTER(def, line_t);
	ACTION_RETURN_BOOL(P_ProjectileHitLinedef(projectile, def));
}

DEFINE_ACTION_FUNCTION(FDestructible, ProjectileHitPlane)
{
	PARAM_PROLOGUE;
	PARAM_OBJECT(projectile, AActor);
	PARAM_INT(part);
	ACTION_RETURN_BOOL(P_ProjectileHitPlane(projectile, part));
}

DEFINE_ACTION_FUNCTION(FDestructible, CheckLinedefVulnerable)
{
	PARAM_PROLOGUE;
	PARAM_POINTER(def, line_t);
	PARAM_INT(side);
	PARAM_INT(part);
	ACTION_RETURN_BOOL(P_CheckLinedefVulnerable(def, side, part));
}

DEFINE_ACTION_FUNCTION(FDestructible, CheckSectorVulnerable)
{
	PARAM_PROLOGUE;
	PARAM_POINTER(sec, sector_t);
	PARAM_INT(part);
	ACTION_RETURN_BOOL(P_CheckSectorVulnerable(sec, part));
}

DEFINE_ACTION_FUNCTION(_Line, GetHealth)
{
	PARAM_SELF_STRUCT_PROLOGUE(line_t);
	if (self->healthgroup)
	{
		FHealthGroup* grp = P_GetHealthGroup(self->GetLevel(), self->healthgroup);
		if (grp) ACTION_RETURN_INT(grp->health);
	}

	ACTION_RETURN_INT(self->health);
}

DEFINE_ACTION_FUNCTION(_Line, SetHealth)
{
	PARAM_SELF_STRUCT_PROLOGUE(line_t);
	PARAM_INT(newhealth);

	if (newhealth < 0)
		newhealth = 0;

	self->health = newhealth;
	if (self->healthgroup)
	{
		FHealthGroup* grp = P_GetHealthGroup(self->GetLevel(), self->healthgroup);
		if (grp) P_SetHealthGroupHealth(grp, newhealth);
	}

	return 0;
}

DEFINE_ACTION_FUNCTION(_Sector, GetHealth)
{
	PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	PARAM_INT(part);

	FHealthGroup* grp;
	auto Level = self->Level;
	switch (part)
	{
	case SECPART_Floor:
		ACTION_RETURN_INT((self->healthfloorgroup && (grp = P_GetHealthGroup(Level, self->healthfloorgroup))) ? grp->health : self->healthfloor);
	case SECPART_Ceiling:
		ACTION_RETURN_INT((self->healthceilinggroup && (grp = P_GetHealthGroup(Level, self->healthceilinggroup))) ? grp->health : self->healthceiling);
	case SECPART_3D:
		ACTION_RETURN_INT((self->health3dgroup && (grp = P_GetHealthGroup(Level, self->health3dgroup))) ? grp->health : self->health3d);
	default:
		ACTION_RETURN_INT(0);
	}
}

DEFINE_ACTION_FUNCTION(_Sector, SetHealth)
{
	PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	PARAM_INT(part);
	PARAM_INT(newhealth);

	if (newhealth < 0)
		newhealth = 0;

	int group;
	int* health;
	switch (part)
	{
	case SECPART_Floor:
		group = self->healthfloorgroup;
		health = &self->healthfloor;
		break;
	case SECPART_Ceiling:
		group = self->healthceilinggroup;
		health = &self->healthceiling;
		break;
	case SECPART_3D:
		group = self->health3dgroup;
		health = &self->health3d;
		break;
	default:
		return 0;
	}

	FHealthGroup* grp = group ? P_GetHealthGroup(self->Level, group) : nullptr;
	*health = newhealth;
	if (grp) P_SetHealthGroupHealth(grp, newhealth);
	return 0;
}
