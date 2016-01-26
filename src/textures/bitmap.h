/*
** bitmap.h
**
**---------------------------------------------------------------------------
** Copyright 2008 Christoph Oelckers
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


#ifndef __BITMAP_H__
#define __BITMAP_H__

#include "doomtype.h"
#include "templates.h"

struct FCopyInfo;

struct FClipRect
{
	int x, y, width, height;

	bool Intersect(int ix, int iy, int iw, int ih);
};

class FBitmap
{
protected:
	BYTE *data;
	int Width;
	int Height;
	int Pitch;
	bool FreeBuffer;
	FClipRect ClipRect;


public:
	FBitmap()
	{
		data = NULL;
		Width = Height = 0;
		Pitch = 0;
		FreeBuffer = false;
		ClipRect.x = ClipRect.y = ClipRect.width = ClipRect.height = 0;
	}

	FBitmap(BYTE *buffer, int pitch, int width, int height)
	{
		data = buffer;

		Pitch = pitch;
		Width = width;
		Height = height;
		FreeBuffer = false;
		ClipRect.x = ClipRect.y = 0;
		ClipRect.width = width;
		ClipRect.height = height;
	}

	virtual ~FBitmap()
	{
		Destroy();
	}

	void Destroy()
	{
		if (data != NULL && FreeBuffer) delete [] data;
		data = NULL;
		FreeBuffer = false;
	}

	bool Create (int w, int h)
	{
		Pitch = w*4;
		Width = w;
		Height = h;
		data = new BYTE[4*w*h];
		memset(data, 0, 4*w*h);
		FreeBuffer = true;
		ClipRect.x = ClipRect.y = 0;
		ClipRect.width = w;
		ClipRect.height = h;
		return data != NULL;
	}

	int GetHeight() const
	{
		return Height;
	}

	int GetWidth() const
	{
		return Width;
	}

	int GetPitch() const
	{
		return Pitch;
	}

	const BYTE *GetPixels() const
	{
		return data;
	}

	BYTE *GetPixels()
	{
		return data;
	}

	void SetClipRect(FClipRect &clip)
	{
		ClipRect = clip;
	}

	void IntersectClipRect(FClipRect &clip)
	{
		ClipRect.Intersect(clip.x, clip.y, clip.width, clip.height);
	}

	void IntersectClipRect(int cx, int cy, int cw, int ch)
	{
		ClipRect.Intersect(cx, cy, cw, ch);
	}

	const FClipRect &GetClipRect() const
	{
		return ClipRect;
	}

	void Zero();


	virtual void CopyPixelDataRGB(int originx, int originy, const BYTE *patch, int srcwidth, 
								int srcheight, int step_x, int step_y, int rotate, int ct, FCopyInfo *inf = NULL,
		/* for PNG tRNS */		int r=0, int g=0, int b=0);
	virtual void CopyPixelData(int originx, int originy, const BYTE * patch, int srcwidth, int srcheight, 
								int step_x, int step_y, int rotate, PalEntry * palette, FCopyInfo *inf = NULL);


};

bool ClipCopyPixelRect(const FClipRect *cr, int &originx, int &originy,
						const BYTE *&patch, int &srcwidth, int &srcheight, 
						int &step_x, int &step_y, int rotate);

//===========================================================================
// 
// True color conversion classes for the different pixel formats
// used by the supported texture formats
//
//===========================================================================
struct cRGB
{
	static __forceinline unsigned char R(const unsigned char * p) { return p[0]; }
	static __forceinline unsigned char G(const unsigned char * p) { return p[1]; }
	static __forceinline unsigned char B(const unsigned char * p) { return p[2]; }
	static __forceinline unsigned char A(const unsigned char * p, BYTE x, BYTE y, BYTE z) { return 255; }
	static __forceinline int Gray(const unsigned char * p) { return (p[0]*77 + p[1]*143 + p[2]*36)>>8; }
};

struct cRGBT
{
	static __forceinline unsigned char R(const unsigned char * p) { return p[0]; }
	static __forceinline unsigned char G(const unsigned char * p) { return p[1]; }
	static __forceinline unsigned char B(const unsigned char * p) { return p[2]; }
	static __forceinline unsigned char A(const unsigned char * p, BYTE r, BYTE g, BYTE b) { return (p[0] != r || p[1] != g || p[2] != b) ? 255 : 0; }
	static __forceinline int Gray(const unsigned char * p) { return (p[0]*77 + p[1]*143 + p[2]*36)>>8; }
};

struct cRGBA
{
	enum
	{
		RED = 0,
		GREEN = 1,
		BLUE = 1,
		ALPHA = 3
	};
	static __forceinline unsigned char R(const unsigned char * p) { return p[0]; }
	static __forceinline unsigned char G(const unsigned char * p) { return p[1]; }
	static __forceinline unsigned char B(const unsigned char * p) { return p[2]; }
	static __forceinline unsigned char A(const unsigned char * p, BYTE x, BYTE y, BYTE z) { return p[3]; }
	static __forceinline int Gray(const unsigned char * p) { return (p[0]*77 + p[1]*143 + p[2]*36)>>8; }
};

struct cIA
{
	static __forceinline unsigned char R(const unsigned char * p) { return p[0]; }
	static __forceinline unsigned char G(const unsigned char * p) { return p[0]; }
	static __forceinline unsigned char B(const unsigned char * p) { return p[0]; }
	static __forceinline unsigned char A(const unsigned char * p, BYTE x, BYTE y, BYTE z) { return p[1]; }
	static __forceinline int Gray(const unsigned char * p) { return p[0]; }
};

struct cCMYK
{
	static __forceinline unsigned char R(const unsigned char * p) { return p[3] - (((256-p[0])*p[3]) >> 8); }
	static __forceinline unsigned char G(const unsigned char * p) { return p[3] - (((256-p[1])*p[3]) >> 8); }
	static __forceinline unsigned char B(const unsigned char * p) { return p[3] - (((256-p[2])*p[3]) >> 8); }
	static __forceinline unsigned char A(const unsigned char * p, BYTE x, BYTE y, BYTE z) { return 255; }
	static __forceinline int Gray(const unsigned char * p) { return (R(p)*77 + G(p)*143 + B(p)*36)>>8; }
};

struct cBGR
{
	static __forceinline unsigned char R(const unsigned char * p) { return p[2]; }
	static __forceinline unsigned char G(const unsigned char * p) { return p[1]; }
	static __forceinline unsigned char B(const unsigned char * p) { return p[0]; }
	static __forceinline unsigned char A(const unsigned char * p, BYTE x, BYTE y, BYTE z) { return 255; }
	static __forceinline int Gray(const unsigned char * p) { return (p[2]*77 + p[1]*143 + p[0]*36)>>8; }
};

struct cBGRA
{
	enum
	{
		RED = 2,
		GREEN = 1,
		BLUE = 0,
		ALPHA = 3
	};
	static __forceinline unsigned char R(const unsigned char * p) { return p[2]; }
	static __forceinline unsigned char G(const unsigned char * p) { return p[1]; }
	static __forceinline unsigned char B(const unsigned char * p) { return p[0]; }
	static __forceinline unsigned char A(const unsigned char * p, BYTE x, BYTE y, BYTE z) { return p[3]; }
	static __forceinline int Gray(const unsigned char * p) { return (p[2]*77 + p[1]*143 + p[0]*36)>>8; }
};

struct cARGB
{
	enum
	{
		RED = 1,
		GREEN = 2,
		BLUE = 3,
		ALPHA = 0
	};
	static __forceinline unsigned char R(const unsigned char * p) { return p[1]; }
	static __forceinline unsigned char G(const unsigned char * p) { return p[2]; }
	static __forceinline unsigned char B(const unsigned char * p) { return p[3]; }
	static __forceinline unsigned char A(const unsigned char * p, BYTE x, BYTE y, BYTE z) { return p[0]; }
	static __forceinline int Gray(const unsigned char * p) { return (p[1]*77 + p[2]*143 + p[3]*36)>>8; }
};

struct cI16
{
	static __forceinline unsigned char R(const unsigned char * p) { return p[1]; }
	static __forceinline unsigned char G(const unsigned char * p) { return p[1]; }
	static __forceinline unsigned char B(const unsigned char * p) { return p[1]; }
	static __forceinline unsigned char A(const unsigned char * p, BYTE x, BYTE y, BYTE z) { return 255; }
	static __forceinline int Gray(const unsigned char * p) { return p[1]; }
};

struct cRGB555
{
	static __forceinline unsigned char R(const unsigned char * p) { return (((*(WORD*)p)&0x1f)<<3); }
	static __forceinline unsigned char G(const unsigned char * p) { return (((*(WORD*)p)&0x3e0)>>2); }
	static __forceinline unsigned char B(const unsigned char * p) { return (((*(WORD*)p)&0x7c00)>>7); }
	static __forceinline unsigned char A(const unsigned char * p, BYTE x, BYTE y, BYTE z) { return 255; }
	static __forceinline int Gray(const unsigned char * p) { return (R(p)*77 + G(p)*143 + B(p)*36)>>8; }
};

struct cPalEntry
{
	static __forceinline unsigned char R(const unsigned char * p) { return ((PalEntry*)p)->r; }
	static __forceinline unsigned char G(const unsigned char * p) { return ((PalEntry*)p)->g; }
	static __forceinline unsigned char B(const unsigned char * p) { return ((PalEntry*)p)->b; }
	static __forceinline unsigned char A(const unsigned char * p, BYTE x, BYTE y, BYTE z) { return ((PalEntry*)p)->a; }
	static __forceinline int Gray(const unsigned char * p) { return (R(p)*77 + G(p)*143 + B(p)*36)>>8; }
};

enum ColorType
{
	CF_RGB,
	CF_RGBT,
	CF_RGBA,
	CF_IA,
	CF_CMYK,
	CF_BGR,
	CF_BGRA,
	CF_I16,
	CF_RGB555,
	CF_PalEntry
};

enum EBlend
{
	BLEND_NONE = 0,
	BLEND_ICEMAP = 1,
	BLEND_DESATURATE1 = 2,
	BLEND_DESATURATE31 = 32,
	BLEND_SPECIALCOLORMAP1 = 33,
	BLEND_MODULATE = -1,	
	BLEND_OVERLAY = -2,
};

enum ECopyOp
{
	OP_COPY,
	OP_BLEND,
	OP_ADD,
	OP_SUBTRACT,
	OP_REVERSESUBTRACT,
	OP_MODULATE,
	OP_COPYALPHA,
	OP_COPYNEWALPHA,
	OP_OVERLAY,
	OP_OVERWRITE
};

struct FCopyInfo
{
	ECopyOp op;
	EBlend blend;
	fixed_t blendcolor[4];
	fixed_t alpha;
	fixed_t invalpha;
};

struct bOverwrite
{
	static __forceinline void OpC(BYTE &d, BYTE s, BYTE a, FCopyInfo *i) { d = s; }
	static __forceinline void OpA(BYTE &d, BYTE s, FCopyInfo *i) { d = s; }
	static __forceinline bool ProcessAlpha0() { return true; }
};

struct bCopy
{
	static __forceinline void OpC(BYTE &d, BYTE s, BYTE a, FCopyInfo *i) { d = s; }
	static __forceinline void OpA(BYTE &d, BYTE s, FCopyInfo *i) { d = s; }
	static __forceinline bool ProcessAlpha0() { return false; }
};

struct bCopyNewAlpha
{
	static __forceinline void OpC(BYTE &d, BYTE s, BYTE a, FCopyInfo *i) { d = s; }
	static __forceinline void OpA(BYTE &d, BYTE s, FCopyInfo *i) { d = (s*i->alpha) >> FRACBITS; }
	static __forceinline bool ProcessAlpha0() { return false; }
};

struct bCopyAlpha
{
	static __forceinline void OpC(BYTE &d, BYTE s, BYTE a, FCopyInfo *i) { d = (s*a + d*(255-a))/255; }
	static __forceinline void OpA(BYTE &d, BYTE s, FCopyInfo *i) { d = s; }
	static __forceinline bool ProcessAlpha0() { return false; }
};

struct bOverlay
{
	static __forceinline void OpC(BYTE &d, BYTE s, BYTE a, FCopyInfo *i) { d = (s*a + d*(255-a))/255; }
	static __forceinline void OpA(BYTE &d, BYTE s, FCopyInfo *i) { d = MAX(s,d); }
	static __forceinline bool ProcessAlpha0() { return false; }
};

struct bBlend
{
	static __forceinline void OpC(BYTE &d, BYTE s, BYTE a, FCopyInfo *i) { d = (d*i->invalpha + s*i->alpha) >> FRACBITS; }
	static __forceinline void OpA(BYTE &d, BYTE s, FCopyInfo *i) { d = s; }
	static __forceinline bool ProcessAlpha0() { return false; }
};

struct bAdd
{
	static __forceinline void OpC(BYTE &d, BYTE s, BYTE a, FCopyInfo *i) { d = MIN<int>((d*FRACUNIT + s*i->alpha) >> FRACBITS, 255); }
	static __forceinline void OpA(BYTE &d, BYTE s, FCopyInfo *i) { d = s; }
	static __forceinline bool ProcessAlpha0() { return false; }
};

struct bSubtract
{
	static __forceinline void OpC(BYTE &d, BYTE s, BYTE a, FCopyInfo *i) { d = MAX<int>((d*FRACUNIT - s*i->alpha) >> FRACBITS, 0); }
	static __forceinline void OpA(BYTE &d, BYTE s, FCopyInfo *i) { d = s; }
	static __forceinline bool ProcessAlpha0() { return false; }
};

struct bReverseSubtract
{
	static __forceinline void OpC(BYTE &d, BYTE s, BYTE a, FCopyInfo *i) { d = MAX<int>((-d*FRACUNIT + s*i->alpha) >> FRACBITS, 0); }
	static __forceinline void OpA(BYTE &d, BYTE s, FCopyInfo *i) { d = s; }
	static __forceinline bool ProcessAlpha0() { return false; }
};

struct bModulate
{
	static __forceinline void OpC(BYTE &d, BYTE s, BYTE a, FCopyInfo *i) { d = (s*d)/255; }
	static __forceinline void OpA(BYTE &d, BYTE s, FCopyInfo *i) { d = s; }
	static __forceinline bool ProcessAlpha0() { return false; }
};


#endif
