
#ifndef __GL_MATERIAL_H
#define __GL_MATERIAL_H

#include "m_fixed.h"
#include "textures.h"

struct FRemapTable;
class IHardwareTexture;

enum
{
	CLAMP_NONE = 0,
	CLAMP_X = 1,
	CLAMP_Y = 2,
	CLAMP_XY = 3,
	CLAMP_XY_NOMIP = 4,
	CLAMP_NOFILTER = 5,
	CLAMP_CAMTEX = 6,
};


//===========================================================================
// 
// this is the material class for OpenGL. 
//
//===========================================================================

class FMaterial
{
	private:
	TArray<FTexture*> mTextureLayers;
	int mShaderIndex;
	int mLayerFlags = 0;

	short mLeftOffset;
	short mTopOffset;
	short mWidth;
	short mHeight;
	short mRenderWidth;
	short mRenderHeight;
	bool mExpanded;

public:
	FTexture *sourcetex;	// the owning texture. 
	FTexture* imgtex;		// the master texture for the backing image. Can be different from sourcetex and should not be in the layer array because that'd confuse the precacher.

	FMaterial(FTexture *tex, bool forceexpand);
	~FMaterial();
	int GetLayerFlags() const { return mLayerFlags; }
	void SetSpriteRect();
	int GetShaderIndex() const { return mShaderIndex; }

	FTexture* Source() const
	{
		return sourcetex;
	}
	FTexture* BaseLayer() const
	{
		// Only for spftpoly!
		return imgtex;
	}
	bool isFullbright() const
	{
		return sourcetex->isFullbright();
	}
	bool isHardwareCanvas() const
	{
		return sourcetex->isHardwareCanvas();
	}
	bool GetTranslucency()
	{
		// This queries the master texture to reduce recalculations.
		return imgtex->GetTranslucency();
	}
	void AddTextureLayer(FTexture *tex)
	{
		ValidateTexture(tex, false);
		mTextureLayers.Push(tex);
	}
	bool isMasked() const
	{
		return sourcetex->bMasked;
	}
	bool isExpanded() const
	{
		return mExpanded;
	}

	int GetLayers() const
	{
		return mTextureLayers.Size() + 1;
	}
	
	bool hasCanvas()
	{
		return sourcetex->isHardwareCanvas();
	}

	IHardwareTexture *GetLayer(int i, int translation, FTexture **pLayer = nullptr);

	// Patch drawing utilities

	// This is scaled size in integer units as needed by walls and flats
	int TextureHeight() const { return mRenderHeight; }
	int TextureWidth() const { return mRenderWidth; }

	int GetWidth() const
	{
		return mWidth;
	}

	int GetHeight() const
	{
		return mHeight;
	}

	int GetLeftOffset() const
	{
		return mLeftOffset;
	}

	int GetTopOffset() const
	{
		return mTopOffset;
	}

	// Get right/bottom UV coordinates for patch drawing
	float GetUL() const { return 0; }
	float GetVT() const { return 0; }
	float GetUR() const { return 1; }
	float GetVB() const { return 1; }
	float GetU(float upix) const { return upix/(float)mWidth; }
	float GetV(float vpix) const { return vpix/(float)mHeight; }

	static FMaterial *ValidateTexture(FTexture * tex, bool expand, bool create = true);
	static FMaterial *ValidateTexture(FTextureID no, bool expand, bool trans, bool create = true);
	const TArray<FTexture*> &GetLayerArray() const
	{
		return mTextureLayers;
	}
};

#endif


