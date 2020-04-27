#pragma once
#include "textures.h"
#include "v_video.h"
#include "g_levellocals.h"
#include "d_main.h"


struct FSoftwareTextureSpan
{
	uint16_t TopOffset;
	uint16_t Length;	// A length of 0 terminates this column
};


// For now this is just a minimal wrapper around FTexture. Once the software renderer no longer accesses FTexture directly, it is time for cleaning up.
class FSoftwareTexture : public ISoftwareTexture
{
protected:
	FGameTexture *mTexture;
	FTexture *mSource;
	TArray<uint8_t> Pixels;
	TArray<uint32_t> PixelsBgra;
	FSoftwareTextureSpan **Spandata[3] = { };
	DVector2 Scale;
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
	FSoftwareTexture(FGameTexture *tex);
	
	virtual ~FSoftwareTexture()
	{
		FreeAllSpans();
	}

	FGameTexture *GetTexture() const
	{
		return mTexture;
	}
	
	// The feature from hell... :(
	bool useWorldPanning(FLevelLocals *Level) const
	{
		return mTexture->useWorldPanning() || (Level->flags3 & LEVEL3_FORCEWORLDPANNING);
	}

	bool isMasked()
	{
		return mTexture->isMasked();
	}

	uint16_t GetRotations() const
	{
		return mTexture->GetRotations();
	}
	
	int GetSkyOffset() const { return mTexture->GetSkyOffset(); }
	
	int GetWidth () { return mTexture->GetTexelWidth(); }
	int GetHeight () { return mTexture->GetTexelHeight(); }
	int GetWidthBits() { return WidthBits; }
	int GetHeightBits() { return HeightBits; }

	double GetScaledWidth () { return mTexture->GetDisplayWidth(); }
	double GetScaledHeight () { return mTexture->GetDisplayHeight(); }

	// Now with improved offset adjustment.
	int GetLeftOffset(int adjusted) { return mTexture->GetTexelLeftOffset(adjusted); }
	int GetTopOffset(int adjusted) { return mTexture->GetTexelTopOffset(adjusted); }
	
	// Interfaces for the different renderers. Everything that needs to check renderer-dependent offsets
	// should use these, so that if changes are needed, this is the only place to edit.
	
	// For the original software renderer
	int GetLeftOffsetSW() { return GetLeftOffset(r_spriteadjustSW); }
	int GetTopOffsetSW() { return GetTopOffset(r_spriteadjustSW); }
	double GetScaledLeftOffsetSW() { return mTexture->GetDisplayLeftOffset(r_spriteadjustSW); }
	double GetScaledTopOffsetSW() { return mTexture->GetDisplayTopOffset(r_spriteadjustSW); }
	
	DVector2 GetScale() const { return Scale; }
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
	FWarpTexture (FGameTexture *source, int warptype);

	const uint32_t *GetPixelsBgra() override;
	const uint8_t *GetPixels(int style) override;
	bool CheckModified (int which) override;
	void GenerateBgraMipmapsFast();

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

	FSWCanvasTexture(FGameTexture* source);
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

FSoftwareTexture* GetSoftwareTexture(FGameTexture* tex);
FSoftwareTexture* GetPalettedSWTexture(FTextureID texid, bool animate, bool checkcompat = false, bool allownull = false);
