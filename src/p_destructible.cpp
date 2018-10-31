#include "p_spec.h"
#include "g_levellocals.h"
#include "p_destructible.h"
#include "v_text.h"
#include "actor.h"
#include "p_trace.h"
#include "p_lnspec.h"
#include "r_sky.h"
#include "p_local.h"
#include "p_maputl.h"
#include "c_cvars.h"
#include "serializer.h"

//==========================================================================
//
// [ZZ] Geometry damage logic callbacks
//
//==========================================================================
void P_SetHealthGroupHealth(int group, int health)
{
	FHealthGroup* grp = P_GetHealthGroup(group);
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
		if (lsector->healthceilinggroup == group)
			lsector->healthceiling = health;
		if (lsector->healthfloorgroup == group)
			lsector->healthfloor = health;
	}
}

void P_DamageHealthGroup(FHealthGroup* grp, void* object, AActor* source, int damage, FName damagetype, int side, int part, DVector3 position)
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
		P_DamageLinedef(lline, source, damage, damagetype, side, position, false);
	}
	//
	for (unsigned i = 0; i < grp->sectors.Size(); i++)
	{
		sector_t* lsector = grp->sectors[i];
		
		if (lsector->healthceilinggroup == group && (lsector != object || part != SECPART_Ceiling))
		{
			lsector->healthceiling = grp->health + damage;
			P_DamageSector(lsector, source, damage, damagetype, SECPART_Ceiling, position, false);
		}
		
		if (lsector->healthfloorgroup == group && (lsector != object || part != SECPART_Floor))
		{
			lsector->healthfloor = grp->health + damage;
			P_DamageSector(lsector, source, damage, damagetype, SECPART_Floor, position, false);
		}
	}
}

void P_DamageLinedef(line_t* line, AActor* source, int damage, FName damagetype, int side, DVector3 position, bool dogroups)
{
	line->health -= damage;
	if (line->health < 0) line->health = 0;

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
		FHealthGroup* grp = P_GetHealthGroup(line->healthgroup);
		if (grp)
			grp->health = line->health;
		P_DamageHealthGroup(grp, line, source, damage, damagetype, side, -1, position);
	}

	//Printf("P_DamageLinedef: %d damage (type=%s, source=%p), new health = %d\n", damage, damagetype.GetChars(), source, line->health);
}

void P_DamageSector(sector_t* sector, AActor* source, int damage, FName damagetype, int part, DVector3 position, bool dogroups)
{
	int sectorhealth = (part == SECPART_Ceiling) ? sector->healthceiling : sector->healthfloor;
	int newhealth = sectorhealth - damage;
	if (newhealth < 0) newhealth = 0;
	if (part == SECPART_Ceiling)
		sector->healthceiling = newhealth;
	else sector->healthfloor = newhealth;

	// callbacks here
	int dmg = (part == SECPART_Ceiling) ? SECSPAC_DamageCeiling : SECSPAC_DamageFloor;
	int dth = (part == SECPART_Ceiling) ? SECSPAC_DeathCeiling : SECSPAC_DeathFloor;
	if (sector->SecActTarget)
	{
		sector->TriggerSectorActions(source, dmg);
		if (newhealth == 0)
			sector->TriggerSectorActions(source, dth);
	}

	int group = (part == SECPART_Ceiling) ? sector->healthceilinggroup : sector->healthfloorgroup;
	if (dogroups && group)
	{
		FHealthGroup* grp = P_GetHealthGroup(group);
		if (grp)
			grp->health = newhealth;
		P_DamageHealthGroup(grp, sector, source, damage, damagetype, 0, part, position);
	}

	//Printf("P_DamageSector: %d damage (type=%s, position=%s, source=%p), new health = %d\n", damage, damagetype.GetChars(), (part == SECPART_Ceiling) ? "ceiling" : "floor", source, newhealth);
}


//===========================================================================
//
//
//
//===========================================================================

FHealthGroup* P_GetHealthGroup(int id)
{
	FHealthGroup* grp = level.healthGroups.CheckKey(id);
	return grp;
}

FHealthGroup* P_GetHealthGroupOrNew(int id, int health)
{
	FHealthGroup* grp = level.healthGroups.CheckKey(id);
	if (!grp)
	{
		FHealthGroup newgrp;
		newgrp.id = id;
		newgrp.health = health;
		grp = &(level.healthGroups.Insert(id, newgrp));
	}
	return grp;
}

void P_InitHealthGroups()
{
	level.healthGroups.Clear();
	TArray<int> groupsInError;
	for (unsigned i = 0; i < level.lines.Size(); i++)
	{
		line_t* lline = &level.lines[i];
		if (lline->healthgroup > 0)
		{
			FHealthGroup* grp = P_GetHealthGroupOrNew(lline->healthgroup, lline->health);
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
	for (unsigned i = 0; i < level.sectors.Size(); i++)
	{
		sector_t* lsector = &level.sectors[i];
		if (lsector->healthceilinggroup > 0)
		{
			FHealthGroup* grp = P_GetHealthGroupOrNew(lsector->healthceilinggroup, lsector->healthceiling);
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
			FHealthGroup* grp = P_GetHealthGroupOrNew(lsector->healthfloorgroup, lsector->healthfloor);
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
		FHealthGroup* grp = P_GetHealthGroup(groupsInError[i]);
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
				P_DamageSector(backsector, thing, damage, damageType, (trace.Tier == TIER_Upper) ? SECPART_Ceiling : SECPART_Floor, trace.HitPos);
			}
		}
		// always process linedef health if any
		if (trace.Line->health > 0)
		{
			P_DamageLinedef(trace.Line, thing, damage, damageType, trace.Side, trace.HitPos);
		}
		// fake floors are not handled
	}
	else if (trace.HitType == TRACE_HitFloor || trace.HitType == TRACE_HitCeiling)
	{
		int sectorhealth = 0;
		if (trace.HitType == TRACE_HitFloor && trace.Sector->healthfloor > 0 && P_CheckSectorVulnerable(trace.Sector, SECPART_Floor))
			sectorhealth = trace.Sector->healthfloor;
		if (trace.HitType == TRACE_HitCeiling && trace.Sector->healthceiling > 0 && P_CheckSectorVulnerable(trace.Sector, SECPART_Ceiling))
			sectorhealth = trace.Sector->healthceiling;
		if (sectorhealth > 0)
		{
			P_DamageSector(trace.Sector, thing, damage, damageType, (trace.HitType == TRACE_HitCeiling) ? SECPART_Ceiling : SECPART_Floor, trace.HitPos);
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

static void PGRA_InsertIfCloser(TMap<int, pgra_data_t>& damageGroupPos, int group, DVector3 pt, DVector3 check, sector_t* checksector, sector_t* sector, line_t* line, int secpart)
{
	// simple solid geometry sight check between "check" and "pt"
	// expected - Trace hits nothing
	DVector3 ptVec = (pt - check);
	double ptDst = ptVec.Length() - 0.5;
	ptVec.MakeUnit();
	FTraceResults res;
	bool isblocked = Trace(check, checksector, ptVec, ptDst, 0, 0xFFFFFFFF, nullptr, res);
	if (isblocked) return;

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

void P_GeometryRadiusAttack(AActor* bombspot, AActor* bombsource, int bombdamage, int bombdistance, FName damagetype, int fulldamagedistance)
{
	TMap<int, pgra_data_t> damageGroupPos;

	double bombdistancefloat = 1. / (double)(bombdistance - fulldamagedistance);

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
			grp = 0x80000000 | (srcsector->sectornum & 0x7FFFFFFF);
		PGRA_InsertIfCloser(damageGroupPos, grp, spotTo, bombspot->Pos(), srcsector, srcsector, nullptr, SECPART_Ceiling);
	}

	if (srcsector->healthfloor > 0)
	{
		double dstfloor = srcsector->floorplane.Normal() | (bombspot->Pos() + srcsector->floorplane.Normal()*srcsector->floorplane.D);
		DVector3 spotTo = bombspot->Pos() - srcsector->floorplane.Normal() * dstfloor;
		int grp = srcsector->healthfloorgroup;
		if (grp <= 0)
			grp = 0x40000000 | (srcsector->sectornum & 0x7FFFFFFF);
		PGRA_InsertIfCloser(damageGroupPos, grp, spotTo, bombspot->Pos(), srcsector, srcsector, nullptr, SECPART_Floor);
	}

	// enumerate all lines around
	FBoundingBox bombbox(bombspot->X(), bombspot->Y(), bombdistance * 16);
	FBlockLinesIterator it(bombbox);
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
		if (!ln->health && (!othersector || (!othersector->healthfloor && !othersector->healthceiling)))
			continue; // non-interactive geometry

		DVector2 to2d = PGRA_ClosestPointOnLine2D(bombspot->Pos().XY(), side->V1()->p, side->V2()->p);
		// this gives us x/y
		double distto2d = (to2d - pos2d).Length();
		double z_top1, z_top2, z_bottom1, z_bottom2; // here, z_top1 is closest to the ceiling, and z_bottom1 is closest to the floor.
		z_top1 = sector->ceilingplane.ZatPoint(to2d);
		z_bottom1 = sector->floorplane.ZatPoint(to2d);
		DVector3 to3d_fullheight(to2d.X, to2d.Y, clamp(bombspot->Z(), z_bottom1, z_top1));

		if (ln->health && P_CheckLinedefVulnerable(ln, sd))
		{
			bool cantdamage = false;
			bool linefullheight = othersector && !!(ln->flags & (ML_BLOCKEVERYTHING));
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
				else
				{
					// otherwise just damage line
					double dst = (to3d_fullheight - bombspot->Pos()).Length();
					int damage = 0;
					if (dst < bombdistance)
					{
						dst = clamp<double>(dst - (double)fulldamagedistance, 0, dst);
						damage = (int)((double)bombdamage * (1. - dst * bombdistancefloat));
						if (bombsource == bombspot)
							damage = (int)(damage * splashfactor);
					}
					P_DamageLinedef(ln, bombsource, damage, damagetype, sd, to3d_fullheight);
				}
			}
		}

		if (othersector && othersector->healthceiling && P_CheckLinedefVulnerable(ln, sd, SECPART_Ceiling))
		{
			z_top2 = othersector->ceilingplane.ZatPoint(to2d);
			if (z_top2 < z_top1) // we have front side to hit against
			{
				DVector3 to3d_upper(to2d.X, to2d.Y, clamp(bombspot->Z(), z_top2, z_top1));
				int grp = othersector->healthceilinggroup;
				if (grp <= 0)
					grp = 0x80000000 | (othersector->sectornum & 0x7FFFFFFF);
				PGRA_InsertIfCloser(damageGroupPos, grp, to3d_upper, bombspot->Pos(), srcsector, othersector, nullptr, SECPART_Ceiling);
			}
		}

		if (othersector && othersector->healthfloor && P_CheckLinedefVulnerable(ln, sd, SECPART_Floor))
		{
			z_bottom2 = othersector->floorplane.ZatPoint(to2d);
			if (z_bottom2 > z_bottom1) // we have front side to hit against
			{
				DVector3 to3d_lower(to2d.X, to2d.Y, clamp(bombspot->Z(), z_bottom1, z_bottom2));
				int grp = othersector->healthfloorgroup;
				if (grp <= 0)
					grp = 0x40000000 | (othersector->sectornum & 0x7FFFFFFF);
				PGRA_InsertIfCloser(damageGroupPos, grp, to3d_lower, bombspot->Pos(), srcsector, othersector, nullptr, SECPART_Floor);
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
			dst = clamp<double>(dst - (double)fulldamagedistance, 0, dst);
			damage = (int)((double)bombdamage * (1. - dst * bombdistancefloat));
			if (bombsource == bombspot)
				damage = (int)(damage * splashfactor);
		}

		// for debug
		//P_SpawnParticle(damageGroupPair->Value.pos, DVector3(), DVector3(), PalEntry(0xFF, 0x00, 0x00), 1.0, 35, 5.0, 0.0, 0.0, 0);

		int grp = damageGroupPair->Key;
		if (grp & 0x80000000) // sector ceiling
		{
			assert(damageGroupPair->Value.sector != nullptr);
			P_DamageSector(damageGroupPair->Value.sector, bombsource, damage, damagetype, SECPART_Ceiling, pos);
		}
		else if (grp & 0x40000000) // sector floor
		{
			assert(damageGroupPair->Value.sector != nullptr);
			P_DamageSector(damageGroupPair->Value.sector, bombsource, damage, damagetype, SECPART_Floor, pos);
		}
		else
		{
			assert((damageGroupPair->Value.sector != nullptr) != (damageGroupPair->Value.line != nullptr));
			if (damageGroupPair->Value.line != nullptr)
				P_DamageLinedef(damageGroupPair->Value.line, bombsource, damage, damagetype, P_PointOnLineSide(pos.XY(), damageGroupPair->Value.line), pos);
			else P_DamageSector(damageGroupPair->Value.sector, bombsource, damage, damagetype, damageGroupPair->Value.secpart, pos);
		}
	}
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

void P_SerializeHealthGroups(FSerializer& arc)
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
				FHealthGroup* existinggrp = P_GetHealthGroup(grp.id);
				if (!existinggrp)
					continue;
				existinggrp->health = grp.health;
			}
		}
		else
		{
			TMap<int, FHealthGroup>::ConstIterator it(level.healthGroups);
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