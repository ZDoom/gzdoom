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

IHardwareTexture* CreateHardwareTexture();

//===========================================================================
//
// Constructor
//
//===========================================================================

FMaterial::FMaterial(FGameTexture * tx, bool expanded)
{
	mShaderIndex = SHADER_Default;
	sourcetex = tx;
	imgtex = tx->GetTexture();

	if (tx->GetUseType() == ETextureType::SWCanvas && static_cast<FWrapperTexture*>(imgtex)->GetColorFormat() == 0)
	{
		mShaderIndex = SHADER_Paletted;
	}
	else if (tx->isHardwareCanvas())
	{
		if (tx->GetShaderIndex() >= FIRST_USER_SHADER)
		{
			mShaderIndex = tx->GetShaderIndex();
		}
		// no brightmap for cameratexture
	}
	else
	{
		if (tx->isWarped())
		{
			mShaderIndex = tx->isWarped(); // This picks SHADER_Warp1 or SHADER_Warp2
		}
		else if (imgtex->Normal && imgtex->Specular)
		{
			for (auto &texture : { imgtex->Normal, imgtex->Specular })
			{
				mTextureLayers.Push(texture);
			}
			mShaderIndex = SHADER_Specular;
		}
		else if (imgtex->Normal && imgtex->Metallic && imgtex->Roughness && imgtex->AmbientOcclusion)
		{
			for (auto &texture : { imgtex->Normal, imgtex->Metallic, imgtex->Roughness, imgtex->AmbientOcclusion })
			{
				mTextureLayers.Push(texture);
			}
			mShaderIndex = SHADER_PBR;
		}

		// Note that these layers must present a valid texture even if not used, because empty TMUs in the shader are an undefined condition.
		imgtex->CreateDefaultBrightmap();
		if (imgtex->Brightmap)
		{
			mTextureLayers.Push(imgtex->Brightmap);
			mLayerFlags |= TEXF_Brightmap;
		}
		else	
		{ 
			mTextureLayers.Push(TexMan.ByIndex(1));
		}
		if (imgtex->Detailmap)
		{
			mTextureLayers.Push(imgtex->Detailmap);
			mLayerFlags |= TEXF_Detailmap;
		}
		else
		{
			mTextureLayers.Push(TexMan.ByIndex(1));
		}
		if (imgtex->Glowmap)
		{
			mTextureLayers.Push(imgtex->Glowmap);
			mLayerFlags |= TEXF_Glowmap;
		}
		else
		{
			mTextureLayers.Push(TexMan.ByIndex(1));
		}

		if (imgtex->shaderindex >= FIRST_USER_SHADER)
		{
			const UserShaderDesc &usershader = usershaders[imgtex->shaderindex - FIRST_USER_SHADER];
			if (usershader.shaderType == mShaderIndex) // Only apply user shader if it matches the expected material
			{
				for (auto &texture : imgtex->CustomShaderTextures)
				{
					if (texture == nullptr) continue;
					mTextureLayers.Push(texture);
				}
				mShaderIndex = tx->GetShaderIndex();
			}
		}
	}
	mExpanded = expanded;

	mTextureLayers.ShrinkToFit();
	imgtex->Material[expanded] = this;
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

IHardwareTexture *FMaterial::GetLayer(int i, int translation, FTexture **pLayer)
{
	FTexture *layer = i == 0 ? imgtex : mTextureLayers[i - 1];
	if (pLayer) *pLayer = layer;
	
	if (layer && layer->UseType!=ETextureType::Null)
	{
		IHardwareTexture *hwtex = layer->SystemTextures.GetHardwareTexture(translation, mExpanded);
		if (hwtex == nullptr) 
		{
			hwtex = CreateHardwareTexture();
			layer->SystemTextures.AddHardwareTexture(translation, mExpanded, hwtex);
 		}
		return hwtex;
	}
	return nullptr;
}

//==========================================================================
//
// Gets a texture from the texture manager and checks its validity for
// GL rendering. 
//
//==========================================================================

FMaterial * FMaterial::ValidateTexture(FGameTexture * gtex, bool expand, bool create)
{
	if (gtex && gtex->isValid())
	{
		auto tex = gtex->GetTexture();
		if (!tex->ShouldExpandSprite()) expand = false;

		FMaterial *hwtex = tex->Material[expand];
		if (hwtex == NULL && create)
		{
			hwtex = new FMaterial(gtex, expand);
		}
		return hwtex;
	}
	return NULL;
}

FMaterial * FMaterial::ValidateTexture(FTextureID no, bool expand, bool translate, bool create)
{
	return ValidateTexture(TexMan.GetGameTexture(no, translate), expand, create);
}


void DeleteMaterial(FMaterial* mat)
{
	delete mat;
}


