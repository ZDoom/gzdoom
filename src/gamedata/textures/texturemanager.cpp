/*
** texturemanager.cpp
** The texture manager class
**
**---------------------------------------------------------------------------
** Copyright 2004-2008 Randy Heit
** Copyright 2006-2008 Christoph Oelckers
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

#include "doomtype.h"
#include "doomstat.h"
#include "w_wad.h"
#include "templates.h"
#include "i_system.h"
#include "gstrings.h"

#include "r_data/r_translate.h"
#include "r_data/sprites.h"
#include "c_dispatch.h"
#include "v_text.h"
#include "sc_man.h"
#include "gi.h"
#include "st_start.h"
#include "cmdlib.h"
#include "g_level.h"
#include "v_video.h"
#include "r_sky.h"
#include "vm.h"
#include "image.h"
#include "formats/multipatchtexture.h"
#include "swrenderer/textures/r_swtexture.h"

FTextureManager TexMan;

CUSTOM_CVAR(Bool, vid_nopalsubstitutions, false, CVAR_ARCHIVE|CVAR_NOINITCALL)
{
	// This is in case the sky texture has been substituted.
	R_InitSkyMap ();
}

//==========================================================================
//
// FTextureManager :: FTextureManager
//
//==========================================================================

FTextureManager::FTextureManager ()
{
	memset (HashFirst, -1, sizeof(HashFirst));

	for (int i = 0; i < 2048; ++i)
	{
		sintable[i] = short(sin(i*(M_PI / 1024)) * 16384);
	}
}

//==========================================================================
//
// FTextureManager :: ~FTextureManager
//
//==========================================================================

FTextureManager::~FTextureManager ()
{
	DeleteAll();
}

//==========================================================================
//
// FTextureManager :: DeleteAll
//
//==========================================================================

void FTextureManager::DeleteAll()
{
	FImageSource::ClearImages();
	for (unsigned int i = 0; i < Textures.Size(); ++i)
	{
		delete Textures[i].Texture;
	}
	Textures.Clear();
	Translation.Clear();
	FirstTextureForFile.Clear();
	memset (HashFirst, -1, sizeof(HashFirst));
	DefaultTexture.SetInvalid();

	for (unsigned i = 0; i < mAnimations.Size(); i++)
	{
		if (mAnimations[i] != NULL)
		{
			M_Free (mAnimations[i]);
			mAnimations[i] = NULL;
		}
	}
	mAnimations.Clear();

	for (unsigned i = 0; i < mSwitchDefs.Size(); i++)
	{
		if (mSwitchDefs[i] != NULL)
		{
			M_Free (mSwitchDefs[i]);
			mSwitchDefs[i] = NULL;
		}
	}
	mSwitchDefs.Clear();

	for (unsigned i = 0; i < mAnimatedDoors.Size(); i++)
	{
		if (mAnimatedDoors[i].TextureFrames != NULL)
		{
			delete[] mAnimatedDoors[i].TextureFrames;
			mAnimatedDoors[i].TextureFrames = NULL;
		}
	}
	mAnimatedDoors.Clear();
	BuildTileData.Clear();
	tmanips.Clear();
}

//==========================================================================
//
// Flushes all hardware dependent data.
// Thia must not, under any circumstances, delete the wipe textures, because
// all CCMDs triggering a flush can be executed while a wipe is in progress
//
// This now also deletes the software textures because having the software
// renderer use the texture scalers is a planned feature and that is the
// main reason to call this outside of the destruction code.
//
//==========================================================================

void FTextureManager::FlushAll()
{
	for (int i = TexMan.NumTextures() - 1; i >= 0; i--)
	{
		for (int j = 0; j < 2; j++)
		{
			Textures[i].Texture->SystemTextures.Clean(true, true);
			delete Textures[i].Texture->SoftwareTexture;
			Textures[i].Texture->SoftwareTexture = nullptr;
		}
	}
}

//==========================================================================
//
// FTextureManager :: CheckForTexture
//
//==========================================================================

FTextureID FTextureManager::CheckForTexture (const char *name, ETextureType usetype, BITFIELD flags)
{
	int i;
	int firstfound = -1;
	auto firsttype = ETextureType::Null;

	if (name == NULL || name[0] == '\0')
	{
		return FTextureID(-1);
	}
	// [RH] Doom counted anything beginning with '-' as "no texture".
	// Hopefully nobody made use of that and had textures like "-EMPTY",
	// because -NOFLAT- is a valid graphic for ZDoom.
	if (name[0] == '-' && name[1] == '\0')
	{
		return FTextureID(0);
	}

	for(i = HashFirst[MakeKey(name) % HASH_SIZE]; i != HASH_END; i = Textures[i].HashNext)
	{
		const FTexture *tex = Textures[i].Texture;


		if (stricmp (tex->Name, name) == 0 )
		{
			// If we look for short names, we must ignore any long name texture.
			if ((flags & TEXMAN_ShortNameOnly) && tex->bFullNameTexture)
			{
				continue;
			}
			// The name matches, so check the texture type
			if (usetype == ETextureType::Any)
			{
				// All NULL textures should actually return 0
				if (tex->UseType == ETextureType::FirstDefined && !(flags & TEXMAN_ReturnFirst)) return 0;
				if (tex->UseType == ETextureType::SkinGraphic && !(flags & TEXMAN_AllowSkins)) return 0;
				return FTextureID(tex->UseType==ETextureType::Null ? 0 : i);
			}
			else if ((flags & TEXMAN_Overridable) && tex->UseType == ETextureType::Override)
			{
				return FTextureID(i);
			}
			else if (tex->UseType == usetype)
			{
				return FTextureID(i);
			}
			else if (tex->UseType == ETextureType::FirstDefined && usetype == ETextureType::Wall)
			{
				if (!(flags & TEXMAN_ReturnFirst)) return FTextureID(0);
				else return FTextureID(i);
			}
			else if (tex->UseType == ETextureType::Null && usetype == ETextureType::Wall)
			{
				// We found a NULL texture on a wall -> return 0
				return FTextureID(0);
			}
			else
			{
				if (firsttype == ETextureType::Null ||
					(firsttype == ETextureType::MiscPatch &&
					 tex->UseType != firsttype &&
					 tex->UseType != ETextureType::Null)
				   )
				{
					firstfound = i;
					firsttype = tex->UseType;
				}
			}
		}
	}

	if ((flags & TEXMAN_TryAny) && usetype != ETextureType::Any)
	{
		// Never return the index of NULL textures.
		if (firstfound != -1)
		{
			if (firsttype == ETextureType::Null) return FTextureID(0);
			if (firsttype == ETextureType::FirstDefined && !(flags & TEXMAN_ReturnFirst)) return FTextureID(0);
			return FTextureID(firstfound);
		}
	}

	
	if (!(flags & TEXMAN_ShortNameOnly))
	{
		// We intentionally only look for textures in subdirectories.
		// Any graphic being placed in the zip's root directory can not be found by this.
		if (strchr(name, '/'))
		{
			FTexture *const NO_TEXTURE = (FTexture*)-1;
			int lump = Wads.CheckNumForFullName(name);
			if (lump >= 0)
			{
				FTexture *tex = Wads.GetLinkedTexture(lump);
				if (tex == NO_TEXTURE) return FTextureID(-1);
				if (tex != NULL) return tex->id;
				if (flags & TEXMAN_DontCreate) return FTextureID(-1);	// we only want to check, there's no need to create a texture if we don't have one yet.
				tex = FTexture::CreateTexture("", lump, ETextureType::Override);
				if (tex != NULL)
				{
					tex->AddAutoMaterials();
					Wads.SetLinkedTexture(lump, tex);
					return AddTexture(tex);
				}
				else
				{
					// mark this lump as having no valid texture so that we don't have to retry creating one later.
					Wads.SetLinkedTexture(lump, NO_TEXTURE);
				}
			}
		}
	}

	return FTextureID(-1);
}

static int CheckForTexture(const FString &name, int type, int flags)
{
	return TexMan.CheckForTexture(name, static_cast<ETextureType>(type), flags).GetIndex();
}

DEFINE_ACTION_FUNCTION_NATIVE(_TexMan, CheckForTexture, CheckForTexture)
{
	PARAM_PROLOGUE;
	PARAM_STRING(name);
	PARAM_INT(type);
	PARAM_INT(flags);
	ACTION_RETURN_INT(CheckForTexture(name, type, flags));
}

//==========================================================================
//
// FTextureManager :: ListTextures
//
//==========================================================================

int FTextureManager::ListTextures (const char *name, TArray<FTextureID> &list, bool listall)
{
	int i;

	if (name == NULL || name[0] == '\0')
	{
		return 0;
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
			// NULL textures must be ignored.
			if (tex->UseType!=ETextureType::Null) 
			{
				unsigned int j = list.Size();
				if (!listall)
				{
					for (j = 0; j < list.Size(); j++)
					{
						// Check for overriding definitions from newer WADs
						if (Textures[list[j].GetIndex()].Texture->UseType == tex->UseType) break;
					}
				}
				if (j==list.Size()) list.Push(FTextureID(i));
			}
		}
		i = Textures[i].HashNext;
	}
	return list.Size();
}

//==========================================================================
//
// FTextureManager :: GetTextures
//
//==========================================================================

FTextureID FTextureManager::GetTextureID (const char *name, ETextureType usetype, BITFIELD flags)
{
	FTextureID i;

	if (name == NULL || name[0] == 0)
	{
		return FTextureID(0);
	}
	else
	{
		i = CheckForTexture (name, usetype, flags | TEXMAN_TryAny);
	}

	if (!i.Exists())
	{
		// Use a default texture instead of aborting like Doom did
		Printf ("Unknown texture: \"%s\"\n", name);
		i = DefaultTexture;
	}
	return FTextureID(i);
}

//==========================================================================
//
// FTextureManager :: FindTexture
//
//==========================================================================

FTexture *FTextureManager::FindTexture(const char *texname, ETextureType usetype, BITFIELD flags)
{
	FTextureID texnum = CheckForTexture (texname, usetype, flags);
	return GetTexture(texnum.GetIndex());
}

//==========================================================================
//
// Defines how graphics substitution is handled.
// 0: Never replace a text-containing graphic with a font-based text.
// 1: Always replace, regardless of any missing information. Useful for testing the substitution without providing full data.
// 2: Only replace for non-default texts, i.e. if some language redefines the string's content, use it instead of the graphic. Never replace a localized graphic.
// 3: Only replace if the string is not the default and the graphic comes from the IWAD. Never replace a localized graphic.
// 4: Like 1, but lets localized graphics pass.
//
// The default is 3, which only replaces known content with non-default texts.
//
//==========================================================================

CUSTOM_CVAR(Int, cl_gfxlocalization, 3, CVAR_ARCHIVE)
{
	if (self < 0 || self > 4) self = 0;
}

//==========================================================================
//
// FTextureManager :: OkForLocalization
//
//==========================================================================

bool FTextureManager::OkForLocalization(FTextureID texnum, const char *substitute)
{
	if (!texnum.isValid()) return false;
	
	// First the unconditional settings, 0='never' and 1='always'.
	if (cl_gfxlocalization == 1 || gameinfo.forcetextinmenus) return false;
	if (cl_gfxlocalization == 0 || gameinfo.forcenogfxsubstitution) return true;
	
	uint32_t langtable = 0;
	if (*substitute == '$') substitute = GStrings.GetString(substitute+1, &langtable);
	else return true;	// String literals from the source data should never override graphics from the same definition.
	if (substitute == nullptr) return true;	// The text does not exist.
	
	// Modes 2, 3 and 4 must not replace localized textures.
	int localizedTex = ResolveLocalizedTexture(texnum.GetIndex());
	if (localizedTex != texnum.GetIndex()) return true;	// Do not substitute a localized variant of the graphics patch.
	
	// For mode 4 we are done now.
	if (cl_gfxlocalization == 4) return false;
	
	// Mode 2 and 3 must reject any text replacement from the default language tables.
	if ((langtable & MAKE_ID(255,0,0,0)) == MAKE_ID('*', 0, 0, 0)) return true;	// Do not substitute if the string comes from the default table.
	if (cl_gfxlocalization == 2) return false;
	
	// Mode 3 must also reject substitutions for non-IWAD content.
	int file = Wads.GetLumpFile(Textures[texnum.GetIndex()].Texture->SourceLump);
	if (file > Wads.GetMaxIwadNum()) return true;

	return false;
}

static int OkForLocalization(int index, const FString &substitute)
{
	return TexMan.OkForLocalization(FSetTextureID(index), substitute);
}

DEFINE_ACTION_FUNCTION_NATIVE(_TexMan, OkForLocalization, OkForLocalization)
{
	PARAM_PROLOGUE;
	PARAM_INT(name);
	PARAM_STRING(subst)
	ACTION_RETURN_INT(OkForLocalization(name, subst));
}

//==========================================================================
//
// FTextureManager :: AddTexture
//
//==========================================================================

FTextureID FTextureManager::AddTexture (FTexture *texture)
{
	int bucket;
	int hash;

	if (texture == NULL) return FTextureID(-1);

	// Later textures take precedence over earlier ones

	// Textures without name can't be looked for
	if (texture->Name[0] != '\0')
	{
		bucket = int(MakeKey (texture->Name) % HASH_SIZE);
		hash = HashFirst[bucket];
	}
	else
	{
		bucket = -1;
		hash = -1;
	}

	TextureHash hasher = { texture, hash };
	int trans = Textures.Push (hasher);
	Translation.Push (trans);
	if (bucket >= 0) HashFirst[bucket] = trans;
	return (texture->id = FTextureID(trans));
}

//==========================================================================
//
// FTextureManager :: CreateTexture
//
// Calls FTexture::CreateTexture and adds the texture to the manager.
//
//==========================================================================

FTextureID FTextureManager::CreateTexture (int lumpnum, ETextureType usetype)
{
	if (lumpnum != -1)
	{
		FString str;
		Wads.GetLumpName(str, lumpnum);
		FTexture *out = FTexture::CreateTexture(str, lumpnum, usetype);

		if (out != NULL) return AddTexture (out);
		else
		{
			Printf (TEXTCOLOR_ORANGE "Invalid data encountered for texture %s\n", Wads.GetLumpFullPath(lumpnum).GetChars());
			return FTextureID(-1);
		}
	}
	return FTextureID(-1);
}

//==========================================================================
//
// FTextureManager :: ReplaceTexture
//
//==========================================================================

void FTextureManager::ReplaceTexture (FTextureID picnum, FTexture *newtexture, bool free)
{
	int index = picnum.GetIndex();
	if (unsigned(index) >= Textures.Size())
		return;

	FTexture *oldtexture = Textures[index].Texture;

	newtexture->Name = oldtexture->Name;
	newtexture->UseType = oldtexture->UseType;
	Textures[index].Texture = newtexture;
	newtexture->id = oldtexture->id;
	oldtexture->Name = "";
	AddTexture(oldtexture);
}

//==========================================================================
//
// FTextureManager :: AreTexturesCompatible
//
// Checks if 2 textures are compatible for a ranged animation
//
//==========================================================================

bool FTextureManager::AreTexturesCompatible (FTextureID picnum1, FTextureID picnum2)
{
	int index1 = picnum1.GetIndex();
	int index2 = picnum2.GetIndex();
	if (unsigned(index1) >= Textures.Size() || unsigned(index2) >= Textures.Size())
		return false;

	FTexture *texture1 = Textures[index1].Texture;
	FTexture *texture2 = Textures[index2].Texture;

	// both textures must be the same type.
	if (texture1 == NULL || texture2 == NULL || texture1->UseType != texture2->UseType)
		return false;

	// both textures must be from the same file
	for(unsigned i = 0; i < FirstTextureForFile.Size() - 1; i++)
	{
		if (index1 >= FirstTextureForFile[i] && index1 < FirstTextureForFile[i+1])
		{
			return (index2 >= FirstTextureForFile[i] && index2 < FirstTextureForFile[i+1]);
		}
	}
	return false;
}


//==========================================================================
//
// FTextureManager :: AddGroup
//
//==========================================================================

void FTextureManager::AddGroup(int wadnum, int ns, ETextureType usetype)
{
	int firsttx = Wads.GetFirstLump(wadnum);
	int lasttx = Wads.GetLastLump(wadnum);
	FString Name;

	// Go from first to last so that ANIMDEFS work as expected. However,
	// to avoid duplicates (and to keep earlier entries from overriding
	// later ones), the texture is only inserted if it is the one returned
	// by doing a check by name in the list of wads.

	for (; firsttx <= lasttx; ++firsttx)
	{
		if (Wads.GetLumpNamespace(firsttx) == ns)
		{
			Wads.GetLumpName (Name, firsttx);

			if (Wads.CheckNumForName (Name, ns) == firsttx)
			{
				CreateTexture (firsttx, usetype);
			}
			StartScreen->Progress();
		}
		else if (ns == ns_flats && Wads.GetLumpFlags(firsttx) & LUMPF_MAYBEFLAT)
		{
			if (Wads.CheckNumForName (Name, ns) < firsttx)
			{
				CreateTexture (firsttx, usetype);
			}
			StartScreen->Progress();
		}
	}
}

//==========================================================================
//
// Adds all hires texture definitions.
//
//==========================================================================

void FTextureManager::AddHiresTextures (int wadnum)
{
	int firsttx = Wads.GetFirstLump(wadnum);
	int lasttx = Wads.GetLastLump(wadnum);

	FString Name;
	TArray<FTextureID> tlist;

	if (firsttx == -1 || lasttx == -1)
	{
		return;
	}

	for (;firsttx <= lasttx; ++firsttx)
	{
		if (Wads.GetLumpNamespace(firsttx) == ns_hires)
		{
			Wads.GetLumpName (Name, firsttx);

			if (Wads.CheckNumForName (Name, ns_hires) == firsttx)
			{
				tlist.Clear();
				int amount = ListTextures(Name, tlist);
				if (amount == 0)
				{
					// A texture with this name does not yet exist
					FTexture * newtex = FTexture::CreateTexture (Name, firsttx, ETextureType::Any);
					if (newtex != NULL)
					{
						newtex->UseType=ETextureType::Override;
						AddTexture(newtex);
					}
				}
				else
				{
					for(unsigned int i = 0; i < tlist.Size(); i++)
					{
						FTexture * newtex = FTexture::CreateTexture ("", firsttx, ETextureType::Any);
						if (newtex != NULL)
						{
							FTexture * oldtex = Textures[tlist[i].GetIndex()].Texture;

							// Replace the entire texture and adjust the scaling and offset factors.
							newtex->bWorldPanning = true;
							newtex->SetScaledSize(oldtex->GetScaledWidth(), oldtex->GetScaledHeight());
							newtex->_LeftOffset[0] = int(oldtex->GetScaledLeftOffset(0) * newtex->Scale.X);
							newtex->_LeftOffset[1] = int(oldtex->GetScaledLeftOffset(1) * newtex->Scale.X);
							newtex->_TopOffset[0] = int(oldtex->GetScaledTopOffset(0) * newtex->Scale.Y);
							newtex->_TopOffset[1] = int(oldtex->GetScaledTopOffset(1) * newtex->Scale.Y);
							ReplaceTexture(tlist[i], newtex, true);
						}
					}
				}
				StartScreen->Progress();
			}
		}
	}
}

//==========================================================================
//
// Loads the HIRESTEX lumps
//
//==========================================================================

void FTextureManager::LoadTextureDefs(int wadnum, const char *lumpname, FMultipatchTextureBuilder &build)
{
	int remapLump, lastLump;

	lastLump = 0;

	while ((remapLump = Wads.FindLump(lumpname, &lastLump)) != -1)
	{
		if (Wads.GetLumpFile(remapLump) == wadnum)
		{
			ParseTextureDef(remapLump, build);
		}
	}
}

void FTextureManager::ParseTextureDef(int lump, FMultipatchTextureBuilder &build)
{
	TArray<FTextureID> tlist;

	FScanner sc(lump);
	while (sc.GetString())
	{
		if (sc.Compare("remap")) // remap an existing texture
		{
			sc.MustGetString();

			// allow selection by type
			int mode;
			ETextureType type;
			if (sc.Compare("wall")) type=ETextureType::Wall, mode=FTextureManager::TEXMAN_Overridable;
			else if (sc.Compare("flat")) type=ETextureType::Flat, mode=FTextureManager::TEXMAN_Overridable;
			else if (sc.Compare("sprite")) type=ETextureType::Sprite, mode=0;
			else type = ETextureType::Any, mode = 0;

			if (type != ETextureType::Any) sc.MustGetString();

			sc.String[8]=0;

			tlist.Clear();
			int amount = ListTextures(sc.String, tlist);
			FName texname = sc.String;

			sc.MustGetString();
			int lumpnum = Wads.CheckNumForFullName(sc.String, true, ns_patches);
			if (lumpnum == -1) lumpnum = Wads.CheckNumForFullName(sc.String, true, ns_graphics);

			if (tlist.Size() == 0)
			{
				Printf("Attempting to remap non-existent texture %s to %s\n",
					texname.GetChars(), sc.String);
			}
			else if (lumpnum == -1)
			{
				Printf("Attempting to remap texture %s to non-existent lump %s\n",
					texname.GetChars(), sc.String);
			}
			else
			{
				for(unsigned int i = 0; i < tlist.Size(); i++)
				{
					FTexture * oldtex = Textures[tlist[i].GetIndex()].Texture;
					int sl;

					// only replace matching types. For sprites also replace any MiscPatches
					// based on the same lump. These can be created for icons.
					if (oldtex->UseType == type || type == ETextureType::Any ||
						(mode == TEXMAN_Overridable && oldtex->UseType == ETextureType::Override) ||
						(type == ETextureType::Sprite && oldtex->UseType == ETextureType::MiscPatch &&
						(sl=oldtex->GetSourceLump()) >= 0 && Wads.GetLumpNamespace(sl) == ns_sprites)
						)
					{
						FTexture * newtex = FTexture::CreateTexture ("", lumpnum, ETextureType::Any);
						if (newtex != NULL)
						{
							// Replace the entire texture and adjust the scaling and offset factors.
							newtex->bWorldPanning = true;
							newtex->SetScaledSize(oldtex->GetScaledWidth(), oldtex->GetScaledHeight());
							newtex->_LeftOffset[0] = int(oldtex->GetScaledLeftOffset(0) * newtex->Scale.X);
							newtex->_LeftOffset[1] = int(oldtex->GetScaledLeftOffset(1) * newtex->Scale.X);
							newtex->_TopOffset[0] = int(oldtex->GetScaledTopOffset(0) * newtex->Scale.Y);
							newtex->_TopOffset[1] = int(oldtex->GetScaledTopOffset(1) * newtex->Scale.Y);
							ReplaceTexture(tlist[i], newtex, true);
						}
					}
				}
			}
		}
		else if (sc.Compare("define")) // define a new "fake" texture
		{
			sc.GetString();
					
			FString base = ExtractFileBase(sc.String, false);
			if (!base.IsEmpty())
			{
				FString src = base.Left(8);

				int lumpnum = Wads.CheckNumForFullName(sc.String, true, ns_patches);
				if (lumpnum == -1) lumpnum = Wads.CheckNumForFullName(sc.String, true, ns_graphics);

				sc.GetString();
				bool is32bit = !!sc.Compare("force32bit");
				if (!is32bit) sc.UnGet();

				sc.MustGetNumber();
				int width = sc.Number;
				sc.MustGetNumber();
				int height = sc.Number;

				if (lumpnum>=0)
				{
					FTexture *newtex = FTexture::CreateTexture(src, lumpnum, ETextureType::Override);

					if (newtex != NULL)
					{
						// Replace the entire texture and adjust the scaling and offset factors.
						newtex->bWorldPanning = true;
						newtex->SetScaledSize(width, height);

						FTextureID oldtex = TexMan.CheckForTexture(src, ETextureType::MiscPatch);
						if (oldtex.isValid()) 
						{
							ReplaceTexture(oldtex, newtex, true);
							newtex->UseType = ETextureType::Override;
						}
						else AddTexture(newtex);
					}
				}
			}				
			//else Printf("Unable to define hires texture '%s'\n", tex->Name);
		}
		else if (sc.Compare("texture"))
		{
			build.ParseTexture(sc, ETextureType::Override);
		}
		else if (sc.Compare("sprite"))
		{
			build.ParseTexture(sc, ETextureType::Sprite);
		}
		else if (sc.Compare("walltexture"))
		{
			build.ParseTexture(sc, ETextureType::Wall);
		}
		else if (sc.Compare("flat"))
		{
			build.ParseTexture(sc, ETextureType::Flat);
		}
		else if (sc.Compare("graphic"))
		{
			build.ParseTexture(sc, ETextureType::MiscPatch);
		}
		else if (sc.Compare("#include"))
		{
			sc.MustGetString();

			// This is not using sc.Open because it can print a more useful error message when done here
			int includelump = Wads.CheckNumForFullName(sc.String, true);
			if (includelump == -1)
			{
				sc.ScriptError("Lump '%s' not found", sc.String);
			}
			else
			{
				ParseTextureDef(includelump, build);
			}
		}
		else
		{
			sc.ScriptError("Texture definition expected, found '%s'", sc.String);
		}
	}
}

//==========================================================================
//
// FTextureManager :: AddPatches
//
//==========================================================================

void FTextureManager::AddPatches (int lumpnum)
{
	auto file = Wads.ReopenLumpReader (lumpnum, true);
	uint32_t numpatches, i;
	char name[9];

	numpatches = file.ReadUInt32();
	name[8] = '\0';

	for (i = 0; i < numpatches; ++i)
	{
		file.Read (name, 8);

		if (CheckForTexture (name, ETextureType::WallPatch, 0) == -1)
		{
			CreateTexture (Wads.CheckNumForName (name, ns_patches), ETextureType::WallPatch);
		}
		StartScreen->Progress();
	}
}


//==========================================================================
//
// FTextureManager :: LoadTexturesX
//
// Initializes the texture list with the textures from the world map.
//
//==========================================================================

void FTextureManager::LoadTextureX(int wadnum, FMultipatchTextureBuilder &build)
{
	// Use the most recent PNAMES for this WAD.
	// Multiple PNAMES in a WAD will be ignored.
	int pnames = Wads.CheckNumForName("PNAMES", ns_global, wadnum, false);

	if (pnames < 0)
	{
		// should never happen except for zdoom.pk3
		return;
	}

	// Only add the patches if the PNAMES come from the current file
	// Otherwise they have already been processed.
	if (Wads.GetLumpFile(pnames) == wadnum) TexMan.AddPatches (pnames);

	int texlump1 = Wads.CheckNumForName ("TEXTURE1", ns_global, wadnum);
	int texlump2 = Wads.CheckNumForName ("TEXTURE2", ns_global, wadnum);
	build.AddTexturesLumps (texlump1, texlump2, pnames);
}

//==========================================================================
//
// FTextureManager :: AddTexturesForWad
//
//==========================================================================

void FTextureManager::AddTexturesForWad(int wadnum, FMultipatchTextureBuilder &build)
{
	int firsttexture = Textures.Size();
	int lumpcount = Wads.GetNumLumps();
	bool iwad = wadnum >= Wads.GetIwadNum() && wadnum <= Wads.GetMaxIwadNum();

	FirstTextureForFile.Push(firsttexture);

	// First step: Load sprites
	AddGroup(wadnum, ns_sprites, ETextureType::Sprite);

	// When loading a Zip, all graphics in the patches/ directory should be
	// added as well.
	AddGroup(wadnum, ns_patches, ETextureType::WallPatch);

	// Second step: TEXTUREx lumps
	LoadTextureX(wadnum, build);

	// Third step: Flats
	AddGroup(wadnum, ns_flats, ETextureType::Flat);

	// Fourth step: Textures (TX_)
	AddGroup(wadnum, ns_newtextures, ETextureType::Override);

	// Sixth step: Try to find any lump in the WAD that may be a texture and load as a TEX_MiscPatch
	int firsttx = Wads.GetFirstLump(wadnum);
	int lasttx = Wads.GetLastLump(wadnum);

	for (int i= firsttx; i <= lasttx; i++)
	{
		bool skin = false;
		FString Name;
		Wads.GetLumpName(Name, i);

		// Ignore anything not in the global namespace
		int ns = Wads.GetLumpNamespace(i);
		if (ns == ns_global)
		{
			// In Zips all graphics must be in a separate namespace.
			if (Wads.GetLumpFlags(i) & LUMPF_ZIPFILE) continue;

			// Ignore lumps with empty names.
			if (Wads.CheckLumpName(i, "")) continue;

			// Ignore anything belonging to a map
			if (Wads.CheckLumpName(i, "THINGS")) continue;
			if (Wads.CheckLumpName(i, "LINEDEFS")) continue;
			if (Wads.CheckLumpName(i, "SIDEDEFS")) continue;
			if (Wads.CheckLumpName(i, "VERTEXES")) continue;
			if (Wads.CheckLumpName(i, "SEGS")) continue;
			if (Wads.CheckLumpName(i, "SSECTORS")) continue;
			if (Wads.CheckLumpName(i, "NODES")) continue;
			if (Wads.CheckLumpName(i, "SECTORS")) continue;
			if (Wads.CheckLumpName(i, "REJECT")) continue;
			if (Wads.CheckLumpName(i, "BLOCKMAP")) continue;
			if (Wads.CheckLumpName(i, "BEHAVIOR")) continue;

			bool force = false;
			// Don't bother looking at this lump if something later overrides it.
			if (Wads.CheckNumForName(Name, ns_graphics) != i)
			{
				if (iwad)
				{ 
					// We need to make an exception for font characters of the SmallFont coming from the IWAD to be able to construct the original font.
					if (Name.IndexOf("STCFN") != 0 && Name.IndexOf("FONTA") != 0) continue;
					force = true;
				}
				else continue;
			}

			// skip this if it has already been added as a wall patch.
			if (!force && CheckForTexture(Name, ETextureType::WallPatch, 0).Exists()) continue;
		}
		else if (ns == ns_graphics)
		{
			if (Wads.CheckNumForName(Name, ns_graphics) != i)
			{
				if (iwad)
				{
					// We need to make an exception for font characters of the SmallFont coming from the IWAD to be able to construct the original font.
					if (Name.IndexOf("STCFN") != 0 && Name.IndexOf("FONTA") != 0) continue;
				}
				else continue;
			}
		}
		else if (ns >= ns_firstskin)
		{
			// Don't bother looking this lump if something later overrides it.
			if (Wads.CheckNumForName(Name, ns) != i) continue;
			skin = true;
		}
		else continue;

		// Try to create a texture from this lump and add it.
		// Unfortunately we have to look at everything that comes through here...
		FTexture *out = FTexture::CreateTexture(Name, i, skin ? ETextureType::SkinGraphic : ETextureType::MiscPatch);

		if (out != NULL) 
		{
			AddTexture (out);
		}
	}

	// Check for text based texture definitions
	LoadTextureDefs(wadnum, "TEXTURES", build);
	LoadTextureDefs(wadnum, "HIRESTEX", build);

	// Seventh step: Check for hires replacements.
	AddHiresTextures(wadnum);

	SortTexturesByType(firsttexture, Textures.Size());
}

//==========================================================================
//
// FTextureManager :: SortTexturesByType
// sorts newly added textures by UseType so that anything defined
// in TEXTURES and HIRESTEX gets in its proper place.
//
//==========================================================================

void FTextureManager::SortTexturesByType(int start, int end)
{
	TArray<FTexture *> newtextures;

	// First unlink all newly added textures from the hash chain
	for (int i = 0; i < HASH_SIZE; i++)
	{
		while (HashFirst[i] >= start && HashFirst[i] != HASH_END)
		{
			HashFirst[i] = Textures[HashFirst[i]].HashNext;
		}
	}
	newtextures.Resize(end-start);
	for(int i=start; i<end; i++)
	{
		newtextures[i-start] = Textures[i].Texture;
	}
	Textures.Resize(start);
	Translation.Resize(start);

	static ETextureType texturetypes[] = {
		ETextureType::Sprite, ETextureType::Null, ETextureType::FirstDefined, 
		ETextureType::WallPatch, ETextureType::Wall, ETextureType::Flat, 
		ETextureType::Override, ETextureType::MiscPatch, ETextureType::SkinGraphic
	};

	for(unsigned int i=0;i<countof(texturetypes);i++)
	{
		for(unsigned j = 0; j<newtextures.Size(); j++)
		{
			if (newtextures[j] != NULL && newtextures[j]->UseType == texturetypes[i])
			{
				AddTexture(newtextures[j]);
				newtextures[j] = NULL;
			}
		}
	}
	// This should never happen. All other UseTypes are only used outside
	for(unsigned j = 0; j<newtextures.Size(); j++)
	{
		if (newtextures[j] != NULL)
		{
			Printf("Texture %s has unknown type!\n", newtextures[j]->Name.GetChars());
			AddTexture(newtextures[j]);
		}
	}
}

//==========================================================================
//
// FTextureManager :: AddLocalizedVariants
//
//==========================================================================

void FTextureManager::AddLocalizedVariants()
{
	TArray<FolderEntry> content;
	Wads.GetLumpsInFolder("localized/textures/", content, false);
	for (auto &entry : content)
	{
		FString name = entry.name;
		auto tokens = name.Split(".", FString::TOK_SKIPEMPTY);
		if (tokens.Size() == 2)
		{
			auto ext = tokens[1];
			// Do not interpret common extensions for images as language IDs.
			if (ext.CompareNoCase("png") == 0 || ext.CompareNoCase("jpg") == 0 || ext.CompareNoCase("gfx") == 0 || ext.CompareNoCase("tga") == 0 || ext.CompareNoCase("lmp") == 0)
			{
				Printf("%s contains no language IDs and will be ignored\n", entry.name);
				continue;
			}
		}
		if (tokens.Size() >= 2)
		{
			FString base = ExtractFileBase(tokens[0]);
			FTextureID origTex = CheckForTexture(base, ETextureType::MiscPatch);
			if (origTex.isValid())
			{
				FTextureID tex = CheckForTexture(entry.name, ETextureType::MiscPatch);
				if (tex.isValid())
				{
					FTexture *otex = GetTexture(origTex);
					FTexture *ntex = GetTexture(tex);
					if (otex->GetDisplayWidth() != ntex->GetDisplayWidth() || otex->GetDisplayHeight() != ntex->GetDisplayHeight())
					{
						Printf("Localized texture %s must be the same size as the one it replaces\n", entry.name);
					}
					else
					{
						tokens[1].ToLower();
						auto langids = tokens[1].Split("-", FString::TOK_SKIPEMPTY);
						for (auto &lang : langids)
						{
							if (lang.Len() == 2 || lang.Len() == 3)
							{
								uint32_t langid = MAKE_ID(lang[0], lang[1], lang[2], 0);
								uint64_t comboid = (uint64_t(langid) << 32) | origTex.GetIndex();
								LocalizedTextures.Insert(comboid, tex.GetIndex());
								Textures[origTex.GetIndex()].HasLocalization = true;
							}
							else
							{
								Printf("Invalid language ID in texture %s\n", entry.name);
							}
						}
					}
				}
				else
				{
					Printf("%s is not a texture\n", entry.name);
				}
			}
			else
			{
				Printf("Unknown texture %s for localized variant %s\n", tokens[0].GetChars(), entry.name);
			}
		}
		else
		{
			Printf("%s contains no language IDs and will be ignored\n", entry.name);
		}

	}
}

//==========================================================================
//
// FTextureManager :: Init
//
//==========================================================================
FTexture *CreateShaderTexture(bool, bool);

void FTextureManager::Init()
{
	DeleteAll();
	GenerateGlobalBrightmapFromColormap();
	SpriteFrames.Clear();
	//if (BuildTileFiles.Size() == 0) CountBuildTiles ();
	FTexture::InitGrayMap();

	// Texture 0 is a dummy texture used to indicate "no texture"
	auto nulltex = new FImageTexture(nullptr);
	nulltex->SetUseType(ETextureType::Null);
	AddTexture (nulltex);
	// some special textures used in the game.
	AddTexture(CreateShaderTexture(false, false));
	AddTexture(CreateShaderTexture(false, true));
	AddTexture(CreateShaderTexture(true, false));
	AddTexture(CreateShaderTexture(true, true));

	int wadcnt = Wads.GetNumWads();

	FMultipatchTextureBuilder build(*this);

	for(int i = 0; i< wadcnt; i++)
	{
		AddTexturesForWad(i, build);
	}
	build.ResolveAllPatches();

	// Add one marker so that the last WAD is easier to handle and treat
	// Build tiles as a completely separate block.
	FirstTextureForFile.Push(Textures.Size());
	InitBuildTiles ();
	FirstTextureForFile.Push(Textures.Size());

	DefaultTexture = CheckForTexture ("-NOFLAT-", ETextureType::Override, 0);

	// The Hexen scripts use BLANK as a blank texture, even though it's really not.
	// I guess the Doom renderer must have clipped away the line at the bottom of
	// the texture so it wasn't visible. Change its use type to a blank null texture to really make it blank.
	if (gameinfo.gametype == GAME_Hexen)
	{
		FTextureID tex = CheckForTexture ("BLANK", ETextureType::Wall, false);
		if (tex.Exists())
		{
			auto texture = GetTexture(tex, false);
			texture->UseType = ETextureType::Null;
		}
	}

	// Hexen parallax skies use color 0 to indicate transparency on the front
	// layer, so we must not remap color 0 on these textures. Unfortunately,
	// the only way to identify these textures is to check the MAPINFO.
	for (unsigned int i = 0; i < wadlevelinfos.Size(); ++i)
	{
		if (wadlevelinfos[i].flags & LEVEL_DOUBLESKY)
		{
			FTextureID picnum = CheckForTexture (wadlevelinfos[i].SkyPic1, ETextureType::Wall, false);
			if (picnum.isValid())
			{
				Textures[picnum.GetIndex()].Texture->SetFrontSkyLayer ();
			}
		}
	}

	InitAnimated();
	InitAnimDefs();
	FixAnimations();
	InitSwitchList();
	InitPalettedVersions();
	AdjustSpriteOffsets();
	// Add auto materials to each texture after everything has been set up.
	// Textures array can be reallocated in process, so ranged for loop is not suitable.
	// There is no need to process discovered material textures here,
	// CheckForTexture() did this already.
	for (unsigned int i = 0, count = Textures.Size(); i < count; ++i)
	{
		Textures[i].Texture->AddAutoMaterials();
	}

	glLight = TexMan.CheckForTexture("glstuff/gllight.png", ETextureType::MiscPatch);
	glPart2 = TexMan.CheckForTexture("glstuff/glpart2.png", ETextureType::MiscPatch);
	glPart = TexMan.CheckForTexture("glstuff/glpart.png", ETextureType::MiscPatch);
	mirrorTexture = TexMan.CheckForTexture("glstuff/mirror.png", ETextureType::MiscPatch);
	AddLocalizedVariants();
}

//==========================================================================
//
// FTextureManager :: InitPalettedVersions
//
//==========================================================================

void FTextureManager::InitPalettedVersions()
{
	int lump, lastlump = 0;

	while ((lump = Wads.FindLump("PALVERS", &lastlump)) != -1)
	{
		FScanner sc(lump);

		while (sc.GetString())
		{
			FTextureID pic1 = CheckForTexture(sc.String, ETextureType::Any);
			if (!pic1.isValid())
			{
				sc.ScriptMessage("Unknown texture %s to replace", sc.String);
			}
			sc.MustGetString();
			FTextureID pic2 = CheckForTexture(sc.String, ETextureType::Any);
			if (!pic2.isValid())
			{
				sc.ScriptMessage("Unknown texture %s to use as replacement", sc.String);
			}
			if (pic1.isValid() && pic2.isValid())
			{
				FTexture *owner = GetTexture(pic1);
				FTexture *owned = GetTexture(pic2);

				if (owner && owned) owner->PalVersion = owned;
			}
		}
	}
}

//==========================================================================
//
// FTextureManager :: PalCheck
//
//==========================================================================

int FTextureManager::PalCheck(int tex)
{
	// In any true color mode this shouldn't do anything.
	if (vid_nopalsubstitutions || V_IsTrueColor()) return tex;
	auto ftex = Textures[tex].Texture;
	if (ftex != nullptr && ftex->PalVersion != nullptr) return ftex->PalVersion->id.GetIndex();
	return tex;
}

//==========================================================================
//
// FTextureManager :: PalCheck
//
//==========================================================================
EXTERN_CVAR(String, language)

int FTextureManager::ResolveLocalizedTexture(int tex)
{
	size_t langlen = strlen(language);
	int lang = (langlen < 2 || langlen > 3) ?
		MAKE_ID('e', 'n', 'u', '\0') :
		MAKE_ID(language[0], language[1], language[2], '\0');

	uint64_t index = (uint64_t(lang) << 32) + tex;
	if (auto pTex = LocalizedTextures.CheckKey(index)) return *pTex;
	index = (uint64_t(lang & MAKE_ID(255, 255, 0, 0)) << 32) + tex;
	if (auto pTex = LocalizedTextures.CheckKey(index)) return *pTex;

	return tex;
}

//===========================================================================
//
// R_GuesstimateNumTextures
//
// Returns an estimate of the number of textures R_InitData will have to
// process. Used by D_DoomMain() when it calls ST_Init().
//
//===========================================================================

int FTextureManager::GuesstimateNumTextures ()
{
	int numtex = 0;
	
	for(int i = Wads.GetNumLumps()-1; i>=0; i--)
	{
		int space = Wads.GetLumpNamespace(i);
		switch(space)
		{
		case ns_flats:
		case ns_sprites:
		case ns_newtextures:
		case ns_hires:
		case ns_patches:
		case ns_graphics:
			numtex++;
			break;

		default:
			if (Wads.GetLumpFlags(i) & LUMPF_MAYBEFLAT) numtex++;

			break;
		}
	}

	//numtex += CountBuildTiles (); // this cannot be done with a lot of overhead so just leave it out.
	numtex += CountTexturesX ();
	return numtex;
}

//===========================================================================
//
// R_CountTexturesX
//
// See R_InitTextures() for the logic in deciding what lumps to check.
//
//===========================================================================

int FTextureManager::CountTexturesX ()
{
	int count = 0;
	int wadcount = Wads.GetNumWads();
	for (int wadnum = 0; wadnum < wadcount; wadnum++)
	{
		// Use the most recent PNAMES for this WAD.
		// Multiple PNAMES in a WAD will be ignored.
		int pnames = Wads.CheckNumForName("PNAMES", ns_global, wadnum, false);

		// should never happen except for zdoom.pk3
		if (pnames < 0) continue;

		// Only count the patches if the PNAMES come from the current file
		// Otherwise they have already been counted.
		if (Wads.GetLumpFile(pnames) == wadnum) 
		{
			count += CountLumpTextures (pnames);
		}

		int texlump1 = Wads.CheckNumForName ("TEXTURE1", ns_global, wadnum);
		int texlump2 = Wads.CheckNumForName ("TEXTURE2", ns_global, wadnum);

		count += CountLumpTextures (texlump1) - 1;
		count += CountLumpTextures (texlump2) - 1;
	}
	return count;
}

//===========================================================================
//
// R_CountLumpTextures
//
// Returns the number of patches in a PNAMES/TEXTURE1/TEXTURE2 lump.
//
//===========================================================================

int FTextureManager::CountLumpTextures (int lumpnum)
{
	if (lumpnum >= 0)
	{
		auto file = Wads.OpenLumpReader (lumpnum); 
		uint32_t numtex = file.ReadUInt32();;

		return int(numtex) >= 0 ? numtex : 0;
	}
	return 0;
}

//-----------------------------------------------------------------------------
//
// Adjust sprite offsets for GL rendering (IWAD resources only)
//
//-----------------------------------------------------------------------------

void FTextureManager::AdjustSpriteOffsets()
{
	int lump, lastlump = 0;
	int sprid;
	TMap<int, bool> donotprocess;

	int numtex = Wads.GetNumLumps();

	for (int i = 0; i < numtex; i++)
	{
		if (Wads.GetLumpFile(i) > Wads.GetMaxIwadNum()) break; // we are past the IWAD
		if (Wads.GetLumpNamespace(i) == ns_sprites && Wads.GetLumpFile(i) >= Wads.GetIwadNum() && Wads.GetLumpFile(i) <= Wads.GetMaxIwadNum())
		{
			char str[9];
			Wads.GetLumpName(str, i);
			str[8] = 0;
			FTextureID texid = TexMan.CheckForTexture(str, ETextureType::Sprite, 0);
			if (texid.isValid() && Wads.GetLumpFile(GetTexture(texid)->SourceLump) > Wads.GetMaxIwadNum())
			{
				// This texture has been replaced by some PWAD.
				memcpy(&sprid, str, 4);
				donotprocess[sprid] = true;
			}
		}
	}

	while ((lump = Wads.FindLump("SPROFS", &lastlump, false)) != -1)
	{
		FScanner sc;
		sc.OpenLumpNum(lump);
		sc.SetCMode(true);
		int ofslumpno = Wads.GetLumpFile(lump);
		while (sc.GetString())
		{
			int x, y;
			bool iwadonly = false;
			bool forced = false;
			FTextureID texno = TexMan.CheckForTexture(sc.String, ETextureType::Sprite);
			sc.MustGetStringName(",");
			sc.MustGetNumber();
			x = sc.Number;
			sc.MustGetStringName(",");
			sc.MustGetNumber();
			y = sc.Number;
			if (sc.CheckString(","))
			{
				sc.MustGetString();
				if (sc.Compare("iwad")) iwadonly = true;
				if (sc.Compare("iwadforced")) forced = iwadonly = true;
			}
			if (texno.isValid())
			{
				FTexture * tex = GetTexture(texno);

				int lumpnum = tex->GetSourceLump();
				// We only want to change texture offsets for sprites in the IWAD or the file this lump originated from.
				if (lumpnum >= 0 && lumpnum < Wads.GetNumLumps())
				{
					int wadno = Wads.GetLumpFile(lumpnum);
					if ((iwadonly && wadno >= Wads.GetIwadNum() && wadno <= Wads.GetMaxIwadNum()) || (!iwadonly && wadno == ofslumpno))
					{
						if (wadno >= Wads.GetIwadNum() && wadno <= Wads.GetMaxIwadNum() && !forced && iwadonly)
						{
							memcpy(&sprid, &tex->Name[0], 4);
							if (donotprocess.CheckKey(sprid)) continue;	// do not alter sprites that only get partially replaced.
						}
						tex->_LeftOffset[1] = x;
						tex->_TopOffset[1] = y;
					}
				}
			}
		}
	}
}

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

void FTextureManager::SpriteAdjustChanged()
{
	for (auto &texi : Textures)
	{
		auto tex = texi.Texture;
		if (tex->GetLeftOffset(0) != tex->GetLeftOffset(1) || tex->GetTopOffset(0) != tex->GetTopOffset(1))
		{
			tex->SetSpriteAdjust();
		}
	}
}

//===========================================================================
// 
// Examines the colormap to see if some of the colors have to be
// considered fullbright all the time.
//
//===========================================================================

void FTextureManager::GenerateGlobalBrightmapFromColormap()
{
	Wads.CheckNumForFullName("textures/tgapal", false, 0, true);
	HasGlobalBrightmap = false;
	int lump = Wads.CheckNumForName("COLORMAP");
	if (lump == -1) lump = Wads.CheckNumForName("COLORMAP", ns_colormaps);
	if (lump == -1) return;
	FMemLump cmap = Wads.ReadLump(lump);
	uint8_t palbuffer[768];
	ReadPalette(Wads.CheckNumForName("PLAYPAL"), palbuffer);

	const unsigned char *cmapdata = (const unsigned char *)cmap.GetMem();
	const uint8_t *paldata = palbuffer;

	const int black = 0;
	const int white = ColorMatcher.Pick(255, 255, 255);


	GlobalBrightmap.MakeIdentity();
	memset(GlobalBrightmap.Remap, white, 256);
	for (int i = 0; i<256; i++) GlobalBrightmap.Palette[i] = PalEntry(255, 255, 255, 255);
	for (int j = 0; j<32; j++)
	{
		for (int i = 0; i<256; i++)
		{
			// the palette comparison should be for ==0 but that gives false positives with Heretic
			// and Hexen.
			if (cmapdata[i + j * 256] != i || (paldata[3 * i]<10 && paldata[3 * i + 1]<10 && paldata[3 * i + 2]<10))
			{
				GlobalBrightmap.Remap[i] = black;
				GlobalBrightmap.Palette[i] = PalEntry(255, 0, 0, 0);
			}
		}
	}
	for (int i = 0; i<256; i++)
	{
		HasGlobalBrightmap |= GlobalBrightmap.Remap[i] == white;
		if (GlobalBrightmap.Remap[i] == white) DPrintf(DMSG_NOTIFY, "Marked color %d as fullbright\n", i);
	}
}


//==========================================================================
//
//
//
//==========================================================================

DEFINE_ACTION_FUNCTION(_TexMan, GetName)
{
	PARAM_PROLOGUE;
	PARAM_INT(texid);
	auto tex = TexMan.ByIndex(texid);
	FString retval;

	if (tex != nullptr)
	{
		if (tex->GetName().IsNotEmpty()) retval = tex->GetName();
		else
		{
			// Textures for full path names do not have their own name, they merely link to the source lump.
			auto lump = tex->GetSourceLump();
			if (Wads.GetLinkedTexture(lump) == tex)
				retval = Wads.GetLumpFullName(lump);
		}
	}
	ACTION_RETURN_STRING(retval);
}

//==========================================================================
//
//
//
//==========================================================================

static int GetTextureSize(int texid, int *py)
{
	auto tex = TexMan.ByIndex(texid);
	int x, y;
	if (tex != nullptr)
	{
		x = tex->GetDisplayWidth();
		y = tex->GetDisplayHeight();
	}
	else x = y = -1;
	if (py) *py = y;
	return x;
}

DEFINE_ACTION_FUNCTION_NATIVE(_TexMan, GetSize, GetTextureSize)
{
	PARAM_PROLOGUE;
	PARAM_INT(texid);
	int x, y;
	x = GetTextureSize(texid, &y);
	if (numret > 0) ret[0].SetInt(x);
	if (numret > 1) ret[1].SetInt(y);
	return MIN(numret, 2);
}

//==========================================================================
//
//
//
//==========================================================================
static void GetScaledSize(int texid, DVector2 *pvec)
{
	auto tex = TexMan.ByIndex(texid);
	double x, y;
	if (tex != nullptr)
	{
		x = tex->GetDisplayWidthDouble();
		y = tex->GetDisplayHeightDouble();
	}
	else x = y = -1;
	if (pvec)
	{
		pvec->X = x;
		pvec->Y = y;
	}
}

DEFINE_ACTION_FUNCTION_NATIVE(_TexMan, GetScaledSize, GetScaledSize)
{
	PARAM_PROLOGUE;
	PARAM_INT(texid);
	DVector2 vec;
	GetScaledSize(texid, &vec);
	ACTION_RETURN_VEC2(vec);
}

//==========================================================================
//
//
//
//==========================================================================
static void GetScaledOffset(int texid, DVector2 *pvec)
{
	auto tex = TexMan.ByIndex(texid);
	double x, y;
	if (tex != nullptr)
	{
		x = tex->GetDisplayLeftOffsetDouble();
		y = tex->GetDisplayTopOffsetDouble();
	}
	else x = y = -1;
	if (pvec)
	{
		pvec->X = x;
		pvec->Y = y;
	}
}

DEFINE_ACTION_FUNCTION_NATIVE(_TexMan, GetScaledOffset, GetScaledOffset)
{
	PARAM_PROLOGUE;
	PARAM_INT(texid);
	DVector2 vec;
	GetScaledOffset(texid, &vec);
	ACTION_RETURN_VEC2(vec);
}

//==========================================================================
//
//
//
//==========================================================================

static int CheckRealHeight(int texid)
{
	auto tex = TexMan.ByIndex(texid);
	if (tex != nullptr) return tex->CheckRealHeight();
	else return -1;
}

DEFINE_ACTION_FUNCTION_NATIVE(_TexMan, CheckRealHeight, CheckRealHeight)
{
	PARAM_PROLOGUE;
	PARAM_INT(texid);
	ACTION_RETURN_INT(CheckRealHeight(texid));
}

//==========================================================================
//
// FTextureID::operator+
// Does not return invalid texture IDs
//
//==========================================================================

FTextureID FTextureID::operator +(int offset) throw()
{
	if (!isValid()) return *this;
	if (texnum + offset >= TexMan.NumTextures()) return FTextureID(-1);
	return FTextureID(texnum + offset);
}
