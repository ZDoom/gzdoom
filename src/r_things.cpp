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
//		Refresh of things, i.e. objects represented by sprites.
//
//-----------------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>

#include "templates.h"
#include "m_alloc.h"
#include "doomdef.h"
#include "m_swap.h"
#include "m_argv.h"
#include "i_system.h"
#include "w_wad.h"
#include "r_local.h"
#include "p_effect.h"
#include "c_console.h"
#include "c_cvars.h"
#include "c_dispatch.h"
#include "doomstat.h"
#include "v_video.h"
#include "sc_man.h"
#include "s_sound.h"
#include "sbar.h"
#include "gi.h"

extern fixed_t globaluclip, globaldclip;


#define MINZ			(2048*4)
#define BASEYCENTER 	(100)

EXTERN_CVAR (Bool, st_scale)
CVAR (Bool, r_drawfuzz, true, CVAR_ARCHIVE)


//
// Sprite rotation 0 is facing the viewer,
//	rotation 1 is one angle turn CLOCKWISE around the axis.
// This is not the same as the angle,
//	which increases counter clockwise (protractor).
//
fixed_t 		pspritexscale;
fixed_t			pspriteyscale;
fixed_t 		pspritexiscale;
fixed_t			sky1scale;			// [RH] Sky 1 scale factor
fixed_t			sky2scale;			// [RH] Sky 2 scale factor

static int		spriteshade;

TArray<WORD>	ParticlesInSubsec;

// constant arrays
//	used for psprite clipping and initializing clipping
short			negonearray[MAXWIDTH];
short			screenheightarray[MAXWIDTH];

#define MAX_SPRITE_FRAMES	29		// [RH] Macro-ized as in BOOM.


CVAR (Bool, r_drawplayersprites, true, 0)	// [RH] Draw player sprites?

//
// INITIALIZATION FUNCTIONS
//

// variables used to look up
//	and range check thing_t sprites patches
TArray<spritedef_t> sprites;
TArray<spriteframe_t> SpriteFrames;
size_t			NumStdSprites;		// The first x sprites that don't belong to skins.

spriteframe_t	sprtemp[MAX_SPRITE_FRAMES];
int 			maxframe;
char*			spritename;

// [RH] skin globals
FPlayerSkin		*skins;
size_t			numskins;
BYTE			OtherGameSkinRemap[256];

// [RH] particle globals
int				NumParticles;
int				ActiveParticles;
int				InactiveParticles;
particle_t		*Particles;

CVAR (Bool, r_particles, true, 0);


//
// R_InstallSpriteLump
// Local function for R_InitSprites.
//
// [RH] Removed checks for coexistance of rotation 0 with other
//		rotations and made it look more like BOOM's version.
//
static void R_InstallSpriteLump (int lump, unsigned frame, char rot, BOOL flipped)
{
	unsigned rotation;

	if (rot >= '0' && rot <= '9')
	{
		rotation = rot - '0';
	}
	else if (rot >= 'A')
	{
		rotation = rot - 'A' + 10;
	}
	else
	{
		rotation = 17;
	}

	if (frame >= MAX_SPRITE_FRAMES || rotation > 16)
		I_FatalError ("R_InstallSpriteLump: Bad frame characters in lump %i", lump);

	if ((int)frame > maxframe)
		maxframe = frame;
				
	if (rotation == 0)
	{
		// the lump should be used for all rotations
        // false=0, true=1, but array initialised to -1
        // allows doom to have a "no value set yet" boolean value!
		int r;

		for (r = 14; r >= 0; r -= 2)
		{
			if (sprtemp[frame].lump[r] == -1)
			{
				sprtemp[frame].lump[r] = (short)(lump);
				if (flipped)
				{
					sprtemp[frame].flip |= 1 << r;
				}
				sprtemp[frame].rotate = false;
			}
		}
	}
	else
	{
		if (rotation <= 8)
		{
			rotation = (rotation - 1) * 2;
		}
		else
		{
			rotation = (rotation - 9) * 2 + 1;
		}

		if (sprtemp[frame].lump[rotation] == -1)
		{
			// the lump is only used for one rotation
			sprtemp[frame].lump[rotation] = (short)(lump);
			if (flipped)
			{
				sprtemp[frame].flip |= 1 << rotation;
			}
			sprtemp[frame].rotate = true;
		}
	}
}


// [RH] Seperated out of R_InitSpriteDefs()
static void R_InstallSprite (int num)
{
	int frame;

	if (maxframe == -1)
	{
		sprites[num].numframes = 0;
		return;
	}

	maxframe++;
	
	for (frame = 0; frame < maxframe; frame++)
	{
		switch ((int)sprtemp[frame].rotate)
		{
		case -1:
			// no rotations were found for that frame at all
			I_FatalError ("R_InstallSprite: No patches found for %s frame %c", sprites[num].name, frame+'A');
			break;
			
		case 0:
			// only the first rotation is needed
			break;
					
		case 1:
			// must have all 8 frame pairs
			{
				int rot;

				for (rot = 0; rot < 8; ++rot)
				{
					if (sprtemp[frame].lump[rot*2+1] == -1)
					{
						sprtemp[frame].lump[rot*2+1] = sprtemp[frame].lump[rot*2];
						if (sprtemp[frame].flip & (1 << (rot*2)))
						{
							sprtemp[frame].flip |= 1 << (rot*2+1);
						}
					}
					if (sprtemp[frame].lump[rot*2] == -1)
					{
						sprtemp[frame].lump[rot*2] = sprtemp[frame].lump[rot*2+1];
						if (sprtemp[frame].flip & (1 << (rot*2+1)))
						{
							sprtemp[frame].flip |= 1 << (rot*2);
						}
					}

				}
				for (rot = 0; rot < 16; ++rot)
				{
					if (sprtemp[frame].lump[rot] == -1)
						I_FatalError ("R_InstallSprite: Sprite %c%c%c%c frame %c is missing rotations",
									  sprites[num].name[0],
									  sprites[num].name[1],
									  sprites[num].name[2],
									  sprites[num].name[3], frame+'A');
				}
			}
			break;
		}
	}
	
	// allocate space for the frames present and copy sprtemp to it
	sprites[num].numframes = maxframe;
	sprites[num].spriteframes = (WORD)SpriteFrames.Reserve (maxframe);
	memcpy (&SpriteFrames[sprites[num].spriteframes], sprtemp, maxframe*sizeof(spriteframe_t));
}


//
// R_InitSpriteDefs
// Pass a null terminated list of sprite names
//	(4 chars exactly) to be used.
// Builds the sprite rotation matrices to account
//	for horizontally flipped sprites.
// Will report an error if the lumps are inconsistant. 
// Only called at startup.
//
// Sprite lump names are 4 characters for the actor,
//	a letter for the frame, and a number for the rotation.
// A sprite that is flippable will have an additional
//	letter/number appended.
// The rotation character can be 0 to signify no rotations.
//
void R_InitSpriteDefs () 
{ 
	size_t i;
	int l;
	int intname;
	int start;
	int end;
				
	start = firstspritelump - 1;
	end = lastspritelump + 1;
		
	// scan all the lump names for each of the names,
	//	noting the highest frame letter.
	// Just compare 4 characters as ints
	for (i = 0; i < sprites.Size (); i++)
	{
		spritename = sprites[i].name;
		memset (sprtemp, -1, sizeof(sprtemp));
		for (int j = 0; j < MAX_SPRITE_FRAMES; ++j)
		{
			sprtemp[j].flip = 0;
		}
				
		maxframe = -1;
		intname = *(int *)spritename;
		
		// scan the lumps,
		//	filling in the frames for whatever is found
		for (l = lastspritelump; l >= firstspritelump; l--)
		{
			if (*(int *)lumpinfo[l].name == intname)
			{
				R_InstallSpriteLump (l, 
									 lumpinfo[l].name[4] - 'A',
									 lumpinfo[l].name[5],
									 false);

				if (lumpinfo[l].name[6])
					R_InstallSpriteLump (l,
									 lumpinfo[l].name[6] - 'A',
									 lumpinfo[l].name[7],
									 true);
			}
		}
		
		R_InstallSprite ((int)i);
	}
}

// [RH]
// R_InitSkins
// Reads in everything applicable to a skin. The skins should have already
// been counted and had their identifiers assigned to namespaces.
//
#define NUMSKINSOUNDS 17
static const char *skinsoundnames[NUMSKINSOUNDS][2] =
{ // The *painXXX sounds must be the first four
	{ "dsplpain",	"*pain100" },
	{ "dsplpain",	"*pain75" },
	{ "dsplpain",	"*pain50" },
	{ "dsplpain",	"*pain25" },
	{ "dsplpain",	"*poison" },

	{ "dsoof",		"*grunt" },
	{ "dsoof",		"*land" },

	{ "dspldeth",	"*death" },
	{ "dspldeth",	"*wimpydeath" },

	{ "dspdiehi",	"*xdeath" },
	{ "dspdiehi",	"*crazydeath" },

	{ "dsnoway",	"*usefail" },
	{ "dsnoway",	"*puzzfail" },

	{ "dsslop",		"*gibbed" },
	{ "dsslop",		"*splat" },

	{ "dspunch",	"*fist" },
	{ "dsjump",		"*jump" }
};

static int STACK_ARGS skinsorter (const void *a, const void *b)
{
	return stricmp (((FPlayerSkin *)a)->name, ((FPlayerSkin *)b)->name);
}

void R_InitSkins (void)
{
	WORD playersoundrefs[NUMSKINSOUNDS];
	spritedef_t temp;
	int sndlumps[NUMSKINSOUNDS];
	char key[65];
	int intname;
	size_t i;
	int j, k, base;
	int lastlump;
	int aliasid;

	key[sizeof(key)-1] = 0;
	i = 0;
	lastlump = 0;

	for (j = 0; j < NUMSKINSOUNDS; ++j)
	{
		playersoundrefs[j] = S_FindSound (skinsoundnames[j][1]);
	}

	while ((base = W_FindLump ("S_SKIN", &lastlump)) != -1)
	{
		// The player sprite has 23 frames. This means that the S_SKIN
		// marker needs a minimum of 23 lumps after it.
		if (base >= numlumps - 23 || base == -1)
			continue;

		i++;
		for (j = 0; j < NUMSKINSOUNDS; j++)
			sndlumps[j] = -1;
		skins[i].namespc = lumpinfo[base].namespc;

		SC_OpenLumpNum (base, "S_SKIN");
		intname = 0;

		// Data is stored as "key = data".
		while (SC_GetString ())
		{
			strncpy (key, sc_String, sizeof(key)-1);
			if (!SC_GetString() || sc_String[0] != '=')
			{
				Printf (PRINT_BOLD, "Bad format for skin %d: %s\n", i, key);
				break;
			}
			SC_GetString ();
			if (0 == stricmp (key, "name"))
			{
				strncpy (skins[i].name, sc_String, 16);
				for (j = 0; (size_t)j < i; j++)
				{
					if (stricmp (skins[i].name, skins[j].name) == 0)
					{
						sprintf (skins[i].name, "skin%d", i);
						Printf (PRINT_BOLD, "Skin %s duplicated as %s\n",
							skins[j].name, skins[i].name);
						break;
					}
				}
			}
			else if (0 == stricmp (key, "sprite"))
			{
				for (j = 3; j >= 0; j--)
					sc_String[j] = toupper (sc_String[j]);
				intname = *((int *)sc_String);
			}
			else if (0 == stricmp (key, "face"))
			{
				for (j = 2; j >= 0; j--)
					skins[i].face[j] = toupper (sc_String[j]);
			}
			else if (0 == stricmp (key, "gender"))
			{
				skins[i].gender = D_GenderToInt (sc_String);
			}
			else if (0 == stricmp (key, "scale"))
			{
				skins[i].scale = clamp ((int)(atof (sc_String) * 64), 1, 256) - 1;
			}
			else if (0 == stricmp (key, "game"))
			{
				if (stricmp (sc_String, "heretic"))
				{
					skins[i].game = GAME_Heretic;
					skins[i].range0start = 225;
					skins[i].range0end = 240;
				}
			}
			else if (key[0] == '*')
			{ // Player sound replacment (ZDoom extension)
				int lump = W_CheckNumForName (sc_String, skins[i].namespc);
				if (lump == -1)
				{
					lump = W_CheckNumForName (sc_String);
				}
				if (lump != -1)
				{
					if (stricmp (key, "*pain") == 0)
					{ // Replace all pain sounds in one go
						aliasid = S_AddPlayerSound (skins[i].name, skins[i].gender,
							playersoundrefs[0], lump);
						for (int l = 3; l > 0; --l)
						{
							S_AddPlayerSoundExisting (skins[i].name, skins[i].gender,
								playersoundrefs[l], aliasid);
						}
					}
					else
					{
						int sndref = S_FindSoundNoHash (key);
						if (sndref != 0)
						{
							S_AddPlayerSound (skins[i].name, skins[i].gender, sndref, lump);
						}
					}
				}
			}
			else
			{
				for (j = 0; j < NUMSKINSOUNDS; j++)
				{
					if (stricmp (key, skinsoundnames[j][0]) == 0)
					{
						sndlumps[j] = W_CheckNumForName (sc_String, skins[i].namespc);
						if (sndlumps[j] == -1)
						{ // Replacement not found, try finding it in the global namespace
							sndlumps[j] = W_CheckNumForName (sc_String);
						}
					}
				}
				//if (j == 8)
				//	Printf ("Funny info for skin %i: %s = %s\n", i, key, sc_String);
			}
		}

		if (skins[i].name[0] == 0)
			sprintf (skins[i].name, "skin%d", i);

		// Now collect the sprite frames for this skin. If the sprite name was not
		// specified, use whatever immediately follows the specifier lump.
		if (intname == 0)
		{
			intname = *(int *)(lumpinfo[base+1].name);
		}

		memset (sprtemp, -1, sizeof(sprtemp));
		for (k = 0; k < MAX_SPRITE_FRAMES; ++k)
		{
			sprtemp[k].flip = 0;
		}
		maxframe = -1;

		for (k = base + 1; lumpinfo[k].wadnum == lumpinfo[base].wadnum; k++)
		{
			if (*(int *)lumpinfo[k].name == intname)
			{
				R_InstallSpriteLump (k, 
									 lumpinfo[k].name[4] - 'A',
									 lumpinfo[k].name[5],
									 false);

				if (lumpinfo[k].name[6])
					R_InstallSpriteLump (k,
									 lumpinfo[k].name[6] - 'A',
									 lumpinfo[k].name[7],
									 true);
			}
		}

		if (maxframe <= 0)
		{
			Printf (PRINT_BOLD, "Skin %s (#%d) has no frames. Removing.\n", skins[i].name, i);
			if (i < numskins-1)
				memmove (&skins[i], &skins[i+1], sizeof(skins[0])*(numskins-i-1));
			i--;
			continue;
		}

		strncpy (temp.name, lumpinfo[base+1].name, 4);
		skins[i].sprite = (int)sprites.Push (temp);
		R_InstallSprite (skins[i].sprite);

		// Register any sounds this skin provides
		aliasid = 0;
		for (j = 0; j < NUMSKINSOUNDS; j++)
		{
			if (sndlumps[j] != -1)
			{
				if (j == 0 || sndlumps[j] != sndlumps[j-1])
				{
					aliasid = S_AddPlayerSound (skins[i].name, skins[i].gender,
						playersoundrefs[j], sndlumps[j]);
				}
				else
				{
					S_AddPlayerSoundExisting (skins[i].name, skins[i].gender,
						playersoundrefs[j], aliasid);
				}
			}
		}

		SC_Close ();

		// Make sure face prefix is a full 3 chars
		if (skins[i].face[1] == 0 || skins[i].face[2] == 0)
		{
			skins[i].face[0] = 0;
		}
	}

	if (numskins > 1)
	{ // The sound table may have changed, so rehash it.
		S_HashSounds ();
		S_ShrinkPlayerSoundLists ();
	}
}

// [RH] Find a skin by name
int R_FindSkin (const char *name)
{
	int min, max, mid;
	int lexx;

	if (stricmp ("base", name) == 0)
	{
		return 0;
	}

	min = 1;
	max = (int)numskins-1;

	while (min <= max)
	{
		mid = (min + max)/2;
		lexx = strnicmp (skins[mid].name, name, 16);
		if (lexx == 0)
			return mid;
		else if (lexx < 0)
			min = mid + 1;
		else
			max = mid - 1;
	}
	return 0;
}

// [RH] List the names of all installed skins
CCMD (skins)
{
	int i;

	for (i = 0; i < (int)numskins; i++)
		Printf ("% 3d %s\n", i, skins[i].name);
}

//
// GAME FUNCTIONS
//
int				MaxVisSprites;
vissprite_t 	*vissprites;
vissprite_t		*firstvissprite;
vissprite_t		*vissprite_p;
vissprite_t		*lastvissprite;
int 			newvissprite;

static void R_CreateSkinTranslation (const char *palname)
{
	const BYTE *otherPal = (BYTE *)W_MapLumpName (palname);
	const BYTE *pal_p = otherPal;

	for (int i = 0; i < 256; ++i)
	{
		OtherGameSkinRemap[i] = ColorMatcher.Pick (otherPal[0], otherPal[1], otherPal[2]);
		otherPal += 3;
	}

	W_UnMapLump (pal_p);
}


//
// R_InitSprites
// Called at program start.
//
void R_InitSprites ()
{
	byte rangestart, rangeend;
	int lump, lastlump;
	size_t i;

	for (i = 0; i < MAXWIDTH; i++)
	{
		negonearray[i] = 0;
	}

	MaxVisSprites = 128;	// [RH] This is the initial default value. It grows as needed.
	firstvissprite = vissprites = (vissprite_t *)Malloc (MaxVisSprites * sizeof(vissprite_t));
	lastvissprite = &vissprites[MaxVisSprites];

	if (gameinfo.gametype == GAME_Doom)
	{
		rangestart = 112;
		rangeend = 127;
	}
	else if (gameinfo.gametype == GAME_Heretic)
	{
		rangestart = 225;
		rangeend = 240;
	}
	else // Hexen - Not used, because we don't do skins with Hexen
	{
		rangestart = 146;
		rangeend = 163;
	}

	// [RH] Create a standard translation to map skins between Heretic and Doom
	if (gameinfo.gametype == GAME_Doom)
	{
		R_CreateSkinTranslation ("SPALHTIC");
	}
	else
	{
		R_CreateSkinTranslation ("SPALDOOM");
	}

	// [RH] Count the number of skins.
	numskins = 1;
	lastlump = 0;
	if (gameinfo.gametype != GAME_Hexen)
	{
		while ((lump = W_FindLump ("S_SKIN", &lastlump)) != -1)
		{
			numskins++;
		}
	}

	// [RH] Do some preliminary setup
	skins = new FPlayerSkin[numskins];
	memset (skins, 0, sizeof(*skins) * numskins);
	for (i = 0; i < numskins; i++)
	{ // Assume Doom skin by default
		skins[i].range0start = 112;
		skins[i].range0end = 127;
		skins[i].scale = 63;
		skins[i].game = GAME_Doom;
	}

	R_InitSpriteDefs ();
	NumStdSprites = sprites.Size();
	if (gameinfo.gametype != GAME_Hexen)
	{
		R_InitSkins ();		// [RH] Finish loading skin data
	}

	// [RH] Set up base skin
	strcpy (skins[0].name, "Base");
	skins[0].face[0] = 'S';
	skins[0].face[1] = 'T';
	skins[0].face[2] = 'F';
	skins[0].namespc = ns_global;
	skins[0].scale = GetDefaultByName ("DoomPlayer")->xscale;
	skins[0].game = gameinfo.gametype;
	if (gameinfo.gametype == GAME_Heretic)
	{
		skins[0].range0start = 225;
		skins[0].range0end = 240;
	}

	for (i = 0; i < sprites.Size (); i++)
	{
		if (memcmp (sprites[i].name, deh.PlayerSprite, 4) == 0)
		{
			skins[0].sprite = (int)i;
			break;
		}
	}

	// [RH] Sort the skins, but leave base as skin 0
	qsort (&skins[1], numskins-1, sizeof(FPlayerSkin), skinsorter);
}



//
// R_ClearSprites
// Called at frame start.
//
void R_ClearSprites (void)
{
	vissprite_p = firstvissprite;
}


//
// R_NewVisSprite
//
vissprite_t *R_NewVisSprite (void)
{
	if (vissprite_p == lastvissprite)
	{
		ptrdiff_t firstvisspritenum = firstvissprite - vissprites;
		ptrdiff_t prevvisspritenum = vissprite_p - vissprites;

		MaxVisSprites *= 2;
		vissprites = (vissprite_t *)Realloc (vissprites, MaxVisSprites * sizeof(vissprite_t));
		lastvissprite = &vissprites[MaxVisSprites];
		firstvissprite = &vissprites[firstvisspritenum];
		vissprite_p = &vissprites[prevvisspritenum];
		DPrintf ("MaxVisSprites increased to %d\n", MaxVisSprites);
	}
	
	vissprite_p++;
	return vissprite_p-1;
}


//
// R_DrawMaskedColumn
// Used for sprites and masked mid textures.
// Masked means: partly transparent, i.e. stored
//	in posts/runs of opaque pixels.
//
short*			mfloorclip;
short*			mceilingclip;

fixed_t 		spryscale;
fixed_t 		sprtopscreen;

bool			sprflipvert;

void R_DrawMaskedColumn (column_t *column)
{
	int top = -1;

	while (column->topdelta != 0xff)
	{
		if (column->topdelta <= top)
		{
			top += column->topdelta;
		}
		else
		{
			top = column->topdelta;
		}

		if (column->length == 0)
		{
			goto nextpost;
		}

		// calculate unclipped screen coordinates for post
		dc_yl = (sprtopscreen + spryscale * top) >> FRACBITS;
		dc_yh = (sprtopscreen + spryscale * (top + column->length) - FRACUNIT) >> FRACBITS;

		if (sprflipvert)
		{
			swap (dc_yl, dc_yh);
		}

		if (dc_yh >= mfloorclip[dc_x])
		{
			dc_yh = mfloorclip[dc_x] - 1;
		}
		if (dc_yl < mceilingclip[dc_x])
		{
			dc_yl = mceilingclip[dc_x];
		}

		if (dc_yl <= dc_yh)
		{
			if (sprflipvert)
			{
				dc_texturefrac = (dc_yl*dc_iscale) - (top << FRACBITS)
					- FixedMul (centeryfrac, dc_iscale) - dc_texturemid;
				const fixed_t maxfrac = column->length << FRACBITS;
				while (dc_texturefrac >= maxfrac)
				{
					if (++dc_yl > dc_yh)
						goto nextpost;
					dc_texturefrac += dc_iscale;
				}
				fixed_t endfrac = dc_texturefrac + (dc_yh-dc_yl)*dc_iscale;
				while (endfrac < 0)
				{
					if (--dc_yh < dc_yl)
						goto nextpost;
					endfrac -= dc_iscale;
				}
			}
			else
			{
				dc_texturefrac = dc_texturemid - (top << FRACBITS)
					+ (dc_yl*dc_iscale) - FixedMul (centeryfrac-FRACUNIT, dc_iscale);
				while (dc_texturefrac < 0)
				{
					if (++dc_yl > dc_yh)
						goto nextpost;
					dc_texturefrac += dc_iscale;
				}
				fixed_t endfrac = dc_texturefrac + (dc_yh-dc_yl)*dc_iscale;
				const fixed_t maxfrac = column->length << FRACBITS;
				while (endfrac >= maxfrac)
				{
					if (--dc_yh < dc_yl)
						goto nextpost;
					endfrac -= dc_iscale;
				}
			}
			dc_source = (byte *)column + 3;
			colfunc ();
		}
nextpost:
		column = (column_t *)((byte *)column + column->length + 4);
	}
}

void R_DrawMaskedColumn2 (column2_t *column)
{
	while (column->Length != 0)
	{
		const int length = column->Length;
		const int top = column->TopDelta;

		// calculate unclipped screen coordinates for post
		dc_yl = (sprtopscreen + spryscale * top) >> FRACBITS;
		dc_yh = (sprtopscreen + spryscale * (top + length) - FRACUNIT) >> FRACBITS;

		if (sprflipvert)
		{
			swap (dc_yl, dc_yh);
		}

		if (dc_yh >= mfloorclip[dc_x])
		{
			dc_yh = mfloorclip[dc_x] - 1;
		}
		if (dc_yl < mceilingclip[dc_x])
		{
			dc_yl = mceilingclip[dc_x];
		}

		if (dc_yl <= dc_yh)
		{
			if (sprflipvert)
			{
				dc_texturefrac = (dc_yl*dc_iscale) - (top << FRACBITS)
					- FixedMul (centeryfrac, dc_iscale) - dc_texturemid;
				const fixed_t maxfrac = length << FRACBITS;
				while (dc_texturefrac >= maxfrac)
				{
					if (++dc_yl > dc_yh)
						goto nextpost;
					dc_texturefrac += dc_iscale;
				}
				fixed_t endfrac = dc_texturefrac + (dc_yh-dc_yl)*dc_iscale;
				while (endfrac < 0)
				{
					if (--dc_yh < dc_yl)
						goto nextpost;
					endfrac -= dc_iscale;
				}
			}
			else
			{
				dc_texturefrac = dc_texturemid - (top << FRACBITS)
					+ (dc_yl*dc_iscale) - FixedMul (centeryfrac-FRACUNIT, dc_iscale);
				while (dc_texturefrac < 0)
				{
					if (++dc_yl > dc_yh)
						goto nextpost;
					dc_texturefrac += dc_iscale;
				}
				fixed_t endfrac = dc_texturefrac + (dc_yh-dc_yl)*dc_iscale;
				const fixed_t maxfrac = length << FRACBITS;
				while (endfrac >= maxfrac)
				{
					if (--dc_yh < dc_yl)
						goto nextpost;
					endfrac -= dc_iscale;
				}
			}
			dc_source = (byte *)column + 4;
			colfunc ();
		}
nextpost:
		column = (column2_t *)((byte *)column + length + 4);
	}
}

//
// R_DrawVisSprite
//	mfloorclip and mceilingclip should also be set.
//
void R_DrawVisSprite (vissprite_t *vis)
{
	fixed_t 		frac;
	const patch_t*	patch;
	int				x2, stop4;
	fixed_t			xiscale;
	ESPSResult		mode;

	dc_colormap = vis->colormap;

	mode = R_SetPatchStyle (vis->RenderStyle, vis->alpha, vis->Translation, vis->AlphaColor);

	if (mode != DontDraw)
	{
		if (mode == DoDraw0)
		{
			// One column at a time
			stop4 = vis->x1;
		}
		else	 // DoDraw1
		{
			// Up to four columns at a time
			stop4 = (vis->x2 + 1) & ~3;
		}

		patch = TileCache[R_CacheTileNum (vis->picnum)];
		spryscale = vis->yscale;
		sprflipvert = false;
		dc_iscale = 0xffffffffu / (unsigned)vis->yscale;
		dc_texturemid = vis->texturemid;
		frac = vis->startfrac;
		xiscale = vis->xiscale;

		sprtopscreen = centeryfrac - FixedMul (dc_texturemid, spryscale);

		dc_x = vis->x1;
		x2 = vis->x2 + 1;

		if (dc_x < x2)
		{
			while ((dc_x < stop4) && (dc_x & 3))
			{
				R_DrawMaskedColumn ((column_t *)((byte *)patch + LONG(patch->columnofs[frac>>FRACBITS])));
				dc_x++;
				frac += xiscale;
			}

			while (dc_x < stop4)
			{
				rt_initcols();
				for (int zz = 4; zz; --zz)
				{
					R_DrawMaskedColumnHoriz ((column_t *)((byte *)patch + LONG(patch->columnofs[frac>>FRACBITS])));
					dc_x++;
					frac += xiscale;
				}
				rt_draw4cols (dc_x - 4);
			}

			while (dc_x < x2)
			{
				R_DrawMaskedColumn ((column_t *)((byte *)patch + LONG(patch->columnofs[frac>>FRACBITS])));
				dc_x++;
				frac += xiscale;
			}
		}
	}

	R_FinishSetPatchStyle ();
}

//
// R_ProjectSprite
// Generates a vissprite for a thing if it might be visible.
//
void R_ProjectSprite (AActor *thing, int fakeside)
{
	fixed_t 			tr_x;
	fixed_t 			tr_y;
	
	fixed_t				gzt;				// killough 3/27/98
	fixed_t				gzb;				// [RH] use bottom of sprite, not actor
	fixed_t 			tx, tx2;
	fixed_t 			tz;

	fixed_t 			xscale;
	
	int 				x1;
	int 				x2;

	spritedef_t*		sprdef;
	spriteframe_t*		sprframe;
	int 				lump;
	
	WORD 				flip;
	
	vissprite_t*		vis;
	
	fixed_t 			iscale;

	sector_t*			heightsec;			// killough 3/27/98

	if ((thing->renderflags & RF_INVISIBLE) ||
		thing->RenderStyle == STYLE_None ||
		(thing->RenderStyle >= STYLE_Translucent && thing->alpha <= 0))
	{
		return;
	}

	// transform the origin point
	tr_x = thing->x - viewx;
	tr_y = thing->y - viewy;

	tz = DMulScale20 (tr_x, viewtancos, tr_y, viewtansin);

	// thing is behind view plane?
	if (tz < MINZ)
		return;

	tx = DMulScale16 (tr_x, viewsin, -tr_y, viewcos);

	// [RH] Flip for mirrors
	if (MirrorFlags & RF_XFLIP)
	{
		tx = viewwidth - tx - 1;
	}
	tx2 = tx >> 4;

	// too far off the side?
	if ((abs (tx) >> 6) > tz)
	{
		return;
	}

	xscale = DivScale12 (centerxfrac, tz);

	// decide which patch to use for the sprite, based on angle to player
#ifdef RANGECHECK
	if ((unsigned)thing->sprite >= (unsigned)sprites.Size ())
	{
		DPrintf ("R_ProjectSprite: invalid sprite number %i\n", thing->sprite);
		return;
	}
#endif
	sprdef = &sprites[thing->sprite];
	if (thing->frame >= sprdef->numframes)
	{
		DPrintf ("R_ProjectSprite: invalid sprite frame %c%c%c%c: %c (max %c)\n",
			sprdef->name[0], sprdef->name[1], sprdef->name[2], sprdef->name[3],
			thing->frame + 'A', sprdef->numframes + 'A' - 1);
		return;
	}
	sprframe = &SpriteFrames[sprdef->spriteframes + thing->frame];

	if (sprframe->rotate)
	{
		// choose a different rotation based on player view
		angle_t ang = R_PointToAngle (thing->x, thing->y);
		unsigned rot = (ang-thing->angle+(unsigned)(ANG45/2)*9)>>28;
		lump = sprframe->lump[rot];
		flip = sprframe->flip & (1 << rot);
	}
	else
	{
		// use single rotation for all views
		lump = sprframe->lump[0];
		flip = sprframe->flip & 1;
	}

	if (TileSizes[lump].Width == 0xffff)
	{
		R_CacheTileNum (lump);	// [RH] get sprite's size
	}
	
	// [RH] Added scaling
	gzt = thing->z + (TileSizes[lump].TopOffset << (FRACBITS-6)) * (thing->yscale+1);
	gzb = thing->z + ((TileSizes[lump].TopOffset - TileSizes[lump].Height) << (FRACBITS-6)) * (thing->yscale+1);

	// [RH] Reject sprites that are off the top or bottom of the screen
	if (MulScale12 (globaluclip, tz) > viewz - gzb ||
		MulScale12 (globaldclip, tz) < viewz - gzt)
	{
		return;
	}

	// calculate edges of the shape
	const fixed_t thingxscalemul = (thing->xscale+1) << (16-6);

	tx -= TileSizes[lump].LeftOffset * thingxscalemul;
	x1 = centerx + MulScale32 (tx, xscale);

	// off the right side?
	if (x1 > WindowRight)
		return;

	tx += TileSizes[lump].Width * thingxscalemul;
	x2 = centerx + MulScale32 (tx, xscale);

	// off the left side or too small?
	if (x2 < WindowLeft || x2 <= x1)
		return;

	xscale = MulScale6 (thing->xscale+1, xscale);
	iscale = (TileSizes[lump].Width << FRACBITS) / (x2 - x1);
	x2--;

	// killough 3/27/98: exclude things totally separated
	// from the viewer, by either water or fake ceilings
	// killough 4/11/98: improve sprite clipping for underwater/fake ceilings

	heightsec = thing->Sector->heightsec;

	if (heightsec != NULL && heightsec->MoreFlags & SECF_IGNOREHEIGHTSEC)
	{
		heightsec = NULL;
	}

	if (heightsec)	// only clip things which are in special sectors
	{
		if (fakeside == FAKED_AboveCeiling)
		{
			if (gzt < heightsec->ceilingplane.ZatPoint (thing->x, thing->y))
				return;
		}
		else if (fakeside == FAKED_BelowFloor)
		{
			if (gzb >= heightsec->floorplane.ZatPoint (thing->x, thing->y))
				return;
		}
		else
		{
			if (gzt < heightsec->floorplane.ZatPoint (thing->x, thing->y))
				return;
			if (gzb >= heightsec->ceilingplane.ZatPoint (thing->x, thing->y))
				return;
		}
	}

	// [RH] Flip for mirrors and renderflags
	if ((MirrorFlags ^ thing->renderflags) & RF_XFLIP)
	{
		flip = !flip;
	}

	// store information in a vissprite
	vis = R_NewVisSprite ();

	// killough 3/27/98: save sector for special clipping later
	vis->heightsec = heightsec;

	vis->renderflags = thing->renderflags;
	vis->RenderStyle = thing->RenderStyle;
	vis->AlphaColor = thing->alphacolor;
	vis->xscale = xscale;
	vis->yscale = Scale (InvZtoScale, (thing->yscale+1)<<6, tz);
	vis->depth = tz;
	vis->cx = tx2;
	vis->gx = thing->x;
	vis->gy = thing->y;
	vis->gz = gzb;		// [RH] use gzb, not thing->z
	vis->gzt = gzt;		// killough 3/27/98
	vis->floorclip = SafeDivScale6 (thing->floorclip, (thing->yscale+1));
	vis->texturemid = (TileSizes[lump].TopOffset << FRACBITS)
		- SafeDivScale6 (viewz-thing->z+thing->floorclip, (thing->yscale+1));
	vis->x1 = x1 < WindowLeft ? WindowLeft : x1;
	vis->x2 = x2 > WindowRight ? WindowRight : x2;
	vis->Translation = thing->Translation;		// [RH] thing translation table
	vis->FakeFlatStat = fakeside;
	vis->alpha = thing->alpha;
	vis->picnum = lump;

	if (flip)
	{
		vis->startfrac = (TileSizes[lump].Width << FRACBITS) - 1;
		vis->xiscale = -iscale;
	}
	else
	{
		vis->startfrac = 0;
		vis->xiscale = iscale;
	}

	if (vis->x1 > x1)
		vis->startfrac += vis->xiscale*(vis->x1-x1);
	
	// get light level
	if (fixedlightlev)
	{
		vis->colormap = basecolormap + fixedlightlev;
	}
	else if (fixedcolormap)
	{
		// fixed map
		vis->colormap = fixedcolormap;
	}
	else if (!foggy && (thing->renderflags & RF_FULLBRIGHT))
	{
		// full bright
		vis->colormap = basecolormap;	// [RH] Use basecolormap
	}
	else
	{
		// diminished light
		vis->colormap = basecolormap + (GETPALOOKUP (
			(fixed_t)DivScale12 (r_SpriteVisibility, tz), spriteshade) << COLORMAPSHIFT);
	}
}


//
// R_AddSprites
// During BSP traversal, this adds sprites by sector.
//
// killough 9/18/98: add lightlevel as parameter, fixing underwater lighting
// [RH] Save which side of heightsec sprite is on here.
void R_AddSprites (sector_t *sec, int lightlevel, int fakeside)
{
	AActor *thing;

	// BSP is traversed by subsector.
	// A sector might have been split into several
	//	subsectors during BSP building.
	// Thus we check whether it was already added.
	if (sec->thinglist == NULL || sec->validcount == validcount)
		return;

	// Well, now it will be done.
	sec->validcount = validcount;

	spriteshade = LIGHT2SHADE(lightlevel + r_actualextralight);

	// Handle all things in sector.
	for (thing = sec->thinglist; thing; thing = thing->snext)
	{
		R_ProjectSprite (thing, fakeside);
	}
}


//
// R_DrawPSprite
//
void R_DrawPSprite (pspdef_t* psp, AActor *owner)
{
	fixed_t 			tx;
	int 				x1;
	int 				x2;
	spritedef_t*		sprdef;
	spriteframe_t*		sprframe;
	int 				lump;
	BOOL 				flip;
	vissprite_t*		vis;
	vissprite_t 		avis;
	
	// decide which patch to use
#ifdef RANGECHECK
	if ( (unsigned)psp->state->sprite.index >= (unsigned)sprites.Size ())
	{
		DPrintf ("R_DrawPSprite: invalid sprite number %i\n", psp->state->sprite.index);
		return;
	}
#endif
	sprdef = &sprites[psp->state->sprite.index];
#ifdef RANGECHECK
	if (psp->state->GetFrame() >= sprdef->numframes)
	{
		DPrintf ("R_DrawPSprite: invalid sprite frame %i : %i\n", psp->state->sprite.index, psp->state->GetFrame());
		return;
	}
#endif
	sprframe = &SpriteFrames[sprdef->spriteframes + psp->state->GetFrame()];

	lump = sprframe->lump[0];
	flip = (BOOL)sprframe->flip & 1;

	if (TileSizes[lump].Width == 0xffff)
	{
		R_CacheTileNum (lump);
	}

	// calculate edges of the shape
	tx = psp->sx-((320/2)<<FRACBITS);
		
	tx -= TileSizes[lump].LeftOffset << FRACBITS;
	x1 = (centerxfrac + FixedMul (tx, pspritexscale)) >>FRACBITS;

	// off the right side
	if (x1 > viewwidth)
		return; 

	tx += TileSizes[lump].Width << FRACBITS;
	x2 = ((centerxfrac + FixedMul (tx, pspritexscale)) >>FRACBITS) - 1;

	// off the left side
	if (x2 < 0)
		return;
	
	// store information in a vissprite
	vis = &avis;
	vis->renderflags = owner->renderflags;
	vis->floorclip = 0;
	vis->texturemid = (BASEYCENTER<<FRACBITS) -
		(psp->sy - (TileSizes[lump].TopOffset << FRACBITS));
	if (camera->player && (RenderTarget != screen ||
		realviewheight == RenderTarget->GetHeight() ||
		(RenderTarget->GetWidth() > 320 && !st_scale)))
	{	// Adjust PSprite for fullscreen views
		FWeaponInfo *weapon;
		if (camera->player->powers[pw_weaponlevel2])
			weapon = wpnlev2info[camera->player->readyweapon];
		else
			weapon = wpnlev1info[camera->player->readyweapon];
		if (weapon && weapon->yadjust)
		{
			if (RenderTarget != screen || realviewheight == RenderTarget->GetHeight())
			{
				vis->texturemid -= weapon->yadjust;
			}
			else
			{
				vis->texturemid -= FixedMul (StatusBar->GetDisplacement (),
					weapon->yadjust);
			}
		}
	}
	vis->x1 = x1 < 0 ? 0 : x1;
	vis->x2 = x2 >= viewwidth ? viewwidth-1 : x2;
	vis->xscale = pspritexscale;
	vis->yscale = pspriteyscale;
	vis->Translation = 0;		// [RH] Use default colors
	vis->alpha = owner->alpha;
	vis->RenderStyle = owner->RenderStyle;
	vis->picnum = lump;

	if (flip)
	{
		vis->xiscale = -pspritexiscale;
		vis->startfrac = (TileSizes[lump].Width << FRACBITS) - 1;
	}
	else
	{
		vis->xiscale = pspritexiscale;
		vis->startfrac = 0;
	}

	if (vis->x1 > x1)
		vis->startfrac += vis->xiscale*(vis->x1-x1);

	if (fixedlightlev)
	{
		vis->colormap = basecolormap + fixedlightlev;
	}
	else if (fixedcolormap)
	{
		// fixed color
		vis->colormap = fixedcolormap;
	}
	else if (psp->state->GetFullbright())
	{
		// full bright
		vis->colormap = basecolormap;	// [RH] use basecolormap
	}
	else
	{
		// local light
		vis->colormap = basecolormap + (GETPALOOKUP (0, spriteshade) << COLORMAPSHIFT);
	}
	if (camera->player &&
		camera->player->powers[pw_invisibility] > 0 &&
		camera->player->powers[pw_invisibility] < 4*32 &&
		!(camera->player->powers[pw_invisibility] & 8))
	{
		// shadow draw
		vis->RenderStyle = STYLE_Normal;
	}
		
	R_DrawVisSprite (vis);
}



//
// R_DrawPlayerSprites
//
void R_DrawPlayerSprites (void)
{
	int 		i;
	int 		lightnum;
	pspdef_t*	psp;
	sector_t*	sec;
	static sector_t tempsec;
	int			floorlight, ceilinglight;
	
	if (!r_drawplayersprites ||
		!camera->player ||
		(players[consoleplayer].cheats & CF_CHASECAM))
		return;

	sec = R_FakeFlat (camera->Sector, &tempsec, &floorlight,
		&ceilinglight, false);

	// [RH] set foggy flag
	foggy = (level.fadeto || sec->ColorMap->Fade);
	r_actualextralight = foggy ? 0 : extralight << 4;

	// [RH] set basecolormap
	basecolormap = sec->ColorMap->Maps;

	// get light level
	lightnum = ((floorlight + ceilinglight) >> 1) + r_actualextralight;
	spriteshade = LIGHT2SHADE(lightnum) - 24*FRACUNIT;

	// clip to screen bounds
	mfloorclip = screenheightarray;
	mceilingclip = negonearray;

	if (camera->player != NULL)
	{
		fixed_t centerhack = centeryfrac;

		centery = viewheight >> 1;
		centeryfrac = centery << FRACBITS;

		// add all active psprites
		for (i = 0, psp = camera->player->psprites;
			 i < NUMPSPRITES;
			 i++, psp++)
		{
			if (psp->state)
				R_DrawPSprite (psp, camera);
		}

		centeryfrac = centerhack;
		centery = centerhack >> FRACBITS;
	}
}




//
// R_SortVisSprites
//
// [RH] The old code for this function used a bubble sort, which was far less
//		than optimal with large numbers of sprites. I changed it to use the
//		stdlib qsort() function instead, and now it is a *lot* faster; the
//		more vissprites that need to be sorted, the better the performance
//		gain compared to the old function.
//
static vissprite_t **spritesorter;
static int spritesortersize = 0;
static int vsprcount;

static int STACK_ARGS sv_compare (const void *arg1, const void *arg2)
{
	int diff = (*(vissprite_t **)arg1)->depth - (*(vissprite_t **)arg2)->depth;
	if (diff == 0)
		return (*(vissprite_t **)arg2)->gzt - (*(vissprite_t **)arg1)->gzt;
	return diff;
}

void R_SortVisSprites (void)
{
	int i;
	vissprite_t *spr;

	vsprcount = vissprite_p - firstvissprite;

	if (!vsprcount)
		return;

	if (spritesortersize < MaxVisSprites)
	{
		if (spritesorter != NULL)
			delete[] spritesorter;
		spritesorter = new vissprite_t *[MaxVisSprites];
		spritesortersize = MaxVisSprites;
	}

	for (i = 0, spr = firstvissprite; i < vsprcount; i++, spr++)
	{
		spritesorter[i] = spr;
	}

	qsort (spritesorter, vsprcount, sizeof (vissprite_t *), sv_compare);
}


//
// R_DrawSprite
//
void R_DrawSprite (vissprite_t *spr)
{
	static short clipbot[MAXWIDTH];
	static short cliptop[MAXWIDTH];
	drawseg_t *ds;
	int i;
	int r1, r2;
	short topclip, botclip;
	short *clip1, *clip2;

	// [RH] Check for particles
	if (spr->picnum == -1)
	{
		R_DrawParticle (spr);
		return;
	}

	// [RH] Quickly reject sprites with bad x ranges.
	if (spr->x1 > spr->x2)
		return;

	// [RH] Initialize the clipping arrays to their largest possible range
	// instead of using a special "not clipped" value. This eliminates
	// visual anomalies when looking down and should be faster, too.
	topclip = 0;
	botclip = viewheight;

	// killough 3/27/98:
	// Clip the sprite against deep water and/or fake ceilings.
	// [RH] rewrote this to be based on which part of the sector is really visible

	if (spr->heightsec &&
		!(spr->heightsec->MoreFlags & SECF_IGNOREHEIGHTSEC))
	{ // only things in specially marked sectors
		if (spr->FakeFlatStat != FAKED_AboveCeiling)
		{
			fixed_t h = spr->heightsec->floorplane.ZatPoint (spr->gx, spr->gy);
			h = (centeryfrac - FixedMul (h-viewz, spr->yscale)) >> FRACBITS;

			if (spr->FakeFlatStat == FAKED_BelowFloor)
			{ // seen below floor: clip top
				if (h > topclip)
				{
					topclip = MIN<short> (h, viewheight);
				}
			}
			else
			{ // seen in the middle: clip bottom
				if (h < botclip)
				{
					botclip = MAX<short> (0, h);
				}
			}
		}
		if (spr->FakeFlatStat != FAKED_BelowFloor)
		{
			fixed_t h = spr->heightsec->ceilingplane.ZatPoint (spr->gx, spr->gy);
			h = (centeryfrac - FixedMul (h-viewz, spr->yscale)) >> FRACBITS;

			if (spr->FakeFlatStat == FAKED_AboveCeiling)
			{ // seen above ceiling: clip bottom
				if (h < botclip)
				{
					botclip = MAX<short> (0, h);
				}
			}
			else
			{ // seen in the middle: clip top
				if (h > topclip)
				{
					topclip = MIN<short> (h, viewheight);
				}
			}
		}
	}
	// killough 3/27/98: end special clipping for deep water / fake ceilings
	else if (spr->floorclip)
	{ // [RH] Move floorclip stuff from R_DrawVisSprite to here
		if (TileSizes[spr->picnum].Width == 0xffff)
		{
			R_CacheTileNum (spr->picnum);
		}
		int clip = ((centeryfrac - FixedMul (spr->texturemid -
			(TileSizes[spr->picnum].Height<<FRACBITS) +
			spr->floorclip, spr->yscale)) >> FRACBITS);
		if (clip < botclip)
		{
			botclip = MAX<short> (0, clip);
		}
	}

	i = spr->x2 - spr->x1 + 1;
	clip1 = clipbot + spr->x1;
	clip2 = cliptop + spr->x1;
	do
	{
		*clip1++ = botclip;
		*clip2++ = topclip;
	} while (--i);
	
	// Scan drawsegs from end to start for obscuring segs.
	// The first drawseg that is closer than the sprite is the clip seg.

	// Modified by Lee Killough:
	// (pointer check was originally nonportable
	// and buggy, by going past LEFT end of array):

	//		for (ds=ds_p-1 ; ds >= drawsegs ; ds--)    old buggy code

	for (ds = ds_p; ds-- > firstdrawseg; )  // new -- killough
	{
		// determine if the drawseg obscures the sprite
		if (ds->x1 > spr->x2 || ds->x2 < spr->x1 ||
			(!(ds->silhouette & SIL_BOTH) && ds->maskedtexturecol == -1 &&
			 !ds->bFogBoundary) )
		{
			// does not cover sprite
			continue;
		}

		r1 = MAX<int> (ds->x1, spr->x1);
		r2 = MIN<int> (ds->x2, spr->x2);

		if (ds->neardepth > spr->depth || (ds->fardepth > spr->depth &&
			// Check if sprite is in front of draw seg:
			DMulScale24 (spr->depth - ds->cy, ds->cdx,
						 ds->cdy, ds->cx - spr->cx) < 0))
		{
			// seg is behind sprite, so draw the mid texture if it has one
			if (ds->maskedtexturecol != -1 || ds->bFogBoundary)
				R_RenderMaskedSegRange (ds, r1, r2);
			continue;
		}

		// clip this piece of the sprite
		// killough 3/27/98: optimized and made much shorter
		// [RH] Optimized further (at least for VC++;
		// other compilers should be at least as good as before)

		if (ds->silhouette & SIL_BOTTOM) //bottom sil
		{
			clip1 = clipbot + r1;
			clip2 = openings + ds->sprbottomclip + r1 - ds->x1;
			i = r2 - r1 + 1;
			do
			{
				if (*clip1 > *clip2)
					*clip1 = *clip2;
				clip1++;
				clip2++;
			} while (--i);
		}

		if (ds->silhouette & SIL_TOP)   // top sil
		{
			clip1 = cliptop + r1;
			clip2 = openings + ds->sprtopclip + r1 - ds->x1;
			i = r2 - r1 + 1;
			do
			{
				if (*clip1 < *clip2)
					*clip1 = *clip2;
				clip1++;
				clip2++;
			} while (--i);
		}
	}

	// all clipping has been performed, so draw the sprite

	mfloorclip = clipbot;
	mceilingclip = cliptop;
	R_DrawVisSprite (spr);
}

//
// R_DrawMasked
//
void R_DrawMasked (void)
{
	drawseg_t *ds;
	int i;

	R_SortVisSprites ();

	for (i = vsprcount; i > 0; i--)
	{
		R_DrawSprite (spritesorter[i-1]);
	}

	// render any remaining masked mid textures

	// Modified by Lee Killough:
	// (pointer check was originally nonportable
	// and buggy, by going past LEFT end of array):

	//		for (ds=ds_p-1 ; ds >= drawsegs ; ds--)    old buggy code

	for (ds = ds_p; ds-- > firstdrawseg; )	// new -- killough
	{
		if (ds->maskedtexturecol != -1 || ds->bFogBoundary)
		{
			R_RenderMaskedSegRange (ds, ds->x1, ds->x2);
		}
	}
	
	// draw the psprites on top of everything
	//	but does not draw on side views
	if (!viewangleoffset)
	{
		R_DrawPlayerSprites ();
	}
}


//
// [RH] Particle functions
//

void R_InitParticles ()
{
	char *i;

	if ((i = Args.CheckValue ("-numparticles")))
		NumParticles = atoi (i);
	if (NumParticles == 0)
		NumParticles = 4000;
	else if (NumParticles < 100)
		NumParticles = 100;

	Particles = new particle_t[NumParticles * sizeof(particle_t)];
	R_ClearParticles ();
}

void R_ClearParticles ()
{
	int i;

	memset (Particles, 0, NumParticles * sizeof(particle_t));
	ActiveParticles = NO_PARTICLE;
	InactiveParticles = 0;
	for (i = 0; i < NumParticles-1; i++)
		Particles[i].tnext = i + 1;
	Particles[i].tnext = NO_PARTICLE;
}

// Group particles by subsectors. Because particles are always
// in motion, there is little benefit to caching this information
// from one frame to the next.

void R_FindParticleSubsectors ()
{
	if (ParticlesInSubsec.Size() < (size_t)numsubsectors)
	{
		ParticlesInSubsec.Reserve (numsubsectors - ParticlesInSubsec.Size());
	}

	clearbufshort (&ParticlesInSubsec[0], numsubsectors, NO_PARTICLE);

	if (!r_particles)
	{
		return;
	}
	for (WORD i = ActiveParticles; i != NO_PARTICLE; i = Particles[i].tnext)
	{
		subsector_t *ssec = R_PointInSubsector (Particles[i].x, Particles[i].y);
		int ssnum = ssec-subsectors;
		Particles[i].snext = ParticlesInSubsec[ssnum];
		ParticlesInSubsec[ssnum] = i;
	}
}

void R_ProjectParticle (particle_t *particle, const sector_t *sector, int shade, int fakeside)
{
	fixed_t 			tr_x;
	fixed_t 			tr_y;
	fixed_t 			tx, ty;
	fixed_t 			tz, tiz;
	fixed_t 			xscale, yscale;
	int 				x1, x2, y1, y2;
	vissprite_t*		vis;
	sector_t*			heightsec = NULL;
	byte*				map;

	// transform the origin point
	tr_x = particle->x - viewx;
	tr_y = particle->y - viewy;

	tz = DMulScale20 (tr_x, viewtancos, tr_y, viewtansin);

	// particle is behind view plane?
	if (tz < MINZ)
		return;

	tx = DMulScale20 (tr_x, viewsin, -tr_y, viewcos);

	// Flip for mirrors
	if (MirrorFlags & RF_XFLIP)
	{
		tx = viewwidth - tx - 1;
	}

	// too far off the side?
	if (tz <= abs (tx))
		return;

	tiz = 268435456 / tz;
	xscale = centerx * tiz;

	// calculate edges of the shape
	int psize = particle->size << (12-3);

	x1 = MAX<int> (WindowLeft, (centerxfrac + MulScale12 (tx-psize, xscale)) >> FRACBITS);
	x2 = MIN<int> (WindowRight, (centerxfrac + MulScale12 (tx+psize, xscale)) >> FRACBITS);

	if (x1 > x2)
		return;

	yscale = MulScale16 (yaspectmul, xscale);
	ty = particle->z - viewz;
	psize <<= 4;
	y1 = (centeryfrac - FixedMul (ty+psize, yscale)) >> FRACBITS;
	y2 = (centeryfrac - FixedMul (ty-psize, yscale)) >> FRACBITS;

	// Clip the particle now. Because it's a point and projected as its subsector is
	// entered, we don't need to clip it to drawsegs like a normal sprite.

	// Clip particles behind walls.
	if (y1 <  ceilingclip[x1])		y1 = ceilingclip[x1];
	if (y1 <  ceilingclip[x2-1])	y1 = ceilingclip[x2-1];
	if (y2 >= floorclip[x1])		y2 = floorclip[x1] - 1;
	if (y2 >= floorclip[x2-1])		y2 = floorclip[x2-1] - 1;

	if (y1 > y2)
		return;

	// Clip particles above the ceiling or below the floor.
	heightsec = sector->heightsec;

	if (heightsec != NULL && heightsec->MoreFlags & SECF_IGNOREHEIGHTSEC)
	{
		heightsec = NULL;
	}

	const secplane_t *topplane;
	const secplane_t *botplane;
	int toppic;
	int botpic;

	if (heightsec)	// only clip things which are in special sectors
	{
		if (fakeside == FAKED_AboveCeiling)
		{
			topplane = &sector->ceilingplane;
			botplane = &heightsec->ceilingplane;
			toppic = sector->ceilingpic;
			botpic = heightsec->ceilingpic;
			map = heightsec->ColorMap->Maps;
		}
		else if (fakeside == FAKED_BelowFloor)
		{
			topplane = &heightsec->floorplane;
			botplane = &sector->floorplane;
			toppic = heightsec->floorpic;
			botpic = sector->floorpic;
			map = heightsec->ColorMap->Maps;
		}
		else
		{
			topplane = &heightsec->ceilingplane;
			botplane = &heightsec->floorplane;
			toppic = heightsec->ceilingpic;
			botpic = heightsec->floorpic;
			map = sector->ColorMap->Maps;
		}
	}
	else
	{
		topplane = &sector->ceilingplane;
		botplane = &sector->floorplane;
		toppic = sector->ceilingpic;
		botpic = sector->floorpic;
		map = sector->ColorMap->Maps;
	}

	if (botpic != skyflatnum && particle->z < botplane->ZatPoint (particle->x, particle->y))
		return;
	if (toppic != skyflatnum && particle->z >= topplane->ZatPoint (particle->x, particle->y))
		return;

	// store information in a vissprite
	vis = R_NewVisSprite ();
	vis->heightsec = heightsec;
	vis->xscale = xscale;
//	vis->yscale = FixedMul (xscale, InvZtoScale);
	vis->yscale = xscale;
	vis->depth = tz;
	vis->cx = tx;
	vis->gx = particle->x;
	vis->gy = particle->y;
	vis->gz = y1;
	vis->gzt = y2;
	vis->x1 = x1;
	vis->x2 = x2;
	vis->Translation = 0;
	vis->startfrac = particle->color;
	vis->picnum = -1;
	vis->renderflags = particle->trans;
	vis->FakeFlatStat = fakeside;
	vis->floorclip = 0;
	vis->heightsec = heightsec;

	if (fixedlightlev)
	{
		vis->colormap = map + fixedlightlev;
	}
	else if (fixedcolormap)
	{
		vis->colormap = fixedcolormap;
	}
	else
	{
		vis->colormap = map + (GETPALOOKUP (FixedMul (tiz, r_ParticleVisibility),
			shade) << COLORMAPSHIFT);
	}
}

static void R_DrawMaskedSegsBehindParticle (const vissprite_t *vis)
{
	const int x1 = vis->x1;
	const int x2 = vis->x2;

	// Draw any masked textures behind this particle so that when the
	// particle is drawn, it will be in front of them.
	for (size_t p = InterestingDrawsegs.Size(); p-- > FirstInterestingDrawseg; )
	{
		drawseg_t *ds = &drawsegs[InterestingDrawsegs[p]];
		if (ds->x1 >= x2 || ds->x2 < x1)
		{
			continue;
		}
		if ((//ds->neardepth > vis->depth || (ds->fardepth > vis->depth &&
			DMulScale24 (vis->depth - ds->cy, ds->cdx,
						 ds->cdy, ds->cx - vis->cx) < 0))
		{
			R_RenderMaskedSegRange (ds, MAX<int> (ds->x1, x1), MIN<int> (ds->x2, x2-1));
		}
	}
}

void R_DrawParticle (vissprite_t *vis)
{
	DWORD *bg2rgb;
	int spacing;
	byte *dest;
	DWORD fg;
	byte color = vis->colormap[vis->startfrac];
	int yl = vis->gz;
	int ycount = vis->gzt - yl + 1;
	int x1 = vis->x1;
	int countbase = vis->x2 - x1 + 1;

	R_DrawMaskedSegsBehindParticle (vis);

	// vis->renderflags holds translucency level (0-255)
	{
		fixed_t fglevel, bglevel;
		DWORD *fg2rgb;

		fglevel = ((vis->renderflags + 1) << 8) & ~0x3ff;
		bglevel = FRACUNIT-fglevel;
		fg2rgb = Col2RGB8[fglevel>>10];
		bg2rgb = Col2RGB8[bglevel>>10];
		fg = fg2rgb[color];
	}

	spacing = (RenderTarget->GetPitch()<<detailyshift) - countbase;
	dest = ylookup[yl] + x1;

	do
	{
		int count = countbase;
		do
		{
			DWORD bg = bg2rgb[*dest];
			bg = (fg+bg) | 0x1f07c1f;
			*dest++ = RGB32k[0][0][bg & (bg>>15)];
		} while (--count);
		dest += spacing;
	} while (--ycount);
}
