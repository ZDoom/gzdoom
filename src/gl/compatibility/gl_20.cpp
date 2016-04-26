/*
** gl_20.cpp
**
** Fallback code for ancient hardware
** This file collects everything larger that is only needed for
** OpenGL 2.0/no shader compatibility.
**
**---------------------------------------------------------------------------
** Copyright 2005-2016 Christoph Oelckers
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
** 4. Full disclosure of the entire project's source code, except for third
**    party libraries is mandatory. (NOTE: This clause is non-negotiable!)
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
#include "tarray.h"
#include "doomtype.h"
#include "m_argv.h"
#include "zstring.h"
#include "version.h"
#include "i_system.h"
#include "v_text.h"
#include "r_utility.h"
#include "gl/system/gl_interface.h"
#include "gl/system/gl_cvars.h"
#include "gl/renderer/gl_renderstate.h"


void gl_SetTextureMode(int type)
{
	static float white[] = {1.f,1.f,1.f,1.f};

	if (type == TM_MASK)
	{
		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
		glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_REPLACE);
		glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_PRIMARY_COLOR);
		glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_COLOR);

		glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_MODULATE); 
		glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, GL_PRIMARY_COLOR);
		glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_ALPHA, GL_TEXTURE0);
		glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_ALPHA, GL_SRC_ALPHA);
		glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_ALPHA, GL_SRC_ALPHA);
	}
	else if (type == TM_OPAQUE)
	{
		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
		glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_MODULATE);
		glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_TEXTURE0);
		glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB, GL_PRIMARY_COLOR);
		glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_COLOR);
		glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB, GL_SRC_COLOR);

		glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_REPLACE); 
		glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, GL_PRIMARY_COLOR);
		glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_ALPHA, GL_SRC_ALPHA);
	}
	else if (type == TM_INVERSE)
	{
		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
		glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_MODULATE);
		glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_TEXTURE0);
		glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB, GL_PRIMARY_COLOR);
		glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_ONE_MINUS_SRC_COLOR);
		glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB, GL_SRC_COLOR);

		glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_MODULATE); 
		glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, GL_PRIMARY_COLOR);
		glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_ALPHA, GL_TEXTURE0);
		glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_ALPHA, GL_SRC_ALPHA);
		glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_ALPHA, GL_SRC_ALPHA);
	}
	else if (type == TM_INVERTOPAQUE)
	{
		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
		glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_MODULATE);
		glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_TEXTURE0);
		glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB, GL_PRIMARY_COLOR);
		glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_ONE_MINUS_SRC_COLOR);
		glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB, GL_SRC_COLOR);

		glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_REPLACE); 
		glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, GL_PRIMARY_COLOR);
		glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_ALPHA, GL_SRC_ALPHA);
	}
	else // if (type == TM_MODULATE)
	{
		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	}
}

//===========================================================================
//
// FGLTex::WarpBuffer
//
//===========================================================================

BYTE *gl_WarpBuffer(BYTE *buffer, int Width, int Height, int warp, float Speed)
{
	if (Width > 256 || Height > 256) return buffer;

	DWORD *in = (DWORD*)buffer;
	DWORD *out = (DWORD*)new BYTE[4 * Width*Height];

	static DWORD linebuffer[256];	// anything larger will bring down performance so it is excluded above.
	DWORD timebase = DWORD(r_FrameTime*Speed * 23 / 28);
	int xsize = Width;
	int ysize = Height;
	int xmask = xsize - 1;
	int ymask = ysize - 1;
	int ds_xbits;
	int i, x;

	if (warp == 1)
	{
		for (ds_xbits = -1, i = Width; i; i >>= 1, ds_xbits++);

		for (x = xsize - 1; x >= 0; x--)
		{
			int yt, yf = (finesine[(timebase + (x + 17) * 128)&FINEMASK] >> 13) & ymask;
			const DWORD *source = in + x;
			DWORD *dest = out + x;
			for (yt = ysize; yt; yt--, yf = (yf + 1)&ymask, dest += xsize)
			{
				*dest = *(source + (yf << ds_xbits));
			}
		}
		timebase = DWORD(r_FrameTime*Speed * 32 / 28);
		int y;
		for (y = ysize - 1; y >= 0; y--)
		{
			int xt, xf = (finesine[(timebase + y * 128)&FINEMASK] >> 13) & xmask;
			DWORD *source = out + (y << ds_xbits);
			DWORD *dest = linebuffer;
			for (xt = xsize; xt; xt--, xf = (xf + 1)&xmask)
			{
				*dest++ = *(source + xf);
			}
			memcpy(out + y*xsize, linebuffer, xsize * sizeof(DWORD));
		}
	}
	else
	{
		int ybits;
		for (ybits = -1, i = ysize; i; i >>= 1, ybits++);

		DWORD timebase = (r_FrameTime * Speed * 40 / 28);
		for (x = xsize - 1; x >= 0; x--)
		{
			for (int y = ysize - 1; y >= 0; y--)
			{
				int xt = (x + 128
					+ ((finesine[(y * 128 + timebase * 5 + 900) & 8191] * 2) >> FRACBITS)
					+ ((finesine[(x * 256 + timebase * 4 + 300) & 8191] * 2) >> FRACBITS)) & xmask;
				int yt = (y + 128
					+ ((finesine[(y * 128 + timebase * 3 + 700) & 8191] * 2) >> FRACBITS)
					+ ((finesine[(x * 256 + timebase * 4 + 1200) & 8191] * 2) >> FRACBITS)) & ymask;
				const DWORD *source = in + (xt << ybits) + yt;
				DWORD *dest = out + (x << ybits) + y;
				*dest = *source;
			}
		}
	}
	delete[] buffer;
	return (BYTE*)out;
}


static int ffTextureMode;
static bool ffTextureEnabled;
static bool ffFogEnabled;
static PalEntry ffFogColor;
static int ffSpecialEffect;
static float ffFogDensity;

void FRenderState::ApplyFixedFunction()
{
	if (mTextureMode != ffTextureMode)
	{
		ffTextureMode = mTextureMode;
		if (ffTextureMode == TM_CLAMPY) ffTextureMode = TM_MODULATE;	// this cannot be replicated. Too bad if it creates visual artifacts
		gl_SetTextureMode(ffTextureMode);
	}
	if (mTextureEnabled != ffTextureEnabled)
	{
		if ((ffTextureEnabled = mTextureEnabled)) glEnable(GL_TEXTURE_2D);
		else glDisable(GL_TEXTURE_2D);
	}
	if (mFogEnabled != ffFogEnabled)
	{
		if ((ffFogEnabled = mFogEnabled))
		{
			glEnable(GL_FOG);
		}
		else glDisable(GL_FOG);
	}
	if (mFogEnabled)
	{
		if (ffFogColor != mFogColor)
		{
			ffFogColor = mFogColor;
			GLfloat FogColor[4] = { mFogColor.r / 255.0f,mFogColor.g / 255.0f,mFogColor.b / 255.0f,0.0f };
			glFogfv(GL_FOG_COLOR, FogColor);
		}
		if (ffFogDensity != mLightParms[2])
		{
			glFogf(GL_FOG_DENSITY, mLightParms[2] * -0.6931471f);	// = 1/log(2)
			ffFogDensity = mLightParms[2];
		}
	}
	if (mSpecialEffect != ffSpecialEffect)
	{
		switch (ffSpecialEffect)
		{
		case EFF_SPHEREMAP:
			glDisable(GL_TEXTURE_GEN_T);
			glDisable(GL_TEXTURE_GEN_S);

		default:
			break;
		}
		switch (mSpecialEffect)
		{
		case EFF_SPHEREMAP:
			// Use sphere mapping for this
			glEnable(GL_TEXTURE_GEN_T);
			glEnable(GL_TEXTURE_GEN_S);
			glTexGeni(GL_S, GL_TEXTURE_GEN_MODE, GL_SPHERE_MAP);
			glTexGeni(GL_T, GL_TEXTURE_GEN_MODE, GL_SPHERE_MAP);
			break;

		default:
			break;
		}
		ffSpecialEffect = mSpecialEffect;
	}

	FStateVec4 col = mColor;

	col.vec[0] += mDynColor.vec[0];
	col.vec[1] += mDynColor.vec[1];
	col.vec[2] += mDynColor.vec[2];
	col.vec[0] = clamp(col.vec[0], 0.f, 1.f);

	col.vec[0] = clamp(col.vec[0], 0.f, 1.f);
	col.vec[1] = clamp(col.vec[1], 0.f, 1.f);
	col.vec[2] = clamp(col.vec[2], 0.f, 1.f);
	col.vec[3] = clamp(col.vec[3], 0.f, 1.f);

	col.vec[0] *= (mObjectColor.r / 255.f);
	col.vec[1] *= (mObjectColor.g / 255.f);
	col.vec[2] *= (mObjectColor.b / 255.f);
	col.vec[3] *= (mObjectColor.a / 255.f);
	glColor4fv(col.vec);

	glEnable(GL_BLEND);
	if (mAlphaThreshold > 0)
	{
		glEnable(GL_ALPHA_TEST);
		glAlphaFunc(GL_GREATER, mAlphaThreshold);
	}
	else
	{
		glDisable(GL_ALPHA_TEST);
	}

}

void gl_FillScreen();

void FRenderState::DrawColormapOverlay()
{
	float r, g, b;
	if (mColormapState > CM_DEFAULT && mColormapState < CM_MAXCOLORMAP)
	{
		FSpecialColormap *scm = &SpecialColormaps[gl_fixedcolormap - CM_FIRSTSPECIALCOLORMAP];
		float m[] = { scm->ColorizeEnd[0] - scm->ColorizeStart[0],
			scm->ColorizeEnd[1] - scm->ColorizeStart[1], scm->ColorizeEnd[2] - scm->ColorizeStart[2], 0.f };

		if (m[0] < 0 && m[1] < 0 && m[2] < 0)
		{
			gl_RenderState.SetColor(1, 1, 1, 1);
			gl_RenderState.BlendFunc(GL_ONE_MINUS_DST_COLOR, GL_ZERO);
			gl_FillScreen();

			r = scm->ColorizeStart[0];
			g = scm->ColorizeStart[1];
			b = scm->ColorizeStart[2];
		}
		else
		{
			r = scm->ColorizeEnd[0];
			g = scm->ColorizeEnd[1];
			b = scm->ColorizeEnd[2];
		}
	}
	else if (mColormapState == CM_LITE)
	{
		if (gl_enhanced_nightvision)
		{
			r = 0.375f, g = 1.0f, b = 0.375f;
		}
		else
		{
			return;
		}
	}
	else if (mColormapState >= CM_TORCH)
	{
		int flicker = mColormapState - CM_TORCH;
		r = (0.8f + (7 - flicker) / 70.0f);
		if (r > 1.0f) r = 1.0f;
		b = g = r;
		if (gl_enhanced_nightvision) b = g * 0.75f;
	}
	else return;

	gl_RenderState.SetColor(r, g, b, 1.f);
	gl_RenderState.BlendFunc(GL_DST_COLOR, GL_ZERO);
	gl_FillScreen();
	gl_RenderState.BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}