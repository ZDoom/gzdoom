/*
** multipatchtexturebuilder.cpp
** Texture class for standard Doom multipatch textures
**
**---------------------------------------------------------------------------
** Copyright 2004-2006 Randy Heit
** Copyright 2006-2018 Christoph Oelckers
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
**
*/

#include <ctype.h>
#include "doomtype.h"
#include "files.h"
#include "w_wad.h"
#include "i_system.h"
#include "gi.h"
#include "st_start.h"
#include "sc_man.h"
#include "templates.h"
#include "r_data/r_translate.h"
#include "bitmap.h"
#include "colormatcher.h"
#include "v_palette.h"
#include "v_video.h"
#include "v_text.h"
#include "cmdlib.h"
#include "imagehelpers.h"
#include "image.h"
#include "formats/multipatchtexture.h"
#include "doomerrors.h"

// On the Alpha, accessing the shorts directly if they aren't aligned on a
// 4-byte boundary causes unaligned access warnings. Why it does this at
// all and only while initing the textures is beyond me.

#ifdef ALPHA
#define SAFESHORT(s)	((short)(((uint8_t *)&(s))[0] + ((uint8_t *)&(s))[1] * 256))
#else
#define SAFESHORT(s)	LittleShort(s)
#endif



//--------------------------------------------------------------------------
//
// Data structures for the TEXTUREx lumps
//
//--------------------------------------------------------------------------

//
// Each texture is composed of one or more patches, with patches being lumps
// stored in the WAD. The lumps are referenced by number, and patched into
// the rectangular texture space using origin and possibly other attributes.
//
struct mappatch_t
{
	int16_t	originx;
	int16_t	originy;
	int16_t	patch;
	int16_t	stepdir;
	int16_t	colormap;
};

//
// A wall texture is a list of patches which are to be combined in a
// predefined order.
//
struct maptexture_t
{
	uint8_t		name[8];
	uint16_t		Flags;				// [RH] Was unused
	uint8_t		ScaleX;				// [RH] Scaling (8 is normal)
	uint8_t		ScaleY;				// [RH] Same as above
	int16_t		width;
	int16_t		height;
	uint8_t		columndirectory[4];	// OBSOLETE
	int16_t		patchcount;
	mappatch_t	patches[1];
};

#define MAPTEXF_WORLDPANNING	0x8000

// Strife uses versions of the above structures that remove all unused fields

struct strifemappatch_t
{
	int16_t	originx;
	int16_t	originy;
	int16_t	patch;
};

//
// A wall texture is a list of patches which are to be combined in a
// predefined order.
//
struct strifemaptexture_t
{
	uint8_t		name[8];
	uint16_t		Flags;				// [RH] Was unused
	uint8_t		ScaleX;				// [RH] Scaling (8 is normal)
	uint8_t		ScaleY;				// [RH] Same as above
	int16_t		width;
	int16_t		height;
	int16_t		patchcount;
	strifemappatch_t	patches[1];
};


//==========================================================================
//
// In-memory representation of a single PNAMES lump entry
//
//==========================================================================

struct FPatchLookup
{
	FString Name;
};


void FMultipatchTextureBuilder::MakeTexture(BuildInfo &buildinfo, ETextureType usetype)
{
	FImageTexture *tex = new FImageTexture(nullptr, buildinfo.Name);
	tex->SetUseType(usetype);
	tex->bMultiPatch = true;
	tex->Width = buildinfo.Width;
	tex->Height = buildinfo.Height;
	tex->_LeftOffset[0] = buildinfo.LeftOffset[0];
	tex->_LeftOffset[1] = buildinfo.LeftOffset[1];
	tex->_TopOffset[0] = buildinfo.TopOffset[0];
	tex->_TopOffset[1] = buildinfo.TopOffset[1];
	tex->Scale = buildinfo.Scale;
	tex->bMasked = true;	// we do not really know yet.
	tex->bTranslucent = -1;
	tex->bWorldPanning = buildinfo.bWorldPanning;
	tex->bNoDecals = buildinfo.bNoDecals;
	tex->SourceLump = buildinfo.DefinitionLump;
	buildinfo.tex = tex;
	TexMan.AddTexture(tex);
}

//==========================================================================
//
// The reader for TEXTUREx
//
//==========================================================================


//==========================================================================
//
// FMultiPatchTexture :: FMultiPatchTexture
//
//==========================================================================

void FMultipatchTextureBuilder::BuildTexture(const void *texdef, FPatchLookup *patchlookup, int maxpatchnum, bool strife, int deflumpnum, ETextureType usetype)
{
	BuildInfo &buildinfo = BuiltTextures[BuiltTextures.Reserve(1)];

	union
	{
		const maptexture_t			*d;
		const strifemaptexture_t	*s;
	}
	mtexture;

	union
	{
		const mappatch_t			*d;
		const strifemappatch_t		*s;
	}
	mpatch;

	int i;

	mtexture.d = (const maptexture_t *)texdef;
	int NumParts;

	if (strife)
	{
		NumParts = SAFESHORT(mtexture.s->patchcount);
	}
	else
	{
		NumParts = SAFESHORT(mtexture.d->patchcount);
	}

	if (NumParts < 0)
	{
		I_Error("Bad texture directory");
	}

	buildinfo.Parts.Resize(NumParts);
	buildinfo.Inits.Resize(NumParts);
	buildinfo.Width = SAFESHORT(mtexture.d->width);
	buildinfo.Height = SAFESHORT(mtexture.d->height);
	buildinfo.Name = (char *)mtexture.d->name;

	buildinfo.Scale.X = mtexture.d->ScaleX ? mtexture.d->ScaleX / 8. : 1.;
	buildinfo.Scale.Y = mtexture.d->ScaleY ? mtexture.d->ScaleY / 8. : 1.;

	if (mtexture.d->Flags & MAPTEXF_WORLDPANNING)
	{
		buildinfo.bWorldPanning = true;
	}

	if (strife)
	{
		mpatch.s = &mtexture.s->patches[0];
	}
	else
	{
		mpatch.d = &mtexture.d->patches[0];
	}

	for (i = 0; i < NumParts; ++i)
	{
		if (unsigned(LittleShort(mpatch.d->patch)) >= unsigned(maxpatchnum))
		{
			I_Error("Bad PNAMES and/or texture directory:\n\nPNAMES has %d entries, but\n%s wants to use entry %d.",
				maxpatchnum, buildinfo.Name.GetChars(), LittleShort(mpatch.d->patch) + 1);
		}
		buildinfo.Parts[i].OriginX = LittleShort(mpatch.d->originx);
		buildinfo.Parts[i].OriginY = LittleShort(mpatch.d->originy);
		buildinfo.Parts[i].Image = nullptr;
		buildinfo.Inits[i].TexName = patchlookup[LittleShort(mpatch.d->patch)].Name;
		buildinfo.Inits[i].UseType = ETextureType::WallPatch;
		if (strife)
			mpatch.s++;
		else
			mpatch.d++;
	}
	if (NumParts == 0)
	{
		Printf("Texture %s is left without any patches\n", buildinfo.Name.GetChars());
	}

	// Insert the incomplete texture right here so that it's in the correct place.
	MakeTexture(buildinfo, usetype);
	buildinfo.DefinitionLump = deflumpnum;
}

//==========================================================================
//
// FTextureManager :: AddTexturesLump
//
//==========================================================================

void FMultipatchTextureBuilder::AddTexturesLump(const void *lumpdata, int lumpsize, int deflumpnum, int patcheslump, int firstdup, bool texture1)
{
	TArray<FPatchLookup> patchlookup;
	int i;
	uint32_t numpatches;

	if (firstdup == 0)
	{
		firstdup = (int)TexMan.NumTextures();
	}

	{
		auto pnames = Wads.OpenLumpReader(patcheslump);
		numpatches = pnames.ReadUInt32();

		// Check whether the amount of names reported is correct.
		if ((signed)numpatches < 0)
		{
			Printf("Corrupt PNAMES lump found (negative amount of entries reported)\n");
			return;
		}

		// Check whether the amount of names reported is correct.
		int lumplength = Wads.LumpLength(patcheslump);
		if (numpatches > uint32_t((lumplength - 4) / 8))
		{
			Printf("PNAMES lump is shorter than required (%u entries reported but only %d bytes (%d entries) long\n",
				numpatches, lumplength, (lumplength - 4) / 8);
			// Truncate but continue reading. Who knows how many such lumps exist?
			numpatches = (lumplength - 4) / 8;
		}

		// Catalog the patches these textures use so we know which
		// textures they represent.
		patchlookup.Resize(numpatches);
		for (uint32_t i = 0; i < numpatches; ++i)
		{
			char pname[9];
			pnames.Read(pname, 8);
			pname[8] = '\0';
			patchlookup[i].Name = pname;
		}
	}

	bool isStrife = false;
	const uint32_t *maptex, *directory;
	uint32_t maxoff;
	int numtextures;
	uint32_t offset = 0;   // Shut up, GCC!

	maptex = (const uint32_t *)lumpdata;
	numtextures = LittleLong(*maptex);
	maxoff = lumpsize;

	if (maxoff < uint32_t(numtextures + 1) * 4)
	{
		Printf("Texture directory is too short\n");
		return;
	}

	// Scan the texture lump to decide if it contains Doom or Strife textures
	for (i = 0, directory = maptex + 1; i < numtextures; ++i)
	{
		offset = LittleLong(directory[i]);
		if (offset > maxoff)
		{
			Printf("Bad texture directory\n");
			return;
		}

		maptexture_t *tex = (maptexture_t *)((uint8_t *)maptex + offset);

		// There is bizzarely a Doom editing tool that writes to the
		// first two elements of columndirectory, so I can't check those.
		if (SAFESHORT(tex->patchcount) < 0 ||
			tex->columndirectory[2] != 0 ||
			tex->columndirectory[3] != 0)
		{
			isStrife = true;
			break;
		}
	}


	// Textures defined earlier in the lump take precedence over those defined later,
	// but later TEXTUREx lumps take precedence over earlier ones.
	for (i = 1, directory = maptex; i <= numtextures; ++i)
	{
		if (i == 1 && texture1)
		{
			// The very first texture is just a dummy. Copy its dimensions to texture 0.
			// It still needs to be created in case someone uses it by name.
			offset = LittleLong(directory[1]);
			const maptexture_t *tex = (const maptexture_t *)((const uint8_t *)maptex + offset);
			FTexture *tex0 = TexMan.ByIndex(0);
			tex0->SetSize(SAFESHORT(tex->width), SAFESHORT(tex->height));
		}

		offset = LittleLong(directory[i]);
		if (offset > maxoff)
		{
			Printf("Bad texture directory\n");
			return;
		}

		// If this texture was defined already in this lump, skip it
		// This could cause problems with animations that use the same name for intermediate
		// textures. Should I be worried?
		int j;
		for (j = (int)TexMan.NumTextures() - 1; j >= firstdup; --j)
		{
			if (strnicmp(TexMan.ByIndex(j)->GetName(), (const char *)maptex + offset, 8) == 0)
				break;
		}
		if (j + 1 == firstdup)
		{
			BuildTexture((const uint8_t *)maptex + offset, patchlookup.Data(), numpatches, isStrife, deflumpnum, (i == 1 && texture1) ? ETextureType::FirstDefined : ETextureType::Wall);
			StartScreen->Progress();
		}
	}
}


//==========================================================================
//
// FTextureManager :: AddTexturesLumps
//
//==========================================================================

void FMultipatchTextureBuilder::AddTexturesLumps(int lump1, int lump2, int patcheslump)
{
	int firstdup = (int)TexMan.NumTextures();

	if (lump1 >= 0)
	{
		FMemLump texdir = Wads.ReadLump(lump1);
		AddTexturesLump(texdir.GetMem(), Wads.LumpLength(lump1), lump1, patcheslump, firstdup, true);
	}
	if (lump2 >= 0)
	{
		FMemLump texdir = Wads.ReadLump(lump2);
		AddTexturesLump(texdir.GetMem(), Wads.LumpLength(lump2), lump2, patcheslump, firstdup, false);
	}
}

//==========================================================================
//
// THe reader for the textual format
//
//==========================================================================


//==========================================================================
//
// 
//
//==========================================================================

void FMultipatchTextureBuilder::ParsePatch(FScanner &sc, BuildInfo &info, TexPart & part, TexInit &init)
{
	FString patchname;
	int Mirror = 0;
	sc.MustGetString();

	init.TexName = sc.String;
	sc.MustGetStringName(",");
	sc.MustGetNumber();
	part.OriginX = sc.Number;
	sc.MustGetStringName(",");
	sc.MustGetNumber();
	part.OriginY = sc.Number;

	if (sc.CheckString("{"))
	{
		while (!sc.CheckString("}"))
		{
			sc.MustGetString();
			if (sc.Compare("flipx"))
			{
				Mirror |= 1;
			}
			else if (sc.Compare("flipy"))
			{
				Mirror |= 2;
			}
			else if (sc.Compare("rotate"))
			{
				sc.MustGetNumber();
				sc.Number = (((sc.Number + 90) % 360) - 90);
				if (sc.Number != 0 && sc.Number != 90 && sc.Number != 180 && sc.Number != -90)
				{
					sc.ScriptError("Rotation must be a multiple of 90 degrees.");
				}
				part.Rotate = (sc.Number / 90) & 3;
			}
			else if (sc.Compare("Translation"))
			{
				int match;

				info.bComplex = true;
				if (part.Translation != NULL) delete part.Translation;
				part.Translation = NULL;
				part.Blend = 0;
				static const char *maps[] = { "inverse", "gold", "red", "green", "blue", NULL };
				sc.MustGetString();

				match = sc.MatchString(maps);
				if (match >= 0)
				{
					part.Blend = BLEND_SPECIALCOLORMAP1 + match;
				}
				else if (sc.Compare("ICE"))
				{
					part.Blend = BLEND_ICEMAP;
				}
				else if (sc.Compare("DESATURATE"))
				{
					sc.MustGetStringName(",");
					sc.MustGetNumber();
					part.Blend = BLEND_DESATURATE1 + clamp(sc.Number - 1, 0, 30);
				}
				else
				{
					sc.UnGet();
					part.Translation = new FRemapTable;
					part.Translation->MakeIdentity();
					do
					{
						sc.MustGetString();

						try
						{
							part.Translation->AddToTranslation(sc.String);
						}
						catch (CRecoverableError &err)
						{
							sc.ScriptMessage("Error in translation '%s':\n" TEXTCOLOR_YELLOW "%s\n", sc.String, err.GetMessage());
						}
					} while (sc.CheckString(","));
				}

			}
			else if (sc.Compare("Colormap"))
			{
				float r1, g1, b1;
				float r2, g2, b2;

				sc.MustGetFloat();
				r1 = (float)sc.Float;
				sc.MustGetStringName(",");
				sc.MustGetFloat();
				g1 = (float)sc.Float;
				sc.MustGetStringName(",");
				sc.MustGetFloat();
				b1 = (float)sc.Float;
				if (!sc.CheckString(","))
				{
					part.Blend = AddSpecialColormap(0, 0, 0, r1, g1, b1);
				}
				else
				{
					sc.MustGetFloat();
					r2 = (float)sc.Float;
					sc.MustGetStringName(",");
					sc.MustGetFloat();
					g2 = (float)sc.Float;
					sc.MustGetStringName(",");
					sc.MustGetFloat();
					b2 = (float)sc.Float;
					part.Blend = AddSpecialColormap(r1, g1, b1, r2, g2, b2);
				}
			}
			else if (sc.Compare("Blend"))
			{
				info.bComplex = true;
				if (part.Translation != NULL) delete part.Translation;
				part.Translation = NULL;
				part.Blend = 0;

				if (!sc.CheckNumber())
				{
					sc.MustGetString();
					part.Blend = V_GetColor(NULL, sc);
				}
				else
				{
					int r, g, b;

					r = sc.Number;
					sc.MustGetStringName(",");
					sc.MustGetNumber();
					g = sc.Number;
					sc.MustGetStringName(",");
					sc.MustGetNumber();
					b = sc.Number;
					//sc.MustGetStringName(","); This was never supposed to be here. 
					part.Blend = MAKERGB(r, g, b);
				}
				// Blend.a may never be 0 here.
				if (sc.CheckString(","))
				{
					sc.MustGetFloat();
					if (sc.Float > 0.f)
						part.Blend.a = clamp<int>(int(sc.Float * 255), 1, 254);
					else
						part.Blend = 0;
				}
				else part.Blend.a = 255;
			}
			else if (sc.Compare("alpha"))
			{
				sc.MustGetFloat();
				part.Alpha = clamp<blend_t>(int(sc.Float * BLENDUNIT), 0, BLENDUNIT);
				// bComplex is not set because it is only needed when the style is not OP_COPY.
			}
			else if (sc.Compare("style"))
			{
				static const char *styles[] = { "copy", "translucent", "add", "subtract", "reversesubtract", "modulate", "copyalpha", "copynewalpha", "overlay", NULL };
				sc.MustGetString();
				part.op = sc.MustMatchString(styles);
				info.bComplex |= (part.op != OP_COPY);
			}
			else if (sc.Compare("useoffsets"))
			{
				init.UseOffsets = true;
			}
		}
	}
	if (Mirror & 2)
	{
		part.Rotate = (part.Rotate + 2) & 3;
		Mirror ^= 1;
	}
	if (Mirror & 1)
	{
		part.Rotate |= 4;
	}
}


//==========================================================================
//
// Constructor for text based multipatch definitions
//
//==========================================================================

void FMultipatchTextureBuilder::ParseTexture(FScanner &sc, ETextureType UseType)
{
	BuildInfo &buildinfo = BuiltTextures[BuiltTextures.Reserve(1)];

	bool bSilent = false;
	
	buildinfo.textual = true;
	sc.SetCMode(true);
	sc.MustGetString();

	const char *textureName = nullptr;
	if (sc.Compare("optional"))
	{
		bSilent = true;
		sc.MustGetString();
		if (sc.Compare(","))
		{
			// this is not right. Apparently a texture named 'optional' is being defined right now...
			sc.UnGet();
			textureName = "optional";
			bSilent = false;
		}
	}
	buildinfo.Name = !textureName ? sc.String : textureName;
	buildinfo.Name.ToUpper();
	sc.MustGetStringName(",");
	sc.MustGetNumber();
	buildinfo.Width = sc.Number;
	sc.MustGetStringName(",");
	sc.MustGetNumber();
	buildinfo.Height = sc.Number;

	bool offset2set = false;
	if (sc.CheckString("{"))
	{
		while (!sc.CheckString("}"))
		{
			sc.MustGetString();
			if (sc.Compare("XScale"))
			{
				sc.MustGetFloat();
				buildinfo.Scale.X = sc.Float;
				if (buildinfo.Scale.X == 0) sc.ScriptError("Texture %s is defined with null x-scale\n", buildinfo.Name.GetChars());
			}
			else if (sc.Compare("YScale"))
			{
				sc.MustGetFloat();
				buildinfo.Scale.Y = sc.Float;
				if (buildinfo.Scale.Y == 0) sc.ScriptError("Texture %s is defined with null y-scale\n", buildinfo.Name.GetChars());
			}
			else if (sc.Compare("WorldPanning"))
			{
				buildinfo.bWorldPanning = true;
			}
			else if (sc.Compare("NullTexture"))
			{
				UseType = ETextureType::Null;
			}
			else if (sc.Compare("NoDecals"))
			{
				buildinfo.bNoDecals = true;
			}
			else if (sc.Compare("Patch"))
			{
				TexPart part;
				TexInit init;
				ParsePatch(sc, buildinfo, part, init);
				if (init.TexName.IsNotEmpty())
				{
					buildinfo.Parts.Push(part);
					init.UseType = ETextureType::WallPatch;
					init.Silent = bSilent;
					init.HasLine = true;
					init.sc = sc;
					buildinfo.Inits.Push(init);
				}
				part.Image = nullptr;
				part.Translation = nullptr;
			}
			else if (sc.Compare("Sprite"))
			{
				TexPart part;
				TexInit init;
				ParsePatch(sc, buildinfo, part, init);
				if (init.TexName.IsNotEmpty())
				{
					buildinfo.Parts.Push(part);
					init.UseType = ETextureType::Sprite;
					init.Silent = bSilent;
					init.HasLine = true;
					init.sc = sc;
					buildinfo.Inits.Push(init);
				}
				part.Image = nullptr;
				part.Translation = nullptr;
			}
			else if (sc.Compare("Graphic"))
			{
				TexPart part;
				TexInit init;
				ParsePatch(sc, buildinfo, part, init);
				if (init.TexName.IsNotEmpty())
				{
					buildinfo.Parts.Push(part);
					init.UseType = ETextureType::MiscPatch;
					init.Silent = bSilent;
					init.HasLine = true;
					init.sc = sc;
					buildinfo.Inits.Push(init);
				}
				part.Image = nullptr;
				part.Translation = nullptr;
			}
			else if (sc.Compare("Offset"))
			{
				sc.MustGetNumber();
				buildinfo.LeftOffset[0] = sc.Number;
				sc.MustGetStringName(",");
				sc.MustGetNumber();
				buildinfo.TopOffset[0] = sc.Number;
				if (!offset2set)
				{
					buildinfo.LeftOffset[1] = buildinfo.LeftOffset[0];
					buildinfo.TopOffset[1] = buildinfo.TopOffset[0];
				}
			}
			else if (sc.Compare("Offset2"))
			{
				sc.MustGetNumber();
				buildinfo.LeftOffset[1] = sc.Number;
				sc.MustGetStringName(",");
				sc.MustGetNumber();
				buildinfo.TopOffset[1] = sc.Number;
				offset2set = true;
			}
			else
			{
				sc.ScriptError("Unknown texture property '%s'", sc.String);
			}
		}
	}

	if (buildinfo.Width <= 0 || buildinfo.Height <= 0)
	{
		UseType = ETextureType::Null;
		Printf("Texture %s has invalid dimensions (%d, %d)\n", buildinfo.Name.GetChars(), buildinfo.Width, buildinfo.Height);
		buildinfo.Width = buildinfo.Height = 1;
	}

	MakeTexture(buildinfo, UseType);
	sc.SetCMode(false);
}



//==========================================================================
//
// FMultiPatchTexture :: CheckForHacks
//
//==========================================================================

void FMultipatchTextureBuilder::CheckForHacks(BuildInfo &buildinfo)
{
	if (buildinfo.Parts.Size() == 0)
	{
		return;
	}

	// Heretic sky textures are marked as only 128 pixels tall,
	// even though they are really 200 pixels tall.
	if (gameinfo.gametype == GAME_Heretic &&
		buildinfo.Name.Len() == 4 &&
		buildinfo.Name[0] == 'S' &&
		buildinfo.Name[1] == 'K' &&
		buildinfo.Name[2] == 'Y' &&
		buildinfo.Name[3] >= '1' &&
		buildinfo.Name[3] <= '3' &&
		buildinfo.Height == 128)
	{
		buildinfo.Height = 200;
		buildinfo.tex->SetSize(buildinfo.tex->Width, 200);
		return;
	}

	// The Doom E1 sky has its patch's y offset at -8 instead of 0.
	if (gameinfo.gametype == GAME_Doom &&
		!(gameinfo.flags & GI_MAPxx) &&
		buildinfo.Name.Len() == 4 &&
		buildinfo.Parts.Size() == 1 &&
		buildinfo.Height == 128 &&
		buildinfo.Parts[0].OriginY == -8 &&
		buildinfo.Name[0] == 'S' &&
		buildinfo.Name[1] == 'K' &&
		buildinfo.Name[2] == 'Y' &&
		buildinfo.Name[3] == '1')
	{
		buildinfo.Parts[0].OriginY = 0;
		return;
	}

	// BIGDOOR7 in Doom also has patches at y offset -4 instead of 0.
	if (gameinfo.gametype == GAME_Doom &&
		!(gameinfo.flags & GI_MAPxx) &&
		buildinfo.Name.CompareNoCase("BIGDOOR7") == 0 &&
		buildinfo.Parts.Size() == 2 &&
		buildinfo.Height == 128 &&
		buildinfo.Parts[0].OriginY == -4 &&
		buildinfo.Parts[1].OriginY == -4)
	{
		buildinfo.Parts[0].OriginY = 0;
		buildinfo.Parts[1].OriginY = 0;
		return;
	}
}

//==========================================================================
//
//
//
//==========================================================================

void FMultipatchTextureBuilder::ResolvePatches(BuildInfo &buildinfo)
{
	for (unsigned i = 0; i < buildinfo.Inits.Size(); i++)
	{
		FTextureID texno = TexMan.CheckForTexture(buildinfo.Inits[i].TexName, buildinfo.Inits[i].UseType);
		if (texno == buildinfo.tex->id)	// we found ourselves. Try looking for another one with the same name which is not a multipatch texture itself.
		{
			TArray<FTextureID> list;
			TexMan.ListTextures(buildinfo.Inits[i].TexName, list, true);
			for (int i = list.Size() - 1; i >= 0; i--)
			{
				if (list[i] != buildinfo.tex->id && !TexMan.GetTexture(list[i])->bMultiPatch)
				{
					texno = list[i];
					break;
				}
			}
			if (texno == buildinfo.tex->id)
			{
				if (buildinfo.Inits[i].HasLine) buildinfo.Inits[i].sc.Message(MSG_WARNING, "Texture '%s' references itself as patch\n", buildinfo.Inits[i].TexName.GetChars());
				else Printf(TEXTCOLOR_YELLOW  "Texture '%s' references itself as patch\n", buildinfo.Inits[i].TexName.GetChars());
				continue;
			}
			else
			{
				// If it could be resolved, just print a developer warning.
				DPrintf(DMSG_WARNING, "Resolved self-referencing texture by picking an older entry for %s\n", buildinfo.Inits[i].TexName.GetChars());
			}
		}

		if (!texno.isValid())
		{
			if (!buildinfo.Inits[i].Silent)
			{
				if (buildinfo.Inits[i].HasLine) buildinfo.Inits[i].sc.Message(MSG_WARNING, "Unknown patch '%s' in texture '%s'\n", buildinfo.Inits[i].TexName.GetChars(), buildinfo.Name.GetChars());
				else Printf(TEXTCOLOR_YELLOW  "Unknown patch '%s' in texture '%s'\n", buildinfo.Inits[i].TexName.GetChars(), buildinfo.Name.GetChars());
			}
		}
		else
		{
			FTexture *tex = TexMan.GetTexture(texno);

			if (tex != nullptr && tex->isValid())
			{
				//We cannot set the image source yet. First all textures need to be resolved.
				buildinfo.Inits[i].Texture = tex;
				buildinfo.tex->bComplex |= tex->bComplex;
				buildinfo.bComplex |= tex->bComplex;
				if (buildinfo.Inits[i].UseOffsets)
				{
					buildinfo.Parts[i].OriginX -= tex->GetLeftOffset(0);
					buildinfo.Parts[i].OriginY -= tex->GetTopOffset(0);
				}
			}
			else
			{
				// The patch is bogus. Remove it.
				if (buildinfo.Inits[i].HasLine) buildinfo.Inits[i].sc.Message(MSG_WARNING, "Invalid patch '%s' in texture '%s'\n", buildinfo.Inits[i].TexName.GetChars(), buildinfo.Name.GetChars());
				else Printf(TEXTCOLOR_YELLOW  "Invalid patch '%s' in texture '%s'\n", buildinfo.Inits[i].TexName.GetChars(), buildinfo.Name.GetChars());
				i--;
			}
		}
	}
	for (unsigned i = 0; i < buildinfo.Inits.Size(); i++)
	{
		if (buildinfo.Inits[i].Texture == nullptr)
		{
			buildinfo.Inits.Delete(i);
			buildinfo.Parts.Delete(i);
			i--;
		}
	}

	CheckForHacks(buildinfo);
}

void FMultipatchTextureBuilder::ResolveAllPatches()
{
	for (auto &bi : BuiltTextures)
	{
		ResolvePatches(bi);
	}
	// Now try to resolve the images. We only can do this at the end when all multipatch textures are set up.
	int i = 0;

	// reverse the list so that the Delete operation in the loop below deletes at the end.
	// For normal sized lists this is of no real concern, but Total Chaos has over 250000 textures where this becomes a performance issue.
	for (unsigned i = 0; i < BuiltTextures.Size() / 2; i++)
	{
		// std::swap is VERY inefficient here...
		BuiltTextures[i].swap(BuiltTextures[BuiltTextures.Size() - 1 - i]);
	}


	while (BuiltTextures.Size() > 0)
	{
		bool donesomething = false;

		for (int i = BuiltTextures.Size()-1; i>= 0; i--)
		{
			auto &buildinfo = BuiltTextures[i];
			bool hasEmpty = false;

			for (unsigned j = 0; j < buildinfo.Inits.Size(); j++)
			{
				if (buildinfo.Parts[j].Image == nullptr)
				{
					auto image = buildinfo.Inits[j].Texture->GetImage();
					if (image != nullptr)
					{
						buildinfo.Parts[j].Image = image;
						donesomething = true;
					}
					else hasEmpty = true;
				}
			}
			if (!hasEmpty)
			{
				// If this texture is just a wrapper around a single patch, we can simply
				// use that patch's image directly here.

				bool done = false;
				if (buildinfo.Parts.Size() == 1)
				{
					if (buildinfo.Parts[0].OriginX == 0 && buildinfo.Parts[0].OriginY == 0 &&
						buildinfo.Parts[0].Image->GetWidth() == buildinfo.Width &&
						buildinfo.Parts[0].Image->GetHeight() == buildinfo.Height &&
						buildinfo.Parts[0].Rotate == 0 &&
						!buildinfo.bComplex)
					{
						buildinfo.tex->SetImage(buildinfo.Parts[0].Image);
						done = true;
					}
				}
				if (!done)
				{
					auto img = new FMultiPatchTexture(buildinfo.Width, buildinfo.Height, buildinfo.Parts, buildinfo.bComplex, buildinfo.textual);
					buildinfo.tex->SetImage(img);
				}

				BuiltTextures.Delete(i);
				donesomething = true;
			}
		}
		if (!donesomething)
		{
			Printf("%d Unresolved textures remain\n", BuiltTextures.Size());
			for (auto &b : BuiltTextures)
			{
				Printf("%s\n", b.Name.GetChars());
			}
			break;
		}
	}
}
