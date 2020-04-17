/*
** texture.cpp
** The base texture class
**
**---------------------------------------------------------------------------
** Copyright 2004-2007 Randy Heit
** Copyright 2006-2018 Christoph Oelckers
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
#include "templates.h"
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

EXTERN_CVAR(Int, gl_texture_hqresize_targets)

// Wrappers to keep the definitions of these classes out of here.
void DeleteMaterial(FMaterial* mat);
IHardwareTexture* CreateHardwareTexture();


FTexture *CreateBrightmapTexture(FImageSource*);

// Make sprite offset adjustment user-configurable per renderer.
int r_spriteadjustSW, r_spriteadjustHW;

//==========================================================================
//
//
//
//==========================================================================


// Examines the lump contents to decide what type of texture to create,
// and creates the texture.
FTexture * FTexture::CreateTexture(const char *name, int lumpnum, bool allowflats)
{
	if (lumpnum == -1) return nullptr;

	auto image = FImageSource::GetImage(lumpnum, allowflats);
	if (image != nullptr)
	{
		FTexture *tex = new FImageTexture(image);
		if (tex != nullptr) 
		{
			tex->Name = name;
			tex->Name.ToUpper();
			return tex;
		}
	}
	return nullptr;
}

//==========================================================================
//
// 
//
//==========================================================================

FTexture::FTexture (const char *name, int lumpnum)
	: 
  Scale(1,1), SourceLump(lumpnum),
  bNoDecals(false), bNoRemap0(false), bWorldPanning(false),
  bMasked(true), bAlphaTexture(false), bHasCanvas(false), bWarped(0), bComplex(false), bMultiPatch(false), bFullNameTexture(false),
	Rotations(0xFFFF), SkyOffset(0), Width(0), Height(0)
{
	bBrightmapChecked = false;
	bGlowing = false;
	bAutoGlowing = false;
	bFullbright = false;
	bDisableFullbright = false;
	bSkybox = false;
	bNoCompress = false;
	bTranslucent = -1;


	_LeftOffset[0] = _LeftOffset[1] = _TopOffset[0] = _TopOffset[1] = 0;
	if (name != NULL)
	{
		Name = name;
		Name.ToUpper();
	}
	else if (lumpnum < 0)
	{
		Name = FString();
	}
	else
	{
		fileSystem.GetFileShortName (Name, lumpnum);
	}
}

FTexture::~FTexture ()
{
	if (areas != nullptr) delete[] areas;
	areas = nullptr;
}

//===========================================================================
//
// FTexture::GetBgraBitmap
//
// Default returns just an empty bitmap. This needs to be overridden by
// any subclass that actually does return a software pixel buffer.
//
//===========================================================================

FBitmap FTexture::GetBgraBitmap(const PalEntry *remap, int *ptrans)
{
	FBitmap bmp;
	bmp.Create(Width, Height);
	return bmp;
}

void FTexture::SetDisplaySize(int fitwidth, int fitheight)
{
	Scale.X = double(Width) / fitwidth;
	Scale.Y =double(Height) / fitheight;
	// compensate for roundoff errors
	if (int(Scale.X * fitwidth) != Width) Scale.X += (1 / 65536.);
	if (int(Scale.Y * fitheight) != Height) Scale.Y += (1 / 65536.);
}

//====================================================================
//
// CheckRealHeight
//
// Checks the posts in a texture and returns the lowest row (plus one)
// of the texture that is actually used.
//
//====================================================================

int FTexture::CheckRealHeight()
{
	auto pixels = Get8BitPixels(false);
	
	for(int h = GetTexelHeight()-1; h>= 0; h--)
	{
		for(int w = 0; w < GetTexelWidth(); w++)
		{
			if (pixels[h + w * GetTexelHeight()] != 0)
			{
				// Scale maxy before returning it
				h = int((h * 2) / Scale.Y);
				h = (h >> 1) + (h & 1);
				return h;
			}
		}
	}
	return 0;
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
		const char *path;
		RefCountedPtr<FTexture> FGameTexture::*pointer;
	};

	static AutoTextureSearchPath autosearchpaths[] =
	{
		{ "brightmaps/", &FGameTexture::Brightmap }, // For backwards compatibility, only for short names
	{ "materials/brightmaps/", &FGameTexture::Brightmap },
	{ "materials/normalmaps/", &FGameTexture::Normal },
	{ "materials/specular/", &FGameTexture::Specular },
	{ "materials/metallic/", &FGameTexture::Metallic },
	{ "materials/roughness/", &FGameTexture::Roughness },
	{ "materials/ao/", &FGameTexture::AmbientOcclusion }
	};


	int startindex = Base->bFullNameTexture ? 1 : 0;
	FString searchname = GetName();

	if (Base->bFullNameTexture)
	{
		auto dot = searchname.LastIndexOf('.');
		auto slash = searchname.LastIndexOf('/');
		if (dot > slash) searchname.Truncate(dot);
	}

	for (size_t i = 0; i < countof(autosearchpaths); i++)
	{
		auto &layer = autosearchpaths[i];
		if (this->*(layer.pointer) == nullptr)	// only if no explicit assignment had been done.
		{
			FStringf lookup("%s%s%s", layer.path, Base->bFullNameTexture ? "" : "auto/", searchname.GetChars());
			auto lump = fileSystem.CheckNumForFullName(lookup, false, ns_global, true);
			if (lump != -1)
			{
				auto bmtex = TexMan.FindGameTexture(fileSystem.GetFileFullName(lump), ETextureType::Any, FTextureManager::TEXMAN_TryAny);
				if (bmtex != nullptr)
				{
					bmtex->GetTexture()->bMasked = false;
					this->*(layer.pointer) = bmtex->GetTexture();
				}
			}
		}
	}
}

//===========================================================================
// 
// Checks if the texture has a default brightmap and creates it if so
//
//===========================================================================
void FGameTexture::CreateDefaultBrightmap()
{
	auto tex = GetTexture();
	if (!tex->bBrightmapChecked)
	{
		// Check for brightmaps
		if (tex->GetImage() && tex->GetImage()->UseGamePalette() && GPalette.HasGlobalBrightmap &&
			GetUseType() != ETextureType::Decal && GetUseType() != ETextureType::MiscPatch && GetUseType() != ETextureType::FontChar &&
			Brightmap == nullptr)
		{
			// May have one - let's check when we use this texture
			auto texbuf = tex->Get8BitPixels(false);
			const int white = ColorMatcher.Pick(255, 255, 255);

			int size = GetTexelWidth() * GetTexelHeight();
			for (int i = 0; i<size; i++)
			{
				if (GPalette.GlobalBrightmap.Remap[texbuf[i]] == white)
				{
					// Create a brightmap
					DPrintf(DMSG_NOTIFY, "brightmap created for texture '%s'\n", GetName().GetChars());
					Brightmap = CreateBrightmapTexture(tex->GetImage());
					tex->bBrightmapChecked = true;
					//TexMan.AddGameTexture(MakeGameTexture(tex->Brightmap));
					return;
				}
			}
			// No bright pixels found
			DPrintf(DMSG_SPAMMY, "No bright pixels found in texture '%s'\n", GetName().GetChars());
			tex->bBrightmapChecked = true;
		}
		else
		{
			// does not have one so set the flag to 'done'
			tex->bBrightmapChecked = true;
		}
	}
}


//==========================================================================
//
// Calculates glow color for a texture
//
//==========================================================================

void FTexture::GetGlowColor(float *data)
{
	if (bGlowing && GlowColor == 0)
	{
		auto buffer = GetBgraBitmap(nullptr);
		GlowColor = averageColor((uint32_t*)buffer.GetPixels(), buffer.GetWidth() * buffer.GetHeight(), 153);

		// Black glow equals nothing so switch glowing off
		if (GlowColor == 0) bGlowing = false;
	}
	data[0] = GlowColor.r / 255.0f;
	data[1] = GlowColor.g / 255.0f;
	data[2] = GlowColor.b / 255.0f;
}

//===========================================================================
//
//
//
//===========================================================================

int FTexture::GetAreas(FloatRect** pAreas) const
{
	if (shaderindex == SHADER_Default)	// texture splitting can only be done if there's no attached effects
	{
		*pAreas = areas;
		return areacount;
	}
	else
	{
		return 0;
	}
}

//===========================================================================
// 
//	Finds gaps in the texture which can be skipped by the renderer
//  This was mainly added to speed up one area in E4M6 of 007LTSD
//
//===========================================================================

bool FTexture::FindHoles(const unsigned char * buffer, int w, int h)
{
	const unsigned char * li;
	int y, x;
	int startdraw, lendraw;
	int gaps[5][2];
	int gapc = 0;


	// already done!
	if (areacount) return false;
	areacount = -1;	//whatever happens next, it shouldn't be done twice!

							// large textures are excluded for performance reasons
	if (h>512) return false;

	startdraw = -1;
	lendraw = 0;
	for (y = 0; y<h; y++)
	{
		li = buffer + w * y * 4 + 3;

		for (x = 0; x<w; x++, li += 4)
		{
			if (*li != 0) break;
		}

		if (x != w)
		{
			// non - transparent
			if (startdraw == -1)
			{
				startdraw = y;
				// merge transparent gaps of less than 16 pixels into the last drawing block
				if (gapc && y <= gaps[gapc - 1][0] + gaps[gapc - 1][1] + 16)
				{
					gapc--;
					startdraw = gaps[gapc][0];
					lendraw = y - startdraw;
				}
				if (gapc == 4) return false;	// too many splits - this isn't worth it
			}
			lendraw++;
		}
		else if (startdraw != -1)
		{
			if (lendraw == 1) lendraw = 2;
			gaps[gapc][0] = startdraw;
			gaps[gapc][1] = lendraw;
			gapc++;

			startdraw = -1;
			lendraw = 0;
		}
	}
	if (startdraw != -1)
	{
		gaps[gapc][0] = startdraw;
		gaps[gapc][1] = lendraw;
		gapc++;
	}
	if (startdraw == 0 && lendraw == h) return false;	// nothing saved so don't create a split list

	if (gapc > 0)
	{
		FloatRect * rcs = new FloatRect[gapc];

		for (x = 0; x < gapc; x++)
		{
			// gaps are stored as texture (u/v) coordinates
			rcs[x].width = rcs[x].left = -1.0f;
			rcs[x].top = (float)gaps[x][0] / (float)h;
			rcs[x].height = (float)gaps[x][1] / (float)h;
		}
		areas = rcs;
	}
	else areas = nullptr;
	areacount = gapc;

	return true;
}

//----------------------------------------------------------------------------
//
//
//
//----------------------------------------------------------------------------

void FTexture::CheckTrans(unsigned char * buffer, int size, int trans)
{
	if (bTranslucent == -1)
	{
		bTranslucent = trans;
		if (trans == -1)
		{
			uint32_t * dwbuf = (uint32_t*)buffer;
			for (int i = 0; i<size; i++)
			{
				uint32_t alpha = dwbuf[i] >> 24;

				if (alpha != 0xff && alpha != 0)
				{
					bTranslucent = 1;
					return;
				}
			}
			bTranslucent = 0;
		}
	}
}


//===========================================================================
// 
// smooth the edges of transparent fields in the texture
//
//===========================================================================

#ifdef WORDS_BIGENDIAN
#define MSB 0
#define SOME_MASK 0xffffff00
#else
#define MSB 3
#define SOME_MASK 0x00ffffff
#endif

#define CHKPIX(ofs) (l1[(ofs)*4+MSB]==255 ? (( ((uint32_t*)l1)[0] = ((uint32_t*)l1)[ofs]&SOME_MASK), trans=true ) : false)

bool FTexture::SmoothEdges(unsigned char * buffer, int w, int h)
{
	int x, y;
	bool trans = buffer[MSB] == 0; // If I set this to false here the code won't detect textures 
								   // that only contain transparent pixels.
	bool semitrans = false;
	unsigned char * l1;

	if (h <= 1 || w <= 1) return false;  // makes (a) no sense and (b) doesn't work with this code!

	l1 = buffer;


	if (l1[MSB] == 0 && !CHKPIX(1)) CHKPIX(w);
	else if (l1[MSB]<255) semitrans = true;
	l1 += 4;
	for (x = 1; x<w - 1; x++, l1 += 4)
	{
		if (l1[MSB] == 0 && !CHKPIX(-1) && !CHKPIX(1)) CHKPIX(w);
		else if (l1[MSB]<255) semitrans = true;
	}
	if (l1[MSB] == 0 && !CHKPIX(-1)) CHKPIX(w);
	else if (l1[MSB]<255) semitrans = true;
	l1 += 4;

	for (y = 1; y<h - 1; y++)
	{
		if (l1[MSB] == 0 && !CHKPIX(-w) && !CHKPIX(1)) CHKPIX(w);
		else if (l1[MSB]<255) semitrans = true;
		l1 += 4;
		for (x = 1; x<w - 1; x++, l1 += 4)
		{
			if (l1[MSB] == 0 && !CHKPIX(-w) && !CHKPIX(-1) && !CHKPIX(1) && !CHKPIX(-w - 1) && !CHKPIX(-w + 1) && !CHKPIX(w - 1) && !CHKPIX(w + 1)) CHKPIX(w);
			else if (l1[MSB]<255) semitrans = true;
		}
		if (l1[MSB] == 0 && !CHKPIX(-w) && !CHKPIX(-1)) CHKPIX(w);
		else if (l1[MSB]<255) semitrans = true;
		l1 += 4;
	}

	if (l1[MSB] == 0 && !CHKPIX(-w)) CHKPIX(1);
	else if (l1[MSB]<255) semitrans = true;
	l1 += 4;
	for (x = 1; x<w - 1; x++, l1 += 4)
	{
		if (l1[MSB] == 0 && !CHKPIX(-w) && !CHKPIX(-1)) CHKPIX(1);
		else if (l1[MSB]<255) semitrans = true;
	}
	if (l1[MSB] == 0 && !CHKPIX(-w)) CHKPIX(-1);
	else if (l1[MSB]<255) semitrans = true;

	return trans || semitrans;
}

//===========================================================================
// 
// Post-process the texture data after the buffer has been created
//
//===========================================================================

bool FTexture::ProcessData(unsigned char * buffer, int w, int h, bool ispatch)
{
	if (bMasked)
	{
		bMasked = SmoothEdges(buffer, w, h);
		if (bMasked && !ispatch) FindHoles(buffer, w, h);
	}
	return true;
}

//===========================================================================
// 
//	Initializes the buffer for the texture data
//
//===========================================================================

FTextureBuffer FTexture::CreateTexBuffer(int translation, int flags)
{
	FTextureBuffer result;

	unsigned char * buffer = nullptr;
	int W, H;
	int isTransparent = -1;
	bool checkonly = !!(flags & CTF_CheckOnly);

	int exx = !!(flags & CTF_Expand);

	W = GetTexelWidth() + 2 * exx;
	H = GetTexelHeight() + 2 * exx;

	if (!checkonly)
	{
		buffer = new unsigned char[W*(H + 1) * 4];
		memset(buffer, 0, W * (H + 1) * 4);

		auto remap = translation <= 0 ? nullptr : GPalette.TranslationToTable(translation);
		if (remap) translation = remap->Index;
		FBitmap bmp(buffer, W * 4, W, H);

		int trans;
		auto Pixels = GetBgraBitmap(remap? remap->Palette : nullptr, &trans);
		bmp.Blit(exx, exx, Pixels);

		if (remap == nullptr)
		{
			CheckTrans(buffer, W*H, trans);
			isTransparent = bTranslucent;
		}
		else
		{
			isTransparent = 0;
			// A translated image is not conclusive for setting the texture's transparency info.
		}
	}

	if (GetImage())
	{
		FContentIdBuilder builder;
		builder.id = 0;
		builder.imageID = GetImage()->GetId();
		builder.translation = MAX(0, translation);
		builder.expand = exx;
		result.mContentId = builder.id;
	}
	else result.mContentId = 0;	// for non-image backed textures this has no meaning so leave it at 0.

	result.mBuffer = buffer;
	result.mWidth = W;
	result.mHeight = H;

	// Only do postprocessing for image-backed textures. (i.e. not for the burn texture which can also pass through here.)
	if (GetImage() && flags & CTF_ProcessData) 
	{
		if (flags & CTF_Upscale) CreateUpsampledTextureBuffer(result, !!isTransparent, checkonly);
		if (!checkonly) ProcessData(result.mBuffer, result.mWidth, result.mHeight, false);
	}

	return result;
}

//===========================================================================
// 
// Dummy texture for the 0-entry.
//
//===========================================================================

bool FTexture::DetermineTranslucency()
{
	if (!bHasCanvas)
	{
		// This will calculate all we need, so just discard the result.
		CreateTexBuffer(0);
	}
	else
	{
		bTranslucent = 0;
	}
	return !!bTranslucent;
}


void FTexture::CleanHardwareTextures(bool reallyclean)
{
	SystemTextures.Clean(reallyclean);
}

//===========================================================================
// 
// the default just returns an empty texture.
//
//===========================================================================

TArray<uint8_t> FTexture::Get8BitPixels(bool alphatex)
{
	TArray<uint8_t> Pixels(Width * Height, true);
	memset(Pixels.Data(), 0, Width * Height);
	return Pixels;
}

//===========================================================================
// 
// Checks if a sprite may be expanded with an empty frame
//
//===========================================================================

bool FGameTexture::ShouldExpandSprite()
{
	if (Base->bExpandSprite != -1) return Base->bExpandSprite;
	if (isWarped() || isHardwareCanvas() || GetShaderIndex() != SHADER_Default)
	{
		Base->bExpandSprite = false;
		return false;
	}
	if (Brightmap != NULL && (GetTexelWidth() != Brightmap->GetTexelWidth() || GetTexelHeight() != Brightmap->GetTexelHeight()))
	{
		// do not expand if the brightmap's size differs.
		Base->bExpandSprite = false;
		return false;
	}
	Base->bExpandSprite = true;
	return true;
}

//===========================================================================
// 
//  Finds empty space around the texture. 
//  Used for sprites that got placed into a huge empty frame.
//
//===========================================================================

bool FTexture::TrimBorders(uint16_t* rect)
{

	auto texbuffer = CreateTexBuffer(0);
	int w = texbuffer.mWidth;
	int h = texbuffer.mHeight;
	auto Buffer = texbuffer.mBuffer;

	if (texbuffer.mBuffer == nullptr)
	{
		return false;
	}
	if (w != Width || h != Height)
	{
		// external Hires replacements cannot be trimmed.
		return false;
	}

	int size = w * h;
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

	for (first = 0; first < size; first++)
	{
		if (Buffer[first * 4 + 3] != 0) break;
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

	for (last = size - 1; last >= first; last--)
	{
		if (Buffer[last * 4 + 3] != 0) break;
	}

	rect[1] = first / w;
	rect[3] = 1 + last / w - rect[1];

	rect[0] = 0;
	rect[2] = w;

	unsigned char* bufferoff = Buffer + (rect[1] * w * 4);
	h = rect[3];

	for (int x = 0; x < w; x++)
	{
		for (int y = 0; y < h; y++)
		{
			if (bufferoff[(x + y * w) * 4 + 3] != 0) goto outl;
		}
		rect[0]++;
	}
outl:
	rect[2] -= rect[0];

	for (int x = w - 1; rect[2] > 1; x--)
	{
		for (int y = 0; y < h; y++)
		{
			if (bufferoff[(x + y * w) * 4 + 3] != 0)
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
			spi.mTrimResult = Base->TrimBorders(spi.trim);	// get the trim size before adding the empty frame
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
	auto leftOffset = GetLeftOffsetHW();
	auto topOffset = GetTopOffsetHW();

	float fxScale = (float)Base->Scale.X;
	float fyScale = (float)Base->Scale.Y;

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
// Create a hardware texture for this texture image.
//
//===========================================================================

IHardwareTexture* FTexture::GetHardwareTexture(int translation, int scaleflags)
{
	//if (UseType != ETextureType::Null)
	{
		IHardwareTexture* hwtex = SystemTextures.GetHardwareTexture(translation, scaleflags);
		if (hwtex == nullptr)
		{
			hwtex = CreateHardwareTexture();
			SystemTextures.AddHardwareTexture(translation, scaleflags, hwtex);
		}
		return hwtex;
	}
	return nullptr;
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
	float scale = fabs(mScale.Y);
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
	float scale = fabs(mScale.X);
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
		float tscale = fabs(mTempScale.X);
		if (tscale == 1.f) return (float)mRenderWidth;
		else return mWidth / fabs(tscale);
	}
	else return (float)mWidth;
}


//===========================================================================
//
// Retrieve texture coordinate info for per-wall scaling
//
//===========================================================================

void FTexCoordInfo::GetFromTexture(FTexture *tex, float x, float y, bool forceworldpanning)
{
	if (x == 1.f)
	{
		mRenderWidth = tex->GetDisplayWidth();
		mScale.X = (float)tex->Scale.X;
		mTempScale.X = 1.f;
	}
	else
	{
		float scale_x = x * (float)tex->Scale.X;
		mRenderWidth = xs_CeilToInt(tex->GetTexelWidth() / scale_x);
		mScale.X = scale_x;
		mTempScale.X = x;
	}

	if (y == 1.f)
	{
		mRenderHeight = tex->GetDisplayHeight();
		mScale.Y = (float)tex->Scale.Y;
		mTempScale.Y = 1.f;
	}
	else
	{
		float scale_y = y * (float)tex->Scale.Y;
		mRenderHeight = xs_CeilToInt(tex->GetTexelHeight() / scale_y);
		mScale.Y = scale_y;
		mTempScale.Y = y;
	}
	if (tex->bHasCanvas)
	{
		mScale.Y = -mScale.Y;
		mRenderHeight = -mRenderHeight;
	}
	mWorldPanning = tex->bWorldPanning || forceworldpanning;
	mWidth = tex->GetTexelWidth();
}

void FTexCoordInfo::GetFromTexture(FGameTexture* tex, float x, float y, bool forceworldpanning)
{
	GetFromTexture(tex->GetTexture(), x, y, forceworldpanning);
}

//==========================================================================
//
// this must be copied back to textures.cpp later.
//
//==========================================================================

FWrapperTexture::FWrapperTexture(int w, int h, int bits)
{
	Width = w;
	Height = h;
	Format = bits;
	bNoCompress = true;
	auto hwtex = CreateHardwareTexture();
	// todo: Initialize here.
	SystemTextures.AddHardwareTexture(0, false, hwtex);
}


FGameTexture::~FGameTexture()
{
	FGameTexture* link = fileSystem.GetLinkedTexture(GetSourceLump());
	if (link == this) fileSystem.SetLinkedTexture(GetSourceLump(), nullptr);
	if (SoftwareTexture != nullptr)
	{
		delete SoftwareTexture;
		SoftwareTexture = nullptr;
	}
	for (auto &mat : Material)
	{
		if (mat != nullptr) DeleteMaterial(mat);
		mat = nullptr;
	}

}

bool FGameTexture::isUserContent() const
{
	int filenum = fileSystem.GetFileContainer(Base->GetSourceLump());
	return (filenum > fileSystem.GetMaxIwadNum());
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
