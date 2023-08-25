
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
	MaterialLayerSampling layerFiltering;
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
	static void SetLayerCallback(IHardwareTexture* (*layercallback)(int layer, int translation));

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

	void ClearLayers()
	{
		mTextureLayers.Resize(1);
	}

	void AddTextureLayer(FTexture *tex, bool allowscale, MaterialLayerSampling filter)
	{
		mTextureLayers.Push({ tex, allowscale, -1, filter });
	}

	int NumLayers() const
	{
		return mTextureLayers.Size();
	}

	IHardwareTexture *GetLayer(int i, int translation, MaterialLayerInfo **pLayer = nullptr) const;
	
	MaterialLayerSampling GetLayerFilter(int index) const
	{
		return mTextureLayers[index].layerFiltering;
	}

	static FMaterial *ValidateTexture(FGameTexture * tex, int scaleflags, bool create = true);
	const TArray<MaterialLayerInfo> &GetLayerArray() const
	{
		return mTextureLayers;
	}
};

#endif


