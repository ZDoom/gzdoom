/*
** r_data.cpp
**
**---------------------------------------------------------------------------
** Copyright 2008-2011 Randy Heit
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

#include "farchive.h"
#include "templates.h"
#include "renderstyle.h"
#include "c_cvars.h"

CVAR (Bool, r_drawtrans, true, 0)
CVAR (Int, r_drawfuzz, 1, CVAR_ARCHIVE)

// Convert legacy render styles to flexible render styles.

// Apple's GCC 4.0.1 apparently wants to initialize the AsDWORD member of FRenderStyle
// rather than the struct before it, which goes against the standard.
#ifndef __APPLE__
FRenderStyle LegacyRenderStyles[STYLE_Count] =
{
			/* STYLE_None */  {{ STYLEOP_None, 		STYLEALPHA_Zero,	STYLEALPHA_Zero,	0 }},
		  /* STYLE_Normal */  {{ STYLEOP_Add,		STYLEALPHA_Src,		STYLEALPHA_InvSrc,	STYLEF_Alpha1 }},
		   /* STYLE_Fuzzy */  {{ STYLEOP_Fuzz,		STYLEALPHA_Src,		STYLEALPHA_InvSrc,	0 }},
	   /* STYLE_SoulTrans */  {{ STYLEOP_Add,		STYLEALPHA_Src,		STYLEALPHA_InvSrc,	STYLEF_TransSoulsAlpha }},
		/* STYLE_OptFuzzy */  {{ STYLEOP_FuzzOrAdd,	STYLEALPHA_Src,		STYLEALPHA_InvSrc,	0 }},
		 /* STYLE_Stencil */  {{ STYLEOP_Add,		STYLEALPHA_Src,		STYLEALPHA_InvSrc,	STYLEF_Alpha1 | STYLEF_ColorIsFixed }},
	 /* STYLE_Translucent */  {{ STYLEOP_Add,		STYLEALPHA_Src,		STYLEALPHA_InvSrc,	0 }},
			 /* STYLE_Add */  {{ STYLEOP_Add,		STYLEALPHA_Src,		STYLEALPHA_One,		0 }},
		  /* STYLE_Shaded */  {{ STYLEOP_Add,		STYLEALPHA_Src,		STYLEALPHA_InvSrc,	STYLEF_RedIsAlpha | STYLEF_ColorIsFixed }},
/* STYLE_TranslucentStencil */{{ STYLEOP_Add,		STYLEALPHA_Src,		STYLEALPHA_InvSrc,	STYLEF_ColorIsFixed }},
          /* STYLE_Shadow */  {{ STYLEOP_Shadow,	0,					0,					0 }},
		  /* STYLE_Subtract*/ {{ STYLEOP_RevSub,	STYLEALPHA_Src,		STYLEALPHA_One,		0 }},
};
#else
FRenderStyle LegacyRenderStyles[STYLE_Count];

static const BYTE Styles[STYLE_Count * 4] =
{
	STYLEOP_None, 		STYLEALPHA_Zero,	STYLEALPHA_Zero,	0,
	STYLEOP_Add,		STYLEALPHA_Src,		STYLEALPHA_InvSrc,	STYLEF_Alpha1,
	STYLEOP_Fuzz,		STYLEALPHA_Src,		STYLEALPHA_InvSrc,	0,
	STYLEOP_Add,		STYLEALPHA_Src,		STYLEALPHA_InvSrc,	STYLEF_TransSoulsAlpha,
	STYLEOP_FuzzOrAdd,	STYLEALPHA_Src,		STYLEALPHA_InvSrc,	0,
	STYLEOP_Add,		STYLEALPHA_Src,		STYLEALPHA_InvSrc,	STYLEF_Alpha1 | STYLEF_ColorIsFixed,
	STYLEOP_Add,		STYLEALPHA_Src,		STYLEALPHA_InvSrc,	0,
	STYLEOP_Add,		STYLEALPHA_Src,		STYLEALPHA_One,		0,
	STYLEOP_Add,		STYLEALPHA_Src,		STYLEALPHA_InvSrc,	STYLEF_RedIsAlpha | STYLEF_ColorIsFixed,
	STYLEOP_Add,		STYLEALPHA_Src,		STYLEALPHA_InvSrc,	STYLEF_ColorIsFixed,
	STYLEOP_Shadow,		0,					0,					0,
	STYLEOP_RevSub,		STYLEALPHA_Src,		STYLEALPHA_One,		0,
};

static struct LegacyInit
{
	LegacyInit()
	{
		for (int i = 0; i < STYLE_Count; ++i)
		{
			LegacyRenderStyles[i].BlendOp = Styles[i*4];
			LegacyRenderStyles[i].SrcAlpha = Styles[i*4+1];
			LegacyRenderStyles[i].DestAlpha = Styles[i*4+2];
			LegacyRenderStyles[i].Flags = Styles[i*4+3];
		}
	}
} DoLegacyInit;

#endif

FArchive &operator<< (FArchive &arc, FRenderStyle &style)
{
	arc << style.BlendOp << style.SrcAlpha << style.DestAlpha << style.Flags;
	return arc;
}

//==========================================================================
//
// FRenderStyle :: IsVisible
//
// Coupled with the given alpha, will this render style produce something
// visible on-screen?
//
//==========================================================================

bool FRenderStyle::IsVisible(fixed_t alpha) const throw()
{
	if (BlendOp == STYLEOP_None)
	{
		return false;
	}
	if (BlendOp == STYLEOP_Add || BlendOp == STYLEOP_RevSub)
	{
		if (Flags & STYLEF_Alpha1)
		{
			alpha = FRACUNIT;
		}
		else
		{
			alpha = clamp(alpha, 0, FRACUNIT);
		}
		return GetAlpha(SrcAlpha, alpha) != 0 || GetAlpha(DestAlpha, alpha) != FRACUNIT;
	}
	// Treat anything else as visible.
	return true;
}


//==========================================================================
//
// FRenderStyle :: CheckFuzz
//
// Adjusts settings based on r_drawfuzz CVAR
//
//==========================================================================

void FRenderStyle::CheckFuzz()
{
	switch (BlendOp)
	{
	default:
		return;

	case STYLEOP_FuzzOrAdd:
		if (r_drawtrans && r_drawfuzz == 0)
		{
			BlendOp = STYLEOP_Add;
			return;
		}
		break;

	case STYLEOP_FuzzOrSub:
		if (r_drawtrans && r_drawfuzz == 0)
		{
			BlendOp = STYLEOP_Sub;
			return;
		}
		break;

	case STYLEOP_FuzzOrRevSub:
		if (r_drawtrans && r_drawfuzz == 0)
		{
			BlendOp = STYLEOP_RevSub;
			return;
		}
		break;
	}

	if (r_drawfuzz == 2)
	{
		BlendOp = STYLEOP_Shadow;
	}
	else
	{
		BlendOp = STYLEOP_Fuzz;
	}
}

fixed_t GetAlpha(int type, fixed_t alpha)
{
	switch (type)
	{
	case STYLEALPHA_Zero:		return 0;
	case STYLEALPHA_One:		return FRACUNIT;
	case STYLEALPHA_Src:		return alpha;
	case STYLEALPHA_InvSrc:		return FRACUNIT - alpha;
	default:					return 0;
	}
}

