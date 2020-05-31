
#ifndef __GL_MATERIAL_H
#define __GL_MATERIAL_H

#include "m_fixed.h"
#include "textures.h"

struct FRemapTable;
class IHardwareTexture;

struct MaterialLayerInfo
{
	FTexture* layerTexture;
	int scaleFlags;
	int clampflags;
};

//===========================================================================
// 
// this is the material class for OpenGL. 
//
//===========================================================================

class FMaterial
{
	private:
	TArray<MaterialLayerInfo> mTextureLayers; // the only layers allowed to scale are the brightmap and the glowmap.
	int mShaderIndex;
	int mLayerFlags = 0;
	int mScaleFlags;

public:
	FGameTexture *sourcetex;	// the owning texture. 

	FMaterial(FGameTexture *tex, int scaleflags);
	virtual ~FMaterial();
	int GetLayerFlags() const { return mLayerFlags; }
	int GetShaderIndex() const { return mShaderIndex; }
	int GetScaleFlags() const { return mScaleFlags; }
	virtual void DeleteDescriptors() { }
	FVector2 GetDetailScale() const
	{
		return sourcetex->GetDetailScale();
	}

	FGameTexture* Source() const
	{
		return sourcetex;
	}

	void AddTextureLayer(FTexture *tex, bool allowscale)
	{
		mTextureLayers.Push({ tex, allowscale });
	}

	int NumLayers() const
	{
		return mTextureLayers.Size();
	}

	IHardwareTexture *GetLayer(int i, int translation, MaterialLayerInfo **pLayer = nullptr) const;


	static FMaterial *ValidateTexture(FGameTexture * tex, int scaleflags, bool create = true);
	const TArray<MaterialLayerInfo> &GetLayerArray() const
	{
		return mTextureLayers;
	}
};

#endif


