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
#include "colormatcher.h"
#include "textures/warpbuffer.h"
#include "textures/bitmap.h"

//#include "gl/gl_intern.h"

#include "gl/system/gl_interface.h"
#include "gl/system/gl_framebuffer.h"
#include "gl/renderer/gl_lightdata.h"
#include "gl/renderer/gl_renderer.h"
#include "gl/data/gl_data.h"
#include "gl/textures/gl_texture.h"
#include "gl/textures/gl_translate.h"
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
	HiresLump = -1;
	hirestexture = NULL;
	bHasColorkey = false;
	bIsTransparent = -1;
	bExpandFlag = expandpatches;
	lastSampler = 254;
	lastTranslation = -1;
	tex->gl_info.SystemTexture[expandpatches] = this;
}

//===========================================================================
//
// Destructor
//
//===========================================================================

FGLTexture::~FGLTexture()
{
	Clean(true);
	if (hirestexture) delete hirestexture;
}

//==========================================================================
//
// Checks for the presence of a hires texture replacement and loads it
//
//==========================================================================
unsigned char *FGLTexture::LoadHiresTexture(FTexture *tex, int *width, int *height)
{
	if (bExpandFlag) return NULL;	// doesn't work for expanded textures

	if (HiresLump==-1) 
	{
		bHasColorkey = false;
		HiresLump = CheckDDPK3(tex);
		if (HiresLump < 0) HiresLump = CheckExternalFile(tex, bHasColorkey);

		if (HiresLump >=0) 
		{
			hirestexture = FTexture::CreateTexture(HiresLump, FTexture::TEX_Any);
		}
	}
	if (hirestexture != NULL)
	{
		int w=hirestexture->GetWidth();
		int h=hirestexture->GetHeight();

		unsigned char * buffer=new unsigned char[w*(h+1)*4];
		memset(buffer, 0, w * (h+1) * 4);

		FBitmap bmp(buffer, w*4, w, h);
		
		int trans = hirestexture->CopyTrueColorPixels(&bmp, 0, 0);
		hirestexture->CheckTrans(buffer, w*h, trans);
		bIsTransparent = hirestexture->gl_info.mIsTransparent;

		if (bHasColorkey)
		{
			// This is a crappy Doomsday color keyed image
			// We have to remove the key manually. :(
			uint32_t * dwdata=(uint32_t*)buffer;
			for (int i=(w*h);i>0;i--)
			{
				if (dwdata[i]==0xffffff00 || dwdata[i]==0xffff00ff) dwdata[i]=0;
			}
		}
		*width = w;
		*height = h;
		return buffer;
	}
	return NULL;
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
//	Initializes the buffer for the texture data
//
//===========================================================================

unsigned char * FGLTexture::CreateTexBuffer(int translation, int & w, int & h, FTexture *hirescheck, bool createexpanded, bool alphatrans)
{
	unsigned char * buffer;
	int W, H;
	int isTransparent = -1;


	// Textures that are already scaled in the texture lump will not get replaced
	// by hires textures
	if (gl_texture_usehires && hirescheck != NULL && !alphatrans)
	{
		buffer = LoadHiresTexture (hirescheck, &w, &h);
		if (buffer)
		{
			return buffer;
		}
	}

	int exx = bExpandFlag && createexpanded;

	W = w = tex->GetWidth() + 2 * exx;
	H = h = tex->GetHeight() + 2 * exx;


	buffer=new unsigned char[W*(H+1)*4];
	memset(buffer, 0, W * (H+1) * 4);

	FBitmap bmp(buffer, W*4, W, H);

	if (translation <= 0)
	{
		// Q: Is this special treatment still needed? Needs to be checked.
		if (tex->bComplex)
		{
			FBitmap imgCreate;

			// The texture contains special processing so it must be fully composited before being converted as a whole.
			if (imgCreate.Create(W, H))
			{
				memset(imgCreate.GetPixels(), 0, W * H * 4);
				int trans = tex->CopyTrueColorPixels(&imgCreate, exx, exx);
				bmp.CopyPixelDataRGB(0, 0, imgCreate.GetPixels(), W, H, 4, W * 4, 0, CF_BGRA);
				tex->CheckTrans(buffer, W*H, trans);
				isTransparent = tex->gl_info.mIsTransparent;
				if (bIsTransparent == -1) bIsTransparent = isTransparent;
			}
		}
		else
		{
			int trans = tex->CopyTrueColorPixels(&bmp, exx, exx);
			tex->CheckTrans(buffer, W*H, trans);
			isTransparent = tex->gl_info.mIsTransparent;
			if (bIsTransparent == -1) bIsTransparent = isTransparent;
		}
	}
	else
	{
		// When using translations everything must be mapped to the base palette.
		// so use CopyTrueColorTranslated
		tex->CopyTrueColorTranslated(&bmp, exx, exx, 0, GLTranslationPalette::GetPalette(translation));
		isTransparent = 0;
		// This is not conclusive for setting the texture's transparency info.
	}

	// if we just want the texture for some checks there's no need for upsampling.
	if (!createexpanded) return buffer;

	// [BB] The hqnx upsampling (not the scaleN one) destroys partial transparency, don't upsamle textures using it.
	// [BB] Potentially upsample the buffer.
	return gl_CreateUpsampledTextureBuffer ( tex, buffer, W, H, w, h, !!isTransparent);
}


//===========================================================================
// 
//	Create hardware texture for world use
//
//===========================================================================

FHardwareTexture *FGLTexture::CreateHwTexture()
{
	if (tex->UseType==FTexture::TEX_Null) return NULL;		// Cannot register a NULL texture
	if (mHwTexture == NULL)
	{
		mHwTexture = new FHardwareTexture(tex->GetWidth() + bExpandFlag*2, tex->GetHeight() + bExpandFlag*2, tex->gl_info.bNoCompress);
	}
	return mHwTexture; 
}

//===========================================================================
// 
//	Binds a texture to the renderer
//
//===========================================================================

const FHardwareTexture *FGLTexture::Bind(int texunit, int clampmode, int translation, FTexture *hirescheck)
{
	int usebright = false;
	bool alphatrans = false;

	if (translation <= 0) translation = -translation;
	else
	{
		alphatrans = (gl.legacyMode && uint32_t(translation) == TRANSLATION(TRANSLATION_Standard, 8));
		translation = GLTranslationPalette::GetInternalTranslation(translation);
	}

	bool needmipmap = (clampmode <= CLAMP_XY);

	FHardwareTexture *hwtex = CreateHwTexture();

	if (hwtex)
	{
		// Texture has become invalid
		if ((!tex->bHasCanvas && (!tex->bWarped || gl.legacyMode)) && tex->CheckModified())
		{
			Clean(true);
			hwtex = CreateHwTexture();
		}

		// Bind it to the system.
		if (!hwtex->Bind(texunit, translation, needmipmap))
		{
			
			int w=0, h=0;

			// Create this texture
			unsigned char * buffer = NULL;
			
			if (!tex->bHasCanvas)
			{
				buffer = CreateTexBuffer(translation, w, h, hirescheck, true, alphatrans);
				if (tex->bWarped && gl.legacyMode && w*h <= 256*256)	// do not software-warp larger textures, especially on the old systems that still need this fallback.
				{
					// need to do software warping
					FWarpTexture *wt = static_cast<FWarpTexture*>(tex);
					unsigned char *warpbuffer = new unsigned char[w*h*4];
					WarpBuffer((uint32_t*)warpbuffer, (const uint32_t*)buffer, w, h, wt->WidthOffsetMultiplier, wt->HeightOffsetMultiplier, screen->FrameTime, wt->Speed, tex->bWarped);
					delete[] buffer;
					buffer = warpbuffer;
					wt->GenTime = screen->FrameTime;
				}
				tex->ProcessData(buffer, w, h, false);
			}
			if (!hwtex->CreateTexture(buffer, w, h, texunit, needmipmap, translation, "FGLTexture.Bind")) 
			{
				// could not create texture
				delete[] buffer;
				return NULL;
			}
			delete[] buffer;
		}
		if (tex->bHasCanvas) static_cast<FCanvasTexture*>(tex)->NeedUpdate();
		if (translation != lastTranslation) lastSampler = 254;
		if (lastSampler != clampmode)
			lastSampler = GLRenderer->mSamplerManager->Bind(texunit, clampmode, lastSampler);
		lastTranslation = translation;
		return hwtex; 
	}
	return NULL;
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
	if (tex	&& tex->UseType!=FTexture::TEX_Null)
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
	mShaderIndex = 0;
	tex = tx;

	// TODO: apply custom shader object here
	/* if (tx->CustomShaderDefinition)
	{
	}
	else
	*/
	if (tx->bWarped)
	{
		mShaderIndex = tx->bWarped;
		tx->gl_info.shaderspeed = static_cast<FWarpTexture*>(tx)->GetSpeed();
	}
	else if (tx->bHasCanvas)
	{
		if (tx->gl_info.shaderindex >= FIRST_USER_SHADER)
		{
			mShaderIndex = tx->gl_info.shaderindex;
		}
		// no brightmap for cameratexture
	}
	else
	{
		if (tx->gl_info.shaderindex >= FIRST_USER_SHADER)
		{
			mShaderIndex = tx->gl_info.shaderindex;
		}
		else
		{
			tx->CreateDefaultBrightmap();
			if (tx->gl_info.Brightmap != NULL)
			{
				ValidateSysTexture(tx->gl_info.Brightmap, expanded);
				FTextureLayer layer = {tx->gl_info.Brightmap, false};
				mTextureLayers.Push(layer);
				mShaderIndex = 3;
			}
		}
	}
	mBaseLayer = ValidateSysTexture(tx, expanded);


	mWidth = tx->GetWidth();
	mHeight = tx->GetHeight();
	mLeftOffset = tx->LeftOffset;
	mTopOffset = tx->TopOffset;
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

	float fxScale = tx->Scale.X;
	float fyScale = tx->Scale.Y;

	// mSpriteRect is for positioning the sprite in the scene.
	mSpriteRect.left = -mLeftOffset / fxScale;
	mSpriteRect.top = -mTopOffset / fyScale;
	mSpriteRect.width = mWidth / fxScale;
	mSpriteRect.height = mHeight / fyScale;

	if (expanded)
	{
		// a little adjustment to make sprites look better with texture filtering:
		// create a 1 pixel wide empty frame around them.
		int trim[4];
		bool trimmed = TrimBorders(trim);	// get the trim size before adding the empty frame

		int oldwidth = mWidth;
		int oldheight = mHeight;

		mWidth+=2;
		mHeight+=2;
		mLeftOffset+=1;
		mTopOffset+=1;
		mRenderWidth = mRenderWidth * mWidth / oldwidth;
		mRenderHeight = mRenderHeight * mHeight / oldheight;

		// Reposition the sprite with the frame considered
		mSpriteRect.left = -mLeftOffset / fxScale;
		mSpriteRect.top = -mTopOffset / fyScale;
		mSpriteRect.width = mWidth / fxScale;
		mSpriteRect.height = mHeight / fyScale;

		if (trimmed)
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

	mTextureLayers.ShrinkToFit();
	mMaxBound = -1;
	mMaterials.Push(this);
	tx->gl_info.Material[expanded] = this;
	if (tx->bHasCanvas) tx->gl_info.mIsTransparent = 0;
	mExpanded = expanded;
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
//	Finds gaps in the texture which can be skipped by the renderer
//  This was mainly added to speed up one area in E4M6 of 007LTSD
//
//===========================================================================

bool FMaterial::TrimBorders(int *rect)
{
	PalEntry col;
	int w;
	int h;

	unsigned char *buffer = CreateTexBuffer(0, w, h, false, false);

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
	bool allowhires = tex->Scale.X == 1 && tex->Scale.Y == 1 && clampmode <= CLAMP_XY && !mExpanded;

	if (tex->bHasCanvas) clampmode = CLAMP_CAMTEX;
	else if (tex->bWarped && clampmode <= CLAMP_XY) clampmode = CLAMP_NONE;

	const FHardwareTexture *gltexture = mBaseLayer->Bind(0, clampmode, translation, allowhires? tex:NULL);
	if (gltexture != NULL)
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
			layer->gl_info.SystemTexture[mExpanded]->Bind(i+1, clampmode, 0, NULL);
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
	if (mShaderIndex == 0)	// texture splitting can only be done if there's no attached effects
	{
		FTexture *tex = mBaseLayer->tex;
		*pAreas = tex->gl_info.areas;
		return tex->gl_info.areacount;
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
		mBaseLayer->Bind(0, 0, 0, NULL);
		FHardwareTexture::Unbind(0);
		ClearLastTexture();
	}
	mBaseLayer->mHwTexture->BindToFrameBuffer();
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
	if (tex	&& tex->UseType!=FTexture::TEX_Null)
	{
		if (tex->gl_info.bNoExpand) expand = false;

		FMaterial *gltex = tex->gl_info.Material[expand];
		if (gltex == NULL) 
		{
			if (expand)
			{
				if (tex->bWarped || tex->bHasCanvas || tex->gl_info.shaderindex >= FIRST_USER_SHADER)
				{
					tex->gl_info.bNoExpand = true;
					goto again;
				}
				if (tex->gl_info.Brightmap != NULL &&
					(tex->GetWidth() != tex->gl_info.Brightmap->GetWidth() ||
					tex->GetHeight() != tex->gl_info.Brightmap->GetHeight())
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
