//-----------------------------------------------------------------------------
//
// Copyright 1993-1996 id Software
// Copyright 1994-1996 Raven Software
// Copyright 1998-1998 Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman
// Copyright 1999-2016 Randy Heit
// Copyright 2002-2016 Christoph Oelckers
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/
//
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//		Implements special effects:
//		Texture animation, height or lighting changes
//		 according to adjacent sectors, respective
//		 utility functions, etc.
//		Line Tag handling. Line and Sector triggers.
//		Implements donut linedef triggers
//		Initializes and implements BOOM linedef triggers for
//			Scrollers/Conveyors
//			Friction
//			Wind/Current
//
//-----------------------------------------------------------------------------

/* For code that originates from ZDoom the following applies:
**
**---------------------------------------------------------------------------
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

#include <stdlib.h>


#include "doomdef.h"
#include "doomstat.h"
#include "d_event.h"
#include "g_level.h"
#include "gstrings.h"
#include "events.h"

#include "m_random.h"

#include "p_local.h"
#include "p_spec.h"
#include "p_blockmap.h"
#include "p_lnspec.h"
#include "p_terrain.h"
#include "p_acs.h"
#include "p_3dmidtex.h"

#include "g_game.h"

#include "a_sharedglobal.h"
#include "a_keys.h"
#include "c_dispatch.h"
#include "r_sky.h"
#include "d_player.h"
#include "g_levellocals.h"
#include "actorinlines.h"
#include "vm.h"
#include "p_setup.h"

#include "c_console.h"
#include "p_spec_thinkers.h"
#include "maploader/maploader.h"

EXTERN_CVAR(Bool, forcewater)

//-----------------------------------------------------------------------------
//
// Portals
//
//-----------------------------------------------------------------------------

//============================================================================
//
// Spawns a single line portal
//
//============================================================================

void MapLoader::SpawnLinePortal(line_t* line)
{
	// portal destination is special argument #0
	line_t* dst = nullptr;

	if ((line->args[2] >= PORTT_VISUAL && line->args[2] <= PORTT_LINKED) || line->special == Line_QuickPortal)
	{
		int type = (line->special != Line_QuickPortal) ? line->args[2] : line->args[0] == 0 ? PORTT_LINKED : PORTT_VISUAL;
		int tag = (line->special == Line_QuickPortal) ? Level->tagManager.GetFirstLineID(line) : line->args[0];
		dst = Level->FindPortalDestination(line, tag, line->special == Line_QuickPortal? Line_QuickPortal : -1);

		line->portalindex = Level->linePortals.Reserve(1);
		FLinePortal *port = &Level->linePortals.Last();

		port->mOrigin = line;
		port->mDestination = dst;
		port->mType = uint8_t(type);	// range check is done above.

		if (port->mType == PORTT_LINKED)
		{
			// Linked portals have no z-offset ever.
			port->mAlign = PORG_ABSOLUTE;
		}
		else
		{
			int flags = (line->special == Line_QuickPortal) ? PORG_ABSOLUTE : line->args[3];
			port->mAlign = uint8_t(flags >= PORG_ABSOLUTE && flags <= PORG_CEILING ? flags : PORG_ABSOLUTE);
			if (port->mType == PORTT_INTERACTIVE && port->mAlign != PORG_ABSOLUTE)
			{
				// Due to the way z is often handled, these pose a major issue for parts of the code that needs to transparently handle interactive portals.
				Printf(TEXTCOLOR_RED "Warning: z-offsetting not allowed for interactive portals. Changing line %d to teleport-portal!\n", line->Index());
				port->mType = PORTT_TELEPORT;
			}
		}
		port->mDefFlags = port->mType == PORTT_VISUAL ? PORTF_VISIBLE :
			port->mType == PORTT_TELEPORT ? PORTF_TYPETELEPORT :
			PORTF_TYPEINTERACTIVE;
	}
	else if (line->args[2] == PORTT_LINKEDEE && line->args[0] == 0)
	{
		// EE-style portals require that the first line ID is identical and the first arg of the two linked linedefs are 0 and 1 respectively.

		int mytag = Level->GetFirstLineId(line);

		for (auto &ln : Level->lines)
		{
			if (Level->GetFirstLineId(&ln) == mytag && ln.args[0] == 1 && ln.special == Line_SetPortal)
			{
				line->portalindex = Level->linePortals.Reserve(1);
				FLinePortal *port = &Level->linePortals.Last();

				memset(port, 0, sizeof(FLinePortal));
				port->mOrigin = line;
				port->mDestination = &ln;
				port->mType = PORTT_LINKED;
				port->mAlign = PORG_ABSOLUTE;
				port->mDefFlags = PORTF_TYPEINTERACTIVE;

				// we need to create the backlink here, too.
				ln.portalindex = Level->linePortals.Reserve(1);
				port = &Level->linePortals.Last();

				memset(port, 0, sizeof(FLinePortal));
				port->mOrigin = &ln;
				port->mDestination = line;
				port->mType = PORTT_LINKED;
				port->mAlign = PORG_ABSOLUTE;
				port->mDefFlags = PORTF_TYPEINTERACTIVE;
			}
		}
	}
	else
	{
		// undefined type
		return;
	}
}

//-----------------------------------------------------------------------------
//
// Lower stacks go in the bottom sector.
//
//-----------------------------------------------------------------------------

void MapLoader::SetupFloorPortal (AActor *point)
{
	auto it = Level->GetActorIterator(NAME_LowerStackLookOnly, point->tid);
	sector_t *Sector = point->Sector;
	auto skyv = it.Next();
	if (skyv != nullptr)
	{
		skyv->target = point;
		if (Sector->GetAlpha(sector_t::floor) == 1.)
			Sector->SetAlpha(sector_t::floor, clamp(point->args[0], 0, 255) / 255.);

		Sector->Portals[sector_t::floor] = Level->GetStackPortal(skyv, sector_t::floor);
	}
}

//-----------------------------------------------------------------------------
//
// Upper stacks go in the top sector.
//
//-----------------------------------------------------------------------------

void MapLoader::SetupCeilingPortal (AActor *point)
{
	auto it = Level->GetActorIterator(NAME_UpperStackLookOnly, point->tid);
	sector_t *Sector = point->Sector;
	auto skyv = it.Next();
	if (skyv != nullptr)
	{
		skyv->target = point;
		if (Sector->GetAlpha(sector_t::ceiling) == 1.)
			Sector->SetAlpha(sector_t::ceiling, clamp(point->args[0], 0, 255) / 255.);

		Sector->Portals[sector_t::ceiling] = Level->GetStackPortal(skyv, sector_t::ceiling);
	}
}

//-----------------------------------------------------------------------------
//
// 
//
//-----------------------------------------------------------------------------

void MapLoader::SetupPortals()
{
	auto it = Level->GetThinkerIterator<AActor>(NAME_StackPoint);
	AActor *pt;
	TArray<AActor *> points;

	while ((pt = it.Next()))
	{
		FName nm = pt->GetClass()->TypeName;
		if (nm == NAME_UpperStackLookOnly)
		{
			SetupFloorPortal(pt);
		}
		else if (nm == NAME_LowerStackLookOnly)
		{
			SetupCeilingPortal(pt);
		}
		pt->special1 = 0;
		points.Push(pt);
	}
	// the semantics here are incredibly lax so the final setup can only be done once all portals have been created,
	// because later stackpoints will happily overwrite info in older ones, if there are multiple links.
	for (auto &s : Level->sectorPortals)
	{
		if (s.mType == PORTS_STACKEDSECTORTHING && s.mSkybox)
		{
			for (auto &ss : Level->sectorPortals)
			{
				if (ss.mType == PORTS_STACKEDSECTORTHING && ss.mSkybox == s.mSkybox->target)
				{
					s.mPartner = unsigned((&ss) - &Level->sectorPortals[0]);
				}
			}
		}
	}
	// Now we can finally set the displacement and delete the stackpoint reference.
	for (auto &s : Level->sectorPortals)
	{
		if (s.mType == PORTS_STACKEDSECTORTHING && s.mSkybox)
		{
			s.mDisplacement = s.mSkybox->Pos() - s.mSkybox->target->Pos();
			s.mSkybox = nullptr;
		}
	}
}

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

void MapLoader::SetPortal(sector_t *sector, int plane, unsigned pnum, double alpha)
{
	// plane: 0=floor, 1=ceiling, 2=both
	if (plane > 0)
	{
		if (sector->GetPortalType(sector_t::ceiling) == PORTS_SKYVIEWPOINT)
		{
			sector->Portals[sector_t::ceiling] = pnum;
			if (sector->GetAlpha(sector_t::ceiling) == 1.)
				sector->SetAlpha(sector_t::ceiling, alpha);

			if (Level->sectorPortals[pnum].mFlags & PORTSF_SKYFLATONLY)
				sector->SetTexture(sector_t::ceiling, skyflatnum);
		}
	}
	if (plane == 2 || plane == 0)
	{
		if (sector->GetPortalType(sector_t::floor) == PORTS_SKYVIEWPOINT)
		{
			sector->Portals[sector_t::floor] = pnum;
		}
		if (sector->GetAlpha(sector_t::floor) == 1.)
			sector->SetAlpha(sector_t::floor, alpha);

		if (Level->sectorPortals[pnum].mFlags & PORTSF_SKYFLATONLY)
			sector->SetTexture(sector_t::floor, skyflatnum);
	}
}

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

void MapLoader::CopyPortal(int sectortag, int plane, unsigned pnum, double alpha, bool tolines)
{
	int s;
	auto itr = Level->GetSectorTagIterator(sectortag);
	while ((s = itr.Next()) >= 0)
	{
		SetPortal(&Level->sectors[s], plane, pnum, alpha);
	}

	for (auto &line : Level->lines)
	{
		// Check if this portal needs to be copied to other sectors
		// This must be done here to ensure that it gets done only after the portal is set up
		if (line.special == Sector_SetPortal &&
			line.args[1] == 1 &&
			(line.args[2] == plane || line.args[2] == 3) &&
			line.args[3] == sectortag)
		{
			if (line.args[0] == 0)
			{
				SetPortal(line.frontsector, plane, pnum, alpha);
			}
			else
			{
				auto itr = Level->GetSectorTagIterator(line.args[0]);
				while ((s = itr.Next()) >= 0)
				{
					SetPortal(&Level->sectors[s], plane, pnum, alpha);
				}
			}
		}
		if (tolines && line.special == Sector_SetPortal &&
			line.args[1] == 5 &&
			line.args[3] == sectortag)
		{
			if (line.args[0] == 0)
			{
				line.portaltransferred = pnum;
			}
			else
			{
				auto itr = Level->GetLineIdIterator(line.args[0]);
				while ((s = itr.Next()) >= 0)
				{
					Level->lines[s].portaltransferred = pnum;
				}
			}
		}
	}
}

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

void MapLoader::SpawnPortal(line_t *line, int sectortag, int plane, int bytealpha, int linked)
{
	if (plane < 0 || plane > 2 || (linked && plane == 2)) return;
	for (auto &oline : Level->lines)
	{
		// We must look for the reference line with a linear search unless we want to waste the line ID for it
		// which is not a good idea.
		if (oline.special == Sector_SetPortal &&
			oline.args[0] == sectortag &&
			oline.args[1] == linked &&
			oline.args[2] == plane &&
			oline.args[3] == 1)
		{
			// beware of overflows.
			DVector2 pos1 = line->v1->fPos() + line->Delta() / 2;
			DVector2 pos2 = oline.v1->fPos() + oline.Delta() / 2;
			unsigned pnum = Level->GetPortal(linked ? PORTS_LINKEDPORTAL : PORTS_PORTAL, plane, line->frontsector, oline.frontsector, pos2 - pos1);
			CopyPortal(sectortag, plane, pnum, bytealpha / 255., false);
			return;
		}
	}
}

//-----------------------------------------------------------------------------
//
// This searches the viewpoint's sector
// for a skybox line special, gets its tag and transfers the skybox to all tagged sectors.
//
//-----------------------------------------------------------------------------

void MapLoader::SpawnSkybox(AActor *origin)
{
	sector_t *Sector = origin->Sector;
	if (Sector == NULL)
	{
		Printf("Sector not initialized for SkyCamCompat\n");
		origin->Sector = Sector = Level->PointInSector(origin->Pos());
	}
	if (Sector)
	{
		for(auto refline : Sector->Lines)
		{
			if (refline->special == Sector_SetPortal && refline->args[1] == 2)
			{
				// We found the setup linedef for this skybox, so let's use it for our init.
				unsigned pnum = Level->GetSkyboxPortal(origin);
				CopyPortal(refline->args[0], refline->args[2], pnum, 0, true);
				return;
			}
		}
	}
}

//-----------------------------------------------------------------------------
//
// P_SetSectorDamage
//
// Sets damage properties for one sector. Allows combination of original specials with explicit use of the damage properties
//
//-----------------------------------------------------------------------------

static void SetupSectorDamage(sector_t *sector, int damage, int interval, int leakchance, FName type, int flags)
{
	// Only set if damage is not yet initialized. This ensures that UDMF takes precedence over sector specials.
	if (sector->damageamount == 0 && !(sector->Flags & (SECF_EXIT1|SECF_EXIT2)))
	{
		sector->damageamount = damage;
		sector->damageinterval = max(1, interval);
		sector->leakydamage = leakchance;
		sector->damagetype = type;
		sector->Flags = (sector->Flags & ~SECF_DAMAGEFLAGS) | (flags & SECF_DAMAGEFLAGS);
	}
}

//-----------------------------------------------------------------------------
//
// P_InitSectorSpecial
//
// Sets up everything derived from 'sector->special' for one sector
// ('fromload' is necessary to allow conversion upon savegame load.)
//
//-----------------------------------------------------------------------------

void MapLoader::InitSectorSpecial(sector_t *sector, int special)
{
	// [RH] All secret sectors are marked with a BOOM-ish bitfield
	if (sector->special & SECRET_MASK)
	{
		sector->Flags |= SECF_SECRET | SECF_WASSECRET;
		Level->total_secrets++;
	}
	if (sector->special & FRICTION_MASK)
	{
		sector->Flags |= SECF_FRICTION;
	}
	if (sector->special & PUSH_MASK)
	{
		sector->Flags |= SECF_PUSH;
	}
	// Nom MBF21 compatibility needs to be checked here, because after this point there is no longer any context in which it can be done.
	if ((sector->special & KILL_MONSTERS_MASK) && Level->MBF21Enabled())
	{
		sector->Flags |= SECF_KILLMONSTERS;
	}
	if (!(sector->special & DEATH_MASK) || !Level->MBF21Enabled())
	{
		if ((sector->special & DAMAGE_MASK) == 0x100)
		{
			SetupSectorDamage(sector, 5, 32, 0, NAME_Fire, 0);
		}
		else if ((sector->special & DAMAGE_MASK) == 0x200)
		{
			SetupSectorDamage(sector, 10, 32, 0, NAME_Slime, 0);
		}
		else if ((sector->special & DAMAGE_MASK) == 0x300)
		{
			SetupSectorDamage(sector, 20, 32, 5, NAME_Slime, 0);
		}
	}
	else
	{
		if ((sector->special & DAMAGE_MASK) == 0x100)
		{
			SetupSectorDamage(sector, TELEFRAG_DAMAGE, 0, 0, NAME_InstantDeath, 0);
		}
		else if ((sector->special & DAMAGE_MASK) == 0x200)
		{
			sector->Flags |= SECF_EXIT1;
		}
		else if ((sector->special & DAMAGE_MASK) == 0x300)
		{
			sector->Flags |= SECF_EXIT2;
		}
		else // 0
		{
			SetupSectorDamage(sector, TELEFRAG_DAMAGE-1, 0, 0, NAME_InstantDeath, 0);
		}
	}
	sector->special &= 0xff;

	// [RH] Normal DOOM special or BOOM specialized?
	bool keepspecial = false;
	SpawnLights(sector);
	switch (sector->special)
	{
	case dLight_Strobe_Hurt:
		SetupSectorDamage(sector, 20, 32, 5, NAME_Slime, 0);
		break;

	case dDamage_Hellslime:
		SetupSectorDamage(sector, 10, 32, 0, NAME_Slime, 0);
		break;

	case dDamage_Nukage:
		SetupSectorDamage(sector, 5, 32, 0, NAME_Slime, 0);
		break;

	case dSector_DoorCloseIn30:
		Level->CreateThinker<DDoor>(sector, DDoor::doorWaitClose, 2, 0, 0, 30 * TICRATE);
		break;
			
	case dDamage_End:
		SetupSectorDamage(sector, 20, 32, 256, NAME_None, SECF_ENDGODMODE|SECF_ENDLEVEL);
		break;

	case dSector_DoorRaiseIn5Mins:
		Level->CreateThinker<DDoor> (sector, DDoor::doorWaitRaise, 2, TICRATE*30/7, 0, 5*60*TICRATE);
		break;

	case dFriction_Low:
		sector->friction = FRICTION_LOW;
		sector->movefactor = 0x269/65536.;
		sector->Flags |= SECF_FRICTION;
		break;

	case dDamage_SuperHellslime:
		SetupSectorDamage(sector, 20, 32, 5, NAME_Slime, 0);
		break;

	case dDamage_LavaWimpy:
		SetupSectorDamage(sector, 5, 16, 256, NAME_Fire, SECF_DMGTERRAINFX);
		break;

	case dDamage_LavaHefty:
		SetupSectorDamage(sector, 8, 16, 256, NAME_Fire, SECF_DMGTERRAINFX);
		break;

	case dScroll_EastLavaDamage:
		SetupSectorDamage(sector, 5, 16, 256, NAME_Fire, SECF_DMGTERRAINFX);
		CreateScroller(EScroll::sc_floor, -4., 0, sector, 0);
		keepspecial = true;
		break;

	case hDamage_Sludge:
		SetupSectorDamage(sector, 4, 32, 0, NAME_Slime, 0);
		break;

	case sLight_Strobe_Hurt:
		SetupSectorDamage(sector, 5, 32, 0, NAME_Slime, 0);
		break;

	case sDamage_Hellslime:
		SetupSectorDamage(sector, 2, 32, 0, NAME_Slime, SECF_HAZARD);
		break;

	case Damage_InstantDeath:
		// Strife's instant death sector
		SetupSectorDamage(sector, TELEFRAG_DAMAGE, 1, 256, NAME_InstantDeath, 0);
		break;

	case sDamage_SuperHellslime:
		SetupSectorDamage(sector, 4, 32, 0, NAME_Slime, SECF_HAZARD);
		break;

	case Sector_Hidden:
		sector->MoreFlags |= SECMF_HIDDEN;
		break;

	case Sector_Heal:
		// CoD's healing sector
		SetupSectorDamage(sector, -1, 32, 0, NAME_None, 0);
		break;

	case Sky2:
		sector->sky = PL_SKYFLAT;
		break;

	default:
		if (sector->special >= Scroll_North_Slow && sector->special <= Scroll_SouthWest_Fast)
		{ // Hexen scroll special
			static const int8_t hexenScrollies[24][2] =
			{
				{  0,  1 }, {  0,  2 }, {  0,  4 },
				{ -1,  0 }, { -2,  0 }, { -4,  0 },
				{  0, -1 }, {  0, -2 }, {  0, -4 },
				{  1,  0 }, {  2,  0 }, {  4,  0 },
				{  1,  1 }, {  2,  2 }, {  4,  4 },
				{ -1,  1 }, { -2,  2 }, { -4,  4 },
				{ -1, -1 }, { -2, -2 }, { -4, -4 },
				{  1, -1 }, {  2, -2 }, {  4, -4 }
			};

			
			int i = sector->special - Scroll_North_Slow;
			double dx = hexenScrollies[i][0] / 2.;
			double dy = hexenScrollies[i][1] / 2.;
			CreateScroller(EScroll::sc_floor, dx, dy, sector, 0);
		}
		else if (sector->special >= Carry_East5 && sector->special <= Carry_East35)
		{ 
			// Heretic scroll special
			// Only east scrollers also scroll the texture
			CreateScroller(EScroll::sc_floor,	-0.5 * (1 << ((sector->special & 0xff) - Carry_East5)),	0, sector, 0);
		}
		keepspecial = true;
		break;
	}
	if (!keepspecial) sector->special = 0;
}

//-----------------------------------------------------------------------------
//
// P_SpawnSpecials
//
// After the map has been loaded, scan for specials that spawn thinkers
//
//-----------------------------------------------------------------------------

void MapLoader::SpawnSpecials ()
{
	SetupPortals();

	for (auto &sec : Level->sectors)
	{
		if (sec.special == 0)
			continue;

		InitSectorSpecial(&sec, sec.special);
	}

	ProcessEDSectors();
	
	// Init other misc stuff

	SpawnScrollers(); // killough 3/7/98: Add generalized scrollers
	SpawnFriction();	// phares 3/12/98: New friction model using linedefs
	SpawnPushers();	// phares 3/20/98: New pusher model using linedefs

	auto it2 = Level->GetThinkerIterator<AActor>(NAME_SkyCamCompat);
	AActor *pt2;
	while ((pt2 = it2.Next()))
	{
		SpawnSkybox(pt2);
	}

	for (auto &line : Level->lines)
	{
		switch (line.special)
		{
			int s;
			sector_t *sec;

		// killough 3/7/98:
		// support for drawn heights coming from different sector
		case Transfer_Heights:
			{
				sec = line.frontsector;
				
				if (line.args[1] & 2)
				{
					sec->MoreFlags |= SECMF_FAKEFLOORONLY;
				}
				if (line.args[1] & 4)
				{
					sec->MoreFlags |= SECMF_CLIPFAKEPLANES;
				}
				if (line.args[1] & 8)
				{
					sec->MoreFlags |= SECMF_UNDERWATER;
				}
				else if (forcewater)
				{
					sec->MoreFlags |= SECMF_FORCEDUNDERWATER;
				}
				if (line.args[1] & 16)
				{
					sec->MoreFlags |= SECMF_IGNOREHEIGHTSEC;
				}
				else Level->HasHeightSecs = true;
				if (line.args[1] & 32)
				{
					sec->MoreFlags |= SECMF_NOFAKELIGHT;
				}
				auto itr = Level->GetSectorTagIterator(line.args[0]);
				while ((s = itr.Next()) >= 0)
				{
					Level->sectors[s].heightsec = sec;
					sec->e->FakeFloor.Sectors.Push(&Level->sectors[s]);
					Level->sectors[s].MoreFlags |= (sec->MoreFlags & SECMF_IGNOREHEIGHTSEC);	// copy this to the destination sector for easier checking.
					Level->sectors[s].AdjustFloorClip();
				}
				break;
			}

		// killough 3/16/98: Add support for setting
		// floor lighting independently (e.g. lava)
		case Transfer_FloorLight:
			Level->CreateThinker<DLightTransfer> (line.frontsector, line.args[0], true);
			break;

		// killough 4/11/98: Add support for setting
		// ceiling lighting independently
		case Transfer_CeilingLight:
			Level->CreateThinker<DLightTransfer> (line.frontsector, line.args[0], false);
			break;

		// [Graf Zahl] Add support for setting lighting
		// per wall independently
		case Transfer_WallLight:
			Level->CreateThinker<DWallLightTransfer> (line.frontsector, line.args[0], line.args[1]);
			break;

		case Sector_Attach3dMidtex:
			P_Attach3dMidtexLinesToSector(line.frontsector, line.args[0], line.args[1], !!line.args[2]);
			break;

		case Sector_SetLink:
			if (line.args[0] == 0)
			{
				P_AddSectorLinks(line.frontsector, line.args[1], line.args[2], line.args[3]);
			}
			break;

		case Sector_SetPortal:
			// arg 0 = sector tag
			// arg 1 = type
			//	- 0: normal (handled here)
			//	- 1: copy (handled by the portal they copy)
			//	- 2: EE-style skybox (handled by the camera object)
			//  - 3: EE-style flat portal (GZDoom HW renderer only for now)
			//  - 4: EE-style horizon portal (GZDoom HW renderer only for now)
			//  - 5: copy portal to line (GZDoom HW renderer only for now)
			//  - 6: linked portal
			//	other values reserved for later use
			// arg 2 = 0:floor, 1:ceiling, 2:both
			// arg 3 = 0: anchor, 1: reference line
			// arg 4 = for the anchor only: alpha
			if ((line.args[1] == 0 || line.args[1] == 6) && line.args[3] == 0)
			{
				SpawnPortal(&line, line.args[0], line.args[2], line.args[4], line.args[1]);
			}
			else if (line.args[1] == 3 || line.args[1] == 4)
			{
				unsigned pnum = Level->GetPortal(line.args[1] == 3 ? PORTS_PLANE : PORTS_HORIZON, line.args[2], line.frontsector, NULL, { 0,0 });
				CopyPortal(line.args[0], line.args[2], pnum, 0, true);
			}
			break;

		case Line_SetPortal:
		case Line_QuickPortal:
			SpawnLinePortal(&line);
			break;

			// partial support for MBF's stay-on-lift feature.
			// Unlike MBF we cannot scan all lines for a proper special each time because it'd take too long.
			// So instead, set the info here, but only for repeatable lifts to keep things simple. 
			// This also cannot consider lifts triggered by scripts etc.
		case Generic_Lift:
			if (line.args[3] != 1) continue;
		case Plat_DownWaitUpStay:
		case Plat_DownWaitUpStayLip:
		case Plat_UpWaitDownStay:
		case Plat_UpNearestWaitDownStay:
			if (line.flags & ML_REPEAT_SPECIAL)
			{
				auto it = Level->GetSectorTagIterator(line.args[0], &line);
				int secno;
				while ((secno = it.Next()) != -1)
				{
					Level->sectors[secno].MoreFlags |= SECMF_LIFT;
				}
			}
			break;

		// [RH] ZDoom Static_Init settings
		case Static_Init:
			switch (line.args[1])
			{
			case Init_Gravity:
				{
					double grav = line.Delta().Length() / 100.;
					auto itr = Level->GetSectorTagIterator(line.args[0]);
					while ((s = itr.Next()) >= 0)
						Level->sectors[s].gravity = grav;
				}
				break;

			//case Init_Color:
			// handled in P_LoadSideDefs2()

			case Init_Damage:
				{
					int damage = int(line.Delta().Length());
					auto itr = Level->GetSectorTagIterator(line.args[0]);
					while ((s = itr.Next()) >= 0)
					{
						sector_t *sec = &Level->sectors[s];
						sec->damageamount = damage;
						sec->damagetype = NAME_None;
						if (sec->damageamount < 20)
						{
							sec->leakydamage = 0;
							sec->damageinterval = 32;
						}
						else if (sec->damageamount < 50)
						{
							sec->leakydamage = 5;
							sec->damageinterval = 32;
						}
						else
						{
							sec->leakydamage = 256;
							sec->damageinterval = 1;
						}
					}
				}
				break;

			case Init_SectorLink:
				if (line.args[3] == 0)
					P_AddSectorLinksByID(line.frontsector, line.args[0], line.args[2]);
				break;

			// killough 10/98:
			//
			// Support for sky textures being transferred from sidedefs.
			// Allows scrolling and other effects (but if scrolling is
			// used, then the same sector tag needs to be used for the
			// sky sector, the sky-transfer linedef, and the scroll-effect
			// linedef). Still requires user to use F_SKY1 for the floor
			// or ceiling texture, to distinguish floor and ceiling sky.

			case Init_TransferSky:
				{
					auto itr = Level->GetSectorTagIterator(line.args[0]);
					while ((s = itr.Next()) >= 0)
						Level->sectors[s].sky = (line.Index() + 1) | PL_SKYFLAT;
					break;
				}
			}
			break;
		}
	}
	// [RH] Start running any open scripts on this map
	Level->Behaviors.StartTypedScripts (SCRIPT_Open, NULL, false);
}

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

void MapLoader::SpawnFriction()
{
	line_t *l = &Level->lines[0];

	for (unsigned i = 0 ; i < Level->lines.Size() ; i++,l++)
	{
		if (l->special == Sector_SetFriction)
		{
			int length;

			if (l->args[1])
			{	// [RH] Allow setting friction amount from parameter
				length = l->args[1] <= 200 ? l->args[1] : 200;
			}
			else
			{
				length = int(l->Delta().Length());
			}

			P_SetSectorFriction (Level, l->args[0], length, false);
			l->special = 0;
		}
	}
}

//==========================================================================
//
// Creates all 3D floors defined by one linedef
//
//==========================================================================

int MapLoader::Set3DFloor(line_t * line, int param, int param2, int alpha)
{
	int s;
	int flags;
	int tag = line->args[0];
	sector_t * sec = line->frontsector, *ss;
	
	auto itr = Level->GetSectorTagIterator(tag);
	while ((s = itr.Next()) >= 0)
	{
		ss = &Level->sectors[s];
		
		if (param == 0)
		{
			flags = FF_EXISTS | FF_RENDERALL | FF_SOLID | FF_INVERTSECTOR;
			alpha = 255;
			for (auto l: sec->Lines)
			{
				if (l->special == Sector_SetContents && l->frontsector == sec)
				{
					alpha = clamp<int>(l->args[1], 0, 100);
					if (l->args[2] & 1) flags &= ~FF_SOLID;
					if (l->args[2] & 2) flags |= FF_SEETHROUGH;
					if (l->args[2] & 4) flags |= FF_SHOOTTHROUGH;
					if (l->args[2] & 8) flags |= FF_ADDITIVETRANS;
					if (alpha != 100) flags |= FF_TRANSLUCENT;//|FF_BOTHPLANES|FF_ALLSIDES;
					if (l->args[0])
					{
						// Yes, Vavoom's 3D-floor definitions suck!
						// The content list changed in r1783 of Vavoom to be unified
						// among all its supported games, so it has now ten different
						// values instead of just five.
						static uint32_t vavoomcolors[] = { VC_EMPTY,
							VC_WATER, VC_LAVA, VC_NUKAGE, VC_SLIME, VC_HELLSLIME,
							VC_BLOOD, VC_SLUDGE, VC_HAZARD, VC_BOOMWATER };
						flags |= FF_SWIMMABLE | FF_BOTHPLANES | FF_ALLSIDES | FF_FLOOD;
						
						l->frontsector->Colormap.FadeColor = vavoomcolors[l->args[0]] & VC_COLORMASK;
						l->frontsector->Colormap.FogDensity = 0;
					}
					alpha = (alpha * 255) / 100;
					break;
				}
			}
		}
		else if (param == 4)
		{
			flags = FF_EXISTS | FF_RENDERPLANES | FF_INVERTPLANES | FF_NOSHADE | FF_FIX;
			if (param2 & 1) flags |= FF_SEETHROUGH;	// marker for allowing missing texture checks
			alpha = 255;
		}
		else
		{
			static const int defflags[] = { 0,
				FF_SOLID,
				FF_SWIMMABLE | FF_BOTHPLANES | FF_ALLSIDES | FF_SHOOTTHROUGH | FF_SEETHROUGH,
				FF_SHOOTTHROUGH | FF_SEETHROUGH,
			};
			
			flags = defflags[param & 3] | FF_EXISTS | FF_RENDERALL;
			
			if (param & 4) flags |= FF_ALLSIDES | FF_BOTHPLANES;
			if (param & 16) flags ^= FF_SEETHROUGH;
			if (param & 32) flags ^= FF_SHOOTTHROUGH;
			
			if (param2 & 1) flags |= FF_NOSHADE;
			if (param2 & 2) flags |= FF_DOUBLESHADOW;
			if (param2 & 4) flags |= FF_FOG;
			if (param2 & 8) flags |= FF_THINFLOOR;
			if (param2 & 16) flags |= FF_UPPERTEXTURE;
			if (param2 & 32) flags |= FF_LOWERTEXTURE;
			if (param2 & 64) flags |= FF_ADDITIVETRANS | FF_TRANSLUCENT;
			// if flooding is used the floor must be non-solid and is automatically made shootthrough and seethrough
			if ((param2 & 128) && !(flags & FF_SOLID)) flags |= FF_FLOOD | FF_SEETHROUGH | FF_SHOOTTHROUGH;
			if (param2 & 512) flags |= FF_FADEWALLS;
			if (param2 & 1024) flags |= FF_RESET;
			if (param2 & 2048) flags |= FF_NODAMAGE;
			FTextureID tex = line->sidedef[0]->GetTexture(side_t::top);
			if (!tex.Exists() && alpha < 255)
			{
				alpha = -tex.GetIndex();
			}
			alpha = clamp(alpha, 0, 255);
			if (alpha == 0) flags &= ~(FF_RENDERALL | FF_BOTHPLANES | FF_ALLSIDES);
			else if (alpha != 255) flags |= FF_TRANSLUCENT;
			
		}
		P_Add3DFloor(ss, sec, line, flags, alpha);
	}
	// To be 100% safe this should be done even if the alpha by texture value isn't used.
	if (!line->sidedef[0]->GetTexture(side_t::top).isValid())
		line->sidedef[0]->SetTexture(side_t::top, FNullTextureID());
	return 1;
}

//==========================================================================
//
// Spawns 3D floors
//
//==========================================================================

void MapLoader::Spawn3DFloors ()
{
	static int flagvals[] = {512+2048, 2+512+2048, 512+1024+2048};

	for (auto &line : Level->lines)
	{
		switch(line.special)
		{
		case ExtraFloor_LightOnly:
			if (line.args[1] < 0 || line.args[1] > 2) line.args[1] = 0;
			if (line.args[0] != 0)
				Set3DFloor(&line, 3, flagvals[line.args[1]], 0);
			break;

		case Sector_Set3DFloor:
			// The flag high-byte/line id is only needed in Hexen format.
			// UDMF can set both of these parameters without any restriction of the usable values.
			// In Doom format the translators can take full integers for the tag and the line ID always is the same as the tag.
			if (Level->maptype == MAPTYPE_HEXEN)
			{
				if (line.args[1]&8)
				{
					Level->tagManager.AddLineID(line.Index(), line.args[4]);
				}
				else
				{
					line.args[0]+=256*line.args[4];
					line.args[4]=0;
				}
			}
			Set3DFloor(&line, line.args[1]&~8, line.args[2], line.args[3]);
			break;

		default:
			continue;
		}
		line.special=0;
		line.args[0] = line.args[1] = line.args[2] = line.args[3] = line.args[4] = 0;
	}

	for (auto &sec : Level->sectors)
	{
		P_Recalculate3DFloors(&sec);
	}
}

//==========================================================================
//
// Spawns light effects
//
//==========================================================================

void MapLoader::SpawnLights(sector_t *sector)
{
	const int STROBEBRIGHT = 5;
	const int FASTDARK = 15;
	const int SLOWDARK = TICRATE;

	switch (sector->special)
	{
	case Light_Phased:
		Level->CreateThinker<DPhased>(sector, 48, 63 - (sector->lightlevel & 63));
		break;

		// [RH] Hexen-like phased lighting
	case LightSequenceStart:
		Level->CreateThinker<DPhased>(sector)->Propagate();
		break;

	case dLight_Flicker:
		Level->CreateThinker<DLightFlash>(sector);
		break;

	case dLight_StrobeFast:
		Level->CreateThinker<DStrobe>(sector, STROBEBRIGHT, FASTDARK, false);
		break;

	case dLight_StrobeSlow:
		Level->CreateThinker<DStrobe>(sector, STROBEBRIGHT, SLOWDARK, false);
		break;

	case dLight_Strobe_Hurt:
		Level->CreateThinker<DStrobe>(sector, STROBEBRIGHT, FASTDARK, false);
		break;

	case dLight_Glow:
		Level->CreateThinker<DGlow>(sector);
		break;

	case dLight_StrobeSlowSync:
		Level->CreateThinker<DStrobe>(sector, STROBEBRIGHT, SLOWDARK, true);
		break;

	case dLight_StrobeFastSync:
		Level->CreateThinker<DStrobe>(sector, STROBEBRIGHT, FASTDARK, true);
		break;

	case dLight_FireFlicker:
		Level->CreateThinker<DFireFlicker>(sector);
		break;

	case dScroll_EastLavaDamage:
		Level->CreateThinker<DStrobe>(sector, STROBEBRIGHT, FASTDARK, false);
		break;

	case sLight_Strobe_Hurt:
		Level->CreateThinker<DStrobe>(sector, STROBEBRIGHT, FASTDARK, false);
		break;

	default:
		break;
	}
}

/////////////////////////////
//
// P_GetPushThing() returns a pointer to an MT_PUSH or MT_PULL thing,
// NULL otherwise.

AActor *MapLoader::GetPushThing(int s)
{
	AActor* thing;
	sector_t* sec;

	sec = &Level->sectors[s];
	thing = sec->thinglist;

	while (thing &&
		thing->GetClass()->TypeName != NAME_PointPusher &&
		thing->GetClass()->TypeName != NAME_PointPuller)
	{
		thing = thing->snext;
	}
	return thing;
}

/////////////////////////////
//
// Initialize the sectors where pushers are present
//

void MapLoader::SpawnPushers()
{
	line_t *l = &Level->lines[0];
	int s;

	for (unsigned i = 0; i < Level->lines.Size(); i++, l++)
	{
		switch (l->special)
		{
		case Sector_SetWind: // wind
		{
			auto itr = Level->GetSectorTagIterator(l->args[0]);
			while ((s = itr.Next()) >= 0)
				Level->CreateThinker<DPusher>(DPusher::p_wind, l->args[3] ? l : nullptr, l->args[1], l->args[2], nullptr, s);
			l->special = 0;
			break;
		}

		case Sector_SetCurrent: // current
		{
			auto itr = Level->GetSectorTagIterator(l->args[0]);
			while ((s = itr.Next()) >= 0)
				Level->CreateThinker<DPusher>(DPusher::p_current, l->args[3] ? l : nullptr, l->args[1], l->args[2], nullptr, s);
			l->special = 0;
			break;
		}

		case PointPush_SetForce: // push/pull
			if (l->args[0]) {	// [RH] Find thing by sector
				auto itr = Level->GetSectorTagIterator(l->args[0]);
				while ((s = itr.Next()) >= 0)
				{
					AActor *thing = GetPushThing(s);
					if (thing) {	// No MT_P* means no effect
						// [RH] Allow narrowing it down by tid
						if (!l->args[1] || l->args[1] == thing->tid)
							Level->CreateThinker<DPusher>(DPusher::p_push, l->args[3] ? l : NULL, l->args[2],
								0, thing, s);
					}
				}
			}
			else {	// [RH] Find thing by tid
				AActor *thing;
				auto iterator = Level->GetActorIterator(l->args[1]);

				while ((thing = iterator.Next()))
				{
					if (thing->GetClass()->TypeName == NAME_PointPusher ||
						thing->GetClass()->TypeName == NAME_PointPuller)
					{
						Level->CreateThinker<DPusher>(DPusher::p_push, l->args[3] ? l : NULL, l->args[2], 0, thing, thing->Sector->Index());
					}
				}
			}
			l->special = 0;
			break;
		}
	}
}

//-----------------------------------------------------------------------------
//
// Initialize the scrollers
//
//-----------------------------------------------------------------------------

void MapLoader::SpawnScrollers()
{
	auto SCROLLTYPE = [](int i) { return EScrollPos((i <= 0) || (i & ~7) ? 7 : i); };

	line_t *l = &Level->lines[0];
	side_t *side;
	TArray<int> copyscrollers;

	for (auto &line : Level->lines)
	{
		if (line.special == Sector_CopyScroller)
		{
			// don't allow copying the scroller if the sector has the same tag as it would just duplicate it.
			if (!Level->SectorHasTag(line.frontsector, line.args[0]))
			{
				copyscrollers.Push(line.Index());
			}
			line.special = 0;
		}
	}

	for (unsigned i = 0; i < Level->lines.Size(); i++, l++)
	{
		double dx;	// direction and speed of scrolling
		double dy;
		sector_t *control = nullptr;
		int accel = 0;		// no control sector or acceleration
		int special = l->special;

		// Check for undefined parameters that are non-zero and output messages for them.
		// We don't report for specials we don't understand.
		FLineSpecial *spec = P_GetLineSpecialInfo(special);
		if (spec != nullptr)
		{
			int max = spec->map_args;
			for (unsigned arg = max; arg < countof(l->args); ++arg)
			{
				if (l->args[arg] != 0)
				{
					Printf("Line %d (type %d:%s), arg %u is %d (should be 0)\n",
						i, special, spec->name, arg + 1, l->args[arg]);
				}
			}
		}

		// killough 3/7/98: Types 245-249 are same as 250-254 except that the
		// first side's sector's heights cause scrolling when they change, and
		// this linedef controls the direction and speed of the scrolling. The
		// most complicated linedef since donuts, but powerful :)
		//
		// killough 3/15/98: Add acceleration. Types 214-218 are the same but
		// are accelerative.

		// [RH] Assume that it's a scroller and zero the line's special.
		l->special = 0;

		dx = dy = 0;	// Shut up, GCC

		if (special == Scroll_Ceiling ||
			special == Scroll_Floor ||
			special == Scroll_Texture_Model)
		{
			if (l->args[1] & 3)
			{
				// if 1, then displacement
				// if 2, then accelerative (also if 3)
				control = l->sidedef[0]->sector;
				if (l->args[1] & 2)
					accel = 1;
			}
			if (special == Scroll_Texture_Model || l->args[1] & 4)
			{
				// The line housing the special controls the
				// direction and speed of scrolling.
				dx = l->Delta().X / 32.;
				dy = l->Delta().Y / 32.;
			}
			else
			{
				// The speed and direction are parameters to the special.
				dx = (l->args[3] - 128) / 32.;
				dy = (l->args[4] - 128) / 32.;
			}
		}

		switch (special)
		{
			int s;

		case Scroll_Ceiling:
		{
			auto itr = Level->GetSectorTagIterator(l->args[0]);
			while ((s = itr.Next()) >= 0)
			{
				Level->CreateThinker<DScroller>(EScroll::sc_ceiling, -dx, dy, control, &Level->sectors[s], nullptr, accel);
			}
			for (unsigned j = 0; j < copyscrollers.Size(); j++)
			{
				line_t *line = &Level->lines[copyscrollers[j]];

				if (line->args[0] == l->args[0] && (line->args[1] & 1))
				{
					Level->CreateThinker<DScroller>(EScroll::sc_ceiling, -dx, dy, control, line->frontsector, nullptr, accel);
				}
			}
			break;
		}

		case Scroll_Floor:
			if (l->args[2] != 1)
			{ // scroll the floor texture
				auto itr = Level->GetSectorTagIterator(l->args[0]);
				while ((s = itr.Next()) >= 0)
				{
					Level->CreateThinker<DScroller>(EScroll::sc_floor, -dx, dy, control, &Level->sectors[s], nullptr, accel);
				}
				for (unsigned j = 0; j < copyscrollers.Size(); j++)
				{
					line_t *line = &Level->lines[copyscrollers[j]];

					if (line->args[0] == l->args[0] && (line->args[1] & 2))
					{
						Level->CreateThinker<DScroller>(EScroll::sc_floor, -dx, dy, control, line->frontsector, nullptr, accel);
					}
				}
			}

			if (l->args[2] > 0)
			{ // carry objects on the floor
				auto itr = Level->GetSectorTagIterator(l->args[0]);
				while ((s = itr.Next()) >= 0)
				{
					Level->CreateThinker<DScroller>(EScroll::sc_carry, dx, dy, control, &Level->sectors[s], nullptr, accel);
				}
				for (unsigned j = 0; j < copyscrollers.Size(); j++)
				{
					line_t *line = &Level->lines[copyscrollers[j]];

					if (line->args[0] == l->args[0] && (line->args[1] & 4))
					{
						Level->CreateThinker<DScroller>(EScroll::sc_carry, dx, dy, control, line->frontsector, nullptr, accel);
					}
				}
			}
			break;

			// killough 3/1/98: scroll wall according to linedef
			// (same direction and speed as scrolling floors)
		case Scroll_Texture_Model:
		{
			if (l->args[0] != 0)
			{
				auto itr = Level->GetLineIdIterator(l->args[0]);
				while ((s = itr.Next()) >= 0)
				{
					if (s != (int)i)
						Level->CreateThinker<DScroller>(dx, dy, &Level->lines[s], control, accel);
				}
			}
			else
			{
				Level->CreateThinker<DScroller>(dx, dy, l, control, accel);
			}
			break;
		}

		case Scroll_Texture_Offsets:
		{
			double divider = max(1, l->args[3]);
			// killough 3/2/98: scroll according to sidedef offsets
			side = l->sidedef[0];
			if (l->args[2] & 3)
			{
				// if 1, then displacement
				// if 2, then accelerative (also if 3)
				control = l->sidedef[0]->sector;
				if (l->args[2] & 2)
					accel = 1;
			}
			double dx = -side->GetTextureXOffset(side_t::mid) / divider;
			double dy = side->GetTextureYOffset(side_t::mid) / divider;
			if (l->args[1] == 0)
			{
				Level->CreateThinker<DScroller>(EScroll::sc_side, dx, dy, control, nullptr, side, accel, SCROLLTYPE(l->args[0]));
			}
			else
			{
				auto it = Level->GetLineIdIterator(l->args[1]);
				while ((s = it.Next()) >= 0)
				{
					Level->CreateThinker<DScroller>(EScroll::sc_side, dx, dy, control, nullptr, Level->lines[s].sidedef[0], accel, SCROLLTYPE(l->args[0]));
				}
			}
			break;
		}

		case Scroll_Texture_Left:
			l->special = special;	// Restore the special, for compat_useblocking's benefit.
			side = Level->lines[i].sidedef[0];
			Level->CreateThinker<DScroller>(EScroll::sc_side, l->args[0] / 64., 0, nullptr, nullptr, side, accel, SCROLLTYPE(l->args[1]));
			break;

		case Scroll_Texture_Right:
			l->special = special;
			side = Level->lines[i].sidedef[0];
			Level->CreateThinker<DScroller>(EScroll::sc_side, -l->args[0] / 64., 0, nullptr, nullptr, side, accel, SCROLLTYPE(l->args[1]));
			break;

		case Scroll_Texture_Up:
			l->special = special;
			side = Level->lines[i].sidedef[0];
			Level->CreateThinker<DScroller>(EScroll::sc_side, 0, l->args[0] / 64., nullptr, nullptr, side, accel, SCROLLTYPE(l->args[1]));
			break;

		case Scroll_Texture_Down:
			l->special = special;
			side = Level->lines[i].sidedef[0];
			Level->CreateThinker<DScroller>(EScroll::sc_side, 0, -l->args[0] / 64., nullptr, nullptr, side, accel, SCROLLTYPE(l->args[1]));
			break;

		case Scroll_Texture_Both:
			side = Level->lines[i].sidedef[0];
			if (l->args[0] == 0) {
				dx = (l->args[1] - l->args[2]) / 64.;
				dy = (l->args[4] - l->args[3]) / 64.;
				Level->CreateThinker<DScroller>(EScroll::sc_side, dx, dy, nullptr, nullptr, side, accel);
			}
			break;

		default:
			// [RH] It wasn't a scroller after all, so restore the special.
			l->special = special;
			break;
		}
	}
}


void MapLoader::CreateScroller(EScroll type, double dx, double dy, sector_t *affectee, int accel, EScrollPos scrollpos)
{
	Level->CreateThinker<DScroller>(type, dx, dy, nullptr, affectee, nullptr, accel, scrollpos);
}
