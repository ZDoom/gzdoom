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

// On the Alpha, accessing the shorts directly if they aren't aligned on a
// 4-byte boundary causes unaligned access warnings. Why it does this at
// all and only while initing the textures is beyond me.

#ifdef ALPHA
#define SAFESHORT(s)	((short)(((byte *)&(s))[0] + ((byte *)&(s))[1] * 256))
#else
#define SAFESHORT(s)	SHORT(s)
#endif

#include <stddef.h>
#include "i_system.h"
#include "m_alloc.h"

#include "m_swap.h"

#include "w_wad.h"

#include "doomdef.h"
#include "r_local.h"
#include "p_local.h"

#include "doomstat.h"
#include "r_sky.h"


#include "r_data.h"

#include "v_palette.h"
#include "v_video.h"
#include "gi.h"

//
// Graphics.
// DOOM graphics for walls and sprites
// is stored in vertical runs of opaque pixels (posts).
// A column is composed of zero or more posts,
// a patch or sprite is composed of zero or more columns.
// 

// A single patch from a texture definition,
//	basically a rectangular area within
//	the texture rectangle.
typedef struct
{
	// Block origin (always UL),
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

	// [RH] Fix bad textures (see R_GenerateLookup)
	bool		BadPatched;	
	
	// All the patches[patchcount]
	//	are drawn back to front into the cached texture.
	short		patchcount;
	texpatch_t	patches[1];
	
} texture_t;

FTileSize		*TileSizes;
const patch_t	**TileCache;

int 			firstflat;
int 			lastflat;
int				numflats;

int 			firstspritelump;
int 			lastspritelump;
int				numspritelumps;

int				numtextures;
texture_t** 	textures;


int*			texturewidthmask;
byte*			textureheightlog2;		// [RH] Tutti-Frutti fix
fixed_t*		textureheight;			// needed for texture pegging
static int*		texturecompositesize;
static short** 	texturecolumnlump;
static unsigned **texturecolumnofs;	// killough 4/9/98: make 32-bit
static byte**	texturecomposite;
byte*			texturenodecals;
byte*			texturescalex;			// [RH] Texture scales
byte*			texturescaley;
byte*			texturetype2;			// [RH] Texture uses 2 bytes for column lengths

// for global animation
bool*			flatwarp;
byte**			warpedflats;
int*			flatwarpedwhen;
int*			flattranslation;

int*			texturetranslation;


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



// Rewritten by Lee Killough for performance and to fix Medusa bug
//

void R_DrawColumnInCache (const column_t *patch, byte *cache,
						  int originy, int cacheheight, byte *marks,
						  bool bad)
{
	int top = -1; // [RH] DeePsea tall patches

	while (patch->topdelta != 0xff)
	{
		int count;
		if (bad)
		{
			top = 0;
			count = cacheheight;
		}
		else
		{
			if (patch->topdelta <= top)
			{
				top += patch->topdelta;
			}
			else
			{
				top = patch->topdelta;
			}
			count = patch->length;
		}
		int position = originy + top;
		int adv = 0;

		if (position < 0)
		{
			// [RH] Correctly draw columns that start above the texture
			adv = -position;
			count += position;
			position = 0;
		}

		if (position + count > cacheheight)
			count = cacheheight - position;

		if (count > 0)
		{
			memcpy (cache + position, (byte *)patch + adv + 3, count);

			// killough 4/9/98: remember which cells in column have been drawn,
			// so that column can later be converted into a series of posts, to
			// fix the Medusa bug.

			memset (marks + position, 0xff, count);
		}

		if (bad)
		{
			return;
		}
		patch = (column_t *)((byte *) patch + patch->length + 4);
	}
}

//
// R_GenerateComposite
// Using the texture definition,
//	the composite texture is created from the patches,
//	and each column is cached.
//
// Rewritten by Lee Killough for performance and to fix Medusa bug

void R_GenerateComposite (int texnum)
{
	texturecomposite[texnum] = new byte[texturecompositesize[texnum]];
	byte *block = texturecomposite[texnum];
	texture_t *texture = textures[texnum];
	// Composite the columns together.
	texpatch_t *patch = texture->patches;
	short *collump = texturecolumnlump[texnum];
	unsigned *colofs = texturecolumnofs[texnum]; // killough 4/9/98: make 32-bit
	int i = texture->patchcount;
	// killough 4/9/98: marks to identify transparent regions in merged textures
	byte *marks = (byte *)Calloc (texture->width, texture->height), *source;

	for (; --i >=0; patch++)
	{
		const patch_t *realpatch = (patch_t *)W_MapLumpNum (patch->patch);
		int x1 = patch->originx, x2 = x1 + SHORT(realpatch->width);
		const int *cofs = realpatch->columnofs-x1;
		if (x1<0)
			x1 = 0;
		if (x2 > texture->width)
			x2 = texture->width;
		for (; x1<x2 ; x1++)
			if (collump[x1] == -1)			// Column has multiple patches?
				// killough 1/25/98, 4/9/98: Fix medusa bug.
				R_DrawColumnInCache((column_t*)((byte*)realpatch+LONG(cofs[x1])),
									block+colofs[x1],patch->originy,texture->height,
									marks + x1 * texture->height,texture->BadPatched);
		W_UnMapLump (realpatch);
	}

	// killough 4/9/98: Next, convert multipatched columns into true columns,
	// to fix Medusa bug while still allowing for transparent regions.
	// [RH] The patches constructed use two bytes for column lengths and offsets
	// each and do not have padding for precision errors like normal Doom patches.
	// This lets them work with tall textures.

	source = new byte[texture->height]; 	// temporary column
	for (i = 0; i < texture->width; i++)
	{
		if (collump[i] == -1) 				// process only multipatched columns
		{
			column2_t *col = (column2_t *)(block + colofs[i] - 4); // cached column
			const byte *mark = marks + i * texture->height;
			int j = 0;

			// save column in temporary so we can shuffle it around
			memcpy (source, (byte *)col + 4, texture->height);

			for (;;)	// reconstruct the column by scanning transparency marks
			{
				while (j < texture->height && !mark[j]) // skip transparent cells
					j++;
				if (j >= texture->height) 				// if at end of column
				{
					col->Length = 0;					// end-of-column marker
					break;
				}
				col->TopDelta = j;						// starting offset of post
				col->Length = 0;
				do										// count opaque cells
				{
					col->Length++;
					j++;
				} while (j < texture->height && mark[j]);
				// copy opaque cells from the temporary back into the column
				memcpy ((byte *)col + 4, source + col->TopDelta, col->Length);
				col = (column2_t *)((byte *)col + col->Length + 4); // next post
			}
		}
	}
	delete[] source;			// free temporary column
	free(marks);				// free transparency marks
}

//
// R_GenerateLookup
//
// Rewritten by Lee Killough for performance and to fix Medusa bug
//

static void R_GenerateLookup(int texnum, int *const errors)
{
	texture_t *texture = textures[texnum];

	// Composited texture not created yet.

	short *collump = texturecolumnlump[texnum];
	unsigned *colofs = texturecolumnofs[texnum]; // killough 4/9/98: make 32-bit

	// killough 4/9/98: keep count of posts in addition to patches.
	// Part of fix for medusa bug for multipatched 2s normals.

	struct cs {
		unsigned short patches, posts;
	} *count = (cs *)Calloc (sizeof *count, texture->width);

	int i = texture->patchcount;
	const texpatch_t *patch = texture->patches;
	bool mustComposite = false;
	bool mustFix = false;

	// [RH] Some wads (I forget which!) have single-patch textures 256
	// pixels tall that have patch lengths recorded as 0. I can't think of
	// any good reason for them to do this, and since I didn't make note
	// of which wad made me hack in support for them, the hack is gone
	// because I've added support for DeePsea's true tall patches.
	//
	// Okay, I found a wad with crap patches: Pleiades.wad's sky patches almost
	// fit this description and are a big mess, but they're not single patch!
	if (texture->height == 256)
	{
		// Check if this patch is likely to be a problem.
		// It must be 256 pixels tall, and all its columns must have exactly
		// one post, where each post has a supposed length of 0.
		const patch_t *realpatch = (patch_t *)W_MapLumpNum (patch->patch);
		const int *cofs = realpatch->columnofs;
		int x, x2 = SHORT(realpatch->width);

		// Note that only the first patch is checked. It's assumed that if one of
		// them is a problem, they all are, because Doom would not have rendered
		// the texture as intended if it was used on a two-sided line.
		if (SHORT(realpatch->height) == 256)
		{
			for (x = 0; x < x2; ++x)
			{
				const column_t *col = (column_t*)((byte*)realpatch+LONG(cofs[x]));
				if (col->topdelta != 0 || col->length != 0)
				{
					break;	// It's not bad!
				}
				col = (column_t *)((byte *)col + 256 + 4);
				if (col->topdelta != 0xff)
				{
					break;	// More than one post in a column!
				}
			}
			if (x == x2)
			{ // If all columns were checked, it needs fixing.
				mustComposite = true;
				mustFix = true;
			}
		}
		W_UnMapLump (realpatch);
	}

	while (--i >= 0)
	{
		int pat = patch->patch;
		const patch_t *realpatch = (patch_t *)W_MapLumpNum (pat);
		int x1 = patch->originx, x2 = x1 + SHORT(realpatch->width), x = x1;
		const int *cofs = realpatch->columnofs-x1;

		// [RH] Force composite generation if a patch starts above the top of
		// the texture or does not cover the entire height of the texture.
		if ((patch->originy < 0 || patch->originy > 0 ||
			 SHORT(realpatch->height) < texture->height))
		{
			mustComposite = true;
		}

		patch++;

		if (x2 > texture->width)
			x2 = texture->width;
		if (x1 < 0)
			x = 0;
		for (; x < x2; x++)
		{
			// killough 4/9/98: keep a count of the number of posts in column,
			// to fix Medusa bug while allowing for transparent multipatches.

			if (!mustFix)
			{
				const column_t *col = (column_t*)((byte*)realpatch+LONG(cofs[x]));
				for (;col->topdelta != 0xff; count[x].posts++)
					col = (column_t *)((byte *)col + col->length + 4);
			}
			else
			{
				count[x].posts++;
			}
			count[x].patches++;
			collump[x] = pat;
			colofs[x] = LONG(cofs[x])+3;
		}
		W_UnMapLump (realpatch);
	}

	// Now count the number of columns that are covered by more than one patch.
	// Fill in the lump / offset, so columns with only a single patch are all done.

	texturecomposite[texnum] = 0;

	{
		int x = texture->width;
		int height = texture->height;
		int csize;

		while (--x >= 0)
		{
			if (!count[x].patches)				// killough 4/9/98
			{
				mustComposite = true;
				//Printf ("\nR_GenerateLookup: Column %d is without a patch in texture %.8s",
				//		x, texture->name);
				//		++*errors;
			}
		}

		// [RH] Always create a composite texture for multipatch textures
		// or tall textures in order to keep things simpler.

		if (texture->patchcount > 1 || texture->height > 254 || mustComposite)
		{
			texture->BadPatched = mustFix;
			texturetype2[texnum] = 1;
			x = texture->width;
			csize = 0;
			while (--x >= 0)
			{
				if (!count[x].patches)				// killough 4/9/98
				{
					//Printf ("\nR_GenerateLookup: Column %d is without a patch in texture %.8s",
					//		x, texture->name);
					//		++*errors;
					collump[x] = -1;
					colofs[x] = csize + 4;
					csize += 6;
				}
				else
				{
					// killough 1/25/98, 4/9/98:
					//
					// Fix Medusa bug, by adding room for column header
					// and trailer bytes for each post in merged column.
					// For now, just allocate conservatively 4 bytes
					// per post per patch per column, since we don't
					// yet know how many posts the merged column will
					// require, and it's bounded above by this limit.

					collump[x] = -1;				// mark lump as multipatched
					colofs[x] = csize + 4;			// four header bytes in a column
					csize += 4*count[x].posts+2+height;	// 2 stop bytes plus 4 bytes per post
				}
			}
		}
		else
		{
			csize = texture->width*height;
		}
		texturecompositesize[texnum] = csize;
	}
	free(count);								// killough 4/9/98
}

//
// R_GetColumn
//
const byte *R_GetColumn (int tex, int col)
{
	int lump;
	int ofs;

	col &= texturewidthmask[tex];
	lump = texturecolumnlump[tex][col];
	ofs = texturecolumnofs[tex][col];
	
	if (lump > 0)
		//return (byte *)W_MapLumpNum(lump)+ofs;
	{
		if (TileCache[lump] == NULL)
		{
			R_CacheTileNum (lump);
		}
		return (const byte *)TileCache[lump]+ofs;
	}

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
	union
	{
		maptexture_t	*d;
		strifemaptexture_t *s;
	} mtexture;
	union
	{
		mappatch_t		*d;
		strifemappatch_t *s;
	} mpatch;

	texture_t*			texture;
	texpatch_t* 		patch;

	int					i;
	int 				j;
	int					patchCount;

	const int*			maptex;
	const int*			maptex2;
	const int*			maptex1;
	
	int*				patchlookup;
	
	int 				totalwidth;
	int					nummappatches;
	int 				offset;
	int 				maxoff;
	int 				maxoff2;
	int					numtextures1;
	int					numtextures2;

	const int*			directory;

	int					errors = 0;
	bool				bStrifeTextures = false;

	
	// Load the patch names from pnames.lmp.
	{
		const char *names = (char *)W_MapLumpName ("PNAMES");
		const char *name_p = names+4;

		nummappatches = LONG ( *((int *)names) );
		patchlookup = new int[nummappatches];
	
		for (i = 0; i < nummappatches; i++)
		{
			patchlookup[i] = W_CheckNumForName (name_p + i*8);
			if (patchlookup[i] == -1)
			{
				// killough 4/17/98:
				// Some wads use sprites as wall patches, so repeat check and
				// look for sprites this time, but only if there were no wall
				// patches found. This is the same as allowing for both, except
				// that wall patches always win over sprites, even when they
				// appear first in a wad. This is a kludgy solution to the wad
				// lump namespace problem.

				patchlookup[i] = W_CheckNumForName (name_p + i*8, ns_sprites);
			}
		}
		W_UnMapLump (names);
	}
	
	// Load the map texture definitions from textures.lmp.
	// The data is contained in one or two lumps,
	//	TEXTURE1 for shareware, plus TEXTURE2 for commercial.
	maptex = maptex1 = (int *)W_MapLumpName ("TEXTURE1");
	numtextures1 = LONG(*maptex);
	maxoff = W_LumpLength (W_GetNumForName ("TEXTURE1"));
	directory = maptex+1;
		
	if (W_CheckNumForName ("TEXTURE2") != -1)
	{
		maptex2 = (int *)W_MapLumpName ("TEXTURE2");
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

	textures = new texture_t *[numtextures];
	texturecolumnlump = new short *[numtextures];
	texturecolumnofs = new unsigned int *[numtextures];
	texturecomposite = new byte *[numtextures];
	texturecompositesize = new int[numtextures];
	texturewidthmask = new int[numtextures];
	textureheightlog2 = new byte[numtextures];
	textureheight = new fixed_t[numtextures];
	texturenodecals = new byte[numtextures];
	texturescalex = new byte[numtextures];
	texturescaley = new byte[numtextures];
	texturetype2 = new byte[numtextures];

	memset (texturenodecals, 0, numtextures);
	memset (texturetype2, 0, numtextures);

	totalwidth = 0;

	i = 0;
restart:
	for (j = 0; j < i; ++j)
	{
		if (textures[j] != NULL)
		{
			free (textures[j]);
		}
		textures[j] = NULL;
	}

	for (i = 0; i < numtextures; i++, directory++)
	{
		if (i == numtextures1)
		{
			// Start looking in second texture file.
			maptex = maptex2;
			maxoff = maxoff2;
			directory = maptex+1;
		}

		offset = LONG(*directory);

		if (offset > maxoff)
		{
			if (bStrifeTextures)
			{
				I_FatalError ("R_InitTextures: bad texture directory");
			}
			else
			{
				bStrifeTextures = true;
				goto restart;
			}
		}
		
		mtexture.d = (maptexture_t *) ((byte *)maptex + offset);

		if (!bStrifeTextures)
		{
			patchCount = SAFESHORT(mtexture.d->patchcount);
			if (patchCount <= 0 ||
				mtexture.d->columndirectory[0] != 0 ||
				mtexture.d->columndirectory[1] != 0 ||
				mtexture.d->columndirectory[2] != 0 ||
				mtexture.d->columndirectory[3] != 0)
			{
				bStrifeTextures = true;
				goto restart;
			}
		}
		else
		{
			patchCount = SAFESHORT(mtexture.s->patchcount);
			if (patchCount <= 0)
			{
				I_FatalError ("R_InitTextures: bad texture directory");
			}
		}

		texture = textures[i] = (texture_t *)
			Malloc (sizeof(texture_t)
					  + sizeof(texpatch_t)*(patchCount-1));

		texture->width = SAFESHORT(mtexture.d->width);
		texture->height = SAFESHORT(mtexture.d->height);
		texture->patchcount = patchCount;
		uppercopy (texture->name, mtexture.d->name);

		if (!bStrifeTextures)
		{
			mpatch.d = &mtexture.d->patches[0];
		}
		else
		{
			mpatch.s = &mtexture.s->patches[0];
		}
		patch = &texture->patches[0];

		for (j = 0; j < texture->patchcount; j++, patch++)
		{
			patch->originx = SHORT(mpatch.d->originx);
			patch->originy = SHORT(mpatch.d->originy);
			patch->patch = patchlookup[SHORT(mpatch.d->patch)];
			if (patch->patch == -1)
			{
				char name[9];
				memcpy (name, texture->name, 8);
				name[8] = 0;
				Printf ("R_InitTextures: Missing patch in texture %s\n", name);
				--texture->patchcount;
				--patch;
				--j;
				//errors++;
			}
			if (bStrifeTextures)
				mpatch.s++;
			else
				mpatch.d++;
		}
		if (texture->patchcount == 0)
		{
			char name[9];
			memcpy (name, texture->name, 8);
			name[8] = 0;
			Printf ("R_InitTextures: Texture %s is left without any patches\n", name);
			//errors++;
		}

		// [RH] Heretic sky textures are marked as only 128 pixels tall,
		// even though they are really 200 pixels tall. Hack around this.
		if (gameinfo.gametype == GAME_Heretic &&
			texture->name[0] == 'S' &&
			texture->name[1] == 'K' &&
			texture->name[2] == 'Y' &&
			texture->name[4] == 0 &&
			texture->name[3] >= '1' &&
			texture->name[3] <= '3' &&
			texture->height == 128)
		{
			texture->height = 200;
		}
		// [RH] The Doom E1 sky has its patch's y offset at -8 instead of 0.
		else if (gameinfo.gametype == GAME_Doom &&
			!(gameinfo.flags & GI_MAPxx) &&
			texture->height == 128 &&
			texture->patchcount == 1 &&
			texture->patches->originy == -8 &&
			texture->name[0] == 'S' &&
			texture->name[1] == 'K' &&
			texture->name[2] == 'Y' &&
			texture->name[3] == '1' &&
			texture->name[4] == 0)
		{
			texture->patches->originy = 0;
		}

		texturecolumnlump[i] = new short[texture->width];
		texturecolumnofs[i] = new unsigned int[texture->width];

		for (j = 1; j*2 <= texture->width; j <<= 1)
			;
		texturewidthmask[i] = j-1;

		// [RH] Tutti-Frutti fix
		// Sorry, only power-of-2 tall textures are actually fixed.
		// For performance reasons, I have no intention of changing this.
		for (j = 0; (1<<j) < texture->height; ++j)
			;
		textureheightlog2[i] = j;
		textureheight[i] = texture->height<<FRACBITS;

		// [RH] Special for beta 29: Values of 0 will use the tx/ty cvars
		// to determine scaling instead of defaulting to 8. I will likely
		// remove this once I finish the betas, because by then, users
		// should be able to actually create scaled textures.
		texturescalex[i] = mtexture.d->ScaleX ? mtexture.d->ScaleX : 0;
		texturescaley[i] = mtexture.d->ScaleY ? mtexture.d->ScaleY : 0;

		totalwidth += texture->width;
	}
	delete[] patchlookup;

	W_UnMapLump (maptex1);
	if (maptex2)
		W_UnMapLump (maptex2);
	
	if (errors)
		I_FatalError ("%d errors in R_InitTextures.", errors);

	// [RH] Setup hash chains. Go from back to front so that if
	//		duplicates are found, the first one gets used instead
	//		of the last (thus mimicing the original behavior
	//		of R_CheckTextureNumForName().
	for (i = 0; i < numtextures; i++)
		textures[i]->index = -1;

	for (i = numtextures - 1; i >= 0; i--)
	{
		j = W_LumpNameHash (textures[i]->name) % (unsigned) numtextures;
		textures[i]->next = textures[j]->index;
		textures[j]->index = i;
	}

	// Precalculate whatever possible.	
	for (i = 0; i < numtextures; i++)
		R_GenerateLookup (i, &errors);

	if (errors)
		I_FatalError ("%d errors encountered during texture generation.", errors);
	
	// Create translation table for global animation.
	texturetranslation = new int[numtextures+1];
	
	for (i = 0; i < numtextures; i++)
		texturetranslation[i] = i;

	// The Hexen scripts use BLANK as a blank texture, even though it's really not.
	// I guess the Doom renderer must have clipped away the line at the bottom of
	// the texture so it wasn't visible. I'll just map it to 0, so it really is blakn.
	if (gameinfo.gametype == GAME_Hexen &&
		0 <= (i = R_CheckTextureNumForName ("BLANK")))
	{
		texturetranslation[i] = 0;
	}
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
	flattranslation = new int[numflats+1];
	
	for (i = 0; i < numflats; i++)
		flattranslation[i] = i;

	flatwarp = new bool[numflats+1];
	memset (flatwarp, 0, sizeof(bool) * (numflats+1));

	warpedflats = new byte *[numflats+1];
	memset (warpedflats, 0, sizeof(byte *) * (numflats+1));

	flatwarpedwhen = new int[numflats+1];
	memset (flatwarpedwhen, 0xff, sizeof(int) * (numflats+1));
}


//
// R_InitSpriteLumps
// Finds the width and hoffset of all sprites in the wad,
//	so the sprite does not need to be cached completely
//	just for having the header info ready during rendering.
//
void R_InitSpriteLumps (void)
{
	firstspritelump = W_GetNumForName ("S_START") + 1;
	lastspritelump = W_GetNumForName ("S_END") - 1;

	numspritelumps = lastspritelump - firstspritelump + 1;

	// [RH] Rather than maintaining separate spritewidth, spriteoffset,
	//		and spritetopoffset arrays, this data has now been moved into
	//		the sprite frame definition and gets initialized by
	//		R_InstallSpriteLump(), so there really isn't anything to do here.
}


static struct FakeCmap {
	char name[8];
	PalEntry blend;
} *fakecmaps;
size_t numfakecmaps;
int firstfakecmap;
byte *realcolormaps;
int lastusedcolormap;

void R_SetDefaultColormap (const char *name)
{
	if (strnicmp (fakecmaps[0].name, name, 8) != 0)
	{
		int lump;

		lump = W_CheckNumForName (name, ns_colormaps);
		if (lump == -1)
			lump = W_CheckNumForName (name, ns_global);
		const byte *data = (byte *)W_MapLumpNum (lump);
		memcpy (realcolormaps, data, NUMCOLORMAPS*256);
		W_UnMapLump (data);
		uppercopy (fakecmaps[0].name, name);
		fakecmaps[0].blend = 0;
	}
}

//
// R_InitColormaps
//
void R_InitColormaps ()
{
	// [RH] Try and convert BOOM colormaps into blending values.
	//		This is a really rough hack, but it's better than
	//		not doing anything with them at all (right?)
	int lastfakecmap = W_CheckNumForName ("C_END");
	firstfakecmap = W_CheckNumForName ("C_START");

	if (firstfakecmap == -1 || lastfakecmap == -1)
		numfakecmaps = 1;
	else
		numfakecmaps = lastfakecmap - firstfakecmap;
	realcolormaps = new BYTE[256*NUMCOLORMAPS*numfakecmaps+255];
	realcolormaps = (BYTE *)(((ptrdiff_t)realcolormaps + 255) & ~255);
	fakecmaps = new FakeCmap[numfakecmaps];

	fakecmaps[0].name[0] = 0;
	R_SetDefaultColormap ("COLORMAP");

	if (numfakecmaps > 1)
	{
		int i;
		size_t j;

		for (i = ++firstfakecmap, j = 1; j < numfakecmaps; i++, j++)
		{
			if (W_LumpLength (i) >= (NUMCOLORMAPS+1)*256)
			{
				int k, r, g, b;
				const byte *map = (byte *)W_MapLumpNum (i);

				memcpy (realcolormaps+NUMCOLORMAPS*256*j,
						map, NUMCOLORMAPS*256);

				W_UnMapLump (map);

				r = g = b = 0;

				W_GetLumpName (fakecmaps[j].name, i);
				for (k = 0; k < 256; k++)
				{
					r += GPalette.BaseColors[map[k]].r;
					g += GPalette.BaseColors[map[k]].g;
					b += GPalette.BaseColors[map[k]].b;
				}
				fakecmaps[j].blend = PalEntry (255, r/256, g/256, b/256);
			}
		}
	}
}

// [RH] Returns an index into realcolormaps. Multiply it by
//		256*NUMCOLORMAPS to find the start of the colormap to use.
//		WATERMAP is an exception and returns a blending value instead.
DWORD R_ColormapNumForName (const char *name)
{
	int lump;
	DWORD blend = 0;

	if (strnicmp (name, "COLORMAP", 8))
	{	// COLORMAP always returns 0
		if (-1 != (lump = W_CheckNumForName (name, ns_colormaps)) )
			blend = lump - firstfakecmap + 1;
		else if (!strnicmp (name, "WATERMAP", 8))
			blend = MAKEARGB (128,0,0x4f,0xa5);
	}

	return blend;
}

DWORD R_BlendForColormap (DWORD map)
{
	return APART(map) ? map : 
		   map < numfakecmaps ? DWORD(fakecmaps[map].blend) : 0;
}

//
// [RH] R_InitTiles
//
// Initialize the tile cache.
//
static int NumTileLumps;

void R_InitTiles ()
{
	int i;

	NumTileLumps = numlumps;

	TileSizes = new FTileSize[NumTileLumps];
	TileCache = new const patch_t *[NumTileLumps];

	for (i = 0; i < NumTileLumps; i++)
	{
		TileSizes[i].Width = 0xffff;
		TileCache[i] = NULL;
	}
}

//
// R_InitData
// Locates all the lumps that will be used by all views
// Must be called after W_Init.
//
void R_InitData ()
{
	R_InitTiles ();
	R_InitTextures ();
	R_InitFlats ();
	R_InitSpriteLumps ();
	R_InitColormaps ();
}



//
// R_FlatNumForName
// Retrieval, get a flat number for a flat name.
//
int R_FlatNumForName (const char* name)
{
	int i = W_CheckNumForName (name, ns_flats);

	if (i == -1)	// [RH] Default flat for not found ones
		i = W_CheckNumForName ("-NOFLAT-", ns_flats);

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

	while (i != -1)
	{
		if (!strncmp (textures[i]->name, uname, 8))
			break;
		i = textures[i]->next;
	}

	return i;
}

const char *R_GetTextureName (int texnum)
{
	if (texnum <= 0 || texnum >= numtextures)
	{
		return "-";
	}
	return textures[texnum]->name;
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

	if (i==-1)
	{
		char namet[9];
		strncpy (namet, name, 8);
		namet[8] = 0;
		//I_Error ("R_TextureNumForName: %s not found", namet);
		// [RH] Return default texture if it wasn't found.		Printf ("Texture %s not found\n", namet);
		return 1;
	}

	return i;
}

//
// R_CheckTileNumForName
//
int R_CheckTileNumForName (const char *name, ETileType type)
{
	static const namespace_t spaces[2][2] =
	{ {ns_global,ns_sprites}, {ns_sprites,ns_global} };

	const namespace_t *space = spaces[type];
	int i;
	int lump = -1;

	for (i = 0; i < NUM_TILE_TYPES; i++)
	{
		lump = W_CheckNumForName (name, space[i]);
		if (lump != -1)
			break;
	}
	return lump;
}

//
// R_CacheTileNum
//
int R_CacheTileNum (int picnum)
{
	if (TileCache[picnum] != NULL)
	{
		return picnum;
	}
	TileCache[picnum] = (const patch_t *)W_MapLumpNum (picnum);
	TileSizes[picnum].Width = SHORT(TileCache[picnum]->width);
	TileSizes[picnum].Height = SHORT(TileCache[picnum]->height);
	TileSizes[picnum].LeftOffset = SHORT(TileCache[picnum]->leftoffset);
	TileSizes[picnum].TopOffset = SHORT(TileCache[picnum]->topoffset);
	return picnum;
}

//
// R_CacheTileName
//
int R_CacheTileName (const char *name, ETileType type)
{
	int picnum = R_CheckTileNumForName (name, type);
	if (picnum == -1)
	{
		I_Error ("Can't find tile \"%s\"\n", name);
	}
	return R_CacheTileNum (picnum);
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

	// Unmap all loaded tiles (if they are memory-mapped, this does nothing)
	for (i = NumTileLumps - 1; i >= 0; --i)
	{
		if (TileCache[i] != NULL)
		{
			W_UnMapLump (TileCache[i]);
			TileCache[i] = NULL;
		}
	}
	// Decommit composite textures.
	for (i = numtextures - 1; i >= 0; --i)
	{
		if (texturecomposite[i] != NULL)
		{
			delete[] texturecomposite[i];
			texturecomposite[i] = NULL;
		}
	}

	{
		int size = (numflats > (int)sprites.Size ()) ? numflats : (int)sprites.Size ();

		hitlist = new byte[(numtextures > size) ? numtextures : size];
	}
	
	// Precache flats.
	memset (hitlist, 0, numflats);

	for (i = numsectors - 1; i >= 0; i--)
		hitlist[sectors[i].floorpic] = hitlist[sectors[i].ceilingpic] = 1;
		
	for (i = numflats - 1; i >= 0; i--)
		if (hitlist[i])
			W_MapLumpNum (firstflat + i);
	
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

	for (i = numtextures - 1; i >= 0; i--)
	{
		if (hitlist[i])
		{
			int j;
			texture_t *texture = textures[i];

			for (j = texture->patchcount - 1; j > 0; j--)
				R_CacheTileNum (texture->patches[j].patch);
		}
	}

	// Precache sprites.
	memset (hitlist, 0, sprites.Size ());
		
	{
		AActor *actor;
		TThinkerIterator<AActor> iterator;

		while ( (actor = iterator.Next ()) )
			hitlist[actor->sprite] = 1;
	}

	for (i = (int)(sprites.Size () - 1); i >= 0; i--)
	{
		if (hitlist[i])
		{
			int j, k;
			for (j = 0; j < sprites[i].numframes; j++)
			{
				for (k = 0; k < 8; k++)
				{
					if (SpriteFrames[sprites[i].spriteframes + j].lump[k] != -1)
					{
						R_CacheTileNum (SpriteFrames[sprites[i].spriteframes + j].lump[k]);
					}
				}
			}
		}
	}

	delete[] hitlist;
}
