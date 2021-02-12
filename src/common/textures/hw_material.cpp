// 
//---------------------------------------------------------------------------
//
// Copyright(C) 2004-2016 Christoph Oelckers
// All rights reserved.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/
//
//--------------------------------------------------------------------------
//

#include "filesystem.h"
#include "m_png.h"
#include "c_dispatch.h"
#include "hw_ihwtexture.h"
#include "hw_material.h"
#include "texturemanager.h"
#include "c_cvars.h"
#include "v_video.h"

static IHardwareTexture* (*layercallback)(int layer, int translation);

void FMaterial::SetLayerCallback(IHardwareTexture* (*cb)(int layer, int translation))
{
	layercallback = cb;
}

//===========================================================================
//
// Constructor
//
//===========================================================================

FMaterial::FMaterial(FGameTexture * tx, int scaleflags)
{
	mShaderIndex = SHADER_Default;
	sourcetex = tx;
	auto imgtex = tx->GetTexture();
	mTextureLayers.Push({ imgtex, scaleflags, -1 });

	if (tx->GetUseType() == ETextureType::SWCanvas && static_cast<FWrapperTexture*>(imgtex)->GetColorFormat() == 0)
	{
		mShaderIndex = SHADER_Paletted;
	}
	else if (scaleflags & CTF_Indexed)
	{
		mTextureLayers[0].scaleFlags |= CTF_Indexed;
		mShaderIndex = SHADER_Paletted;
	}
	else if (tx->isHardwareCanvas())
	{
		if (tx->GetShaderIndex() >= FIRST_USER_SHADER)
		{
			mShaderIndex = tx->GetShaderIndex();
		}
		mTextureLayers.Last().clampflags = CLAMP_CAMTEX;
		// no additional layers for cameratexture
	}
	else
	{
		if (tx->isWarped())
		{
			mShaderIndex = tx->isWarped(); // This picks SHADER_Warp1 or SHADER_Warp2
		}
		// Note that the material takes no ownership of the texture!
		else if (tx->Normal.get() && tx->Specular.get())
		{
			for (auto &texture : { tx->Normal.get(), tx->Specular.get() })
			{
				mTextureLayers.Push({ texture, 0, -1 });
			}
			mShaderIndex = SHADER_Specular;
		}
		else if (tx->Normal.get() && tx->Metallic.get() && tx->Roughness.get() && tx->AmbientOcclusion.get())
		{
			for (auto &texture : { tx->Normal.get(), tx->Metallic.get(), tx->Roughness.get(), tx->AmbientOcclusion.get() })
			{
				mTextureLayers.Push({ texture, 0, -1 });
			}
			mShaderIndex = SHADER_PBR;
		}

		// Note that these layers must present a valid texture even if not used, because empty TMUs in the shader are an undefined condition.
		tx->CreateDefaultBrightmap();
		auto placeholder = TexMan.GameByIndex(1);
		if (tx->Brightmap.get())
		{
			mTextureLayers.Push({ tx->Brightmap.get(), scaleflags, -1 });
			mLayerFlags |= TEXF_Brightmap;
		}
		else	
		{ 
			mTextureLayers.Push({ placeholder->GetTexture(), 0, -1 });
		}
		if (tx->Detailmap.get())
		{
			mTextureLayers.Push({ tx->Detailmap.get(), 0, CLAMP_NONE });
			mLayerFlags |= TEXF_Detailmap;
		}
		else
		{
			mTextureLayers.Push({ placeholder->GetTexture(), 0, -1 });
		}
		if (tx->Glowmap.get())
		{
			mTextureLayers.Push({ tx->Glowmap.get(), scaleflags, -1 });
			mLayerFlags |= TEXF_Glowmap;
		}
		else
		{
			mTextureLayers.Push({ placeholder->GetTexture(), 0, -1 });
		}

		auto index = tx->GetShaderIndex();
		if (index >= FIRST_USER_SHADER)
		{
			const UserShaderDesc &usershader = usershaders[index - FIRST_USER_SHADER];
			if (usershader.shaderType == mShaderIndex) // Only apply user shader if it matches the expected material
			{
				for (auto &texture : tx->CustomShaderTextures)
				{
					if (texture == nullptr) continue;
					mTextureLayers.Push({ texture.get(), 0 });	// scalability should be user-definable.
				}
				mShaderIndex = index;
			}
		}
	}
	mScaleFlags = scaleflags;

	mTextureLayers.ShrinkToFit();
	tx->Material[scaleflags] = this;
	if (tx->isHardwareCanvas()) tx->SetTranslucent(false);
}

//===========================================================================
//
// Destructor
//
//===========================================================================

FMaterial::~FMaterial()
{
}


//===========================================================================
//
//
//
//===========================================================================

IHardwareTexture* FMaterial::GetLayer(int i, int translation, MaterialLayerInfo** pLayer) const
{
	if ((mScaleFlags & CTF_Indexed) && i > 0 && layercallback)
	{
		static MaterialLayerInfo deflayer = { nullptr, 0, CLAMP_XY };
		if (i == 1 || i == 2)
		{
			if (pLayer) *pLayer = &deflayer;
			//This must be done with a user supplied callback because we cannot set up the rules for palette data selection here
			return layercallback(i, translation);
		}
	}
	else
	{
		auto& layer = mTextureLayers[i];
		if (pLayer) *pLayer = &layer;
		if (mScaleFlags & CTF_Indexed) translation = -1;
		if (layer.layerTexture) return layer.layerTexture->GetHardwareTexture(translation, layer.scaleFlags);
	}
	return nullptr;
}

//==========================================================================
//
// Gets a texture from the texture manager and checks its validity for
// GL rendering. 
//
//==========================================================================

FMaterial * FMaterial::ValidateTexture(FGameTexture * gtex, int scaleflags, bool create)
{
	if (gtex && gtex->isValid())
	{
		if (scaleflags & CTF_Indexed) scaleflags = CTF_Indexed;
		if (!gtex->expandSprites()) scaleflags &= ~CTF_Expand;

		FMaterial *hwtex = gtex->Material[scaleflags];
		if (hwtex == NULL && create)
		{
			hwtex = screen->CreateMaterial(gtex, scaleflags);
		}
		return hwtex;
	}
	return NULL;
}

void DeleteMaterial(FMaterial* mat)
{
	delete mat;
}


