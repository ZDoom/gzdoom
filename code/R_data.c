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
// Revision 1.3  1997/01/29 20:10
// DESCRIPTION:
//		Preparation of data for rendering,
//		generation of lookups, caching, retrieval by name.
//
//-----------------------------------------------------------------------------



#include "i_system.h"
#include "z_zone.h"

#include "m_swap.h"

#include "w_wad.h"

#include "doomdef.h"
#include "r_local.h"
#include "p_local.h"

#include "doomstat.h"
#include "r_sky.h"


#include "r_data.h"

#include "v_palett.h"
#include "v_video.h"

//
// Graphics.
// DOOM graphics for walls and sprites
// is stored in vertical runs of opaque pixels (posts).
// A column is composed of zero or more posts,
// a patch or sprite is composed of zero or more columns.
// 



//
// Texture definition.
// Each texture is composed of one or more patches,
// with patches being lumps stored in the WAD.
// The lumps are referenced by number, and patched
// into the rectangular texture space using origin
// and possibly other attributes.
//
typedef struct
{
	short		originx;
	short		originy;
	short		patch;
	short		stepdir;
	short		colormap;
} mappatch_t;


//
// Texture definition.
// A DOOM wall texture is a list of patches
// which are to be combined in a predefined order.
//
typedef struct
{
	char		name[8];
	BOOL	 	masked; 
	short		width;
	short		height;
	void		**columndirectory;		// OBSOLETE
	short		patchcount;
	mappatch_t	patches[1];
} maptexture_t;


// A single patch from a texture definition,
//	basically a rectangular area within
//	the texture rectangle.
typedef struct
{
	// Block origin (allways UL),
	// which has already accounted
	// for the internal origin of the patch.
	int 		originx;		
	int 		originy;
	int 		patch;
} texpatch_t;


// A maptexturedef_t describes a rectangular texture,
//	which is composed of one or more mappatch_t structures
//	that arrange graphic patches.
typedef struct
{
	// Keep name for switch changing, etc.
	char		name[8];
	short		width;
	short		height;

	// [RH] Use a hash table similar to the one now used
	//		in w_wad.c, thus speeding up level loads.
	//		(possibly quite considerably for larger levels)
	int			index;
	int			next;
	
	// All the patches[patchcount]
	//	are drawn back to front into the cached texture.
	short		patchcount;
	texpatch_t	patches[1];
	
} texture_t;



int 			firstflat;
int 			lastflat;
static int		numflats;

int 			firstspritelump;
int 			lastspritelump;
int				numspritelumps;

int				numtextures;
texture_t** 	textures;


static int*		texturewidthmask;
static byte*	textureheightmask;		// [RH] Tutti-Frutti fix
fixed_t*		textureheight;			// needed for texture pegging
static int*		texturecompositesize;
static short** 	texturecolumnlump;
static unsigned short**texturecolumnofs;
static byte**	texturecomposite;

// for global animation
int*			flattranslation;
int*			texturetranslation;

// needed for pre rendering
fixed_t*		spritewidth;	
fixed_t*		spriteoffset;
fixed_t*		spritetopoffset;

//lighttable_t	*colormaps;


//
// MAPTEXTURE_T CACHING
// When a texture is first needed,
//	it counts the number of composite columns
//	required in the texture and allocates space
//	for a column directory and any new columns.
// The directory will simply point inside other patches
//	if there is only one patch in a given column,
//	but any columns with multiple patches
//	will have new column_ts generated.
//



//
// R_DrawColumnInCache
// Clip and draw a column
//	from a patch into a cached post.
//
void R_DrawColumnInCache (column_t *patch, byte *cache, int originy, int cacheheight)
{
    while (patch->topdelta != 0xff)
    {
		int count = patch->length;
		int position = originy + patch->topdelta;

		if (position < 0)
		{
			count += position;
			position = 0;
		}

		if (position + count > cacheheight)
			count = cacheheight - position;

		if (count > 0)
			memcpy (cache + position, (byte *)patch + 3, count);
			
		patch = (column_t *)(  (byte *)patch + patch->length + 4); 
    }
}



//
// R_GenerateComposite
// Using the texture definition,
//	the composite texture is created from the patches,
//	and each column is cached.
//
void R_GenerateComposite (int texnum)
{
	byte*				block;
	texture_t*			texture;
	texpatch_t* 		patch;	
	int 				i;
	column_t*			patchcol;
	short*				collump;
	unsigned short* 	colofs;
		
	texture = textures[texnum];

	block = Z_Malloc (texturecompositesize[texnum], PU_STATIC, 
					  &texturecomposite[texnum]);		

	collump = texturecolumnlump[texnum];
	colofs = texturecolumnofs[texnum];
	
	// Composite the columns together.
	patch = texture->patches;
				
	for (i = texture->patchcount, patch = texture->patches; i > 0; i--, patch++)
	{
		patch_t *realpatch = W_CacheLumpNum (patch->patch, PU_CACHE);
		int x1 = patch->originx;
		int x2 = x1 + SHORT(realpatch->width);
		int x = (x1 < 0) ? 0 : x1;

		if (x2 > texture->width)
			x2 = texture->width;

		for ( ; x < x2; x++)
		{
			// Column does not have multiple patches?
			if (collump[x] >= 0)
				continue;
			
			patchcol = (column_t *)((byte *)realpatch
									+ LONG(realpatch->columnofs[x-x1]));
			R_DrawColumnInCache (patchcol,
								 block + colofs[x],
								 patch->originy,
								 texture->height);
		}

	}

	// Now that the texture has been built in column cache,
	//	it is purgable from zone memory.
	Z_ChangeTag (block, PU_CACHE);
}



//
// R_GenerateLookup
//
void R_GenerateLookup (int texnum)
{
	texture_t*			texture;
	byte*				patchcount; 	// patchcount[texture->width]
	texpatch_t* 		patch;	
	int 				i;
	short*				collump;
	unsigned short* 	colofs;
		
	texture = textures[texnum];

	// Composited texture not created yet.
	texturecomposite[texnum] = 0;
	
	texturecompositesize[texnum] = 0;
	collump = texturecolumnlump[texnum];
	colofs = texturecolumnofs[texnum];
	
	// Now count the number of columns
	//	that are covered by more than one patch.
	// Fill in the lump / offset, so columns
	//	with only a single patch are all done.
	patchcount = (byte *)Z_Malloc (texture->width, PU_STATIC, 0);
	memset (patchcount, 0, texture->width);
	patch = texture->patches;
				
	for (i = texture->patchcount, patch = texture->patches; i > 0; i--, patch++)
	{
		patch_t *realpatch = W_CacheLumpNum (patch->patch, PU_CACHE);
		int x1 = patch->originx;
		int x2 = x1 + SHORT(realpatch->width);
		int x = (x1 < 0) ? 0 : x1;

		if (x2 > texture->width)
			x2 = texture->width;

		for ( ; x < x2 ; x++)
		{
			patchcount[x]++;
			collump[x] = (short)patch->patch;
			colofs[x] = (short)(LONG(realpatch->columnofs[x-x1])+3);
		}
	}
		
	for (i = 0; i < texture->width; i++)
	{
		if (!patchcount[i])
		{
			char namet[9];
			strncpy (namet, texture->name, 8);
			namet[8] = 0;
		// I_Error ("R_GenerateLookup: column without a patch");
			Printf ("R_GenerateLookup: column without a patch (%s)\n", namet);
			return;
		}
		
		if (patchcount[i] > 1)
		{
			// Use the cached block.
			collump[i] = -1;	
			colofs[i] = (short)texturecompositesize[texnum];
			
			if (texturecompositesize[texnum] > 0x10000-texture->height)
				I_Error ("R_GenerateLookup: texture %i is >64k", texnum);
			
			texturecompositesize[texnum] += texture->height;
		}
	}
	Z_Free (patchcount);
}




//
// R_GetColumn
//
byte *R_GetColumn (int tex, int col)
{
	int lump;
	int ofs;

	col &= texturewidthmask[tex];
	lump = texturecolumnlump[tex][col];
	ofs = texturecolumnofs[tex][col];
	dc_mask = textureheightmask[tex];		// [RH] Tutti-Frutti fix
	
	if (lump > 0)
		return (byte *)W_CacheLumpNum(lump,PU_CACHE)+ofs;

	if (!texturecomposite[tex])
		R_GenerateComposite (tex);

	return texturecomposite[tex] + ofs;
}




//
// R_InitTextures
// Initializes the texture list
//	with the textures from the world map.
//
void R_InitTextures (void)
{
	maptexture_t*		mtexture;
	texture_t*			texture;
	mappatch_t* 		mpatch;
	texpatch_t* 		patch;

	int					i;
	int 				j;

	int*				maptex;
	int*				maptex2;
	int*				maptex1;
	
	int*				patchlookup;
	
	int 				totalwidth;
	int					nummappatches;
	int 				offset;
	int 				maxoff;
	int 				maxoff2;
	int					numtextures1;
	int					numtextures2;

	int*				directory;

	
	// Load the patch names from pnames.lmp.
	{
		char *names = W_CacheLumpName ("PNAMES", PU_STATIC);
		char *name_p = names+4;

		nummappatches = LONG ( *((int *)names) );
		patchlookup = Z_Malloc (nummappatches*sizeof(*patchlookup), PU_STATIC, 0);
	
		for (i = 0; i < nummappatches; i++)
			patchlookup[i] = W_CheckNumForName (name_p + i * 8);

		Z_Free (names);
	}
	
	// Load the map texture definitions from textures.lmp.
	// The data is contained in one or two lumps,
	//	TEXTURE1 for shareware, plus TEXTURE2 for commercial.
	maptex = maptex1 = W_CacheLumpName ("TEXTURE1", PU_STATIC);
	numtextures1 = LONG(*maptex);
	maxoff = W_LumpLength (W_GetNumForName ("TEXTURE1"));
	directory = maptex+1;
		
	if (W_CheckNumForName ("TEXTURE2") != -1)
	{
		maptex2 = W_CacheLumpName ("TEXTURE2", PU_STATIC);
		numtextures2 = LONG(*maptex2);
		maxoff2 = W_LumpLength (W_GetNumForName ("TEXTURE2"));
	}
	else
	{
		maptex2 = NULL;
		numtextures2 = 0;
		maxoff2 = 0;
	}

	numtextures = numtextures1 + numtextures2;

#define ALLOC(a)	a = Z_Malloc (numtextures * sizeof(*a), PU_STATIC, 0);
	ALLOC (textures);
	ALLOC (texturecolumnlump);
	ALLOC (texturecolumnofs);
	ALLOC (texturecomposite);
	ALLOC (texturecompositesize);
	ALLOC (texturewidthmask);
	ALLOC (textureheightmask);	// [RH] Tutti-Frutti fix
	ALLOC (textureheight);
#undef ALLOC

	totalwidth = 0;
	
	//	Really complex printing shit...
	{
		int temp3 = (((W_GetNumForName ("S_END") - 1) -
						W_GetNumForName ("S_START") + 127) >> 7) +
						((numtextures+127) >> 8);

		Printf("[");
		for (i = temp3; i > 0; i--)
			Printf(" ");
		Printf("        ]");
		for (i = temp3; i > 0; i--)
			Printf("\x8");
		Printf("\x8\x8\x8\x8\x8\x8\x8\x8\x8\x8");	
	}

	for (i = 0; i < numtextures; i++, directory++)
	{
		if (!(i&127))
			Printf (".");

		if (i == numtextures1)
		{
			// Start looking in second texture file.
			maptex = maptex2;
			maxoff = maxoff2;
			directory = maptex+1;
		}

		offset = LONG(*directory);

		if (offset > maxoff)
			I_Error ("R_InitTextures: bad texture directory");
		
		mtexture = (maptexture_t *) ( (byte *)maptex + offset);

		texture = textures[i] =
			Z_Malloc (sizeof(texture_t)
					  + sizeof(texpatch_t)*(SHORT(mtexture->patchcount)-1),
					  PU_STATIC, 0);
		
		texture->width = SHORT(mtexture->width);
		texture->height = SHORT(mtexture->height);
		texture->patchcount = SHORT(mtexture->patchcount);

		uppercopy (texture->name, mtexture->name);
		mpatch = &mtexture->patches[0];
		patch = &texture->patches[0];

		for (j=0 ; j<texture->patchcount ; j++, mpatch++, patch++)
		{
			patch->originx = SHORT(mpatch->originx);
			patch->originy = SHORT(mpatch->originy);
			patch->patch = patchlookup[SHORT(mpatch->patch)];
			if (patch->patch == -1)
			{
				I_Error ("R_InitTextures: Missing patch in texture %s",
						 texture->name);
			}
		}
		texturecolumnlump[i] = Z_Malloc (texture->width*sizeof(**texturecolumnlump), PU_STATIC,0);
		texturecolumnofs[i] = Z_Malloc (texture->width*sizeof(**texturecolumnofs), PU_STATIC,0);

		j = 1;
		while (j*2 <= texture->width)
			j<<=1;

		texturewidthmask[i] = j-1;
		textureheight[i] = texture->height<<FRACBITS;

		// [RH] Tutti-Frutti fix
		// Sorry, only power-of-2 tall textures are actually fixed.
		j = 1;
		while (j < texture->height)
			j <<= 1;

		textureheightmask[i] = j-1;
				
		totalwidth += texture->width;
	}
	Z_Free (patchlookup);

	Z_Free (maptex1);
	if (maptex2)
		Z_Free (maptex2);
	
	// [RH] Setup hash chains. Go from back to front so that if
	//		duplicates are found, the first one gets used instead
	//		of the last (thus mimicing the original behavior
	//		of R_CheckTextureNumForName().
	for (i = 0; i < numtextures; i++)
		textures[i]->index = -1;

	for (i = numtextures - 1; i >= 0; i--) {
		j = W_LumpNameHash (textures[i]->name) % (unsigned) numtextures;
		textures[i]->next = textures[j]->index;
		textures[j]->index = i;
	}

	// Precalculate whatever possible.	
	for (i = 0; i < numtextures; i++)
		R_GenerateLookup (i);
	
	// Create translation table for global animation.
	texturetranslation = Z_Malloc ((numtextures+1)*sizeof(*texturetranslation), PU_STATIC, 0);
	
	for (i = 0; i < numtextures; i++)
		texturetranslation[i] = i;

}



//
// R_InitFlats
//
void R_InitFlats (void)
{
	int i;
		
	firstflat = W_GetNumForName ("F_START") + 1;
	lastflat = W_GetNumForName ("F_END") - 1;
	numflats = lastflat - firstflat + 1;
		
	// Create translation table for global animation.
	flattranslation = Z_Malloc ((numflats+1) * (sizeof(int)), PU_STATIC, 0);
	
	for (i = 0; i < numflats; i++)
		flattranslation[i] = i;
}


//
// R_InitSpriteLumps
// Finds the width and hoffset of all sprites in the wad,
//	so the sprite does not need to be cached completely
//	just for having the header info ready during rendering.
//
void R_InitSpriteLumps (void)
{
	int			i;
	patch_t 	*patch;
		
	firstspritelump = W_GetNumForName ("S_START") + 1;
	lastspritelump = W_GetNumForName ("S_END") - 1;
	
	numspritelumps = lastspritelump - firstspritelump + 1;
	spritewidth = Z_Malloc (numspritelumps*4, PU_STATIC, 0);
	spriteoffset = Z_Malloc (numspritelumps*4, PU_STATIC, 0);
	spritetopoffset = Z_Malloc (numspritelumps*4, PU_STATIC, 0);
		
	for (i = 0; i < numspritelumps; i++)
	{
		if (!(i&127))
			Printf (".");

		patch = W_CacheLumpNum (firstspritelump+i, PU_CACHE);
		spritewidth[i] = SHORT(patch->width)<<FRACBITS;
		spriteoffset[i] = SHORT(patch->leftoffset)<<FRACBITS;
		spritetopoffset[i] = SHORT(patch->topoffset)<<FRACBITS;
	}
}



//
// R_InitColormaps
//
void R_InitColormaps (void)
{
	// [RH] This is all done dynamically in v_palette.c
#if 0
	int lump, length;

	// Load in the light tables,
	//	256 byte align tables.
	lump = W_GetNumForName("COLORMAP");
	length = W_LumpLength (lump) + 255;
	colormaps = Z_Malloc (length, PU_STATIC, 0);
	colormaps = (byte *)( ((int)colormaps + 255)&~0xff);
	W_ReadLump (lump,colormaps);
#endif
}



//
// R_InitData
// Locates all the lumps
//	that will be used by all views
// Must be called after W_Init.
//
void R_InitData (void)
{
	R_InitTextures ();
//	Printf ("\nInitTextures");
	R_InitFlats ();
//	Printf ("\nInitFlats");
	R_InitSpriteLumps ();
//	Printf (".");
//	Printf ("\nInitSprites");
	R_InitColormaps ();
//	Printf (".");
//	Printf ("\nInitColormaps");
}



//
// R_FlatNumForName
// Retrieval, get a flat number for a flat name.
//
int R_FlatNumForName (const char* name)
{
	int i = W_CheckNumForName (name);

	if (i == -1)
		i = W_CheckNumForName ("-NOFLAT-");

	if (i == -1) {
		char namet[9];

		strncpy (namet, name, 8);
		namet[8] = 0;

		I_Error ("R_FlatNumForName: %s not found", namet);
	}

	return i - firstflat;
}




//
// R_CheckTextureNumForName
// Check whether texture is available.
// Filter out NoTexture indicator.
//
int R_CheckTextureNumForName (const char *name)
{
	char uname[8];
	int  i;

	// "NoTexture" marker.
	if (name[0] == '-') 		
		return 0;

	// [RH] Use a hash table instead of linear search
	uppercopy (uname, name);
	i = textures[W_LumpNameHash (uname) % (unsigned) numtextures]->index;

	while (i != -1) {
		if (!strncmp (textures[i]->name, uname, 8))
			break;
		i = textures[i]->next;
	}

	return i;
}



//
// R_TextureNumForName
// Calls R_CheckTextureNumForName,
//	aborts with error message.
//
int R_TextureNumForName (const char *name)
{
	int i;
		
	i = R_CheckTextureNumForName (name);

	if (i==-1) {
		char namet[9];
		strncpy (namet, name, 8);
		namet[8] = 0;
		//I_Error ("R_TextureNumForName: %s not found", namet);
		// [RH] Return empty texture if it wasn't found.
		Printf ("Texture %s not found\n", namet);
		return 0;
	}

	return i;
}




//
// R_PrecacheLevel
// Preloads all relevant graphics for the level.
//
// [RH] Rewrote this using Lee Killough's code in BOOM as an example.

void R_PrecacheLevel (void)
{
	byte *hitlist;
	int i;

	if (demoplayback)
		return;

	{
		int size = (numflats > numsprites) ? numflats : numsprites;

		hitlist = Z_Malloc ((numtextures > size) ? numtextures : size, PU_STATIC, NULL);
	}
	
	// Precache flats.
	memset (hitlist, 0, numflats);

	for (i = numsectors - 1; i >= 0; i--)
		hitlist[sectors[i].floorpic] = hitlist[sectors[i].ceilingpic] = 1;
		
	for (i = numflats - 1; i >= 0; i--)
		if (hitlist[i])
			W_CacheLumpNum (firstflat + i, PU_CACHE);
	
	// Precache textures.
	memset (hitlist, 0, numtextures);

	for (i = numsides - 1; i >= 0; i--)
	{
		hitlist[sides[i].toptexture] =
			hitlist[sides[i].midtexture] =
			hitlist[sides[i].bottomtexture] = 1;
	}

	// Sky texture is always present.
	// Note that F_SKY1 is the name used to
	//	indicate a sky floor/ceiling as a flat,
	//	while the sky texture is stored like
	//	a wall texture, with an episode dependend
	//	name.
	//
	// [RH] Possibly two sky textures now.

	hitlist[sky1texture] =
		hitlist[sky2texture] = 1;

	for (i = numtextures - 1; i >= 0; i--) {
		if (hitlist[i]) {
			int j;
			texture_t *texture = textures[i];

			for (j = texture->patchcount - 1; j > 0; j--)
				W_CacheLumpNum(texture->patches[j].patch, PU_CACHE);
		}
	}

	// Precache sprites.
	memset (hitlist, 0, numsprites);
		
	{
		thinker_t *th;

		for (th = thinkercap.next; th != &thinkercap; th = th->next)
			if (th->function.acp1 == (actionf_p1)P_MobjThinker)
				hitlist[((mobj_t *)th)->sprite] = 1;
	}

	for (i = numsprites - 1; i >= 0; i--)
	{
		if (hitlist[i]) {
			int j;

			for (j = sprites[i].numframes - 1; j >= 0; j--)
			{
				short *sflumps = sprites[i].spriteframes[j].lump;
				int k;

				for (k = 7; k >= 0; k--)
					W_CacheLumpNum(firstspritelump + sflumps[k], PU_CACHE);
			}
		}
	}

	Z_Free (hitlist);
}
