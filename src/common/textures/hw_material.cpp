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

// We really do not need to create more than one hardware texture per image source.
// However, the internal handling depends on FTexture for everything so this array maps each all textures sharing the same master texture that gets used in the layer array.
static TMap<FImageSource*, FTexture*> imageToMasterTexture;

static FTexture* GetMasterTexture(FTexture* tx)
{
	if (tx->GetUseType() == ETextureType::Null) return tx;	// Null textures never get redirected
	auto img = tx->GetImage();
	if (!img) return tx;	// this is not an image texture and represents itself.
	auto find = imageToMasterTexture.CheckKey(img);
	if (find) return *find;	// already got a master texture for this. Return it.
	imageToMasterTexture.Insert(img, tx);
	return tx;				// this is a newly added master texture.
}

//===========================================================================
//
// Constructor
//
//===========================================================================

FMaterial::FMaterial(FTexture * tx, bool expanded)
{
	mShaderIndex = SHADER_Default;
	sourcetex = tx;
	imgtex = GetMasterTexture(tx);

	if (tx->UseType == ETextureType::SWCanvas && static_cast<FWrapperTexture*>(tx)->GetColorFormat() == 0)
	{
		mShaderIndex = SHADER_Paletted;
	}
	else if (tx->isHardwareCanvas())
	{
		if (tx->shaderindex >= FIRST_USER_SHADER)
		{
			mShaderIndex = tx->shaderindex;
		}
		// no brightmap for cameratexture
	}
	else
	{
		if (tx->isWarped())
		{
			mShaderIndex = tx->isWarped(); // This picks SHADER_Warp1 or SHADER_Warp2
		}
		else if (tx->Normal && tx->Specular)
		{
			for (auto &texture : { tx->Normal, tx->Specular })
			{
				mTextureLayers.Push(GetMasterTexture(texture));
			}
			mShaderIndex = SHADER_Specular;
		}
		else if (tx->Normal && tx->Metallic && tx->Roughness && tx->AmbientOcclusion)
		{
			for (auto &texture : { tx->Normal, tx->Metallic, tx->Roughness, tx->AmbientOcclusion })
			{
				mTextureLayers.Push(GetMasterTexture(texture));
			}
			mShaderIndex = SHADER_PBR;
		}

		// Note that these layers must present a valid texture even if not used, because empty TMUs in the shader are an undefined condition.
		tx->CreateDefaultBrightmap();
		if (tx->Brightmap)
		{
			mTextureLayers.Push(GetMasterTexture(tx->Brightmap));
			mLayerFlags |= TEXF_Brightmap;
		}
		else	
		{ 
			mTextureLayers.Push(TexMan.ByIndex(1));
		}
		if (tx->Detailmap)
		{
			mTextureLayers.Push(GetMasterTexture(tx->Detailmap));
			mLayerFlags |= TEXF_Detailmap;
		}
		else
		{
			mTextureLayers.Push(TexMan.ByIndex(1));
		}
		if (tx->Glowmap)
		{
			mTextureLayers.Push(GetMasterTexture(tx->Glowmap));
			mLayerFlags |= TEXF_Glowmap;
		}
		else
		{
			mTextureLayers.Push(TexMan.ByIndex(1));
		}

		if (tx->shaderindex >= FIRST_USER_SHADER)
		{
			const UserShaderDesc &usershader = usershaders[tx->shaderindex - FIRST_USER_SHADER];
			if (usershader.shaderType == mShaderIndex) // Only apply user shader if it matches the expected material
			{
				for (auto &texture : tx->CustomShaderTextures)
				{
					if (texture == nullptr) continue;
					mTextureLayers.Push(GetMasterTexture(texture));
				}
				mShaderIndex = tx->shaderindex;
			}
		}
	}
	mWidth = tx->GetTexelWidth();
	mHeight = tx->GetTexelHeight();
	mLeftOffset = tx->GetLeftOffset(0);	// These only get used by decals and decals should not use renderer-specific offsets.
	mTopOffset = tx->GetTopOffset(0);
	mRenderWidth = tx->GetScaledWidth();
	mRenderHeight = tx->GetScaledHeight();
	mSpriteU[0] = mSpriteV[0] = 0.f;
	mSpriteU[1] = mSpriteV[1] = 1.f;

	mExpanded = expanded;
	if (expanded)
	{
		int oldwidth = mWidth;
		int oldheight = mHeight;

		mTrimResult = TrimBorders(trim);	// get the trim size before adding the empty frame
		mWidth += 2;
		mHeight += 2;
		mRenderWidth = mRenderWidth * mWidth / oldwidth;
		mRenderHeight = mRenderHeight * mHeight / oldheight;

	}
	SetSpriteRect();

	mTextureLayers.ShrinkToFit();
	tx->Material[expanded] = this;
	if (tx->isHardwareCanvas()) tx->bTranslucent = 0;
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
// Set the sprite rectangle
//
//===========================================================================

void FMaterial::SetSpriteRect()
{
	auto leftOffset = sourcetex->GetLeftOffsetHW();
	auto topOffset = sourcetex->GetTopOffsetHW();

	float fxScale = (float)sourcetex->Scale.X;
	float fyScale = (float)sourcetex->Scale.Y;

	// mSpriteRect is for positioning the sprite in the scene.
	mSpriteRect.left = -leftOffset / fxScale;
	mSpriteRect.top = -topOffset / fyScale;
	mSpriteRect.width = mWidth / fxScale;
	mSpriteRect.height = mHeight / fyScale;

	if (mExpanded)
	{
		// a little adjustment to make sprites look better with texture filtering:
		// create a 1 pixel wide empty frame around them.

		int oldwidth = mWidth - 2;
		int oldheight = mHeight - 2;

		leftOffset += 1;
		topOffset += 1;

		// Reposition the sprite with the frame considered
		mSpriteRect.left = -leftOffset / fxScale;
		mSpriteRect.top = -topOffset / fyScale;
		mSpriteRect.width = mWidth / fxScale;
		mSpriteRect.height = mHeight / fyScale;

		if (mTrimResult)
		{
			mSpriteRect.left += trim[0] / fxScale;
			mSpriteRect.top += trim[1] / fyScale;

			mSpriteRect.width -= (oldwidth - trim[2]) / fxScale;
			mSpriteRect.height -= (oldheight - trim[3]) / fyScale;

			mSpriteU[0] = trim[0] / (float)mWidth;
			mSpriteV[0] = trim[1] / (float)mHeight;
			mSpriteU[1] -= (oldwidth - trim[0] - trim[2]) / (float)mWidth;
			mSpriteV[1] -= (oldheight - trim[1] - trim[3]) / (float)mHeight;
		}
	}
}


//===========================================================================
// 
//  Finds empty space around the texture. 
//  Used for sprites that got placed into a huge empty frame.
//
//===========================================================================

bool FMaterial::TrimBorders(uint16_t *rect)
{

	auto texbuffer = sourcetex->CreateTexBuffer(0);
	int w = texbuffer.mWidth;
	int h = texbuffer.mHeight;
	auto Buffer = texbuffer.mBuffer;

	if (texbuffer.mBuffer == nullptr) 
	{
		return false;
	}
	if (w != mWidth || h != mHeight)
	{
		// external Hires replacements cannot be trimmed.
		return false;
	}

	int size = w*h;
	if (size == 1)
	{
		// nothing to be done here.
		rect[0] = 0;
		rect[1] = 0;
		rect[2] = 1;
		rect[3] = 1;
		return true;
	}
	int first, last;

	for(first = 0; first < size; first++)
	{
		if (Buffer[first*4+3] != 0) break;
	}
	if (first >= size)
	{
		// completely empty
		rect[0] = 0;
		rect[1] = 0;
		rect[2] = 1;
		rect[3] = 1;
		return true;
	}

	for(last = size-1; last >= first; last--)
	{
		if (Buffer[last*4+3] != 0) break;
	}

	rect[1] = first / w;
	rect[3] = 1 + last/w - rect[1];

	rect[0] = 0;
	rect[2] = w;

	unsigned char *bufferoff = Buffer + (rect[1] * w * 4);
	h = rect[3];

	for(int x = 0; x < w; x++)
	{
		for(int y = 0; y < h; y++)
		{
			if (bufferoff[(x+y*w)*4+3] != 0) goto outl;
		}
		rect[0]++;
	}
outl:
	rect[2] -= rect[0];

	for(int x = w-1; rect[2] > 1; x--)
	{
		for(int y = 0; y < h; y++)
		{
			if (bufferoff[(x+y*w)*4+3] != 0) 
			{
				return true;
			}
		}
		rect[2]--;
	}
	return true;
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

//===========================================================================
//
//
//
//===========================================================================

int FMaterial::GetAreas(FloatRect **pAreas) const
{
	if (mShaderIndex == SHADER_Default)	// texture splitting can only be done if there's no attached effects
	{
		*pAreas = sourcetex->areas;
		return sourcetex->areacount;
	}
	else
	{
		return 0;
	}
}

//==========================================================================
//
// Gets a texture from the texture manager and checks its validity for
// GL rendering. 
//
//==========================================================================

FMaterial * FMaterial::ValidateTexture(FTexture * tex, bool expand, bool create)
{
again:
	if (tex	&& tex->isValid())
	{
		if (tex->bNoExpand) expand = false;

		FMaterial *hwtex = tex->Material[expand];
		if (hwtex == NULL && create)
		{
			if (expand)
			{
				if (tex->isWarped() || tex->isHardwareCanvas() || tex->shaderindex >= FIRST_USER_SHADER || tex->shaderindex == SHADER_Specular || tex->shaderindex == SHADER_PBR)
				{
					tex->bNoExpand = true;
					goto again;
				}
				if (tex->Brightmap != NULL &&
					(tex->GetTexelWidth() != tex->Brightmap->GetTexelWidth() ||
					tex->GetTexelHeight() != tex->Brightmap->GetTexelHeight())
					)
				{
					// do not expand if the brightmap's size differs.
					tex->bNoExpand = true;
					goto again;
				}
			}
			hwtex = new FMaterial(tex, expand);
		}
		return hwtex;
	}
	return NULL;
}

FMaterial * FMaterial::ValidateTexture(FTextureID no, bool expand, bool translate, bool create)
{
	return ValidateTexture(TexMan.GetTexture(no, translate), expand, create);
}


void DeleteMaterial(FMaterial* mat)
{
	delete mat;
}


//-----------------------------------------------------------------------------
//
// Make sprite offset adjustment user-configurable per renderer.
//
//-----------------------------------------------------------------------------

extern int r_spriteadjustSW, r_spriteadjustHW;

CUSTOM_CVAR(Int, r_spriteadjust, 2, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	r_spriteadjustHW = !!(self & 2);
	r_spriteadjustSW = !!(self & 1);
	for (int i = 0; i < TexMan.NumTextures(); i++)
	{
		auto tex = TexMan.GetTexture(FSetTextureID(i));
		if (tex->GetTexelLeftOffset(0) != tex->GetTexelLeftOffset(1) || tex->GetTexelTopOffset(0) != tex->GetTexelTopOffset(1))
		{
			for (int i = 0; i < 2; i++)
			{
				auto mat = tex->GetMaterial(i);
				if (mat != nullptr) mat->SetSpriteRect();
			}
		}

	}
}
