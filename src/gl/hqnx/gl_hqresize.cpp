/*
** gl_hqresize.cpp
** Contains high quality upsampling functions.
** So far supports Scale2x/3x/4x as described in http://scale2x.sourceforge.net/
** and Maxim Stepin's hq2x/3x/4x.
**
**---------------------------------------------------------------------------
** Copyright 2008-2009 Benjamin Berkels
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

#include "gl_pch.h"
#include "gl_hqresize.h"
#include "gl_intern.h"
#include "c_cvars.h"
// [BB] hqnx scaling is only supported with the MS compiler.
#ifdef _MSC_VER
#include "../hqnx/hqnx.h"
#endif

CUSTOM_CVAR(Int, gl_texture_hqresize, 0, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOINITCALL)
{
#ifdef _MSC_VER
	if (self < 0 || self > 6)
#else
	if (self < 0 || self > 3)
#endif
		self = 0;
	FGLTexture::FlushAll();
}

CUSTOM_CVAR(Int, gl_texture_hqresize_maxinputsize, 512, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOINITCALL)
{
	if (self > 1024) self = 1024;
	FGLTexture::FlushAll();
}

CUSTOM_CVAR(Int, gl_texture_hqresize_targets, 7, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOINITCALL)
{
	FGLTexture::FlushAll();
}

CVAR (Flag, gl_texture_hqresize_textures, gl_texture_hqresize_targets, 1);
CVAR (Flag, gl_texture_hqresize_sprites, gl_texture_hqresize_targets, 2);
CVAR (Flag, gl_texture_hqresize_fonts, gl_texture_hqresize_targets, 4);


static void scale2x ( uint32* inputBuffer, uint32* outputBuffer, int inWidth, int inHeight )
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
			const uint32 A = inputBuffer[ iMinus +inWidth*jMinus];
			const uint32 B = inputBuffer[ iMinus +inWidth*j    ];
			const uint32 C = inputBuffer[ iMinus +inWidth*jPlus];
			const uint32 D = inputBuffer[ i     +inWidth*jMinus];
			const uint32 E = inputBuffer[ i     +inWidth*j    ];
			const uint32 F = inputBuffer[ i     +inWidth*jPlus];
			const uint32 G = inputBuffer[ iPlus +inWidth*jMinus];
			const uint32 H = inputBuffer[ iPlus +inWidth*j    ];
			const uint32 I = inputBuffer[ iPlus +inWidth*jPlus];
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

static void scale3x ( uint32* inputBuffer, uint32* outputBuffer, int inWidth, int inHeight )
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
			const uint32 A = inputBuffer[ iMinus +inWidth*jMinus];
			const uint32 B = inputBuffer[ iMinus +inWidth*j    ];
			const uint32 C = inputBuffer[ iMinus +inWidth*jPlus];
			const uint32 D = inputBuffer[ i     +inWidth*jMinus];
			const uint32 E = inputBuffer[ i     +inWidth*j    ];
			const uint32 F = inputBuffer[ i     +inWidth*jPlus];
			const uint32 G = inputBuffer[ iPlus +inWidth*jMinus];
			const uint32 H = inputBuffer[ iPlus +inWidth*j    ];
			const uint32 I = inputBuffer[ iPlus +inWidth*jPlus];
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

static void scale4x ( uint32* inputBuffer, uint32* outputBuffer, int inWidth, int inHeight )
{
	int width = 2* inWidth;
	int height = 2 * inHeight;
	uint32 * buffer2x = new uint32[width*height];

	scale2x ( reinterpret_cast<uint32*> ( inputBuffer ), reinterpret_cast<uint32*> ( buffer2x ), inWidth, inHeight );
	width *= 2;
	height *= 2;
	scale2x ( reinterpret_cast<uint32*> ( buffer2x ), reinterpret_cast<uint32*> ( outputBuffer ), 2*inWidth, 2*inHeight );
	delete[] buffer2x;
}


static unsigned char *scaleNxHelper( void (*scaleNxFunction) ( uint32* , uint32* , int , int),
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

	scaleNxFunction ( reinterpret_cast<uint32*> ( inputBuffer ), reinterpret_cast<uint32*> ( newBuffer ), inWidth, inHeight );
	delete[] inputBuffer;
	return newBuffer;
}

// [BB] hqnx scaling is only supported with the MS compiler.
#ifdef _MSC_VER
static unsigned char *hqNxHelper( void (*hqNxFunction) ( int*, unsigned char*, int, int, int ),
							  const int N,
							  unsigned char *inputBuffer,
							  const int inWidth,
							  const int inHeight,
							  int &outWidth,
							  int &outHeight )
{
	outWidth = N * inWidth;
	outHeight = N *inHeight;

	CImage cImageIn;
	cImageIn.SetImage(inputBuffer, inWidth, inHeight, 32);
	cImageIn.Convert32To17();

	unsigned char * newBuffer = new unsigned char[outWidth*outHeight*4];
	hqNxFunction( reinterpret_cast<int*>(cImageIn.m_pBitmap), newBuffer, cImageIn.m_Xres, cImageIn.m_Yres, outWidth*4 );
	delete[] inputBuffer;
	return newBuffer;
}
#endif

//===========================================================================
// 
// [BB] Upsamples the texture in inputBuffer, frees inputBuffer and returns
//  the upsampled buffer.
//
//===========================================================================
unsigned char *gl_CreateUpsampledTextureBuffer ( const FGLTexture *inputGLTexture, unsigned char *inputBuffer, const int inWidth, const int inHeight, int &outWidth, int &outHeight )
{
	// [BB] Make sure that outWidth and outHeight denote the size of
	// the returned buffer even if we don't upsample the input buffer.
	outWidth = inWidth;
	outHeight = inHeight;

	// [BB] Don't resample if the width or height of the input texture is bigger than gl_texture_hqresize_maxinputsize.
	if ( ( inWidth > gl_texture_hqresize_maxinputsize ) || ( inHeight > gl_texture_hqresize_maxinputsize ) )
		return inputBuffer;

	// [BB] The hqnx upsampling (not the scaleN one) destroys partial transparency, don't upsamle textures using it.
	if ( inputGLTexture->bIsTransparent == 1 )
		return inputBuffer;

	// [BB] Don't try to upsample textures based off FCanvasTexture.
	if ( inputGLTexture->tex->bHasCanvas )
		return inputBuffer;

	// [BB] Don't upsample non-shader handled warped textures. Needs too much memory.
	if ( (!(gl.flags & RFL_GLSL) || !gl_warp_shader) && inputGLTexture->tex->bWarped )
		return inputBuffer;

	switch (inputGLTexture->tex->UseType)
	{
	case FTexture::TEX_Sprite:
	case FTexture::TEX_SkinSprite:
		if (!(gl_texture_hqresize_targets & 2)) return inputBuffer;
		break;

	case FTexture::TEX_FontChar:
		if (!(gl_texture_hqresize_targets & 4)) return inputBuffer;
		break;

	default:
		if (!(gl_texture_hqresize_targets & 1)) return inputBuffer;
		break;
	}

	if (inputBuffer)
	{
		int type = gl_texture_hqresize;
		switch (type)
		{
		case 1:
			return scaleNxHelper( &scale2x, 2, inputBuffer, inWidth, inHeight, outWidth, outHeight );
		case 2:
			return scaleNxHelper( &scale3x, 3, inputBuffer, inWidth, inHeight, outWidth, outHeight );
		case 3:
			return scaleNxHelper( &scale4x, 4, inputBuffer, inWidth, inHeight, outWidth, outHeight );
// [BB] hqnx scaling is only supported with the MS compiler.
#ifdef _MSC_VER
		case 4:
			return hqNxHelper( &hq2x_32, 2, inputBuffer, inWidth, inHeight, outWidth, outHeight );
		case 5:
			return hqNxHelper( &hq3x_32, 3, inputBuffer, inWidth, inHeight, outWidth, outHeight );
		case 6:
			return hqNxHelper( &hq4x_32, 4, inputBuffer, inWidth, inHeight, outWidth, outHeight );
#endif
		}
	}
	return inputBuffer;
}
