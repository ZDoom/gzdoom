#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>

#include "doomtype.h"
#include "doomstat.h"
#include "info.h"
#include "sounds.h"
#include "d_items.h"
#include "g_level.h"
#include "m_cheat.h"
#include "cmdlib.h"


// Miscellaneous info that used to be constant
int deh_StartHealth = 100;
int deh_StartBullets = 50;
int deh_MaxHealth = 100;
int deh_MaxArmor = 200;
int deh_GreenAC = 1;
int deh_BlueAC = 2;
int deh_MaxSoulsphere = 200;
int deh_SoulsphereHealth = 100;
int deh_MegasphereHealth = 200;
int deh_GodHealth = 100;
int deh_FAArmor = 200;
int deh_FAAC = 2;
int deh_KFAArmor = 200;
int deh_KFAAC = 2;
int deh_BFGCells = 40;
int deh_Infight;		// Just where is this used?


#define LINESIZE 2048

#define CHECKMODE(a,b)		if (!stricmp (Line1, (a))) cont = (b)(num);

#define CHECKKEY(a,b)		if (!stricmp (Line1, (a))) (b) = atoi(Line2);
#define CHECKCHEAT(a,b,c)	if (!stricmp (Line1, (a))) ChangeCheat (Line2, (b), (c));

static char *LineBuff, *Line1, *Line2;
static FILE *deh;
static int	 dversion, pversion;

static const char *unknown_str = "Unknown key %s encountered in %s %d.\n";

// This is an offset to be used for computing the text stuff.
// Straight from the DeHackEd source which was
// Written by Greg Lewis, gregl@umich.edu.
static int toff[] = {129044L, 129044L, 129044L, 129284L, 129380L};

// A conversion array to convert from the 448 code pointers to the 966
// Frames that exist.
// Again taken from the DeHackEd source.
static short codepconv[448] = {  1,    2,   3,   4,    6,   9,  10,   11,  12,  14,
							  16,   17,  18,  19,   20,  22,  29,   30,  31,  32,
							  33,	 34,  36,  38,	 39,  41,  43,	 44,  47,  48,
							  49,	50,  51,  52,	 53,  54,  55,	 56,  57,  58,
							  59,	60,  61,  62,	 63,  65,  66,	 67,  68,  69,
							  70,    71,  72,  73,	 74,  75,  76,	 77,  78,  79,
							  80,	 81,  82,  83,  84,  85,  86,	 87,  88,  89,
							 119,	127, 157, 159,	160, 166, 167,	174, 175, 176,
							 177,	178, 179, 180,	181, 182, 183,	184, 185, 188,
							 190,	191, 195, 196,	207, 208, 209,	210, 211, 212,
							 213,	214, 215, 216,	217, 218, 221,	223, 224, 228,
							 229,	241, 242, 243,	244, 245, 246,	247, 248, 249,
							 250,	251, 252, 253,	254, 255, 256,  257, 258, 259,
							 260,	261, 262, 263,	264, 270, 272,	273, 281, 282,
							 283,	284, 285, 286, 287, 288, 289,	290, 291, 292,
							 293,	294, 295, 296,	297, 298, 299,	300, 301, 302,
							 303,	304, 305, 306,	307, 308, 309,	310, 316, 317,
							 321,	322, 323, 324,	325, 326, 327,	328, 329, 330,
							 331,	332, 333, 334, 335, 336, 337,	338, 339, 340,
							 341, 342, 344, 347,	348, 362, 363,	364, 365, 366,
							 367, 368, 369, 370,	371, 372, 373,  374, 375, 376,
							 377, 378, 379, 380,	381, 382, 383,	384, 385, 387,
							 389, 390, 397, 406,	407, 408, 409,	410, 411, 412,
							 413, 414, 415, 416,	417, 418, 419,	421, 423, 424,
							 430, 431, 442, 443,	444, 445, 446,	447, 448, 449,
							 450, 451, 452, 453,	454, 456, 458,	460, 463, 465,
							 475, 476, 477, 478,	479, 480, 481,	482, 483, 484,
							 485, 486, 487, 489,	491, 493, 502,	503, 504, 505,
							 506, 508, 511, 514,	527, 528, 529,	530, 531, 532,
							 533, 534, 535, 536,	537, 538, 539,	541, 543, 545,
							 548, 556, 557, 558,	559, 560, 561,	562, 563, 564,
							 565, 566, 567, 568,	570, 572, 574,	585, 586, 587,
							 588, 589, 590, 594,	596, 598, 601,	602, 603, 604,
							 605, 606, 607, 608,	609, 610, 611,	612, 613, 614,
							 615, 616, 617, 618,	620, 621, 622,	631, 632, 633,
							 635, 636, 637, 638,	639, 640, 641,	642, 643, 644,
							 645, 646, 647, 648,	650, 652, 653,	654, 659, 674,
							 675, 676, 677, 678,	679, 680, 681,	682, 683, 684,
							 685, 686, 687, 688,	689, 690, 692,	696, 700, 701,
							 702, 703, 704, 705,	706, 707, 708,	709, 710, 711,
							 713, 715, 718, 726,	727, 728, 729,	730, 731, 732,
							 733, 734, 735, 736,	737, 738, 739,	740, 741, 743,
							 745, 746, 750, 751,	766, 774, 777,	779, 780, 783,
							 784, 785, 786, 787,	788, 789, 790,	791, 792, 793,
							 794, 795, 796, 797,	798, 801, 809,	811};

boolean BackedUpData = false;
// This is the original data before it gets replaced by a patch.
char *OrgSfxNames[NUMSFX];
char *OrgSprNames[NUMSPRITES];
actionf_t OrgActionPtrs[NUMSTATES];


extern byte cheat_choppers_seq[11];
extern byte cheat_god_seq[6];
extern byte cheat_ammo_seq[6];
extern byte cheat_ammonokey_seq[5];
extern byte cheat_noclip_seq[11];
extern byte cheat_commercial_noclip_seq[7];
extern byte cheat_powerup_seq[7][10];
extern byte cheat_clev_seq[10];
extern byte cheat_mypos_seq[8];
extern byte cheat_amap_seq[5];


void BackUpData (void)
{
	int i;

	if (BackedUpData)
		return;

	for (i = 0; i < NUMSFX; i++)
		OrgSfxNames[i] = S_sfx[i].name;

	for (i = 0; i < NUMSPRITES; i++)
		OrgSprNames[i] = sprnames[i];

	for (i = 0; i < NUMSTATES; i++)
		OrgActionPtrs[i] = states[i].action;
}

void ChangeCheat (byte *newcheat, byte *cheatseq, boolean needsval)
{
	while (*cheatseq != 0xff && *cheatseq != 1 && *newcheat) {
		*cheatseq++ = SCRAMBLE(*newcheat);
		newcheat++;
	}

	if (needsval) {
		*cheatseq++ = 1;
		*cheatseq++ = 0;
		*cheatseq++ = 0;
	}

	*cheatseq = 0xff;
}

static boolean ReadChars (char **stuff, int size)
{
	char *str = *stuff;
	int character;

	if (!size) {
		*str = 0;
		return true;
	}

	do {
		character = fgetc (deh);
		if (character == EOF)
			return false;

		*str++ = character;
	} while (--size);

	*str = 0;
	return true;
}

int GetLine (void)
{
	char *line, *line2;

	do {
		while (line = fgets (LineBuff, LINESIZE, deh))
			if (line[0] != '#')		// Skip comment lines
				break;

		if (!line)
			return 0;

		Line1 = line;
		while (*Line1 && *Line1 < ' ')
			Line1++;
	} while (*Line1 == 0);			// Loop until we get a line with
									// more than just whitespace.
	line = strchr (Line1, '=');

	if (line) {					// We have an '=' in the input line
		line2 = line;
		while (--line2 >= Line1)
			if (*line2 > ' ')
				break;

		if (line2 < Line1)
			return 0;			// Nothing before '='

		*(line2 + 1) = 0;

		line++;
		while (*line && *line <= ' ')
			line++;

		if (*line == 0)
			return 0;			// Nothing after '='

		Line2 = line;

		return 1;
	} else {					// No '=' in input line
		line = Line1 + 1;
		while (*line > ' ')
			line++;				// Get beyond first word

		*line++ = 0;
		while (*line && *line <= ' ')
			line++;				// Skip white space

		if (*line == 0)
			return 0;			// No second word

		Line2 = line;

		return 2;
	}
}

int PatchThing (int thingNum)
{
	int result;
	mobjinfo_t *info, dummy;

	if (thingNum >= 1 && thingNum <= NUMMOBJTYPES) {
		info = &mobjinfo[thingNum - 1];
		DEVONLY (Printf, "Thing %d\n", thingNum, 0);
	} else {
		info = &dummy;
		Printf ("Thing %d out of range.\n", thingNum);
	}

	// Turn off transparency for the mobj, since original DOOM
	// didn't have any. If some is wanted, let the patch specify it.
	info->flags &= ~MF_TRANSLUCBITS;

	while ((result = GetLine ()) == 1) {
			 CHECKKEY ("ID #",					info->doomednum)
		else CHECKKEY ("Initial frame",			info->spawnstate)
		else CHECKKEY ("Hit points",			info->spawnhealth)
		else CHECKKEY ("First moving frame",	info->seestate)
		else CHECKKEY ("Alert sound",			info->seesound)
		else CHECKKEY ("Reaction time",			info->reactiontime)
		else CHECKKEY ("Attack sound",			info->attacksound)
		else CHECKKEY ("Injury frame",			info->painstate)
		else CHECKKEY ("Pain chance",			info->painchance)
		else CHECKKEY ("Pain sound",			info->painsound)
		else CHECKKEY ("Close attack frame",	info->meleestate)
		else CHECKKEY ("Far attack frame",		info->missilestate)
		else CHECKKEY ("Death frame",			info->deathstate)
		else CHECKKEY ("Exploding frame",		info->xdeathstate)
		else CHECKKEY ("Death sound",			info->deathsound)
		else CHECKKEY ("Speed",					info->speed)
		else CHECKKEY ("Width",					info->radius)
		else CHECKKEY ("Height",				info->height)
		else CHECKKEY ("Mass",					info->mass)
		else CHECKKEY ("Missile damage",		info->damage)
		else CHECKKEY ("Action sound",			info->activesound)
		else CHECKKEY ("Bits",					info->flags)
		else CHECKKEY ("Respawn frame",			info->raisestate)
		else Printf (unknown_str, Line1, "Thing", thingNum);
	}
	return result;
}

int PatchSound (int soundNum)
{
	int result;
	sfxinfo_t *info, dummy;
	int offset = 0;

	if (soundNum >= 1 && soundNum <= NUMSFX) {
		info = &S_sfx[soundNum];
		DEVONLY (Printf, "Sound %d\n", soundNum, 0);
	} else {
		info = &dummy;
		Printf ("Sound %d out of range.\n");
	}

	while ((result = GetLine ()) == 1) {
		if (!stricmp  ("Offset", Line1))
			offset = atoi (Line2);
		else CHECKKEY ("Zero/One",			info->singularity)
		else CHECKKEY ("Value",				info->priority)
		/*
		else CHECKKEY ("Zero 1",			info->link)
		else CHECKKEY ("Neg. One 1",		info->pitch)
		else CHECKKEY ("Neg. One 2",		info->volume)
		else CHECKKEY ("Zero 2",			info->data)
		else CHECKKEY ("Zero 3",			info->usefulness)
		else CHECKKEY ("Zero 4",			info->lumpnum)
		*/
		else Printf (unknown_str, Line1, "Sound", soundNum);
	}

	if (offset) {
		// Calculate offset from start of sound names
		offset -= toff[dversion] + 21076;

		if (offset <= 64)			// pistol .. bfg
			offset >>= 3;
		else if (offset <= 260)		// sawup .. oof
			offset = (offset + 4) >> 3;
		else						// telept .. skeatk
			offset = (offset + 8) >> 3;

		if (offset >= 0 && offset < NUMSFX) {
			S_sfx[soundNum].name = OrgSfxNames[offset + 1];
		} else {
			Printf ("Sound name %d out of range.\n", offset + 1);
		}
	}

	return result;
}

int PatchFrame (int frameNum)
{
	int result;
	state_t *info, dummy;

	if (frameNum >= 0 && frameNum < NUMSTATES) {
		info = &states[frameNum];
		DEVONLY (Printf, "Frame %d\n", frameNum, 0);
	} else {
		info = &dummy;
		Printf ("Frame %d out of range\n", frameNum);
	}

	while ((result = GetLine ()) == 1) {
			 CHECKKEY ("Sprite number",		info->sprite)
		else CHECKKEY ("Sprite subnumber",	info->frame)
		else CHECKKEY ("Duration",			info->tics)
		else CHECKKEY ("Next frame",		info->nextstate)
		else CHECKKEY ("Unknown 1",			info->misc1)
		else CHECKKEY ("Unknown 2",			info->misc2)
		else Printf (unknown_str, Line1, "Frame", frameNum);
	}
	return result;
}

int PatchSprite (int sprNum)
{
	int result;
	int offset = 0;

	if (sprNum >= 0 && sprNum < NUMSPRITES) {
		DEVONLY (Printf, "Sprite %d\n", sprNum, 0);
	} else {
		Printf ("Sprite %d out of range.\n", sprNum);
		sprNum = -1;
	}

	while ((result = GetLine ()) == 1) {
		if (!stricmp ("Offset", Line1))
			offset = atoi (Line2);
		else Printf (unknown_str, Line1, "Sprite", sprNum);
	}

	if (offset > 0 && sprNum != -1) {
		// Calculate offset from beginning of sprite names.
		offset = (offset - toff[dversion] - 22044) / 8;

		if (offset >= 0 && offset < NUMSPRITES) {
			sprnames[sprNum] = OrgSprNames[offset];
		} else {
			Printf ("Sprite name %d out of range.\n", offset);
		}
	}

	return result;
}

int PatchAmmo (int ammoNum)
{
	extern int clipammo[NUMAMMO];

	int result;
	int *max;
	int *per;
	int dummy;

	if (ammoNum >= 0 && ammoNum < NUMAMMO) {
		DEVONLY (Printf, "Ammo %d.\n", ammoNum, 0);
		max = &maxammo[ammoNum];
		per = &clipammo[ammoNum];
	} else {
		Printf ("Ammo %d out of range.\n", ammoNum);
		max = per = &dummy;
	}

	while ((result = GetLine ()) == 1) {
			 CHECKKEY ("Max ammo", *max)
		else CHECKKEY ("Per ammo", *per)
		else Printf (unknown_str, Line1, "Ammo", ammoNum);
	}

	return result;
}

int PatchWeapon (int weapNum)
{
	int result;
	weaponinfo_t *info, dummy;

	if (weapNum >= 0 && weapNum < NUMWEAPONS) {
		info = &weaponinfo[weapNum];
		DEVONLY (Printf, "Weapon %d\n", weapNum, 0);
	} else {
		info = &dummy;
		Printf ("Weapon %d out of range.\n", weapNum);
	}

	while ((result = GetLine ()) == 1) {
			 CHECKKEY ("Ammo type",			info->ammo)
		else CHECKKEY ("Deselect frame",	info->upstate)
		else CHECKKEY ("Select frame",		info->downstate)
		else CHECKKEY ("Bobbing frame",		info->readystate)
		else CHECKKEY ("Shooting frame",	info->atkstate)
		else CHECKKEY ("Firing frame",		info->flashstate)
		else Printf (unknown_str, Line1, "Weapon", weapNum);
	}
	return result;
}

int PatchPointer (int ptrNum)
{
	int result;

	if (ptrNum >= 0 && ptrNum < 448) {
		DEVONLY (Printf, "Pointer %d\n", ptrNum, 0);
	} else {
		Printf ("Pointer %d out of range.\n", ptrNum);
		ptrNum = -1;
	}

	while ((result = GetLine ()) == 1) {
		if ((ptrNum != -1) && (!stricmp (Line1, "Codep Frame")))
			states[codepconv[ptrNum]].action = OrgActionPtrs[atoi (Line2)];
		else Printf (unknown_str, Line1, "Pointer", ptrNum);
	}
	return result;
}

int PatchCheats (int dummy)
{
	int result;

	// idmus cheat is (at least temporarily) removed
	byte musEater[] = { 0xb2, 0x26, 0xb6, 0xae, 0xea, 1, 0, 0, 0xff };

	DEVONLY (Printf, "Cheats\n", 0, 0);

	while ((result = GetLine ()) == 1) {
			 CHECKCHEAT ("Change music",		musEater,					 true)
		else CHECKCHEAT ("Chainsaw",			cheat_choppers_seq,			 false)
		else CHECKCHEAT ("God mode",			cheat_god_seq,				 false)
		else CHECKCHEAT ("Ammo & Keys",			cheat_ammo_seq,				 false)
		else CHECKCHEAT ("Ammo",				cheat_ammonokey_seq,		 false)
		else CHECKCHEAT ("No Clipping 1",		cheat_noclip_seq,			 false)
		else CHECKCHEAT ("No Clipping 2",		cheat_commercial_noclip_seq, false)
		else CHECKCHEAT ("Invincibility",		cheat_powerup_seq[0],		 false)
		else CHECKCHEAT ("Berserk",				cheat_powerup_seq[1],		 false)
		else CHECKCHEAT ("Invisibility",		cheat_powerup_seq[2],		 false)
		else CHECKCHEAT ("Radiation Suit",		cheat_powerup_seq[3],		 false)
		else CHECKCHEAT ("Auto-map",			cheat_powerup_seq[4],		 false)
		else CHECKCHEAT ("Lite-Amp Goggles",	cheat_powerup_seq[5],		 false)
		else CHECKCHEAT ("BEHOLD menu",			cheat_powerup_seq[6],		 false)
		else CHECKCHEAT ("Level Warp",			cheat_clev_seq,				 true)
		else CHECKCHEAT ("Player Position",		cheat_mypos_seq,			 false)
		else CHECKCHEAT ("Map cheat",			cheat_amap_seq,				 false)
		else Printf ("Unknown cheat %s.\n", Line2);
	}
	return result;
}

int PatchMisc (int dummy)
{
	int result;
	gitem_t *item;

	DEVONLY (Printf, "Misc", 0, 0);

	while ((result = GetLine()) == 1) {
			 CHECKKEY ("Initial Health",		deh_StartHealth)
		else CHECKKEY ("Initial Bullets",		deh_StartBullets)
		else CHECKKEY ("Max Health",			deh_MaxHealth)
		else CHECKKEY ("Max Armor",				deh_MaxArmor)
		else CHECKKEY ("Green Armor Class",		deh_GreenAC)
		else CHECKKEY ("Blue Armor Class",		deh_BlueAC)
		else CHECKKEY ("Max Soulsphere",		deh_MaxSoulsphere)
		else CHECKKEY ("Soulsphere Health",		deh_SoulsphereHealth)
		else CHECKKEY ("Megasphere Health",		deh_MegasphereHealth)
		else CHECKKEY ("God Mode Health",		deh_GodHealth)
		else CHECKKEY ("IDFA Armor",			deh_FAArmor)
		else CHECKKEY ("IDFA Armor Class",		deh_FAAC)
		else CHECKKEY ("IDKFA Armor",			deh_KFAArmor)
		else CHECKKEY ("IDKFA Armor Class",		deh_KFAAC)
		else CHECKKEY ("BFG Cells/Shot",		deh_BFGCells)
		else CHECKKEY ("Monsters Infight",		deh_Infight)
		else Printf ("Unknown miscellaneous info %s.\n", Line2);
	}

	if (item = FindItem ("Basic Armor"))
		item->offset = deh_GreenAC;

	if (item = FindItem ("Mega Armor"))
		item->offset = deh_BlueAC;

	return result;
}

int PatchText (int oldSize)
{
	int newSize;
	char *oldStr;
	char *newStr;
	char *temp;
	boolean good;
	int result;
	int i;

	temp = COM_Parse (Line2);		// Skip old size, since we already have it
	if (!COM_Parse (temp)) {
		Printf ("Text chunk is missing size of new string.\n");
		return 2;
	}
	newSize = atoi (com_token);

	oldStr = malloc (oldSize + 1);
	newStr = malloc (newSize + 1);

	if (!oldStr || !newStr) {
		Printf ("Out of memory.\n");
		goto donewithtext;
	}

	good = ReadChars (&oldStr, oldSize);
	good += ReadChars (&newStr, newSize);

	if (!good) {
		free (newStr);
		free (oldStr);
		Printf ("Unexpected end-of-file.\n");
		return 0;
	}

	DEVONLY (Printf, "Searching for text:\n%s\n", oldStr, 0);
	good = false;


	// Search through sfx names
	for (i = 0; i < NUMSFX; i++) {
		if (!strcmp (S_sfx[i].name, oldStr)) {
			S_sfx[i].name = copystring (newStr);
			good = true;
			// Yes, we really need to check them all. A sound patch could
			// have created two sounds that point to the same name, and
			// stopping at the first would miss the change to the second.
		}
	}

	if (good)
		goto donewithtext;


	// Search through sprite names
	for (i = 0; i < NUMSPRITES; i++) {
		if (!strcmp (sprnames[i], oldStr)) {
			sprnames[i] = copystring (newStr);
			good = true;
			// See above.
		}
	}

	if (good)
		goto donewithtext;


	// Search through music names.
	// This is something of an even bigger hack
	// since I changed the way music is handled.
	if (oldSize < 7) {		// Music names are never >6 chars
		if (temp = malloc (oldSize + 3)) {
			level_info_t *info = LevelInfos;
			sprintf (temp, "d_%s", oldStr);

			while (info->level_name) {
				if (!strnicmp (info->music, temp, 8)) {
					good = true;
					strncpy (info->music, temp, 8);
				}
				info++;
			}

			free (temp);
		}
	}

	if (good)
		goto donewithtext;


	// Search through level names.
	// This is even hackier than the music changing code.
	if ( (oldStr[0] == 'E' && oldStr[2] == 'M' && oldStr[4] == ':' && oldStr[5] == ' ') ||
		!strnicmp (oldStr, "level ", 6) ) {

		temp = strchr (oldStr, ':') + 2;
		if (temp != (char *)2) {
			level_info_t *info = LevelInfos;

			while (info->level_name) {
				if (!stricmp (info->level_name, temp)) {
					info->level_name = copystring (newStr);
					good = true;
				}
				info++;
			}
		}
	}

	if (good)
		goto donewithtext;


	// Search through cluster entrance and exit texts.
	{
		cluster_info_t *info = ClusterInfos;

		while (info->cluster) {
			if (info->exittext && !stricmp (info->exittext, oldStr)) {
				info->exittext = copystring (newStr);
				good = true;
			} else if (info->entertext && !stricmp (info->entertext, oldStr)) {
				info->entertext = copystring (newStr);
				good = true;
			}
			info++;
		}
	}

	if (!good)
		DEVONLY (Printf, "   (Unmatched)\n", 0, 0);

donewithtext:
	if (newStr)
		free (newStr);
	if (oldStr)
		free (oldStr);

	// Fetch next identifier for main loop
	while ((result = GetLine ()) == 1)
		;

	return result;
}
	

void DoDehPatch (char *patchfile)
{
	int cont;

	BackUpData ();

	deh = fopen (patchfile, "r");
	if (!deh) {
		Printf ("Could not open DeHackEd patch \"%s\"\n", patchfile);
		return;
	}

	LineBuff = malloc (LINESIZE);
	if (!LineBuff) {
		fclose (deh);
		Printf ("No memory for DeHackEd line buffer\n");
		return;
	}

	dversion = pversion = -1;

	cont = 0;
	if (fgets (LineBuff, LINESIZE, deh)) {
		if (!strnicmp (LineBuff, "Patch File for DeHackEd v", 25)) {
			while ((cont = GetLine()) == 1) {
					 CHECKKEY ("Doom version", dversion)
				else CHECKKEY ("Patch format", pversion)
			}
		}
	}

	if (!cont || dversion == -1 || pversion == -1) {
		fclose (deh);
		free (LineBuff);
		Printf ("\"%s\" is not a DeHackEd patch file\n");
		return;
	}

	if (pversion != 6) {
		Printf ("DeHackEd patch version is %d.\nUnexpected results may occur.\n");
	}

	if (dversion == 16)
		dversion = 0;
	else if (dversion == 17)
		dversion = 2;
	else if (dversion == 19)
		dversion = 3;
	else if (dversion == 20)
		dversion = 1;
	else if (dversion == 21)
		dversion = 4;
	else {
		Printf ("Patch created with unknown DOOM version.\nAssuming version 1.9.\n");
		dversion = 3;
	}

	do {
		if (cont == 1) {
			Printf ("Key %s encountered out of context\n", Line1);
			break;
		} else if (cont == 2) {
			int num = atoi (Line2);

				 CHECKMODE ("Thing",	PatchThing)
			else CHECKMODE ("Sound",	PatchSound)
			else CHECKMODE ("Frame",	PatchFrame)
			else CHECKMODE ("Sprite",	PatchSprite)
			else CHECKMODE ("Ammo",		PatchAmmo)
			else CHECKMODE ("Weapon",	PatchWeapon)
			else CHECKMODE ("Pointer",	PatchPointer)
			else CHECKMODE ("Cheat",	PatchCheats)
			else CHECKMODE ("Misc",		PatchMisc)
			else CHECKMODE ("Text",		PatchText)
			else {
				// Handle unknown or unimplemented data
				Printf ("Unknown chunk %s %s encountered. Skipping.\n", Line1, Line2);
				do {
					cont = GetLine ();
				} while (cont == 1);
			}
		}
	} while (cont);

	fclose (deh);
	free (LineBuff);
	Printf ("Patch installed\n");
}