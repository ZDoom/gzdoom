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


// We are referring to sprite numbers.
#include "info.h"

#include "d_items.h"
#include "a_artifacts.h"


int num_items;

// [RH] Guess what. These next three functions are from Quake2:
//	g_items.c

/*
===============
GetItemByIndex
===============
*/
gitem_t	*GetItemByIndex (int index)
{
	if (index == 0 || index >= num_items)
		return NULL;

	return &itemlist[index];
}


/*
===============
FindItemByClassname

===============
*/
gitem_t	*FindItemByClassname (const char *classname)
{
	int		i;
	gitem_t	*it;

	it = itemlist;
	for (i = 0; i < num_items; i++, it++)
		if (it->classname && !stricmp(it->classname, classname))
			return it;

	return NULL;
}

/*
===============
FindItem

===============
*/
gitem_t	*FindItem (const char *pickup_name)
{
	int		i;
	gitem_t	*it;

	it = itemlist;
	for (i = 0; i < num_items; i++, it++)
		if (it->pickup_name && !stricmp(it->pickup_name, pickup_name))
			return it;

	return NULL;
}


// Item info
// Used mainly by the give command. Hopefully will
// become more general-purpose later.
// (Yes, this was inspired by Quake 2)
gitem_t itemlist[] = {
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
		"weapon_chainsaw",
		NULL,
		NULL,
		IT_WEAPON,
		wp_chainsaw,
		0,
		"Chainsaw"
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
		"weapon_supershotgun",
		NULL,
		NULL,
		IT_WEAPON,
		wp_supershotgun,
		0,
		"Super Shotgun"
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
		IT_POWERUP,
		pw_invulnerability,
		0,
		"Invulnerability"
	},

	{
		"item_berserk",
		NULL,
		NULL,
		IT_POWERUP,
		pw_strength,
		0,
		"Berserk"
	},

	{
		"item_invisibility",
		NULL,
		NULL,
		IT_POWERUP,
		pw_invisibility,
		0,
		"Invisibility"
	},

	{
		"item_ironfeet",
		NULL,
		NULL,
		IT_POWERUP,
		pw_ironfeet,
		0,
		"Radiation Suit"
	},

	{
		"item_allmap",
		NULL,
		NULL,
		IT_POWERUP,
		pw_allmap,
		0,
		"Computer Map"
	},

	{
		"item_visor",
		NULL,
		NULL,
		IT_POWERUP,
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
	num_items = sizeof(itemlist)/sizeof(itemlist[0]) - 1;
}
