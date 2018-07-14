
#ifndef __GL_MATERIAL_H
#define __GL_MATERIAL_H

#include "m_fixed.h"
#include "textures/textures.h"
#include "i_system.h"
#include "r_defs.h"

EXTERN_CVAR(Bool, gl_precache)

struct FRemapTable;
class FTextureShader;
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



struct FTexCoordInfo
{
	int mRenderWidth;
	int mRenderHeight;
	int mWidth;
	FVector2 mScale;
	FVector2 mTempScale;
	bool mWorldPanning;

	float FloatToTexU(float v) const { return v / mRenderWidth; }
	float FloatToTexV(float v) const { return v / mRenderHeight; }
	float RowOffset(float ofs) const;
	float TextureOffset(float ofs) const;
	float TextureAdjustWidth() const;
};

//===========================================================================
// 
// this is the material class for OpenGL. 
//
//===========================================================================

class FMaterial
{
	friend class FRenderState;

	// This array is needed because not all textures are managed by the texture manager
	// but some code needs to discard all hardware dependent data attached to any created texture.
	// Font characters are not, for example.
	static TArray<FMaterial *> mMaterials;
	static int mMaxBound;

	IHardwareTexture *mBaseLayer;	
	TArray<FTexture*> mTextureLayers;
	int mShaderIndex;

	short mLeftOffset;
	short mTopOffset;
	short mWidth;
	short mHeight;
	short mRenderWidth;
	short mRenderHeight;
	bool mExpanded;
	bool mTrimResult;
	uint16_t trim[4];

	float mSpriteU[2], mSpriteV[2];
	FloatRect mSpriteRect;

	bool TrimBorders(uint16_t *rect);

public:
	FTexture *tex;
	FTexture *sourcetex;	// in case of redirection this is different from tex.
	
	FMaterial(FTexture *tex, bool forceexpand);
	~FMaterial();
	void SetSpriteRect();
	void Precache();
	void PrecacheList(SpriteHits &translations);
	IHardwareTexture * ValidateSysTexture(FTexture * tex, bool expand);
	void AddTextureLayer(FTexture *tex)
	{
		ValidateTexture(tex, false);
		mTextureLayers.Push(tex);
	}
	bool isMasked() const
	{
		return !!sourcetex->bMasked;
	}
	bool isExpanded() const
	{
		return mExpanded;
	}

	int GetLayers() const
	{
		return mTextureLayers.Size() + 1;
	}

	IHardwareTexture *GetLayer(int i, FTexture **pLayer = nullptr)
	{
		if (i == 0)
		{
			if (pLayer) *pLayer = tex;
			return mBaseLayer;
		}
		else
		{
			i--;
			FTexture *layer = mTextureLayers[i];
			if (pLayer) *pLayer = layer;
			return ValidateSysTexture(layer, isExpanded());
		}
	}

	void Clean(bool f);

	// Patch drawing utilities

	void GetSpriteRect(FloatRect * r) const
	{
		*r = mSpriteRect;
	}

	void GetTexCoordInfo(FTexCoordInfo *tci, float x, float y) const;

	void GetTexCoordInfo(FTexCoordInfo *tci, side_t *side, int texpos) const
	{
		GetTexCoordInfo(tci, (float)side->GetTextureXScale(texpos), (float)side->GetTextureYScale(texpos));
	}

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


	static void DeleteAll();
	static void FlushAll();
	static FMaterial *ValidateTexture(FTexture * tex, bool expand);
	static FMaterial *ValidateTexture(FTextureID no, bool expand, bool trans);
};

#endif


