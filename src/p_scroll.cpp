//-----------------------------------------------------------------------------
//
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
//		Initializes and implements BOOM linedef triggers for
//			Scrollers/Conveyors
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
#include "actor.h"
#include "p_spec.h"
#include "serializer.h"
#include "p_lnspec.h"
#include "r_data/r_interpolate.h"
#include "g_levellocals.h"

//-----------------------------------------------------------------------------
//
// killough 3/7/98: Add generalized scroll effects
//
//-----------------------------------------------------------------------------

class DScroller : public DThinker
{
	DECLARE_CLASS (DScroller, DThinker)
	HAS_OBJECT_POINTERS
public:
	
	DScroller(EScroll type, double dx, double dy, sector_t *control, sector_t *sec, side_t *side, int accel, EScrollPos scrollpos = EScrollPos::scw_all);
	DScroller (double dx, double dy, const line_t *l, sector_t *control, int accel, EScrollPos scrollpos = EScrollPos::scw_all);
	void OnDestroy() override;

	void Serialize(FSerializer &arc);
	void Tick ();

	bool AffectsWall (side_t * wall) const { return m_Side == wall; }
	side_t *GetWall () const { return m_Side; }
	sector_t *GetSector() const { return m_Sector; }
	void SetRate (double dx, double dy) { m_dx = dx; m_dy = dy; }
	bool IsType (EScroll type) const { return type == m_Type; }
	EScrollPos GetScrollParts() const { return m_Parts; }

protected:
	EScroll m_Type;		// Type of scroll effect
	double m_dx, m_dy;		// (dx,dy) scroll speeds
	sector_t *m_Sector;		// Affected sector
	side_t *m_Side;			// ... or side
	sector_t *m_Controller;	// Control sector (nullptr if none) used to control scrolling
	double m_LastHeight;	// Last known height of control sector
	double m_vdx, m_vdy;	// Accumulated velocity if accelerative
	int m_Accel;			// Whether it's accelerative
	EScrollPos m_Parts;			// Which parts of a sidedef are being scrolled?
	TObjPtr<DInterpolation*> m_Interpolations[3];

private:
	DScroller ()
	{
	}
};

IMPLEMENT_CLASS(DScroller, false, true)

IMPLEMENT_POINTERS_START(DScroller)
	IMPLEMENT_POINTER(m_Interpolations[0])
	IMPLEMENT_POINTER(m_Interpolations[1])
	IMPLEMENT_POINTER(m_Interpolations[2])
IMPLEMENT_POINTERS_END


//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

EScrollPos operator &(EScrollPos one, EScrollPos two)
{
	return EScrollPos(int(one) & int(two));
}

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

void DScroller::Serialize(FSerializer &arc)
{
	Super::Serialize (arc);
	arc.Enum("type", m_Type)
		("dx", m_dx)
		("dy", m_dy)
		("sector", m_Sector)
		("side", m_Side)
		("control", m_Controller)
		("lastheight", m_LastHeight)
		("vdx", m_vdx)
		("vdy", m_vdy)
		("accel", m_Accel)
		.Enum("parts", m_Parts)
		.Array("interpolations", m_Interpolations, 3);
}

//-----------------------------------------------------------------------------
//
// [RH] Compensate for rotated sector textures by rotating the scrolling
// in the opposite direction.
//
//-----------------------------------------------------------------------------

static void RotationComp(const sector_t *sec, int which, double dx, double dy, double &tdx, double &tdy)
{
	DAngle an = sec->GetAngle(which);
	if (an == 0)
	{
		tdx = dx;
		tdy = dy;
	}
	else
	{
		double ca = -an.Cos();
		double sa = -an.Sin();
		tdx = dx*ca - dy*sa;
		tdy = dy*ca + dx*sa;
	}
}

//-----------------------------------------------------------------------------
//
// killough 2/28/98:
//
// This function, with the help of r_plane.c and r_bsp.c, supports generalized
// scrolling floors and walls, with optional mobj-carrying properties, e.g.
// conveyor belts, rivers, etc. A linedef with a special type affects all
// tagged sectors the same way, by creating scrolling and/or object-carrying
// properties. Multiple linedefs may be used on the same sector and are
// cumulative, although the special case of scrolling a floor and carrying
// things on it, requires only one linedef. The linedef's direction determines
// the scrolling direction, and the linedef's length determines the scrolling
// speed. This was designed so that an edge around the sector could be used to
// control the direction of the sector's scrolling, which is usually what is
// desired.
//
// Process the active scrollers.
//
// This is the main scrolling code
// killough 3/7/98
//
//-----------------------------------------------------------------------------

void DScroller::Tick ()
{
	double dx = m_dx, dy = m_dy, tdx, tdy;

	if (m_Controller != nullptr)
	{	// compute scroll amounts based on a sector's height changes
		double height = m_Controller->CenterFloor () + m_Controller->CenterCeiling ();
		double delta = height - m_LastHeight;
		m_LastHeight = height;
		dx *= delta;
		dy *= delta;
	}

	// killough 3/14/98: Add acceleration
	if (m_Accel)
	{
		m_vdx = dx += m_vdx;
		m_vdy = dy += m_vdy;
	}

	if (dx == 0 && dy == 0)
		return;

	switch (m_Type)
	{
		case EScroll::sc_side:					// killough 3/7/98: Scroll wall texture
			if (m_Parts & EScrollPos::scw_top)
			{
				m_Side->AddTextureXOffset(side_t::top, dx);
				m_Side->AddTextureYOffset(side_t::top, dy);
			}
			if (m_Parts & EScrollPos::scw_mid && (m_Side->linedef->backsector == nullptr ||
				!(m_Side->linedef->flags&ML_3DMIDTEX)))
			{
				m_Side->AddTextureXOffset(side_t::mid, dx);
				m_Side->AddTextureYOffset(side_t::mid, dy);
			}
			if (m_Parts & EScrollPos::scw_bottom)
			{
				m_Side->AddTextureXOffset(side_t::bottom, dx);
				m_Side->AddTextureYOffset(side_t::bottom, dy);
			}
			break;

		case EScroll::sc_floor:						// killough 3/7/98: Scroll floor texture
			RotationComp(m_Sector, sector_t::floor, dx, dy, tdx, tdy);
			m_Sector->AddXOffset(sector_t::floor, tdx);
			m_Sector->AddYOffset(sector_t::floor, tdy);
			break;

		case EScroll::sc_ceiling:					// killough 3/7/98: Scroll ceiling texture
			RotationComp(m_Sector, sector_t::ceiling, dx, dy, tdx, tdy);
			m_Sector->AddXOffset(sector_t::ceiling, tdx);
			m_Sector->AddYOffset(sector_t::ceiling, tdy);
			break;

		// [RH] Don't actually carry anything here. That happens later.
		case EScroll::sc_carry:
			level.Scrolls[m_Sector->Index()] += { dx, dy };
			// mark all potentially affected things here so that the very expensive calculation loop in AActor::Tick does not need to run for actors which do not touch a scrolling sector.
			for (auto n = m_Sector->touching_thinglist; n; n = n->m_snext)
			{
				n->m_thing->flags8 |= MF8_INSCROLLSEC;
			}
			break;

		case EScroll::sc_carry_ceiling:       // to be added later
			break;
	}
}

//-----------------------------------------------------------------------------
//
// Add_Scroller()
//
// Add a generalized scroller to the thinker list.
//
// type: the enumerated type of scrolling: floor, ceiling, floor carrier,
//   wall, floor carrier & scroller
//
// (dx,dy): the direction and speed of the scrolling or its acceleration
//
// control: the sector whose heights control this scroller's effect
//   remotely, or -1 if no control sector
//
// affectee: the index of the affected object (sector or sidedef)
//
// accel: non-zero if this is an accelerative effect
//
//-----------------------------------------------------------------------------

DScroller::DScroller (EScroll type, double dx, double dy,  sector_t *ctrl, sector_t *sec, side_t *side, int accel, EScrollPos scrollpos)
	: DThinker (STAT_SCROLLER)
{
	m_Type = type;
	m_dx = dx;
	m_dy = dy;
	m_Accel = accel;
	m_Parts = scrollpos;
	m_vdx = m_vdy = 0;
	m_LastHeight = 0;
	if ((m_Controller = ctrl) != nullptr)
	{
		m_LastHeight = m_Controller->CenterFloor() + m_Controller->CenterCeiling();
	}
	m_Side = side;
	m_Sector = sec;
	m_Interpolations[0] = m_Interpolations[1] = m_Interpolations[2] = nullptr;

	switch (type)
	{
	case EScroll::sc_carry:
		assert(sec != nullptr);
		level.AddScroller (sec->Index());
		break;

	case EScroll::sc_side:
		assert(side != nullptr);
		side->Flags |= WALLF_NOAUTODECALS;
		if (m_Parts & EScrollPos::scw_top)
		{
			m_Interpolations[0] = m_Side->SetInterpolation(side_t::top);
		}
		if (m_Parts & EScrollPos::scw_mid && (m_Side->linedef->backsector == nullptr ||
			!(m_Side->linedef->flags&ML_3DMIDTEX)))
		{
			m_Interpolations[1] = m_Side->SetInterpolation(side_t::mid);
		}
		if (m_Parts & EScrollPos::scw_bottom)
		{
			m_Interpolations[2] = m_Side->SetInterpolation(side_t::bottom);
		}
		break;

	case EScroll::sc_floor:
		assert(sec != nullptr);
		m_Interpolations[0] = m_Sector->SetInterpolation(sector_t::FloorScroll, false);
		break;

	case EScroll::sc_ceiling:
		assert(sec != nullptr);
		m_Interpolations[0] = m_Sector->SetInterpolation(sector_t::CeilingScroll, false);
		break;

	default:
		break;
	}
}

void DScroller::OnDestroy ()
{
	for(int i=0;i<3;i++)
	{
		if (m_Interpolations[i] != nullptr)
		{
			m_Interpolations[i]->DelRef();
			m_Interpolations[i] = nullptr;
		}
	}
	Super::OnDestroy();
}

//-----------------------------------------------------------------------------
//
// Adds wall scroller. Scroll amount is rotated with respect to wall's
// linedef first, so that scrolling towards the wall in a perpendicular
// direction is translated into vertical motion, while scrolling along
// the wall in a parallel direction is translated into horizontal motion.
//
// killough 5/25/98: cleaned up arithmetic to avoid drift due to roundoff
//
//-----------------------------------------------------------------------------

DScroller::DScroller (double dx, double dy, const line_t *l, sector_t * control, int accel, EScrollPos scrollpos)
	: DThinker (STAT_SCROLLER)
{
	double x = fabs(l->Delta().X), y = fabs(l->Delta().Y), d;
	if (y > x) d = x, x = y, y = d;

	d = x / g_sin(g_atan2(y, x) + M_PI / 2);
	x = -(dy * l->Delta().Y + dx * l->Delta().X) / d;
	y = -(dx * l->Delta().Y - dy * l->Delta().X) / d;

	m_Type = EScroll::sc_side;
	m_dx = x;
	m_dy = y;
	m_vdx = m_vdy = 0;
	m_Accel = accel;
	m_Parts = scrollpos;
	m_LastHeight = 0;
	if ((m_Controller = control) != nullptr)
	{
		m_LastHeight = m_Controller->CenterFloor() + m_Controller->CenterCeiling();
	}
	m_Sector = nullptr;
	m_Side = l->sidedef[0];
	m_Side->Flags |= WALLF_NOAUTODECALS;
	m_Interpolations[0] = m_Interpolations[1] = m_Interpolations[2] = nullptr;

	if (m_Parts & EScrollPos::scw_top)
	{
		m_Interpolations[0] = m_Side->SetInterpolation(side_t::top);
	}
	if (m_Parts & EScrollPos::scw_mid && (m_Side->linedef->backsector == nullptr ||
		!(m_Side->linedef->flags&ML_3DMIDTEX)))
	{
		m_Interpolations[1] = m_Side->SetInterpolation(side_t::mid);
	}
	if (m_Parts & EScrollPos::scw_bottom)
	{
		m_Interpolations[2] = m_Side->SetInterpolation(side_t::bottom);
	}
}

// Amount (dx,dy) vector linedef is shifted right to get scroll amount
#define SCROLL_SHIFT 5
#define SCROLLTYPE(i) EScrollPos(((i) <= 0) || ((i) & ~7) ? 7 : (i))

//-----------------------------------------------------------------------------
//
// Initialize the scrollers
//
//-----------------------------------------------------------------------------

void P_SpawnScrollers(FLevelLocals *Level)
{
	line_t *l = &Level->lines[0];
	side_t *side;
	TArray<int> copyscrollers;

	for (auto &line : Level->lines)
	{
		if (line.special == Sector_CopyScroller)
		{
			// don't allow copying the scroller if the sector has the same tag as it would just duplicate it.
			if (!tagManager.SectorHasTag(line.frontsector, line.args[0]))
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
						i, special, spec->name, arg+1, l->args[arg]);
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
			FSectorTagIterator itr(l->args[0]);
			while ((s = itr.Next()) >= 0)
			{
				Create<DScroller>(EScroll::sc_ceiling, -dx, dy, control, &Level->sectors[s], nullptr, accel);
			}
			for (unsigned j = 0; j < copyscrollers.Size(); j++)
			{
				line_t *line = &Level->lines[copyscrollers[j]];

				if (line->args[0] == l->args[0] && (line->args[1] & 1))
				{
					Create<DScroller>(EScroll::sc_ceiling, -dx, dy, control, line->frontsector, nullptr, accel);
				}
			}
			break;
		}

		case Scroll_Floor:
			if (l->args[2] != 1)
			{ // scroll the floor texture
				FSectorTagIterator itr(l->args[0]);
				while ((s = itr.Next()) >= 0)
				{
					Create<DScroller> (EScroll::sc_floor, -dx, dy, control, &Level->sectors[s], nullptr, accel);
				}
				for(unsigned j = 0;j < copyscrollers.Size(); j++)
				{
					line_t *line = &Level->lines[copyscrollers[j]];

					if (line->args[0] == l->args[0] && (line->args[1] & 2))
					{
						Create<DScroller>(EScroll::sc_floor, -dx, dy, control, line->frontsector, nullptr, accel);
					}
				}
			}

			if (l->args[2] > 0)
			{ // carry objects on the floor
				FSectorTagIterator itr(l->args[0]);
				while ((s = itr.Next()) >= 0)
				{
					Create<DScroller> (EScroll::sc_carry, dx, dy, control, &Level->sectors[s], nullptr, accel);
				}
				for(unsigned j = 0;j < copyscrollers.Size(); j++)
				{
					line_t *line = &Level->lines[copyscrollers[j]];

					if (line->args[0] == l->args[0] && (line->args[1] & 4))
					{
						Create<DScroller> (EScroll::sc_carry, dx, dy, control, line->frontsector, nullptr, accel);
					}
				}
			}
			break;

		// killough 3/1/98: scroll wall according to linedef
		// (same direction and speed as scrolling floors)
		case Scroll_Texture_Model:
		{
			FLineIdIterator itr(l->args[0]);
			while ((s = itr.Next()) >= 0)
			{
				if (s != (int)i)
					Create<DScroller>(dx, dy, &Level->lines[s], control, accel);
			}
			break;
		}

		case Scroll_Texture_Offsets:
			// killough 3/2/98: scroll according to sidedef offsets
			side = Level->lines[i].sidedef[0];
			Create<DScroller> (EScroll::sc_side, -side->GetTextureXOffset(side_t::mid),
				side->GetTextureYOffset(side_t::mid), nullptr, nullptr, side, accel, SCROLLTYPE(l->args[0]));
			break;

		case Scroll_Texture_Left:
			l->special = special;	// Restore the special, for compat_useblocking's benefit.
			side = Level->lines[i].sidedef[0];
			Create<DScroller> (EScroll::sc_side, l->args[0] / 64., 0, nullptr, nullptr, side, accel, SCROLLTYPE(l->args[1]));
			break;

		case Scroll_Texture_Right:
			l->special = special;
			side = Level->lines[i].sidedef[0];
			Create<DScroller> (EScroll::sc_side, -l->args[0] / 64., 0, nullptr, nullptr, side, accel, SCROLLTYPE(l->args[1]));
			break;

		case Scroll_Texture_Up:
			l->special = special;
			side = Level->lines[i].sidedef[0];
			Create<DScroller> (EScroll::sc_side, 0, l->args[0] / 64., nullptr, nullptr, side, accel, SCROLLTYPE(l->args[1]));
			break;

		case Scroll_Texture_Down:
			l->special = special;
			side = Level->lines[i].sidedef[0];
			Create<DScroller> (EScroll::sc_side, 0, -l->args[0] / 64., nullptr, nullptr, side, accel, SCROLLTYPE(l->args[1]));
			break;

		case Scroll_Texture_Both:
			side = Level->lines[i].sidedef[0];
			if (l->args[0] == 0) {
				dx = (l->args[1] - l->args[2]) / 64.;
				dy = (l->args[4] - l->args[3]) / 64.;
				Create<DScroller> (EScroll::sc_side, dx, dy, nullptr, nullptr, side, accel);
			}
			break;

		default:
			// [RH] It wasn't a scroller after all, so restore the special.
			l->special = special;
			break;
		}
	}
}

//-----------------------------------------------------------------------------
//
// Modify a wall scroller
//
//-----------------------------------------------------------------------------

void SetWallScroller (FLevelLocals *Level, int id, int sidechoice, double dx, double dy, EScrollPos Where)
{
	Where = Where & scw_all;
	if (Where == 0) return;

	if (dx == 0 && dy == 0)
	{
		// Special case: Remove the scroller, because the deltas are both 0.
		TThinkerIterator<DScroller> iterator (STAT_SCROLLER);
		DScroller *scroller;

		while ( (scroller = iterator.Next ()) )
		{
			auto wall = scroller->GetWall ();

			if (wall != nullptr && tagManager.LineHasID(wall->linedef, id) && wall->linedef->sidedef[sidechoice] == wall && Where == scroller->GetScrollParts())
			{
				scroller->Destroy ();
			}
		}
	}
	else
	{
		// Find scrollers already attached to the matching walls, and change
		// their rates.
		TArray<DScroller *> Collection;
		{
			TThinkerIterator<DScroller> iterator (STAT_SCROLLER);
			DScroller *scroll;

			while ( (scroll = iterator.Next ()) )
			{
				auto wall = scroll->GetWall();
				if (wall != nullptr)
				{
					auto line = wall->linedef;
					if (tagManager.LineHasID(line, id) && line->sidedef[sidechoice] == wall && Where == scroll->GetScrollParts())
					{
						scroll->SetRate(dx, dy);
						Collection.Push(scroll);
					}
				}
			}
		}

		size_t numcollected = Collection.Size ();
		int linenum;

		// Now create scrollers for any walls that don't already have them.
		FLineIdIterator itr(id);
		while ((linenum = itr.Next()) >= 0)
		{
			side_t *side = Level->lines[linenum].sidedef[sidechoice];
			if (side != nullptr)
			{
				if (Collection.FindEx([=](const DScroller *element) { return element->GetWall() == side; }) == Collection.Size())
				{
					Create<DScroller> (EScroll::sc_side, dx, dy, nullptr, nullptr, side, 0, Where);
				}
			}
		}
	}
}

void SetScroller (FLevelLocals *Level, int tag, EScroll type, double dx, double dy)
{
	TThinkerIterator<DScroller> iterator (STAT_SCROLLER);
	DScroller *scroller;
	int i;

	// Check if there is already a scroller for this tag
	// If at least one sector with this tag is scrolling, then they all are.
	// If the deltas are both 0, we don't remove the scroller, because a
	// displacement/accelerative scroller might have been set up, and there's
	// no way to create one after the level is fully loaded.
	i = 0;
	while ( (scroller = iterator.Next ()) )
	{
		if (scroller->IsType (type))
		{
			if (tagManager.SectorHasTag(scroller->GetSector(), tag))
			{
				i++;
				scroller->SetRate (dx, dy);
			}
		}
	}

	if (i > 0 || (dx == 0 && dy == 0))
	{
		return;
	}

	// Need to create scrollers for the sector(s)
	FSectorTagIterator itr(tag);
	while ((i = itr.Next()) >= 0)
	{
		Create<DScroller> (type, dx, dy, nullptr, &Level->sectors[i], nullptr, 0);
	}
}

void P_CreateScroller(EScroll type, double dx, double dy, sector_t *affectee, int accel, EScrollPos scrollpos)
{
	Create<DScroller>(type, dx, dy, nullptr, affectee, nullptr, accel, scrollpos);
}
