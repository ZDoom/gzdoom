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
//	Globally defined strings.
// 
//-----------------------------------------------------------------------------

#include <string.h>

#include "c_cvars.h"
#include "doomtype.h"
#include "doomstat.h"
#include "dstrings.h"
#include "d_protocol.h"
#include "m_argv.h"
#include "w_wad.h"
#include "z_zone.h"
#include "cmdlib.h"
#include "g_level.h"

char* endmsg[NUM_QUITMESSAGES+1]=
{
  // DOOM1
  /*QUITMSG*/NULL,
  "please don't leave, there's more\ndemons to toast!",
  "let's beat it -- this is turning\ninto a bloodbath!",
  "i wouldn't leave if i were you.\ndos is much worse.",
  "you're trying to say you like dos\nbetter than me, right?",
  "don't leave yet -- there's a\ndemon around that corner!",
  "ya know, next time you come in here\ni'm gonna toast ya.",
  "go ahead and leave. see if i care.",

  // QuitDOOM II messages
  "you want to quit?\nthen, thou hast lost an eighth!",
  "don't go now, there's a \ndimensional shambler waiting\nat the dos prompt!",
  "get outta here and go back\nto your boring programs.",
  "if i were your boss, i'd \n deathmatch ya in a minute!",
  "look, bud. you leave now\nand you forfeit your body count!",
  "just leave. when you come\nback, i'll be waiting with a bat.",
  "you're lucky i don't smack\nyou for thinking about leaving."
};


gamestring_t Strings[NUMSTRINGS] = {
	{ str_notchanged, "D_DEVSTR", "Useless mode ON.\n", NULL },
	{ str_notchanged, "D_CDROM", "CD-ROM Version: zdoom.cfg from c:\\zdoomdat\n", NULL },
	{ str_notchanged, "PRESSKEY", "press a key.", NULL },
	{ str_notchanged, "PRESSYN", "press y or n.", NULL },
	{ str_notchanged, "QUITMSG", "are you sure you want to\nquit this great game?", NULL },
	{ str_notchanged, "LOADNET", "you can't do load while in a net game!\n\npress a key.", NULL },
	{ str_notchanged, "QLOADNET", "you can't quickload during a netgame!\n\npress a key.", NULL },
	{ str_notchanged, "QSAVESPOT", "you haven't picked a quicksave slot yet!\n\npress a key.", NULL },
	{ str_notchanged, "SAVEDEAD", "you can't save if you aren't playing!\n\npress a key.", NULL },
	{ str_notchanged, "QSPROMPT", "quicksave over your game named\n\n'%s'?\n\npress y or n.", NULL },
	{ str_notchanged, "QLPROMPT", "do you want to quickload the game named\n\n'%s'?\n\npress y or n.", NULL },
	{ str_notchanged, "NEWGAME", "you can't start a new game\nwhile in a network game.\n\npress a key.", NULL },
	{ str_notchanged, "NIGHTMARE", "are you sure? this skill level\nisn't even remotely fair.\n\npress y or n.", NULL },
	{ str_notchanged, "SWSTRING", "this is the shareware version of doom.\n\nyou need to order the entire trilogy.\n\npress a key.", NULL },
	{ str_notchanged, "MSGOFF", "Messages OFF", NULL },
	{ str_notchanged, "MSGON", "Messages ON", NULL },
	{ str_notchanged, "NETEND", "you can't end a netgame!\n\npress a key.", NULL },
	{ str_notchanged, "ENDGAME", "are you sure you want to end the game?\n\npress y or n.", NULL },
	{ str_notchanged, "DOSY", "(press y to quit)", NULL },
	{ str_notchanged, "EMPTYSTRING", "empty slot", NULL },
	{ str_notchanged, "GOTARMOR", "Picked up the armor.", NULL },
	{ str_notchanged, "GOTMEGA", "Picked up the MegaArmor!", NULL },
	{ str_notchanged, "GOTHTHBONUS", "Picked up a health bonus.", NULL },
	{ str_notchanged, "GOTARMBONUS", "Picked up an armor bonus.", NULL },
	{ str_notchanged, "GOTSTIM", "Picked up a stimpack.", NULL },
	{ str_notchanged, "GOTMEDINEED", "Picked up a medikit that you REALLY need!", NULL },
	{ str_notchanged, "GOTMEDIKIT", "Picked up a medikit.", NULL },
	{ str_notchanged, "GOTSUPER", "Supercharge!", NULL },
	{ str_notchanged, "GOTBLUECARD", "Picked up a blue keycard.", NULL },
	{ str_notchanged, "GOTYELWCARD", "Picked up a yellow keycard.", NULL },
	{ str_notchanged, "GOTREDCARD", "Picked up a red keycard.", NULL },
	{ str_notchanged, "GOTBLUESKUL", "Picked up a blue skull key.", NULL },
	{ str_notchanged, "GOTYELWSKUL", "Picked up a yellow skull key.", NULL },
	{ str_notchanged, "GOTREDSKUL", "Picked up a red skull key.", NULL },
	{ str_notchanged, "GOTINVUL", "Invulnerability!", NULL },
	{ str_notchanged, "GOTBERSERK", "Berserk!", NULL },
	{ str_notchanged, "GOTINVIS", "Partial Invisibility", NULL },
	{ str_notchanged, "GOTSUIT", "Radiation Shielding Suit", NULL },
	{ str_notchanged, "GOTMAP", "Computer Area Map", NULL },
	{ str_notchanged, "GOTVISOR", "Light Amplification Visor", NULL },
	{ str_notchanged, "GOTMSPHERE", "MegaSphere!", NULL },
	{ str_notchanged, "GOTCLIP", "Picked up a clip.", NULL },
	{ str_notchanged, "GOTCLIPBOX", "Picked up a box of bullets.", NULL },
	{ str_notchanged, "GOTROCKET", "Picked up a rocket.", NULL },
	{ str_notchanged, "GOTROCKBOX", "Picked up a box of rockets.", NULL },
	{ str_notchanged, "GOTCELL", "Picked up an energy cell.", NULL },
	{ str_notchanged, "GOTCELLBOX", "Picked up an energy cell pack.", NULL },
	{ str_notchanged, "GOTSHELLS", "Picked up 4 shotgun shells.", NULL },
	{ str_notchanged, "GOTSHELLBOX", "Picked up a box of shotgun shells.", NULL },
	{ str_notchanged, "GOTBACKPACK", "Picked up a backpack full of ammo!", NULL },
	{ str_notchanged, "GOTBFG9000", "You got the BFG9000!  Oh, yes.", NULL },
	{ str_notchanged, "GOTCHAINGUN", "You got the chaingun!", NULL },
	{ str_notchanged, "GOTCHAINSAW", "A chainsaw!  Find some meat!", NULL },
	{ str_notchanged, "GOTLAUNCHER", "You got the rocket launcher!", NULL },
	{ str_notchanged, "GOTPLASMA", "You got the plasma gun!", NULL },
	{ str_notchanged, "GOTSHOTGUN", "You got the shotgun!", NULL },
	{ str_notchanged, "GOTSHOTGUN2", "You got the super shotgun!", NULL },
	{ str_notchanged, "PD_BLUEO", "You need a blue key to activate this object", NULL },
	{ str_notchanged, "PD_REDO", "You need a red key to activate this object", NULL },
	{ str_notchanged, "PD_YELLOWO", "You need a yellow key to activate this object", NULL },
	{ str_notchanged, "PD_BLUEK", "You need a blue key to open this door", NULL },
	{ str_notchanged, "PD_REDK", "You need a red key to open this door", NULL },
	{ str_notchanged, "PD_YELLOWK", "You need a yellow key to open this door", NULL },
	{ str_notchanged, "GGSAVED", "game saved.", NULL },
	{ str_notchanged, "HUSTR_MSGU", "[Message unsent]", NULL },
	{ str_notchanged, "HUSTR_E1M1", "E1M1: Hangar", NULL },
	{ str_notchanged, "HUSTR_E1M2", "E1M2: Nuclear Plant", NULL },
	{ str_notchanged, "HUSTR_E1M3", "E1M3: Toxin Refinery", NULL },
	{ str_notchanged, "HUSTR_E1M4", "E1M4: Command Control", NULL },
	{ str_notchanged, "HUSTR_E1M5", "E1M5: Phobos Lab", NULL },
	{ str_notchanged, "HUSTR_E1M6", "E1M6: Central Processing", NULL },
	{ str_notchanged, "HUSTR_E1M7", "E1M7: Computer Station", NULL },
	{ str_notchanged, "HUSTR_E1M8", "E1M8: Phobos Anomaly", NULL },
	{ str_notchanged, "HUSTR_E1M9", "E1M9: Military Base", NULL },
	{ str_notchanged, "HUSTR_E2M1", "E2M1: Deimos Anomaly", NULL },
	{ str_notchanged, "HUSTR_E2M2", "E2M2: Containment Area", NULL },
	{ str_notchanged, "HUSTR_E2M3", "E2M3: Refinery", NULL },
	{ str_notchanged, "HUSTR_E2M4", "E2M4: Deimos Lab", NULL },
	{ str_notchanged, "HUSTR_E2M5", "E2M5: Command Center", NULL },
	{ str_notchanged, "HUSTR_E2M6", "E2M6: Halls of the Damned", NULL },
	{ str_notchanged, "HUSTR_E2M7", "E2M7: Spawning Vats", NULL },
	{ str_notchanged, "HUSTR_E2M8", "E2M8: Tower of Babel", NULL },
	{ str_notchanged, "HUSTR_E2M9", "E2M9: Fortress of Mystery", NULL },
	{ str_notchanged, "HUSTR_E3M1", "E3M1: Hell Keep", NULL },
	{ str_notchanged, "HUSTR_E3M2", "E3M2: Slough of Despair", NULL },
	{ str_notchanged, "HUSTR_E3M3", "E3M3: Pandemonium", NULL },
	{ str_notchanged, "HUSTR_E3M4", "E3M4: House of Pain", NULL },
	{ str_notchanged, "HUSTR_E3M5", "E3M5: Unholy Cathedral", NULL },
	{ str_notchanged, "HUSTR_E3M6", "E3M6: Mt. Erebus", NULL },
	{ str_notchanged, "HUSTR_E3M7", "E3M7: Limbo", NULL },
	{ str_notchanged, "HUSTR_E3M8", "E3M8: Dis", NULL },
	{ str_notchanged, "HUSTR_E3M9", "E3M9: Warrens", NULL },
	{ str_notchanged, "HUSTR_E4M1", "E4M1: Hell Beneath", NULL },
	{ str_notchanged, "HUSTR_E4M2", "E4M2: Perfect Hatred", NULL },
	{ str_notchanged, "HUSTR_E4M3", "E4M3: Sever The Wicked", NULL },
	{ str_notchanged, "HUSTR_E4M4", "E4M4: Unruly Evil", NULL },
	{ str_notchanged, "HUSTR_E4M5", "E4M5: They Will Repent", NULL },
	{ str_notchanged, "HUSTR_E4M6", "E4M6: Against Thee Wickedly", NULL },
	{ str_notchanged, "HUSTR_E4M7", "E4M7: And Hell Followed", NULL },
	{ str_notchanged, "HUSTR_E4M8", "E4M8: Unto The Cruel", NULL },
	{ str_notchanged, "HUSTR_E4M9", "E4M9: Fear", NULL },
	{ str_notchanged, "HUSTR_1", "level 1: entryway", NULL },
	{ str_notchanged, "HUSTR_2", "level 2: underhalls", NULL },
	{ str_notchanged, "HUSTR_3", "level 3: the gantlet", NULL },
	{ str_notchanged, "HUSTR_4", "level 4: the focus", NULL },
	{ str_notchanged, "HUSTR_5", "level 5: the waste tunnels", NULL },
	{ str_notchanged, "HUSTR_6", "level 6: the crusher", NULL },
	{ str_notchanged, "HUSTR_7", "level 7: dead simple", NULL },
	{ str_notchanged, "HUSTR_8", "level 8: tricks and traps", NULL },
	{ str_notchanged, "HUSTR_9", "level 9: the pit", NULL },
	{ str_notchanged, "HUSTR_10", "level 10: refueling base", NULL },
	{ str_notchanged, "HUSTR_11", "level 11: 'o' of destruction!", NULL },
	{ str_notchanged, "HUSTR_12", "level 12: the factory", NULL },
	{ str_notchanged, "HUSTR_13", "level 13: downtown", NULL },
	{ str_notchanged, "HUSTR_14", "level 14: the inmost dens", NULL },
	{ str_notchanged, "HUSTR_15", "level 15: industrial zone", NULL },
	{ str_notchanged, "HUSTR_16", "level 16: suburbs", NULL },
	{ str_notchanged, "HUSTR_17", "level 17: tenements", NULL },
	{ str_notchanged, "HUSTR_18", "level 18: the courtyard", NULL },
	{ str_notchanged, "HUSTR_19", "level 19: the citadel", NULL },
	{ str_notchanged, "HUSTR_20", "level 20: gotcha!", NULL },
	{ str_notchanged, "HUSTR_21", "level 21: nirvana", NULL },
	{ str_notchanged, "HUSTR_22", "level 22: the catacombs", NULL },
	{ str_notchanged, "HUSTR_23", "level 23: barrels o' fun", NULL },
	{ str_notchanged, "HUSTR_24", "level 24: the chasm", NULL },
	{ str_notchanged, "HUSTR_25", "level 25: bloodfalls", NULL },
	{ str_notchanged, "HUSTR_26", "level 26: the abandoned mines", NULL },
	{ str_notchanged, "HUSTR_27", "level 27: monster condo", NULL },
	{ str_notchanged, "HUSTR_28", "level 28: the spirit world", NULL },
	{ str_notchanged, "HUSTR_29", "level 29: the living end", NULL },
	{ str_notchanged, "HUSTR_30", "level 30: icon of sin", NULL },
	{ str_notchanged, "HUSTR_31", "level 31: wolfenstein", NULL },
	{ str_notchanged, "HUSTR_32", "level 32: grosse", NULL },
	{ str_notchanged, "PHUSTR_1", "level 1: congo", NULL },
	{ str_notchanged, "PHUSTR_2", "level 2: well of souls", NULL },
	{ str_notchanged, "PHUSTR_3", "level 3: aztec", NULL },
	{ str_notchanged, "PHUSTR_4", "level 4: caged", NULL },
	{ str_notchanged, "PHUSTR_5", "level 5: ghost town", NULL },
	{ str_notchanged, "PHUSTR_6", "level 6: baron's lair", NULL },
	{ str_notchanged, "PHUSTR_7", "level 7: caughtyard", NULL },
	{ str_notchanged, "PHUSTR_8", "level 8: realm", NULL },
	{ str_notchanged, "PHUSTR_9", "level 9: abattoire", NULL },
	{ str_notchanged, "PHUSTR_10", "level 10: onslaught", NULL },
	{ str_notchanged, "PHUSTR_11", "level 11: hunted", NULL },
	{ str_notchanged, "PHUSTR_12", "level 12: speed", NULL },
	{ str_notchanged, "PHUSTR_13", "level 13: the crypt", NULL },
	{ str_notchanged, "PHUSTR_14", "level 14: genesis", NULL },
	{ str_notchanged, "PHUSTR_15", "level 15: the twilight", NULL },
	{ str_notchanged, "PHUSTR_16", "level 16: the omen", NULL },
	{ str_notchanged, "PHUSTR_17", "level 17: compound", NULL },
	{ str_notchanged, "PHUSTR_18", "level 18: neurosphere", NULL },
	{ str_notchanged, "PHUSTR_19", "level 19: nme", NULL },
	{ str_notchanged, "PHUSTR_20", "level 20: the death domain", NULL },
	{ str_notchanged, "PHUSTR_21", "level 21: slayer", NULL },
	{ str_notchanged, "PHUSTR_22", "level 22: impossible mission", NULL },
	{ str_notchanged, "PHUSTR_23", "level 23: tombstone", NULL },
	{ str_notchanged, "PHUSTR_24", "level 24: the final frontier", NULL },
	{ str_notchanged, "PHUSTR_25", "level 25: the temple of darkness", NULL },
	{ str_notchanged, "PHUSTR_26", "level 26: bunker", NULL },
	{ str_notchanged, "PHUSTR_27", "level 27: anti-christ", NULL },
	{ str_notchanged, "PHUSTR_28", "level 28: the sewers", NULL },
	{ str_notchanged, "PHUSTR_29", "level 29: odyssey of noises", NULL },
	{ str_notchanged, "PHUSTR_30", "level 30: the gateway of hell", NULL },
	{ str_notchanged, "PHUSTR_31", "level 31: cyberden", NULL },
	{ str_notchanged, "PHUSTR_32", "level 32: go 2 it", NULL },
	{ str_notchanged, "THUSTR_1", "level 1: system control", NULL },
	{ str_notchanged, "THUSTR_2", "level 2: human bbq", NULL },
	{ str_notchanged, "THUSTR_3", "level 3: power control", NULL },
	{ str_notchanged, "THUSTR_4", "level 4: wormhole", NULL },
	{ str_notchanged, "THUSTR_5", "level 5: hanger", NULL },
	{ str_notchanged, "THUSTR_6", "level 6: open season", NULL },
	{ str_notchanged, "THUSTR_7", "level 7: prison", NULL },
	{ str_notchanged, "THUSTR_8", "level 8: metal", NULL },
	{ str_notchanged, "THUSTR_9", "level 9: stronghold", NULL },
	{ str_notchanged, "THUSTR_10", "level 10: redemption", NULL },
	{ str_notchanged, "THUSTR_11", "level 11: storage facility", NULL },
	{ str_notchanged, "THUSTR_12", "level 12: crater", NULL },
	{ str_notchanged, "THUSTR_13", "level 13: nukage processing", NULL },
	{ str_notchanged, "THUSTR_14", "level 14: steel works", NULL },
	{ str_notchanged, "THUSTR_15", "level 15: dead zone", NULL },
	{ str_notchanged, "THUSTR_16", "level 16: deepest reaches", NULL },
	{ str_notchanged, "THUSTR_17", "level 17: processing area", NULL },
	{ str_notchanged, "THUSTR_18", "level 18: mill", NULL },
	{ str_notchanged, "THUSTR_19", "level 19: shipping/respawning", NULL },
	{ str_notchanged, "THUSTR_20", "level 20: central processing", NULL },
	{ str_notchanged, "THUSTR_21", "level 21: administration center", NULL },
	{ str_notchanged, "THUSTR_22", "level 22: habitat", NULL },
	{ str_notchanged, "THUSTR_23", "level 23: lunar mining project", NULL },
	{ str_notchanged, "THUSTR_24", "level 24: quarry", NULL },
	{ str_notchanged, "THUSTR_25", "level 25: baron's den", NULL },
	{ str_notchanged, "THUSTR_26", "level 26: ballistyx", NULL },
	{ str_notchanged, "THUSTR_27", "level 27: mount pain", NULL },
	{ str_notchanged, "THUSTR_28", "level 28: heck", NULL },
	{ str_notchanged, "THUSTR_29", "level 29: river styx", NULL },
	{ str_notchanged, "THUSTR_30", "level 30: last call", NULL },
	{ str_notchanged, "THUSTR_31", "level 31: pharaoh", NULL },
	{ str_notchanged, "THUSTR_32", "level 32: caribbean", NULL },
	{ str_notchanged, "HUSTR_TALKTOSELF1", "You mumble to yourself", NULL },
	{ str_notchanged, "HUSTR_TALKTOSELF2", "Who's there?", NULL },
	{ str_notchanged, "HUSTR_TALKTOSELF3", "You scare yourself", NULL },
	{ str_notchanged, "HUSTR_TALKTOSELF4", "You start to rave", NULL },
	{ str_notchanged, "HUSTR_TALKTOSELF5", "You've lost it...", NULL },
	{ str_notchanged, "HUSTR_MESSAGESENT", "[Message Sent]", NULL },
	{ str_notchanged, "AMSTR_FOLLOWON", "Follow Mode ON", NULL },
	{ str_notchanged, "AMSTR_FOLLOWOFF", "Follow Mode OFF", NULL },
	{ str_notchanged, "AMSTR_GRIDON", "Grid ON", NULL },
	{ str_notchanged, "AMSTR_GRIDOFF", "Grid OFF", NULL },
	{ str_notchanged, "AMSTR_MARKEDSPOT", "Marked Spot", NULL },
	{ str_notchanged, "AMSTR_MARKSCLEARED", "All Marks Cleared", NULL },
	{ str_notchanged, "STSTR_MUS", "Music Change", NULL },
	{ str_notchanged, "STSTR_NOMUS", "IMPOSSIBLE SELECTION", NULL },
	{ str_notchanged, "STSTR_DQDON", "Degreelessness Mode ON", NULL },
	{ str_notchanged, "STSTR_DQDOFF", "Degreelessness Mode OFF", NULL },
	{ str_notchanged, "STSTR_KFAADDED", "Very Happy Ammo Added", NULL },
	{ str_notchanged, "STSTR_FAADDED", "Ammo (no keys) Added", NULL },
	{ str_notchanged, "STSTR_NCON", "No Clipping Mode ON", NULL },
	{ str_notchanged, "STSTR_NCOFF", "No Clipping Mode OFF", NULL },
	{ str_notchanged, "STSTR_BEHOLD", "inVuln, Str, Inviso, Rad, Allmap, or Lite-amp", NULL },
	{ str_notchanged, "STSTR_BEHOLDX", "Power-up Toggled", NULL },
	{ str_notchanged, "STSTR_CHOPPERS", "... doesn't suck - GM", NULL },
	{ str_notchanged, "STSTR_CLEV", "Changing Level...\n", NULL },
	{ str_notchanged, "E1TEXT", "Once you beat the big badasses and\n"\
		"clean out the moon base you're supposed\n"\
		"to win, aren't you? Aren't you? Where's\n"\
		"your fat reward and ticket home? What\n"\
		"the hell is this? It's not supposed to\n"\
		"end this way!\n"\
		"\n" \
		"It stinks like rotten meat, but looks\n"\
		"like the lost Deimos base.  Looks like\n"\
		"you're stuck on The Shores of Hell.\n"\
		"The only way out is through.\n"\
		"\n"\
		"To continue the DOOM experience, play\n"\
		"The Shores of Hell and its amazing\n"\
		"sequel, Inferno!\n"
		, NULL },
	{ str_notchanged, "E2TEXT", "You've done it! The hideous cyber-\n"\
		"demon lord that ruled the lost Deimos\n"\
		"moon base has been slain and you\n"\
		"are triumphant! But ... where are\n"\
		"you? You clamber to the edge of the\n"\
		"moon and look down to see the awful\n"\
		"truth.\n" \
		"\n"\
		"Deimos floats above Hell itself!\n"\
		"You've never heard of anyone escaping\n"\
		"from Hell, but you'll make the bastards\n"\
		"sorry they ever heard of you! Quickly,\n"\
		"you rappel down to  the surface of\n"\
		"Hell.\n"\
		"\n" \
		"Now, it's on to the final chapter of\n"\
		"DOOM! -- Inferno."
		, NULL },
	{ str_notchanged, "E3TEXT", "The loathsome spiderdemon that\n"\
		"masterminded the invasion of the moon\n"\
		"bases and caused so much death has had\n"\
		"its ass kicked for all time.\n"\
		"\n"\
		"A hidden doorway opens and you enter.\n"\
		"You've proven too tough for Hell to\n"\
		"contain, and now Hell at last plays\n"\
		"fair -- for you emerge from the door\n"\
		"to see the green fields of Earth!\n"\
		"Home at last.\n" \
		"\n"\
		"You wonder what's been happening on\n"\
		"Earth while you were battling evil\n"\
		"unleashed. It's good that no Hell-\n"\
		"spawn could have come through that\n"\
		"door with you ..."
		, NULL },
	{ str_notchanged, "E4TEXT", "the spider mastermind must have sent forth\n"\
		"its legions of hellspawn before your\n"\
		"final confrontation with that terrible\n"\
		"beast from hell.  but you stepped forward\n"\
		"and brought forth eternal damnation and\n"\
		"suffering upon the horde as a true hero\n"\
		"would in the face of something so evil.\n"\
		"\n"\
		"besides, someone was gonna pay for what\n"\
		"happened to daisy, your pet rabbit.\n"\
		"\n"\
		"but now, you see spread before you more\n"\
		"potential pain and gibbitude as a nation\n"\
		"of demons run amok among our cities.\n"\
		"\n"\
		"next stop, hell on earth!"
		, NULL },
	{ str_notchanged, "C1TEXT", "YOU HAVE ENTERED DEEPLY INTO THE INFESTED\n" \
		"STARPORT. BUT SOMETHING IS WRONG. THE\n" \
		"MONSTERS HAVE BROUGHT THEIR OWN REALITY\n" \
		"WITH THEM, AND THE STARPORT'S TECHNOLOGY\n" \
		"IS BEING SUBVERTED BY THEIR PRESENCE.\n" \
		"\n"\
		"AHEAD, YOU SEE AN OUTPOST OF HELL, A\n" \
		"FORTIFIED ZONE. IF YOU CAN GET PAST IT,\n" \
		"YOU CAN PENETRATE INTO THE HAUNTED HEART\n" \
		"OF THE STARBASE AND FIND THE CONTROLLING\n" \
		"SWITCH WHICH HOLDS EARTH'S POPULATION\n" \
		"HOSTAGE."
		, NULL },
	{ str_notchanged, "C2TEXT", "YOU HAVE WON! YOUR VICTORY HAS ENABLED\n" \
		"HUMANKIND TO EVACUATE EARTH AND ESCAPE\n"\
		"THE NIGHTMARE.  NOW YOU ARE THE ONLY\n"\
		"HUMAN LEFT ON THE FACE OF THE PLANET.\n"\
		"CANNIBAL MUTATIONS, CARNIVOROUS ALIENS,\n"\
		"AND EVIL SPIRITS ARE YOUR ONLY NEIGHBORS.\n"\
		"YOU SIT BACK AND WAIT FOR DEATH, CONTENT\n"\
		"THAT YOU HAVE SAVED YOUR SPECIES.\n"\
		"\n"\
		"BUT THEN, EARTH CONTROL BEAMS DOWN A\n"\
		"MESSAGE FROM SPACE: \"SENSORS HAVE LOCATED\n"\
		"THE SOURCE OF THE ALIEN INVASION. IF YOU\n"\
		"GO THERE, YOU MAY BE ABLE TO BLOCK THEIR\n"\
		"ENTRY.  THE ALIEN BASE IS IN THE HEART OF\n"\
		"YOUR OWN HOME CITY, NOT FAR FROM THE\n"\
		"STARPORT.\" SLOWLY AND PAINFULLY YOU GET\n"\
		"UP AND RETURN TO THE FRAY."
		, NULL },
	{ str_notchanged, "C3TEXT", "YOU ARE AT THE CORRUPT HEART OF THE CITY,\n"\
		"SURROUNDED BY THE CORPSES OF YOUR ENEMIES.\n"\
		"YOU SEE NO WAY TO DESTROY THE CREATURES'\n"\
		"ENTRYWAY ON THIS SIDE, SO YOU CLENCH YOUR\n"\
		"TEETH AND PLUNGE THROUGH IT.\n"\
		"\n"\
		"THERE MUST BE A WAY TO CLOSE IT ON THE\n"\
		"OTHER SIDE. WHAT DO YOU CARE IF YOU'VE\n"\
		"GOT TO GO THROUGH HELL TO GET TO IT?"
		, NULL },
	{ str_notchanged, "C4TEXT", "THE HORRENDOUS VISAGE OF THE BIGGEST\n"\
		"DEMON YOU'VE EVER SEEN CRUMBLES BEFORE\n"\
		"YOU, AFTER YOU PUMP YOUR ROCKETS INTO\n"\
		"HIS EXPOSED BRAIN. THE MONSTER SHRIVELS\n"\
		"UP AND DIES, ITS THRASHING LIMBS\n"\
		"DEVASTATING UNTOLD MILES OF HELL'S\n"\
		"SURFACE.\n"\
		"\n"\
		"YOU'VE DONE IT. THE INVASION IS OVER.\n"\
		"EARTH IS SAVED. HELL IS A WRECK. YOU\n"\
		"WONDER WHERE BAD FOLKS WILL GO WHEN THEY\n"\
		"DIE, NOW. WIPING THE SWEAT FROM YOUR\n"\
		"FOREHEAD YOU BEGIN THE LONG TREK BACK\n"\
		"HOME. REBUILDING EARTH OUGHT TO BE A\n"\
		"LOT MORE FUN THAN RUINING IT WAS.\n"
		, NULL },
	{ str_notchanged, "C5TEXT", "CONGRATULATIONS, YOU'VE FOUND THE SECRET\n"\
		"LEVEL! LOOKS LIKE IT'S BEEN BUILT BY\n"\
		"HUMANS, RATHER THAN DEMONS. YOU WONDER\n"\
		"WHO THE INMATES OF THIS CORNER OF HELL\n"\
		"WILL BE."
		, NULL },
	{ str_notchanged, "C6TEXT", "CONGRATULATIONS, YOU'VE FOUND THE\n"\
		"SUPER SECRET LEVEL!  YOU'D BETTER\n"\
		"BLAZE THROUGH THIS ONE!\n"
		, NULL },
	{ str_notchanged, "P1TEXT", "You gloat over the steaming carcass of the\n"\
		"Guardian.  With its death, you've wrested\n"\
		"the Accelerator from the stinking claws\n"\
		"of Hell.  You relax and glance around the\n"\
		"room.  Damn!  There was supposed to be at\n"\
		"least one working prototype, but you can't\n"\
		"see it. The demons must have taken it.\n"\
		"\n"\
		"You must find the prototype, or all your\n"\
		"struggles will have been wasted. Keep\n"\
		"moving, keep fighting, keep killing.\n"\
		"Oh yes, keep living, too."
		, NULL },
	{ str_notchanged, "P2TEXT", "Even the deadly Arch-Vile labyrinth could\n"\
		"not stop you, and you've gotten to the\n"\
		"prototype Accelerator which is soon\n"\
		"efficiently and permanently deactivated.\n"\
		"\n"\
		"You're good at that kind of thing."
		, NULL },
	{ str_notchanged, "P3TEXT", "You've bashed and battered your way into\n"\
		"the heart of the devil-hive.  Time for a\n"\
		"Search-and-Destroy mission, aimed at the\n"\
		"Gatekeeper, whose foul offspring is\n"\
		"cascading to Earth.  Yeah, he's bad. But\n"\
		"you know who's worse!\n"\
		"\n"\
		"Grinning evilly, you check your gear, and\n"\
		"get ready to give the bastard a little Hell\n"\
		"of your own making!"
		, NULL },
	{ str_notchanged, "P4TEXT", "The Gatekeeper's evil face is splattered\n"\
		"all over the place.  As its tattered corpse\n"\
		"collapses, an inverted Gate forms and\n"\
		"sucks down the shards of the last\n"\
		"prototype Accelerator, not to mention the\n"\
		"few remaining demons.  You're done. Hell\n"\
		"has gone back to pounding bad dead folks \n"\
		"instead of good live ones.  Remember to\n"\
		"tell your grandkids to put a rocket\n"\
		"launcher in your coffin. If you go to Hell\n"\
		"when you die, you'll need it for some\n"\
		"final cleaning-up ..."
		, NULL },
	{ str_notchanged, "P5TEXT", "You've found the second-hardest level we\n"\
		"got. Hope you have a saved game a level or\n"\
		"two previous.  If not, be prepared to die\n"\
		"aplenty. For master marines only."
		, NULL },
	{ str_notchanged, "P6TEXT", "Betcha wondered just what WAS the hardest\n"\
		"level we had ready for ya?  Now you know.\n"\
		"No one gets out alive."
		, NULL },
	{ str_notchanged, "T1TEXT", "You've fought your way out of the infested\n"\
		"experimental labs.   It seems that UAC has\n"\
		"once again gulped it down.  With their\n"\
		"high turnover, it must be hard for poor\n"\
		"old UAC to buy corporate health insurance\n"\
		"nowadays..\n"\
		"\n"\
		"Ahead lies the military complex, now\n"\
		"swarming with diseased horrors hot to get\n"\
		"their teeth into you. With luck, the\n"\
		"complex still has some warlike ordnance\n"\
		"laying around."
		, NULL },
	{ str_notchanged, "T2TEXT", "You hear the grinding of heavy machinery\n"\
		"ahead.  You sure hope they're not stamping\n"\
		"out new hellspawn, but you're ready to\n"\
		"ream out a whole herd if you have to.\n"\
		"They might be planning a blood feast, but\n"\
		"you feel about as mean as two thousand\n"\
		"maniacs packed into one mad killer.\n"\
		"\n"\
		"You don't plan to go down easy."
		, NULL },
	{ str_notchanged, "T3TEXT", "The vista opening ahead looks real damn\n"\
		"familiar. Smells familiar, too -- like\n"\
		"fried excrement. You didn't like this\n"\
		"place before, and you sure as hell ain't\n"\
		"planning to like it now. The more you\n"\
		"brood on it, the madder you get.\n"\
		"Hefting your gun, an evil grin trickles\n"\
		"onto your face. Time to take some names."
		, NULL },
	{ str_notchanged, "T4TEXT", "Suddenly, all is silent, from one horizon\n"\
		"to the other. The agonizing echo of Hell\n"\
		"fades away, the nightmare sky turns to\n"\
		"blue, the heaps of monster corpses start \n"\
		"to evaporate along with the evil stench \n"\
		"that filled the air. Jeeze, maybe you've\n"\
		"done it. Have you really won?\n"\
		"\n"\
		"Something rumbles in the distance.\n"\
		"A blue light begins to glow inside the\n"\
		"ruined skull of the demon-spitter."
		, NULL },
	{ str_notchanged, "T5TEXT", "What now? Looks totally different. Kind\n"\
		"of like King Tut's condo. Well,\n"\
		"whatever's here can't be any worse\n"\
		"than usual. Can it?  Or maybe it's best\n"\
		"to let sleeping gods lie.."
		, NULL },
	{ str_notchanged, "T6TEXT", "Time for a vacation. You've burst the\n"\
		"bowels of hell and by golly you're ready\n"\
		"for a break. You mutter to yourself,\n"\
		"Maybe someone else can kick Hell's ass\n"\
		"next time around. Ahead lies a quiet town,\n"\
		"with peaceful flowing water, quaint\n"\
		"buildings, and presumably no Hellspawn.\n"\
		"\n"\
		"As you step off the transport, you hear\n"\
		"the stomp of a cyberdemon's iron shoe."
		, NULL },
	{ str_notchanged, "CC_ZOMBIE", "ZOMBIEMAN", NULL },
	{ str_notchanged, "CC_SHOTGUN", "SHOTGUN GUY", NULL },
	{ str_notchanged, "CC_HEAVY", "HEAVY WEAPON DUDE", NULL },
	{ str_notchanged, "CC_IMP", "IMP", NULL },
	{ str_notchanged, "CC_DEMON", "DEMON", NULL },
	{ str_notchanged, "CC_LOST", "LOST SOUL", NULL },
	{ str_notchanged, "CC_CACO", "CACODEMON", NULL },
	{ str_notchanged, "CC_HELL", "HELL KNIGHT", NULL },
	{ str_notchanged, "CC_BARON", "BARON OF HELL", NULL },
	{ str_notchanged, "CC_ARACH", "ARACHNOTRON", NULL },
	{ str_notchanged, "CC_PAIN", "PAIN ELEMENTAL", NULL },
	{ str_notchanged, "CC_REVEN", "REVENANT", NULL },
	{ str_notchanged, "CC_MANCU", "MANCUBUS", NULL },
	{ str_notchanged, "CC_ARCH", "ARCH-VILE", NULL },
	{ str_notchanged, "CC_SPIDER", "THE SPIDER MASTERMIND", NULL },
	{ str_notchanged, "CC_CYBER", "THE CYBERDEMON", NULL },
	{ str_notchanged, "CC_HERO", "OUR HERO", NULL },

	// [RH] New strings from BOOM
	{ str_notchanged, "PD_BLUEC", "You need a blue card to open this door", NULL },
	{ str_notchanged, "PD_REDC", "You need a red card to open this door", NULL },
	{ str_notchanged, "PD_YELLOWC", "You need a yellow card to open this door", NULL },
	{ str_notchanged, "PD_BLUES", "You need a blue skull to open this door", NULL },
	{ str_notchanged, "PD_REDS", "You need a red skull to open this door", NULL },
	{ str_notchanged, "PD_YELLOWS", "You need a yellow skull to open this door", NULL },
	{ str_notchanged, "PD_ANY", "Any key will open this door", NULL },
	{ str_notchanged, "PD_ALL3", "You need all three keys to open this door", NULL },
	{ str_notchanged, "PD_ALL6", "You need all six keys to open this door", NULL },

	// [RH] Obituary strings
	{ str_notchanged, "OB_SUICIDE", "suicides", NULL },
	{ str_notchanged, "OB_FALLING", "fell too far", NULL },
	{ str_notchanged, "OB_CRUSH", "was squished", NULL },
	{ str_notchanged, "OB_EXIT", "tried to leave", NULL },
	{ str_notchanged, "OB_WATER", "can't swim", NULL },
	{ str_notchanged, "OB_SLIME", "mutated", NULL },
	{ str_notchanged, "OB_LAVA", "melted", NULL },
	{ str_notchanged, "OB_BARREL", "went boom", NULL },
	{ str_notchanged, "OB_SPLASH", "stood in the wrong spot", NULL },
	{ str_notchanged, "OB_R_SPLASH", "should have stood back", NULL },
	{ str_notchanged, "OB_ROCKET", "should have stood back", NULL },
	{ str_notchanged, "OB_KILLEDSELF", "killed %hself", NULL },
	{ str_notchanged, "OB_STEALTHBABY", "thought %g saw an arachnotron", NULL },
	{ str_notchanged, "OB_STEALTHVILE", "thought %g saw an archvile", NULL },
	{ str_notchanged, "OB_STEALTHBARON", "thought %g saw a Baron of Hell", NULL },
	{ str_notchanged, "OB_STEALTHCACO", "thought %g saw a cacodemon", NULL },
	{ str_notchanged, "OB_STEALTHCHAINGUY", "thought %g saw a chaingunner", NULL },
	{ str_notchanged, "OB_STEALTHDEMON", "thought %g saw a demon", NULL },
	{ str_notchanged, "OB_STEALTHKNIGHT", "thought %g saw a Hell Knight", NULL },
	{ str_notchanged, "OB_STEALTHIMP", "thought %g saw an imp", NULL },
	{ str_notchanged, "OB_STEALTHFATSO", "thought %g saw a mancubus", NULL },
	{ str_notchanged, "OB_STEALTHUNDEAD", "thought %g saw a revenant", NULL },
	{ str_notchanged, "OB_STEALTHSHOTGUY", "thought %g saw a sargeant", NULL },
	{ str_notchanged, "OB_STEALTHZOMBIE", "thought %g saw a zombieman", NULL },
	{ str_notchanged, "OB_UNDEADHIT", "was punched by a revenant", NULL },
	{ str_notchanged, "OB_IMPHIT", "was slashed by an imp", NULL },
	{ str_notchanged, "OB_CACOHIT", "got too close to a cacodemon", NULL },
	{ str_notchanged, "OB_DEMONHIT", "was bit by a demon", NULL },
	{ str_notchanged, "OB_SPECTREHIT", "was eaten by a spectre", NULL },
	{ str_notchanged, "OB_BARONHIT", "was ripped open by a Baron of Hell", NULL },
	{ str_notchanged, "OB_KNIGHTHIT", "was gutted by a Hell Knight", NULL },
	{ str_notchanged, "OB_ZOMBIE", "was killed by a zombieman", NULL },
	{ str_notchanged, "OB_SHOTGUY", "was shot by a sargeant", NULL },
	{ str_notchanged, "OB_VILE", "was incinerated by an archvile", NULL },
	{ str_notchanged, "OB_UNDEAD", "couldn't evade a revevant's fireball", NULL },
	{ str_notchanged, "OB_FATSO", "was squashed by a mancubus", NULL },
	{ str_notchanged, "OB_CHAINGUY", "was perforated by a chaingunner", NULL },
	{ str_notchanged, "OB_SKULL", "was spooked by a lost soul", NULL },
	{ str_notchanged, "OB_IMP", "was burned by an imp", NULL },
	{ str_notchanged, "OB_CACO", "was smitten by a cacodemon", NULL },
	{ str_notchanged, "OB_BARON", "was bruised by a Baron of Hell", NULL },
	{ str_notchanged, "OB_KNIGHT", "was splayed by a Hell Knight", NULL },
	{ str_notchanged, "OB_SPIDER", "stood in awe of the spider demon", NULL },
	{ str_notchanged, "OB_BABY", "let an arachnotron get %h", NULL },
	{ str_notchanged, "OB_CYBORG", "was splattered by a cyberdemon", NULL },
	{ str_notchanged, "OB_WOLFSS", "met a Nazi", NULL },
	{ str_notchanged, "OB_MPFIST", "chewed on %s's fist", NULL },
	{ str_notchanged, "OB_MPCHAINSAW", "was mowed over by %s's chainsaw", NULL },
	{ str_notchanged, "OB_MPPISTOL", "was tickled by %s's pea shooter", NULL },
	{ str_notchanged, "OB_MPSHOTGUN", "chewed on %s's boomstick", NULL },
	{ str_notchanged, "OB_MPSSHOTGUN", "was splattered by %s's super shotgun", NULL },
	{ str_notchanged, "OB_MPCHAINGUN", "was mowed down by %s's chaingun", NULL },
	{ str_notchanged, "OB_MPROCKET", "rode %s's rocket", NULL },
	{ str_notchanged, "OB_MPR_SPLASH", "almost dodged %s's rocket", NULL },
	{ str_notchanged, "OB_MPPLASMARIFLE", "was melted by %s's plasma gun", NULL },
	{ str_notchanged, "OB_MPBFG_BOOM", "was splintered by %s's BFG", NULL },
	{ str_notchanged, "OB_MPBFG_SPLASH", "couldn't hide from %s's BFG", NULL },
	{ str_notchanged, "OB_MPTELEFRAG", "was telefragged by %s", NULL },
	{ str_notchanged, "OB_DEFAULT", "died", NULL },
	{ str_notchanged, "OB_FRIENDLY1", "mows down a teammate", NULL },
	{ str_notchanged, "OB_FRIENDLY2", "checks %p glasses", NULL },
	{ str_notchanged, "OB_FRIENDLY3", "gets a frag for the other team", NULL },
	{ str_notchanged, "OB_FRIENDLY4", "loses another friend", NULL },
	{ str_notchanged, "OB_RAILGUN", "was railed by %s", NULL },
	{ str_notchanged, "SAVEGAMENAME", "zdoomsv", NULL },
	{ str_notchanged, "STARTUP1", "", NULL },
	{ str_notchanged, "STARTUP2", "", NULL },
	{ str_notchanged, "STARTUP3", "", NULL },
	{ str_notchanged, "STARTUP4", "", NULL },
	{ str_notchanged, "STARTUP5", "", NULL },
};

void D_InitStrings (void)
{
	int i;

	for (i = 0; i < NUMSTRINGS; i++) {
		if (Strings[i].type == str_notchanged)
			ReplaceString (&Strings[i].string, Strings[i].builtin);
	}

	endmsg[0] = QUITMSG;
}

void ReplaceString (char **ptr, char *str)
{
	if (*ptr)
	{
		if (*ptr == str)
			return;
		delete[] *ptr;
	}
	*ptr = copystring (str);
}