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

#include <stddef.h>
#include <malloc.h>
#include <stdio.h>

#include "i_system.h"
#include "m_alloc.h"

#include "m_swap.h"
#include "m_png.h"

#include "w_wad.h"

#include "doomdef.h"
#include "r_local.h"
#include "p_local.h"

#include "doomstat.h"
#include "r_sky.h"

#include "c_dispatch.h"
#include "c_console.h"
#include "r_data.h"
#include "sc_man.h"

#include "v_palette.h"
#include "v_video.h"
#include "v_text.h"
#include "gi.h"
#include "cmdlib.h"
#include "templates.h"

static void R_InitPatches ();
void R_InitBuildTiles();
void R_DeinitBuildTiles();

//
// Graphics.
// DOOM graphics for walls and sprites
// is stored in vertical runs of opaque pixels (posts).
// A column is composed of zero or more posts,
// a patch or sprite is composed of zero or more columns.
// 

// for global animation
bool*			flatwarp;
BYTE**			warpedflats;
int*			flatwarpedwhen;


FTextureManager TexMan;

FTextureManager::FTextureManager ()
{
	memset (HashFirst, -1, sizeof(HashFirst));
	// Texture 0 is a dummy texture used to indicate "no texture"
	AddTexture (new FDummyTexture);

}

FTextureManager::~FTextureManager ()
{
	for (unsigned int i = 0; i < Textures.Size(); ++i)
	{
		delete Textures[i].Texture;
	}
}

int FTextureManager::CheckForTexture (const char *name, int usetype, BITFIELD flags)
{
	int i;

	if (name == NULL || name[0] == '\0')
	{
		return -1;
	}
	// [RH] Doom counted anything beginning with '-' as "no texture".
	// Hopefully nobody made use of that and had textures like "-EMPTY",
	// because -NOFLAT- is a valid graphic for ZDoom.
	if (name[0] == '-' && name[1] == '\0')
	{
		return 0;
	}
	i = HashFirst[MakeKey (name) % HASH_SIZE];

	while (i != HASH_END)
	{
		const FTexture *tex = Textures[i].Texture;

		if (stricmp (tex->Name, name) == 0)
		{
			// The name matches, so check the texture type
			if (usetype == FTexture::TEX_Any)
			{
				return i;
			}
			else if ((flags & TEXMAN_Overridable) && tex->UseType == FTexture::TEX_Override)
			{
				return i;
			}
			else if (tex->UseType == usetype)
			{
				return i;
			}
		}
		i = Textures[i].HashNext;
	}

	if ((flags & TEXMAN_TryAny) && usetype != FTexture::TEX_Any)
	{
		return CheckForTexture (name, FTexture::TEX_Any, flags & ~TEXMAN_TryAny);
	}

	return -1;
}

int FTextureManager::GetTexture (const char *name, int usetype, BITFIELD flags)
{
	int i;

	if (name == NULL || name[0] == 0)
	{
		return 0;
	}
	else
	{
		i = CheckForTexture (name, usetype, flags | TEXMAN_TryAny);
	}

	if (i == -1)
	{
		// Use a default texture instead of aborting like Doom did
		Printf ("Unknown texture: \"%s\"\n", name);
		i = DefaultTexture;
	}
	return i;
}

void FTextureManager::WriteTexture (FArchive &arc, int picnum)
{
	FTexture *pic;

	if ((size_t)picnum >= Textures.Size())
	{
		pic = Textures[0].Texture;
	}
	else
	{
		pic = Textures[picnum].Texture;
	}

	arc.WriteCount (pic->UseType);
	arc.WriteName (pic->Name);
}

int FTextureManager::ReadTexture (FArchive &arc)
{
	int usetype;
	const char *name;

	usetype = arc.ReadCount ();
	name = arc.ReadName ();

	return GetTexture (name, usetype);
}

void FTextureManager::UnloadAll ()
{
	for (unsigned int i = 0; i < Textures.Size(); ++i)
	{
		Textures[i].Texture->Unload ();
	}
}

int FTextureManager::AddTexture (FTexture *texture)
{
	// Later textures take precedence over earlier ones
	size_t bucket = MakeKey (texture->Name) % HASH_SIZE;
	TextureHash hasher = { texture, HashFirst[bucket] };
	WORD trans = Textures.Push (hasher);
	Translation.Push (trans);
	HashFirst[bucket] = trans;
	return trans;
}

// Calls FTexture::CreateTexture and adds the texture to the manager.
int FTextureManager::CreateTexture (int lumpnum, int usetype)
{
	if (lumpnum != -1)
	{
		FTexture *out = FTexture::CreateTexture(lumpnum, usetype);

		if (out != NULL) return AddTexture (out);
		else
		{
			Printf (TEXTCOLOR_ORANGE "Invalid data encountered for texture %s\n", Wads.GetLumpFullName(lumpnum));
			return -1;
		}
	}
	return -1;
}

void FTextureManager::ReplaceTexture (int picnum, FTexture *newtexture, bool free)
{
	if ((size_t)picnum >= Textures.Size())
		return;

	FTexture *oldtexture = Textures[picnum].Texture;

	strcpy (newtexture->Name, oldtexture->Name);
	newtexture->UseType = oldtexture->UseType;
	Textures[picnum].Texture = newtexture;

	if (free)
	{
		delete oldtexture;
	}
}

int FTextureManager::AddPatch (const char *patchname, int namespc)
{
	if (patchname == NULL)
	{
		return -1;
	}
	int lumpnum = CheckForTexture (patchname, FTexture::TEX_MiscPatch, false);

	if (lumpnum >= 0)
	{
		return lumpnum;
	}
	lumpnum = Wads.CheckNumForName (patchname, namespc==ns_global? ns_graphics:namespc);
	if (lumpnum < 0)
	{
		return -1;
	}

	return CreateTexture (lumpnum, FTexture::TEX_MiscPatch);
}

void FTextureManager::AddGroup(const char * startlump, const char * endlump, int ns, int usetype)
{
	int firsttx = Wads.CheckNumForName (startlump);
	int lasttx = Wads.CheckNumForName (endlump);
	char name[9];

	if (firsttx == -1 || lasttx == -1)
	{
		return;
	}

	name[8] = 0;

	// Go from first to last so that ANIMDEFS work as expected. However,
	// to avoid duplicates (and to keep earlier entries from overriding
	// later ones), the texture is only inserted if it is the one returned
	// by doing a check by name in the list of wads.

	for (firsttx += 1; firsttx < lasttx; ++firsttx)
	{
		Wads.GetLumpName (name, firsttx);

		if (Wads.CheckNumForName (name, ns) == firsttx)
		{
			CreateTexture (firsttx, usetype);
		}
	}
}

//==========================================================================
//
// Adds all hires texture definitions.
//
//==========================================================================

void FTextureManager::AddHiresTextures ()
{
	int firsttx = Wads.CheckNumForName ("HI_START");
	int lasttx = Wads.CheckNumForName ("HI_END");
	char name[9];

	if (firsttx == -1 || lasttx == -1)
	{
		return;
	}

	name[8] = 0;

	for (firsttx += 1; firsttx < lasttx; ++firsttx)
	{
		Wads.GetLumpName (name, firsttx);

		if (Wads.CheckNumForName (name, ns_hires) == firsttx)
		{
			FTexture * newtex = FTexture::CreateTexture (firsttx, FTexture::TEX_Any);
			int oldtexno = CheckForTexture(name, FTexture::TEX_Wall, TEXMAN_Overridable|TEXMAN_TryAny);

			if (oldtexno<0)
			{
				// A texture with this name does not yet exist
				newtex->UseType=FTexture::TEX_Override;
				AddTexture(newtex);
			}
			else
			{
				FTexture * oldtex = Textures[oldtexno].Texture;

				// Replace the entire texture and adjust the scaling and offset factors.
				newtex->ScaleX = 8 * newtex->GetWidth() / oldtex->GetWidth();
				newtex->ScaleY = 8 * newtex->GetHeight() / oldtex->GetHeight();
				newtex->LeftOffset = Scale(oldtex->LeftOffset, newtex->ScaleX, 8);
				newtex->TopOffset = Scale(oldtex->TopOffset, newtex->ScaleY, 8);
				ReplaceTexture(oldtexno, newtex, true);
			}
		}
	}
}

//==========================================================================
//
// Loads the HIRESTEX lumps
//
//==========================================================================

void FTextureManager::LoadHiresTex()
{
	int remapLump, lastLump;
	char src[9];
	bool is32bit;
	int width, height;
	int type, mode;

	lastLump = 0;
	src[8] = '\0';

	while ((remapLump = Wads.FindLump("HIRESTEX", &lastLump)) != -1)
	{
		SC_OpenLumpNum(remapLump, "HIRESTEX");
		while (SC_GetString())
		{
			if (SC_Compare("remap")) // remap an existing texture
			{
				SC_MustGetString();
				
				// allow selection by type
				if (SC_Compare("wall")) type=FTexture::TEX_Wall, mode=FTextureManager::TEXMAN_Overridable;
				else if (SC_Compare("flat")) type=FTexture::TEX_Flat, mode=FTextureManager::TEXMAN_Overridable;
				else if (SC_Compare("sprite")) type=FTexture::TEX_Sprite, mode=0;
				else type = FTexture::TEX_Any, mode = 0;
				
				sc_String[8]=0;

				int tex = TexMan.CheckForTexture(sc_String, type, mode);

				SC_MustGetString();
				int lumpnum = Wads.CheckNumForFullName(sc_String);
				if (lumpnum < 0) lumpnum = Wads.CheckNumForName(sc_String, ns_graphics);

				if (tex>0) 
				{
					FTexture * oldtex = TexMan[tex];
					FTexture * newtex = FTexture::CreateTexture (lumpnum, FTexture::TEX_Any);

					// Replace the entire texture and adjust the scaling and offset factors.
					newtex->ScaleX = 8 * newtex->GetWidth() / oldtex->GetWidth();
					newtex->ScaleY = 8 * newtex->GetHeight() / oldtex->GetHeight();
					newtex->LeftOffset = MulScale3(oldtex->LeftOffset, newtex->ScaleX);
					newtex->TopOffset = MulScale3(oldtex->TopOffset, newtex->ScaleY);
					ReplaceTexture(tex, newtex, true);
				}
			}
			else if (SC_Compare("define")) // define a new "fake" texture
			{
				SC_GetString();
				memcpy(src, sc_String, 8);

				int lumpnum = Wads.CheckNumForFullName(sc_String);
				if (lumpnum < 0) lumpnum = Wads.CheckNumForName(sc_String, ns_graphics);

				SC_GetString();
				is32bit = !!SC_Compare("force32bit");
				if (!is32bit) SC_UnGet();

				SC_GetNumber();
				width = sc_Number;
				SC_GetNumber();
				height = sc_Number;

				if (lumpnum>=0)
				{
					FTexture *newtex = FTexture::CreateTexture(lumpnum, FTexture::TEX_Override);

					// Replace the entire texture and adjust the scaling and offset factors.
					newtex->ScaleX = 8 * width / newtex->GetWidth();
					newtex->ScaleY = 8 * height / newtex->GetHeight();
					newtex->LeftOffset = MulScale3(newtex->LeftOffset, newtex->ScaleX);
					newtex->TopOffset = MulScale3(newtex->TopOffset, newtex->ScaleY);
					memcpy(newtex->Name, src, sizeof(newtex->Name));

					int oldtex = TexMan.CheckForTexture(src, FTexture::TEX_Override);
					if (oldtex>=0) TexMan.ReplaceTexture(oldtex, newtex, true);
					else TexMan.AddTexture(newtex);
				}				
				//else Printf("Unable to define hires texture '%s'\n", tex->Name);
			}
		}
		SC_Close();
	}
}

void FTextureManager::AddPatches (int lumpnum)
{
	FWadLump *file = Wads.ReopenLumpNum (lumpnum);
	DWORD numpatches, i;
	char name[9];

	*file >> numpatches;
	name[8] = 0;

	for (i = 0; i < numpatches; ++i)
	{
		file->Read (name, 8);

		if (CheckForTexture (name, FTexture::TEX_WallPatch, false) == -1)
		{
			CreateTexture (Wads.CheckNumForName (name, ns_patches), FTexture::TEX_WallPatch);
		}
	}

	delete file;
}




//
// R_InitTextures
// Initializes the texture list with the textures from the world map.
//
void R_InitTextures (void)
{
	int lastlump = 0, lump;
	int texlump1 = -1, texlump2 = -1, texlump1a, texlump2a;
	int i;
	int pfile = -1;

	// For each PNAMES lump, load the TEXTURE1 and/or TEXTURE2 lumps from the same wad.
	while ((lump = Wads.FindLump ("PNAMES", &lastlump)) != -1)
	{
		pfile = Wads.GetLumpFile (lump);

		TexMan.AddPatches (lump);
		texlump1 = Wads.CheckNumForName ("TEXTURE1", ns_global, pfile);
		texlump2 = Wads.CheckNumForName ("TEXTURE2", ns_global, pfile);
		TexMan.AddTexturesLumps (texlump1, texlump2, lump);
	}

	// If the final TEXTURE1 and/or TEXTURE2 lumps are in a wad without a PNAMES lump,
	// they have not been loaded yet, so load them now.
	texlump1a = Wads.CheckNumForName ("TEXTURE1");
	texlump2a = Wads.CheckNumForName ("TEXTURE2");
	if (texlump1a != -1 && (texlump1a == texlump1 || Wads.GetLumpFile (texlump1a) <= pfile))
	{
		texlump1a = -1;
	}
	if (texlump2a != -1 && (texlump2a == texlump2 || Wads.GetLumpFile (texlump2a) <= pfile))
	{
		texlump2a = -1;
	}
	TexMan.AddTexturesLumps (texlump1a, texlump2a, Wads.GetNumForName ("PNAMES"));

	// The Hexen scripts use BLANK as a blank texture, even though it's really not.
	// I guess the Doom renderer must have clipped away the line at the bottom of
	// the texture so it wasn't visible. I'll just map it to 0, so it really is blank.
	if (gameinfo.gametype == GAME_Hexen &&
		0 <= (i = TexMan.CheckForTexture ("BLANK", FTexture::TEX_Wall, false)))
	{
		TexMan.SetTranslation (i, 0);
	}

	// Hexen parallax skies use color 0 to indicate transparency on the front
	// layer, so we must not remap color 0 on these textures. Unfortunately,
	// the only way to identify these textures is to check the MAPINFO.
	for (unsigned int i = 0; i < wadlevelinfos.Size(); ++i)
	{
		if (wadlevelinfos[i].flags & LEVEL_DOUBLESKY)
		{
			int picnum = TexMan.CheckForTexture (wadlevelinfos[i].skypic1, FTexture::TEX_Wall, false);
			if (picnum > 0)
			{
				TexMan[picnum]->SetFrontSkyLayer ();
			}
		}
	}
}

static struct FakeCmap {
	char name[8];
	PalEntry blend;
} *fakecmaps;
size_t numfakecmaps;
int firstfakecmap;
BYTE *realcolormaps;
int lastusedcolormap;

void R_SetDefaultColormap (const char *name)
{
	if (strnicmp (fakecmaps[0].name, name, 8) != 0)
	{
		int lump, i, j;
		BYTE map[256];
		BYTE unremap[256];
		BYTE remap[256];

		// [RH] If using BUILD's palette, generate the colormap
		if (Wads.CheckNumForFullName("palette.dat") >= 0 || Wads.CheckNumForFullName("blood.pal") >= 0)
		{
			Printf ("Make colormap\n");
			FDynamicColormap foo;

			foo.Color = 0xFFFFFF;
			foo.Fade = 0;
			foo.Maps = realcolormaps;
			foo.Desaturate = 0;
			foo.Next = NULL;
			foo.BuildLights ();
		}
		else
		{
			lump = Wads.CheckNumForName (name, ns_colormaps);
			if (lump == -1)
				lump = Wads.CheckNumForName (name, ns_global);
			FWadLump lumpr = Wads.OpenLumpNum (lump);

			// [RH] The colormap may not have been designed for the specific
			// palette we are using, so remap it to match the current palette.
			memcpy (remap, GPalette.Remap, 256);
			memset (unremap, 0, 256);
			for (i = 0; i < 256; ++i)
			{
				unremap[remap[i]] = i;
			}
			// Mapping to color 0 is okay, because the colormap won't be used to
			// produce a masked texture.
			remap[0] = 0;
			for (i = 0; i < NUMCOLORMAPS; ++i)
			{
				BYTE *map2 = &realcolormaps[i*256];
				lumpr.Read (map, 256);
				for (j = 0; j < 256; ++j)
				{
					map2[j] = remap[map[unremap[j]]];
				}
			}
		}

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
	int lastfakecmap = Wads.CheckNumForName ("C_END");
	firstfakecmap = Wads.CheckNumForName ("C_START");

	if (firstfakecmap == -1 || lastfakecmap == -1)
		numfakecmaps = 1;
	else
		numfakecmaps = lastfakecmap - firstfakecmap;
	realcolormaps = new BYTE[256*NUMCOLORMAPS*numfakecmaps];
	fakecmaps = new FakeCmap[numfakecmaps];

	fakecmaps[0].name[0] = 0;
	R_SetDefaultColormap ("COLORMAP");

	if (numfakecmaps > 1)
	{
		BYTE unremap[256], remap[256], mapin[256];
		int i;
		size_t j;

		memcpy (remap, GPalette.Remap, 256);
		memset (unremap, 0, 256);
		for (i = 0; i < 256; ++i)
		{
			unremap[remap[i]] = i;
		}
		remap[0] = 0;
		for (i = ++firstfakecmap, j = 1; j < numfakecmaps; i++, j++)
		{
			if (Wads.LumpLength (i) >= (NUMCOLORMAPS+1)*256)
			{
				int k, r, g, b;
				FWadLump lump = Wads.OpenLumpNum (i);
				BYTE *const map = realcolormaps + NUMCOLORMAPS*256*j;

				for (k = 0; k < NUMCOLORMAPS; ++k)
				{
					BYTE *map2 = &map[k*256];
					lump.Read (mapin, 256);
					map2[0] = 0;
					for (r = 1; r < 256; ++r)
					{
						map2[r] = remap[mapin[unremap[r]]];
					}
				}

				r = g = b = 0;

				for (k = 0; k < 256; k++)
				{
					r += GPalette.BaseColors[map[k]].r;
					g += GPalette.BaseColors[map[k]].g;
					b += GPalette.BaseColors[map[k]].b;
				}
				Wads.GetLumpName (fakecmaps[j].name, i);
				fakecmaps[j].blend = PalEntry (255, r/256, g/256, b/256);
			}
		}
	}
}

void R_DeinitColormaps ()
{
	if (fakecmaps != NULL)
	{
		delete[] fakecmaps;
		fakecmaps = NULL;
	}
	if (realcolormaps != NULL)
	{
		delete[] realcolormaps;
		realcolormaps = NULL;
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
		if (-1 != (lump = Wads.CheckNumForName (name, ns_colormaps)) )
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
// R_InitData
// Locates all the lumps that will be used by all views
// Must be called after W_Init.
//
void R_InitData ()
{
	FTexture::InitGrayMap();
	TexMan.AddGroup("S_START", "S_END", ns_sprites, FTexture::TEX_Sprite);
	R_InitPatches ();
	R_InitTextures ();
	TexMan.AddGroup("F_START", "F_END", ns_flats, FTexture::TEX_Flat);
	R_InitBuildTiles ();
	TexMan.AddGroup("TX_START", "TX_END", ns_newtextures, FTexture::TEX_Override);
	TexMan.AddHiresTextures ();
	TexMan.LoadHiresTex ();
	TexMan.DefaultTexture = TexMan.CheckForTexture ("-NOFLAT-", FTexture::TEX_Override, 0);

	R_InitColormaps ();
	C_InitConsole (SCREENWIDTH, SCREENHEIGHT, true);
}

void R_DeinitData ()
{
	R_DeinitColormaps ();
	R_DeinitBuildTiles();
	FCanvasTextureInfo::EmptyList();

	// Free openings
	if (openings != NULL)
	{
		free (openings);
		openings = NULL;
	}

	// Free drawsegs
	if (drawsegs != NULL)
	{
		free (drawsegs);
		drawsegs = NULL;
	}

}

//
// R_PrecacheLevel
// Preloads all relevant graphics for the level.
//

void R_PrecacheLevel (void)
{
	BYTE *hitlist;
	BYTE *spritelist;
	int i;

	if (demoplayback)
		return;

	hitlist = new BYTE[TexMan.NumTextures()];
	spritelist = new BYTE[sprites.Size()];
	
	// Precache textures (and sprites).
	memset (hitlist, 0, TexMan.NumTextures());
	memset (spritelist, 0, sprites.Size());

	{
		AActor *actor;
		TThinkerIterator<AActor> iterator;

		while ( (actor = iterator.Next ()) )
			spritelist[actor->sprite] = 1;
	}

	for (i = (int)(sprites.Size () - 1); i >= 0; i--)
	{
		if (spritelist[i])
		{
			int j, k;
			for (j = 0; j < sprites[i].numframes; j++)
			{
				const spriteframe_t *frame = &SpriteFrames[sprites[i].spriteframes + j];

				for (k = 0; k < 16; k++)
				{
					int pic = frame->Texture[k];
					if (pic != 0xFFFF)
					{
						hitlist[pic] = 1;
					}
				}
			}
		}
	}

	delete[] spritelist;

	for (i = numsectors - 1; i >= 0; i--)
	{
		hitlist[sectors[i].floorpic] = hitlist[sectors[i].ceilingpic] = 1;
	}

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
	//	a wall texture, with an episode dependant
	//	name.

	if (sky1texture >= 0)
	{
		hitlist[sky1texture] = 1;
	}
	if (sky2texture >= 0)
	{
		hitlist[sky2texture] = 1;
	}

	for (i = TexMan.NumTextures() - 1; i >= 0; i--)
	{
		FTexture *tex = TexMan[i];
		if (tex != NULL)
		{
			if (hitlist[i])
			{
				tex->GetPixels ();
			}
			else
			{
				tex->Unload ();
			}
		}
	}

	delete[] hitlist;
}

const BYTE *R_GetColumn (FTexture *tex, int col)
{
	return tex->GetColumn (col, NULL);
}

// Add all the miscellaneous 2D patches that are used to the texture manager
// Unfortunately, the wad format does not provide an elegant way to express
// which lumps are patches unless they are used in a wall texture, so I have
// to list them all here.

static void R_InitPatches ()
{
	static const char patches[][9] = 
	{
		"CONBACK",
		"ADVISOR",
		"BOSSBACK",
		"PFUB1",
		"PFUB2",
		"END0",
		"END1",
		"END2",
		"END3",
		"END4",
		"END5",
		"END6",
		"FINALE1",
		"FINALE2",
		"FINALE3",
		"CHESSALL",
		"CHESSC",
		"CHESSM",
		"FITEFACE",
		"CLERFACE",
		"MAGEFACE",
		"M_NGAME",
		"M_OPTION",
		"M_RDTHIS",
		"M_QUITG",
		"M_JKILL",
		"M_ROUGH",
		"M_HURT",
		"M_ULTRA",
		"M_NMARE",
		"M_LOADG",
		"M_LSLEFT",
		"M_LSCNTR",
		"M_LSRGHT",
		"M_FSLOT",
		"M_SAVEG",
		"M_DOOM",
		"M_HTIC",
		"M_STRIFE",
		"M_NEWG",
		"M_NGAME",
		"M_SKILL",
		"M_EPISOD",
		"M_EPI1",
		"M_EPI2",
		"M_EPI3",
		"M_EPI4",
		"MAPE1",
		"MAPE2",
		"MAPE3",
		"WIMAP0",
		"WIURH0",
		"WIURH1",
		"WISPLAT",
		"WIMAP1",
		"WIMAP2",
		"INTERPIC",
		"WIOSTK",
		"WIOSTI",
		"WIF",
		"WIMSTT",
		"WIOSTS",
		"WIOSTF",
		"WITIME",
		"WIPAR",
		"WIMSTAR",
		"WIMINUS",
		"WIPCNT",
		"WICOLON",
		"WISUCKS",
		"WIFRGS",
		"WISCRT2",
		"WIENTER",
		"WIKILRS",
		"WIVCTMS",
		"IN_YAH",
		"IN_X",
		"FONTB13",
		"FONTB05",
		"FONTB26",
		"FONTB15",
		"FACEA0",
		"FACEB0",
		"STFDEAD0",
		"STBANY",
		"M_PAUSE",
		"PAUSED",
		"M_SKULL1",
		"M_SKULL2",
		"M_SLCTR1",
		"M_SLCTR2",
		"M_CURS1",
		"M_CURS2",
		"M_CURS3",
		"M_CURS4",
		"M_CURS5",
		"M_CURS6",
		"M_CURS7",
		"M_CURS8",
		"BRDR_TL",
		"BRDR_T",
		"BRDR_TR",
		"BRDR_L",
		"BRDR_R",
		"BRDR_BL",
		"BRDR_B",
		"BRDR_BR",
		"BORDTL",
		"BORDT",
		"BORDTR",
		"BORDL",
		"BORDR",
		"BORDBL",
		"BORDB",
		"BORDBR",
		"TITLE",
		"CREDIT",
		"ORDER",
		"HELP",
		"HELP1",
		"HELP2",
		"HELP3",
		"HELP0",
		"TITLEPIC",
		"ENDPIC",
		"STTPRCNT",
		"STARMS",
		"VICTORY2",
		"STFBANY",
		"STPBANY",
		"RGELOGO",
		"VELLOGO",
		"FINAL1",
		"FINAL2",
		"E2END"
	};
	static const char spinners[][9] =
	{
		"SPINBK%d",
		"SPFLY%d",
		"SPSHLD%d",
		"SPBOOT%d",
		"SPMINO%d"
	};
	static const char classChars[3] = { 'F', 'C', 'M' };

	int i, j;
	char name[9];

	for (i = countof(patches); i >= 0; --i)
	{
		TexMan.AddPatch (patches[i]);
	}

	// Some digits
	for (i = 9; i >= 0; --i)
	{
		sprintf (name, "WINUM%d", i);
		TexMan.AddPatch (name);
		sprintf (name, "FONTB%d", i + 16);
		TexMan.AddPatch (name);
		sprintf (name, "AMMNUM%d", i);
		TexMan.AddPatch (name);
	}

	// Spinning power up icons for Heretic and Hexen
	for (j = countof(spinners)-1; j >= 0; --j)
	{
		for (i = 0; i <= 15; ++i)
		{
			sprintf (name, spinners[j], i);
			TexMan.AddPatch (name);
		}
	}

	// Animating overlays for the Doom E1 map
	for (i = 9; i >= 0; --i)
	{
		for (j = (i == 6) ? 3 : 2; j >= 0; --j)
		{
			sprintf (name, "WIA0%.2d%.2d", i, j);
			TexMan.AddPatch (name);
		}
	}
	// Animating overlays for the Doom E2 map
	for (i = 7; i >= 0; --i)
	{
		for (j = (i == 7) ? 2 : 0; j >= 0; --j)
		{
			sprintf (name, "WIA1%.2d%.2d", i, j);
			TexMan.AddPatch (name);
		}
	}
	// Animating overlays for the Doom E3 map
	for (i = 5; i >= 0; --i)
	{
		for (j = 2; j >= 0; --j)
		{
			sprintf (name, "WIA2%.2d%.2d", i, j);
			TexMan.AddPatch (name);
		}
	}

	// Player class animations for the Hexen new game menu
	for (i = 2; i >= 0; --i)
	{
		sprintf (name, "M_%cBOX", classChars[i]);
		TexMan.AddPatch (name);
		for (j = 4; j >= 1; --j)
		{
			sprintf (name, "M_%cWALK%d", classChars[i], j);
			TexMan.AddPatch (name);
		}
	}

	// The spinning skull in Heretic's top-level menu
	for (i = 0; i <= 17; ++i)
	{
		sprintf (name, "M_SKL%.2d", i);
		TexMan.AddPatch (name);
	}

	// Strife story panels
	for (i = 0; i <= 7; ++i)
	{
		sprintf (name, "PANEL%d", i);
		TexMan.AddPatch (name);
	}
	for (i = 2; i <= 6; ++i)
	{
		for (j = 3 + (i < 5); j > 0; --j)
		{
			sprintf (name, "SS%dF%d", i, j);
			TexMan.AddPatch (name);
		}
	}
}

#ifdef _DEBUG
// Prints the spans generated for a texture. Only needed for debugging.
CCMD (printspans)
{
	if (argv.argc() != 2)
		return;

	int picnum = TexMan.CheckForTexture (argv[1], FTexture::TEX_Any);
	if (picnum < 0)
	{
		Printf ("Unknown texture %s\n", argv[1]);
		return;
	}
	FTexture *tex = TexMan[picnum];
	for (int x = 0; x < tex->GetWidth(); ++x)
	{
		const FTexture::Span *spans;
		Printf ("%4d:", x);
		tex->GetColumn (x, &spans);
		while (spans->Length != 0)
		{
			Printf (" (%4d,%4d)", spans->TopOffset, spans->TopOffset+spans->Length-1);
			spans++;
		}
		Printf ("\n");
	}
}

CCMD (picnum)
{
	int picnum = TexMan.GetTexture (argv[1], FTexture::TEX_Any);
	Printf ("%d: %s - %s\n", picnum, TexMan[picnum]->Name, TexMan(picnum)->Name);
}
#endif
