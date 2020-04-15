
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
	bool mExpanded;

public:
	FGameTexture *sourcetex;	// the owning texture. 
	FTexture* imgtex;			// the first layer's texture image - should be moved into the array

	FMaterial(FGameTexture *tex, bool forceexpand);
	~FMaterial();
	int GetLayerFlags() const { return mLayerFlags; }
	int GetShaderIndex() const { return mShaderIndex; }

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
	bool isExpanded() const
	{
		return mExpanded;
	}

	int GetLayers() const
	{
		return mTextureLayers.Size() + 1;
	}

	IHardwareTexture *GetLayer(int i, int translation, FTexture **pLayer = nullptr);


	static FMaterial *ValidateTexture(FGameTexture * tex, bool expand, bool create = true);
	static FMaterial *ValidateTexture(FTextureID no, bool expand, bool trans, bool create = true);
	const TArray<FTexture*> &GetLayerArray() const
	{
		return mTextureLayers;
	}
};

#endif


