
#ifndef __GL_TEXTURE_H
#define __GL_TEXTURE_H

#include "m_fixed.h"
#include "textures/textures.h"
#include "gl/textures/gl_hwtexture.h"
#include "gl/renderer/gl_colormap.h"
#include "i_system.h"

EXTERN_CVAR(Bool, gl_precache)

struct FRemapTable;
class FTextureShader;

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



struct FTexCoordInfo
{
	int mRenderWidth;
	int mRenderHeight;
	int mWidth;
	fixed_t mScaleX;
	fixed_t mScaleY;
	fixed_t mTempScaleX;
	fixed_t mTempScaleY;
	bool mWorldPanning;

	float FloatToTexU(float v) const { return v / mRenderWidth; }
	float FloatToTexV(float v) const { return v / mRenderHeight; }
	fixed_t RowOffset(fixed_t ofs) const;
	fixed_t TextureOffset(fixed_t ofs) const;
	fixed_t TextureAdjustWidth() const;
};

//===========================================================================
// 
// this is the texture maintenance class for OpenGL. 
//
//===========================================================================
class FMaterial;


class FGLTexture
{
	friend class FMaterial;
public:
	FTexture * tex;
	FTexture * hirestexture;
	char bIsTransparent;
	int HiresLump;

private:
	FHardwareTexture *mHwTexture;

	bool bHasColorkey;		// only for hires
	bool bExpandFlag;

	unsigned char * LoadHiresTexture(FTexture *hirescheck, int *width, int *height);

	FHardwareTexture *CreateHwTexture();

	const FHardwareTexture *Bind(int texunit, int clamp, int translation, FTexture *hirescheck);
	
public:
	FGLTexture(FTexture * tx, bool expandpatches);
	~FGLTexture();

	unsigned char * CreateTexBuffer(int translation, int & w, int & h, FTexture *hirescheck, bool createexpanded = true);

	void Clean(bool all);
	int Dump(int i);

};

//===========================================================================
// 
// this is the material class for OpenGL. 
//
//===========================================================================

class FMaterial
{
	friend class FRenderState;

	struct FTextureLayer
	{
		FTexture *texture;
		bool animated;
	};

	static TArray<FMaterial *> mMaterials;
	static int mMaxBound;

	FGLTexture *mBaseLayer;	
	TArray<FTextureLayer> mTextureLayers;
	int mShaderIndex;

	short mLeftOffset;
	short mTopOffset;
	short mWidth;
	short mHeight;
	short mRenderWidth;
	short mRenderHeight;
	bool mExpanded;

	float mSpriteU[2], mSpriteV[2];
	FloatRect mSpriteRect;

	FGLTexture * ValidateSysTexture(FTexture * tex, bool expand);
	bool TrimBorders(int *rect);

public:
	FTexture *tex;
	
	FMaterial(FTexture *tex, bool forceexpand);
	~FMaterial();
	void Precache();
	bool isMasked() const
	{
		return !!mBaseLayer->tex->bMasked;
	}

	int GetLayers() const
	{
		return mTextureLayers.Size() + 1;
	}

	void Bind(int clamp, int translation);

	unsigned char * CreateTexBuffer(int translation, int & w, int & h, bool allowhires=true, bool createexpanded = true) const
	{
		return mBaseLayer->CreateTexBuffer(translation, w, h, allowhires? tex : NULL, createexpanded);
	}

	void Clean(bool f)
	{
		mBaseLayer->Clean(f);
	}

	void BindToFrameBuffer();
	// Patch drawing utilities

	void GetSpriteRect(FloatRect * r) const
	{
		*r = mSpriteRect;
	}

	void GetTexCoordInfo(FTexCoordInfo *tci, fixed_t x, fixed_t y) const;

	// This is scaled size in integer units as needed by walls and flats
	int TextureHeight() const { return mRenderHeight; }
	int TextureWidth() const { return mRenderWidth; }

	int GetAreas(FloatRect **pAreas) const;

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

	int GetScaledLeftOffset() const
	{
		return DivScale16(mLeftOffset, tex->xScale);
	}

	int GetScaledTopOffset() const
	{
		return DivScale16(mTopOffset, tex->yScale);
	}

	float GetScaledLeftOffsetFloat() const
	{
		return mLeftOffset / FIXED2FLOAT(tex->xScale);
	}

	float GetScaledTopOffsetFloat() const
	{
		return mTopOffset/ FIXED2FLOAT(tex->yScale);
	}

	// This is scaled size in floating point as needed by sprites
	float GetScaledWidthFloat() const
	{
		return mWidth / FIXED2FLOAT(tex->xScale);
	}

	float GetScaledHeightFloat() const
	{
		return mHeight / FIXED2FLOAT(tex->yScale);
	}

	// Get right/bottom UV coordinates for patch drawing
	float GetUL() const { return 0; }
	float GetVT() const { return 0; }
	float GetUR() const { return 1; }
	float GetVB() const { return 1; }
	float GetU(float upix) const { return upix/(float)mWidth; }
	float GetV(float vpix) const { return vpix/(float)mHeight; }

	float GetSpriteUL() const { return mSpriteU[0]; }
	float GetSpriteVT() const { return mSpriteV[0]; }
	float GetSpriteUR() const { return mSpriteU[1]; }
	float GetSpriteVB() const { return mSpriteV[1]; }



	bool GetTransparent() const
	{
		if (mBaseLayer->bIsTransparent == -1) 
		{
			if (!mBaseLayer->tex->bHasCanvas)
			{
				int w, h;
				unsigned char *buffer = CreateTexBuffer(0, w, h);
				delete [] buffer;
			}
			else
			{
				mBaseLayer->bIsTransparent = 0;
			}
		}
		return !!mBaseLayer->bIsTransparent;
	}

	static void DeleteAll();
	static void FlushAll();
	static FMaterial *ValidateTexture(FTexture * tex, bool expand);
	static FMaterial *ValidateTexture(FTextureID no, bool expand, bool trans);
	static void ClearLastTexture();

};

#endif
