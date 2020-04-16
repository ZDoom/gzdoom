
#ifndef __GL_MATERIAL_H
#define __GL_MATERIAL_H

#include "m_fixed.h"
#include "textures.h"

struct FRemapTable;
class IHardwareTexture;

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
	int mScaleFlags;

public:
	FGameTexture *sourcetex;	// the owning texture. 
	FTexture* imgtex;			// the first layer's texture image - should be moved into the array

	FMaterial(FGameTexture *tex, int scaleflags);
	~FMaterial();
	int GetLayerFlags() const { return mLayerFlags; }
	int GetShaderIndex() const { return mShaderIndex; }
	int GetScaleFlags() const { return mScaleFlags; }

	FGameTexture* Source() const
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
		//ValidateTexture(tex, false);
		mTextureLayers.Push(tex);
	}

	int GetLayers() const
	{
		return mTextureLayers.Size() + 1;
	}

	IHardwareTexture *GetLayer(int i, int translation, FTexture **pLayer = nullptr) const;


	static FMaterial *ValidateTexture(FGameTexture * tex, int scaleflags, bool create = true);
	const TArray<FTexture*> &GetLayerArray() const
	{
		return mTextureLayers;
	}
};

#endif


