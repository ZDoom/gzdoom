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
#include "d_proto.h"
#include "m_argv.h"
#include "w_wad.h"
#include "z_zone.h"
#include "cmdlib.h"
#include "g_level.h"

cvar_t *var_language;

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
	{ 0, "Development mode ON.\n", NULL },
	{ 0, "CD-ROM Version: default.cfg from c:\\doomdata\n", NULL },
	{ 0, "press a key.", NULL },
	{ 0, "press y or n.", NULL },
	{ 0, "are you sure you want to\nquit this great game?", NULL },
	{ 0, "you can't do load while in a net game!\n\npress a key.", NULL },
	{ 0, "you can't quickload during a netgame!\n\npress a key.", NULL },
	{ 0, "you haven't picked a quicksave slot yet!\n\npress a key.", NULL },
	{ 0, "you can't save if you aren't playing!\n\npress a key.", NULL },
	{ 0, "quicksave over your game named\n\n'%s'?\n\npress y or n.", NULL },
	{ 0, "do you want to quickload the game named\n\n'%s'?\n\npress y or n.", NULL },
	{ 0, "you can't start a new game\nwhile in a network game.\n\npress a key.", NULL },
	{ 0, "are you sure? this skill level\nisn't even remotely fair.\n\npress y or n.", NULL },
	{ 0, "this is the shareware version of doom.\n\nyou need to order the entire trilogy.\n\npress a key.", NULL },
	{ 0, "Messages OFF", NULL },
	{ 0, "Messages ON", NULL },
	{ 0, "you can't end a netgame!\n\npress a key.", NULL },
	{ 0, "are you sure you want to end the game?\n\npress y or n.", NULL },
	{ 0, "(press y to quit)", NULL },
	{ 0, "empty slot", NULL },
	{ 0, "Picked up the armor.", NULL },
	{ 0, "Picked up the MegaArmor!", NULL },
	{ 0, "Picked up a health bonus.", NULL },
	{ 0, "Picked up an armor bonus.", NULL },
	{ 0, "Picked up a stimpack.", NULL },
	{ 0, "Picked up a medikit that you REALLY need!", NULL },
	{ 0, "Picked up a medikit.", NULL },
	{ 0, "Supercharge!", NULL },
	{ 0, "Picked up a blue keycard.", NULL },
	{ 0, "Picked up a yellow keycard.", NULL },
	{ 0, "Picked up a red keycard.", NULL },
	{ 0, "Picked up a blue skull key.", NULL },
	{ 0, "Picked up a yellow skull key.", NULL },
	{ 0, "Picked up a red skull key.", NULL },
	{ 0, "Invulnerability!", NULL },
	{ 0, "Berserk!", NULL },
	{ 0, "Partial Invisibility", NULL },
	{ 0, "Radiation Shielding Suit", NULL },
	{ 0, "Computer Area Map", NULL },
	{ 0, "Light Amplification Visor", NULL },
	{ 0, "MegaSphere!", NULL },
	{ 0, "Picked up a clip.", NULL },
	{ 0, "Picked up a box of bullets.", NULL },
	{ 0, "Picked up a rocket.", NULL },
	{ 0, "Picked up a box of rockets.", NULL },
	{ 0, "Picked up an energy cell.", NULL },
	{ 0, "Picked up an energy cell pack.", NULL },
	{ 0, "Picked up 4 shotgun shells.", NULL },
	{ 0, "Picked up a box of shotgun shells.", NULL },
	{ 0, "Picked up a backpack full of ammo!", NULL },
	{ 0, "You got the BFG9000!  Oh, yes.", NULL },
	{ 0, "You got the chaingun!", NULL },
	{ 0, "A chainsaw!  Find some meat!", NULL },
	{ 0, "You got the rocket launcher!", NULL },
	{ 0, "You got the plasma gun!", NULL },
	{ 0, "You got the shotgun!", NULL },
	{ 0, "You got the super shotgun!", NULL },
	{ 0, "You need a blue key to activate this object", NULL },
	{ 0, "You need a red key to activate this object", NULL },
	{ 0, "You need a yellow key to activate this object", NULL },
	{ 0, "You need a blue key to open this door", NULL },
	{ 0, "You need a red key to open this door", NULL },
	{ 0, "You need a yellow key to open this door", NULL },
	{ 0, "game saved.", NULL },
	{ 0, "[Message unsent]", NULL },
	{ 0, "E1M1: Hangar", NULL },
	{ 0, "E1M2: Nuclear Plant", NULL },
	{ 0, "E1M3: Toxin Refinery", NULL },
	{ 0, "E1M4: Command Control", NULL },
	{ 0, "E1M5: Phobos Lab", NULL },
	{ 0, "E1M6: Central Processing", NULL },
	{ 0, "E1M7: Computer Station", NULL },
	{ 0, "E1M8: Phobos Anomaly", NULL },
	{ 0, "E1M9: Military Base", NULL },
	{ 0, "E2M1: Deimos Anomaly", NULL },
	{ 0, "E2M2: Containment Area", NULL },
	{ 0, "E2M3: Refinery", NULL },
	{ 0, "E2M4: Deimos Lab", NULL },
	{ 0, "E2M5: Command Center", NULL },
	{ 0, "E2M6: Halls of the Damned", NULL },
	{ 0, "E2M7: Spawning Vats", NULL },
	{ 0, "E2M8: Tower of Babel", NULL },
	{ 0, "E2M9: Fortress of Mystery", NULL },
	{ 0, "E3M1: Hell Keep", NULL },
	{ 0, "E3M2: Slough of Despair", NULL },
	{ 0, "E3M3: Pandemonium", NULL },
	{ 0, "E3M4: House of Pain", NULL },
	{ 0, "E3M5: Unholy Cathedral", NULL },
	{ 0, "E3M6: Mt. Erebus", NULL },
	{ 0, "E3M7: Limbo", NULL },
	{ 0, "E3M8: Dis", NULL },
	{ 0, "E3M9: Warrens", NULL },
	{ 0, "E4M1: Hell Beneath", NULL },
	{ 0, "E4M2: Perfect Hatred", NULL },
	{ 0, "E4M3: Sever The Wicked", NULL },
	{ 0, "E4M4: Unruly Evil", NULL },
	{ 0, "E4M5: They Will Repent", NULL },
	{ 0, "E4M6: Against Thee Wickedly", NULL },
	{ 0, "E4M7: And Hell Followed", NULL },
	{ 0, "E4M8: Unto The Cruel", NULL },
	{ 0, "E4M9: Fear", NULL },
	{ 0, "level 1: entryway", NULL },
	{ 0, "level 2: underhalls", NULL },
	{ 0, "level 3: the gantlet", NULL },
	{ 0, "level 4: the focus", NULL },
	{ 0, "level 5: the waste tunnels", NULL },
	{ 0, "level 6: the crusher", NULL },
	{ 0, "level 7: dead simple", NULL },
	{ 0, "level 8: tricks and traps", NULL },
	{ 0, "level 9: the pit", NULL },
	{ 0, "level 10: refueling base", NULL },
	{ 0, "level 11: 'o' of destruction!", NULL },
	{ 0, "level 12: the factory", NULL },
	{ 0, "level 13: downtown", NULL },
	{ 0, "level 14: the inmost dens", NULL },
	{ 0, "level 15: industrial zone", NULL },
	{ 0, "level 16: suburbs", NULL },
	{ 0, "level 17: tenements", NULL },
	{ 0, "level 18: the courtyard", NULL },
	{ 0, "level 19: the citadel", NULL },
	{ 0, "level 20: gotcha!", NULL },
	{ 0, "level 21: nirvana", NULL },
	{ 0, "level 22: the catacombs", NULL },
	{ 0, "level 23: barrels o' fun", NULL },
	{ 0, "level 24: the chasm", NULL },
	{ 0, "level 25: bloodfalls", NULL },
	{ 0, "level 26: the abandoned mines", NULL },
	{ 0, "level 27: monster condo", NULL },
	{ 0, "level 28: the spirit world", NULL },
	{ 0, "level 29: the living end", NULL },
	{ 0, "level 30: icon of sin", NULL },
	{ 0, "level 31: wolfenstein", NULL },
	{ 0, "level 32: grosse", NULL },
	{ 0, "level 1: congo", NULL },
	{ 0, "level 2: well of souls", NULL },
	{ 0, "level 3: aztec", NULL },
	{ 0, "level 4: caged", NULL },
	{ 0, "level 5: ghost town", NULL },
	{ 0, "level 6: baron's lair", NULL },
	{ 0, "level 7: caughtyard", NULL },
	{ 0, "level 8: realm", NULL },
	{ 0, "level 9: abattoire", NULL },
	{ 0, "level 10: onslaught", NULL },
	{ 0, "level 11: hunted", NULL },
	{ 0, "level 12: speed", NULL },
	{ 0, "level 13: the crypt", NULL },
	{ 0, "level 14: genesis", NULL },
	{ 0, "level 15: the twilight", NULL },
	{ 0, "level 16: the omen", NULL },
	{ 0, "level 17: compound", NULL },
	{ 0, "level 18: neurosphere", NULL },
	{ 0, "level 19: nme", NULL },
	{ 0, "level 20: the death domain", NULL },
	{ 0, "level 21: slayer", NULL },
	{ 0, "level 22: impossible mission", NULL },
	{ 0, "level 23: tombstone", NULL },
	{ 0, "level 24: the final frontier", NULL },
	{ 0, "level 25: the temple of darkness", NULL },
	{ 0, "level 26: bunker", NULL },
	{ 0, "level 27: anti-christ", NULL },
	{ 0, "level 28: the sewers", NULL },
	{ 0, "level 29: odyssey of noises", NULL },
	{ 0, "level 30: the gateway of hell", NULL },
	{ 0, "level 31: cyberden", NULL },
	{ 0, "level 32: go 2 it", NULL },
	{ 0, "level 1: system control", NULL },
	{ 0, "level 2: human bbq", NULL },
	{ 0, "level 3: power control", NULL },
	{ 0, "level 4: wormhole", NULL },
	{ 0, "level 5: hanger", NULL },
	{ 0, "level 6: open season", NULL },
	{ 0, "level 7: prison", NULL },
	{ 0, "level 8: metal", NULL },
	{ 0, "level 9: stronghold", NULL },
	{ 0, "level 10: redemption", NULL },
	{ 0, "level 11: storage facility", NULL },
	{ 0, "level 12: crater", NULL },
	{ 0, "level 13: nukage processing", NULL },
	{ 0, "level 14: steel works", NULL },
	{ 0, "level 15: dead zone", NULL },
	{ 0, "level 16: deepest reaches", NULL },
	{ 0, "level 17: processing area", NULL },
	{ 0, "level 18: mill", NULL },
	{ 0, "level 19: shipping/respawning", NULL },
	{ 0, "level 20: central processing", NULL },
	{ 0, "level 21: administration center", NULL },
	{ 0, "level 22: habitat", NULL },
	{ 0, "level 23: lunar mining project", NULL },
	{ 0, "level 24: quarry", NULL },
	{ 0, "level 25: baron's den", NULL },
	{ 0, "level 26: ballistyx", NULL },
	{ 0, "level 27: mount pain", NULL },
	{ 0, "level 28: heck", NULL },
	{ 0, "level 29: river styx", NULL },
	{ 0, "level 30: last call", NULL },
	{ 0, "level 31: pharaoh", NULL },
	{ 0, "level 32: caribbean", NULL },
	{ 0, "You mumble to yourself", NULL },
	{ 0, "Who's there?", NULL },
	{ 0, "You scare yourself", NULL },
	{ 0, "You start to rave", NULL },
	{ 0, "You've lost it...", NULL },
	{ 0, "[Message Sent]", NULL },
	{ 0, "Follow Mode ON", NULL },
	{ 0, "Follow Mode OFF", NULL },
	{ 0, "Grid ON", NULL },
	{ 0, "Grid OFF", NULL },
	{ 0, "Marked Spot", NULL },
	{ 0, "All Marks Cleared", NULL },
	{ 0, "Music Change", NULL },
	{ 0, "IMPOSSIBLE SELECTION", NULL },
	{ 0, "Degreelessness Mode ON", NULL },
	{ 0, "Degreelessness Mode OFF", NULL },
	{ 0, "Very Happy Ammo Added", NULL },
	{ 0, "Ammo (no keys) Added", NULL },
	{ 0, "No Clipping Mode ON", NULL },
	{ 0, "No Clipping Mode OFF", NULL },
	{ 0, "inVuln, Str, Inviso, Rad, Allmap, or Lite-amp", NULL },
	{ 0, "Power-up Toggled", NULL },
	{ 0, "... doesn't suck - GM", NULL },
	{ 0, "Changing Level...\n", NULL },
	{ 0, "Once you beat the big badasses and\n"\
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
	{ 0, "You've done it! The hideous cyber-\n"\
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
	{ 0, "The loathsome spiderdemon that\n"\
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
	{ 0, "the spider mastermind must have sent forth\n"\
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
	{ 0, "YOU HAVE ENTERED DEEPLY INTO THE INFESTED\n" \
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
	{ 0, "YOU HAVE WON! YOUR VICTORY HAS ENABLED\n" \
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
	{ 0, "YOU ARE AT THE CORRUPT HEART OF THE CITY,\n"\
		"SURROUNDED BY THE CORPSES OF YOUR ENEMIES.\n"\
		"YOU SEE NO WAY TO DESTROY THE CREATURES'\n"\
		"ENTRYWAY ON THIS SIDE, SO YOU CLENCH YOUR\n"\
		"TEETH AND PLUNGE THROUGH IT.\n"\
		"\n"\
		"THERE MUST BE A WAY TO CLOSE IT ON THE\n"\
		"OTHER SIDE. WHAT DO YOU CARE IF YOU'VE\n"\
		"GOT TO GO THROUGH HELL TO GET TO IT?"
		, NULL },
	{ 0, "THE HORRENDOUS VISAGE OF THE BIGGEST\n"\
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
	{ 0, "CONGRATULATIONS, YOU'VE FOUND THE SECRET\n"\
		"LEVEL! LOOKS LIKE IT'S BEEN BUILT BY\n"\
		"HUMANS, RATHER THAN DEMONS. YOU WONDER\n"\
		"WHO THE INMATES OF THIS CORNER OF HELL\n"\
		"WILL BE."
		, NULL },
	{ 0, "CONGRATULATIONS, YOU'VE FOUND THE\n"\
		"SUPER SECRET LEVEL!  YOU'D BETTER\n"\
		"BLAZE THROUGH THIS ONE!\n"
		, NULL },
	{ 0, "You gloat over the steaming carcass of the\n"\
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
	{ 0, "Even the deadly Arch-Vile labyrinth could\n"\
		"not stop you, and you've gotten to the\n"\
		"prototype Accelerator which is soon\n"\
		"efficiently and permanently deactivated.\n"\
		"\n"\
		"You're good at that kind of thing."
		, NULL },
	{ 0, "You've bashed and battered your way into\n"\
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
	{ 0, "The Gatekeeper's evil face is splattered\n"\
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
	{ 0, "You've found the second-hardest level we\n"\
		"got. Hope you have a saved game a level or\n"\
		"two previous.  If not, be prepared to die\n"\
		"aplenty. For master marines only."
		, NULL },
	{ 0, "Betcha wondered just what WAS the hardest\n"\
		"level we had ready for ya?  Now you know.\n"\
		"No one gets out alive."
		, NULL },
	{ 0, "You've fought your way out of the infested\n"\
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
	{ 0, "You hear the grinding of heavy machinery\n"\
		"ahead.  You sure hope they're not stamping\n"\
		"out new hellspawn, but you're ready to\n"\
		"ream out a whole herd if you have to.\n"\
		"They might be planning a blood feast, but\n"\
		"you feel about as mean as two thousand\n"\
		"maniacs packed into one mad killer.\n"\
		"\n"\
		"You don't plan to go down easy."
		, NULL },
	{ 0, "The vista opening ahead looks real damn\n"\
		"familiar. Smells familiar, too -- like\n"\
		"fried excrement. You didn't like this\n"\
		"place before, and you sure as hell ain't\n"\
		"planning to like it now. The more you\n"\
		"brood on it, the madder you get.\n"\
		"Hefting your gun, an evil grin trickles\n"\
		"onto your face. Time to take some names."
		, NULL },
	{ 0, "Suddenly, all is silent, from one horizon\n"\
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
	{ 0, "What now? Looks totally different. Kind\n"\
		"of like King Tut's condo. Well,\n"\
		"whatever's here can't be any worse\n"\
		"than usual. Can it?  Or maybe it's best\n"\
		"to let sleeping gods lie.."
		, NULL },
	{ 0, "Time for a vacation. You've burst the\n"\
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
	{ 0, "ZOMBIEMAN", NULL },
	{ 0, "SHOTGUN GUY", NULL },
	{ 0, "HEAVY WEAPON DUDE", NULL },
	{ 0, "IMP", NULL },
	{ 0, "DEMON", NULL },
	{ 0, "LOST SOUL", NULL },
	{ 0, "CACODEMON", NULL },
	{ 0, "HELL KNIGHT", NULL },
	{ 0, "BARON OF HELL", NULL },
	{ 0, "ARACHNOTRON", NULL },
	{ 0, "PAIN ELEMENTAL", NULL },
	{ 0, "REVENANT", NULL },
	{ 0, "MANCUBUS", NULL },
	{ 0, "ARCH-VILE", NULL },
	{ 0, "THE SPIDER MASTERMIND", NULL },
	{ 0, "THE CYBERDEMON", NULL },
	{ 0, "OUR HERO", NULL }
};

void newlanguage (cvar_t *var)
{
	D_InitStrings ();
	G_SetLevelStrings ();
}

void D_InitStrings (void)
{
	byte *strings;
	char *str;
	char *languagename;
	int i;
	int chunksize;

	var_language->u.callback = newlanguage;
	languagename = var_language->string;

	if (languagename) {
		i = W_CheckNumForName ("LANGDATA");
		str = "";
		if (i != -1) {
			strings = W_CacheLumpNum (i, PU_CACHE);

			while (1) {
				str = ReadString (&strings);
				if (*str == 0)
					break;			// No more languages

				chunksize = ReadLong (&strings);
				if (!stricmp (str, languagename))
					break;

				strings += chunksize;
			}

			if (*str) {
				language = english + 1;
				for (i = 0; i < NUMSTRINGS; i++) {
					str = ReadString (&strings);
					chunksize -= strlen (str) + 1;
					if (chunksize < 0) {
						Printf ("Language \"%s\" is missing %d strings\n", languagename, NUMSTRINGS - i);
						languagename = NULL;
						break;
					}
					if (Strings[i].type == str_notchanged || Strings[i].type == str_foreign) {
						ReplaceString (&Strings[i].string, str);
						Strings[i].type = str_foreign;
					}
				}
			}

		}
		if (languagename && !stricmp (languagename, "English")) {
			languagename = NULL;
		} else if (*str == 0) {
			Printf ("Unknown language \"%s\"\n", languagename);
			languagename = NULL;
		}
	}

	if (!languagename) {
		for (i = 0; i < NUMSTRINGS; i++) {
			if (Strings[i].type == str_notchanged || Strings[i].type == str_foreign) {
				ReplaceString (&Strings[i].string, Strings[i].builtin);
				Strings[i].type = str_notchanged;
			}
		}
	}

	endmsg[0] = QUITMSG;
}

void ReplaceString (char **ptr, char *str)
{
	if (*ptr)
		free (*ptr);
	*ptr = copystring (str);
}