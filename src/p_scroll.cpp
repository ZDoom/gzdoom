// Emacs style mode select	 -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id:$
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// The source is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
// for more details.
//
// $Log:$
//
// DESCRIPTION:
//		Initializes and implements BOOM linedef triggers for
//			Scrollers/Conveyors
//
//-----------------------------------------------------------------------------


#include <stdlib.h>
#include "actor.h"
#include "p_spec.h"
#include "farchive.h"
#include "p_lnspec.h"
#include "r_data/r_interpolate.h"

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
	
	DScroller (EScroll type, double dx, double dy, int control, int affectee, int accel, EScrollPos scrollpos = EScrollPos::scw_all);
	DScroller (double dx, double dy, const line_t *l, int control, int accel, EScrollPos scrollpos = EScrollPos::scw_all);
	void Destroy();

	void Serialize (FArchive &arc);
	void Tick ();

	bool AffectsWall (int wallnum) const { return m_Type == EScroll::sc_side && m_Affectee == wallnum; }
	int GetWallNum () const { return m_Type == EScroll::sc_side ? m_Affectee : -1; }
	void SetRate (double dx, double dy) { m_dx = dx; m_dy = dy; }
	bool IsType (EScroll type) const { return type == m_Type; }
	int GetAffectee () const { return m_Affectee; }
	EScrollPos GetScrollParts() const { return m_Parts; }

protected:
	EScroll m_Type;		// Type of scroll effect
	double m_dx, m_dy;		// (dx,dy) scroll speeds
	int m_Affectee;			// Number of affected sidedef, sector, tag, or whatever
	int m_Control;			// Control sector (-1 if none) used to control scrolling
	double m_LastHeight;	// Last known height of control sector
	double m_vdx, m_vdy;	// Accumulated velocity if accelerative
	int m_Accel;			// Whether it's accelerative
	EScrollPos m_Parts;			// Which parts of a sidedef are being scrolled?
	TObjPtr<DInterpolation> m_Interpolations[3];

private:
	DScroller ()
	{
	}
};


IMPLEMENT_POINTY_CLASS (DScroller)
 DECLARE_POINTER (m_Interpolations[0])
 DECLARE_POINTER (m_Interpolations[1])
 DECLARE_POINTER (m_Interpolations[2])
END_POINTERS


//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

inline FArchive &operator<< (FArchive &arc, EScroll &type)
{
	BYTE val = (BYTE)type;
	arc << val;
	type = (EScroll)val;
	return arc;
}

inline FArchive &operator<< (FArchive &arc, EScrollPos &type)
{
	int val = (int)type;
	arc << val;
	type = (EScrollPos)val;
	return arc;
}

EScrollPos operator &(EScrollPos one, EScrollPos two)
{
	return EScrollPos(int(one) & int(two));
}

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

void DScroller::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	arc << m_Type
		<< m_dx << m_dy
		<< m_Affectee
		<< m_Control
		<< m_LastHeight
		<< m_vdx << m_vdy
		<< m_Accel
		<< m_Parts
		<< m_Interpolations[0]
		<< m_Interpolations[1]
		<< m_Interpolations[2];
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

	if (m_Control != -1)
	{	// compute scroll amounts based on a sector's height changes
		double height = sectors[m_Control].CenterFloor () +
						 sectors[m_Control].CenterCeiling ();
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
				sides[m_Affectee].AddTextureXOffset(side_t::top, dx);
				sides[m_Affectee].AddTextureYOffset(side_t::top, dy);
			}
			if (m_Parts & EScrollPos::scw_mid && (sides[m_Affectee].linedef->backsector == NULL ||
				!(sides[m_Affectee].linedef->flags&ML_3DMIDTEX)))
			{
				sides[m_Affectee].AddTextureXOffset(side_t::mid, dx);
				sides[m_Affectee].AddTextureYOffset(side_t::mid, dy);
			}
			if (m_Parts & EScrollPos::scw_bottom)
			{
				sides[m_Affectee].AddTextureXOffset(side_t::bottom, dx);
				sides[m_Affectee].AddTextureYOffset(side_t::bottom, dy);
			}
			break;

		case EScroll::sc_floor:						// killough 3/7/98: Scroll floor texture
			RotationComp(&sectors[m_Affectee], sector_t::floor, dx, dy, tdx, tdy);
			sectors[m_Affectee].AddXOffset(sector_t::floor, tdx);
			sectors[m_Affectee].AddYOffset(sector_t::floor, tdy);
			break;

		case EScroll::sc_ceiling:					// killough 3/7/98: Scroll ceiling texture
			RotationComp(&sectors[m_Affectee], sector_t::ceiling, dx, dy, tdx, tdy);
			sectors[m_Affectee].AddXOffset(sector_t::ceiling, tdx);
			sectors[m_Affectee].AddYOffset(sector_t::ceiling, tdy);
			break;

		// [RH] Don't actually carry anything here. That happens later.
		case EScroll::sc_carry:
			level.Scrolls[m_Affectee].Scroll.X += dx;
			level.Scrolls[m_Affectee].Scroll.Y += dy;
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

DScroller::DScroller (EScroll type, double dx, double dy,
					  int control, int affectee, int accel, EScrollPos scrollpos)
	: DThinker (STAT_SCROLLER)
{
	m_Type = type;
	m_dx = dx;
	m_dy = dy;
	m_Accel = accel;
	m_Parts = scrollpos;
	m_vdx = m_vdy = 0;
	if ((m_Control = control) != -1)
		m_LastHeight =
			sectors[control].CenterFloor () + sectors[control].CenterCeiling ();
	m_Affectee = affectee;
	m_Interpolations[0] = m_Interpolations[1] = m_Interpolations[2] = NULL;

	switch (type)
	{
	case EScroll::sc_carry:
		level.AddScroller (affectee);
		break;

	case EScroll::sc_side:
		sides[affectee].Flags |= WALLF_NOAUTODECALS;
		if (m_Parts & EScrollPos::scw_top)
		{
			m_Interpolations[0] = sides[m_Affectee].SetInterpolation(side_t::top);
		}
		if (m_Parts & EScrollPos::scw_mid && (sides[m_Affectee].linedef->backsector == NULL ||
			!(sides[m_Affectee].linedef->flags&ML_3DMIDTEX)))
		{
			m_Interpolations[1] = sides[m_Affectee].SetInterpolation(side_t::mid);
		}
		if (m_Parts & EScrollPos::scw_bottom)
		{
			m_Interpolations[2] = sides[m_Affectee].SetInterpolation(side_t::bottom);
		}
		break;

	case EScroll::sc_floor:
		m_Interpolations[0] = sectors[affectee].SetInterpolation(sector_t::FloorScroll, false);
		break;

	case EScroll::sc_ceiling:
		m_Interpolations[0] = sectors[affectee].SetInterpolation(sector_t::CeilingScroll, false);
		break;

	default:
		break;
	}
}

void DScroller::Destroy ()
{
	for(int i=0;i<3;i++)
	{
		if (m_Interpolations[i] != NULL)
		{
			m_Interpolations[i]->DelRef();
			m_Interpolations[i] = NULL;
		}
	}
	Super::Destroy();
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

DScroller::DScroller (double dx, double dy, const line_t *l,
					 int control, int accel, EScrollPos scrollpos)
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
	if ((m_Control = control) != -1)
		m_LastHeight = sectors[control].CenterFloor() + sectors[control].CenterCeiling();
	m_Affectee = int(l->sidedef[0] - sides);
	sides[m_Affectee].Flags |= WALLF_NOAUTODECALS;
	m_Interpolations[0] = m_Interpolations[1] = m_Interpolations[2] = NULL;

	if (m_Parts & EScrollPos::scw_top)
	{
		m_Interpolations[0] = sides[m_Affectee].SetInterpolation(side_t::top);
	}
	if (m_Parts & EScrollPos::scw_mid && (sides[m_Affectee].linedef->backsector == NULL ||
		!(sides[m_Affectee].linedef->flags&ML_3DMIDTEX)))
	{
		m_Interpolations[1] = sides[m_Affectee].SetInterpolation(side_t::mid);
	}
	if (m_Parts & EScrollPos::scw_bottom)
	{
		m_Interpolations[2] = sides[m_Affectee].SetInterpolation(side_t::bottom);
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

void P_SpawnScrollers(void)
{
	int i;
	line_t *l = lines;
	TArray<int> copyscrollers;

	for (i = 0; i < numlines; i++)
	{
		if (lines[i].special == Sector_CopyScroller)
		{
			// don't allow copying the scroller if the sector has the same tag as it would just duplicate it.
			if (!tagManager.SectorHasTag(lines[i].frontsector, lines[i].args[0]))
			{
				copyscrollers.Push(i);
			}
			lines[i].special = 0;
		}
	}

	for (i = 0; i < numlines; i++, l++)
	{
		double dx;	// direction and speed of scrolling
		double dy;
		int control = -1, accel = 0;		// no control sector or acceleration
		int special = l->special;

		// Check for undefined parameters that are non-zero and output messages for them.
		// We don't report for specials we don't understand.
		FLineSpecial *spec = P_GetLineSpecialInfo(special);
		if (spec != NULL)
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
				control = int(l->sidedef[0]->sector - sectors);
				if (l->args[1] & 2)
					accel = 1;
			}
			if (special == Scroll_Texture_Model ||
				l->args[1] & 4)
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
				new DScroller(EScroll::sc_ceiling, -dx, dy, control, s, accel);
			}
			for (unsigned j = 0; j < copyscrollers.Size(); j++)
			{
				line_t *line = &lines[copyscrollers[j]];

				if (line->args[0] == l->args[0] && (line->args[1] & 1))
				{
					new DScroller(EScroll::sc_ceiling, -dx, dy, control, int(line->frontsector - sectors), accel);
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
					new DScroller (EScroll::sc_floor, -dx, dy, control, s, accel);
				}
				for(unsigned j = 0;j < copyscrollers.Size(); j++)
				{
					line_t *line = &lines[copyscrollers[j]];

					if (line->args[0] == l->args[0] && (line->args[1] & 2))
					{
						new DScroller (EScroll::sc_floor, -dx, dy, control, int(line->frontsector-sectors), accel);
					}
				}
			}

			if (l->args[2] > 0)
			{ // carry objects on the floor
				FSectorTagIterator itr(l->args[0]);
				while ((s = itr.Next()) >= 0)
				{
					new DScroller (EScroll::sc_carry, dx, dy, control, s, accel);
				}
				for(unsigned j = 0;j < copyscrollers.Size(); j++)
				{
					line_t *line = &lines[copyscrollers[j]];

					if (line->args[0] == l->args[0] && (line->args[1] & 4))
					{
						new DScroller (EScroll::sc_carry, dx, dy, control, int(line->frontsector-sectors), accel);
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
				if (s != i)
					new DScroller(dx, dy, lines + s, control, accel);
			}
			break;
		}

		case Scroll_Texture_Offsets:
			// killough 3/2/98: scroll according to sidedef offsets
			s = int(lines[i].sidedef[0] - sides);
			new DScroller (EScroll::sc_side, -sides[s].GetTextureXOffset(side_t::mid),
				sides[s].GetTextureYOffset(side_t::mid), -1, s, accel, SCROLLTYPE(l->args[0]));
			break;

		case Scroll_Texture_Left:
			l->special = special;	// Restore the special, for compat_useblocking's benefit.
			s = int(lines[i].sidedef[0] - sides);
			new DScroller (EScroll::sc_side, l->args[0] / 64., 0,
						   -1, s, accel, SCROLLTYPE(l->args[1]));
			break;

		case Scroll_Texture_Right:
			l->special = special;
			s = int(lines[i].sidedef[0] - sides);
			new DScroller (EScroll::sc_side, -l->args[0] / 64., 0,
						   -1, s, accel, SCROLLTYPE(l->args[1]));
			break;

		case Scroll_Texture_Up:
			l->special = special;
			s = int(lines[i].sidedef[0] - sides);
			new DScroller (EScroll::sc_side, 0, l->args[0] / 64.,
						   -1, s, accel, SCROLLTYPE(l->args[1]));
			break;

		case Scroll_Texture_Down:
			l->special = special;
			s = int(lines[i].sidedef[0] - sides);
			new DScroller (EScroll::sc_side, 0, -l->args[0] / 64.,
						   -1, s, accel, SCROLLTYPE(l->args[1]));
			break;

		case Scroll_Texture_Both:
			s = int(lines[i].sidedef[0] - sides);
			if (l->args[0] == 0) {
				dx = (l->args[1] - l->args[2]) / 64.;
				dy = (l->args[4] - l->args[3]) / 64.;
				new DScroller (EScroll::sc_side, dx, dy, -1, s, accel);
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

void SetWallScroller (int id, int sidechoice, double dx, double dy, EScrollPos Where)
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
			int wallnum = scroller->GetWallNum ();

			if (wallnum >= 0 && tagManager.LineHasID(sides[wallnum].linedef, id) &&
				int(sides[wallnum].linedef->sidedef[sidechoice] - sides) == wallnum &&
				Where == scroller->GetScrollParts())
			{
				scroller->Destroy ();
			}
		}
	}
	else
	{
		// Find scrollers already attached to the matching walls, and change
		// their rates.
		TArray<FThinkerCollection> Collection;
		{
			TThinkerIterator<DScroller> iterator (STAT_SCROLLER);
			FThinkerCollection collect;

			while ( (collect.Obj = iterator.Next ()) )
			{
				if ((collect.RefNum = ((DScroller *)collect.Obj)->GetWallNum ()) != -1 &&
					tagManager.LineHasID(sides[collect.RefNum].linedef, id) &&
					int(sides[collect.RefNum].linedef->sidedef[sidechoice] - sides) == collect.RefNum &&
					Where == ((DScroller *)collect.Obj)->GetScrollParts())
				{
					((DScroller *)collect.Obj)->SetRate (dx, dy);
					Collection.Push (collect);
				}
			}
		}

		size_t numcollected = Collection.Size ();
		int linenum;

		// Now create scrollers for any walls that don't already have them.
		FLineIdIterator itr(id);
		while ((linenum = itr.Next()) >= 0)
		{
			if (lines[linenum].sidedef[sidechoice] != NULL)
			{
				int sidenum = int(lines[linenum].sidedef[sidechoice] - sides);
				unsigned int i;
				for (i = 0; i < numcollected; i++)
				{
					if (Collection[i].RefNum == sidenum)
						break;
				}
				if (i == numcollected)
				{
					new DScroller (EScroll::sc_side, dx, dy, -1, sidenum, 0, Where);
				}
			}
		}
	}
}

void SetScroller (int tag, EScroll type, double dx, double dy)
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
			if (tagManager.SectorHasTag(scroller->GetAffectee (), tag))
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
		new DScroller (type, dx, dy, -1, i, 0);
	}
}

void P_CreateScroller(EScroll type, double dx, double dy, int control, int affectee, int accel, EScrollPos scrollpos)
{
	new DScroller(type, dx, dy, control, affectee, accel, scrollpos);
}
