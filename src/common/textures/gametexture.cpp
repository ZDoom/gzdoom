/*
** gametexture.cpp
** The game-facing texture class.
**
**---------------------------------------------------------------------------
** Copyright 2004-2007 Randy Heit
** Copyright 2006-2020 Christoph Oelckers
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
**
*/

#include "printf.h"
#include "files.h"
#include "filesystem.h"

#include "textures.h"
#include "bitmap.h"
#include "colormatcher.h"
#include "c_dispatch.h"
#include "m_fixed.h"
#include "imagehelpers.h"
#include "image.h"
#include "formats/multipatchtexture.h"
#include "texturemanager.h"
#include "c_cvars.h"
#include "hw_material.h"

FTexture *CreateBrightmapTexture(FImageSource*);


FGameTexture::FGameTexture(FTexture* wrap, const char* name) : Name(name)
{
	if (wrap) Setup(wrap);
}

//==========================================================================
//
//
//
//==========================================================================

void FGameTexture::Setup(FTexture *wrap)
{
	Base = wrap;
	id.SetInvalid();
	TexelWidth = Base->GetWidth();
	DisplayWidth = (float)TexelWidth;
	TexelHeight = Base->GetHeight();
	DisplayHeight = (float)TexelHeight;
	auto img = Base->GetImage();
	if (img)
	{
		auto ofs = img->GetOffsets();
		LeftOffset[0] = LeftOffset[1] = ofs.first;
		TopOffset[0] = TopOffset[1] = ofs.second;
	}
	else
	{
		LeftOffset[0] = LeftOffset[1] = 
		TopOffset[0] = TopOffset[1] = 0;

	}
	ScaleX = ScaleY = 1.f;
}

//==========================================================================
//
//
//
//==========================================================================

FGameTexture::~FGameTexture()
{
	if (Base != nullptr)
	{
		FGameTexture* link = fileSystem.GetLinkedTexture(GetSourceLump());
		if (link == this) fileSystem.SetLinkedTexture(GetSourceLump(), nullptr);
	}
	if (SoftwareTexture != nullptr)
	{
		delete SoftwareTexture;
		SoftwareTexture = nullptr;
	}
	for (auto &mat : Material)
	{
		if (mat != nullptr) delete mat;
		mat = nullptr;
	}

}

//==========================================================================
//
//
//
//==========================================================================

bool FGameTexture::isUserContent() const
{
	int filenum = fileSystem.GetFileContainer(Base->GetSourceLump());
	return (filenum > fileSystem.GetMaxIwadNum());
}


//==========================================================================
//
// Search auto paths for extra material textures
//
//==========================================================================

void FGameTexture::AddAutoMaterials()
{
	struct AutoTextureSearchPath
	{
		const char* path;
		RefCountedPtr<FTexture> FGameTexture::* pointer;
	};
	struct AutoTextureSearchPath2
	{
		const char* path;
		RefCountedPtr<FTexture> FMaterialLayers::* pointer;
	};

	static AutoTextureSearchPath autosearchpaths[] =
	{
		{ "brightmaps/", &FGameTexture::Brightmap }, // For backwards compatibility, only for short names
	{ "materials/brightmaps/", &FGameTexture::Brightmap },
	};

	static AutoTextureSearchPath2 autosearchpaths2[] =
	{
	{ "materials/detailmaps/", &FMaterialLayers::Detailmap },
	{ "materials/glowmaps/", &FMaterialLayers::Glowmap },
	{ "materials/normalmaps/", &FMaterialLayers::Normal },
	{ "materials/specular/", &FMaterialLayers::Specular },
	{ "materials/metallic/", &FMaterialLayers::Metallic },
	{ "materials/roughness/", &FMaterialLayers::Roughness },
	{ "materials/ao/", &FMaterialLayers::AmbientOcclusion }
	};

	if (flags & GTexf_AutoMaterialsAdded) return; // do this only once

	bool fullname = !!(flags & GTexf_FullNameTexture);
	FString searchname = GetName();

	if (fullname)
	{
		auto dot = searchname.LastIndexOf('.');
		auto slash = searchname.LastIndexOf('/');
		if (dot > slash) searchname.Truncate(dot);
	}

	for (size_t i = 0; i < countof(autosearchpaths); i++)
	{
		auto& layer = autosearchpaths[i];
		if (this->*(layer.pointer) == nullptr)	// only if no explicit assignment had been done.
		{
			FStringf lookup("%s%s%s", layer.path, fullname ? "" : "auto/", searchname.GetChars());
			auto lump = fileSystem.CheckNumForFullName(lookup, false, ns_global, true);
			if (lump != -1)
			{
				auto bmtex = TexMan.FindGameTexture(fileSystem.GetFileFullName(lump), ETextureType::Any, FTextureManager::TEXMAN_TryAny);
				if (bmtex != nullptr)
				{
					this->*(layer.pointer) = bmtex->GetTexture();
				}
			}
		}
	}
	for (size_t i = 0; i < countof(autosearchpaths2); i++)
	{
		auto& layer = autosearchpaths2[i];
		if (!this->Layers || this->Layers.get()->*(layer.pointer) == nullptr)	// only if no explicit assignment had been done.
		{
			FStringf lookup("%s%s%s", layer.path, fullname ? "" : "auto/", searchname.GetChars());
			auto lump = fileSystem.CheckNumForFullName(lookup, false, ns_global, true);
			if (lump != -1)
			{
				auto bmtex = TexMan.FindGameTexture(fileSystem.GetFileFullName(lump), ETextureType::Any, FTextureManager::TEXMAN_TryAny);
				if (bmtex != nullptr)
				{
					if (this->Layers == nullptr) this->Layers = std::make_unique<FMaterialLayers>();
					this->Layers.get()->* (layer.pointer) = bmtex->GetTexture();
				}
			}
		}
	}
	flags |= GTexf_AutoMaterialsAdded;
}

//===========================================================================
// 
// Checks if the texture has a default brightmap and creates it if so
//
//===========================================================================
void FGameTexture::CreateDefaultBrightmap()
{
	auto tex = GetTexture();
	if (!(flags & GTexf_BrightmapChecked))
	{
		flags |= GTexf_BrightmapChecked;
		// Check for brightmaps
		if (tex->GetImage() && tex->GetImage()->UseGamePalette() && GPalette.HasGlobalBrightmap &&
			GetUseType() != ETextureType::Decal && GetUseType() != ETextureType::MiscPatch && GetUseType() != ETextureType::FontChar &&
			Brightmap == nullptr)
		{
			// May have one - let's check when we use this texture
			auto texbuf = tex->Get8BitPixels(false);
			const int white = ColorMatcher.Pick(255, 255, 255);

			int size = tex->GetWidth() * tex->GetHeight();
			for (int i = 0; i < size; i++)
			{
				if (GPalette.GlobalBrightmap.Remap[texbuf[i]] == white)
				{
					// Create a brightmap
					DPrintf(DMSG_NOTIFY, "brightmap created for texture '%s'\n", GetName().GetChars());
					Brightmap = CreateBrightmapTexture(tex->GetImage());
					return;
				}
			}
			// No bright pixels found
			DPrintf(DMSG_SPAMMY, "No bright pixels found in texture '%s'\n", GetName().GetChars());
		}
	}
}


//==========================================================================
//
// Calculates glow color for a texture
//
//==========================================================================

void FGameTexture::GetGlowColor(float* data)
{
	if (isGlowing() && GlowColor == 0)
	{
		auto buffer = Base->GetBgraBitmap(nullptr);
		GlowColor = averageColor((uint32_t*)buffer.GetPixels(), buffer.GetWidth() * buffer.GetHeight(), 153);

		// Black glow equals nothing so switch glowing off
		if (GlowColor == 0) flags &= ~GTexf_Glowing;
	}
	data[0] = GlowColor.r * (1 / 255.0f);
	data[1] = GlowColor.g * (1 / 255.0f);
	data[2] = GlowColor.b * (1 / 255.0f);
}

//===========================================================================
//
//
//
//===========================================================================

int FGameTexture::GetAreas(FloatRect** pAreas) const
{
	if (shaderindex == SHADER_Default)	// texture splitting can only be done if there's no attached effects
	{
		*pAreas = Base->areas;
		return Base->areacount;
	}
	else
	{
		return 0;
	}
}

//===========================================================================
// 
// Checks if a sprite may be expanded with an empty frame
//
//===========================================================================

bool FGameTexture::ShouldExpandSprite()
{
	if (expandSprite != -1) return expandSprite;
	// Only applicable to image textures with no shader effect.
	if (GetShaderIndex() != SHADER_Default || !dynamic_cast<FImageTexture*>(Base.get()))
	{
		expandSprite = false;
		return false;
	}
	if (Brightmap != NULL && (Base->GetWidth() != Brightmap->GetWidth() || Base->GetHeight() != Brightmap->GetHeight()))
	{
		// do not expand if the brightmap's physical size differs from the base.
		expandSprite = false;
		return false;
	}
	if (Layers && Layers->Glowmap != NULL && (Base->GetWidth() != Layers->Glowmap->GetWidth() || Base->GetHeight() != Layers->Glowmap->GetHeight()))
	{
		// same restriction for the glow map
		expandSprite = false;
		return false;
	}
	expandSprite = true;
	return true;
}

//===========================================================================
// 
// Sets up the sprite positioning data for this texture
//
//===========================================================================

void FGameTexture::SetupSpriteData()
{
	// Since this is only needed for real sprites it gets allocated on demand.
	// It also allocates from the image memory arena because it has the same lifetime and to reduce maintenance.
	if (spi == nullptr) spi = (SpritePositioningInfo*)ImageArena.Alloc(2 * sizeof(SpritePositioningInfo));
	for (int i = 0; i < 2; i++)
	{
		auto& spi = this->spi[i];
		spi.mSpriteU[0] = spi.mSpriteV[0] = 0.f;
		spi.mSpriteU[1] = spi.mSpriteV[1] = 1.f;
		spi.spriteWidth = GetTexelWidth();
		spi.spriteHeight = GetTexelHeight();

		if (i == 1 && ShouldExpandSprite())
		{
			spi.mTrimResult = Base->TrimBorders(spi.trim) && !GetNoTrimming();	// get the trim size before adding the empty frame
			spi.spriteWidth += 2;
			spi.spriteHeight += 2;
		}
	}
	SetSpriteRect();
}

//===========================================================================
//
// Set the sprite rectangle. This is separate because it may be called by a CVAR, too.
//
//===========================================================================

void FGameTexture::SetSpriteRect()
{

	if (!spi) return;
	auto leftOffset = GetTexelLeftOffset(r_spriteadjustHW);
	auto topOffset = GetTexelTopOffset(r_spriteadjustHW);

	float fxScale = GetScaleX();
	float fyScale = GetScaleY();

	for (int i = 0; i < 2; i++)
	{
		auto& spi = this->spi[i];

		// mSpriteRect is for positioning the sprite in the scene.
		spi.mSpriteRect.left = -leftOffset / fxScale;
		spi.mSpriteRect.top = -topOffset / fyScale;
		spi.mSpriteRect.width = spi.spriteWidth / fxScale;
		spi.mSpriteRect.height = spi.spriteHeight / fyScale;

		if (i == 1 && ShouldExpandSprite())
		{
			// a little adjustment to make sprites look better with texture filtering:
			// create a 1 pixel wide empty frame around them.

			int oldwidth = spi.spriteWidth - 2;
			int oldheight = spi.spriteHeight - 2;

			leftOffset += 1;
			topOffset += 1;

			// Reposition the sprite with the frame considered
			spi.mSpriteRect.left = -(float)leftOffset / fxScale;
			spi.mSpriteRect.top = -(float)topOffset / fyScale;
			spi.mSpriteRect.width = (float)spi.spriteWidth / fxScale;
			spi.mSpriteRect.height = (float)spi.spriteHeight / fyScale;

			if (spi.mTrimResult > 0)
			{
				spi.mSpriteRect.left += (float)spi.trim[0] / fxScale;
				spi.mSpriteRect.top += (float)spi.trim[1] / fyScale;

				spi.mSpriteRect.width -= float(oldwidth - spi.trim[2]) / fxScale;
				spi.mSpriteRect.height -= float(oldheight - spi.trim[3]) / fyScale;

				spi.mSpriteU[0] = (float)spi.trim[0] / (float)spi.spriteWidth;
				spi.mSpriteV[0] = (float)spi.trim[1] / (float)spi.spriteHeight;
				spi.mSpriteU[1] -= float(oldwidth - spi.trim[0] - spi.trim[2]) / (float)spi.spriteWidth;
				spi.mSpriteV[1] -= float(oldheight - spi.trim[1] - spi.trim[3]) / (float)spi.spriteHeight;
			}
		}
	}
}

//===========================================================================
//
// Cleans the attached hardware resources.
// This should only be used on textures which alter their content at run time
//or when the engine shuts down.
//
//===========================================================================

void FGameTexture::CleanHardwareData(bool full)
{
	if (full) Base->CleanHardwareTextures();
	for (auto mat : Material) if (mat) mat->DeleteDescriptors();
}


//-----------------------------------------------------------------------------
//
// Make sprite offset adjustment user-configurable per renderer.
//
//-----------------------------------------------------------------------------

CUSTOM_CVAR(Int, r_spriteadjust, 2, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	r_spriteadjustHW = !!(self & 2);
	r_spriteadjustSW = !!(self & 1);
	for (int i = 0; i < TexMan.NumTextures(); i++)
	{
		auto tex = TexMan.GetGameTexture(FSetTextureID(i));
		if (tex->GetTexelLeftOffset(0) != tex->GetTexelLeftOffset(1) || tex->GetTexelTopOffset(0) != tex->GetTexelTopOffset(1))
		{
			tex->SetSpriteRect();
		}
	}
}

//===========================================================================
//
// Coordinate helper.
// The only reason this is even needed is that many years ago someone
// was convinced that having per-texel panning on walls was a good idea.
// If it wasn't for this relatively useless feature the entire positioning
// code for wall textures could be a lot simpler.
//
//===========================================================================

//===========================================================================
//
// 
//
//===========================================================================

float FTexCoordInfo::RowOffset(float rowoffset) const
{
	float scale = fabsf(mScale.Y);
	if (scale == 1.f || mWorldPanning) return rowoffset;
	else return rowoffset / scale;
}

//===========================================================================
//
//
//
//===========================================================================

float FTexCoordInfo::TextureOffset(float textureoffset) const
{
	float scale = fabsf(mScale.X);
	if (scale == 1.f || mWorldPanning) return textureoffset;
	else return textureoffset / scale;
}

//===========================================================================
//
// Returns the size for which texture offset coordinates are used.
//
//===========================================================================

float FTexCoordInfo::TextureAdjustWidth() const
{
	if (mWorldPanning)
	{
		float tscale = fabsf(mTempScale.X);
		if (tscale == 1.f) return (float)mRenderWidth;
		else return mWidth / fabsf(tscale);
	}
	else return (float)mWidth;
}


//===========================================================================
//
// Retrieve texture coordinate info for per-wall scaling
//
//===========================================================================

void FTexCoordInfo::GetFromTexture(FGameTexture *tex, float x, float y, bool forceworldpanning)
{
	if (x == 1.f)
	{
		mRenderWidth = xs_RoundToInt(tex->GetDisplayWidth());
		mScale.X = tex->GetScaleX();
		mTempScale.X = 1.f;
	}
	else
	{
		float scale_x = x * tex->GetScaleX();
		mRenderWidth = xs_CeilToInt(tex->GetTexelWidth() / scale_x);
		mScale.X = scale_x;
		mTempScale.X = x;
	}

	if (y == 1.f)
	{
		mRenderHeight = xs_RoundToInt(tex->GetDisplayHeight());
		mScale.Y = tex->GetScaleY();
		mTempScale.Y = 1.f;
	}
	else
	{
		float scale_y = y * tex->GetScaleY();
		mRenderHeight = xs_CeilToInt(tex->GetTexelHeight() / scale_y);
		mScale.Y = scale_y;
		mTempScale.Y = y;
	}
	if (tex->isHardwareCanvas())
	{
		mScale.Y = -mScale.Y;
		mRenderHeight = -mRenderHeight;
	}
	mWorldPanning = tex->useWorldPanning() || forceworldpanning;
	mWidth = tex->GetTexelWidth();
}

