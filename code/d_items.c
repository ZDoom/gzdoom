// Emacs style mode select   -*- C++ -*- 
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
//
//-----------------------------------------------------------------------------

static const char
rcsid[] = "$Id:$";

// We are referring to sprite numbers.
#include "info.h"

#include "d_englsh.h"
#include "d_items.h"


//
// PSPRITE ACTIONS for waepons.
// This struct controls the weapon animations.
//
// Each entry is:
//   ammo/amunition type
//  upstate
//  downstate
// readystate
// atkstate, i.e. attack/fire/hit frame
// flashstate, muzzle flash
//
weaponinfo_t	weaponinfo[NUMWEAPONS] =
{
	{
		// fist
		am_noammo,
		S_PUNCHUP,
		S_PUNCHDOWN,
		S_PUNCH,
		S_PUNCH1,
		S_NULL
		},	
	{
		// pistol
		am_clip,
		S_PISTOLUP,
		S_PISTOLDOWN,
		S_PISTOL,
		S_PISTOL1,
		S_PISTOLFLASH
	},	
	{
		// shotgun
		am_shell,
		S_SGUNUP,
		S_SGUNDOWN,
		S_SGUN,
		S_SGUN1,
		S_SGUNFLASH1
	},
	{
		// chaingun
		am_clip,
		S_CHAINUP,
		S_CHAINDOWN,
		S_CHAIN,
		S_CHAIN1,
		S_CHAINFLASH1
	},
	{
		// missile launcher
		am_misl,
		S_MISSILEUP,
		S_MISSILEDOWN,
		S_MISSILE,
		S_MISSILE1,
		S_MISSILEFLASH1
	},
	{
		// plasma rifle
		am_cell,
		S_PLASMAUP,
		S_PLASMADOWN,
		S_PLASMA,
		S_PLASMA1,
		S_PLASMAFLASH1
	},
	{
		// bfg 9000
		am_cell,
		S_BFGUP,
		S_BFGDOWN,
		S_BFG,
		S_BFG1,
		S_BFGFLASH1
	},
	{
		// chainsaw
		am_noammo,
		S_SAWUP,
		S_SAWDOWN,
		S_SAW,
		S_SAW1,
		S_NULL
	},
	{
		// super shotgun
		am_shell,
		S_DSGUNUP,
		S_DSGUNDOWN,
		S_DSGUN,
		S_DSGUN1,
		S_DSGUNFLASH1
	},	
};

int num_items;

// [RH] Guess what. These next three functions are from Quake2:
//	game/g_items.c

/*
===============
GetItemByIndex
===============
*/
gitem_t	*GetItemByIndex (int index)
{
	if (index == 0 || index >= num_items)
		return NULL;

	return &ItemList[index];
}


/*
===============
FindItemByClassname

===============
*/
gitem_t	*FindItemByClassname (char *classname)
{
	int		i;
	gitem_t	*it;

	it = ItemList;
	for (i=0 ; i<num_items ; i++, it++)
	{
		if (!it->classname)
			continue;
		if (!stricmp(it->classname, classname))
			return it;
	}

	return NULL;
}

/*
===============
FindItem

===============
*/
gitem_t	*FindItem (char *pickup_name)
{
	int		i;
	gitem_t	*it;

	it = ItemList;
	for (i=0 ; i<num_items ; i++, it++)
	{
		if (!it->pickup_name)
			continue;
		if (!stricmp(it->pickup_name, pickup_name))
			return it;
	}

	return NULL;
}


// Item info
// Used mainly by the give command. Hopefully will
// become more general-purpose later.
// (Yes, this was inspired by Quake 2)
gitem_t ItemList[] = {
	{
		NULL
	},	// leave index 0 alone

	{
		"item_armor_basic",
		NULL,
		NULL,
		IT_ARMOR,
		1,
		0,
		"Basic Armor"
	},

	{
		"item_armor_mega",
		NULL,
		NULL,
		IT_ARMOR,
		2,
		0,
		"Mega Armor"
	},

	{
		"item_armor_bonus",
		NULL,
		NULL,
		IT_ARMOR,
		1,
		0,
		"Armor Bonus"
	},

	{
		"weapon_fist",
		NULL,
		NULL,
		IT_WEAPON,
		wp_fist,
		0,
		"Fist"
	},

	{
		"weapon_pistol",
		NULL,
		NULL,
		IT_WEAPON,
		wp_pistol,
		0,
		"Pistol"
	},
	
	{
		"weapon_shotgun",
		NULL,
		NULL,
		IT_WEAPON,
		wp_shotgun,
		0,
		"Shotgun"
	},

	{
		"weapon_chaingun",
		NULL,
		NULL,
		IT_WEAPON,
		wp_chaingun,
		0,
		"Chaingun"
	},

	{
		"weapon_rocketlauncher",
		NULL,
		NULL,
		IT_WEAPON,
		wp_missile,
		0,
		"Rocket Launcher"
	},

	{
		"weapon_plasmagun",
		NULL,
		NULL,
		IT_WEAPON,
		wp_plasma,
		0,
		"Plasma Gun"
	},

	{
		"weapon_bfg",
		NULL,
		NULL,
		IT_WEAPON,
		wp_bfg,
		0,
		"BFG9000"
	},

	{
		"weapon_chainsaw",
		NULL,
		NULL,
		IT_WEAPON,
		wp_chainsaw,
		0,
		"Chainsaw"
	},

	{
		"weapon_supershotgun",
		NULL,
		NULL,
		IT_WEAPON,
		wp_supershotgun,
		0,
		"Super Shotgun"
	},

	{
		"ammo_bullets",
		NULL,
		NULL,
		IT_AMMO,
		am_clip,
		1,
		"Bullets"
	},

	{
		"ammo_shells",
		NULL,
		NULL,
		IT_AMMO,
		am_shell,
		1,
		"Shells"
	},

	{
		"ammo_cells",
		NULL,
		NULL,
		IT_AMMO,
		am_cell,
		1,
		"Cells"
	},

	{
		"ammo_rocket",
		NULL,
		NULL,
		IT_AMMO,
		am_misl,
		1,
		"Rockets"
	},

	//
	// POWERUP ITEMS
	//
	{
		"item_invulnerability",
		NULL,
		NULL,
		IT_POWER,
		pw_invulnerability,
		0,
		"Invulnerability"
	},

	{
		"item_berserk",
		NULL,
		NULL,
		IT_POWER,
		pw_strength,
		0,
		"Berserk"
	},

	{
		"item_invisibility",
		NULL,
		NULL,
		IT_POWER,
		pw_invisibility,
		0,
		"Partial Invisibility"
	},

	{
		"item_ironfeet",
		NULL,
		NULL,
		IT_POWER,
		pw_ironfeet,
		0,
		"Iron Feet"
	},

	{
		"item_allmap",
		NULL,
		NULL,
		IT_POWER,
		pw_allmap,
		0,
		"Computer Map"
	},

	{
		"item_visor",
		NULL,
		NULL,
		IT_POWER,
		pw_infrared,
		0,
		"Light Amplification Visor"
	},

	//
	// KEYS
	//

	{
		"key_blue_card",
		NULL,
		NULL,
		IT_KEY,
		it_bluecard,
		0,
		"Blue Keycard"
	},

	{
		"key_yellow_card",
		NULL,
		NULL,
		IT_KEY,
		it_yellowcard,
		0,
		"Yellow Keycard"
	},

	{
		"key_red_card",
		NULL,
		NULL,
		IT_KEY,
		it_redcard,
		0,
		"Red Keycard"
	},

	{
		"key_blue_skull",
		NULL,
		NULL,
		IT_KEY,
		it_blueskull,
		0,
		"Blue Skull Key"
	},

	{
		"key_yellow_skull",
		NULL,
		NULL,
		IT_KEY,
		it_yellowskull,
		0,
		"Yellow Skull Key"
	},

	{
		"key_red_skull",
		NULL,
		NULL,
		IT_KEY,
		it_redskull,
		0,
		"Red Skull Key"
	},

	// end of list marker
	{NULL}
};

void InitItems (void)
{
	num_items = sizeof(ItemList)/sizeof(ItemList[0]) - 1;
}