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

#include "gl/system/gl_system.h"
#include "w_wad.h"
#include "m_png.h"
#include "sbar.h"
#include "gi.h"
#include "cmdlib.h"
#include "c_dispatch.h"
#include "stats.h"
#include "r_utility.h"
#include "templates.h"
#include "sc_man.h"
#include "r_data/renderstyle.h"
#include "colormatcher.h"
#include "textures/warpbuffer.h"
#include "textures/bitmap.h"

//#include "gl/gl_intern.h"

#include "gl/system/gl_interface.h"
#include "gl/system/gl_framebuffer.h"
#include "gl/renderer/gl_lightdata.h"
#include "gl/renderer/gl_renderer.h"
#include "gl/textures/gl_material.h"
#include "gl/textures/gl_samplers.h"
#include "gl/shaders/gl_shader.h"

EXTERN_CVAR(Bool, gl_render_precise)
EXTERN_CVAR(Int, gl_lightmode)
EXTERN_CVAR(Bool, gl_precache)
EXTERN_CVAR(Bool, gl_texture_usehires)

//===========================================================================
//
// The GL texture maintenance class
//
//===========================================================================

//===========================================================================
//
// Constructor
//
//===========================================================================
FGLTexture::FGLTexture(FTexture * tx, bool expandpatches)
{
	assert(tx->gl_info.SystemTexture[expandpatches] == NULL);
	tex = tx;

	mHwTexture = NULL;
	lastSampler = 254;
	lastTranslation = -1;
	tex->gl_info.SystemTexture[expandpatches] = this;
}

//===========================================================================
//
// Constructor 2 for the SW framebuffer which provides its own hardware texture.
//
//===========================================================================
FGLTexture::FGLTexture(FTexture * tx, FHardwareTexture *hwtex)
{
	assert(tx->gl_info.SystemTexture[0] == NULL);
	tex = tx;

	mHwTexture = hwtex;
	lastSampler = 254;
	lastTranslation = -1;
	tex->gl_info.SystemTexture[0] = this;
}

//===========================================================================
//
// Destructor
//
//===========================================================================

FGLTexture::~FGLTexture()
{
	Clean(true);
	for (auto & i : tex->gl_info.SystemTexture) if (i == this) i = nullptr;
}

//===========================================================================
// 
//	Deletes all allocated resources
//
//===========================================================================

void FGLTexture::Clean(bool all)
{
	if (mHwTexture != nullptr) 
	{
		if (!all) mHwTexture->Clean(false);
		else
		{
			delete mHwTexture;
			mHwTexture = nullptr;
		}

		lastSampler = 253;
		lastTranslation = -1;
	}
}


void FGLTexture::CleanUnused(SpriteHits &usedtranslations)
{
	if (mHwTexture != nullptr)
	{
		mHwTexture->CleanUnused(usedtranslations);
		lastSampler = 253;
		lastTranslation = -1;
	}
}


//===========================================================================
// 
//	Create hardware texture for world use
//
//===========================================================================

FHardwareTexture *FGLTexture::CreateHwTexture()
{
	if (tex->UseType==ETextureType::Null) return NULL;		// Cannot register a NULL texture
	if (mHwTexture == NULL)
	{
		mHwTexture = new FHardwareTexture(tex->bNoCompress);
	}
	return mHwTexture; 
}

//===========================================================================
// 
//	Binds a texture to the renderer
//
//===========================================================================

bool FGLTexture::Bind(int texunit, int clampmode, int translation, int flags)
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

	FHardwareTexture *hwtex = CreateHwTexture();

	if (hwtex)
	{
		// Texture has become invalid
		if ((!tex->bHasCanvas && (!tex->bWarped || gl.legacyMode)) && tex->CheckModified(DefaultRenderStyle()))
		{
			Clean(true);
			hwtex = CreateHwTexture();
		}

		// Bind it to the system.
		if (!hwtex->Bind(texunit, translation, needmipmap))
		{
			
			int w=0, h=0;

			// Create this texture
			unsigned char * buffer = nullptr;
			
			if (!tex->bHasCanvas)
			{
				buffer = tex->CreateTexBuffer(translation, w, h, flags | CTF_ProcessData);
				if (tex->bWarped && gl.legacyMode && w*h <= 256*256)	// do not software-warp larger textures, especially on the old systems that still need this fallback.
				{
					// need to do software warping
					FWarpTexture *wt = static_cast<FWarpTexture*>(tex);
					unsigned char *warpbuffer = new unsigned char[w*h*4];
					WarpBuffer((uint32_t*)warpbuffer, (const uint32_t*)buffer, w, h, wt->WidthOffsetMultiplier, wt->HeightOffsetMultiplier, screen->FrameTime, wt->Speed, tex->bWarped);
					delete[] buffer;
					buffer = warpbuffer;
					wt->GenTime[0] = screen->FrameTime;
				}
			}
			else
			{
				w = tex->GetWidth();
				h = tex->GetHeight();
			}
			if (!hwtex->CreateTexture(buffer, w, h, texunit, needmipmap, translation, "FGLTexture.Bind")) 
			{
				// could not create texture
				delete[] buffer;
				return false;
			}
			delete[] buffer;
		}
		if (tex->bHasCanvas) static_cast<FCanvasTexture*>(tex)->NeedUpdate();
		if (translation != lastTranslation) lastSampler = 254;
		if (lastSampler != clampmode)
			lastSampler = GLRenderer->mSamplerManager->Bind(texunit, clampmode, lastSampler);
		lastTranslation = translation;
		return true; 
	}
	return false;
}

//===========================================================================
//
//
//
//===========================================================================

float FTexCoordInfo::RowOffset(float rowoffset) const
{
	float tscale = fabs(mTempScale.Y);
	float scale = fabs(mScale.Y);

	if (tscale == 1.f)
	{
		if (scale == 1.f || mWorldPanning) return rowoffset;
		else return rowoffset / scale;
	}
	else
	{
		if (mWorldPanning) return rowoffset / tscale;
		else return rowoffset / scale;
	}
}

//===========================================================================
//
//
//
//===========================================================================

float FTexCoordInfo::TextureOffset(float textureoffset) const
{
	float tscale = fabs(mTempScale.X);
	float scale = fabs(mScale.X);
	if (tscale == 1.f)
	{
		if (scale == 1.f || mWorldPanning) return textureoffset;
		else return textureoffset / scale;
	}
	else
	{
		if (mWorldPanning) return textureoffset / tscale;
		else return textureoffset / scale;
	}
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
		float tscale = fabs(mTempScale.X);
		if (tscale == 1.f) return mRenderWidth;
		else return mWidth / fabs(tscale);
	}
	else return mWidth;
}



//===========================================================================
//
//
//
//===========================================================================
FGLTexture * FMaterial::ValidateSysTexture(FTexture * tex, bool expand)
{
	if (tex	&& tex->UseType!=ETextureType::Null)
	{
		FGLTexture *gltex = tex->gl_info.SystemTexture[expand];
		if (gltex == NULL) 
		{
			gltex = new FGLTexture(tex, expand);
		}
		return gltex;
	}
	return NULL;
}

//===========================================================================
//
// Constructor
//
//===========================================================================
TArray<FMaterial *> FMaterial::mMaterials;
int FMaterial::mMaxBound;

FMaterial::FMaterial(FTexture * tx, bool expanded)
{
	mShaderIndex = SHADER_Default;
	tex = tx;

	// TODO: apply custom shader object here
	/* if (tx->CustomShaderDefinition)
	{
	}
	else
	*/
	if (tx->UseType == ETextureType::SWCanvas && tx->WidthBits == 0)
	{
		mShaderIndex = SHADER_Paletted;
	}
	else if (tx->bWarped)
	{
		mShaderIndex = tx->bWarped; // This picks SHADER_Warp1 or SHADER_Warp2
		tx->shaderspeed = static_cast<FWarpTexture*>(tx)->GetSpeed();
	}
	else if (tx->bHasCanvas)
	{
		if (tx->shaderindex >= FIRST_USER_SHADER)
		{
			mShaderIndex = tx->shaderindex;
		}
		// no brightmap for cameratexture
	}
	else
	{
		if (tx->shaderindex >= FIRST_USER_SHADER)
		{
			mShaderIndex = tx->shaderindex;
		}
		else
		{
			if (tx->Normal && tx->Specular)
			{
				for (auto &texture : { tx->Normal, tx->Specular })
				{
					ValidateSysTexture(texture, expanded);
					mTextureLayers.Push({ texture, false });
				}
				mShaderIndex = SHADER_Specular;
			}
			else if (tx->Normal && tx->Metallic && tx->Roughness && tx->AmbientOcclusion)
			{
				for (auto &texture : { tx->Normal, tx->Metallic, tx->Roughness, tx->AmbientOcclusion })
				{
					ValidateSysTexture(texture, expanded);
					mTextureLayers.Push({ texture, false });
				}
				mShaderIndex = SHADER_PBR;
			}

			tx->CreateDefaultBrightmap();
			if (tx->Brightmap != NULL)
			{
				ValidateSysTexture(tx->Brightmap, expanded);
				FTextureLayer layer = {tx->Brightmap, false};
				mTextureLayers.Push(layer);
				if (mShaderIndex == SHADER_Specular)
					mShaderIndex = SHADER_SpecularBrightmap;
				else if (mShaderIndex == SHADER_PBR)
					mShaderIndex = SHADER_PBRBrightmap;
				else
					mShaderIndex = SHADER_Brightmap;
			}
		}
	}
	mBaseLayer = ValidateSysTexture(tx, expanded);


	mWidth = tx->GetWidth();
	mHeight = tx->GetHeight();
	mLeftOffset = tx->GetLeftOffset(0);	// These only get used by decals and decals should not use renderer-specific offsets.
	mTopOffset = tx->GetTopOffset(0);
	mRenderWidth = tx->GetScaledWidth();
	mRenderHeight = tx->GetScaledHeight();
	mSpriteU[0] = mSpriteV[0] = 0.f;
	mSpriteU[1] = mSpriteV[1] = 1.f;

	FTexture *basetex = (tx->bWarped && gl.legacyMode)? tx : tx->GetRedirect(false);
	// allow the redirect only if the texture is not expanded or the scale matches.
	if (!expanded || (tx->Scale.X == basetex->Scale.X && tx->Scale.Y == basetex->Scale.Y))
	{
		mBaseLayer = ValidateSysTexture(basetex, expanded);
	}

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
	mMaxBound = -1;
	mMaterials.Push(this);
	tx->gl_info.Material[expanded] = this;
	if (tx->bHasCanvas) tx->bTranslucent = 0;
}

//===========================================================================
//
// Destructor
//
//===========================================================================

FMaterial::~FMaterial()
{
	for(unsigned i=0;i<mMaterials.Size();i++)
	{
		if (mMaterials[i]==this) 
		{
			mMaterials.Delete(i);
			break;
		}
	}

}

//===========================================================================
//
// Set the sprite rectangle
//
//===========================================================================

void FMaterial::SetSpriteRect()
{
	auto leftOffset = tex->GetLeftOffsetHW();
	auto topOffset = tex->GetTopOffsetHW();

	float fxScale = tex->Scale.X;
	float fyScale = tex->Scale.Y;

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
	PalEntry col;
	int w;
	int h;

	unsigned char *buffer = tex->CreateTexBuffer(0, w, h);

	if (buffer == NULL) 
	{
		return false;
	}
	if (w != mWidth || h != mHeight)
	{
		// external Hires replacements cannot be trimmed.
		delete [] buffer;
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
		delete[] buffer;
		return true;
	}
	int first, last;

	for(first = 0; first < size; first++)
	{
		if (buffer[first*4+3] != 0) break;
	}
	if (first >= size)
	{
		// completely empty
		rect[0] = 0;
		rect[1] = 0;
		rect[2] = 1;
		rect[3] = 1;
		delete [] buffer;
		return true;
	}

	for(last = size-1; last >= first; last--)
	{
		if (buffer[last*4+3] != 0) break;
	}

	rect[1] = first / w;
	rect[3] = 1 + last/w - rect[1];

	rect[0] = 0;
	rect[2] = w;

	unsigned char *bufferoff = buffer + (rect[1] * w * 4);
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
				delete [] buffer;
				return true;
			}
		}
		rect[2]--;
	}
	delete [] buffer;
	return true;
}


//===========================================================================
// 
//	Binds a texture to the renderer
//
//===========================================================================

static FMaterial *last;
static int lastclamp;
static int lasttrans;

void FMaterial::InitGlobalState()
{
	last = nullptr;
	lastclamp = 0;
	lasttrans = 0;
}

void FMaterial::Bind(int clampmode, int translation)
{
	// avoid rebinding the same texture multiple times.
	if (this == last && lastclamp == clampmode && translation == lasttrans) return;
	last = this;
	lastclamp = clampmode;
	lasttrans = translation;

	int usebright = false;
	int maxbound = 0;

	if (tex->UseType == ETextureType::SWCanvas) clampmode = CLAMP_NOFILTER;
	if (tex->bHasCanvas) clampmode = CLAMP_CAMTEX;
	else if (tex->bWarped && clampmode <= CLAMP_XY) clampmode = CLAMP_NONE;

	// Textures that are already scaled in the texture lump will not get replaced by hires textures.
	int flags = mExpanded? CTF_Expand : (gl_texture_usehires && tex->Scale.X == 1 && tex->Scale.Y == 1 && clampmode <= CLAMP_XY)? CTF_CheckHires : 0;

	if (mBaseLayer->Bind(0, clampmode, translation, flags))
	{
		for(unsigned i=0;i<mTextureLayers.Size();i++)
		{
			FTexture *layer;
			if (mTextureLayers[i].animated)
			{
				FTextureID id = mTextureLayers[i].texture->id;
				layer = TexMan(id);
				ValidateSysTexture(layer, mExpanded);
			}
			else
			{
				layer = mTextureLayers[i].texture;
			}
			layer->gl_info.SystemTexture[mExpanded]->Bind(i+1, clampmode, 0, 0);
			maxbound = i+1;
		}
	}
	// unbind everything from the last texture that's still active
	for(int i=maxbound+1; i<=mMaxBound;i++)
	{
		FHardwareTexture::Unbind(i);
		mMaxBound = maxbound;
	}
}


//===========================================================================
//
//
//
//===========================================================================
void FMaterial::Precache()
{
	Bind(0, 0);
}

//===========================================================================
//
//
//
//===========================================================================
void FMaterial::PrecacheList(SpriteHits &translations)
{
	if (mBaseLayer != nullptr) mBaseLayer->CleanUnused(translations);
	SpriteHits::Iterator it(translations);
	SpriteHits::Pair *pair;
	while(it.NextPair(pair)) Bind(0, pair->Key);
}

//===========================================================================
//
// Retrieve texture coordinate info for per-wall scaling
//
//===========================================================================

void FMaterial::GetTexCoordInfo(FTexCoordInfo *tci, float x, float y) const
{
	if (x == 1.f)
	{
		tci->mRenderWidth = mRenderWidth;
		tci->mScale.X = tex->Scale.X;
		tci->mTempScale.X = 1.f;
	}
	else
	{
		float scale_x = x * tex->Scale.X;
		tci->mRenderWidth = xs_CeilToInt(mWidth / scale_x);
		tci->mScale.X = scale_x;
		tci->mTempScale.X = x;
	}

	if (y == 1.f)
	{
		tci->mRenderHeight = mRenderHeight;
		tci->mScale.Y = tex->Scale.Y;
		tci->mTempScale.Y = 1.f;
	}
	else
	{
		float scale_y = y * tex->Scale.Y;
		tci->mRenderHeight = xs_CeilToInt(mHeight / scale_y);
		tci->mScale.Y = scale_y;
		tci->mTempScale.Y = y;
	}
	if (tex->bHasCanvas) 
	{
		tci->mScale.Y = -tci->mScale.Y;
		tci->mRenderHeight = -tci->mRenderHeight;
	}
	tci->mWorldPanning = tex->bWorldPanning;
	tci->mWidth = mWidth;
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
		FTexture *tex = mBaseLayer->tex;
		*pAreas = tex->areas;
		return tex->areacount;
	}
	else
	{
		return 0;
	}
}

//===========================================================================
//
//
//
//===========================================================================

void FMaterial::BindToFrameBuffer()
{
	if (mBaseLayer->mHwTexture == NULL)
	{
		// must create the hardware texture first
		mBaseLayer->Bind(0, 0, 0, 0);
		FHardwareTexture::Unbind(0);
		ClearLastTexture();
	}
	mBaseLayer->mHwTexture->BindToFrameBuffer(mWidth, mHeight);
}

//==========================================================================
//
// Gets a texture from the texture manager and checks its validity for
// GL rendering. 
//
//==========================================================================

FMaterial * FMaterial::ValidateTexture(FTexture * tex, bool expand)
{
again:
	if (tex	&& tex->UseType!=ETextureType::Null)
	{
		if (tex->gl_info.bNoExpand) expand = false;

		FMaterial *gltex = tex->gl_info.Material[expand];
		if (gltex == NULL) 
		{
			if (expand)
			{
				if (tex->bWarped || tex->bHasCanvas || tex->shaderindex >= FIRST_USER_SHADER || (tex->shaderindex >= SHADER_Specular && tex->shaderindex <= SHADER_PBRBrightmap))
				{
					tex->gl_info.bNoExpand = true;
					goto again;
				}
				if (tex->Brightmap != NULL &&
					(tex->GetWidth() != tex->Brightmap->GetWidth() ||
					tex->GetHeight() != tex->Brightmap->GetHeight())
					)
				{
					// do not expand if the brightmap's size differs.
					tex->gl_info.bNoExpand = true;
					goto again;
				}
			}
			gltex = new FMaterial(tex, expand);
		}
		return gltex;
	}
	return NULL;
}

FMaterial * FMaterial::ValidateTexture(FTextureID no, bool expand, bool translate)
{
	return ValidateTexture(translate? TexMan(no) : TexMan[no], expand);
}


//==========================================================================
//
// Flushes all hardware dependent data
//
//==========================================================================

void FMaterial::FlushAll()
{
	for(int i=mMaterials.Size()-1;i>=0;i--)
	{
		mMaterials[i]->Clean(true);
	}
	// This is for shader layers. All shader layers must be managed by the texture manager
	// so this will catch everything.
	for(int i=TexMan.NumTextures()-1;i>=0;i--)
	{
		for (int j = 0; j < 2; j++)
		{
			FGLTexture *gltex = TexMan.ByIndex(i)->gl_info.SystemTexture[j];
			if (gltex != NULL) gltex->Clean(true);
		}
	}
}

void FMaterial::ClearLastTexture()
{
	last = NULL;
}
