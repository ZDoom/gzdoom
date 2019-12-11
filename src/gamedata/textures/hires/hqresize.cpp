/*
** gl_hqresize.cpp
** Contains high quality upsampling functions.
** So far Scale2x/3x/4x as described in http://scale2x.sourceforge.net/
** are implemented.
**
**---------------------------------------------------------------------------
** Copyright 2008 Benjamin Berkels
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
*/

#include "c_cvars.h"
#include "v_video.h"
#include "hqnx/hqx.h"
#ifdef HAVE_MMX
#include "hqnx_asm/hqnx_asm.h"
#endif
#include "xbr/xbrz.h"
#include "xbr/xbrz_old.h"
#include "parallel_for.h"
#include "hwrenderer/textures/hw_material.h"

EXTERN_CVAR(Int, gl_texture_hqresizemult)
CUSTOM_CVAR(Int, gl_texture_hqresizemode, 0, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOINITCALL)
{
	if (self < 0 || self > 6)
		self = 0;
	if ((gl_texture_hqresizemult > 4) && (self < 4) && (self > 0))
		gl_texture_hqresizemult = 4;
	TexMan.FlushAll();
}

CUSTOM_CVAR(Int, gl_texture_hqresizemult, 1, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOINITCALL)
{
	if (self < 1 || self > 6)
		self = 1;
	if ((self > 4) && (gl_texture_hqresizemode < 4) && (gl_texture_hqresizemode > 0))
		self = 4;
	TexMan.FlushAll();
}

CUSTOM_CVAR(Int, gl_texture_hqresize_maxinputsize, 512, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOINITCALL)
{
	if (self > 1024) self = 1024;
	TexMan.FlushAll();
}

CUSTOM_CVAR(Int, gl_texture_hqresize_targets, 7, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOINITCALL)
{
	TexMan.FlushAll();
}

CVAR (Flag, gl_texture_hqresize_textures, gl_texture_hqresize_targets, 1);
CVAR (Flag, gl_texture_hqresize_sprites, gl_texture_hqresize_targets, 2);
CVAR (Flag, gl_texture_hqresize_fonts, gl_texture_hqresize_targets, 4);

CVAR(Bool, gl_texture_hqresize_multithread, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG);

CUSTOM_CVAR(Int, gl_texture_hqresize_mt_width, 16, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	if (self < 2)    self = 2;
	if (self > 1024) self = 1024;
}

CUSTOM_CVAR(Int, gl_texture_hqresize_mt_height, 4, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	if (self < 2)    self = 2;
	if (self > 1024) self = 1024;
}

CVAR(Int, xbrz_colorformat, 0, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)

static void xbrzApplyOptions()
{
	if (gl_texture_hqresizemult != 0 && (gl_texture_hqresizemode == 4 || gl_texture_hqresizemode == 5))
	{
		if (xbrz_colorformat == 0)
		{
			Printf("Changing xBRZ options requires a restart when buffered color format is used.\n"
				"To avoid this at cost of scaling performance, set xbrz_colorformat CVAR to non-zero value.");
		}
		else
		{
			TexMan.FlushAll();
		}
	}
}

#define XBRZ_CVAR(NAME, VALUE) \
	CUSTOM_CVAR(Float, xbrz_##NAME, VALUE, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOINITCALL) { xbrzApplyOptions(); }

XBRZ_CVAR(luminanceweight, 1.f)
XBRZ_CVAR(equalcolortolerance, 30.f)
XBRZ_CVAR(centerdirectionbias, 4.f)
XBRZ_CVAR(dominantdirectionthreshold, 3.6f)
XBRZ_CVAR(steepdirectionthreshold, 2.2f)

#undef XBRZ_CVAR

static void scale2x ( uint32_t* inputBuffer, uint32_t* outputBuffer, int inWidth, int inHeight )
{
	const int width = 2* inWidth;
	const int height = 2 * inHeight;

	for ( int i = 0; i < inWidth; ++i )
	{
		const int iMinus = (i > 0) ? (i-1) : 0;
		const int iPlus = (i < inWidth - 1 ) ? (i+1) : i;
		for ( int j = 0; j < inHeight; ++j )
		{
			const int jMinus = (j > 0) ? (j-1) : 0;
			const int jPlus = (j < inHeight - 1 ) ? (j+1) : j;
			const uint32_t A = inputBuffer[ iMinus +inWidth*jMinus];
			const uint32_t B = inputBuffer[ iMinus +inWidth*j    ];
			const uint32_t C = inputBuffer[ iMinus +inWidth*jPlus];
			const uint32_t D = inputBuffer[ i     +inWidth*jMinus];
			const uint32_t E = inputBuffer[ i     +inWidth*j    ];
			const uint32_t F = inputBuffer[ i     +inWidth*jPlus];
			const uint32_t G = inputBuffer[ iPlus +inWidth*jMinus];
			const uint32_t H = inputBuffer[ iPlus +inWidth*j    ];
			const uint32_t I = inputBuffer[ iPlus +inWidth*jPlus];
			if (B != H && D != F) {
				outputBuffer[2*i   + width*2*j    ] = D == B ? D : E;
				outputBuffer[2*i   + width*(2*j+1)] = B == F ? F : E;
				outputBuffer[2*i+1 + width*2*j    ] = D == H ? D : E;
				outputBuffer[2*i+1 + width*(2*j+1)] = H == F ? F : E;
			} else {
				outputBuffer[2*i   + width*2*j    ] = E;
				outputBuffer[2*i   + width*(2*j+1)] = E;
				outputBuffer[2*i+1 + width*2*j    ] = E;
				outputBuffer[2*i+1 + width*(2*j+1)] = E;
			}
		}
	}
}

static void scale3x ( uint32_t* inputBuffer, uint32_t* outputBuffer, int inWidth, int inHeight )
{
	const int width = 3* inWidth;
	const int height = 3 * inHeight;

	for ( int i = 0; i < inWidth; ++i )
	{
		const int iMinus = (i > 0) ? (i-1) : 0;
		const int iPlus = (i < inWidth - 1 ) ? (i+1) : i;
		for ( int j = 0; j < inHeight; ++j )
		{
			const int jMinus = (j > 0) ? (j-1) : 0;
			const int jPlus = (j < inHeight - 1 ) ? (j+1) : j;
			const uint32_t A = inputBuffer[ iMinus +inWidth*jMinus];
			const uint32_t B = inputBuffer[ iMinus +inWidth*j    ];
			const uint32_t C = inputBuffer[ iMinus +inWidth*jPlus];
			const uint32_t D = inputBuffer[ i     +inWidth*jMinus];
			const uint32_t E = inputBuffer[ i     +inWidth*j    ];
			const uint32_t F = inputBuffer[ i     +inWidth*jPlus];
			const uint32_t G = inputBuffer[ iPlus +inWidth*jMinus];
			const uint32_t H = inputBuffer[ iPlus +inWidth*j    ];
			const uint32_t I = inputBuffer[ iPlus +inWidth*jPlus];
			if (B != H && D != F) {
				outputBuffer[3*i   + width*3*j    ] = D == B ? D : E;
				outputBuffer[3*i   + width*(3*j+1)] = (D == B && E != C) || (B == F && E != A) ? B : E;
				outputBuffer[3*i   + width*(3*j+2)] = B == F ? F : E;
				outputBuffer[3*i+1 + width*3*j    ] = (D == B && E != G) || (D == H && E != A) ? D : E;
				outputBuffer[3*i+1 + width*(3*j+1)] = E;
				outputBuffer[3*i+1 + width*(3*j+2)] = (B == F && E != I) || (H == F && E != C) ? F : E;
				outputBuffer[3*i+2 + width*3*j    ] = D == H ? D : E;
				outputBuffer[3*i+2 + width*(3*j+1)] = (D == H && E != I) || (H == F && E != G) ? H : E;
				outputBuffer[3*i+2 + width*(3*j+2)] = H == F ? F : E;
			} else {
				outputBuffer[3*i   + width*3*j    ] = E;
				outputBuffer[3*i   + width*(3*j+1)] = E;
				outputBuffer[3*i   + width*(3*j+2)] = E;
				outputBuffer[3*i+1 + width*3*j    ] = E;
				outputBuffer[3*i+1 + width*(3*j+1)] = E;
				outputBuffer[3*i+1 + width*(3*j+2)] = E;
				outputBuffer[3*i+2 + width*3*j    ] = E;
				outputBuffer[3*i+2 + width*(3*j+1)] = E;
				outputBuffer[3*i+2 + width*(3*j+2)] = E;
			}
		}
	}
}

static void scale4x ( uint32_t* inputBuffer, uint32_t* outputBuffer, int inWidth, int inHeight )
{
	int width = 2* inWidth;
	int height = 2 * inHeight;
	uint32_t * buffer2x = new uint32_t[width*height];

	scale2x ( reinterpret_cast<uint32_t*> ( inputBuffer ), reinterpret_cast<uint32_t*> ( buffer2x ), inWidth, inHeight );
	width *= 2;
	height *= 2;
	scale2x ( reinterpret_cast<uint32_t*> ( buffer2x ), reinterpret_cast<uint32_t*> ( outputBuffer ), 2*inWidth, 2*inHeight );
	delete[] buffer2x;
}

static unsigned char *scaleNxHelper( void (*scaleNxFunction) ( uint32_t* , uint32_t* , int , int),
							  const int N,
							  unsigned char *inputBuffer,
							  const int inWidth,
							  const int inHeight,
							  int &outWidth,
							  int &outHeight )
{
	outWidth = N * inWidth;
	outHeight = N *inHeight;
	unsigned char * newBuffer = new unsigned char[outWidth*outHeight*4];

	scaleNxFunction ( reinterpret_cast<uint32_t*> ( inputBuffer ), reinterpret_cast<uint32_t*> ( newBuffer ), inWidth, inHeight );
	delete[] inputBuffer;
	return newBuffer;
}

static void normalNx ( uint32_t* inputBuffer, uint32_t* outputBuffer, int inWidth, int inHeight, int size )
{
	const int width = size * inWidth;
	const int height = size * inHeight;

	for ( int i = 0; i < inWidth; ++i )
	{
		for ( int j = 0; j < inHeight; ++j )
		{
			const uint32_t E = inputBuffer[ i     +inWidth*j    ];
			for ( int k = 0; k < size; k++ )
			{
				for ( int l = 0; l < size; l++ )
				{
					outputBuffer[size*i+k + width*(size*j+l)] = E;
				}
			}
		}
	}
}

static unsigned char *normalNxHelper( void (normalNxFunction) ( uint32_t* , uint32_t* , int , int, int),
							  const int N,
							  unsigned char *inputBuffer,
							  const int inWidth,
							  const int inHeight,
							  int &outWidth,
							  int &outHeight )
{
	outWidth = N * inWidth;
	outHeight = N *inHeight;
	unsigned char * newBuffer = new unsigned char[outWidth*outHeight*4];

	normalNxFunction ( reinterpret_cast<uint32_t*> ( inputBuffer ), reinterpret_cast<uint32_t*> ( newBuffer ), inWidth, inHeight, N );
	delete[] inputBuffer;
	return newBuffer;
}

#ifdef HAVE_MMX
static unsigned char *hqNxAsmHelper( void (*hqNxFunction) ( int*, unsigned char*, int, int, int ),
							  const int N,
							  unsigned char *inputBuffer,
							  const int inWidth,
							  const int inHeight,
							  int &outWidth,
							  int &outHeight )
{
	outWidth = N * inWidth;
	outHeight = N *inHeight;

	static int initdone = false;

	if (!initdone)
	{
		HQnX_asm::InitLUTs();
		initdone = true;
	}

	HQnX_asm::CImage cImageIn;
	cImageIn.SetImage(inputBuffer, inWidth, inHeight, 32);
	cImageIn.Convert32To17();

	unsigned char * newBuffer = new unsigned char[outWidth*outHeight*4];
	hqNxFunction( reinterpret_cast<int*>(cImageIn.m_pBitmap), newBuffer, cImageIn.m_Xres, cImageIn.m_Yres, outWidth*4 );
	delete[] inputBuffer;
	return newBuffer;
}
#endif

static unsigned char *hqNxHelper( void (HQX_CALLCONV *hqNxFunction) ( unsigned*, unsigned*, int, int ),
							  const int N,
							  unsigned char *inputBuffer,
							  const int inWidth,
							  const int inHeight,
							  int &outWidth,
							  int &outHeight )
{
	static int initdone = false;

	if (!initdone)
	{
		hqxInit();
		initdone = true;
	}
	outWidth = N * inWidth;
	outHeight = N *inHeight;

	unsigned char * newBuffer = new unsigned char[outWidth*outHeight*4];
	hqNxFunction( reinterpret_cast<unsigned*>(inputBuffer), reinterpret_cast<unsigned*>(newBuffer), inWidth, inHeight );
	delete[] inputBuffer;
	return newBuffer;
}


template <typename ConfigType>
void xbrzSetupConfig(ConfigType& cfg);

template <>
void xbrzSetupConfig(xbrz::ScalerCfg& cfg)
{
	cfg.luminanceWeight = xbrz_luminanceweight;
	cfg.equalColorTolerance = xbrz_equalcolortolerance;
	cfg.centerDirectionBias = xbrz_centerdirectionbias;
	cfg.dominantDirectionThreshold = xbrz_dominantdirectionthreshold;
	cfg.steepDirectionThreshold = xbrz_steepdirectionthreshold;
}

template <>
void xbrzSetupConfig(xbrz_old::ScalerCfg& cfg)
{
	cfg.luminanceWeight_ = xbrz_luminanceweight;
	cfg.equalColorTolerance_ = xbrz_equalcolortolerance;
	cfg.dominantDirectionThreshold = xbrz_dominantdirectionthreshold;
	cfg.steepDirectionThreshold = xbrz_steepdirectionthreshold;
}

template <typename ConfigType>
static unsigned char *xbrzHelper( void (*xbrzFunction) ( size_t, const uint32_t*, uint32_t*, int, int, xbrz::ColorFormat, const ConfigType&, int, int ),
							  const int N,
							  unsigned char *inputBuffer,
							  const int inWidth,
							  const int inHeight,
							  int &outWidth,
							  int &outHeight )
{
	outWidth = N * inWidth;
	outHeight = N *inHeight;

	unsigned char * newBuffer = new unsigned char[outWidth*outHeight*4];
	
	const int thresholdWidth  = gl_texture_hqresize_mt_width;
	const int thresholdHeight = gl_texture_hqresize_mt_height;

	ConfigType cfg;
	xbrzSetupConfig(cfg);

	const xbrz::ColorFormat colorFormat = xbrz_colorformat == 0
		? xbrz::ColorFormat::ARGB
		: xbrz::ColorFormat::ARGB_UNBUFFERED;

	if (gl_texture_hqresize_multithread
		&& inWidth  > thresholdWidth
		&& inHeight > thresholdHeight)
	{
		parallel_for(inHeight, thresholdHeight, [=, &cfg](int sliceY)
		{
			xbrzFunction(N, reinterpret_cast<uint32_t*>(inputBuffer), reinterpret_cast<uint32_t*>(newBuffer),
				inWidth, inHeight, colorFormat, cfg, sliceY, sliceY + thresholdHeight);
		});
	}
	else
	{
		xbrzFunction(N, reinterpret_cast<uint32_t*>(inputBuffer), reinterpret_cast<uint32_t*>(newBuffer),
			inWidth, inHeight, colorFormat, cfg, 0, std::numeric_limits<int>::max());
	}

	delete[] inputBuffer;
	return newBuffer;
}

static void xbrzOldScale(size_t factor, const uint32_t* src, uint32_t* trg, int srcWidth, int srcHeight, xbrz::ColorFormat colFmt, const xbrz_old::ScalerCfg& cfg, int yFirst, int yLast)
{
	xbrz_old::scale(factor, src, trg, srcWidth, srcHeight, cfg, yFirst, yLast);
}


//===========================================================================
// 
// [BB] Upsamples the texture in texbuffer.mBuffer, frees texbuffer.mBuffer and returns
//  the upsampled buffer.
//
//===========================================================================
void FTexture::CreateUpsampledTextureBuffer(FTextureBuffer &texbuffer, bool hasAlpha, bool checkonly)
{
	// [BB] Make sure that inWidth and inHeight denote the size of
	// the returned buffer even if we don't upsample the input buffer.
	int inWidth = texbuffer.mWidth;
	int inHeight = texbuffer.mHeight;

	// [BB] Don't resample if the width or height of the input texture is bigger than gl_texture_hqresize_maxinputsize.
	if ((inWidth > gl_texture_hqresize_maxinputsize) || (inHeight > gl_texture_hqresize_maxinputsize))
		return;

	// [BB] Don't try to upsample textures based off FCanvasTexture. (This should never get here in the first place!)
	if (bHasCanvas)
		return;

	// already scaled?
	if (Scale.X >= 2 && Scale.Y >= 2)
		return;

	switch (UseType)
	{
	case ETextureType::Sprite:
	case ETextureType::SkinSprite:
		if (!(gl_texture_hqresize_targets & 2)) return;
		break;

	case ETextureType::FontChar:
		if (!(gl_texture_hqresize_targets & 4)) return;
		break;

	default:
		if (!(gl_texture_hqresize_targets & 1)) return;
		break;
	}

	int type = gl_texture_hqresizemode;
	int mult = gl_texture_hqresizemult;
#ifdef HAVE_MMX
	// hqNx MMX does not preserve the alpha channel so fall back to C-version for such textures
	if (hasAlpha && type == 3)
	{
		type = 2;
	}
#endif
	// These checks are to ensure consistency of the content ID.
	if (mult < 2 || mult > 6 || type < 1 || type > 6) return;
	if (type < 4 && mult > 4) mult = 4;

	if (!checkonly)
	{
		if (type == 1)
		{
			if (mult == 2)
				texbuffer.mBuffer = scaleNxHelper(&scale2x, 2, texbuffer.mBuffer, inWidth, inHeight, texbuffer.mWidth, texbuffer.mHeight);
			else if (mult == 3)
				texbuffer.mBuffer = scaleNxHelper(&scale3x, 3, texbuffer.mBuffer, inWidth, inHeight, texbuffer.mWidth, texbuffer.mHeight);
			else if (mult == 4)
				texbuffer.mBuffer = scaleNxHelper(&scale4x, 4, texbuffer.mBuffer, inWidth, inHeight, texbuffer.mWidth, texbuffer.mHeight);
			else return;
		}
		else if (type == 2)
		{
			if (mult == 2)
				texbuffer.mBuffer = hqNxHelper(&hq2x_32, 2, texbuffer.mBuffer, inWidth, inHeight, texbuffer.mWidth, texbuffer.mHeight);
			else if (mult == 3)
				texbuffer.mBuffer = hqNxHelper(&hq3x_32, 3, texbuffer.mBuffer, inWidth, inHeight, texbuffer.mWidth, texbuffer.mHeight);
			else if (mult == 4)
				texbuffer.mBuffer = hqNxHelper(&hq4x_32, 4, texbuffer.mBuffer, inWidth, inHeight, texbuffer.mWidth, texbuffer.mHeight);
			else return;
		}
#ifdef HAVE_MMX
		else if (type == 3)
		{
			if (mult == 2)
				texbuffer.mBuffer = hqNxAsmHelper(&HQnX_asm::hq2x_32, 2, texbuffer.mBuffer, inWidth, inHeight, texbuffer.mWidth, texbuffer.mHeight);
			else if (mult == 3)
				texbuffer.mBuffer = hqNxAsmHelper(&HQnX_asm::hq3x_32, 3, texbuffer.mBuffer, inWidth, inHeight, texbuffer.mWidth, texbuffer.mHeight);
			else if (mult == 4)
				texbuffer.mBuffer = hqNxAsmHelper(&HQnX_asm::hq4x_32, 4, texbuffer.mBuffer, inWidth, inHeight, texbuffer.mWidth, texbuffer.mHeight);
			else return;
		}
#endif
		else if (type == 4)
			texbuffer.mBuffer = xbrzHelper(xbrz::scale, mult, texbuffer.mBuffer, inWidth, inHeight, texbuffer.mWidth, texbuffer.mHeight);
		else if (type == 5)
			texbuffer.mBuffer = xbrzHelper(xbrzOldScale, mult, texbuffer.mBuffer, inWidth, inHeight, texbuffer.mWidth, texbuffer.mHeight);
		else if (type == 6)
			texbuffer.mBuffer = normalNxHelper(&normalNx, mult, texbuffer.mBuffer, inWidth, inHeight, texbuffer.mWidth, texbuffer.mHeight);
		else
			return;
	}
	else
	{
		texbuffer.mWidth *= mult;
		texbuffer.mHeight *= mult;
	}
	// Encode the scaling method in the content ID.
	FContentIdBuilder contentId;
	contentId.id = texbuffer.mContentId;
	contentId.scaler = type;
	contentId.scalefactor = mult;
	texbuffer.mContentId = contentId.id;
}
