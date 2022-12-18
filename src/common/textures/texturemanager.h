#pragma once

#include <stdint.h>
#include "tarray.h"
#include "textureid.h"
#include "textures.h"
#include "basics.h"
#include "texmanip.h"
#include "name.h"

class FxAddSub;
struct BuildInfo;
class FMultipatchTextureBuilder;
class FScanner;

// Texture manager
class FTextureManager
{
	void (*progressFunc)();
	friend class FxAddSub;	// needs access to do a bounds check on the texture ID.
public:
	FTextureManager ();
	~FTextureManager ();

private:
	int ResolveLocalizedTexture(int texnum);

	int ResolveTextureIndex(int texnum, bool animate) const
	{
		if ((unsigned)texnum >= Textures.Size()) return -1;
		if (animate) texnum = Translation[texnum];
		//if (localize && Textures[texnum].Flags & TEXFLAG_HASLOCALIZATION) texnum = ResolveLocalizedTexture(texnum);
		return texnum;
	}

	FGameTexture *InternalGetTexture(int texnum, bool animate) const
	{
		texnum = ResolveTextureIndex(texnum, animate);
		if (texnum == -1) return nullptr;
		return Textures[texnum].Texture;
	}

	FTextureID ResolveTextureIndex(FTextureID texid, bool animate) const
	{
		return FSetTextureID(ResolveTextureIndex(texid.GetIndex(), animate));
	}

public:
	// This only gets used in UI code so we do not need PALVERS handling.
	FGameTexture* GetGameTextureByName(const char *name, bool animate = false, int flags = 0)
	{
		FTextureID texnum = GetTextureID(name, ETextureType::MiscPatch, flags);
		return InternalGetTexture(texnum.GetIndex(), animate);
	}

	FGameTexture* GetGameTexture(FTextureID texnum, bool animate = false) const
	{
		return InternalGetTexture(texnum.GetIndex(), animate);
	}

	FGameTexture* GetPalettedTexture(FTextureID texnum, bool animate = false, bool allowsubstitute = true) const
	{
		auto texid = ResolveTextureIndex(texnum.GetIndex(), animate);
		if (texid == -1) return nullptr;
		if (allowsubstitute && Textures[texid].Paletted > 0) texid = Textures[texid].Paletted;
		return Textures[texid].Texture;
	}

	FGameTexture* GameByIndex(int i, bool animate = false) const
	{
		return InternalGetTexture(i, animate);
	}

	FGameTexture* FindGameTexture(const char* texname, ETextureType usetype = ETextureType::MiscPatch, BITFIELD flags = TEXMAN_TryAny);

	bool OkForLocalization(FTextureID texnum, const char *substitute, int locnum);

	void FlushAll();
	void Listaliases();
	FTextureID GetFrontSkyLayer(FTextureID);
	FTextureID GetRawTexture(FTextureID tex, bool dontlookup = false);
	void SetRawTexture(FTextureID texid) 
	{ 
		int texidx = texid.GetIndex();
		if ((unsigned)texidx < Textures.Size())
		{
			Textures[texidx].RawTexture = texidx;
		}
	}


	enum
	{
		TEXMAN_TryAny = 1,
		TEXMAN_Overridable = 2,
		TEXMAN_ReturnFirst = 4,
		TEXMAN_AllowSkins = 8,
		TEXMAN_ShortNameOnly = 16,
		TEXMAN_DontCreate = 32,
		TEXMAN_Localize = 64,
		TEXMAN_ForceLookup = 128,
		TEXMAN_NoAlias = 256,
		TEXMAN_ReturnAll = 512,
	};

	enum
	{
		HIT_Wall = 1,
		HIT_Flat = 2,
		HIT_Sky = 4,
		HIT_Sprite = 8,

		HIT_Columnmode = HIT_Wall|HIT_Sky|HIT_Sprite
	};

	FTextureID CheckForTexture (const char *name, ETextureType usetype, BITFIELD flags=TEXMAN_TryAny);
	FTextureID GetTextureID (const char *name, ETextureType usetype, BITFIELD flags=0);
	int ListTextures (const char *name, TArray<FTextureID> &list, bool listall = false);

	void AddGroup(int wadnum, int ns, ETextureType usetype);
	void AddPatches (int lumpnum);
	void AddHiresTextures (int wadnum);
	void LoadTextureDefs(int wadnum, const char *lumpname, FMultipatchTextureBuilder &build);
	void ParseColorization(FScanner& sc);
	void ParseTextureDef(int remapLump, FMultipatchTextureBuilder &build);
	void SortTexturesByType(int start, int end);
	bool AreTexturesCompatible (FTextureID picnum1, FTextureID picnum2);
	void AddLocalizedVariants();

	FTextureID CreateTexture (int lumpnum, ETextureType usetype=ETextureType::Any);	// Also calls AddTexture
	FTextureID AddGameTexture(FGameTexture* texture, bool addtohash = true);
	FTextureID GetDefaultTexture() const { return DefaultTexture; }

	void LoadTextureX(int wadnum, FMultipatchTextureBuilder &build);
	void AddTexturesForWad(int wadnum, FMultipatchTextureBuilder &build);
	void Init();
	void AddTextures(void (*progressFunc_)(), void (*checkForHacks)(BuildInfo&), void (*customtexturehandler)() = nullptr);
	void DeleteAll();

	void ReplaceTexture (FTextureID picnum, FGameTexture *newtexture, bool free);

	int NumTextures () const { return (int)Textures.Size(); }

	int GuesstimateNumTextures ();

	TextureManipulation* GetTextureManipulation(FName name)
	{
		return tmanips.CheckKey(name);
	}
	void InsertTextureManipulation(FName cname, TextureManipulation tm)
	{
		tmanips.Insert(cname, tm);
	}
	void RemoveTextureManipulation(FName cname)
	{
		tmanips.Remove(cname);
	}

	void AddAlias(const char* name, int texindex);
	void AddAlias(const char* name, FTextureID texindex)
	{
		AddAlias(name, texindex.GetIndex());
	}

private:

	// texture counting
	int CountTexturesX ();
	int CountLumpTextures (int lumpnum);
	void AdjustSpriteOffsets();

	// Build tiles
	//int CountBuildTiles ();

public:

	TArray<uint8_t>& GetNewBuildTileData()
	{
		BuildTileData.Reserve(1);
		return BuildTileData.Last();
	}
	TArray<TArray<uint8_t>>& GetBuildTileDataStore()
	{
		return BuildTileData;
	}

	FGameTexture* GameTexture(FTextureID id) { return Textures[id.GetIndex()].Texture; }
	void SetTranslation(FTextureID fromtexnum, FTextureID totexnum);

private:

	void InitPalettedVersions();

	// Switches

	struct TextureDescriptor
	{
		FGameTexture* Texture;
		int Paletted;		// redirection to paletted variant
		int FrontSkyLayer;	// and front sky layer,
		int RawTexture;		
		int HashNext;
		uint64_t Flags;
	};

	enum : uint64_t
	{
		TEXFLAG_HASLOCALIZATION = 1,
	};
public:
	constexpr static int TEXFLAG_FIRSTUSER = 65536;	// this leaves 16 flags to the texture manager and 48 flags to the user
private:

	enum { HASH_END = -1, HASH_SIZE = 1027 };
	TArray<TextureDescriptor> Textures;
	TMap<uint64_t, int> LocalizedTextures;
	int HashFirst[HASH_SIZE];
	FTextureID DefaultTexture;
	TArray<int> FirstTextureForFile;
	TArray<TArray<uint8_t> > BuildTileData;
	TArray<int> Translation;

	TMap<FName, TextureManipulation> tmanips;
	TMap<FName, int> aliases;

public:

	short sintable[2048];	// for texture warping
	enum
	{
		SINMASK = 2047
	};

	FTextureID glPart2;
	FTextureID glPart;
	FTextureID mirrorTexture;
	bool usefullnames;

};

extern FTextureManager TexMan;

