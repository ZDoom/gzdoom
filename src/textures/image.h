#pragma once

#include <stdint.h>
#include "tarray.h"
#include "textures/bitmap.h"
#include "memarena.h"


// This represents a naked image. It has no high level logic attached to it.
// All it can do is provide raw image data to its users.
class FImageSource
{
	friend class FBrightmapImage;
protected:

	static FMemArena ImageArena;
	int SourceLump;
	int Width = 0, Height = 0;
	int LeftOffset = 0, TopOffset = 0;			// Offsets stored in the image.
	bool bUseGamePalette = false;				// true if this is an image without its own color set.

public:

	// Images are statically allocated and freed in bulk. None of the subclasses may hold any destructible data.
	void *operator new(size_t block) { return ImageArena.Alloc(block); }
	void operator delete(void *block) {}

	bool bMasked = true;						// Image (might) have holes (Assume true unless proven otherwise!)
	int8_t bTranslucent = -1;					// Image has pixels with a non-0/1 value. (-1 means the user needs to do a real check)

	// Returns the whole texture, paletted and true color versions respectively.
	virtual TArray<uint8_t> GetPalettedPixels(int conversion);		// 'noremap0' will only be looked at by FPatchTexture and forwarded by FMultipatchTexture.
	virtual int CopyPixels(FBitmap *bmp, int conversion);			// This will always ignore 'luminance'.
	int CopyTranslatedPixels(FBitmap *bmp, PalEntry *remap);
	static void ClearImages() { ImageArena.FreeAll(); }
	
	// Conversion option
	enum EType
	{
		normal = 0,
		luminance = 1,
		noremap0 = 2
	};
	
	FImageSource(int sourcelump = -1) : SourceLump(sourcelump) {}
	virtual ~FImageSource() {}
	
	// Creates an image from the given lump.
	static FImageSource *CreateImageSource(int lumpnum);

	int GetWidth() const
	{
		return Width;
	}
	
	int GetHeight() const
	{
		return Height;
	}
	
	std::pair<int, int> GetSize() const
	{
		return std::make_pair(Width, Height);
	}
	
	std::pair<int, int> GetOffsets() const
	{
		return std::make_pair(LeftOffset, TopOffset);
	}
	
	int LumpNum() const
	{
		return SourceLump;
	}
	
	bool UseGamePalette() const
	{
		return bUseGamePalette;
	}
};

//==========================================================================
//
// a TGA texture
//
//==========================================================================

class FImageTexture : public FTexture
{
	FImageSource *mImage;
public:
	FImageTexture (FImageSource *image, const char *name = nullptr);
	virtual TArray<uint8_t> Get8BitPixels(bool alphatex);

	void SetImage(FImageSource *img)	// This is only for the multipatch texture builder!
	{
		mImage = img;
	}

	FImageSource *GetImage() const override { return mImage; }

protected:
	int CopyPixels(FBitmap *bmp) override;
	
};

