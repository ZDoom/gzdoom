/*
** gl_material.cpp
** 
**---------------------------------------------------------------------------
** Copyright 2004-2009 Christoph Oelckers
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
** 4. When not used as part of GZDoom or a GZDoom derivative, this code will be
**    covered by the terms of the GNU Lesser General Public License as published
**    by the Free Software Foundation; either version 2.1 of the License, or (at
**    your option) any later version.
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
*/

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

//#include "gl/gl_intern.h"

#include "gl/system/gl_interface.h"
#include "gl/system/gl_framebuffer.h"
#include "gl/renderer/gl_lightdata.h"
#include "gl/data/gl_data.h"
#include "gl/textures/gl_texture.h"
#include "gl/textures/gl_translate.h"
#include "gl/textures/gl_bitmap.h"
#include "gl/textures/gl_material.h"
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
	assert(tx->gl_info.SystemTexture == NULL);
	tex = tx;

	glpatch=NULL;
	for(int i=0;i<5;i++) gltexture[i]=NULL;
	HiresLump=-1;
	hirestexture = NULL;
	currentwarp = 0;
	bHasColorkey = false;
	bIsTransparent = -1;
	bExpand = expandpatches;
	tex->gl_info.SystemTexture = this;
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
unsigned char *FGLTexture::LoadHiresTexture(FTexture *tex, int *width, int *height, int cm)
{
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

		FGLBitmap bmp(buffer, w*4, w, h);
		bmp.SetTranslationInfo(cm);

		
		int trans = hirestexture->CopyTrueColorPixels(&bmp, 0, 0);
		hirestexture->CheckTrans(buffer, w*h, trans);
		bIsTransparent = hirestexture->gl_info.mIsTransparent;

		if (bHasColorkey)
		{
			// This is a crappy Doomsday color keyed image
			// We have to remove the key manually. :(
			DWORD * dwdata=(DWORD*)buffer;
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
	for(int i=0;i<5;i++)
	{
		if (gltexture[i]) 
		{
			if (!all) gltexture[i]->Clean(false);
			else
			{
				delete gltexture[i];
				gltexture[i]=NULL;
			}
		}
	}
	if (glpatch) 
	{
		if (!all) glpatch->Clean(false);
		else
		{
			delete glpatch;
			glpatch=NULL;
		}
	}
}

//===========================================================================
//
// FGLTex::WarpBuffer
//
//===========================================================================

BYTE *FGLTexture::WarpBuffer(BYTE *buffer, int Width, int Height, int warp)
{
	if (Width > 256 || Height > 256) return buffer;

	DWORD *in = (DWORD*)buffer;
	DWORD *out = (DWORD*)new BYTE[4*Width*Height];
	float Speed = static_cast<FWarpTexture*>(tex)->GetSpeed();

	static_cast<FWarpTexture*>(tex)->GenTime = r_FrameTime;

	static DWORD linebuffer[256];	// anything larger will bring down performance so it is excluded above.
	DWORD timebase = DWORD(r_FrameTime*Speed*23/28);
	int xsize = Width;
	int ysize = Height;
	int xmask = xsize - 1;
	int ymask = ysize - 1;
	int ds_xbits;
	int i,x;

	if (warp == 1)
	{
		for(ds_xbits=-1,i=Width; i; i>>=1, ds_xbits++);

		for (x = xsize-1; x >= 0; x--)
		{
			int yt, yf = (finesine[(timebase+(x+17)*128)&FINEMASK]>>13) & ymask;
			const DWORD *source = in + x;
			DWORD *dest = out + x;
			for (yt = ysize; yt; yt--, yf = (yf+1)&ymask, dest += xsize)
			{
				*dest = *(source+(yf<<ds_xbits));
			}
		}
		timebase = DWORD(r_FrameTime*Speed*32/28);
		int y;
		for (y = ysize-1; y >= 0; y--)
		{
			int xt, xf = (finesine[(timebase+y*128)&FINEMASK]>>13) & xmask;
			DWORD *source = out + (y<<ds_xbits);
			DWORD *dest = linebuffer;
			for (xt = xsize; xt; xt--, xf = (xf+1)&xmask)
			{
				*dest++ = *(source+xf);
			}
			memcpy (out+y*xsize, linebuffer, xsize*sizeof(DWORD));
		}
	}
	else
	{
		int ybits;
		for(ybits=-1,i=ysize; i; i>>=1, ybits++);

		DWORD timebase = (r_FrameTime * Speed * 40 / 28);
		for (x = xsize-1; x >= 0; x--)
		{
			for (int y = ysize-1; y >= 0; y--)
			{
				int xt = (x + 128
					+ ((finesine[(y*128 + timebase*5 + 900) & 8191]*2)>>FRACBITS)
					+ ((finesine[(x*256 + timebase*4 + 300) & 8191]*2)>>FRACBITS)) & xmask;
				int yt = (y + 128
					+ ((finesine[(y*128 + timebase*3 + 700) & 8191]*2)>>FRACBITS)
					+ ((finesine[(x*256 + timebase*4 + 1200) & 8191]*2)>>FRACBITS)) & ymask;
				const DWORD *source = in + (xt << ybits) + yt;
				DWORD *dest = out + (x << ybits) + y;
				*dest = *source;
			}
		}
	}
	delete [] buffer;
	return (BYTE*)out;
}

//===========================================================================
// 
//	Initializes the buffer for the texture data
//
//===========================================================================

unsigned char * FGLTexture::CreateTexBuffer(int cm, int translation, int & w, int & h, bool expand, FTexture *hirescheck, int warp)
{
	unsigned char * buffer;
	int W, H;


	// Textures that are already scaled in the texture lump will not get replaced
	// by hires textures
	if (gl_texture_usehires && hirescheck != NULL)
	{
		buffer = LoadHiresTexture (hirescheck, &w, &h, cm);
		if (buffer)
		{
			return buffer;
		}
	}

	W = w = tex->GetWidth() + expand*2;
	H = h = tex->GetHeight() + expand*2;


	buffer=new unsigned char[W*(H+1)*4];
	memset(buffer, 0, W * (H+1) * 4);

	FGLBitmap bmp(buffer, W*4, W, H);
	bmp.SetTranslationInfo(cm, translation);

	if (tex->bComplex)
	{
		FBitmap imgCreate;

		// The texture contains special processing so it must be composited using the
		// base bitmap class and then be converted as a whole.
		if (imgCreate.Create(W, H))
		{
			memset(imgCreate.GetPixels(), 0, W * H * 4);
			int trans = tex->CopyTrueColorPixels(&imgCreate, expand, expand);
			bmp.CopyPixelDataRGB(0, 0, imgCreate.GetPixels(), W, H, 4, W * 4, 0, CF_BGRA);
			tex->CheckTrans(buffer, W*H, trans);
			bIsTransparent = tex->gl_info.mIsTransparent;
		}
	}
	else if (translation<=0)
	{
		int trans = tex->CopyTrueColorPixels(&bmp, expand, expand);
		tex->CheckTrans(buffer, W*H, trans);
		bIsTransparent = tex->gl_info.mIsTransparent;
	}
	else
	{
		// When using translations everything must be mapped to the base palette.
		// Since FTexture's method is doing exactly that by calling GetPixels let's use that here
		// to do all the dirty work for us. ;)
		tex->FTexture::CopyTrueColorPixels(&bmp, expand, expand);
		bIsTransparent = 0;
	}

	if (warp != 0)
	{
		buffer = WarpBuffer(buffer, W, H, warp);
	}
	// [BB] The hqnx upsampling (not the scaleN one) destroys partial transparency, don't upsamle textures using it.
	// Also don't upsample warped textures.
	else //if (bIsTransparent != 1)
	{
		// [BB] Potentially upsample the buffer.
		buffer = gl_CreateUpsampledTextureBuffer ( tex, buffer, W, H, w, h, bIsTransparent || cm == CM_SHADE );
	}
	currentwarp = warp;
	currentwarptime = gl_frameMS;

	return buffer;
}


//===========================================================================
// 
//	Create hardware texture for world use
//
//===========================================================================

FHardwareTexture *FGLTexture::CreateTexture(int clampmode)
{
	if (tex->UseType==FTexture::TEX_Null) return NULL;		// Cannot register a NULL texture
	if (!gltexture[clampmode]) 
	{
		gltexture[clampmode] = new FHardwareTexture(tex->GetWidth(), tex->GetHeight(), true, true, false, tex->gl_info.bNoCompress);
	}
	return gltexture[clampmode]; 
}

//===========================================================================
// 
//	Create Hardware texture for patch use
//
//===========================================================================

bool FGLTexture::CreatePatch()
{
	if (tex->UseType==FTexture::TEX_Null) return false;		// Cannot register a NULL texture
	if (!glpatch) 
	{
		glpatch=new FHardwareTexture(tex->GetWidth() + bExpand, tex->GetHeight() + bExpand, false, false, tex->gl_info.bNoFilter, tex->gl_info.bNoCompress);
	}
	if (glpatch) return true; 	
	return false;
}


//===========================================================================
// 
//	Binds a texture to the renderer
//
//===========================================================================

const FHardwareTexture *FGLTexture::Bind(int texunit, int cm, int clampmode, int translation, FTexture *hirescheck, int warp)
{
	int usebright = false;

	if (translation <= 0) translation = -translation;
	else translation = GLTranslationPalette::GetInternalTranslation(translation);

	FHardwareTexture *hwtex;
	
	if (gltexture[4] != NULL && clampmode < 4 && gltexture[clampmode] == NULL)
	{
		hwtex = gltexture[clampmode] = gltexture[4];
		gltexture[4] = NULL;

		if (hwtex->Bind(texunit, cm, translation))
		{
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, (clampmode & GLT_CLAMPX)? GL_CLAMP_TO_EDGE : GL_REPEAT);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, (clampmode & GLT_CLAMPY)? GL_CLAMP_TO_EDGE : GL_REPEAT);
		}
	}
	else
	{
		hwtex = CreateTexture(clampmode);
	}

	if (hwtex)
	{
		if ((warp != 0 || currentwarp != warp) && currentwarptime != gl_frameMS)
		{
			// must recreate the texture
			Clean(true);
			hwtex = CreateTexture(clampmode);
		}

		// Texture has become invalid - this is only for special textures, not the regular warping, which is handled above
		else if ((warp == 0 && !tex->bHasCanvas && !tex->bWarped) && tex->CheckModified())
		{
			Clean(true);
			hwtex = CreateTexture(clampmode);
		}

		// Bind it to the system.
		if (!hwtex->Bind(texunit, cm, translation))
		{
			
			int w=0, h=0;

			// Create this texture
			unsigned char * buffer = NULL;
			
			if (!tex->bHasCanvas)
			{
				buffer = CreateTexBuffer(cm, translation, w, h, false, hirescheck, warp);
				tex->ProcessData(buffer, w, h, false);
			}
			if (!hwtex->CreateTexture(buffer, w, h, true, texunit, cm, translation)) 
			{
				// could not create texture
				delete[] buffer;
				return NULL;
			}
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, (clampmode & GLT_CLAMPX)? GL_CLAMP_TO_EDGE : GL_REPEAT);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, (clampmode & GLT_CLAMPY)? GL_CLAMP_TO_EDGE : GL_REPEAT);
			delete[] buffer;
		}

		if (tex->bHasCanvas) static_cast<FCanvasTexture*>(tex)->NeedUpdate();
		return hwtex; 
	}
	return NULL;
}

//===========================================================================
// 
//	Binds a sprite to the renderer
//
//===========================================================================
const FHardwareTexture * FGLTexture::BindPatch(int texunit, int cm, int translation, int warp)
{
	bool usebright = false;
	int transparm = translation;

	if (translation <= 0) translation = -translation;
	else translation = GLTranslationPalette::GetInternalTranslation(translation);

	if (CreatePatch())
	{
		if (warp != 0 || currentwarp != warp)
		{
			// must recreate the texture
			Clean(true);
			CreatePatch();
		}

		// Texture has become invalid - this is only for special textures, not the regular warping, which is handled above
		else if ((warp == 0 && !tex->bHasCanvas && !tex->bWarped) && tex->CheckModified())
		{
			Clean(true);
			CreatePatch();
		}


		// Bind it to the system. 
		if (!glpatch->Bind(texunit, cm, translation))
		{
			int w, h;

			// Create this texture
			unsigned char * buffer = CreateTexBuffer(cm, translation, w, h, bExpand, NULL, warp);
			tex->ProcessData(buffer, w, h, true);
			if (!glpatch->CreateTexture(buffer, w, h, false, texunit, cm, translation)) 
			{
				// could not create texture
				delete[] buffer;
				return NULL;
			}
			delete[] buffer;
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		}
		return glpatch; 	
	}
	return NULL;
}


//===========================================================================
//
//
//
//===========================================================================

fixed_t FTexCoordInfo::RowOffset(fixed_t rowoffset) const
{
	if (mTempScaleY == FRACUNIT)
	{
		if (mScaleY==FRACUNIT || mWorldPanning) return rowoffset;
		else return FixedDiv(rowoffset, mScaleY);
	}
	else
	{
		if (mWorldPanning) return FixedDiv(rowoffset, mTempScaleY);
		else return FixedDiv(rowoffset, mScaleY);
	}
}

//===========================================================================
//
//
//
//===========================================================================

fixed_t FTexCoordInfo::TextureOffset(fixed_t textureoffset) const
{
	if (mTempScaleX == FRACUNIT)
	{
		if (mScaleX==FRACUNIT || mWorldPanning) return textureoffset;
		else return FixedDiv(textureoffset, mScaleX);
	}
	else
	{
		if (mWorldPanning) return FixedDiv(textureoffset, mTempScaleX);
		else return FixedDiv(textureoffset, mScaleX);
	}
}

//===========================================================================
//
// Returns the size for which texture offset coordinates are used.
//
//===========================================================================

fixed_t FTexCoordInfo::TextureAdjustWidth() const
{
	if (mWorldPanning) 
	{
		if (mTempScaleX == FRACUNIT) return mRenderWidth;
		else return FixedDiv(mWidth, mTempScaleX);
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
		FGLTexture *gltex = tex->gl_info.SystemTexture;
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

FMaterial::FMaterial(FTexture * tx, bool forceexpand)
{
	assert(tx->gl_info.Material == NULL);

	bool expanded = tx->UseType == FTexture::TEX_Sprite || 
					tx->UseType == FTexture::TEX_SkinSprite || 
					tx->UseType == FTexture::TEX_Decal ||
					forceexpand;

	mShaderIndex = 0;

	// TODO: apply custom shader object here
	/* if (tx->CustomShaderDefinition)
	{
	}
	else
	*/
	if (tx->bWarped)
	{
		mShaderIndex = tx->bWarped;
		expanded = false;
		tx->gl_info.shaderspeed = static_cast<FWarpTexture*>(tx)->GetSpeed();
	}
	else if (tx->bHasCanvas)
	{
		expanded = false;
	}
	else if (gl.shadermodel > 2) 
	{
		if (tx->gl_info.shaderindex >= FIRST_USER_SHADER)
		{
			mShaderIndex = tx->gl_info.shaderindex;
			expanded = false;
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


	for (int i=GLUSE_PATCH; i<=GLUSE_TEXTURE; i++)
	{
		Width[i] = tx->GetWidth();
		Height[i] = tx->GetHeight();
		LeftOffset[i] = tx->LeftOffset;
		TopOffset[i] = tx->TopOffset;
		RenderWidth[i] = tx->GetScaledWidth();
		RenderHeight[i] = tx->GetScaledHeight();
	}
	Width[GLUSE_SPRITE] = Width[GLUSE_PATCH];
	Height[GLUSE_SPRITE] = Height[GLUSE_PATCH];
	LeftOffset[GLUSE_SPRITE] = LeftOffset[GLUSE_PATCH];
	TopOffset[GLUSE_SPRITE] = TopOffset[GLUSE_PATCH];
	SpriteU[0] = SpriteV[0] = 0;
	spriteright = SpriteU[1] = Width[GLUSE_PATCH] / (float)FHardwareTexture::GetTexDimension(Width[GLUSE_PATCH]);
	spritebottom = SpriteV[1] = Height[GLUSE_PATCH] / (float)FHardwareTexture::GetTexDimension(Height[GLUSE_PATCH]);

	mTextureLayers.ShrinkToFit();
	mMaxBound = -1;
	mMaterials.Push(this);
	tx->gl_info.Material = this;
	if (tx->bHasCanvas) tx->gl_info.mIsTransparent = 0;
	tex = tx;

	tx->gl_info.mExpanded = expanded;
	FTexture *basetex = tx->GetRedirect(gl.shadermodel < 4);
	if (!expanded && !basetex->gl_info.mExpanded && basetex->UseType != FTexture::TEX_Sprite)
	{
		// check if the texture is just a simple redirect to a patch
		// If so we should use the patch for texture creation to
		// avoid eventual redundancies. 
		// This may only be done if both textures use the same expansion mode
		// Redirects to sprites are not permitted because sprites get expanded, however, this won't have been set
		// if the sprite hadn't been used yet.
		mBaseLayer = ValidateSysTexture(basetex, false);
	}
	else if (!expanded)
	{
		// if we got a non-expanded texture that redirects to an expanded one
		mBaseLayer = ValidateSysTexture(tx, false);
	}
	else
	{
		// a little adjustment to make sprites look better with texture filtering:
		// create a 1 pixel wide empty frame around them.
		RenderWidth[GLUSE_PATCH]+=2;
		RenderHeight[GLUSE_PATCH]+=2;
		Width[GLUSE_PATCH]+=2;
		Height[GLUSE_PATCH]+=2;
		LeftOffset[GLUSE_PATCH]+=1;
		TopOffset[GLUSE_PATCH]+=1;
		Width[GLUSE_SPRITE] += 2;
		Height[GLUSE_SPRITE] += 2;
		LeftOffset[GLUSE_SPRITE] += 1;
		TopOffset[GLUSE_SPRITE] += 1;
		spriteright = SpriteU[1] = Width[GLUSE_PATCH] / (float)FHardwareTexture::GetTexDimension(Width[GLUSE_PATCH]);
		spritebottom = SpriteV[1] = Height[GLUSE_PATCH] / (float)FHardwareTexture::GetTexDimension(Height[GLUSE_PATCH]);

		mBaseLayer = ValidateSysTexture(tx, true);

		int trim[4];

		if (TrimBorders(trim))
		{
			Width[GLUSE_SPRITE] = trim[2] + 2;
			Height[GLUSE_SPRITE] = trim[3] + 2;
			LeftOffset[GLUSE_SPRITE] -= trim[0];
			TopOffset[GLUSE_SPRITE] -= trim[1];

			SpriteU[0] = SpriteU[1] * (trim[0] / (float)Width[GLUSE_PATCH]);
			SpriteV[0] = SpriteV[1] * (trim[1] / (float)Height[GLUSE_PATCH]);
			SpriteU[1] *= (trim[0]+trim[2]+2) / (float)Width[GLUSE_PATCH]; 
			SpriteV[1] *= (trim[1]+trim[3]+2) / (float)Height[GLUSE_PATCH]; 
		}
	}
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

	unsigned char *buffer = CreateTexBuffer(CM_DEFAULT, 0, w, h);

	if (buffer == NULL) 
	{
		return false;
	}
	if (w != Width[GLUSE_TEXTURE] || h != Height[GLUSE_TEXTURE])
	{
		// external Hires replacements cannot be trimmed.
		delete [] buffer;
		return false;
	}

	int size = w*h;

	int first, last;

	for(first = 0; first < size; first++)
	{
		if (buffer[first*4+3] != 0) break;
	}
	if (first >= size)
	{
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

void FMaterial::Bind(int cm, int clampmode, int translation, int overrideshader)
{
	int usebright = false;
	int shaderindex = overrideshader > 0? overrideshader : mShaderIndex;
	int maxbound = 0;
	bool allowhires = tex->xScale == FRACUNIT && tex->yScale == FRACUNIT;

	int softwarewarp = gl_RenderState.SetupShader(tex->bHasCanvas, shaderindex, cm, tex->gl_info.shaderspeed);

	if (tex->bHasCanvas || tex->bWarped) clampmode = 0;
	else if (clampmode != -1) clampmode &= 3;
	else clampmode = 4;

	const FHardwareTexture *gltexture = mBaseLayer->Bind(0, cm, clampmode, translation, allowhires? tex:NULL, softwarewarp);
	if (gltexture != NULL && shaderindex > 0 && overrideshader == 0)
	{
		for(unsigned i=0;i<mTextureLayers.Size();i++)
		{
			FTexture *layer;
			if (mTextureLayers[i].animated)
			{
				FTextureID id = mTextureLayers[i].texture->GetID();
				layer = TexMan(id);
				ValidateSysTexture(layer, false);
			}
			else
			{
				layer = mTextureLayers[i].texture;
			}
			layer->gl_info.SystemTexture->Bind(i+1, CM_DEFAULT, clampmode, 0, NULL, false);
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
//	Binds a texture to the renderer
//
//===========================================================================

void FMaterial::BindPatch(int cm, int translation, int overrideshader)
{
	int usebright = false;
	int shaderindex = overrideshader > 0? overrideshader : mShaderIndex;
	int maxbound = 0;

	int softwarewarp = gl_RenderState.SetupShader(tex->bHasCanvas, shaderindex, cm, tex->gl_info.shaderspeed);

	const FHardwareTexture *glpatch = mBaseLayer->BindPatch(0, cm, translation, softwarewarp);
	// The only multitexture effect usable on sprites is the brightmap.
	if (glpatch != NULL && shaderindex == 3)
	{
		mTextureLayers[0].texture->gl_info.SystemTexture->BindPatch(1, CM_DEFAULT, 0, 0);
		maxbound = 1;
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
	if (tex->UseType==FTexture::TEX_Sprite) 
	{
		BindPatch(CM_DEFAULT, 0);
	}
	else 
	{
		int cached = 0;
		for(int i=0;i<4;i++)
		{
			if (mBaseLayer->gltexture[i] != 0)
			{
				Bind (CM_DEFAULT, i, 0);
				cached++;
			}
			if (cached == 0) Bind(CM_DEFAULT, -1, 0);
		}
	}
}

//===========================================================================
//
// This function is needed here to temporarily manipulate the texture
// for per-wall scaling so that the coordinate functions return proper
// results. Doing this here is much easier than having the calling code
// make these calculations.
//
//===========================================================================

void FMaterial::GetTexCoordInfo(FTexCoordInfo *tci, fixed_t x, fixed_t y) const
{
	if (x == FRACUNIT)
	{
		tci->mRenderWidth = RenderWidth[GLUSE_TEXTURE];
		tci->mScaleX = tex->xScale;
		tci->mTempScaleX = FRACUNIT;
	}
	else
	{
		fixed_t scale_x = FixedMul(x, tex->xScale);
		int foo = (Width[GLUSE_TEXTURE] << 17) / scale_x; 
		tci->mRenderWidth = (foo >> 1) + (foo & 1); 
		tci->mScaleX = scale_x;
		tci->mTempScaleX = x;
	}

	if (y == FRACUNIT)
	{
		tci->mRenderHeight = RenderHeight[GLUSE_TEXTURE];
		tci->mScaleY = tex->yScale;
		tci->mTempScaleY = FRACUNIT;
	}
	else
	{
		fixed_t scale_y = FixedMul(y, tex->yScale);
		int foo = (Height[GLUSE_TEXTURE] << 17) / scale_y; 
		tci->mRenderHeight = (foo >> 1) + (foo & 1); 
		tci->mScaleY = scale_y;
		tci->mTempScaleY = y;
	}
	if (tex->bHasCanvas) 
	{
		tci->mScaleY = -tci->mScaleY;
		tci->mRenderHeight = -tci->mRenderHeight;
	}
	tci->mWorldPanning = tex->bWorldPanning;
	tci->mWidth = Width[GLUSE_TEXTURE];
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
	if (mBaseLayer->gltexture == NULL)
	{
		// must create the hardware texture first
		mBaseLayer->Bind(0, CM_DEFAULT, 0, 0, NULL, 0);
		FHardwareTexture::Unbind(0);
	}
	mBaseLayer->gltexture[0]->BindToFrameBuffer();
}

//===========================================================================
//
// GetRect
//
//===========================================================================

void FMaterial::GetRect(FloatRect * r, ETexUse i) const
{
	r->left = -GetScaledLeftOffsetFloat(i);
	r->top = -GetScaledTopOffsetFloat(i);
	r->width = GetScaledWidthFloat(i);
	r->height = GetScaledHeightFloat(i);
}


//==========================================================================
//
// Gets a texture from the texture manager and checks its validity for
// GL rendering. 
//
//==========================================================================

FMaterial * FMaterial::ValidateTexture(FTexture * tex)
{
	if (tex	&& tex->UseType!=FTexture::TEX_Null)
	{
		FMaterial *gltex = tex->gl_info.Material;
		if (gltex == NULL) 
		{
			//@sync-tex
			gltex = new FMaterial(tex, false);
		}
		return gltex;
	}
	return NULL;
}

FMaterial * FMaterial::ValidateTexture(FTextureID no, bool translate)
{
	return ValidateTexture(translate? TexMan(no) : TexMan[no]);
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
		FGLTexture *gltex = TexMan.ByIndex(i)->gl_info.SystemTexture;
		if (gltex != NULL) gltex->Clean(true);
	}
}

//==========================================================================
//
// Prints some texture info
//
//==========================================================================

int FGLTexture::Dump(int i)
{
	int cnt = 0;
	int lump = tex->GetSourceLump();
	Printf(PRINT_LOG, "Texture '%s' (Index %d, Lump %d, Name '%s'):\n", tex->Name, i, lump, Wads.GetLumpFullName(lump));
	if (hirestexture) Printf(PRINT_LOG, "\tHirestexture\n");
	if (glpatch) Printf(PRINT_LOG, "\tPatch\n"),cnt++;
	if (gltexture[0]) Printf(PRINT_LOG, "\tTexture (x:no,  y:no )\n"),cnt++;
	if (gltexture[1]) Printf(PRINT_LOG, "\tTexture (x:yes, y:no )\n"),cnt++;
	if (gltexture[2]) Printf(PRINT_LOG, "\tTexture (x:no,  y:yes)\n"),cnt++;
	if (gltexture[3]) Printf(PRINT_LOG, "\tTexture (x:yes, y:yes)\n"),cnt++;
	if (gltexture[4]) Printf(PRINT_LOG, "\tTexture precache\n"),cnt++;
	return cnt;
}

CCMD(textureinfo)
{
	int cnth = 0, cntt = 0, pix = 0;
	for(int i=0; i<TexMan.NumTextures(); i++)
	{
		FTexture *tex = TexMan.ByIndex(i);
		FGLTexture *systex = tex->gl_info.SystemTexture;
		if (systex != NULL) 
		{
			int cnt = systex->Dump(i);
			cnth+=cnt;
			cntt++;
			pix += cnt * tex->GetWidth() * tex->GetHeight();
		}
	}
	Printf(PRINT_LOG, "%d system textures, %d hardware textures, %d pixels\n", cntt, cnth, pix);
}

