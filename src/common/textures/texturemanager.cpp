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

#include "filesystem.h"
#include "printf.h"
#include "c_cvars.h"
#include "templates.h"
#include "gstrings.h"
#include "textures.h"
#include "texturemanager.h"
#include "c_dispatch.h"
#include "sc_man.h"
#include "image.h"
#include "vectors.h"
#include "animtexture.h"
#include "formats/multipatchtexture.h"

FTextureManager TexMan;


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
		sintable[i] = short(sin(i*(pi::pi() / 1024)) * 16384);
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

	BuildTileData.Clear();
	tmanips.Clear();
}

//==========================================================================
//
// Flushes all hardware dependent data.
// This must not, under any circumstances, delete the wipe textures, because
// all CCMDs triggering a flush can be executed while a wipe is in progress
//
// This now also deletes the software textures because the software
// renderer can also use the texture scalers and that is the
// main reason to call this outside of the destruction code.
//
//==========================================================================

void FTextureManager::FlushAll()
{
	for (int i = TexMan.NumTextures() - 1; i >= 0; i--)
	{
		for (int j = 0; j < 2; j++)
		{
			Textures[i].Texture->CleanHardwareData();
			delete Textures[i].Texture->GetSoftwareTexture();
			calcShouldUpscale(Textures[i].Texture);
			Textures[i].Texture->SetSoftwareTexture(nullptr);
		}
	}
}

//==========================================================================
//
// Examines the lump contents to decide what type of texture to create,
// and creates the texture.
//
//==========================================================================

static FTexture* CreateTextureFromLump(int lumpnum, bool allowflats = false)
{
	if (lumpnum == -1) return nullptr;

	auto image = FImageSource::GetImage(lumpnum, allowflats);
	if (image != nullptr)
	{
		return new FImageTexture(image);
	}
	return nullptr;
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
		auto tex = Textures[i].Texture;


		if (stricmp (tex->GetName(), name) == 0 )
		{
			// If we look for short names, we must ignore any long name texture.
			if ((flags & TEXMAN_ShortNameOnly) && tex->isFullNameTexture())
			{
				continue;
			}
			auto texUseType = tex->GetUseType();
			// The name matches, so check the texture type
			if (usetype == ETextureType::Any)
			{
				// All NULL textures should actually return 0
				if (texUseType == ETextureType::FirstDefined && !(flags & TEXMAN_ReturnFirst)) return 0;
				if (texUseType == ETextureType::SkinGraphic && !(flags & TEXMAN_AllowSkins)) return 0;
				return FTextureID(texUseType==ETextureType::Null ? 0 : i);
			}
			else if ((flags & TEXMAN_Overridable) && texUseType == ETextureType::Override)
			{
				return FTextureID(i);
			}
			else if (texUseType == usetype)
			{
				return FTextureID(i);
			}
			else if (texUseType == ETextureType::FirstDefined && usetype == ETextureType::Wall)
			{
				if (!(flags & TEXMAN_ReturnFirst)) return FTextureID(0);
				else return FTextureID(i);
			}
			else if (texUseType == ETextureType::Null && usetype == ETextureType::Wall)
			{
				// We found a NULL texture on a wall -> return 0
				return FTextureID(0);
			}
			else
			{
				if (firsttype == ETextureType::Null ||
					(firsttype == ETextureType::MiscPatch &&
					 texUseType != firsttype &&
					 texUseType != ETextureType::Null)
				   )
				{
					firstfound = i;
					firsttype = texUseType;
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
			FGameTexture *const NO_TEXTURE = (FGameTexture*)-1;
			int lump = fileSystem.CheckNumForFullName(name);
			if (lump >= 0)
			{
				FGameTexture *tex = fileSystem.GetLinkedTexture(lump);
				if (tex == NO_TEXTURE) return FTextureID(-1);
				if (tex != NULL) return tex->GetID();
				if (flags & TEXMAN_DontCreate) return FTextureID(-1);	// we only want to check, there's no need to create a texture if we don't have one yet.
				tex = MakeGameTexture(CreateTextureFromLump(lump), nullptr, ETextureType::Override);
				if (tex != NULL)
				{
					tex->AddAutoMaterials();
					fileSystem.SetLinkedTexture(lump, tex);
					return AddGameTexture(tex);
				}
				else
				{
					// mark this lump as having no valid texture so that we don't have to retry creating one later.
					fileSystem.SetLinkedTexture(lump, NO_TEXTURE);
				}
			}
		}
	}

	return FTextureID(-1);
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
		auto tex = Textures[i].Texture;

		if (stricmp (tex->GetName(), name) == 0)
		{
			auto texUseType = tex->GetUseType();
			// NULL textures must be ignored.
			if (texUseType!=ETextureType::Null) 
			{
				unsigned int j = list.Size();
				if (!listall)
				{
					for (j = 0; j < list.Size(); j++)
					{
						// Check for overriding definitions from newer WADs
						if (Textures[list[j].GetIndex()].Texture->GetUseType() == texUseType) break;
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

FGameTexture *FTextureManager::FindGameTexture(const char *texname, ETextureType usetype, BITFIELD flags)
{
	FTextureID texnum = CheckForTexture (texname, usetype, flags);
	return GetGameTexture(texnum.GetIndex());
}

//==========================================================================
//
// FTextureManager :: OkForLocalization
//
//==========================================================================

bool FTextureManager::OkForLocalization(FTextureID texnum, const char *substitute, int locmode)
{
	uint32_t langtable = 0;
	if (*substitute == '$') substitute = GStrings.GetString(substitute+1, &langtable);
	else return true;	// String literals from the source data should never override graphics from the same definition.
	if (substitute == nullptr) return true;	// The text does not exist.
	
	// Modes 2, 3 and 4 must not replace localized textures.
	int localizedTex = ResolveLocalizedTexture(texnum.GetIndex());
	if (localizedTex != texnum.GetIndex()) return true;	// Do not substitute a localized variant of the graphics patch.
	
	// For mode 4 we are done now.
	if (locmode == 4) return false;
	
	// Mode 2 and 3 must reject any text replacement from the default language tables.
	if ((langtable & MAKE_ID(255,0,0,0)) == MAKE_ID('*', 0, 0, 0)) return true;	// Do not substitute if the string comes from the default table.
	if (locmode == 2) return false;
	
	// Mode 3 must also reject substitutions for non-IWAD content.
	int file = fileSystem.GetFileContainer(Textures[texnum.GetIndex()].Texture->GetSourceLump());
	if (file > fileSystem.GetMaxIwadNum()) return true;

	return false;
}

//==========================================================================
//
// FTextureManager :: AddTexture
//
//==========================================================================

FTextureID FTextureManager::AddGameTexture (FGameTexture *texture, bool addtohash)
{
	int bucket;
	int hash;

	if (texture == NULL) return FTextureID(-1);

	if (texture->GetTexture())
	{
		// Later textures take precedence over earlier ones
		calcShouldUpscale(texture);	// calculate this once at insertion
	}

	// Textures without name can't be looked for
	if (addtohash && texture->GetName().IsNotEmpty())
	{
		bucket = int(MakeKey (texture->GetName()) % HASH_SIZE);
		hash = HashFirst[bucket];
	}
	else
	{
		bucket = -1;
		hash = -1;
	}

	TextureHash hasher = { texture, -1, -1, -1, hash };
	int trans = Textures.Push (hasher);
	Translation.Push (trans);
	if (bucket >= 0) HashFirst[bucket] = trans;
	auto id = FTextureID(trans);
	texture->SetID(id);
	return id;
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
		fileSystem.GetFileShortName(str, lumpnum);
		auto out = MakeGameTexture(CreateTextureFromLump(lumpnum, usetype == ETextureType::Flat), str, usetype);

		if (out != NULL)
		{
			if (usetype == ETextureType::Flat)
			{
				int w = out->GetTexelWidth();
				int h = out->GetTexelHeight();

				// Auto-scale flats with dimensions 128x128 and 256x256.
				// In hindsight, a bad idea, but RandomLag made it sound better than it really is.
				// Now we're stuck with this stupid behaviour.
				if (w == 128 && h == 128)
				{
					out->SetScale(2, 2);
					out->SetWorldPanning(true);
				}
				else if (w == 256 && h == 256)
				{
					out->SetScale(4, 4);
					out->SetWorldPanning(true);
				}
			}

			return AddGameTexture(out);
		}
		else
		{
			Printf (TEXTCOLOR_ORANGE "Invalid data encountered for texture %s\n", fileSystem.GetFileFullPath(lumpnum).GetChars());
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

void FTextureManager::ReplaceTexture (FTextureID picnum, FGameTexture *newtexture, bool free)
{
	int index = picnum.GetIndex();
	if (unsigned(index) >= Textures.Size())
		return;

	auto oldtexture = Textures[index].Texture;

	newtexture->SetName(oldtexture->GetName());
	newtexture->SetUseType(oldtexture->GetUseType());
	Textures[index].Texture = newtexture;
	newtexture->SetID(oldtexture->GetID());
	oldtexture->SetName("");
	AddGameTexture(oldtexture);
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

	auto texture1 = Textures[index1].Texture;
	auto texture2 = Textures[index2].Texture;

	// both textures must be the same type.
	if (texture1 == NULL || texture2 == NULL || texture1->GetUseType() != texture2->GetUseType())
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
	int firsttx = fileSystem.GetFirstEntry(wadnum);
	int lasttx = fileSystem.GetLastEntry(wadnum);
	FString Name;

	// Go from first to last so that ANIMDEFS work as expected. However,
	// to avoid duplicates (and to keep earlier entries from overriding
	// later ones), the texture is only inserted if it is the one returned
	// by doing a check by name in the list of wads.

	for (; firsttx <= lasttx; ++firsttx)
	{
		if (fileSystem.GetFileNamespace(firsttx) == ns)
		{
			fileSystem.GetFileShortName (Name, firsttx);

			if (fileSystem.CheckNumForName (Name, ns) == firsttx)
			{
				CreateTexture (firsttx, usetype);
			}
			progressFunc();
		}
		else if (ns == ns_flats && fileSystem.GetFileFlags(firsttx) & LUMPF_MAYBEFLAT)
		{
			if (fileSystem.CheckNumForName (Name, ns) < firsttx)
			{
				CreateTexture (firsttx, usetype);
			}
			progressFunc();
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
	int firsttx = fileSystem.GetFirstEntry(wadnum);
	int lasttx = fileSystem.GetLastEntry(wadnum);

	FString Name;
	TArray<FTextureID> tlist;

	if (firsttx == -1 || lasttx == -1)
	{
		return;
	}

	for (;firsttx <= lasttx; ++firsttx)
	{
		if (fileSystem.GetFileNamespace(firsttx) == ns_hires)
		{
			fileSystem.GetFileShortName (Name, firsttx);

			if (fileSystem.CheckNumForName (Name, ns_hires) == firsttx)
			{
				tlist.Clear();
				int amount = ListTextures(Name, tlist);
				if (amount == 0)
				{
					// A texture with this name does not yet exist
					auto newtex = MakeGameTexture(CreateTextureFromLump(firsttx), Name, ETextureType::Override);
					if (newtex != NULL)
					{
						AddGameTexture(newtex);
					}
				}
				else
				{
					for(unsigned int i = 0; i < tlist.Size(); i++)
					{
						FTexture * newtex = CreateTextureFromLump(firsttx);
						if (newtex != NULL)
						{
							auto oldtex = Textures[tlist[i].GetIndex()].Texture;

							// Replace the entire texture and adjust the scaling and offset factors.
							auto gtex = MakeGameTexture(newtex, nullptr, ETextureType::Override);
							gtex->SetWorldPanning(true);
							gtex->SetDisplaySize(oldtex->GetDisplayWidth(), oldtex->GetDisplayHeight());
							double xscale1 = oldtex->GetTexelLeftOffset(0) * gtex->GetScaleX() / oldtex->GetScaleX();
							double xscale2 = oldtex->GetTexelLeftOffset(1) * gtex->GetScaleX() / oldtex->GetScaleX();
							double yscale1 = oldtex->GetTexelTopOffset(0) * gtex->GetScaleY() / oldtex->GetScaleY();
							double yscale2 = oldtex->GetTexelTopOffset(1) * gtex->GetScaleY() / oldtex->GetScaleY();
							gtex->SetOffsets(0, xs_RoundToInt(xscale1), xs_RoundToInt(yscale1));
							gtex->SetOffsets(1, xs_RoundToInt(xscale2), xs_RoundToInt(yscale2));
							ReplaceTexture(tlist[i], gtex, true);
						}
					}
				}
				progressFunc();
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

	while ((remapLump = fileSystem.FindLump(lumpname, &lastLump)) != -1)
	{
		if (fileSystem.GetFileContainer(remapLump) == wadnum)
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
			int lumpnum = fileSystem.CheckNumForFullName(sc.String, true, ns_patches);
			if (lumpnum == -1) lumpnum = fileSystem.CheckNumForFullName(sc.String, true, ns_graphics);

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
					auto oldtex = Textures[tlist[i].GetIndex()].Texture;
					int sl;

					// only replace matching types. For sprites also replace any MiscPatches
					// based on the same lump. These can be created for icons.
					if (oldtex->GetUseType() == type || type == ETextureType::Any ||
						(mode == TEXMAN_Overridable && oldtex->GetUseType() == ETextureType::Override) ||
						(type == ETextureType::Sprite && oldtex->GetUseType() == ETextureType::MiscPatch &&
						(sl=oldtex->GetSourceLump()) >= 0 && fileSystem.GetFileNamespace(sl) == ns_sprites)
						)
					{
						FTexture * newtex = CreateTextureFromLump(lumpnum);
						if (newtex != NULL)
						{
							// Replace the entire texture and adjust the scaling and offset factors.
							auto gtex = MakeGameTexture(newtex, nullptr, ETextureType::Override);
							gtex->SetWorldPanning(true);
							gtex->SetDisplaySize(oldtex->GetDisplayWidth(), oldtex->GetDisplayHeight());
							double xscale1 = oldtex->GetTexelLeftOffset(0) * gtex->GetScaleX() / oldtex->GetScaleX();
							double xscale2 = oldtex->GetTexelLeftOffset(1) * gtex->GetScaleX() / oldtex->GetScaleX();
							double yscale1 = oldtex->GetTexelTopOffset(0) * gtex->GetScaleY() / oldtex->GetScaleY();
							double yscale2 = oldtex->GetTexelTopOffset(1) * gtex->GetScaleY() / oldtex->GetScaleY();
							gtex->SetOffsets(0, xs_RoundToInt(xscale1), xs_RoundToInt(yscale1));
							gtex->SetOffsets(1, xs_RoundToInt(xscale2), xs_RoundToInt(yscale2));
							ReplaceTexture(tlist[i], gtex, true);
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

				int lumpnum = fileSystem.CheckNumForFullName(sc.String, true, ns_patches);
				if (lumpnum == -1) lumpnum = fileSystem.CheckNumForFullName(sc.String, true, ns_graphics);

				sc.GetString();
				bool is32bit = !!sc.Compare("force32bit");
				if (!is32bit) sc.UnGet();

				sc.MustGetNumber();
				int width = sc.Number;
				sc.MustGetNumber();
				int height = sc.Number;

				if (lumpnum>=0)
				{
					auto newtex = MakeGameTexture(CreateTextureFromLump(lumpnum), src, ETextureType::Override);

					if (newtex != NULL)
					{
						// Replace the entire texture and adjust the scaling and offset factors.
						newtex->SetWorldPanning(true);
						newtex->SetDisplaySize((float)width, (float)height);

						FTextureID oldtex = TexMan.CheckForTexture(src, ETextureType::MiscPatch);
						if (oldtex.isValid()) 
						{
							ReplaceTexture(oldtex, newtex, true);
							newtex->SetUseType(ETextureType::Override);
						}
						else AddGameTexture(newtex);
					}
				}
			}				
			//else Printf("Unable to define hires texture '%s'\n", tex->Name);
		}
		else if (sc.Compare("texture"))
		{
			build.ParseTexture(sc, ETextureType::Override, lump);
		}
		else if (sc.Compare("sprite"))
		{
			build.ParseTexture(sc, ETextureType::Sprite, lump);
		}
		else if (sc.Compare("walltexture"))
		{
			build.ParseTexture(sc, ETextureType::Wall, lump);
		}
		else if (sc.Compare("flat"))
		{
			build.ParseTexture(sc, ETextureType::Flat, lump);
		}
		else if (sc.Compare("graphic"))
		{
			build.ParseTexture(sc, ETextureType::MiscPatch, lump);
		}
		else if (sc.Compare("#include"))
		{
			sc.MustGetString();

			// This is not using sc.Open because it can print a more useful error message when done here
			int includelump = fileSystem.CheckNumForFullName(sc.String, true);
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
	auto file = fileSystem.ReopenFileReader (lumpnum, true);
	uint32_t numpatches, i;
	char name[9];

	numpatches = file.ReadUInt32();
	name[8] = '\0';

	for (i = 0; i < numpatches; ++i)
	{
		file.Read (name, 8);

		if (CheckForTexture (name, ETextureType::WallPatch, 0) == -1)
		{
			CreateTexture (fileSystem.CheckNumForName (name, ns_patches), ETextureType::WallPatch);
		}
		progressFunc();
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
	int pnames = fileSystem.CheckNumForName("PNAMES", ns_global, wadnum, false);

	if (pnames < 0)
	{
		// should never happen except for zdoom.pk3
		return;
	}

	// Only add the patches if the PNAMES come from the current file
	// Otherwise they have already been processed.
	if (fileSystem.GetFileContainer(pnames) == wadnum) TexMan.AddPatches (pnames);

	int texlump1 = fileSystem.CheckNumForName ("TEXTURE1", ns_global, wadnum);
	int texlump2 = fileSystem.CheckNumForName ("TEXTURE2", ns_global, wadnum);
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
	int lumpcount = fileSystem.GetNumEntries();
	bool iwad = wadnum >= fileSystem.GetIwadNum() && wadnum <= fileSystem.GetMaxIwadNum();

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
	int firsttx = fileSystem.GetFirstEntry(wadnum);
	int lasttx = fileSystem.GetLastEntry(wadnum);

	for (int i= firsttx; i <= lasttx; i++)
	{
		bool skin = false;
		FString Name;
		fileSystem.GetFileShortName(Name, i);

		// Ignore anything not in the global namespace
		int ns = fileSystem.GetFileNamespace(i);
		if (ns == ns_global)
		{
			// In Zips all graphics must be in a separate namespace.
			if (fileSystem.GetFileFlags(i) & LUMPF_FULLPATH) continue;

			// Ignore lumps with empty names.
			if (fileSystem.CheckFileName(i, "")) continue;

			// Ignore anything belonging to a map
			if (fileSystem.CheckFileName(i, "THINGS")) continue;
			if (fileSystem.CheckFileName(i, "LINEDEFS")) continue;
			if (fileSystem.CheckFileName(i, "SIDEDEFS")) continue;
			if (fileSystem.CheckFileName(i, "VERTEXES")) continue;
			if (fileSystem.CheckFileName(i, "SEGS")) continue;
			if (fileSystem.CheckFileName(i, "SSECTORS")) continue;
			if (fileSystem.CheckFileName(i, "NODES")) continue;
			if (fileSystem.CheckFileName(i, "SECTORS")) continue;
			if (fileSystem.CheckFileName(i, "REJECT")) continue;
			if (fileSystem.CheckFileName(i, "BLOCKMAP")) continue;
			if (fileSystem.CheckFileName(i, "BEHAVIOR")) continue;

			bool force = false;
			// Don't bother looking at this lump if something later overrides it.
			if (fileSystem.CheckNumForName(Name, ns_graphics) != i)
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
			if (fileSystem.CheckNumForName(Name, ns_graphics) != i)
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
			if (fileSystem.CheckNumForName(Name, ns) != i) continue;
			skin = true;
		}
		else continue;

		// Try to create a texture from this lump and add it.
		// Unfortunately we have to look at everything that comes through here...
		auto out = MakeGameTexture(CreateTextureFromLump(i), Name, skin ? ETextureType::SkinGraphic : ETextureType::MiscPatch);

		if (out != NULL) 
		{
			AddGameTexture (out);
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
	TArray<FGameTexture *> newtextures;

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
			if (newtextures[j] != NULL && newtextures[j]->GetUseType() == texturetypes[i])
			{
				AddGameTexture(newtextures[j]);
				newtextures[j] = NULL;
			}
		}
	}
	// This should never happen. All other UseTypes are only used outside
	for(unsigned j = 0; j<newtextures.Size(); j++)
	{
		if (newtextures[j] != NULL)
		{
			Printf("Texture %s has unknown type!\n", newtextures[j]->GetName().GetChars());
			AddGameTexture(newtextures[j]);
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
	fileSystem.GetFilesInFolder("localized/textures/", content, false);
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
					auto otex = GetGameTexture(origTex);
					auto ntex = GetGameTexture(tex);
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
FGameTexture *CreateShaderTexture(bool, bool);
void InitBuildTiles();
FImageSource* CreateEmptyTexture();

void FTextureManager::Init(void (*progressFunc_)(), void (*checkForHacks)(BuildInfo&))
{
	progressFunc = progressFunc_;
	DeleteAll();
	//if (BuildTileFiles.Size() == 0) CountBuildTiles ();

	auto nulltex = MakeGameTexture(new FImageTexture(CreateEmptyTexture()), nullptr, ETextureType::Null);
	AddGameTexture(nulltex);

	// This is for binding to unused texture units, because accessing an unbound texture unit is undefined. It's a one pixel empty texture.
	auto emptytex = MakeGameTexture(new FImageTexture(CreateEmptyTexture()), nullptr, ETextureType::Override);
	emptytex->SetSize(1, 1);
	AddGameTexture(emptytex);	
	// some special textures used in the game.
	AddGameTexture(CreateShaderTexture(false, false));
	AddGameTexture(CreateShaderTexture(false, true));
	AddGameTexture(CreateShaderTexture(true, false));
	AddGameTexture(CreateShaderTexture(true, true));
	// Add two animtexture entries so that movie playback can call functions using texture IDs.
	AddGameTexture(MakeGameTexture(new AnimTexture(), "AnimTextureFrame1", ETextureType::Override));
	AddGameTexture(MakeGameTexture(new AnimTexture(), "AnimTextureFrame2", ETextureType::Override));

	int wadcnt = fileSystem.GetNumWads();

	FMultipatchTextureBuilder build(*this, progressFunc_, checkForHacks);

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

	glPart2 = TexMan.CheckForTexture("glstuff/glpart2.png", ETextureType::MiscPatch);
	glPart = TexMan.CheckForTexture("glstuff/glpart.png", ETextureType::MiscPatch);
	mirrorTexture = TexMan.CheckForTexture("glstuff/mirror.png", ETextureType::MiscPatch);
	AddLocalizedVariants();

	// Make sure all ID's are correct by resetting them here to the proper index.
	for (unsigned int i = 0, count = Textures.Size(); i < count; ++i)
	{
		Textures[i].Texture->SetID(i);
	}

}

//==========================================================================
//
// FTextureManager :: InitPalettedVersions
//
//==========================================================================

void FTextureManager::InitPalettedVersions()
{
	int lump, lastlump = 0;

	while ((lump = fileSystem.FindLump("PALVERS", &lastlump)) != -1)
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
				sc.ScriptMessage("Unknown texture %s to use as paletted replacement", sc.String);
			}
			if (pic1.isValid() && pic2.isValid())
			{
				Textures[pic1.GetIndex()].Paletted = pic2.GetIndex();
			}
		}
	}
}

//==========================================================================
//
// 
//
//==========================================================================

FTextureID FTextureManager::GetRawTexture(FTextureID texid)
{
	int texidx = texid.GetIndex();
	if ((unsigned)texidx >= Textures.Size()) return texid;
	if (Textures[texidx].FrontSkyLayer != -1) return FSetTextureID(Textures[texidx].FrontSkyLayer);

	// Reject anything that cannot have been a front layer for the sky in original Hexen, i.e. it needs to be an unscaled wall texture only using Doom patches.
	auto tex = Textures[texidx].Texture;
	auto ttex = tex->GetTexture();
	auto image = ttex->GetImage();
	// Reject anything that cannot have been a single-patch multipatch texture in vanilla.
	if (image == nullptr || image->IsRawCompatible() || tex->GetUseType() != ETextureType::Wall || ttex->GetWidth() != tex->GetDisplayWidth() ||
		ttex->GetHeight() != tex->GetDisplayHeight())
	{
		Textures[texidx].RawTexture = texidx;
		return texid;
	}

	// Let the hackery begin
	auto mptimage = static_cast<FMultiPatchTexture*>(image);
	auto source = mptimage->GetImageForPart(0);

	// Size must match for this to work as intended
	if (source->GetWidth() != ttex->GetWidth() || source->GetHeight() != ttex->GetHeight())
	{
		Textures[texidx].RawTexture = texidx;
		return texid;
	}

	// Todo: later this can just link to the already existing texture for this source graphic, once it can be retrieved through the image's SourceLump index
	auto RawTexture = MakeGameTexture(new FImageTexture(source), nullptr, ETextureType::Wall);
	texid = TexMan.AddGameTexture(RawTexture);
	Textures[texidx].RawTexture = texid.GetIndex();
	Textures[texid.GetIndex()].RawTexture = texid.GetIndex();
	return texid;
}


//==========================================================================
//
// Same shit for a different hack, this time Hexen's front sky layers.
//
//==========================================================================

FTextureID FTextureManager::GetFrontSkyLayer(FTextureID texid)
{
	int texidx = texid.GetIndex();
	if ((unsigned)texidx >= Textures.Size()) return texid;
	if (Textures[texidx].FrontSkyLayer != -1) return FSetTextureID(Textures[texidx].FrontSkyLayer);

	// Reject anything that cannot have been a front layer for the sky in original Hexen, i.e. it needs to be an unscaled wall texture only using Doom patches.
	auto tex = Textures[texidx].Texture;
	auto ttex = tex->GetTexture();
	auto image = ttex->GetImage();
	if (image == nullptr || !image->SupportRemap0() || tex->GetUseType() != ETextureType::Wall || tex->useWorldPanning() || tex->GetTexelTopOffset() != 0 ||
		ttex->GetWidth() != tex->GetDisplayWidth() || ttex->GetHeight() != tex->GetDisplayHeight())
	{
		Textures[texidx].FrontSkyLayer = texidx;
		return texid;
	}

	// Set this up so that it serializes to the same info as the base texture - this is needed to restore it on load.
	// But do not link the new texture into the hash chain!
	auto itex = new FImageTexture(image);
	itex->SetNoRemap0();
	auto FrontSkyLayer = MakeGameTexture(itex, tex->GetName(), ETextureType::Wall);
	FrontSkyLayer->SetUseType(tex->GetUseType());
	texid = TexMan.AddGameTexture(FrontSkyLayer, false);
	Textures[texidx].FrontSkyLayer = texid.GetIndex();
	Textures[texid.GetIndex()].FrontSkyLayer = texid.GetIndex();	// also let it refer to itself as its front sky layer, in case for repeated InitSkyMap calls.
	return texid;
}

//==========================================================================
//
//
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
	
	for(int i = fileSystem.GetNumEntries()-1; i>=0; i--)
	{
		int space = fileSystem.GetFileNamespace(i);
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
			if (fileSystem.GetFileFlags(i) & LUMPF_MAYBEFLAT) numtex++;

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
	int wadcount = fileSystem.GetNumWads();
	for (int wadnum = 0; wadnum < wadcount; wadnum++)
	{
		// Use the most recent PNAMES for this WAD.
		// Multiple PNAMES in a WAD will be ignored.
		int pnames = fileSystem.CheckNumForName("PNAMES", ns_global, wadnum, false);

		// should never happen except for zdoom.pk3
		if (pnames < 0) continue;

		// Only count the patches if the PNAMES come from the current file
		// Otherwise they have already been counted.
		if (fileSystem.GetFileContainer(pnames) == wadnum) 
		{
			count += CountLumpTextures (pnames);
		}

		int texlump1 = fileSystem.CheckNumForName ("TEXTURE1", ns_global, wadnum);
		int texlump2 = fileSystem.CheckNumForName ("TEXTURE2", ns_global, wadnum);

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
		auto file = fileSystem.OpenFileReader (lumpnum); 
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

	int numtex = fileSystem.GetNumEntries();

	for (int i = 0; i < numtex; i++)
	{
		if (fileSystem.GetFileContainer(i) > fileSystem.GetMaxIwadNum()) break; // we are past the IWAD
		if (fileSystem.GetFileNamespace(i) == ns_sprites && fileSystem.GetFileContainer(i) >= fileSystem.GetIwadNum() && fileSystem.GetFileContainer(i) <= fileSystem.GetMaxIwadNum())
		{
			char str[9];
			fileSystem.GetFileShortName(str, i);
			str[8] = 0;
			FTextureID texid = TexMan.CheckForTexture(str, ETextureType::Sprite, 0);
			if (texid.isValid() && fileSystem.GetFileContainer(GetGameTexture(texid)->GetSourceLump()) > fileSystem.GetMaxIwadNum())
			{
				// This texture has been replaced by some PWAD.
				memcpy(&sprid, str, 4);
				donotprocess[sprid] = true;
			}
		}
	}

	while ((lump = fileSystem.FindLump("SPROFS", &lastlump, false)) != -1)
	{
		FScanner sc;
		sc.OpenLumpNum(lump);
		sc.SetCMode(true);
		int ofslumpno = fileSystem.GetFileContainer(lump);
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
				auto tex = GetGameTexture(texno);

				int lumpnum = tex->GetSourceLump();
				// We only want to change texture offsets for sprites in the IWAD or the file this lump originated from.
				if (lumpnum >= 0 && lumpnum < fileSystem.GetNumEntries())
				{
					int wadno = fileSystem.GetFileContainer(lumpnum);
					if ((iwadonly && wadno >= fileSystem.GetIwadNum() && wadno <= fileSystem.GetMaxIwadNum()) || (!iwadonly && wadno == ofslumpno))
					{
						if (wadno >= fileSystem.GetIwadNum() && wadno <= fileSystem.GetMaxIwadNum() && !forced && iwadonly)
						{
							memcpy(&sprid, tex->GetName().GetChars(), 4);
							if (donotprocess.CheckKey(sprid)) continue;	// do not alter sprites that only get partially replaced.
						}
						tex->SetOffsets(1, x, y);
					}
				}
			}
		}
	}
}


//==========================================================================
//
// FTextureAnimator :: SetTranslation
//
// Sets animation translation for a texture
//
//==========================================================================

void FTextureManager::SetTranslation(FTextureID fromtexnum, FTextureID totexnum)
{
	if ((size_t)fromtexnum.texnum < Translation.Size())
	{
		if ((size_t)totexnum.texnum >= Textures.Size())
		{
			totexnum.texnum = fromtexnum.texnum;
		}
		Translation[fromtexnum.texnum] = totexnum.texnum;
	}
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

CCMD(flushtextures)
{
	TexMan.FlushAll();
}

