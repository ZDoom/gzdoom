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

CUSTOM_CVAR(Int, gl_texture_hqresize, 0, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOINITCALL)
{
	if (self < 0 || self > 24)
	{
		self = 0;
	}
	#ifndef HAVE_MMX
		// This is to allow the menu option to work properly so that these filters can be skipped while cycling through them.
		if (self == 7) self = 10;
		if (self == 8) self = 10;
		if (self == 9) self = 6;
	#endif
	FMaterial::FlushAll();
}

CUSTOM_CVAR(Int, gl_texture_hqresize_maxinputsize, 512, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOINITCALL)
{
	if (self > 1024) self = 1024;
	FMaterial::FlushAll();
}

CUSTOM_CVAR(Int, gl_texture_hqresize_targets, 7, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOINITCALL)
{
	FMaterial::FlushAll();
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


			
static unsigned char *xbrzHelper( void (*xbrzFunction) ( size_t, const uint32_t*, uint32_t*, int, int, xbrz::ColorFormat, const xbrz::ScalerCfg&, int, int ),
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
	
	if (gl_texture_hqresize_multithread
		&& inWidth  > thresholdWidth
		&& inHeight > thresholdHeight)
	{
		parallel_for(inHeight, thresholdHeight, [=](int sliceY)
		{
			xbrzFunction(N, reinterpret_cast<uint32_t*>(inputBuffer), reinterpret_cast<uint32_t*>(newBuffer),
				inWidth, inHeight, xbrz::ColorFormat::ARGB, xbrz::ScalerCfg(), sliceY, sliceY + thresholdHeight);
		});
	}
	else
	{
		xbrzFunction(N, reinterpret_cast<uint32_t*>(inputBuffer), reinterpret_cast<uint32_t*>(newBuffer),
			inWidth, inHeight, xbrz::ColorFormat::ARGB, xbrz::ScalerCfg(), 0, std::numeric_limits<int>::max());
	}

	delete[] inputBuffer;
	return newBuffer;
}

static void xbrzOldScale(size_t factor, const uint32_t* src, uint32_t* trg, int srcWidth, int srcHeight, xbrz::ColorFormat colFmt, const xbrz::ScalerCfg& cfg, int yFirst, int yLast)
{
	static_assert(sizeof(xbrz::ScalerCfg) == sizeof(xbrz_old::ScalerCfg), "ScalerCfg classes have different layout");
	xbrz_old::scale(factor, src, trg, srcWidth, srcHeight, reinterpret_cast<const xbrz_old::ScalerCfg&>(cfg), yFirst, yLast);
}


//===========================================================================
// 
// [BB] Upsamples the texture in inputBuffer, frees inputBuffer and returns
//  the upsampled buffer.
//
//===========================================================================
unsigned char *FTexture::CreateUpsampledTextureBuffer (unsigned char *inputBuffer, const int inWidth, const int inHeight, int &outWidth, int &outHeight, bool hasAlpha )
{
	// [BB] Make sure that outWidth and outHeight denote the size of
	// the returned buffer even if we don't upsample the input buffer.
	outWidth = inWidth;
	outHeight = inHeight;

	// [BB] Don't resample if the width or height of the input texture is bigger than gl_texture_hqresize_maxinputsize.
	if ( ( inWidth > gl_texture_hqresize_maxinputsize ) || ( inHeight > gl_texture_hqresize_maxinputsize ) )
		return inputBuffer;

	// [BB] Don't try to upsample textures based off FCanvasTexture.
	if ( bHasCanvas )
		return inputBuffer;

	// already scaled?
	if (Scale.X >= 2 && Scale.Y >= 2)
		return inputBuffer;

	switch (UseType)
	{
	case ETextureType::Sprite:
	case ETextureType::SkinSprite:
		if (!(gl_texture_hqresize_targets & 2)) return inputBuffer;
		break;

	case ETextureType::FontChar:
		if (!(gl_texture_hqresize_targets & 4)) return inputBuffer;
		break;

	default:
		if (!(gl_texture_hqresize_targets & 1)) return inputBuffer;
		break;
	}

	if (inputBuffer)
	{
		int type = gl_texture_hqresize;
		outWidth = inWidth;
		outHeight = inHeight;
#ifdef HAVE_MMX
		// hqNx MMX does not preserve the alpha channel so fall back to C-version for such textures
		if (hasAlpha && type > 6 && type <= 9)
		{
			type -= 3;
		}
#endif

		switch (type)
		{
		case 1:
			return scaleNxHelper( &scale2x, 2, inputBuffer, inWidth, inHeight, outWidth, outHeight );
		case 2:
			return scaleNxHelper( &scale3x, 3, inputBuffer, inWidth, inHeight, outWidth, outHeight );
		case 3:
			return scaleNxHelper( &scale4x, 4, inputBuffer, inWidth, inHeight, outWidth, outHeight );
		case 4:
			return hqNxHelper( &hq2x_32, 2, inputBuffer, inWidth, inHeight, outWidth, outHeight );
		case 5:
			return hqNxHelper( &hq3x_32, 3, inputBuffer, inWidth, inHeight, outWidth, outHeight );
		case 6:
			return hqNxHelper( &hq4x_32, 4, inputBuffer, inWidth, inHeight, outWidth, outHeight );
#ifdef HAVE_MMX
		case 7:
			return hqNxAsmHelper( &HQnX_asm::hq2x_32, 2, inputBuffer, inWidth, inHeight, outWidth, outHeight );
		case 8:
			return hqNxAsmHelper( &HQnX_asm::hq3x_32, 3, inputBuffer, inWidth, inHeight, outWidth, outHeight );
		case 9:
			return hqNxAsmHelper( &HQnX_asm::hq4x_32, 4, inputBuffer, inWidth, inHeight, outWidth, outHeight );
#endif
		case 10:
		case 11:
		case 12:
			return xbrzHelper(xbrz::scale, type - 8, inputBuffer, inWidth, inHeight, outWidth, outHeight );
			
		case 13:
		case 14:
		case 15:
		case 16:
		case 17:
			return xbrzHelper(xbrzOldScale, type - 11, inputBuffer, inWidth, inHeight, outWidth, outHeight );

		case 18:
		case 19:
			return xbrzHelper(xbrz::scale, type - 13, inputBuffer, inWidth, inHeight, outWidth, outHeight);
		case 20:
			return normalNxHelper( &normalNx, 2, inputBuffer, inWidth, inHeight, outWidth, outHeight );
		case 21:
			return normalNxHelper( &normalNx, 3, inputBuffer, inWidth, inHeight, outWidth, outHeight );
		case 22:
			return normalNxHelper( &normalNx, 4, inputBuffer, inWidth, inHeight, outWidth, outHeight );
		case 23:
			return normalNxHelper( &normalNx, 5, inputBuffer, inWidth, inHeight, outWidth, outHeight );
		case 24:
			return normalNxHelper( &normalNx, 6, inputBuffer, inWidth, inHeight, outWidth, outHeight );
		}
	}
	return inputBuffer;
}
