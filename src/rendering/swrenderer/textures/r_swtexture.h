#pragma once
#include "textures/textures.h"
#include "v_video.h"
#include "g_levellocals.h"


struct FSoftwareTextureSpan
{
	uint16_t TopOffset;
	uint16_t Length;	// A length of 0 terminates this column
};


// For now this is just a minimal wrapper around FTexture. Once the software renderer no longer accesses FTexture directly, it is time for cleaning up.
class FSoftwareTexture
{
protected:
	FTexture *mTexture;
	FTexture *mSource;
	TArray<uint8_t> Pixels;
	TArray<uint32_t> PixelsBgra;
	FSoftwareTextureSpan **Spandata[3] = { };
	uint8_t WidthBits = 0, HeightBits = 0;
	uint16_t WidthMask = 0;
	int mPhysicalWidth, mPhysicalHeight;
	int mPhysicalScale;
	int mBufferFlags;

	void FreeAllSpans();
	template<class T> FSoftwareTextureSpan **CreateSpans(const T *pixels);
	void FreeSpans(FSoftwareTextureSpan **spans);
	void CalcBitSize();

public:
	FSoftwareTexture(FTexture *tex);
	
	virtual ~FSoftwareTexture()
	{
		FreeAllSpans();
	}

	FTexture *GetTexture() const
	{
		return mTexture;
	}
	
	// The feature from hell... :(
	bool useWorldPanning(FLevelLocals *Level) const
	{
		return mTexture->bWorldPanning || (Level->flags3 & LEVEL3_FORCEWORLDPANNING);
	}

	bool isMasked()
	{
		return mTexture->bMasked;
	}
	
	int GetSkyOffset() const { return mTexture->GetSkyOffset(); }
	PalEntry GetSkyCapColor(bool bottom) const { return mTexture->GetSkyCapColor(bottom); }
	
	int GetWidth () { return mTexture->GetWidth(); }
	int GetHeight () { return mTexture->GetHeight(); }
	int GetWidthBits() { return WidthBits; }
	int GetHeightBits() { return HeightBits; }

	int GetScaledWidth () { return mTexture->GetScaledWidth(); }
	int GetScaledHeight () { return mTexture->GetScaledHeight(); }
	double GetScaledWidthDouble () { return mTexture->GetScaledWidthDouble(); }
	double GetScaledHeightDouble () { return mTexture->GetScaledHeightDouble(); }
	
	// Now with improved offset adjustment.
	int GetLeftOffset(int adjusted) { return mTexture->GetLeftOffset(adjusted); }
	int GetTopOffset(int adjusted) { return mTexture->GetTopOffset(adjusted); }
	int GetScaledLeftOffset (int adjusted) { return mTexture->GetScaledLeftOffset(adjusted); }
	int GetScaledTopOffset (int adjusted) { return mTexture->GetScaledTopOffset(adjusted); }
	double GetScaledLeftOffsetDouble(int adjusted) { return mTexture->GetScaledLeftOffsetDouble(adjusted); }
	double GetScaledTopOffsetDouble(int adjusted) { return mTexture->GetScaledTopOffsetDouble(adjusted); }
	
	// Interfaces for the different renderers. Everything that needs to check renderer-dependent offsets
	// should use these, so that if changes are needed, this is the only place to edit.
	
	// For the original software renderer
	int GetLeftOffsetSW() { return GetLeftOffset(r_spriteadjustSW); }
	int GetTopOffsetSW() { return GetTopOffset(r_spriteadjustSW); }
	int GetScaledLeftOffsetSW() { return GetScaledLeftOffset(r_spriteadjustSW); }
	int GetScaledTopOffsetSW() { return GetScaledTopOffset(r_spriteadjustSW); }
	
	// For the softpoly renderer, in case it wants adjustment
	int GetLeftOffsetPo() { return GetLeftOffset(r_spriteadjustSW); }
	int GetTopOffsetPo() { return GetTopOffset(r_spriteadjustSW); }
	int GetScaledLeftOffsetPo() { return GetScaledLeftOffset(r_spriteadjustSW); }
	int GetScaledTopOffsetPo() { return GetScaledTopOffset(r_spriteadjustSW); }
	
	DVector2 GetScale() const { return mTexture->Scale; }
	int GetPhysicalWidth() { return mPhysicalWidth; }
	int GetPhysicalHeight() { return mPhysicalHeight; }
	int GetPhysicalScale() const { return mPhysicalScale; }
	
	virtual void Unload()
	{
		Pixels.Reset();
		PixelsBgra.Reset();
	}
	
	// Returns true if the next call to GetPixels() will return an image different from the
	// last call to GetPixels(). This should be considered valid only if a call to CheckModified()
	// is immediately followed by a call to GetPixels().
	virtual bool CheckModified (int which) { return false; }

	void GenerateBgraFromBitmap(const FBitmap &bitmap);
	void CreatePixelsBgraWithMipmaps();
	void GenerateBgraMipmaps();
	void GenerateBgraMipmapsFast();
	int MipmapLevels();
	
	// Returns true if GetPixelsBgra includes mipmaps
	virtual bool Mipmapped() { return true; }

	// Returns a single column of the texture
	virtual const uint8_t *GetColumn(int style, unsigned int column, const FSoftwareTextureSpan **spans_out);

	// Returns a single column of the texture, in BGRA8 format
	virtual const uint32_t *GetColumnBgra(unsigned int column, const FSoftwareTextureSpan **spans_out);

	// Returns the whole texture, stored in column-major order, in BGRA8 format
	virtual const uint32_t *GetPixelsBgra();

	// Returns the whole texture, stored in column-major order
	virtual const uint8_t *GetPixels(int style);

	const uint8_t *GetPixels(FRenderStyle style)
	{
		bool alpha = !!(style.Flags & STYLEF_RedIsAlpha);
		return GetPixels(alpha);
	}

	// Checks if the pixel data is loaded.
	bool CheckPixels() const
	{
		return V_IsTrueColor() ? PixelsBgra.Size() > 0 : Pixels.Size() > 0;
	}

	const uint8_t *GetColumn(FRenderStyle style, unsigned int column, const FSoftwareTextureSpan **spans_out)
	{
		bool alpha = !!(style.Flags & STYLEF_RedIsAlpha);
		return GetColumn(alpha, column, spans_out);
	}

};

// A texture that returns a wiggly version of another texture.
class FWarpTexture : public FSoftwareTexture
{
	TArray<uint8_t> WarpedPixels[2];
	TArray<uint32_t> WarpedPixelsRgba;

	int bWarped = 0;
	uint64_t GenTime[3] = { UINT64_MAX, UINT64_MAX, UINT64_MAX };
	int WidthOffsetMultiplier, HeightOffsetMultiplier;  // [mxd]

public:
	FWarpTexture (FTexture *source, int warptype);

	const uint32_t *GetPixelsBgra() override;
	const uint8_t *GetPixels(int style) override;
	bool CheckModified (int which) override;

private:

	int NextPo2 (int v); // [mxd]
	void SetupMultipliers (int width, int height); // [mxd]
};

class FSWCanvasTexture : public FSoftwareTexture
{
	void MakeTexture();
	void MakeTextureBgra();

	DCanvas *Canvas = nullptr;
	DCanvas *CanvasBgra = nullptr;

public:

	FSWCanvasTexture(FTexture *source) : FSoftwareTexture(source) {}
	~FSWCanvasTexture();

	// Returns the whole texture, stored in column-major order
	const uint32_t *GetPixelsBgra() override;
	const uint8_t *GetPixels(int style) override;

	virtual void Unload() override;
	void UpdatePixels(bool truecolor);

	DCanvas *GetCanvas() { GetPixels(0); return Canvas; }
	DCanvas *GetCanvasBgra() { GetPixelsBgra(); return CanvasBgra; }
	bool Mipmapped() override { return false; }

};
