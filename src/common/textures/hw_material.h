
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
	bool mExpanded;

public:
	FTexture *sourcetex;	// the owning texture. 
	FTexture* imgtex;		// the master texture for the backing image. Can be different from sourcetex and should not be in the layer array because that'd confuse the precacher.

	FMaterial(FTexture *tex, bool forceexpand);
	~FMaterial();
	int GetLayerFlags() const { return mLayerFlags; }
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
	void AddTextureLayer(FTexture *tex)
	{
		ValidateTexture(tex, false);
		mTextureLayers.Push(tex);
	}
	bool isExpanded() const
	{
		return mExpanded;
	}

	int GetLayers() const
	{
		return mTextureLayers.Size() + 1;
	}

	IHardwareTexture *GetLayer(int i, int translation, FTexture **pLayer = nullptr);


	static FMaterial *ValidateTexture(FTexture * tex, bool expand, bool create = true);
	static FMaterial *ValidateTexture(FTextureID no, bool expand, bool trans, bool create = true);
	const TArray<FTexture*> &GetLayerArray() const
	{
		return mTextureLayers;
	}
};

#endif


