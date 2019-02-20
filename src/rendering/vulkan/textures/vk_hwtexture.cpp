// 
//---------------------------------------------------------------------------
//
// Copyright(C) 2018 Christoph Oelckers
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
/*
** Container class for the various translations a texture can have.
**
*/

#include "templates.h"
#include "c_cvars.h"
#include "r_data/colormaps.h"
#include "hwrenderer/textures/hw_material.h"

#include "hwrenderer/utility/hw_cvars.h"
#include "vk_hwtexture.h"

//===========================================================================
// 
//	Creates the low level texture object
//
//===========================================================================

VkResult VkHardwareTexture::CreateTexture(unsigned char * buffer, int w, int h, bool mipmap, int translation)
{
#if 0
	int rh,rw;
	bool deletebuffer=false;
	auto tTex = GetTexID(translation);

	// We cannot determine if the old texture is still needed, so if something is trying to recreate a still existing texture this must fail.
	// Normally it should never occur that a texture needs to be recreated in the middle of a frame.
	if (tTex->vkTexture != nullptr) return VK_ERROR_INITIALIZATION_FAILED;

	tTex->vkTexture = new VkTexture(vDevice);;

	rw = vDevice->GetTexDimension(w);
	rh = vDevice->GetTexDimension(h);
	if (rw < w || rh < h)
	{
		// The texture is larger than what the hardware can handle so scale it down.
		unsigned char * scaledbuffer=(unsigned char *)calloc(4,rw * (rh+1));
		if (scaledbuffer)
		{
			ResizeTexture(w, h, rw, rh, buffer, scaledbuffer);
			deletebuffer=true;
			buffer=scaledbuffer;
		}
	}

	auto res = tTex->vkTexture->Create(buffer, w, h, mipmap, vkTextureBytes);
	if (res != VK_SUCCESS)
	{
		delete tTex->vkTexture;
		tTex->vkTexture = nullptr;
	}
	return res;
#else
	return VK_ERROR_INITIALIZATION_FAILED; 
#endif
}

//===========================================================================
// 
//	Creates a texture
//
//===========================================================================

VkHardwareTexture::VkHardwareTexture(VulkanDevice *dev, bool nocompression)
{
	forcenocompression = nocompression;

	vkDefTex.vkTexture = nullptr;
	vkDefTex.translation = 0;
	vkDefTex.mipmapped = false;
	vDevice = dev;
}


//===========================================================================
// 
//	Deletes a texture id and unbinds it from the texture units
//
//===========================================================================

void VkHardwareTexture::TranslatedTexture::Delete()
{
	if (vkTexture != nullptr) 
	{
		delete vkTexture;
		vkTexture = nullptr;
		mipmapped = false;
	}
}

//===========================================================================
// 
//	Frees all associated resources
//
//===========================================================================

void VkHardwareTexture::Clean(bool all)
{
	int cm_arraysize = CM_FIRSTSPECIALCOLORMAP + SpecialColormaps.Size();

	if (all)
	{
		vkDefTex.Delete();
	}
	for(unsigned int i=0;i<vkTex_Translated.Size();i++)
	{
		vkTex_Translated[i].Delete();
	}
	vkTex_Translated.Clear();
}

//===========================================================================
// 
// Deletes all allocated resources and considers translations
// This will only be called for sprites
//
//===========================================================================

void VkHardwareTexture::CleanUnused(SpriteHits &usedtranslations)
{
	if (usedtranslations.CheckKey(0) == nullptr)
	{
		vkDefTex.Delete();
	}
	for (int i = vkTex_Translated.Size()-1; i>= 0; i--)
	{
		if (usedtranslations.CheckKey(vkTex_Translated[i].translation) == nullptr)
		{
			vkTex_Translated[i].Delete();
			vkTex_Translated.Delete(i);
		}
	}
}

//===========================================================================
// 
//	Destroys the texture
//
//===========================================================================
VkHardwareTexture::~VkHardwareTexture() 
{ 
	Clean(true); 
}


//===========================================================================
// 
//	Gets a texture ID address and validates all required data
//
//===========================================================================

VkHardwareTexture::TranslatedTexture *VkHardwareTexture::GetTexID(int translation)
{
	if (translation == 0)
	{
		return &vkDefTex;
	}

	// normally there aren't more than very few different 
	// translations here so this isn't performance critical.
	// Maps only start to pay off for larger amounts of elements.
	for (auto &tex : vkTex_Translated)
	{
		if (tex.translation == translation)
		{
			return &tex;
		}
	}

	int add = vkTex_Translated.Reserve(1);
	vkTex_Translated[add].translation = translation;
	vkTex_Translated[add].vkTexture = nullptr;
	vkTex_Translated[add].mipmapped = false;
	return &vkTex_Translated[add];
}

//===========================================================================
// 
//
//
//===========================================================================

VkTexture *VkHardwareTexture::GetVkTexture(FTexture *tex, int translation, bool needmipmap, int flags)
{
#if 0
	int usebright = false;

	if (translation <= 0)
	{
		translation = -translation;
	}
	else
	{
		auto remap = TranslationToTable(translation);
		translation = remap == nullptr ? 0 : remap->GetUniqueIndex();
	}

	TranslatedTexture *pTex = GetTexID(translation);

	if (pTex->vkTexture == nullptr)
	{
		int w, h;
		auto buffer = tex->CreateTexBuffer(translation, w, h, flags | CTF_ProcessData);
		auto res = CreateTexture(buffer, w, h, needmipmap, translation);
		delete[] buffer;
	}
	return pTex->vkTexture;
#endif
	return nullptr;
}
