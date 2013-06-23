
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

enum ETexUse
{
	GLUSE_PATCH,
	GLUSE_TEXTURE,
	GLUSE_SPRITE,
};


class FGLTexture //: protected WorldTextureInfo, protected PatchTextureInfo
{
	friend class FMaterial;
public:
	FTexture * tex;
	FTexture * hirestexture;
	char bIsTransparent;
	int HiresLump;

private:
	FHardwareTexture *gltexture[5];
	FHardwareTexture *glpatch;

	int currentwarp;
	int currentwarptime;

	bool bHasColorkey;		// only for hires
	bool bExpand;
	float AlphaThreshold;

	unsigned char * LoadHiresTexture(FTexture *hirescheck, int *width, int *height, int cm);
	BYTE *WarpBuffer(BYTE *buffer, int Width, int Height, int warp);

	FHardwareTexture *CreateTexture(int clampmode);
	//bool CreateTexture();
	bool CreatePatch();

	const FHardwareTexture *Bind(int texunit, int cm, int clamp, int translation, FTexture *hirescheck, int warp);
	const FHardwareTexture *BindPatch(int texunit, int cm, int translation, int warp);

public:
	FGLTexture(FTexture * tx, bool expandpatches);
	~FGLTexture();

	unsigned char * CreateTexBuffer(int cm, int translation, int & w, int & h, bool expand, FTexture *hirescheck, int warp);

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

	short LeftOffset[3];
	short TopOffset[3];
	short Width[3];
	short Height[3];
	short RenderWidth[2];
	short RenderHeight[2];

	float SpriteU[2], SpriteV[2];
	float spriteright, spritebottom;

	void SetupShader(int shaderindex, int &cm);
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

	void Bind(int cm, int clamp = 0, int translation = 0, int overrideshader = 0);
	void BindPatch(int cm, int translation = 0, int overrideshader = 0);

	unsigned char * CreateTexBuffer(int cm, int translation, int & w, int & h, bool expand = false, bool allowhires=true) const
	{
		return mBaseLayer->CreateTexBuffer(cm, translation, w, h, expand, allowhires? tex:NULL, 0);
	}

	void Clean(bool f)
	{
		mBaseLayer->Clean(f);
	}

	void BindToFrameBuffer();
	// Patch drawing utilities

	void GetRect(FloatRect *r, ETexUse i) const;
	void GetTexCoordInfo(FTexCoordInfo *tci, fixed_t x, fixed_t y) const;

	// This is scaled size in integer units as needed by walls and flats
	int TextureHeight(ETexUse i) const { return RenderHeight[i]; }
	int TextureWidth(ETexUse i) const { return RenderWidth[i]; }

	int GetAreas(FloatRect **pAreas) const;

	int GetWidth(ETexUse i) const
	{
		return Width[i];
	}

	int GetHeight(ETexUse i) const
	{
		return Height[i];
	}

	int GetLeftOffset(ETexUse i) const
	{
		return LeftOffset[i];
	}

	int GetTopOffset(ETexUse i) const
	{
		return TopOffset[i];
	}

	int GetScaledLeftOffset(ETexUse i) const
	{
		return DivScale16(LeftOffset[i], tex->xScale);
	}

	int GetScaledTopOffset(ETexUse i) const
	{
		return DivScale16(TopOffset[i], tex->yScale);
	}

	float GetScaledLeftOffsetFloat(ETexUse i) const
	{
		return LeftOffset[i] / FIXED2FLOAT(tex->xScale);
	}

	float GetScaledTopOffsetFloat(ETexUse i) const
	{
		return TopOffset[i] / FIXED2FLOAT(tex->yScale);
	}

	// This is scaled size in floating point as needed by sprites
	float GetScaledWidthFloat(ETexUse i) const
	{
		return Width[i] / FIXED2FLOAT(tex->xScale);
	}

	float GetScaledHeightFloat(ETexUse i) const
	{
		return Height[i] / FIXED2FLOAT(tex->yScale);
	}

	// Get right/bottom UV coordinates for patch drawing
	float GetUL() const { return 0; }
	float GetVT() const { return 0; }
	float GetUR() const { return spriteright; }
	float GetVB() const { return spritebottom; }
	float GetU(float upix) const { return upix/(float)Width[GLUSE_PATCH] * spriteright; }
	float GetV(float vpix) const { return vpix/(float)Height[GLUSE_PATCH] * spritebottom; }

	float GetSpriteUL() const { return SpriteU[0]; }
	float GetSpriteVT() const { return SpriteV[0]; }
	float GetSpriteUR() const { return SpriteU[1]; }
	float GetSpriteVB() const { return SpriteV[1]; }



	bool GetTransparent() const
	{
		if (mBaseLayer->bIsTransparent == -1) 
		{
			if (!mBaseLayer->tex->bHasCanvas)
			{
				int w, h;
				unsigned char *buffer = CreateTexBuffer(CM_DEFAULT, 0, w, h);
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
	static FMaterial *ValidateTexture(FTexture * tex);
	static FMaterial *ValidateTexture(FTextureID no, bool trans);

};

#endif
