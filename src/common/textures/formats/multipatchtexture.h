#pragma once
#include "sc_man.h"
#include "palettecontainer.h"
#include "textureid.h"
#include "vectors.h"
#include "bitmap.h"
#include "image.h"
#include "textures.h"

class FImageTexture;
class FTextureManager;

//==========================================================================
//
// TexPart is the data that will get passed to the final texture.
//
//==========================================================================

struct TexPart
{
	FRemapTable *Translation = nullptr;
	FImageSource *Image = nullptr;
	PalEntry Blend = 0;
	blend_t Alpha = FRACUNIT;
	int16_t OriginX = 0;
	int16_t OriginY = 0;
	uint8_t Rotate = 0;
	uint8_t op = OP_COPY;
};

struct TexPartBuild
{
	FRemapTable* Translation = nullptr;
	FImageTexture *TexImage = nullptr;
	PalEntry Blend = 0;
	blend_t Alpha = FRACUNIT;
	int16_t OriginX = 0;
	int16_t OriginY = 0;
	uint8_t Rotate = 0;
	uint8_t op = OP_COPY;
};



//==========================================================================
//
// A texture defined in a TEXTURE1 or TEXTURE2 lump
//
//==========================================================================

class FMultiPatchTexture : public FImageSource
{
	friend class FTexture;
	friend class FGameTexture;
public:
	FMultiPatchTexture(int w, int h, const TArray<TexPartBuild> &parts, bool complex, bool textual);
	int GetNumParts() const { return NumParts; }
	// Query some needed info for texture hack support.
	bool SupportRemap0() override;
	bool IsRawCompatible() override 
	{
		return NumParts != 1 || Parts[0].OriginY == 0 || bTextual;
	}
	FImageSource* GetImageForPart(int num)
	{
		if (num >= 0 && num < NumParts) return Parts[num].Image;
		return nullptr;
	}

protected:
	int NumParts;
	bool bComplex;
	bool bTextual;
	TexPart *Parts;

	// The getters must optionally redirect if it's a simple one-patch texture.
	int CopyPixels(FBitmap *bmp, int conversion, int frame = 0) override;
	PalettedPixels CreatePalettedPixels(int conversion, int frame = 0) override;
	void CopyToBlock(uint8_t *dest, int dwidth, int dheight, FImageSource *source, int xpos, int ypos, int rotate, const uint8_t *translation, int style);
	void CollectForPrecache(PrecacheInfo &info, bool requiretruecolor) override;

};


//==========================================================================
//
// Additional data per patch which is needed for initialization
//
//==========================================================================

struct TexInit
{
	FString TexName;
	ETextureType UseType = ETextureType::Null;
	FGameTexture *GameTexture = nullptr;
	bool Silent = false;
	bool HasLine = false;
	bool UseOffsets = false;
	FScriptPosition sc;
};

//==========================================================================
//
// All build info only needed to construct the multipatch textures
//
//==========================================================================

struct FPatchLookup;

struct BuildInfo
{
	FString Name;
	TArray<TexPartBuild> Parts;
	TArray<TexInit> Inits;
	int Width = 0;
	int Height = 0;
	DVector2 Scale = { 1, 1 };
	bool bWorldPanning = false;	// This sucks!
	int DefinitionLump = 0;
	bool bComplex = false;
	bool textual = false;
	bool bNoDecals = false;
	bool bNoTrim = false;
	int LeftOffset[2] = {};
	int TopOffset[2] = {};
	FGameTexture *texture = nullptr;

	void swap(BuildInfo &other)
	{
		Name.Swap(other.Name);
		Parts.Swap(other.Parts);
		Inits.Swap(other.Inits);
		std::swap(Width, other.Width);
		std::swap(Height, other.Height);
		std::swap(Scale, other.Scale);
		std::swap(bWorldPanning, other.bWorldPanning);
		std::swap(DefinitionLump, other.DefinitionLump);
		std::swap(bComplex, other.bComplex);
		std::swap(textual, other.textual);
		std::swap(bNoDecals, other.bNoDecals);
		std::swap(LeftOffset[0], other.LeftOffset[0]);
		std::swap(LeftOffset[1], other.LeftOffset[1]);
		std::swap(TopOffset[0], other.TopOffset[0]);
		std::swap(TopOffset[1], other.TopOffset[1]);
		std::swap(texture, other.texture);
	}
};



class FMultipatchTextureBuilder
{
	FTextureManager &TexMan;
	TArray<BuildInfo> BuiltTextures;
	TMap<FGameTexture*, bool> complex;
	void(*progressFunc)();
	void(*checkForHacks)(BuildInfo&);

	void MakeTexture(BuildInfo &buildinfo, ETextureType usetype);
	void AddImageToTexture(FImageTexture* tex, BuildInfo& buildinfo);

	void BuildTexture(const void *texdef, FPatchLookup *patchlookup, int maxpatchnum, bool strife, int deflumpnum, ETextureType usetyoe);
	void AddTexturesLump(const void *lumpdata, int lumpsize, int deflumpnum, int patcheslump, int firstdup, bool texture1);

	void ParsePatch(FScanner &sc, BuildInfo &info, TexPartBuild &part, TexInit &init);
	void ResolvePatches(BuildInfo &buildinfo);

public:
	FMultipatchTextureBuilder(FTextureManager &texMan, void(*progressFunc_)(), void(*checkForHacks_)(BuildInfo &)) : TexMan(texMan), progressFunc(progressFunc_), checkForHacks(checkForHacks_)
	{
	}

	void AddTexturesLumps(int lump1, int lump2, int patcheslump);
	void ParseTexture(FScanner &sc, ETextureType usetype, int deflump);
	void ResolveAllPatches();
};
