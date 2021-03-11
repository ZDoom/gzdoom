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
#include "hqnx/hqx.h"
#ifdef HAVE_MMX
#include "hqnx_asm/hqnx_asm.h"
#endif
#include "xbr/xbrz.h"
#include "xbr/xbrz_old.h"
#include "parallel_for.h"
#include "textures.h"
#include "texturemanager.h"
#include "printf.h"
#include "templates.h"

int upscalemask;

EXTERN_CVAR(Int, gl_texture_hqresizemult)
CUSTOM_CVAR(Int, gl_texture_hqresizemode, 0, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOINITCALL)
{
	if (self < 0 || self > 7)
		self = 0;
	if ((gl_texture_hqresizemult > 4) && (self < 4) && (self > 0))
		gl_texture_hqresizemult = 4;
	TexMan.FlushAll();
	UpdateUpscaleMask();
}

CUSTOM_CVAR(Int, gl_texture_hqresizemult, 1, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOINITCALL)
{
	if (self < 1 || self > 6)
		self = 1;
	if ((self > 4) && (gl_texture_hqresizemode < 4) && (gl_texture_hqresizemode > 0))
		self = 4;
	TexMan.FlushAll();
	UpdateUpscaleMask();
}

CUSTOM_CVAR(Int, gl_texture_hqresize_maxinputsize, 512, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOINITCALL)
{
	if (self > 1024) self = 1024;
	TexMan.FlushAll();
}

CUSTOM_CVAR(Int, gl_texture_hqresize_targets, 15, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOINITCALL)
{
	TexMan.FlushAll();
	UpdateUpscaleMask();
}

CVAR (Flag, gl_texture_hqresize_textures, gl_texture_hqresize_targets, 1);
CVAR (Flag, gl_texture_hqresize_sprites, gl_texture_hqresize_targets, 2);
CVAR (Flag, gl_texture_hqresize_fonts, gl_texture_hqresize_targets, 4);
CVAR (Flag, gl_texture_hqresize_skins, gl_texture_hqresize_targets, 8);

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

void UpdateUpscaleMask()
{
	if (!gl_texture_hqresizemode || gl_texture_hqresizemult == 1) upscalemask = 0;
	else upscalemask = gl_texture_hqresize_targets;
}


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

static unsigned char *normalNx(const int N,
							  unsigned char *inputBuffer,
							  const int inWidth,
							  const int inHeight,
							  int &outWidth,
							  int &outHeight )
{
	outWidth = N * inWidth;
	outHeight = N *inHeight;
	unsigned char * newBuffer = new unsigned char[outWidth*outHeight*4];

	uint32_t *const inBuffer = reinterpret_cast<uint32_t *>(inputBuffer);
	uint32_t *const outBuffer = reinterpret_cast<uint32_t *>(newBuffer);

	for (int y = 0; y < inHeight; ++y)
	{
		const int inRowPos = inWidth * y;
		const int outRowPos = outWidth * N * y;

		for (int x = 0; x < inWidth; ++x)
		{
			std::fill_n(&outBuffer[outRowPos + N * x], N, inBuffer[inRowPos + x]);
		}

		for (int c = 1; c < N; ++c)
		{
			std::copy_n(&outBuffer[outRowPos], outWidth, &outBuffer[outRowPos + outWidth * c]);
		}
	}

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


namespace MMPXAlgorithm
{
	inline static uint32_t luma(uint32_t color) {
		const uint32_t alpha = (color & 0xFF000000) >> 24;
		return (((color & 0x00FF0000) >> 16) + ((color & 0x0000FF00) >> 8) + (color & 0x000000FF) + 1) * (256 - alpha);
	}

	inline static bool all_eq2(uint32_t B, uint32_t A0, uint32_t A1) {
		return ((B ^ A0) | (B ^ A1)) == 0;
	}

	inline static bool all_eq3(uint32_t B, uint32_t A0, uint32_t A1, uint32_t A2) {
		return ((B ^ A0) | (B ^ A1) | (B ^ A2)) == 0;
	}

	inline static bool all_eq4(uint32_t B, uint32_t A0, uint32_t A1, uint32_t A2, uint32_t A3) {
		return ((B ^ A0) | (B ^ A1) | (B ^ A2) | (B ^ A3)) == 0;
	}

	inline static bool any_eq3(uint32_t B, uint32_t A0, uint32_t A1, uint32_t A2) {
		return B == A0 || B == A1 || B == A2;
	}

	inline static bool none_eq2(uint32_t B, uint32_t A0, uint32_t A1) {
		return (B != A0) && (B != A1);
	}

	inline static bool none_eq4(uint32_t B, uint32_t A0, uint32_t A1, uint32_t A2, uint32_t A3) {
		return B != A0 && B != A1 && B != A2 && B != A3;
	}
}

static unsigned char* mmpx(unsigned char* inputBuffer, const int srcWidth, const int srcHeight, int& outWidth, int& outHeight)
{
	outWidth = 2 * srcWidth;
	outHeight = 2 * srcHeight;
	unsigned char* newBuffer = new unsigned char[outWidth * outHeight * 4];

	uint32_t* const inBuffer = reinterpret_cast<uint32_t*>(inputBuffer);
	uint32_t* const outBuffer = reinterpret_cast<uint32_t*>(newBuffer);

	const uint32_t srcMaxX = srcWidth - 1;
	const uint32_t srcMaxY = srcHeight - 1;

	const auto src = [inBuffer, srcWidth, srcMaxX, srcMaxY](int x, int y)
	{
		// Clamp to border
		if (uint32_t(x) > uint32_t(srcMaxX) || uint32_t(y) > uint32_t(srcMaxY))
		{
			x = clamp(x, 0, srcMaxX);
			y = clamp(y, 0, srcMaxY);
		}

		return inBuffer[y * srcWidth + x];
	};

	using namespace MMPXAlgorithm;

	for (int srcY = 0; srcY < srcHeight; ++srcY) {
		int srcX = 0;

		// Inputs carried along rows
		uint32_t A = src(srcX - 1, srcY - 1);
		uint32_t B = src(srcX, srcY - 1);
		uint32_t C = src(srcX + 1, srcY - 1);

		uint32_t D = src(srcX - 1, srcY);
		uint32_t E = src(srcX, srcY);
		uint32_t F = src(srcX + 1, srcY);

		uint32_t G = src(srcX - 1, srcY + 1);
		uint32_t H = src(srcX, srcY + 1);
		uint32_t I = src(srcX + 1, srcY + 1);

		uint32_t Q = src(srcX - 2, srcY);
		uint32_t R = src(srcX + 2, srcY);

		for (srcX = 0; srcX < srcWidth; ++srcX) {
			// Outputs
			uint32_t J = E, K = E, L = E, M = E;

			if (((A ^ E) | (B ^ E) | (C ^ E) | (D ^ E) | (F ^ E) | (G ^ E) | (H ^ E) | (I ^ E)) != 0) {
				uint32_t P = src(srcX, srcY - 2), S = src(srcX, srcY + 2);
				uint32_t Bl = luma(B), Dl = luma(D), El = luma(E), Fl = luma(F), Hl = luma(H);

				// 1:1 slope rules
				{
					if ((D == B && D != H && D != F) && (El >= Dl || E == A) && any_eq3(E, A, C, G) && ((El < Dl) || A != D || E != P || E != Q)) J = D;
					if ((B == F && B != D && B != H) && (El >= Bl || E == C) && any_eq3(E, A, C, I) && ((El < Bl) || C != B || E != P || E != R)) K = B;
					if ((H == D && H != F && H != B) && (El >= Hl || E == G) && any_eq3(E, A, G, I) && ((El < Hl) || G != H || E != S || E != Q)) L = H;
					if ((F == H && F != B && F != D) && (El >= Fl || E == I) && any_eq3(E, C, G, I) && ((El < Fl) || I != H || E != R || E != S)) M = F;
				}

				// Intersection rules
				{
					if ((E != F && all_eq4(E, C, I, D, Q) && all_eq2(F, B, H)) && (F != src(srcX + 3, srcY))) K = M = F;
					if ((E != D && all_eq4(E, A, G, F, R) && all_eq2(D, B, H)) && (D != src(srcX - 3, srcY))) J = L = D;
					if ((E != H && all_eq4(E, G, I, B, P) && all_eq2(H, D, F)) && (H != src(srcX, srcY + 3))) L = M = H;
					if ((E != B && all_eq4(E, A, C, H, S) && all_eq2(B, D, F)) && (B != src(srcX, srcY - 3))) J = K = B;
					if (Bl < El && all_eq4(E, G, H, I, S) && none_eq4(E, A, D, C, F)) J = K = B;
					if (Hl < El && all_eq4(E, A, B, C, P) && none_eq4(E, D, G, I, F)) L = M = H;
					if (Fl < El && all_eq4(E, A, D, G, Q) && none_eq4(E, B, C, I, H)) K = M = F;
					if (Dl < El && all_eq4(E, C, F, I, R) && none_eq4(E, B, A, G, H)) J = L = D;
				}

				// 2:1 slope rules
				{
					if (H != B) {
						if (H != A && H != E && H != C) {
							if (all_eq3(H, G, F, R) && none_eq2(H, D, src(srcX + 2, srcY - 1))) L = M;
							if (all_eq3(H, I, D, Q) && none_eq2(H, F, src(srcX - 2, srcY - 1))) M = L;
						}

						if (B != I && B != G && B != E) {
							if (all_eq3(B, A, F, R) && none_eq2(B, D, src(srcX + 2, srcY + 1))) J = K;
							if (all_eq3(B, C, D, Q) && none_eq2(B, F, src(srcX - 2, srcY + 1))) K = J;
						}
					} // H !== B

					if (F != D) {
						if (D != I && D != E && D != C) {
							if (all_eq3(D, A, H, S) && none_eq2(D, B, src(srcX + 1, srcY + 2))) J = L;
							if (all_eq3(D, G, B, P) && none_eq2(D, H, src(srcX + 1, srcY - 2))) L = J;
						}

						if (F != E && F != A && F != G) {
							if (all_eq3(F, C, H, S) && none_eq2(F, B, src(srcX - 1, srcY + 2))) K = M;
							if (all_eq3(F, I, B, P) && none_eq2(F, H, src(srcX - 1, srcY - 2))) M = K;
						}
					} // F !== D
				} // 2:1 slope
			}

			int dstIndex = ((srcX + srcX) + (srcY << 2) * srcWidth) >> 0;
			uint32_t* dstPacked = outBuffer + dstIndex;

			*dstPacked = J; dstPacked++;
			*dstPacked = K; dstPacked += srcWidth + srcMaxX;
			*dstPacked = L; dstPacked++;
			*dstPacked = M;

			A = B; B = C; C = src(srcX + 2, srcY - 1);
			Q = D; D = E; E = F; F = R; R = src(srcX + 3, srcY);
			G = H; H = I; I = src(srcX + 2, srcY + 1);
		} // X
	} // Y

	delete[] inputBuffer;
	return newBuffer;
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
	if (mult < 2 || mult > 6 || type < 1 || type > 7) return;
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
			texbuffer.mBuffer = normalNx(mult, texbuffer.mBuffer, inWidth, inHeight, texbuffer.mWidth, texbuffer.mHeight);
		else if (type == 7)
			texbuffer.mBuffer = mmpx(texbuffer.mBuffer, inWidth, inHeight, texbuffer.mWidth, texbuffer.mHeight);
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

//===========================================================================
// 
// This was pulled out of the above function to allow running these
// checks before the texture is passed to the render state.
//
//===========================================================================

void calcShouldUpscale(FGameTexture *tex)
{
	tex->SetUpscaleFlag(0);
	// [BB] Don't resample if width * height of the input texture is bigger than gl_texture_hqresize_maxinputsize squared.
	const int maxInputSize = gl_texture_hqresize_maxinputsize;
	if (tex->GetTexelWidth() * tex->GetTexelHeight() > maxInputSize * maxInputSize)
		return;

	// [BB] Don't try to upsample textures based off FCanvasTexture. (This should never get here in the first place!)
	if (tex->isHardwareCanvas())
		return;

	// already scaled?
	if (tex->GetScaleX() >= 2.f || tex->GetScaleY() > 2.f)
		return;

	tex->SetUpscaleFlag(1);
}