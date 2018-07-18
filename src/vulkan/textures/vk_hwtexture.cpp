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

#if 0
//===========================================================================
// 
//	Creates the low level texture object
//
//===========================================================================

VKResult VkHardwareTexture::CreateTexture(VulkanDevice *vDevice, unsigned char * buffer, int w, int h, bool mipmap, int translation)
{
	int rh,rw;
	bool deletebuffer=false;

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

	auto res = tTex->vkTexture->Create(buffer, w, h, mipmap, vkTextureBytes)
	if (res != VK_SUCCESS)
	{
		delete tTex->vkTexture;
		tTex->vkTexture = nullptr;
	}
	return res;
}


//===========================================================================
// 
//	Creates a texture
//
//===========================================================================

VkHardwareTexture::VkHardwareTexture(bool nocompression) 
{
	forcenocompression = nocompression;

	vkDefTex.vkTexture = nullptr;
	vkDefTex.translation = 0;
	vkDefTex.mipmapped = false;
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
	for(unsigned int i=0;i<glTex_Translated.Size();i++)
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

unsigned int FHardwareTexture::Bind(int texunit, int translation, bool needmipmap)
{
	TranslatedTexture *pTex = GetTexID(translation);

	if (pTex->glTexID != 0)
	{
		if (lastbound[texunit] == pTex->glTexID) return pTex->glTexID;
		lastbound[texunit] = pTex->glTexID;
		if (texunit != 0) glActiveTexture(GL_TEXTURE0 + texunit);
		glBindTexture(GL_TEXTURE_2D, pTex->glTexID);
		// Check if we need mipmaps on a texture that was creted without them.
		if (needmipmap && !pTex->mipmapped && TexFilter[gl_texture_filter].mipmapping)
		{
			glGenerateMipmap(GL_TEXTURE_2D);
			pTex->mipmapped = true;
		}
		if (texunit != 0) glActiveTexture(GL_TEXTURE0);
		return pTex->glTexID;
	}
	return 0;
}

VkTexture *VkHardwareTexture::GetVkTexture(int translation)
{
	TranslatedTexture *pTex = GetTexID(translation);
	return pTex->vkTexture;
}

//===========================================================================
// 
//	Binds a texture to the renderer
//
//===========================================================================

bool FHardwareTexture::BindOrCreate(FTexture *tex, int texunit, int clampmode, int translation, int flags)
{
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

	bool needmipmap = (clampmode <= CLAMP_XY);

	// Texture has become invalid
	if (!tex->bHasCanvas && !tex->bWarped && tex->CheckModified(DefaultRenderStyle()))
	{
		Clean(true);
	}

	// Bind it to the system.
	if (!Bind(texunit, translation, needmipmap))
	{

		int w = 0, h = 0;

		// Create this texture
		unsigned char * buffer = nullptr;

		if (!tex->bHasCanvas)
		{
			buffer = tex->CreateTexBuffer(translation, w, h, flags | CTF_ProcessData);
		}
		else
		{
			w = tex->GetWidth();
			h = tex->GetHeight();
		}
		if (!CreateTexture(buffer, w, h, texunit, needmipmap, translation, "FHardwareTexture.BindOrCreate"))
		{
			// could not create texture
			delete[] buffer;
			return false;
		}
		delete[] buffer;
	}
	if (tex->bHasCanvas) static_cast<FCanvasTexture*>(tex)->NeedUpdate();
	GLRenderer->mSamplerManager->Bind(texunit, clampmode, 255);
	return true;
}

#endif