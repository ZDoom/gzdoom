#pragma once

#include <stdint.h>
#include "tarray.h"
#include "textures/bitmap.h"
#include "memarena.h"

class FImageSource;
using PrecacheInfo = TMap<int, std::pair<int, int>>;

struct PalettedPixels
{
	friend class FImageSource;
	TArrayView<uint8_t> Pixels;
private:
	TArray<uint8_t> PixelStore;

	bool ownsPixels() const
	{
		return Pixels.Data() == PixelStore.Data();
	}
};

// This represents a naked image. It has no high level logic attached to it.
// All it can do is provide raw image data to its users.
class FImageSource
{
	friend class FBrightmapImage;
protected:

	static FMemArena ImageArena;
	static TArray<FImageSource *>ImageForLump;
	static int NextID;

	int SourceLump;
	int Width = 0, Height = 0;
	int LeftOffset = 0, TopOffset = 0;			// Offsets stored in the image.
	bool bUseGamePalette = false;				// true if this is an image without its own color set.
	int ImageID = -1;

public:

	void CopySize(FImageSource &other)
	{
		Width = other.Width;
		Height = other.Height;
		LeftOffset = other.LeftOffset;
		TopOffset = other.TopOffset;
		SourceLump = other.SourceLump;
	}

	// Images are statically allocated and freed in bulk. None of the subclasses may hold any destructible data.
	void *operator new(size_t block) { return ImageArena.Alloc(block); }
	void operator delete(void *block) {}

	bool bMasked = true;						// Image (might) have holes (Assume true unless proven otherwise!)
	int8_t bTranslucent = -1;					// Image has pixels with a non-0/1 value. (-1 means the user needs to do a real check)

	
	// 'noremap0' will only be looked at by FPatchTexture and forwarded by FMultipatchTexture.

	// Always creates a new pixel buffer for the texture
	virtual TArray<uint8_t> CreatePalettedPixels(int conversion);		

	// Either returns a reference to the cache, or a newly created item. The return of this has to be considered transient. If you need to store the result, use GetPalettedPixels
	PalettedPixels GetCachedPalettedPixels(int conversion);

	// tries to get a buffer from the cache. If not available, create a new one. If further references are pending, create a copy.
	TArray<uint8_t> GetPalettedPixels(int conversion);

	virtual int CopyPixels(FBitmap *bmp, int conversion);			// This will always ignore 'luminance'.
	int CopyTranslatedPixels(FBitmap *bmp, PalEntry *remap);
	static void ClearImages() { ImageArena.FreeAll(); ImageForLump.Clear(); NextID = 0; }
	static FImageSource * FImageSource::GetImage(int lumpnum, ETextureType usetype);

	// These functions either allocate a new buffer or reuse the last one, if its reference count was greater than 1 when last used.
	FBitmap GetPixelsWithCache(int conversion);


	// Conversion option
	enum EType
	{
		normal = 0,
		luminance = 1,
		noremap0 = 2
	};
	
	FImageSource(int sourcelump = -1) : SourceLump(sourcelump) { ImageID = ++NextID; }
	virtual ~FImageSource() {}
	
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

	virtual void CollectForPrecache(PrecacheInfo &info, bool requiretruecolor = false);
	static void BeginPrecaching();
	static void EndPrecaching();
	static void RegisterForPrecache(FImageSource *img);
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

