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
// DESCRIPTION:
//		Items: key cards, artifacts, weapon, ammunition.
//
//-----------------------------------------------------------------------------


#ifndef __D_ITEMS__
#define __D_ITEMS__

#include "doomdef.h"
#include "doomtype.h"

struct mobj_s;
struct player_s;

// Weapon info: sprite frames, ammunition use.
struct weaponinfo_s
{
	ammotype_t	ammo;
	int 		upstate;
	int 		downstate;
	int 		readystate;
	int 		atkstate;
	int 		flashstate;

};
typedef struct weaponinfo_s weaponinfo_t;

extern	weaponinfo_t	weaponinfo[NUMWEAPONS];

// Item stuff: (this is d_items.h, right?)

// gitem_t->flags
#define IT_WEAPON				1				// use makes active weapon
#define IT_AMMO 				2
#define IT_ARMOR				4
#define IT_KEY					8
#define IT_ARTIFACT 			16				// Don't auto-activate item (unused)
#define IT_POWERUP				32				// Auto-activate item

struct gitem_s
{
		char			*classname;
		BOOL	 		(*pickup)(struct player_s *ent, struct mobj_s *other);
		void			(*use)(struct player_s *ent, struct gitem_s *item);
		byte			flags;
		byte			offset; 				// For Weapon, Ammo, Armor, Key: Offset in appropriate table
		byte			quantity;				// For Ammo: How much to pickup

		char			*pickup_name;
};
typedef struct gitem_s gitem_t;

extern int num_items;

extern gitem_t itemlist[];

void InitItems (void);

// FindItem
gitem_t	*GetItemByIndex (int index);
gitem_t	*FindItemByClassname (const char *classname);
gitem_t *FindItem (const char *pickup_name);

#define ITEM_INDEX(i)	((i)-itemlist)


#endif
//-----------------------------------------------------------------------------
//
// $Log:$
//
//-----------------------------------------------------------------------------
