//
// Copyright(C) 2020 by Ryan Krafnick
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// DESCRIPTION:
//	DSDA Options Lump
//

#include "filesystem.h"
#include "g_mapinfo.h"
#include "doomdef.h"
#include "templates.h"


struct dsda_options
{
	int monsters_remember = -1;
	int monster_infighting = -1;
	int monster_backing = -1;
	int monster_avoid_hazards = -1;
	int monster_friction = -1;
	int help_friends = 0;
	int player_helpers = 0;
	int friend_distance = 128;
	int dog_jumping = -1;
	int comp_dropoff = -1;
	int comp_vile = -1;
	int comp_pain = -1;
	int comp_doorlight = -1;
	int comp_model = -1;
	int comp_floors = -1;
	int comp_pursuit = -1; // 1 for 'latest'
	int comp_staylift = -1;
	int comp_stairs = -1;
	int comp_ledgeblock = -1;
	int comp_friendlyspawn = -1;
};

struct dsda_option_t
{
	const char* key;
	int dsda_options::* value;
	int min;
	int max;
};

static dsda_option_t option_list[] = {
  { "monsters_remember", &dsda_options::monsters_remember, 0, 1 },
  { "monster_infighting", &dsda_options::monster_infighting, 0, 1 },
  { "monster_backing", &dsda_options::monster_backing, 0, 1 },
  { "monster_avoid_hazards", &dsda_options::monster_avoid_hazards, 0, 1 },
  { "monster_friction", &dsda_options::monster_friction, 0, 1 },
  { "help_friends", &dsda_options::help_friends, 0, 1 },
  { "player_helpers", &dsda_options::player_helpers, 0, 3 },
  { "friend_distance", &dsda_options::friend_distance, 0, 999 },
  { "dog_jumping", &dsda_options::dog_jumping, 0, 1 },
  { "comp_dropoff", &dsda_options::comp_dropoff, 0, 1 },
  { "comp_vile", &dsda_options::comp_vile, 0, 1 },
  { "comp_pain", &dsda_options::comp_pain, 0, 1 },
  { "comp_doorlight", &dsda_options::comp_doorlight, 0, 1 },
  { "comp_model", &dsda_options::comp_model, 0, 1 },
  { "comp_floors", &dsda_options::comp_floors, 0, 1 },
  { "comp_pursuit", &dsda_options::comp_pursuit, 0, 1 },
  { "comp_staylift", &dsda_options::comp_staylift, 0, 1 },
  { "comp_stairs", &dsda_options::comp_stairs, 0, 1 },
  { "comp_ledgeblock", &dsda_options::comp_ledgeblock, 0, 1 },
  { "comp_friendlyspawn", &dsda_options::comp_friendlyspawn, 0, 1 },
  { 0 }
};

enum { OPTIONS_LINE_LENGTH = 80 };

struct options_lump_t
{
	const char* data;
	int length;
};

static const char* dsda_ReadOption(char* buf, size_t size, options_lump_t* lump)
{
	if (lump->length <= 0)
		return nullptr;

	while (size > 1 && *lump->data && lump->length) 
	{
		size--;
		lump->length--;
		if ((*buf++ = *lump->data++) == '\n')
			break;
	}

	*buf = '\0';

	return lump->data;
}

static struct dsda_options dsda_LumpOptions(int lumpnum)
{
	struct dsda_options mbf_options;

	options_lump_t lump;
	char buf[OPTIONS_LINE_LENGTH];
	char key[OPTIONS_LINE_LENGTH];
	char* scan;
	int value, count;
	dsda_option_t* option;

	lump.length = fileSystem.FileLength(lumpnum);
	auto data = fileSystem.ReadFile(lumpnum);
	lump.data = (char*)data.GetMem();

	while (dsda_ReadOption(buf, OPTIONS_LINE_LENGTH, &lump)) 
	{
		if (buf[0] == '#')
			continue;

		scan = buf;
		count = sscanf(scan, "%79s %d", key, &value);

		if (count != 2)
			continue;

		for (option = option_list; option->value; option++) 
		{
			if (!strncmp(key, option->key, OPTIONS_LINE_LENGTH)) 
			{
				mbf_options.*option->value = clamp(option->min, option->max, value);

				//lprintf(LO_INFO, "dsda_LumpOptions: %s = %d\n", key, value);

				break;
			}
		}
	}
	return mbf_options;
}


void parseOptions() 
{
	int lumpnum = fileSystem.FindFile("OPTIONS");

	if (lumpnum == -1)
		return;

	auto opt = dsda_LumpOptions(lumpnum);

	auto setflag = [](auto& flag, auto mask, bool set)
	{
		if (set) flag |= mask;
		else flag &= ~mask;
	};

	for (auto& lev : wadlevelinfos)
	{
		setflag(lev.flags2, LEVEL2_NOINFIGHTING, opt.monster_infighting);
		setflag(lev.flags3, LEVEL3_AVOIDMELEE, opt.monster_backing);
		setflag(lev.compatflags2, COMPATF2_AVOID_HAZARDS, opt.monster_avoid_hazards);
		setflag(lev.compatmask2, COMPATF2_AVOID_HAZARDS, opt.monster_avoid_hazards);
		setflag(lev.compatflags, COMPATF_MBFMONSTERMOVE, opt.monster_friction);
		setflag(lev.compatmask, COMPATF_MBFMONSTERMOVE, opt.monster_friction);
		setflag(lev.compatflags, COMPATF_DROPOFF, opt.comp_dropoff);
		setflag(lev.compatmask, COMPATF_DROPOFF, opt.comp_dropoff);
		setflag(lev.compatflags, COMPATF_DROPOFF, opt.comp_dropoff);
		setflag(lev.compatmask, COMPATF_DROPOFF, opt.comp_dropoff);
		setflag(lev.compatflags, COMPATF_CORPSEGIBS, opt.comp_vile);
		setflag(lev.compatmask, COMPATF_CORPSEGIBS, opt.comp_vile);
		setflag(lev.flags3, LEVEL3_VILEOPTION, opt.comp_vile);
		setflag(lev.flags3, LEVEL3_NOJUMPDOWN, !opt.dog_jumping);	// this one's rather pointless, but well...
		setflag(lev.compatflags, COMPATF_LIMITPAIN, opt.comp_pain);
		setflag(lev.compatmask, COMPATF_LIMITPAIN, opt.comp_pain);
		setflag(lev.compatflags, COMPATF_NODOORLIGHT, opt.comp_doorlight);
		setflag(lev.compatmask, COMPATF_NODOORLIGHT, opt.comp_doorlight);
		setflag(lev.compatflags, COMPATF_LIGHT | COMPATF_SHORTTEX, opt.comp_model); // only the relevant parts of this catch-all option.
		setflag(lev.compatmask, COMPATF_LIGHT | COMPATF_SHORTTEX, opt.comp_model);
		setflag(lev.compatflags2, COMPATF2_FLOORMOVE, opt.comp_floors);
		setflag(lev.compatmask2, COMPATF2_FLOORMOVE, opt.comp_floors);
		setflag(lev.compatflags2, COMPATF2_STAYONLIFT, opt.comp_staylift);
		setflag(lev.compatmask2, COMPATF2_STAYONLIFT, opt.comp_staylift);
		setflag(lev.compatflags, COMPATF_STAIRINDEX, opt.comp_stairs);
		setflag(lev.compatmask, COMPATF_STAIRINDEX, opt.comp_stairs);
		setflag(lev.compatflags, COMPATF_CROSSDROPOFF, opt.comp_ledgeblock);
		setflag(lev.compatmask, COMPATF_CROSSDROPOFF, opt.comp_ledgeblock);

		/* later. these should be supported but are not implemented yet.
		if (opt.monsters_remember == 0)
		if (opt.comp_pursuit)
		if (opt.comp_friendlyspawn == 0)
		int help_friends = 0;
		int player_helpers = 0;
		int friend_distance = 128;
		*/

	}
}

