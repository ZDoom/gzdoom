#pragma once
#include "sc_man.h"

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



//==========================================================================
//
// A texture defined in a TEXTURE1 or TEXTURE2 lump
//
//==========================================================================

class FMultiPatchTexture : public FImageSource
{
	friend class FTexture;
public:
	FMultiPatchTexture(int w, int h, const TArray<TexPart> &parts, bool complex, bool textual);

protected:
	int NumParts;
	bool bComplex;
	bool bTextual;
	TexPart *Parts;

	// The getters must optionally redirect if it's a simple one-patch texture.
	int CopyPixels(FBitmap *bmp, int conversion) override;
	TArray<uint8_t> CreatePalettedPixels(int conversion) override;
	void CopyToBlock(uint8_t *dest, int dwidth, int dheight, FImageSource *source, int xpos, int ypos, int rotate, const uint8_t *translation, int style);
	void CollectForPrecache(PrecacheInfo &info, bool requiretruecolor);

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
	FTexture *Texture = nullptr;
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
	TArray<TexPart> Parts;
	TArray<TexInit> Inits;
	int Width = 0;
	int Height = 0;
	DVector2 Scale = { 1, 1 };
	bool bWorldPanning = false;	// This sucks!
	int DefinitionLump = 0;
	bool bComplex = false;
	bool textual = false;
	bool bNoDecals = false;
	int LeftOffset[2] = {};
	int TopOffset[2] = {};
	FImageTexture *tex = nullptr;

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
		std::swap(tex, other.tex);
	}
};



class FMultipatchTextureBuilder
{
	FTextureManager &TexMan;
	TArray<BuildInfo> BuiltTextures;

	void MakeTexture(BuildInfo &buildinfo, ETextureType usetype);

	void BuildTexture(const void *texdef, FPatchLookup *patchlookup, int maxpatchnum, bool strife, int deflumpnum, ETextureType usetyoe);
	void AddTexturesLump(const void *lumpdata, int lumpsize, int deflumpnum, int patcheslump, int firstdup, bool texture1);

	void ParsePatch(FScanner &sc, BuildInfo &info, TexPart &part, TexInit &init);
	void CheckForHacks(BuildInfo &buildinfo);
	void ResolvePatches(BuildInfo &buildinfo);

public:
	FMultipatchTextureBuilder(FTextureManager &texMan) : TexMan(texMan)
	{
	}

	void AddTexturesLumps(int lump1, int lump2, int patcheslump);
	void ParseTexture(FScanner &sc, ETextureType usetype);
	void ResolveAllPatches();
};
