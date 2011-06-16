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
// This file contains some code from the Build Engine.
//
// "Build Engine & Tools" Copyright (c) 1993-1997 Ken Silverman
// Ken Silverman's official web site: "http://www.advsys.net/ken"
// See the included license file "BUILDLIC.TXT" for license info.
//
//-----------------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <algorithm>

#include "templates.h"
#include "doomdef.h"
#include "m_swap.h"
#include "m_argv.h"
#include "i_system.h"
#include "w_wad.h"
#include "r_local.h"
#include "c_console.h"
#include "c_cvars.h"
#include "c_dispatch.h"
#include "doomstat.h"
#include "v_video.h"
#include "sc_man.h"
#include "s_sound.h"
#include "sbar.h"
#include "gi.h"
#include "r_sky.h"
#include "cmdlib.h"
#include "g_level.h"
#include "d_net.h"
#include "colormatcher.h"
#include "d_netinf.h"
#include "r_bsp.h"
#include "r_plane.h"
#include "r_segs.h"
#include "r_3dfloors.h"
#include "v_palette.h"
#include "r_translate.h"

extern fixed_t globaluclip, globaldclip;


#define MINZ			(2048*4)
#define BASEYCENTER 	(100)

EXTERN_CVAR (Bool, st_scale)
CVAR (Int, r_drawfuzz, 1, CVAR_ARCHIVE)
EXTERN_CVAR(Bool, r_shadercolormaps)

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

vissprite_t		*VisPSprites[NUMPSPRITES];
int				VisPSpritesX1[NUMPSPRITES];
FDynamicColormap *VisPSpritesBaseColormap[NUMPSPRITES];

static int		spriteshade;

TArray<WORD>	ParticlesInSubsec;

// constant arrays
//	used for psprite clipping and initializing clipping
short			zeroarray[MAXWIDTH];
short			screenheightarray[MAXWIDTH];

#define MAX_SPRITE_FRAMES	29		// [RH] Macro-ized as in BOOM.


CVAR (Bool, r_drawplayersprites, true, 0)	// [RH] Draw player sprites?

CVAR (Bool, r_drawvoxels, true, 0)

//
// INITIALIZATION FUNCTIONS
//

// variables used to look up
//	and range check thing_t sprites patches
TArray<spritedef_t> sprites;
TArray<spriteframe_t> SpriteFrames;
DWORD			NumStdSprites;		// The first x sprites that don't belong to skins.

TDeletingArray<FVoxel *> Voxels;	// used only to auto-delete voxels on exit.
TDeletingArray<FVoxelDef *> VoxelDefs;

int OffscreenBufferWidth, OffscreenBufferHeight;
BYTE *OffscreenColorBuffer;
FCoverageBuffer *OffscreenCoverageBuffer;

struct spriteframewithrotate : public spriteframe_t
{
	int rotate;
}
sprtemp[MAX_SPRITE_FRAMES];
int 			maxframe;
char*			spritename;

struct VoxelOptions
{
	VoxelOptions()
	: DroppedSpin(0), PlacedSpin(0), Scale(FRACUNIT), AngleOffset(0)
	{}

	int			DroppedSpin;
	int			PlacedSpin;
	fixed_t		Scale;
	angle_t		AngleOffset;
};

// [RH] skin globals
FPlayerSkin		*skins;
size_t			numskins;
BYTE			OtherGameSkinRemap[256];
PalEntry		OtherGameSkinPalette[256];

// [RH] particle globals
WORD			NumParticles;
WORD			ActiveParticles;
WORD			InactiveParticles;
particle_t		*Particles;

CVAR (Bool, r_particles, true, 0);


//
// R_InstallSpriteLump
// Local function for R_InitSprites.
//
// [RH] Removed checks for coexistance of rotation 0 with other
//		rotations and made it look more like BOOM's version.
//
static bool R_InstallSpriteLump (FTextureID lump, unsigned frame, char rot, bool flipped)
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
	{
		Printf (TEXTCOLOR_RED"R_InstallSpriteLump: Bad frame characters in lump %s\n", TexMan[lump]->Name);
		return false;
	}

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
			if (!sprtemp[frame].Texture[r].isValid())
			{
				sprtemp[frame].Texture[r] = lump;
				if (flipped)
				{
					sprtemp[frame].Flip |= 1 << r;
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

		if (!sprtemp[frame].Texture[rotation].isValid())
		{
			// the lump is only used for one rotation
			sprtemp[frame].Texture[rotation] = lump;
			if (flipped)
			{
				sprtemp[frame].Flip |= 1 << rotation;
			}
			sprtemp[frame].rotate = true;
		}
	}
	return true;
}


// [RH] Seperated out of R_InitSpriteDefs()
static void R_InstallSprite (int num)
{
	int frame;
	int framestart;
	int rot;
//	int undefinedFix;

	if (maxframe == -1)
	{
		sprites[num].numframes = 0;
		return;
	}

	maxframe++;

	// [RH] If any frames are undefined, but there are some defined frames, map
	// them to the first defined frame. This is a fix for Doom Raider, which actually
	// worked with ZDoom 2.0.47, because of a bug here. It does not define frames A,
	// B, or C for the sprite PSBG, but because I had sprtemp[].rotate defined as a
	// bool, this code never detected that it was not actually present. After switching
	// to the unified texture system, this caused it to crash while loading the wad.

// [RH] Let undefined frames actually be blank because LWM uses this in at least
// one of her wads.
//	for (frame = 0; frame < maxframe && sprtemp[frame].rotate == -1; ++frame)
//	{ }
//
//	undefinedFix = frame;

	for (frame = 0; frame < maxframe; ++frame)
	{
		switch (sprtemp[frame].rotate)
		{
		case -1:
			// no rotations were found for that frame at all
			//I_FatalError ("R_InstallSprite: No patches found for %s frame %c", sprites[num].name, frame+'A');
			break;
			
		case 0:
			// only the first rotation is needed
			for (rot = 1; rot < 16; ++rot)
			{
				sprtemp[frame].Texture[rot] = sprtemp[frame].Texture[0];
			}
			// If the frame is flipped, they all should be
			if (sprtemp[frame].Flip & 1)
			{
				sprtemp[frame].Flip = 0xFFFF;
			}
			break;
					
		case 1:
			// must have all 8 frame pairs
			for (rot = 0; rot < 8; ++rot)
			{
				if (!sprtemp[frame].Texture[rot*2+1].isValid())
				{
					sprtemp[frame].Texture[rot*2+1] = sprtemp[frame].Texture[rot*2];
					if (sprtemp[frame].Flip & (1 << (rot*2)))
					{
						sprtemp[frame].Flip |= 1 << (rot*2+1);
					}
				}
				if (!sprtemp[frame].Texture[rot*2].isValid())
				{
					sprtemp[frame].Texture[rot*2] = sprtemp[frame].Texture[rot*2+1];
					if (sprtemp[frame].Flip & (1 << (rot*2+1)))
					{
						sprtemp[frame].Flip |= 1 << (rot*2);
					}
				}

			}
			for (rot = 0; rot < 16; ++rot)
			{
				if (!sprtemp[frame].Texture[rot].isValid())
					I_FatalError ("R_InstallSprite: Sprite %s frame %c is missing rotations",
									sprites[num].name, frame+'A');
			}
			break;
		}
	}

	for (frame = 0; frame < maxframe; ++frame)
	{
		if (sprtemp[frame].rotate == -1)
		{
			memset (&sprtemp[frame].Texture, 0, sizeof(sprtemp[0].Texture));
			sprtemp[frame].Flip = 0;
			sprtemp[frame].rotate = 0;
		}
	}
	
	// allocate space for the frames present and copy sprtemp to it
	sprites[num].numframes = maxframe;
	sprites[num].spriteframes = WORD(framestart = SpriteFrames.Reserve (maxframe));
	for (frame = 0; frame < maxframe; ++frame)
	{
		memcpy (SpriteFrames[framestart+frame].Texture, sprtemp[frame].Texture, sizeof(sprtemp[frame].Texture));
		SpriteFrames[framestart+frame].Flip = sprtemp[frame].Flip;
		SpriteFrames[framestart+frame].Voxel = sprtemp[frame].Voxel;
	}

	// Let the textures know about the rotations
	for (frame = 0; frame < maxframe; ++frame)
	{
		if (sprtemp[frame].rotate == 1)
		{
			for (int rot = 0; rot < 16; ++rot)
			{
				TexMan[sprtemp[frame].Texture[rot]]->Rotations = framestart + frame;
			}
		}
	}
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
	struct Hasher
	{
		int Head, Next;
	} *hashes;
	struct VHasher
	{
		int Head, Next, Name, Spin;
		char Frame;
	} *vhashes;
	unsigned int i, j, smax, vmax;
	DWORD intname;

	// Create a hash table to speed up the process
	smax = TexMan.NumTextures();
	hashes = new Hasher[smax];
	clearbuf(hashes, sizeof(Hasher)*smax/4, -1);
	for (i = 0; i < smax; ++i)
	{
		FTexture *tex = TexMan.ByIndex(i);
		if (tex->UseType == FTexture::TEX_Sprite && strlen(tex->Name) >= 6)
		{
			size_t bucket = tex->dwName % smax;
			hashes[i].Next = hashes[bucket].Head;
			hashes[bucket].Head = i;
		}
	}

	// Repeat, for voxels
	vmax = Wads.GetNumLumps();
	vhashes = new VHasher[vmax];
	clearbuf(vhashes, sizeof(VHasher)*vmax/4, -1);
	for (i = 0; i < vmax; ++i)
	{
		if (Wads.GetLumpNamespace(i) == ns_voxels)
		{
			char name[9];
			size_t namelen;
			int spin;
			int sign;

			Wads.GetLumpName(name, i);
			name[8] = 0;
			namelen = strlen(name);
			if (namelen < 4)
			{ // name is too short
				continue;
			}
			if (name[4] != '\0' && name[4] != ' ' && (name[4] < 'A' || name[4] >= 'A' + MAX_SPRITE_FRAMES))
			{ // frame char is invalid
				continue;
			}
			spin = 0;
			sign = 2;	// 2 to convert from deg/halfsec to deg/sec
			j = 5;
			if (j < namelen && name[j] == '-')
			{ // a minus sign is okay, but only before any digits
				j++;
				sign = -2;
			}
			for (; j < namelen; ++j)
			{ // the remainder to the end of the name must be digits
				if (name[j] >= '0' && name[j] <= '9')
				{
					spin = spin * 10 + name[j] - '0';
				}
				else
				{
					break;
				}
			}
			if (j < namelen)
			{ // the spin part is invalid
				continue;
			}
			memcpy(&vhashes[i].Name, name, 4);
			vhashes[i].Frame = name[4];
			vhashes[i].Spin = spin * sign;
			size_t bucket = vhashes[i].Name % vmax;
			vhashes[i].Next = vhashes[bucket].Head;
			vhashes[bucket].Head = i;
		}
	}

	// scan all the lump names for each of the names, noting the highest frame letter.
	for (i = 0; i < sprites.Size(); ++i)
	{
		memset (sprtemp, 0xFF, sizeof(sprtemp));
		for (j = 0; j < MAX_SPRITE_FRAMES; ++j)
		{
			sprtemp[j].Flip = 0;
			sprtemp[j].Voxel = NULL;
		}
				
		maxframe = -1;
		intname = sprites[i].dwName;

		// scan the lumps, filling in the frames for whatever is found
		int hash = hashes[intname % smax].Head;
		while (hash != -1)
		{
			FTexture *tex = TexMan[hash];
			if (tex->dwName == intname)
			{
				bool res = R_InstallSpriteLump (FTextureID(hash), tex->Name[4] - 'A', tex->Name[5], false);

				if (tex->Name[6] && res)
					R_InstallSpriteLump (FTextureID(hash), tex->Name[6] - 'A', tex->Name[7], true);
			}
			hash = hashes[hash].Next;
		}

		// repeat, for voxels
		hash = vhashes[intname % vmax].Head;
		while (hash != -1)
		{
			VHasher *vh = &vhashes[hash];
			if (vh->Name == (int)intname)
			{
				FVoxel *vox = R_LoadKVX(hash);
				if (vox == NULL)
				{
					Printf("%s is not a valid voxel file\n", Wads.GetLumpFullName(hash));
				}
				else
				{
					FVoxelDef *voxdef = new FVoxelDef;

					voxdef->Voxel = vox;
					voxdef->Scale = FRACUNIT;
					voxdef->DroppedSpin = voxdef->PlacedSpin = vh->Spin;
					voxdef->AngleOffset = 0;

					Voxels.Push(vox);
					VoxelDefs.Push(voxdef);

					if (vh->Frame == ' ' || vh->Frame == '\0')
					{ // voxel applies to every sprite frame
						for (j = 0; j < MAX_SPRITE_FRAMES; ++j)
						{
							if (sprtemp[j].Voxel == NULL)
							{
								sprtemp[j].Voxel = voxdef;
							}
						}
						maxframe = MAX_SPRITE_FRAMES-1;
					}
					else
					{ // voxel applies to a specific frame
						j = vh->Frame - 'A';
						sprtemp[j].Voxel = voxdef;
						maxframe = MAX<int>(maxframe, j);
					}
				}
			}
			hash = vh->Next;
		}
		
		R_InstallSprite ((int)i);
	}

	delete[] hashes;
	delete[] vhashes;
}

//==========================================================================
//
// R_ExtendSpriteFrames
//
// Extends a sprite so that it can hold the desired frame.
//
//==========================================================================

static void R_ExtendSpriteFrames(spritedef_t &spr, int frame)
{
	unsigned int i, newstart;

	if (spr.numframes >= ++frame)
	{ // The sprite already has enough frames, so do nothing.
		return;
	}

	if (spr.numframes == 0 || (spr.spriteframes + spr.numframes == SpriteFrames.Size()))
	{ // Sprite's frames are at the end of the array, or it has no frames
	  // at all, so we can tack the new frames directly on to the end
	  // of the SpriteFrames array.
		newstart = SpriteFrames.Reserve(frame - spr.numframes);
	}
	else
	{ // We need to allocate space for all the sprite's frames and copy
	  // the existing ones over to the new space. The old space will be
	  // lost.
		newstart = SpriteFrames.Reserve(frame);
		for (i = 0; i < spr.numframes; ++i)
		{
			SpriteFrames[newstart + i] = SpriteFrames[spr.spriteframes + i];
		}
		spr.spriteframes = WORD(newstart);
		newstart += i;
	}
	// Initialize all new frames to 0.
	memset(&SpriteFrames[newstart], 0, sizeof(spriteframe_t)*(frame - spr.numframes));
	spr.numframes = frame;
}

//==========================================================================
//
// VOX_ReadSpriteNames
//
// Reads a list of sprite names from a VOXELDEF lump.
//
//==========================================================================

static bool VOX_ReadSpriteNames(FScanner &sc, TArray<DWORD> &vsprites)
{
	unsigned int i;

	vsprites.Clear();
	while (sc.GetString())
	{
		// A sprite name list is terminated by an '=' character.
		if (sc.String[0] == '=')
		{
			if (vsprites.Size() == 0)
			{
				sc.ScriptMessage("No sprites specified for voxel.\n");
			}
			return true;
		}
		if (sc.StringLen != 4 && sc.StringLen != 5)
		{
			sc.ScriptMessage("Sprite name \"%s\" is wrong size.\n", sc.String);
		}
		else if (sc.StringLen == 5 && (sc.String[4] = toupper(sc.String[4]), sc.String[4] < 'A' || sc.String[4] >= 'A' + MAX_SPRITE_FRAMES))
		{
			sc.ScriptMessage("Sprite frame %s is invalid.\n", sc.String[4]);
		}
		else
		{
			int frame = (sc.StringLen == 4) ? 255 : sc.String[4] - 'A';
			int spritename;

			for (i = 0; i < 4; ++i)
			{
				sc.String[i] = toupper(sc.String[i]);
			}
			spritename = *(int *)sc.String;
			for (i = 0; i < sprites.Size(); ++i)
			{
				if ((int)sprites[i].dwName == spritename)
				{
					break;
				}
			}
			if (i != sprites.Size())
			{
				vsprites.Push((frame << 24) | i);
			}
		}
	}
	if (vsprites.Size() != 0)
	{
		sc.ScriptMessage("Unexpected end of file\n");
	}
	return false;
}

//==========================================================================
//
// VOX_ReadOptions
//
// Reads a list of options from a VOXELDEF lump, terminated with a '}'
// character. The leading '{' must already be consumed
//
//==========================================================================

static void VOX_ReadOptions(FScanner &sc, VoxelOptions &opts)
{
	while (sc.GetToken())
	{
		if (sc.TokenType == '}')
		{
			return;
		}
		sc.TokenMustBe(TK_Identifier);
		if (sc.Compare("scale"))
		{
			sc.MustGetToken('=');
			sc.MustGetToken(TK_FloatConst);
			opts.Scale = FLOAT2FIXED(sc.Float);
		}
		else if (sc.Compare("spin"))
		{
			sc.MustGetToken('=');
			sc.MustGetToken(TK_IntConst);
			opts.DroppedSpin = opts.PlacedSpin = sc.Number;
		}
		else if (sc.Compare("placedspin"))
		{
			sc.MustGetToken('=');
			sc.MustGetToken(TK_IntConst);
			opts.PlacedSpin = sc.Number;
		}
		else if (sc.Compare("droppedspin"))
		{
			sc.MustGetToken('=');
			sc.MustGetToken(TK_IntConst);
			opts.DroppedSpin = sc.Number;
		}
		else if (sc.Compare("angleoffset"))
		{
			sc.MustGetToken('=');
			sc.MustGetAnyToken();
			if (sc.TokenType == TK_IntConst)
			{
				sc.Float = sc.Number;
			}
			else
			{
				sc.TokenMustBe(TK_FloatConst);
			}
			opts.AngleOffset = angle_t(sc.Float * ANGLE_180 / 180.0);
		}
		else
		{
			sc.ScriptMessage("Unknown voxel option '%s'\n", sc.String);
			if (sc.CheckToken('='))
			{
				sc.MustGetAnyToken();
			}
		}
	}
	sc.ScriptMessage("Unterminated voxel option block\n");
}

//==========================================================================
//
// VOX_GetVoxel
//
// Returns a voxel object for the given lump or NULL if it is not a valid
// voxel. If the voxel has already been loaded, it will be reused.
//
//==========================================================================

static FVoxel *VOX_GetVoxel(int lumpnum)
{
	// Is this voxel already loaded? If so, return it.
	for (unsigned i = 0; i < Voxels.Size(); ++i)
	{
		if (Voxels[i]->LumpNum == lumpnum)
		{
			return Voxels[i];
		}
	}
	FVoxel *vox = R_LoadKVX(lumpnum);
	if (vox != NULL)
	{
		Voxels.Push(vox);
	}
	return vox;
}

//==========================================================================
//
// VOX_AddVoxel
//
// Sets a voxel for a single sprite frame.
//
//==========================================================================

static void VOX_AddVoxel(int sprnum, int frame, FVoxelDef *def)
{
	R_ExtendSpriteFrames(sprites[sprnum], frame);
	SpriteFrames[sprites[sprnum].spriteframes + frame].Voxel = def;
}

//==========================================================================
//
// R_InitVoxels
//
// Process VOXELDEF lumps for defining voxel options that cannot be
// condensed neatly into a sprite name format.
//
//==========================================================================

void R_InitVoxels()
{
	int lump, lastlump = 0;
	
	while ((lump = Wads.FindLump("VOXELDEF", &lastlump)) != -1)
	{
		FScanner sc(lump);
		TArray<DWORD> vsprites;

		while (VOX_ReadSpriteNames(sc, vsprites))
		{
			FVoxel *voxeldata = NULL;
			int voxelfile;
			VoxelOptions opts;

			sc.SetCMode(true);
			sc.MustGetToken(TK_StringConst);
			voxelfile = Wads.CheckNumForFullName(sc.String, true, ns_voxels);
			if (voxelfile < 0)
			{
				sc.ScriptMessage("Voxel \"%s\" not found.\n", sc.String);
			}
			else
			{
				voxeldata = VOX_GetVoxel(voxelfile);
				if (voxeldata == NULL)
				{
					sc.ScriptMessage("\"%s\" is not a valid voxel file.\n", sc.String);
				}
			}
			if (sc.CheckToken('{'))
			{
				VOX_ReadOptions(sc, opts);
			}
			sc.SetCMode(false);
			if (voxeldata != NULL && vsprites.Size() != 0)
			{
				FVoxelDef *def = new FVoxelDef;

				def->Voxel = voxeldata;
				def->Scale = opts.Scale;
				def->DroppedSpin = opts.DroppedSpin;
				def->PlacedSpin = opts.PlacedSpin;
				def->AngleOffset = opts.AngleOffset;
				VoxelDefs.Push(def);

				for (unsigned i = 0; i < vsprites.Size(); ++i)
				{
					int sprnum = int(vsprites[i] & 0xFFFFFF);
					int frame = int(vsprites[i] >> 24);
					if (frame == 255)
					{ // Apply voxel to all frames.
						for (int j = MAX_SPRITE_FRAMES - 1; j >= 0; --j)
						{
							VOX_AddVoxel(sprnum, j, def);
						}
					}
					else
					{ // Apply voxel to only one frame.
						VOX_AddVoxel(sprnum, frame, def);
					}
				}
			}
		}
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

/*
static int STACK_ARGS skinsorter (const void *a, const void *b)
{
	return stricmp (((FPlayerSkin *)a)->name, ((FPlayerSkin *)b)->name);
}
*/

void R_InitSkins (void)
{
	FSoundID playersoundrefs[NUMSKINSOUNDS];
	spritedef_t temp;
	int sndlumps[NUMSKINSOUNDS];
	char key[65];
	DWORD intname, crouchname;
	size_t i;
	int j, k, base;
	int lastlump;
	int aliasid;
	bool remove;
	const PClass *basetype, *transtype;

	key[sizeof(key)-1] = 0;
	i = PlayerClasses.Size () - 1;
	lastlump = 0;

	for (j = 0; j < NUMSKINSOUNDS; ++j)
	{
		playersoundrefs[j] = skinsoundnames[j][1];
	}

	while ((base = Wads.FindLump ("S_SKIN", &lastlump, true)) != -1)
	{
		// The player sprite has 23 frames. This means that the S_SKIN
		// marker needs a minimum of 23 lumps after it.
		if (base >= Wads.GetNumLumps() - 23 || base == -1)
			continue;

		i++;
		for (j = 0; j < NUMSKINSOUNDS; j++)
			sndlumps[j] = -1;
		skins[i].namespc = Wads.GetLumpNamespace (base);

		FScanner sc(base);
		intname = 0;
		crouchname = 0;

		remove = false;
		basetype = NULL;
		transtype = NULL;

		// Data is stored as "key = data".
		while (sc.GetString ())
		{
			strncpy (key, sc.String, sizeof(key)-1);
			if (!sc.GetString() || sc.String[0] != '=')
			{
				Printf (PRINT_BOLD, "Bad format for skin %d: %s\n", (int)i, key);
				break;
			}
			sc.GetString ();
			if (0 == stricmp (key, "name"))
			{
				strncpy (skins[i].name, sc.String, 16);
				for (j = 0; (size_t)j < i; j++)
				{
					if (stricmp (skins[i].name, skins[j].name) == 0)
					{
						mysnprintf (skins[i].name, countof(skins[i].name), "skin%d", (int)i);
						Printf (PRINT_BOLD, "Skin %s duplicated as %s\n",
							skins[j].name, skins[i].name);
						break;
					}
				}
			}
			else if (0 == stricmp (key, "sprite"))
			{
				for (j = 3; j >= 0; j--)
					sc.String[j] = toupper (sc.String[j]);
				intname = *((DWORD *)sc.String);
			}
			else if (0 == stricmp (key, "crouchsprite"))
			{
				for (j = 3; j >= 0; j--)
					sc.String[j] = toupper (sc.String[j]);
				crouchname = *((DWORD *)sc.String);
			}
			else if (0 == stricmp (key, "face"))
			{
				for (j = 2; j >= 0; j--)
					skins[i].face[j] = toupper (sc.String[j]);
				skins[i].face[3] = '\0';
			}
			else if (0 == stricmp (key, "gender"))
			{
				skins[i].gender = D_GenderToInt (sc.String);
			}
			else if (0 == stricmp (key, "scale"))
			{
				skins[i].ScaleX = clamp<fixed_t> (FLOAT2FIXED(atof (sc.String)), 1, 256*FRACUNIT);
				skins[i].ScaleY = skins[i].ScaleX;
			}
			else if (0 == stricmp (key, "game"))
			{
				if (gameinfo.gametype == GAME_Heretic)
					basetype = PClass::FindClass (NAME_HereticPlayer);
				else if (gameinfo.gametype == GAME_Strife)
					basetype = PClass::FindClass (NAME_StrifePlayer);
				else
					basetype = PClass::FindClass (NAME_DoomPlayer);

				transtype = basetype;

				if (stricmp (sc.String, "heretic") == 0)
				{
					if (gameinfo.gametype & GAME_DoomChex)
					{
						transtype = PClass::FindClass (NAME_HereticPlayer);
						skins[i].othergame = true;
					}
					else if (gameinfo.gametype != GAME_Heretic)
					{
						remove = true;
					}
				}
				else if (stricmp (sc.String, "strife") == 0)
				{
					if (gameinfo.gametype != GAME_Strife)
					{
						remove = true;
					}
				}
				else
				{
					if (gameinfo.gametype == GAME_Heretic)
					{
						transtype = PClass::FindClass (NAME_DoomPlayer);
						skins[i].othergame = true;
					}
					else if (!(gameinfo.gametype & GAME_DoomChex))
					{
						remove = true;
					}
				}

				if (remove)
					break;
			}
			else if (0 == stricmp (key, "class"))
			{ // [GRB] Define the skin for a specific player class
				int pclass = D_PlayerClassToInt (sc.String);

				if (pclass < 0)
				{
					remove = true;
					break;
				}

				basetype = transtype = PlayerClasses[pclass].Type;
			}
			else if (key[0] == '*')
			{ // Player sound replacment (ZDoom extension)
				int lump = Wads.CheckNumForName (sc.String, skins[i].namespc);
				if (lump == -1)
				{
					lump = Wads.CheckNumForFullName (sc.String, true, ns_sounds);
				}
				if (lump != -1)
				{
					if (stricmp (key, "*pain") == 0)
					{ // Replace all pain sounds in one go
						aliasid = S_AddPlayerSound (skins[i].name, skins[i].gender,
							playersoundrefs[0], lump, true);
						for (int l = 3; l > 0; --l)
						{
							S_AddPlayerSoundExisting (skins[i].name, skins[i].gender,
								playersoundrefs[l], aliasid, true);
						}
					}
					else
					{
						int sndref = S_FindSoundNoHash (key);
						if (sndref != 0)
						{
							S_AddPlayerSound (skins[i].name, skins[i].gender, sndref, lump, true);
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
						sndlumps[j] = Wads.CheckNumForName (sc.String, skins[i].namespc);
						if (sndlumps[j] == -1)
						{ // Replacement not found, try finding it in the global namespace
							sndlumps[j] = Wads.CheckNumForFullName (sc.String, true, ns_sounds);
						}
					}
				}
				//if (j == 8)
				//	Printf ("Funny info for skin %i: %s = %s\n", i, key, sc.String);
			}
		}

		// [GRB] Assume Doom skin by default
		if (!remove && basetype == NULL)
		{
			if (gameinfo.gametype & GAME_DoomChex)
			{
				basetype = transtype = PClass::FindClass (NAME_DoomPlayer);
			}
			else if (gameinfo.gametype == GAME_Heretic)
			{
				basetype = PClass::FindClass (NAME_HereticPlayer);
				transtype = PClass::FindClass (NAME_DoomPlayer);
				skins[i].othergame = true;
			}
			else
			{
				remove = true;
			}
		}

		if (!remove)
		{
			skins[i].range0start = transtype->Meta.GetMetaInt (APMETA_ColorRange) & 0xff;
			skins[i].range0end = transtype->Meta.GetMetaInt (APMETA_ColorRange) >> 8;

			remove = true;
			for (j = 0; j < (int)PlayerClasses.Size (); j++)
			{
				const PClass *type = PlayerClasses[j].Type;

				if (type->IsDescendantOf (basetype) &&
					GetDefaultByType (type)->SpawnState->sprite == GetDefaultByType (basetype)->SpawnState->sprite &&
					type->Meta.GetMetaInt (APMETA_ColorRange) == basetype->Meta.GetMetaInt (APMETA_ColorRange))
				{
					PlayerClasses[j].Skins.Push ((int)i);
					remove = false;
				}
			}
		}

		if (!remove)
		{
			if (skins[i].name[0] == 0)
				mysnprintf (skins[i].name, countof(skins[i].name), "skin%d", (int)i);

			// Now collect the sprite frames for this skin. If the sprite name was not
			// specified, use whatever immediately follows the specifier lump.
			if (intname == 0)
			{
				char name[9];
				Wads.GetLumpName (name, base+1);
				memcpy(&intname, name, 4);
			}

			int basens = Wads.GetLumpNamespace(base);

			for(int spr = 0; spr<2; spr++)
			{
				memset (sprtemp, 0xFFFF, sizeof(sprtemp));
				for (k = 0; k < MAX_SPRITE_FRAMES; ++k)
				{
					sprtemp[k].Flip = 0;
					sprtemp[k].Voxel = NULL;
				}
				maxframe = -1;

				if (spr == 1)
				{
					if (crouchname !=0 && crouchname != intname)
					{
						intname = crouchname;
					}
					else
					{
						skins[i].crouchsprite = -1;
						break;
					}
				}

				for (k = base + 1; Wads.GetLumpNamespace(k) == basens; k++)
				{
					char lname[9];
					DWORD lnameint;
					Wads.GetLumpName (lname, k);
					memcpy(&lnameint, lname, 4);
					if (lnameint == intname)
					{
						FTextureID picnum = TexMan.CreateTexture(k, FTexture::TEX_SkinSprite);
						bool res = R_InstallSpriteLump (picnum, lname[4] - 'A', lname[5], false);

						if (lname[6] && res)
							R_InstallSpriteLump (picnum, lname[6] - 'A', lname[7], true);
					}
				}

				if (spr == 0 && maxframe <= 0)
				{
					Printf (PRINT_BOLD, "Skin %s (#%d) has no frames. Removing.\n", skins[i].name, (int)i);
					remove = true;
					break;
				}

				Wads.GetLumpName (temp.name, base+1);
				temp.name[4] = 0;
				int sprno = (int)sprites.Push (temp);
				if (spr==0)	skins[i].sprite = sprno;
				else skins[i].crouchsprite = sprno;
				R_InstallSprite (sprno);
			}
		}

		if (remove)
		{
			if (i < numskins-1)
				memmove (&skins[i], &skins[i+1], sizeof(skins[0])*(numskins-i-1));
			i--;
			continue;
		}

		// Register any sounds this skin provides
		aliasid = 0;
		for (j = 0; j < NUMSKINSOUNDS; j++)
		{
			if (sndlumps[j] != -1)
			{
				if (j == 0 || sndlumps[j] != sndlumps[j-1])
				{
					aliasid = S_AddPlayerSound (skins[i].name, skins[i].gender,
						playersoundrefs[j], sndlumps[j], true);
				}
				else
				{
					S_AddPlayerSoundExisting (skins[i].name, skins[i].gender,
						playersoundrefs[j], aliasid, true);
				}
			}
		}

		// Make sure face prefix is a full 3 chars
		if (skins[i].face[1] == 0 || skins[i].face[2] == 0)
		{
			skins[i].face[0] = 0;
		}
	}

	if (numskins > PlayerClasses.Size ())
	{ // The sound table may have changed, so rehash it.
		S_HashSounds ();
		S_ShrinkPlayerSoundLists ();
	}
}

// [RH] Find a skin by name
int R_FindSkin (const char *name, int pclass)
{
	if (stricmp ("base", name) == 0)
	{
		return pclass;
	}

	for (unsigned i = PlayerClasses.Size(); i < numskins; i++)
	{
		if (strnicmp (skins[i].name, name, 16) == 0)
		{
			if (PlayerClasses[pclass].CheckSkin (i))
				return i;
			else
				return pclass;
		}
	}
	return pclass;
}

// [RH] List the names of all installed skins
CCMD (skins)
{
	int i;

	for (i = PlayerClasses.Size ()-1; i < (int)numskins; i++)
		Printf ("% 3d %s\n", i-PlayerClasses.Size ()+1, skins[i].name);
}

//
// GAME FUNCTIONS
//
int				MaxVisSprites;
vissprite_t 	**vissprites;
vissprite_t		**firstvissprite;
vissprite_t		**vissprite_p;
vissprite_t		**lastvissprite;
int 			newvissprite;

static vissprite_t **spritesorter;
static int spritesortersize = 0;
static int vsprcount;

static void R_CreateSkinTranslation (const char *palname)
{
	FMemLump lump = Wads.ReadLump (palname);
	const BYTE *otherPal = (BYTE *)lump.GetMem();
 
	for (int i = 0; i < 256; ++i)
	{
		OtherGameSkinRemap[i] = ColorMatcher.Pick (otherPal[0], otherPal[1], otherPal[2]);
		OtherGameSkinPalette[i] = PalEntry(otherPal[0], otherPal[1], otherPal[2]);
		otherPal += 3;
	}
}


//
// R_InitSprites
// Called at program start.
//
void R_InitSprites ()
{
	int lump, lastlump;
	unsigned int i, j;

	clearbufshort (zeroarray, MAXWIDTH, 0);

	// [RH] Create a standard translation to map skins between Heretic and Doom
	if (gameinfo.gametype == GAME_DoomChex)
	{
		R_CreateSkinTranslation ("SPALHTIC");
	}
	else
	{
		R_CreateSkinTranslation ("SPALDOOM");
	}

	// [RH] Count the number of skins.
	numskins = PlayerClasses.Size ();
	lastlump = 0;
	while ((lump = Wads.FindLump ("S_SKIN", &lastlump, true)) != -1)
	{
		numskins++;
	}

	SpriteFrames.Clear();

	// [RH] Do some preliminary setup
	if (skins != NULL) delete [] skins;
	skins = new FPlayerSkin[numskins];
	memset (skins, 0, sizeof(*skins) * numskins);
	for (i = 0; i < numskins; i++)
	{ // Assume Doom skin by default
		const PClass *type = PlayerClasses[0].Type;
		skins[i].range0start = type->Meta.GetMetaInt (APMETA_ColorRange) & 255;
		skins[i].range0end = type->Meta.GetMetaInt (APMETA_ColorRange) >> 8;
		skins[i].ScaleX = GetDefaultByType (type)->scaleX;
		skins[i].ScaleY = GetDefaultByType (type)->scaleY;
	}

	R_InitSpriteDefs ();
	R_InitVoxels();		// [RH] Parse VOXELDEF
	NumStdSprites = sprites.Size();
	R_InitSkins ();		// [RH] Finish loading skin data

	// [RH] Set up base skin
	// [GRB] Each player class has its own base skin
	for (i = 0; i < PlayerClasses.Size (); i++)
	{
		const PClass *basetype = PlayerClasses[i].Type;
		const char *pclassface = basetype->Meta.GetMetaString (APMETA_Face);

		strcpy (skins[i].name, "Base");
		if (pclassface == NULL || strcmp(pclassface, "None") == 0)
		{
			skins[i].face[0] = 'S';
			skins[i].face[1] = 'T';
			skins[i].face[2] = 'F';
			skins[i].face[3] = '\0';
		}
		else
		{
			strcpy(skins[i].face, pclassface);
		}
		skins[i].range0start = basetype->Meta.GetMetaInt (APMETA_ColorRange) & 255;
		skins[i].range0end = basetype->Meta.GetMetaInt (APMETA_ColorRange) >> 8;
		skins[i].ScaleX = GetDefaultByType (basetype)->scaleX;
		skins[i].ScaleY = GetDefaultByType (basetype)->scaleY;
		skins[i].sprite = GetDefaultByType (basetype)->SpawnState->sprite;
		skins[i].namespc = ns_global;

		PlayerClasses[i].Skins.Push (i);

		if (memcmp (sprites[skins[i].sprite].name, "PLAY", 4) == 0)
		{
			for (j = 0; j < sprites.Size (); j++)
			{
				if (memcmp (sprites[j].name, deh.PlayerSprite, 4) == 0)
				{
					skins[i].sprite = (int)j;
					break;
				}
			}
		}
	}

	// [RH] Sort the skins, but leave base as skin 0
	//qsort (&skins[PlayerClasses.Size ()], numskins-PlayerClasses.Size (), sizeof(FPlayerSkin), skinsorter);
}

void R_DeinitSprites()
{
	// Free skins
	if (skins != NULL)
	{
		delete[] skins;
		skins = NULL;
	}

	// Free vissprites
	for (int i = 0; i < MaxVisSprites; ++i)
	{
		delete vissprites[i];
	}
	free (vissprites);
	vissprites = NULL;
	vissprite_p = lastvissprite = NULL;
	MaxVisSprites = 0;

	// Free vissprites sorter
	if (spritesorter != NULL)
	{
		delete[] spritesorter;
		spritesortersize = 0;
		spritesorter = NULL;
	}

	// Free offscreen buffer
	if (OffscreenColorBuffer != NULL)
	{
		delete[] OffscreenColorBuffer;
		OffscreenColorBuffer = NULL;
	}
	if (OffscreenCoverageBuffer != NULL)
	{
		delete OffscreenCoverageBuffer;
		OffscreenCoverageBuffer = NULL;
	}
	OffscreenBufferHeight = OffscreenBufferWidth = 0;
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

		MaxVisSprites = MaxVisSprites ? MaxVisSprites * 2 : 128;
		vissprites = (vissprite_t **)M_Realloc (vissprites, MaxVisSprites * sizeof(vissprite_t));
		lastvissprite = &vissprites[MaxVisSprites];
		firstvissprite = &vissprites[firstvisspritenum];
		vissprite_p = &vissprites[prevvisspritenum];
		DPrintf ("MaxVisSprites increased to %d\n", MaxVisSprites);

		// Allocate sprites from the new pile
		for (vissprite_t **p = vissprite_p; p < lastvissprite; ++p)
		{
			*p = new vissprite_t;
		}
	}

	vissprite_p++;
	return *(vissprite_p-1);
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

void R_DrawMaskedColumn (const BYTE *column, const FTexture::Span *span)
{
	while (span->Length != 0)
	{
		const int length = span->Length;
		const int top = span->TopOffset;

		// calculate unclipped screen coordinates for post
		dc_yl = (sprtopscreen + spryscale * top) >> FRACBITS;
		dc_yh = (sprtopscreen + spryscale * (top + length) - FRACUNIT) >> FRACBITS;

		if (sprflipvert)
		{
			swapvalues (dc_yl, dc_yh);
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
				if (dc_yh < mfloorclip[dc_x]-1 && endfrac < maxfrac - dc_iscale)
				{
					dc_yh++;
				}
				else while (endfrac >= maxfrac)
				{
					if (--dc_yh < dc_yl)
						goto nextpost;
					endfrac -= dc_iscale;
				}
			}
			dc_source = column + top;
			dc_dest = ylookup[dc_yl] + dc_x + dc_destorg;
			dc_count = dc_yh - dc_yl + 1;
			colfunc ();
		}
nextpost:
		span++;
	}
}

//
// R_DrawVisSprite
//	mfloorclip and mceilingclip should also be set.
//
void R_DrawVisSprite (vissprite_t *vis)
{
	const BYTE *pixels;
	const FTexture::Span *spans;
	fixed_t 		frac;
	FTexture		*tex;
	int				x2, stop4;
	fixed_t			xiscale;
	ESPSResult		mode;

	dc_colormap = vis->colormap;

	mode = R_SetPatchStyle (vis->RenderStyle, vis->alpha, vis->Translation, vis->FillColor);

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

		tex = vis->pic;
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
				pixels = tex->GetColumn (frac >> FRACBITS, &spans);
				R_DrawMaskedColumn (pixels, spans);
				dc_x++;
				frac += xiscale;
			}

			while (dc_x < stop4)
			{
				rt_initcols();
				for (int zz = 4; zz; --zz)
				{
					pixels = tex->GetColumn (frac >> FRACBITS, &spans);
					R_DrawMaskedColumnHoriz (pixels, spans);
					dc_x++;
					frac += xiscale;
				}
				rt_draw4cols (dc_x - 4);
			}

			while (dc_x < x2)
			{
				pixels = tex->GetColumn (frac >> FRACBITS, &spans);
				R_DrawMaskedColumn (pixels, spans);
				dc_x++;
				frac += xiscale;
			}
		}
	}

	R_FinishSetPatchStyle ();

	NetUpdate ();
}

void R_DrawVisVoxel(vissprite_t *spr, int minslabz, int maxslabz, short *cliptop, short *clipbot)
{
	ESPSResult mode;
	int flags = 0;

	// Do setup for blending.
	dc_colormap = spr->colormap;
	mode = R_SetPatchStyle(spr->RenderStyle, spr->alpha, spr->Translation, spr->FillColor);

	if (mode == DontDraw)
	{
		return;
	}
	if (colfunc == fuzzcolfunc || colfunc == R_FillColumnP)
	{
		flags = DVF_OFFSCREEN | DVF_SPANSONLY;
	}
	else if (colfunc != basecolfunc)
	{
		flags = DVF_OFFSCREEN;
	}
	if (flags != 0)
	{
		R_CheckOffscreenBuffer(RenderTarget->GetWidth(), RenderTarget->GetHeight(), !!(flags & DVF_SPANSONLY));
	}

	// Render the voxel, either directly to the screen or offscreen.
	R_DrawVoxel(spr->gx, spr->gy, spr->gz, spr->angle, spr->xscale, spr->yscale, spr->voxel, spr->colormap, cliptop, clipbot,
		minslabz, maxslabz, flags);

	// Blend the voxel, if that's what we need to do.
	if (flags != 0)
	{
		for (int x = 0; x < viewwidth; ++x)
		{
			if (!(flags & DVF_SPANSONLY) && (x & 3) == 0)
			{
				rt_initcols(OffscreenColorBuffer + x * OffscreenBufferHeight);
			}
			for (FCoverageBuffer::Span *span = OffscreenCoverageBuffer->Spans[x]; span != NULL; span = span->NextSpan)
			{
				if (flags & DVF_SPANSONLY)
				{
					dc_x = x;
					dc_yl = span->Start;
					dc_yh = span->Stop - 1;
					dc_count = span->Stop - span->Start;
					dc_dest = ylookup[span->Start] + x + dc_destorg;
					colfunc();
				}
				else
				{
					unsigned int **tspan = &dc_ctspan[x & 3];
					(*tspan)[0] = span->Start;
					(*tspan)[1] = span->Stop - 1;
					*tspan += 2;
				}
			}
			if (!(flags & DVF_SPANSONLY) && (x & 3) == 3)
			{
				rt_draw4cols(x - 3);
			}
		}
	}

	R_FinishSetPatchStyle();
	NetUpdate();
}

//
// R_ProjectSprite
// Generates a vissprite for a thing if it might be visible.
//
void R_ProjectSprite (AActor *thing, int fakeside, F3DFloor *fakefloor, F3DFloor *fakeceiling)
{
	fixed_t				fx, fy, fz;
	fixed_t 			tr_x;
	fixed_t 			tr_y;
	
	fixed_t				gzt;				// killough 3/27/98
	fixed_t				gzb;				// [RH] use bottom of sprite, not actor
	fixed_t 			tx, tx2;
	fixed_t 			tz;

	fixed_t 			xscale = FRACUNIT, yscale = FRACUNIT;
	
	int 				x1;
	int 				x2;

	FTextureID			picnum;
	FTexture			*tex;
	FVoxelDef			*voxel;
	
	WORD 				flip;
	
	vissprite_t*		vis;
	
	fixed_t 			iscale;

	sector_t*			heightsec;			// killough 3/27/98

	// Don't waste time projecting sprites that are definitely not visible.
	if (thing == NULL ||
		(thing->renderflags & RF_INVISIBLE) ||
		!thing->RenderStyle.IsVisible(thing->alpha))
	{
		return;
	}

	// [RH] Interpolate the sprite's position to make it look smooth
	fx = thing->PrevX + FixedMul (r_TicFrac, thing->x - thing->PrevX);
	fy = thing->PrevY + FixedMul (r_TicFrac, thing->y - thing->PrevY);
	fz = thing->PrevZ + FixedMul (r_TicFrac, thing->z - thing->PrevZ);

	// transform the origin point
	tr_x = fx - viewx;
	tr_y = fy - viewy;

	tz = DMulScale20 (tr_x, viewtancos, tr_y, viewtansin);

	tex = NULL;
	voxel = NULL;

	if (thing->picnum.isValid())
	{
		picnum = thing->picnum;

		tex = TexMan(picnum);
		if (tex->UseType == FTexture::TEX_Null)
		{
			return;
		}
		flip = 0;

		if (tex->Rotations != 0xFFFF)
		{
			// choose a different rotation based on player view
			spriteframe_t *sprframe = &SpriteFrames[tex->Rotations];
			angle_t ang = R_PointToAngle (fx, fy);
			angle_t rot;
			if (sprframe->Texture[0] == sprframe->Texture[1])
			{
				rot = (ang - thing->angle + (angle_t)(ANGLE_45/2)*9) >> 28;
			}
			else
			{
				rot = (ang - thing->angle + (angle_t)(ANGLE_45/2)*9-(angle_t)(ANGLE_180/16)) >> 28;
			}
			picnum = sprframe->Texture[rot];
			flip = sprframe->Flip & (1 << rot);
			tex = TexMan[picnum];	// Do not animate the rotation
		}
	}
	else
	{
		// decide which texture to use for the sprite
#ifdef RANGECHECK
		if ((unsigned)thing->sprite >= (unsigned)sprites.Size ())
		{
			DPrintf ("R_ProjectSprite: invalid sprite number %i\n", thing->sprite);
			return;
		}
#endif
		spritedef_t *sprdef = &sprites[thing->sprite];
		if (thing->frame >= sprdef->numframes)
		{
			// If there are no frames at all for this sprite, don't draw it.
			return;
		}
		else
		{
			//picnum = SpriteFrames[sprdef->spriteframes + thing->frame].Texture[0];
			// choose a different rotation based on player view
			spriteframe_t *sprframe = &SpriteFrames[sprdef->spriteframes + thing->frame];
			angle_t ang = R_PointToAngle (fx, fy);
			angle_t rot;
			if (sprframe->Texture[0] == sprframe->Texture[1])
			{
				rot = (ang - thing->angle + (angle_t)(ANGLE_45/2)*9) >> 28;
			}
			else
			{
				rot = (ang - thing->angle + (angle_t)(ANGLE_45/2)*9-(angle_t)(ANGLE_180/16)) >> 28;
			}
			picnum = sprframe->Texture[rot];
			flip = sprframe->Flip & (1 << rot);
			tex = TexMan[picnum];	// Do not animate the rotation
			if (r_drawvoxels)
			{
				voxel = sprframe->Voxel;
			}
		}
	}
	if (voxel == NULL && (tex == NULL || tex->UseType == FTexture::TEX_Null))
	{
		return;
	}

	// thing is behind view plane?
	if (voxel == NULL && tz < MINZ)
		return;

	tx = DMulScale16 (tr_x, viewsin, -tr_y, viewcos);

	// [RH] Flip for mirrors
	if (MirrorFlags & RF_XFLIP)
	{
		tx = -tx;
	}
	tx2 = tx >> 4;

	// too far off the side?
	if ((abs(tx) >> 6) > abs(tz))
	{
		return;
	}

	if (voxel == NULL)
	{
		// [RH] Added scaling
		int scaled_to = tex->GetScaledTopOffset();
		int scaled_bo = scaled_to - tex->GetScaledHeight();
		gzt = fz + thing->scaleY * scaled_to;
		gzb = fz + thing->scaleY * scaled_bo;
	}
	else
	{
		xscale = FixedMul(thing->scaleX, voxel->Scale);
		yscale = FixedMul(thing->scaleY, voxel->Scale);
		gzt = fz + MulScale8(yscale, voxel->Voxel->Mips[0].PivotZ) - thing->floorclip;
		gzb = fz + MulScale8(yscale, voxel->Voxel->Mips[0].PivotZ - (voxel->Voxel->Mips[0].SizeZ << 8));
		if (gzt <= gzb)
			return;
	}

	// killough 3/27/98: exclude things totally separated
	// from the viewer, by either water or fake ceilings
	// killough 4/11/98: improve sprite clipping for underwater/fake ceilings

	heightsec = thing->Sector->GetHeightSec();

	if (heightsec != NULL)	// only clip things which are in special sectors
	{
		if (fakeside == FAKED_AboveCeiling)
		{
			if (gzt < heightsec->ceilingplane.ZatPoint (fx, fy))
				return;
		}
		else if (fakeside == FAKED_BelowFloor)
		{
			if (gzb >= heightsec->floorplane.ZatPoint (fx, fy))
				return;
		}
		else
		{
			if (gzt < heightsec->floorplane.ZatPoint (fx, fy))
				return;
			if (gzb >= heightsec->ceilingplane.ZatPoint (fx, fy))
				return;
		}
	}

	if (voxel == NULL)
	{
		xscale = DivScale12 (centerxfrac, tz);

		// [RH] Reject sprites that are off the top or bottom of the screen
		if (MulScale12 (globaluclip, tz) > viewz - gzb ||
			MulScale12 (globaldclip, tz) < viewz - gzt)
		{
			return;
		}

		// [RH] Flip for mirrors and renderflags
		if ((MirrorFlags ^ thing->renderflags) & RF_XFLIP)
		{
			flip = !flip;
		}

		// calculate edges of the shape
		const fixed_t thingxscalemul = DivScale16(thing->scaleX, tex->xScale);

		tx -= (flip ? (tex->GetWidth() - tex->LeftOffset - 1) : tex->LeftOffset) * thingxscalemul;
		x1 = centerx + MulScale32 (tx, xscale);

		// off the right side?
		if (x1 > WindowRight)
			return;

		tx += tex->GetWidth() * thingxscalemul;
		x2 = centerx + MulScale32 (tx, xscale);

		// off the left side or too small?
		if ((x2 < WindowLeft || x2 <= x1))
			return;

		xscale = FixedDiv(FixedMul(thing->scaleX, xscale), tex->xScale);
		iscale = (tex->GetWidth() << FRACBITS) / (x2 - x1);
		x2--;

		fixed_t yscale = SafeDivScale16(thing->scaleY, tex->yScale);

		// store information in a vissprite
		vis = R_NewVisSprite();

		vis->xscale = xscale;
		vis->yscale = Scale(InvZtoScale, yscale, tz << 4);
		vis->idepth = (unsigned)DivScale32(1, tz) >> 1;	// tz is 20.12, so idepth ought to be 12.20, but signed math makes it 13.19
		vis->floorclip = FixedDiv (thing->floorclip, yscale);
		vis->texturemid = (tex->TopOffset << FRACBITS) - FixedDiv (viewz - fz + thing->floorclip, yscale);
		vis->x1 = x1 < WindowLeft ? WindowLeft : x1;
		vis->x2 = x2 > WindowRight ? WindowRight : x2;
		vis->angle = thing->angle;

		if (flip)
		{
			vis->startfrac = (tex->GetWidth() << FRACBITS) - 1;
			vis->xiscale = -iscale;
		}
		else
		{
			vis->startfrac = 0;
			vis->xiscale = iscale;
		}

		if (vis->x1 > x1)
			vis->startfrac += vis->xiscale * (vis->x1 - x1);
	}
	else
	{
		vis = R_NewVisSprite();

		vis->xscale = xscale;
		vis->yscale = yscale;
		vis->x1 = WindowLeft;
		vis->x2 = WindowRight;
		vis->idepth = (unsigned)DivScale32(1, MAX(tz, MINZ)) >> 1;
		vis->floorclip = thing->floorclip;

		fz -= thing->floorclip;

		vis->angle = thing->angle + voxel->AngleOffset;

		int voxelspin = (thing->flags & MF_DROPPED) ? voxel->DroppedSpin : voxel->PlacedSpin;
		if (voxelspin != 0)
		{
			double ang = double(I_FPSTime()) * voxelspin / 1000;
			vis->angle += angle_t(ang * (4294967296.f / 360));
		}

		// These are irrelevant for voxels.
		vis->texturemid = 0x1CEDBEEF;
		vis->startfrac	= 0x1CEDBEEF;
		vis->xiscale	= 0x1CEDBEEF;
	}

	// killough 3/27/98: save sector for special clipping later
	vis->heightsec = heightsec;
	vis->sector = thing->Sector;

	vis->cx = tx2;
	vis->depth = tz;
	vis->gx = fx;
	vis->gy = fy;
	vis->gz = fz;
	vis->gzb = gzb;		// [RH] use gzb, not thing->z
	vis->gzt = gzt;		// killough 3/27/98
	vis->renderflags = thing->renderflags;
	if(thing->flags5 & MF5_BRIGHT) vis->renderflags |= RF_FULLBRIGHT; // kg3D
	vis->RenderStyle = thing->RenderStyle;
	vis->FillColor = thing->fillcolor;
	vis->Translation = thing->Translation;		// [RH] thing translation table
	vis->FakeFlatStat = fakeside;
	vis->alpha = thing->alpha;
	vis->fakefloor = fakefloor;
	vis->fakeceiling = fakeceiling;

	if (voxel != NULL)
	{
		vis->voxel = voxel->Voxel;
		vis->bIsVoxel = true;
	}
	else
	{
		vis->pic = tex;
		vis->bIsVoxel = false;
	}

	// The software renderer cannot invert the source without inverting the overlay
	// too. That means if the source is inverted, we need to do the reverse of what
	// the invert overlay flag says to do.
	INTBOOL invertcolormap = (vis->RenderStyle.Flags & STYLEF_InvertOverlay);

	if (vis->RenderStyle.Flags & STYLEF_InvertSource)
	{
		invertcolormap = !invertcolormap;
	}

	FDynamicColormap *mybasecolormap = basecolormap;

	// Sprites that are added to the scene must fade to black.
	if (vis->RenderStyle == LegacyRenderStyles[STYLE_Add] && mybasecolormap->Fade != 0)
	{
		mybasecolormap = GetSpecialLights(mybasecolormap->Color, 0, mybasecolormap->Desaturate);
	}

	if (vis->RenderStyle.Flags & STYLEF_FadeToBlack)
	{
		if (invertcolormap)
		{ // Fade to white
			mybasecolormap = GetSpecialLights(mybasecolormap->Color, MAKERGB(255,255,255), mybasecolormap->Desaturate);
			invertcolormap = false;
		}
		else
		{ // Fade to black
			mybasecolormap = GetSpecialLights(mybasecolormap->Color, MAKERGB(0,0,0), mybasecolormap->Desaturate);
		}
	}

	// get light level
	if (fixedcolormap != NULL)
	{ // fixed map
		vis->colormap = fixedcolormap;
	}
	else
	{
		if (invertcolormap)
		{
			mybasecolormap = GetSpecialLights(mybasecolormap->Color, mybasecolormap->Fade.InverseColor(), mybasecolormap->Desaturate);
		}
		if (fixedlightlev >= 0)
		{
			vis->colormap = mybasecolormap->Maps + fixedlightlev;
		}
		else if (!foggy && ((thing->renderflags & RF_FULLBRIGHT) || (thing->flags5 & MF5_BRIGHT)))
		{ // full bright
			vis->colormap = mybasecolormap->Maps;
		}
		else
		{ // diminished light
			vis->colormap = mybasecolormap->Maps + (GETPALOOKUP (
				(fixed_t)DivScale12 (r_SpriteVisibility, MAX(tz, MINZ)), spriteshade) << COLORMAPSHIFT);
		}
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
	F3DFloor *rover;
	F3DFloor *fakeceiling = NULL;
	F3DFloor *fakefloor = NULL;

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
		// find fake level
		for(int i = 0; i < (int)frontsector->e->XFloor.ffloors.Size(); i++) {
			rover = frontsector->e->XFloor.ffloors[i];
			if(!(rover->flags & FF_EXISTS) || !(rover->flags & FF_RENDERPLANES)) continue;
			if(!(rover->flags & FF_SOLID) || rover->alpha != 255) continue;
			if(!fakefloor)
			{
				if(!(rover->top.plane->a) && !(rover->top.plane->b))
				{
					if(rover->top.plane->Zat0() <= thing->z) fakefloor = rover;
				}
			}
			if(!(rover->bottom.plane->a) && !(rover->bottom.plane->b))
			{
				if(rover->bottom.plane->Zat0() >= thing->z + thing->height) fakeceiling = rover;
			}
		}	
		R_ProjectSprite (thing, fakeside, fakefloor, fakeceiling);
		fakeceiling = NULL;
		fakefloor = NULL;
	}
}


//
// R_DrawPSprite
//
void R_DrawPSprite (pspdef_t* psp, int pspnum, AActor *owner, fixed_t sx, fixed_t sy)
{
	fixed_t 			tx;
	int 				x1;
	int 				x2;
	spritedef_t*		sprdef;
	spriteframe_t*		sprframe;
	FTextureID			picnum;
	WORD				flip;
	FTexture*			tex;
	vissprite_t*		vis;
	static vissprite_t	avis[NUMPSPRITES];
	bool noaccel;

	assert(pspnum >= 0 && pspnum < NUMPSPRITES);

	// decide which patch to use
	if ( (unsigned)psp->sprite >= (unsigned)sprites.Size ())
	{
		DPrintf ("R_DrawPSprite: invalid sprite number %i\n", psp->sprite);
		return;
	}
	sprdef = &sprites[psp->sprite];
	if (psp->frame >= sprdef->numframes)
	{
		DPrintf ("R_DrawPSprite: invalid sprite frame %i : %i\n", psp->sprite, psp->frame);
		return;
	}
	sprframe = &SpriteFrames[sprdef->spriteframes + psp->frame];

	picnum = sprframe->Texture[0];
	flip = sprframe->Flip & 1;
	tex = TexMan(picnum);

	if (tex->UseType == FTexture::TEX_Null)
		return;

	// calculate edges of the shape
	tx = sx-((320/2)<<FRACBITS);

	tx -= tex->GetScaledLeftOffset() << FRACBITS;
	x1 = (centerxfrac + FixedMul (tx, pspritexscale)) >>FRACBITS;
	VisPSpritesX1[pspnum] = x1;

	// off the right side
	if (x1 > viewwidth)
		return; 

	tx += tex->GetScaledWidth() << FRACBITS;
	x2 = ((centerxfrac + FixedMul (tx, pspritexscale)) >>FRACBITS) - 1;

	// off the left side
	if (x2 < 0)
		return;
	
	// store information in a vissprite
	vis = &avis[pspnum];
	vis->renderflags = owner->renderflags;
	vis->floorclip = 0;


	vis->texturemid = MulScale16((BASEYCENTER<<FRACBITS) - sy, tex->yScale) + (tex->TopOffset << FRACBITS);


	if (camera->player && (RenderTarget != screen ||
		viewheight == RenderTarget->GetHeight() ||
		(RenderTarget->GetWidth() > 320 && !st_scale)))
	{	// Adjust PSprite for fullscreen views
		AWeapon *weapon = NULL;
		if (camera->player != NULL)
		{
			weapon = camera->player->ReadyWeapon;
		}
		if (pspnum <= ps_flash && weapon != NULL && weapon->YAdjust != 0)
		{
			if (RenderTarget != screen || viewheight == RenderTarget->GetHeight())
			{
				vis->texturemid -= weapon->YAdjust;
			}
			else
			{
				vis->texturemid -= FixedMul (StatusBar->GetDisplacement (),
					weapon->YAdjust);
			}
		}
	}
	if (pspnum <= ps_flash)
	{ // Move the weapon down for 1280x1024.
		vis->texturemid -= BaseRatioSizes[WidescreenRatio][2];
	}
	vis->x1 = x1 < 0 ? 0 : x1;
	vis->x2 = x2 >= viewwidth ? viewwidth-1 : x2;
	vis->xscale = DivScale16(pspritexscale, tex->xScale);
	vis->yscale = DivScale16(pspriteyscale, tex->yScale);
	vis->Translation = 0;		// [RH] Use default colors
	vis->pic = tex;

	if (flip)
	{
		vis->xiscale = -MulScale16(pspritexiscale, tex->xScale);
		vis->startfrac = (tex->GetWidth() << FRACBITS) - 1;
	}
	else
	{
		vis->xiscale = MulScale16(pspritexiscale, tex->xScale);
		vis->startfrac = 0;
	}

	if (vis->x1 > x1)
		vis->startfrac += vis->xiscale*(vis->x1-x1);

	noaccel = false;
	if (pspnum <= ps_flash)
	{
		vis->alpha = owner->alpha;
		vis->RenderStyle = owner->RenderStyle;

		// The software renderer cannot invert the source without inverting the overlay
		// too. That means if the source is inverted, we need to do the reverse of what
		// the invert overlay flag says to do.
		INTBOOL invertcolormap = (vis->RenderStyle.Flags & STYLEF_InvertOverlay);

		if (vis->RenderStyle.Flags & STYLEF_InvertSource)
		{
			invertcolormap = !invertcolormap;
		}

		FDynamicColormap *mybasecolormap = basecolormap;

		if (vis->RenderStyle.Flags & STYLEF_FadeToBlack)
		{
			if (invertcolormap)
			{ // Fade to white
				mybasecolormap = GetSpecialLights(mybasecolormap->Color, MAKERGB(255,255,255), mybasecolormap->Desaturate);
				invertcolormap = false;
			}
			else
			{ // Fade to black
				mybasecolormap = GetSpecialLights(mybasecolormap->Color, MAKERGB(0,0,0), mybasecolormap->Desaturate);
			}
		}

		if (realfixedcolormap != NULL)
		{ // fixed color
			vis->colormap = realfixedcolormap->Colormap;
		}
		else
		{
			if (invertcolormap)
			{
				mybasecolormap = GetSpecialLights(mybasecolormap->Color, mybasecolormap->Fade.InverseColor(), mybasecolormap->Desaturate);
			}
			if (fixedlightlev >= 0)
			{
				vis->colormap = mybasecolormap->Maps + fixedlightlev;
			}
			else if (!foggy && psp->state->GetFullbright())
			{ // full bright
				vis->colormap = mybasecolormap->Maps;	// [RH] use basecolormap
			}
			else
			{ // local light
				vis->colormap = mybasecolormap->Maps + (GETPALOOKUP (0, spriteshade) << COLORMAPSHIFT);
			}
		}
		if (camera->Inventory != NULL)
		{
			lighttable_t *oldcolormap = vis->colormap;
			camera->Inventory->AlterWeaponSprite (vis);
			if (vis->colormap != oldcolormap)
			{
				// The colormap has changed. Is it one we can easily identify?
				// If not, then don't bother trying to identify it for
				// hardware accelerated drawing.
				if (vis->colormap < SpecialColormaps[0].Colormap || 
					vis->colormap > SpecialColormaps.Last().Colormap)
				{
					noaccel = true;
				}
				// Has the basecolormap changed? If so, we can't hardware accelerate it,
				// since we don't know what it is anymore.
				else if (vis->colormap < mybasecolormap->Maps ||
					vis->colormap >= mybasecolormap->Maps + NUMCOLORMAPS*256)
				{
					noaccel = true;
				}
			}
		}
		// If we're drawing with a special colormap, but shaders for them are disabled, do
		// not accelerate.
		if (!r_shadercolormaps && (vis->colormap >= SpecialColormaps[0].Colormap &&
			vis->colormap <= SpecialColormaps.Last().Colormap))
		{
			noaccel = true;
		}
		// If the main colormap has fixed lights, and this sprite is being drawn with that
		// colormap, disable acceleration so that the lights can remain fixed.
		if (!noaccel &&
			NormalLightHasFixedLights && mybasecolormap == &NormalLight &&
			vis->pic->UseBasePalette())
		{
			noaccel = true;
		}
		VisPSpritesBaseColormap[pspnum] = mybasecolormap;
	}
	else
	{
		VisPSpritesBaseColormap[pspnum] = basecolormap;
		vis->colormap = basecolormap->Maps;
		vis->RenderStyle = STYLE_Normal;
	}

	// Check for hardware-assisted 2D. If it's available, and this sprite is not
	// fuzzy, don't draw it until after the switch to 2D mode.
	if (!noaccel && RenderTarget == screen && (DFrameBuffer *)screen->Accel2D)
	{
		FRenderStyle style = vis->RenderStyle;
		style.CheckFuzz();
		if (style.BlendOp != STYLEOP_Fuzz)
		{
			VisPSprites[pspnum] = vis;
			return;
		}
	}
	R_DrawVisSprite (vis);
}



//==========================================================================
//
// R_DrawPlayerSprites
//
//==========================================================================

void R_DrawPlayerSprites ()
{
	int 		i;
	int 		lightnum;
	pspdef_t*	psp;
	sector_t*	sec = NULL;
	static sector_t tempsec;
	int			floorlight, ceilinglight;
	F3DFloor *rover;

	if (!r_drawplayersprites ||
		!camera->player ||
		(players[consoleplayer].cheats & CF_CHASECAM))
		return;

	if(fixedlightlev < 0 && viewsector->e && viewsector->e->XFloor.lightlist.Size()) {
		for(i = viewsector->e->XFloor.lightlist.Size() - 1; i >= 0; i--)
			if(viewz <= viewsector->e->XFloor.lightlist[i].plane.Zat0()) {
				rover = viewsector->e->XFloor.lightlist[i].caster;
				if(rover) {
					if(rover->flags & FF_DOUBLESHADOW && viewz <= rover->bottom.plane->Zat0())
						break;
					sec = rover->model;
					if(rover->flags & FF_FADEWALLS)
						basecolormap = sec->ColorMap;
					else
						basecolormap = viewsector->e->XFloor.lightlist[i].extra_colormap;
				}
				break;
			}
		if(!sec) {
			sec = viewsector;
			basecolormap = sec->ColorMap;
		}
		floorlight = ceilinglight = sec->lightlevel;
	} else {
		// This used to use camera->Sector but due to interpolation that can be incorrect
		// when the interpolated viewpoint is in a different sector than the camera.
		sec = R_FakeFlat (viewsector, &tempsec, &floorlight,
			&ceilinglight, false);

		// [RH] set basecolormap
		basecolormap = sec->ColorMap;
	}

	// [RH] set foggy flag
	foggy = (level.fadeto || basecolormap->Fade || (level.flags & LEVEL_HASFADETABLE));
	r_actualextralight = foggy ? 0 : extralight << 4;

	// get light level
	lightnum = ((floorlight + ceilinglight) >> 1) + r_actualextralight;
	spriteshade = LIGHT2SHADE(lightnum) - 24*FRACUNIT;

	// clip to screen bounds
	mfloorclip = screenheightarray;
	mceilingclip = zeroarray;

	if (camera->player != NULL)
	{
		fixed_t centerhack = centeryfrac;
		fixed_t ofsx, ofsy;

		centery = viewheight >> 1;
		centeryfrac = centery << FRACBITS;

		P_BobWeapon (camera->player, &camera->player->psprites[ps_weapon], &ofsx, &ofsy);

		// add all active psprites
		for (i = 0, psp = camera->player->psprites;
			 i < NUMPSPRITES;
			 i++, psp++)
		{
			// [RH] Don't draw the targeter's crosshair if the player already has a crosshair set.
			if (psp->state && (i != ps_targetcenter || CrosshairImage == NULL))
			{
				R_DrawPSprite (psp, i, camera, psp->sx + ofsx, psp->sy + ofsy);
			}
			// [RH] Don't bob the targeter.
			if (i == ps_flash)
			{
				ofsx = ofsy = 0;
			}
		}

		centeryfrac = centerhack;
		centery = centerhack >> FRACBITS;
	}
}

//==========================================================================
//
// R_DrawRemainingPlayerSprites
//
// Called from D_Display to draw sprites that were not drawn by
// R_DrawPlayerSprites().
//
//==========================================================================

void R_DrawRemainingPlayerSprites()
{
	for (int i = 0; i < NUMPSPRITES; ++i)
	{
		vissprite_t *vis;
		
		vis = VisPSprites[i];
		VisPSprites[i] = NULL;

		if (vis != NULL)
		{
			FDynamicColormap *colormap = VisPSpritesBaseColormap[i];
			bool flip = vis->xiscale < 0;
			FSpecialColormap *special = NULL;
			PalEntry overlay = 0;
			FColormapStyle colormapstyle;
			bool usecolormapstyle = false;

			if (vis->colormap >= SpecialColormaps[0].Colormap && 
				vis->colormap < SpecialColormaps[SpecialColormaps.Size()].Colormap)
			{
				// Yuck! There needs to be a better way to store colormaps in the vissprite... :(
				ptrdiff_t specialmap = (vis->colormap - SpecialColormaps[0].Colormap) / sizeof(FSpecialColormap);
				special = &SpecialColormaps[specialmap];
			}
			else if (colormap->Color == PalEntry(255,255,255) &&
				colormap->Desaturate == 0)
			{
				overlay = colormap->Fade;
				overlay.a = BYTE(((vis->colormap - colormap->Maps) >> 8) * 255 / NUMCOLORMAPS);
			}
			else
			{
				usecolormapstyle = true;
				colormapstyle.Color = colormap->Color;
				colormapstyle.Fade = colormap->Fade;
				colormapstyle.Desaturate = colormap->Desaturate;
				colormapstyle.FadeLevel = ((vis->colormap - colormap->Maps) >> 8) / float(NUMCOLORMAPS);
			}
			screen->DrawTexture(vis->pic,
				viewwindowx + VisPSpritesX1[i],
				viewwindowy + viewheight/2 - (vis->texturemid / 65536.0) * (vis->yscale / 65536.0) - 0.5,
				DTA_DestWidthF, FIXED2FLOAT(vis->pic->GetWidth() * vis->xscale),
				DTA_DestHeightF, FIXED2FLOAT(vis->pic->GetHeight() * vis->yscale),
				DTA_Translation, TranslationToTable(vis->Translation),
				DTA_FlipX, flip,
				DTA_TopOffset, 0,
				DTA_LeftOffset, 0,
				DTA_ClipLeft, viewwindowx,
				DTA_ClipTop, viewwindowy,
				DTA_ClipRight, viewwindowx + viewwidth,
				DTA_ClipBottom, viewwindowy + viewheight,
				DTA_Alpha, vis->alpha,
				DTA_RenderStyle, vis->RenderStyle,
				DTA_FillColor, vis->FillColor,
				DTA_SpecialColormap, special,
				DTA_ColorOverlay, overlay.d,
				DTA_ColormapStyle, usecolormapstyle ? &colormapstyle : NULL,
				TAG_DONE);
		}
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
// Sort vissprites by depth, far to near
static bool sv_compare(vissprite_t *a, vissprite_t *b)
{
	return a->idepth > b->idepth;
}

#if 0
static drawseg_t **drawsegsorter;
static int drawsegsortersize = 0;

// Sort vissprites by leftmost column, left to right
static int STACK_ARGS sv_comparex (const void *arg1, const void *arg2)
{
	return (*(vissprite_t **)arg2)->x1 - (*(vissprite_t **)arg1)->x1;
}

// Sort drawsegs by rightmost column, left to right
static int STACK_ARGS sd_comparex (const void *arg1, const void *arg2)
{
	return (*(drawseg_t **)arg2)->x2 - (*(drawseg_t **)arg1)->x2;
}

CVAR (Bool, r_splitsprites, true, CVAR_ARCHIVE)

// Split up vissprites that intersect drawsegs
void R_SplitVisSprites ()
{
	size_t start, stop;
	size_t numdrawsegs = ds_p - firstdrawseg;
	size_t numsprites;
	size_t spr, dseg, dseg2;

	if (!r_splitsprites)
		return;

	if (numdrawsegs == 0 || vissprite_p - firstvissprite == 0)
		return;

	// Sort drawsegs from left to right
	if (numdrawsegs > drawsegsortersize)
	{
		if (drawsegsorter != NULL)
			delete[] drawsegsorter;
		drawsegsortersize = numdrawsegs * 2;
		drawsegsorter = new drawseg_t *[drawsegsortersize];
	}
	for (dseg = dseg2 = 0; dseg < numdrawsegs; ++dseg)
	{
		// Drawsegs that don't clip any sprites don't need to be considered.
		if (firstdrawseg[dseg].silhouette)
		{
			drawsegsorter[dseg2++] = &firstdrawseg[dseg];
		}
	}
	numdrawsegs = dseg2;
	if (numdrawsegs == 0)
	{
		return;
	}
	qsort (drawsegsorter, numdrawsegs, sizeof(drawseg_t *), sd_comparex);

	// Now sort vissprites from left to right, and walk them simultaneously
	// with the drawsegs, splitting any that intersect.
	start = firstvissprite - vissprites;

	int p = 0;
	do
	{
		p++;
		R_SortVisSprites (sv_comparex, start);
		stop = vissprite_p - vissprites;
		numsprites = stop - start;

		spr = dseg = 0;
		do
		{
			vissprite_t *vis = spritesorter[spr], *vis2;

			// Skip drawsegs until we get to one that doesn't end before the sprite
			// begins.
			while (dseg < numdrawsegs && drawsegsorter[dseg]->x2 <= vis->x1)
			{
				dseg++;
			}
			// Now split the sprite against any drawsegs it intersects
			for (dseg2 = dseg; dseg2 < numdrawsegs; dseg2++)
			{
				drawseg_t *ds = drawsegsorter[dseg2];

				if (ds->x1 > vis->x2 || ds->x2 < vis->x1)
					continue;

				if ((vis->idepth < ds->siz1) != (vis->idepth < ds->siz2))
				{ // The drawseg is crossed; find the x where the intersection occurs
					int cross = Scale (vis->idepth - ds->siz1, ds->sx2 - ds->sx1, ds->siz2 - ds->siz1) + ds->sx1 + 1;

/*					if (cross < ds->x1 || cross > ds->x2)
					{ // The original seg is crossed, but the drawseg is not
						continue;
					}
*/					if (cross <= vis->x1 || cross >= vis->x2)
					{ // Don't create 0-sized sprites
						continue;
					}

					vis->bSplitSprite = true;

					// Create a new vissprite for the right part of the sprite
					vis2 = R_NewVisSprite ();
					*vis2 = *vis;
					vis2->startfrac += vis2->xiscale * (cross - vis2->x1);
					vis->x2 = cross-1;
					vis2->x1 = cross;
					//vis2->alpha /= 2;
					//vis2->RenderStyle = STYLE_Add;

					if (vis->idepth < ds->siz1)
					{ // Left is in back, right is in front
						vis->sector  = ds->curline->backsector;
						vis2->sector = ds->curline->frontsector;
					}
					else
					{ // Right is in front, left is in back
						vis->sector  = ds->curline->frontsector;
						vis2->sector = ds->curline->backsector;
					}
				}
			}
		}
		while (dseg < numdrawsegs && ++spr < numsprites);

		// Repeat for any new sprites that were added.
	}
	while (start = stop, stop != vissprite_p - vissprites);
}
#endif

#ifdef __GNUC__
static void swap(vissprite_t *&a, vissprite_t *&b)
{
	vissprite_t *t = a;
	a = b;
	b = t;
}
#endif

void R_SortVisSprites (bool (*compare)(vissprite_t *, vissprite_t *), size_t first)
{
	int i;
	vissprite_t **spr;

	vsprcount = int(vissprite_p - &vissprites[first]);

	if (vsprcount == 0)
		return;

	if (spritesortersize < MaxVisSprites)
	{
		if (spritesorter != NULL)
			delete[] spritesorter;
		spritesorter = new vissprite_t *[MaxVisSprites];
		spritesortersize = MaxVisSprites;
	}

	if (!(i_compatflags & COMPATF_SPRITESORT))
	{
		for (i = 0, spr = firstvissprite; i < vsprcount; i++, spr++)
		{
			spritesorter[i] = *spr;
		}
	}
	else
	{
		// If the compatibility option is on sprites of equal distance need to
		// be sorted in inverse order. This is most easily achieved by
		// filling the sort array backwards before the sort.
		for (i = 0, spr = firstvissprite + vsprcount-1; i < vsprcount; i++, spr--)
		{
			spritesorter[i] = *spr;
		}
	}

	std::stable_sort(&spritesorter[0], &spritesorter[vsprcount], compare);
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
	int x1, x2;
	int r1, r2;
	short topclip, botclip;
	short *clip1, *clip2;
	lighttable_t *colormap = spr->colormap;
	F3DFloor *rover;
	FDynamicColormap *mybasecolormap;

	// [RH] Check for particles
	if (!spr->bIsVoxel && spr->pic == NULL)
	{
		// kg3D - reject invisible parts
		if ((fake3D & FAKE3D_CLIPBOTTOM) && spr->gz <= sclipBottom) return;
		if ((fake3D & FAKE3D_CLIPTOP)    && spr->gz >= sclipTop) return;
		R_DrawParticle (spr);
		return;
	}

	x1 = spr->x1;
	x2 = spr->x2;

	// [RH] Quickly reject sprites with bad x ranges.
	if (x1 > x2)
		return;

	// [RH] Sprites split behind a one-sided line can also be discarded.
	if (spr->sector == NULL)
		return;

	// kg3D - reject invisible parts
	if ((fake3D & FAKE3D_CLIPBOTTOM) && spr->gzt <= sclipBottom) return;
	if ((fake3D & FAKE3D_CLIPTOP)    && spr->gzb >= sclipTop) return;

	// kg3D - correct colors now
	if (!fixedcolormap && fixedlightlev < 0 && spr->sector->e && spr->sector->e->XFloor.lightlist.Size()) 
	{
		if (!(fake3D & FAKE3D_CLIPTOP))
		{
			sclipTop = spr->sector->ceilingplane.ZatPoint(viewx, viewy);
		}
		sector_t *sec = NULL;
		for (i = spr->sector->e->XFloor.lightlist.Size() - 1; i >= 0; i--)
		{
			if (sclipTop <= spr->sector->e->XFloor.lightlist[i].plane.Zat0()) 
			{
				rover = spr->sector->e->XFloor.lightlist[i].caster;
				if (rover) 
				{
					if (rover->flags & FF_DOUBLESHADOW && sclipTop <= rover->bottom.plane->Zat0())
					{
						break;
					}
					sec = rover->model;
					if (rover->flags & FF_FADEWALLS)
					{
						mybasecolormap = sec->ColorMap;
					}
					else
					{
						mybasecolormap = spr->sector->e->XFloor.lightlist[i].extra_colormap;
					}
				}
				break;
			}
		}
		// found new values, recalculate
		if (sec) 
		{
			INTBOOL invertcolormap = (spr->RenderStyle.Flags & STYLEF_InvertOverlay);

			if (spr->RenderStyle.Flags & STYLEF_InvertSource)
			{
				invertcolormap = !invertcolormap;
			}

			// Sprites that are added to the scene must fade to black.
			if (spr->RenderStyle == LegacyRenderStyles[STYLE_Add] && mybasecolormap->Fade != 0)
			{
				mybasecolormap = GetSpecialLights(mybasecolormap->Color, 0, mybasecolormap->Desaturate);
			}

			if (spr->RenderStyle.Flags & STYLEF_FadeToBlack)
			{
				if (invertcolormap)
				{ // Fade to white
					mybasecolormap = GetSpecialLights(mybasecolormap->Color, MAKERGB(255,255,255), mybasecolormap->Desaturate);
					invertcolormap = false;
				}
				else
				{ // Fade to black
					mybasecolormap = GetSpecialLights(mybasecolormap->Color, MAKERGB(0,0,0), mybasecolormap->Desaturate);
				}
			}

			// get light level
			if (invertcolormap)
			{
				mybasecolormap = GetSpecialLights(mybasecolormap->Color, mybasecolormap->Fade.InverseColor(), mybasecolormap->Desaturate);
			}
			if (fixedlightlev >= 0)
			{
				spr->colormap = mybasecolormap->Maps + fixedlightlev;
			}
			else if (!foggy && (spr->renderflags & RF_FULLBRIGHT))
			{ // full bright
				spr->colormap = mybasecolormap->Maps;
			}
			else
			{ // diminished light
				spriteshade = LIGHT2SHADE(sec->lightlevel + r_actualextralight);
				spr->colormap = mybasecolormap->Maps + (GETPALOOKUP (
					(fixed_t)DivScale12 (r_SpriteVisibility, spr->depth), spriteshade) << COLORMAPSHIFT);
			}
		}
	}

	// [RH] Initialize the clipping arrays to their largest possible range
	// instead of using a special "not clipped" value. This eliminates
	// visual anomalies when looking down and should be faster, too.
	topclip = 0;
	botclip = viewheight;

	// killough 3/27/98:
	// Clip the sprite against deep water and/or fake ceilings.
	// [RH] rewrote this to be based on which part of the sector is really visible

	fixed_t scale = MulScale19 (InvZtoScale, spr->idepth);
	fixed_t hzb = FIXED_MIN, hzt = FIXED_MAX;

	if (spr->bIsVoxel && spr->floorclip != 0)
	{
		hzb = spr->gzb;
	}

	if (spr->heightsec && !(spr->heightsec->MoreFlags & SECF_IGNOREHEIGHTSEC))
	{ // only things in specially marked sectors
		if (spr->FakeFlatStat != FAKED_AboveCeiling)
		{
			fixed_t hz = spr->heightsec->floorplane.ZatPoint (spr->gx, spr->gy);
			fixed_t h = (centeryfrac - FixedMul (hz-viewz, scale)) >> FRACBITS;

			if (spr->FakeFlatStat == FAKED_BelowFloor)
			{ // seen below floor: clip top
				if (!spr->bIsVoxel && h > topclip)
				{
					topclip = MIN<short> (h, viewheight);
				}
				hzt = MIN(hzt, hz);
			}
			else
			{ // seen in the middle: clip bottom
				if (!spr->bIsVoxel && h < botclip)
				{
					botclip = MAX<short> (0, h);
				}
				hzb = MAX(hzb, hz);
			}
		}
		if (spr->FakeFlatStat != FAKED_BelowFloor)
		{
			fixed_t hz = spr->heightsec->ceilingplane.ZatPoint (spr->gx, spr->gy);
			fixed_t h = (centeryfrac - FixedMul (hz-viewz, scale)) >> FRACBITS;

			if (spr->FakeFlatStat == FAKED_AboveCeiling)
			{ // seen above ceiling: clip bottom
				if (!spr->bIsVoxel && h < botclip)
				{
					botclip = MAX<short> (0, h);
				}
				hzb = MAX(hzb, hz);
			}
			else
			{ // seen in the middle: clip top
				if (!spr->bIsVoxel && h > topclip)
				{
					topclip = MIN<short> (h, viewheight);
				}
				hzt = MIN(hzt, hz);
			}
		}
	}
	// killough 3/27/98: end special clipping for deep water / fake ceilings
	else if (!spr->bIsVoxel && spr->floorclip)
	{ // [RH] Move floorclip stuff from R_DrawVisSprite to here
		int clip = ((centeryfrac - FixedMul (spr->texturemid -
			(spr->pic->GetHeight() << FRACBITS) +
			spr->floorclip, spr->yscale)) >> FRACBITS);
		if (clip < botclip)
		{
			botclip = MAX<short> (0, clip);
		}
	}

	if (fake3D & FAKE3D_CLIPBOTTOM)
	{
		if (!spr->bIsVoxel)
		{
			fixed_t h = sclipBottom;
			if (spr->fakefloor)
			{
				fixed_t floorz = spr->fakefloor->top.plane->Zat0();
				if (viewz > floorz && floorz == sclipBottom )
				{
					h = spr->fakefloor->bottom.plane->Zat0();
				}
			}
			h = (centeryfrac - FixedMul(h-viewz, scale)) >> FRACBITS;
			if (h < botclip)
			{
				botclip = MAX<short>(0, h);
			}
		}
		hzb = MAX(hzb, sclipBottom);
	}
	if (fake3D & FAKE3D_CLIPTOP)
	{
		if (!spr->bIsVoxel)
		{
			fixed_t h = sclipTop;

			if (spr->fakeceiling != NULL)
			{
				fixed_t ceilingz = spr->fakeceiling->bottom.plane->Zat0();
				if (viewz < ceilingz && ceilingz == sclipTop)
				{
					h = spr->fakeceiling->top.plane->Zat0();
				}
			}
			h = (centeryfrac - FixedMul (h-viewz, scale)) >> FRACBITS;
			if (h > topclip)
			{
				topclip = MIN<short>(h, viewheight);
			}
		}
		hzt = MIN(hzt, sclipTop);
	}

#if 0
	// [RH] Sprites that were split by a drawseg should also be clipped
	// by the sector's floor and ceiling. (Not sure how/if to handle this
	// with fake floors, since those already do clipping.)
	if (spr->bSplitSprite &&
		(spr->heightsec == NULL || (spr->heightsec->MoreFlags & SECF_IGNOREHEIGHTSEC)))
	{
		fixed_t h = spr->sector->floorplane.ZatPoint (spr->gx, spr->gy);
		h = (centeryfrac - FixedMul (h-viewz, scale)) >> FRACBITS;
		if (h < botclip)
		{
			botclip = MAX<short> (0, h);
		}
		h = spr->sector->ceilingplane.ZatPoint (spr->gx, spr->gy);
		h = (centeryfrac - FixedMul (h-viewz, scale)) >> FRACBITS;
		if (h > topclip)
		{
			topclip = MIN<short> (h, viewheight);
		}
	}
#endif

	if (topclip >= botclip)
	{
		spr->colormap = colormap;
		return;
	}

	i = x2 - x1 + 1;
	clip1 = clipbot + x1;
	clip2 = cliptop + x1;
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
		// kg3D - no clipping on fake segs
		if(ds->fake) continue;
		// determine if the drawseg obscures the sprite
		if (ds->x1 > x2 || ds->x2 < x1 ||
			(!(ds->silhouette & SIL_BOTH) && ds->maskedtexturecol == -1 &&
			 !ds->bFogBoundary) )
		{
			// does not cover sprite
			continue;
		}

		r1 = MAX<int> (ds->x1, x1);
		r2 = MIN<int> (ds->x2, x2);

		fixed_t neardepth, fardepth;
		if (ds->sz1 < ds->sz2)
		{
			neardepth = ds->sz1, fardepth = ds->sz2;
		}
		else
		{
			neardepth = ds->sz2, fardepth = ds->sz1;
		}
		if (neardepth > spr->depth || (fardepth > spr->depth &&
			// Check if sprite is in front of draw seg:
			DMulScale32(spr->gy - ds->curline->v1->y, ds->curline->v2->x - ds->curline->v1->x,
						ds->curline->v1->x - spr->gx, ds->curline->v2->y - ds->curline->v1->y) <= 0))
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

	if (!spr->bIsVoxel)
	{
		mfloorclip = clipbot;
		mceilingclip = cliptop;
		R_DrawVisSprite (spr);
	}
	else
	{
		// If it is completely clipped away, don't bother drawing it.
		if (cliptop[x2] >= clipbot[x2])
		{
			for (i = x1; i < x2; ++i)
			{
				if (cliptop[i] < clipbot[i])
				{
					break;
				}
			}
			if (i == x2)
			{
				spr->colormap = colormap;
				return;
			}
		}
		int minvoxely = spr->gzt <= hzt ? 0 : (spr->gzt - hzt) / spr->yscale;
		int maxvoxely = spr->gzb > hzb ? INT_MAX : (spr->gzt - hzb) / spr->yscale;
		R_DrawVisVoxel(spr, minvoxely, maxvoxely, cliptop, clipbot);
	}
	spr->colormap = colormap;
}

// kg3D:
// R_DrawMasked contains sorting
// original renamed to R_DrawMaskedSingle

void R_DrawMaskedSingle (bool renew)
{
	drawseg_t *ds;
	int i;

#if 0
	R_SplitVisSprites ();
#endif

	for (i = vsprcount; i > 0; i--)
	{
		R_DrawSprite (spritesorter[i-1]);
	}

	// render any remaining masked mid textures

	// Modified by Lee Killough:
	// (pointer check was originally nonportable
	// and buggy, by going past LEFT end of array):

	//		for (ds=ds_p-1 ; ds >= drawsegs ; ds--)    old buggy code

	if (renew)
	{
		fake3D |= FAKE3D_REFRESHCLIP;
	}
	for (ds = ds_p; ds-- > firstdrawseg; )	// new -- killough
	{
		// kg3D - no fake segs
		if (ds->fake) continue;
		if (ds->maskedtexturecol != -1 || ds->bFogBoundary)
		{
			R_RenderMaskedSegRange (ds, ds->x1, ds->x2);
		}
	}
}

void R_DrawHeightPlanes(fixed_t height); // kg3D - fake planes

void R_DrawMasked (void)
{
	R_SortVisSprites (sv_compare, firstvissprite - vissprites);

	if (height_top == NULL)
	{ // kg3D - no visible 3D floors, normal rendering
		R_DrawMaskedSingle(false);
	}
	else
	{ // kg3D - correct sorting
		HeightLevel *hl;

		// ceilings
		for (hl = height_cur; hl != NULL && hl->height >= viewz; hl = hl->prev)
		{
			if (hl->next)
			{
				fake3D = FAKE3D_CLIPBOTTOM | FAKE3D_CLIPTOP;
				sclipTop = hl->next->height;
			}
			else
			{
				fake3D = FAKE3D_CLIPBOTTOM;
			}
			sclipBottom = hl->height;
			R_DrawMaskedSingle(true);
			R_DrawHeightPlanes(hl->height);
		}

		// floors
		fake3D = FAKE3D_DOWN2UP | FAKE3D_CLIPTOP;
		sclipTop = height_top->height;
		R_DrawMaskedSingle(true);
		hl = height_top;
		for (hl = height_top; hl != NULL && hl->height < viewz; hl = hl->next)
		{
			R_DrawHeightPlanes(hl->height);
			if (hl->next)
			{
				fake3D = FAKE3D_DOWN2UP | FAKE3D_CLIPTOP | FAKE3D_CLIPBOTTOM;
				sclipTop = hl->next->height;
			}
			else
			{
				fake3D = FAKE3D_DOWN2UP | FAKE3D_CLIPBOTTOM;
			}
			sclipBottom = hl->height;
			R_DrawMaskedSingle(true);
		}
		R_3D_DeleteHeights();
		fake3D = 0;
	}
	// draw the psprites on top of everything but does not draw on side views
	if (!viewangleoffset)
	{
		R_DrawPlayerSprites ();
	}
}


//
// [RH] Particle functions
//

// [BC] Allow the maximum number of particles to be specified by a cvar (so people
// with lots of nice hardware can have lots of particles!).
CUSTOM_CVAR( Int, r_maxparticles, 4000, CVAR_ARCHIVE )
{
	if ( self == 0 )
		self = 4000;
	else if ( self < 100 )
		self = 100;

	if ( gamestate != GS_STARTUP )
	{
		R_DeinitParticles( );
		R_InitParticles( );
	}
}

void R_InitParticles ()
{
	const char *i;

	if ((i = Args->CheckValue ("-numparticles")))
		NumParticles = atoi (i);
	// [BC] Use r_maxparticles now.
	else
		NumParticles = r_maxparticles;

	// This should be good, but eh...
	if ( NumParticles < 100 )
		NumParticles = 100;

	R_DeinitParticles();
	Particles = new particle_t[NumParticles];
	R_ClearParticles ();
	atterm (R_DeinitParticles);
}

void R_DeinitParticles()
{
	if (Particles != NULL)
	{
		delete[] Particles;
		Particles = NULL;
	}
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
		int ssnum = int(ssec-subsectors);
		Particles[i].subsector = ssec;
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
	BYTE*				map;

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

	if (x1 >= x2)
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
	heightsec = sector->GetHeightSec();

	const secplane_t *topplane;
	const secplane_t *botplane;
	FTextureID toppic;
	FTextureID botpic;

	if (heightsec)	// only clip things which are in special sectors
	{
		if (fakeside == FAKED_AboveCeiling)
		{
			topplane = &sector->ceilingplane;
			botplane = &heightsec->ceilingplane;
			toppic = sector->GetTexture(sector_t::ceiling);
			botpic = heightsec->GetTexture(sector_t::ceiling);
			map = heightsec->ColorMap->Maps;
		}
		else if (fakeside == FAKED_BelowFloor)
		{
			topplane = &heightsec->floorplane;
			botplane = &sector->floorplane;
			toppic = heightsec->GetTexture(sector_t::floor);
			botpic = sector->GetTexture(sector_t::floor);
			map = heightsec->ColorMap->Maps;
		}
		else
		{
			topplane = &heightsec->ceilingplane;
			botplane = &heightsec->floorplane;
			toppic = heightsec->GetTexture(sector_t::ceiling);
			botpic = heightsec->GetTexture(sector_t::floor);
			map = sector->ColorMap->Maps;
		}
	}
	else
	{
		topplane = &sector->ceilingplane;
		botplane = &sector->floorplane;
		toppic = sector->GetTexture(sector_t::ceiling);
		botpic = sector->GetTexture(sector_t::floor);
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
	vis->idepth = (DWORD)DivScale32 (1, tz) >> 1;
	vis->cx = tx;
	vis->gx = particle->x;
	vis->gy = particle->y;
	vis->gz = particle->z; // kg3D
	vis->gzb = y1;
	vis->gzt = y2;
	vis->x1 = x1;
	vis->x2 = x2;
	vis->Translation = 0;
	vis->startfrac = particle->color;
	vis->pic = NULL;
	vis->bIsVoxel = false;
	vis->renderflags = particle->trans;
	vis->FakeFlatStat = fakeside;
	vis->floorclip = 0;

	if (fixedlightlev >= 0)
	{
		vis->colormap = map + fixedlightlev;
	}
	else if (fixedcolormap)
	{
		vis->colormap = fixedcolormap;
	}
	else
	{
		// Using MulScale15 instead of 16 makes particles slightly more visible
		// than regular sprites.
		vis->colormap = map + (GETPALOOKUP (MulScale15 (tiz, r_SpriteVisibility),
			shade) << COLORMAPSHIFT);
	}
}

static void R_DrawMaskedSegsBehindParticle (const vissprite_t *vis)
{
	const int x1 = vis->x1;
	const int x2 = vis->x2;

	// Draw any masked textures behind this particle so that when the
	// particle is drawn, it will be in front of them.
	for (unsigned int p = InterestingDrawsegs.Size(); p-- > FirstInterestingDrawseg; )
	{
		drawseg_t *ds = &drawsegs[InterestingDrawsegs[p]];
		// kg3D - no fake segs
		if(ds->fake) continue;
		if (ds->x1 >= x2 || ds->x2 < x1)
		{
			continue;
		}
		if (Scale (ds->siz2 - ds->siz1, (x2 + x1)/2 - ds->sx1, ds->sx2 - ds->sx1) + ds->siz1 < vis->idepth)
		{
			R_RenderMaskedSegRange (ds, MAX<int> (ds->x1, x1), MIN<int> (ds->x2, x2-1));
		}
	}
}

void R_DrawParticle (vissprite_t *vis)
{
	DWORD *bg2rgb;
	int spacing;
	BYTE *dest;
	DWORD fg;
	BYTE color = vis->colormap[vis->startfrac];
	int yl = vis->gzb;
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

	spacing = RenderTarget->GetPitch() - countbase;
	dest = ylookup[yl] + x1 + dc_destorg;

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

static fixed_t distrecip(fixed_t y)
{
	y >>= 3;
	return y == 0 ? 0 : SafeDivScale32(centerxwide, y);
}

extern fixed_t baseyaspectmul;

void R_DrawVoxel(fixed_t dasprx, fixed_t daspry, fixed_t dasprz, angle_t dasprang,
	fixed_t daxscale, fixed_t dayscale, FVoxel *voxobj,
	lighttable_t *colormap, short *daumost, short *dadmost, int minslabz, int maxslabz, int flags)
{
	int i, j, k, x, y, syoff, ggxstart, ggystart, nxoff;
	fixed_t cosang, sinang, sprcosang, sprsinang;
	int backx, backy, gxinc, gyinc;
	int daxscalerecip, dayscalerecip, cnt, gxstart, gystart, dazscale;
	int lx, rx, nx, ny, x1=0, y1=0, x2=0, y2=0, yplc, yinc=0;
	int yoff, xs=0, ys=0, xe, ye, xi=0, yi=0, cbackx, cbacky, dagxinc, dagyinc;
	kvxslab_t *voxptr, *voxend;
	FVoxelMipLevel *mip;

	const int nytooclose = centerxwide * 2100, nytoofar = 32768*32768 - 1048576;
	const int xdimenscale = Scale(centerxwide, yaspectmul, 160);
	const fixed_t globalposx =  viewx >> 12;
	const fixed_t globalposy = -viewy >> 12;
	const fixed_t globalposz = -viewz >> 8;

	dasprx =  dasprx >> 12;
	daspry = -daspry >> 12;
	dasprz = -dasprz >> 8;
	daxscale >>= 10;
	dayscale >>= 10;

	cosang = viewcos >> 2;
	sinang = -viewsin >> 2;
	sprcosang = finecosine[dasprang >> ANGLETOFINESHIFT] >> 2;
	sprsinang = -finesine[dasprang >> ANGLETOFINESHIFT] >> 2;

	R_SetupDrawSlab(colormap);

	// Select mip level
	i = abs(DMulScale8(dasprx - globalposx, viewcos, daspry - globalposy, -viewsin));
	i = DivScale6(i, MIN(daxscale, dayscale));
	j = FocalLengthX >> 3;
	for (k = 0; k < voxobj->NumMips; ++k)
	{
		if (i < j) { break; }
		i >>= 1;
	}
	if (k >= voxobj->NumMips) k = voxobj->NumMips - 1;

	mip = &voxobj->Mips[k];		if (mip->SlabData == NULL) return;

	minslabz >>= k;
	maxslabz >>= k;

	daxscale <<= (k+8); dayscale <<= (k+8);
	dazscale = FixedDiv(dayscale, baseyaspectmul);
	daxscale = FixedDiv(daxscale, yaspectmul);
	daxscale = Scale(daxscale, xdimenscale, centerxwide << 9);
	dayscale = Scale(dayscale, FixedMul(xdimenscale, viewingrangerecip), centerxwide << 9);

	daxscalerecip = (1<<30) / daxscale;
	dayscalerecip = (1<<30) / dayscale;

	x = FixedMul(globalposx - dasprx, daxscalerecip);
	y = FixedMul(globalposy - daspry, daxscalerecip);
	backx = (DMulScale10(x, sprcosang, y,  sprsinang) + mip->PivotX) >> 8;
	backy = (DMulScale10(y, sprcosang, x, -sprsinang) + mip->PivotY) >> 8;
	cbackx = clamp(backx, 0, mip->SizeX - 1);
	cbacky = clamp(backy, 0, mip->SizeY - 1);

	sprcosang = MulScale14(daxscale, sprcosang);
	sprsinang = MulScale14(daxscale, sprsinang);

	x = (dasprx - globalposx) - DMulScale18(mip->PivotX, sprcosang, mip->PivotY, -sprsinang);
	y = (daspry - globalposy) - DMulScale18(mip->PivotY, sprcosang, mip->PivotX,  sprsinang);

	cosang = FixedMul(cosang, dayscalerecip);
	sinang = FixedMul(sinang, dayscalerecip);

	gxstart = y*cosang - x*sinang;
	gystart = x*cosang + y*sinang;
	gxinc = DMulScale10(sprsinang, cosang, sprcosang, -sinang);
	gyinc = DMulScale10(sprcosang, cosang, sprsinang,  sinang);
	if ((abs(globalposz - dasprz) >> 10) >= abs(dazscale)) return;

	x = 0; y = 0; j = MAX(mip->SizeX, mip->SizeY);
	fixed_t *ggxinc = (fixed_t *)alloca((j + 1) * sizeof(fixed_t) * 2);
	fixed_t *ggyinc = ggxinc + (j + 1);
	for (i = 0; i <= j; i++)
	{
		ggxinc[i] = x; x += gxinc;
		ggyinc[i] = y; y += gyinc;
	}

	syoff = DivScale21(globalposz - dasprz, dazscale) + (mip->PivotZ << 7);
	yoff = (abs(gxinc) + abs(gyinc)) >> 1;

	for (cnt = 0; cnt < 8; cnt++)
	{
		switch (cnt)
		{
			case 0: xs = 0;				ys = 0;				xi =  1; yi =  1; break;
			case 1: xs = mip->SizeX-1;	ys = 0;				xi = -1; yi =  1; break;
			case 2: xs = 0;				ys = mip->SizeY-1;	xi =  1; yi = -1; break;
			case 3: xs = mip->SizeX-1;	ys = mip->SizeY-1;	xi = -1; yi = -1; break;
			case 4: xs = 0;				ys = cbacky;		xi =  1; yi =  2; break;
			case 5: xs = mip->SizeX-1;	ys = cbacky;		xi = -1; yi =  2; break;
			case 6: xs = cbackx;		ys = 0;				xi =  2; yi =  1; break;
			case 7: xs = cbackx;		ys = mip->SizeY-1;	xi =  2; yi = -1; break;
		}
		xe = cbackx; ye = cbacky;
		if (cnt < 4)
		{
			if ((xi < 0) && (xe >= xs)) continue;
			if ((xi > 0) && (xe <= xs)) continue;
			if ((yi < 0) && (ye >= ys)) continue;
			if ((yi > 0) && (ye <= ys)) continue;
		}
		else
		{
			if ((xi < 0) && (xe > xs)) continue;
			if ((xi > 0) && (xe < xs)) continue;
			if ((yi < 0) && (ye > ys)) continue;
			if ((yi > 0) && (ye < ys)) continue;
			xe += xi; ye += yi;
		}

		i = ksgn(ys-backy)+ksgn(xs-backx)*3+4;
		switch(i)
		{
			case 6: case 7: x1 = 0;				y1 = 0;				break;
			case 8: case 5: x1 = gxinc;			y1 = gyinc;			break;
			case 0: case 3: x1 = gyinc;			y1 = -gxinc;		break;
			case 2: case 1: x1 = gxinc+gyinc;	y1 = gyinc-gxinc;	break;
		}
		switch(i)
		{
			case 2: case 5: x2 = 0;				y2 = 0;				break;
			case 0: case 1: x2 = gxinc;			y2 = gyinc;			break;
			case 8: case 7: x2 = gyinc;			y2 = -gxinc;		break;
			case 6: case 3: x2 = gxinc+gyinc;	y2 = gyinc-gxinc;	break;
		}
		BYTE oand = (1 << int(xs<backx)) + (1 << (int(ys<backy)+2));
		BYTE oand16 = oand + 16;
		BYTE oand32 = oand + 32;

		if (yi > 0) { dagxinc =  gxinc; dagyinc =  FixedMul(gyinc, viewingrangerecip); }
			   else { dagxinc = -gxinc; dagyinc = -FixedMul(gyinc, viewingrangerecip); }

			/* Fix for non 90 degree viewing ranges */
		nxoff = FixedMul(x2 - x1, viewingrangerecip);
		x1 = FixedMul(x1, viewingrangerecip);

		ggxstart = gxstart + ggyinc[ys];
		ggystart = gystart - ggxinc[ys];

		for (x = xs; x != xe; x += xi)
		{
			BYTE *slabxoffs = &mip->SlabData[mip->OffsetX[x]];
			short *xyoffs = &mip->OffsetXY[x * (mip->SizeY + 1)];

			nx = FixedMul(ggxstart + ggxinc[x], viewingrangerecip) + x1;
			ny = ggystart + ggyinc[x];
			for (y = ys; y != ye; y += yi, nx += dagyinc, ny -= dagxinc)
			{
				if ((ny <= nytooclose) || (ny >= nytoofar)) continue;
				voxptr = (kvxslab_t *)(slabxoffs + xyoffs[y]);
				voxend = (kvxslab_t *)(slabxoffs + xyoffs[y+1]);
				if (voxptr >= voxend) continue;

				lx = MulScale32(nx >> 3, distrecip(ny+y1)) + centerx;
				if (lx < 0) lx = 0;
				rx = MulScale32((nx + nxoff) >> 3, distrecip(ny+y2)) + centerx;
				if (rx > viewwidth) rx = viewwidth;
				if (rx <= lx) continue;
				rx -= lx;

				fixed_t l1 = distrecip(ny-yoff);
				fixed_t l2 = distrecip(ny+yoff);
				for (; voxptr < voxend; voxptr = (kvxslab_t *)((BYTE *)voxptr + voxptr->zleng + 3))
				{
					const BYTE *col = voxptr->col;
					int zleng = voxptr->zleng;
					int ztop = voxptr->ztop;
					fixed_t z1, z2;

					if (ztop < minslabz)
					{
						int diff = minslabz - ztop;
						ztop = minslabz;
						col += diff;
						zleng -= diff;
					}
					if (ztop + zleng > maxslabz)
					{
						int diff = ztop + zleng - maxslabz;
						zleng -= diff;
					}
					if (zleng <= 0) continue;

					j = (ztop << 15) - syoff;
					if (j < 0)
					{
						k = j + (zleng << 15);
						if (k < 0)
						{
							if ((voxptr->backfacecull & oand32) == 0) continue;
							z2 = MulScale32(l2, k) + centery;					/* Below slab */
						}
						else
						{
							if ((voxptr->backfacecull & oand) == 0) continue;	/* Middle of slab */
							z2 = MulScale32(l1, k) + centery;
						}
						z1 = MulScale32(l1, j) + centery;
					}
					else
					{
						if ((voxptr->backfacecull & oand16) == 0) continue;
						z1 = MulScale32(l2, j) + centery;						/* Above slab */
						z2 = MulScale32(l1, j + (zleng << 15)) + centery;
					}

					if (zleng == 1)
					{
						yplc = 0; yinc = 0;
						if (z1 < daumost[lx]) z1 = daumost[lx];
					}
					else
					{
						if (z2-z1 >= 1024) yinc = FixedDiv(zleng, z2 - z1);
						else if (z2 > z1) yinc = (((1 << 24) - 1) / (z2 - z1)) * zleng >> 8;
						if (z1 < daumost[lx]) { yplc = yinc*(daumost[lx]-z1); z1 = daumost[lx]; } else yplc = 0;
					}
					if (z2 > dadmost[lx]) z2 = dadmost[lx];
					z2 -= z1; if (z2 <= 0) continue;

					if (!(flags & DVF_OFFSCREEN))
					{
						// Draw directly to the screen.
						R_DrawSlab(rx, yplc, z2, yinc, col, ylookup[z1] + lx + dc_destorg);
					}
					else
					{
						// Record the area covered and possibly draw to an offscreen buffer.
						dc_yl = z1;
						dc_yh = z1 + z2 - 1;
						dc_count = z2;
						dc_iscale = yinc;
						for (int x = 0; x < rx; ++x)
						{
							OffscreenCoverageBuffer->InsertSpan(lx + x, z1, z1 + z2);
							if (!(flags & DVF_SPANSONLY))
							{
								dc_x = lx + x;
								rt_initcols(OffscreenColorBuffer + (dc_x & ~3) * OffscreenBufferHeight);
								dc_source = col;
								dc_texturefrac = yplc;
								hcolfunc_pre();
							}
						}
					}
				}
			}
		}
	}
}

//==========================================================================
//
// FCoverageBuffer Constructor
//
//==========================================================================

FCoverageBuffer::FCoverageBuffer(int lists)
	: Spans(NULL), FreeSpans(NULL)
{
	NumLists = lists;
	Spans = new Span *[lists];
	memset(Spans, 0, sizeof(Span*)*lists);
}

//==========================================================================
//
// FCoverageBuffer Destructor
//
//==========================================================================

FCoverageBuffer::~FCoverageBuffer()
{
	if (Spans != NULL)
	{
		delete[] Spans;
	}
}

//==========================================================================
//
// FCoverageBuffer :: Clear
//
//==========================================================================

void FCoverageBuffer::Clear()
{
	SpanArena.FreeAll();
	memset(Spans, 0, sizeof(Span*)*NumLists);
	FreeSpans = NULL;
}

//==========================================================================
//
// FCoverageBuffer :: InsertSpan
//
// start is inclusive.
// stop is exclusive.
//
//==========================================================================

void FCoverageBuffer::InsertSpan(int listnum, int start, int stop)
{
	assert(unsigned(listnum) < NumLists);
	assert(start < stop);

	Span **span_p = &Spans[listnum];
	Span *span;

	if (*span_p == NULL || (*span_p)->Start > stop)
	{ // This list is empty or the first entry is after this one, so we can just insert the span.
		goto addspan;
	}

	// Insert the new span in order, merging with existing ones.
	while (*span_p != NULL)
	{
		if ((*span_p)->Stop < start)							// =====		(existing span)
		{ // Span ends before this one starts.					//		  ++++	(new span)
			span_p = &(*span_p)->NextSpan;
			continue;
		}

		// Does the new span overlap or abut the existing one?
		if ((*span_p)->Start <= start)
		{
			if ((*span_p)->Stop >= stop)						// =============
			{ // The existing span completely covers this one.	//     +++++
				return;
			}
			// Extend the existing span with the new one.		// ======
			span = *span_p;										//     +++++++
			span->Stop = stop;									// (or)  +++++

			// Free up any spans we just covered up.
			span_p = &(*span_p)->NextSpan;
			while (*span_p != NULL && (*span_p)->Start <= stop && (*span_p)->Stop <= stop)
			{
				Span *span = *span_p;							// ======  ======
				*span_p = span->NextSpan;						//     +++++++++++++
				span->NextSpan = FreeSpans;
				FreeSpans = span;
			}
			if (*span_p != NULL && (*span_p)->Start <= stop)	// =======         ========
			{ // Our new span connects two existing spans.		//     ++++++++++++++
			  // They should all be collapsed into a single span.
				span->Stop = (*span_p)->Stop;
				span = *span_p;
				*span_p = span->NextSpan;
				span->NextSpan = FreeSpans;
				FreeSpans = span;
			}
			goto check;
		}
		else if ((*span_p)->Start <= stop)						//        =====
		{ // The new span extends the existing span from		//    ++++
		  // the beginning.										// (or) ++++
			(*span_p)->Start = start;
			goto check;
		}
		else													//         ======
		{ // No overlap, so insert a new span.					// +++++
			goto addspan;
		}
	}
	// Append a new span to the end of the list.
addspan:
	span = AllocSpan();
	span->NextSpan = *span_p;
	span->Start = start;
	span->Stop = stop;
	*span_p = span;
check:
#ifdef _DEBUG
	// Validate the span list: Spans must be in order, and there must be
	// at least one pixel between spans.
	for (span = Spans[listnum]; span != NULL; span = span->NextSpan)
	{
		assert(span->Start < span->Stop);
		if (span->NextSpan != NULL)
		{
			assert(span->Stop < span->NextSpan->Start);
		}
	}
#endif
	;
}

//==========================================================================
//
// FCoverageBuffer :: AllocSpan
//
//==========================================================================

FCoverageBuffer::Span *FCoverageBuffer::AllocSpan()
{
	Span *span;

	if (FreeSpans != NULL)
	{
		span = FreeSpans;
		FreeSpans = span->NextSpan;
	}
	else
	{
		span = (Span *)SpanArena.Alloc(sizeof(Span));
	}
	return span;
}

//==========================================================================
//
// R_CheckOffscreenBuffer
//
// Allocates the offscreen coverage buffer and optionally the offscreen
// color buffer. If they already exist but are the wrong size, they will
// be reallocated.
//
//==========================================================================

void R_CheckOffscreenBuffer(int width, int height, bool spansonly)
{
	if (OffscreenCoverageBuffer == NULL)
	{
		assert(OffscreenColorBuffer == NULL && "The color buffer cannot exist without the coverage buffer");
		OffscreenCoverageBuffer = new FCoverageBuffer(width);
	}
	else if (OffscreenCoverageBuffer->NumLists != (unsigned)width)
	{
		delete OffscreenCoverageBuffer;
		OffscreenCoverageBuffer = new FCoverageBuffer(width);
		if (OffscreenColorBuffer != NULL)
		{
			delete[] OffscreenColorBuffer;
			OffscreenColorBuffer = NULL;
		}
	}
	else
	{
		OffscreenCoverageBuffer->Clear();
	}

	if (!spansonly)
	{
		if (OffscreenColorBuffer == NULL)
		{
			OffscreenColorBuffer = new BYTE[width * height];
		}
		else if (OffscreenBufferWidth != width || OffscreenBufferHeight != height)
		{
			delete[] OffscreenColorBuffer;
			OffscreenColorBuffer = new BYTE[width * height];
		}
	}
	OffscreenBufferWidth = width;
	OffscreenBufferHeight = height;
}
