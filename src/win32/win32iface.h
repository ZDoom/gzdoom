/*
** win32iface.h
**
**---------------------------------------------------------------------------
** Copyright 1998-2008 Randy Heit
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

#ifndef __WIN32IFACE_H
#define __WIN32IFACE_H

#ifndef DIRECTDRAW_VERSION
#define DIRECTDRAW_VERSION 0x0300
#endif
#ifndef DIRECT3D_VERSION
#define DIRECT3D_VERSION 0x0900
#endif

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <ddraw.h>
#include <d3d9.h>

#include "hardware.h"
#include "v_video.h"

#define SAFE_RELEASE(x)		{ if (x != NULL) { x->Release(); x = NULL; } }

EXTERN_CVAR (Bool, vid_vsync)

class D3DTex;
class D3DPal;

class Win32Video : public IVideo
{
 public:
	Win32Video (int parm);
	~Win32Video ();

	bool InitD3D9();
	void InitDDraw();

	EDisplayType GetDisplayType () { return DISPLAY_Both; }
	void SetWindowedScale (float scale);

	DFrameBuffer *CreateFrameBuffer (int width, int height, bool fs, DFrameBuffer *old);

	void StartModeIterator (int bits, bool fs);
	bool NextMode (int *width, int *height, bool *letterbox);

	bool GoFullscreen (bool yes);
	void BlankForGDI ();

 private:
	struct ModeInfo
	{
		ModeInfo (int inX, int inY, int inBits, int inRealY, int inDoubling)
			: next (NULL),
			  width (inX),
			  height (inY),
			  bits (inBits),
			  realheight (inRealY),
			  doubling (inDoubling)
		{}

		ModeInfo *next;
		int width, height, bits;
		int realheight;
		int doubling;
	} *m_Modes;

	ModeInfo *m_IteratorMode;
	int m_IteratorBits;
	bool m_IteratorFS;
	bool m_IsFullscreen;

	bool m_CalledCoInitialize;

	void AddMode (int x, int y, int bits, int baseHeight, int doubling);
	void FreeModes ();

	static HRESULT WINAPI EnumDDModesCB (LPDDSURFACEDESC desc, void *modes);
	void AddD3DModes (D3DFORMAT format);
	void AddLowResModes ();
	void AddLetterboxModes ();

	friend class DDrawFB;
	friend class D3DFB;
};

class BaseWinFB : public DFrameBuffer
{
	DECLARE_ABSTRACT_CLASS(BaseWinFB, DFrameBuffer)
public:
	BaseWinFB (int width, int height) : DFrameBuffer (width, height), Windowed (true) {}

	bool IsFullscreen () { return !Windowed; }
	virtual void Blank () = 0;
	virtual bool PaintToWindow () = 0;
	virtual HRESULT GetHR () = 0;

protected:
	virtual bool CreateResources () = 0;
	virtual void ReleaseResources () = 0;

	bool Windowed;

	friend int I_PlayMovie (const char *name);
	friend class Win32Video;

	BaseWinFB() {}
};

class DDrawFB : public BaseWinFB
{
	DECLARE_CLASS(DDrawFB, BaseWinFB)
public:
	DDrawFB (int width, int height, bool fullscreen);
	~DDrawFB ();

	bool IsValid ();
	bool Lock ();
	bool Lock (bool buffer);
	void Unlock ();
	void ForceBuffering (bool force);
	void Update ();
	PalEntry *GetPalette ();
	void GetFlashedPalette (PalEntry pal[256]);
	void UpdatePalette ();
	bool SetGamma (float gamma);
	bool SetFlash (PalEntry rgb, int amount);
	void GetFlash (PalEntry &rgb, int &amount);
	int GetPageCount ();
	int QueryNewPalette ();
	void PaletteChanged ();
	void SetVSync (bool vsync);
	void NewRefreshRate();
	HRESULT GetHR ();

	void Blank ();
	bool PaintToWindow ();

private:
	enum LockSurfRes { NoGood, Good, GoodWasLost };
	
	bool CreateResources ();
	void ReleaseResources ();
	bool CreateSurfacesAttached ();
	bool CreateSurfacesComplex ();
	bool CreateBlitterSource ();
	LockSurfRes LockSurf (LPRECT lockrect, LPDIRECTDRAWSURFACE surf);
	void RebuildColorTable ();
	void MaybeCreatePalette ();
	bool AddBackBuf (LPDIRECTDRAWSURFACE *surface, int num);
	HRESULT AttemptRestore ();

	HRESULT LastHR;
	BYTE GammaTable[3][256];
	PalEntry SourcePalette[256];
	PALETTEENTRY PalEntries[256];
	DWORD FlipFlags;

	LPDIRECTDRAWPALETTE Palette;
	LPDIRECTDRAWSURFACE PrimarySurf;
	LPDIRECTDRAWSURFACE BackSurf;
	LPDIRECTDRAWSURFACE BackSurf2;
	LPDIRECTDRAWSURFACE BlitSurf;
	LPDIRECTDRAWSURFACE LockingSurf;
	LPDIRECTDRAWCLIPPER Clipper;
	HPALETTE GDIPalette;
	BYTE *ClipRegion;
	DWORD ClipSize;
	PalEntry Flash;
	int FlashAmount;
	int BufferCount;
	int BufferPitch;
	int TrueHeight;
	float Gamma;

	bool NeedGammaUpdate;
	bool NeedPalUpdate;
	bool NeedResRecreate;
	bool MustBuffer;		// The screen is not 8-bit, or there is no backbuffer
	bool BufferingNow;		// Most recent Lock was buffered
	bool WasBuffering;		// Second most recent Lock was buffered
	bool Write8bit;
	bool UpdatePending;		// On final unlock, call Update()
	bool UseBlitter;		// Use blitter to copy from sys mem to video mem
	bool UsePfx;

	DDrawFB() {}
};

class D3DFB : public BaseWinFB
{
	DECLARE_CLASS(D3DFB, BaseWinFB)
public:
	D3DFB (int width, int height, bool fullscreen);
	~D3DFB ();

	bool IsValid ();
	bool Lock ();
	bool Lock (bool buffered);
	void Unlock ();
	void Update ();
	PalEntry *GetPalette ();
	void GetFlashedPalette (PalEntry palette[256]);
	void UpdatePalette ();
	bool SetGamma (float gamma);
	bool SetFlash (PalEntry rgb, int amount);
	void GetFlash (PalEntry &rgb, int &amount);
	int GetPageCount ();
	bool IsFullscreen ();
	void PaletteChanged ();
	int QueryNewPalette ();
	void Blank ();
	bool PaintToWindow ();
	void SetVSync (bool vsync);
	void NewRefreshRate();
	void GetScreenshotBuffer(const BYTE *&buffer, int &pitch, ESSType &color_type);
	void ReleaseScreenshotBuffer();
	void SetBlendingRect (int x1, int y1, int x2, int y2);
	bool Begin2D (bool copy3d);
	FNativeTexture *CreateTexture (FTexture *gametex, bool wrapping);
	FNativePalette *CreatePalette (FRemapTable *remap);
	void STACK_ARGS DrawTextureV (FTexture *img, int x, int y, uint32 tag, va_list tags);
	void Clear (int left, int top, int right, int bottom, int palcolor, uint32 color);
	void Dim (PalEntry color, float amount, int x1, int y1, int w, int h);
	void FlatFill (int left, int top, int right, int bottom, FTexture *src, bool local_origin);
	void DrawLine(int x0, int y0, int x1, int y1, int palColor, uint32 realcolor);
	void DrawPixel(int x, int y, int palcolor, uint32 rgbcolor);
	bool WipeStartScreen(int type);
	void WipeEndScreen();
	bool WipeDo(int ticks);
	void WipeCleanup();
	HRESULT GetHR ();

private:
	friend class D3DTex;
	friend class D3DPal;

	struct PackedTexture;
	struct PackingTexture;

	struct FBVERTEX
	{
		FLOAT x, y, z, rhw;
		D3DCOLOR color0, color1;
		FLOAT tu, tv;
	};
#define D3DFVF_FBVERTEX (D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_SPECULAR | D3DFVF_TEX1)

	struct BufferedQuad
	{
		union
		{
			struct
			{
				BYTE Flags;
				BYTE ShaderNum:4;
				BYTE BlendOp:4;
				BYTE SrcBlend, DestBlend;
			};
			DWORD Group1;
		};
		D3DPal *Palette;
		PackingTexture *Texture;
	};

	void SetInitialState();
	bool CreateResources();
	void ReleaseResources();
	bool CreateFBTexture();
	bool CreatePaletteTexture();
	bool CreateGrayPaletteTexture();
	bool CreateVertexes();
	void UploadPalette();
	void FillPresentParameters (D3DPRESENT_PARAMETERS *pp, bool fullscreen, bool vsync);
	void CalcFullscreenCoords (FBVERTEX verts[4], bool viewarea_only, bool can_double, D3DCOLOR color0, D3DCOLOR color1) const;
	bool Reset();
	IDirect3DTexture9 *GetCurrentScreen();
	void ReleaseDefaultPoolItems();
	void KillNativePals();
	void KillNativeTexs();
	PackedTexture *AllocPackedTexture(int width, int height, bool wrapping, D3DFORMAT format);
	void DrawPackedTextures(int packnum);
	void DrawLetterbox();
	void Draw3DPart(bool copy3d);
	bool SetStyle(D3DTex *tex, DCanvas::DrawParms &parms, D3DCOLOR &color0, D3DCOLOR &color1, BufferedQuad &quad);
	static D3DBLEND GetStyleAlpha(int type);
	static void SetColorOverlay(DWORD color, float alpha, D3DCOLOR &color0, D3DCOLOR &color1);
	void DoWindowedGamma();
	void AddColorOnlyQuad(int left, int top, int width, int height, D3DCOLOR color);
	void CheckQuadBatch();
	void BeginQuadBatch();
	void EndQuadBatch();
	void BeginLineBatch();
	void EndLineBatch();
	void EndBatch();

	// State
	void EnableAlphaTest(BOOL enabled);
	void SetAlphaBlend(D3DBLENDOP op, D3DBLEND srcblend=D3DBLEND(0), D3DBLEND destblend=D3DBLEND(0));
	void SetConstant(int cnum, float r, float g, float b, float a);
	void SetPixelShader(IDirect3DPixelShader9 *shader);
	void SetTexture(int tnum, IDirect3DTexture9 *texture);
	void SetPaletteTexture(IDirect3DTexture9 *texture, int count, D3DCOLOR border_color);
	void SetPalTexBilinearConstants(PackingTexture *texture);

	BOOL AlphaTestEnabled;
	BOOL AlphaBlendEnabled;
	D3DBLENDOP AlphaBlendOp;
	D3DBLEND AlphaSrcBlend;
	D3DBLEND AlphaDestBlend;
	float Constant[3][4];
	D3DCOLOR CurBorderColor;
	IDirect3DPixelShader9 *CurPixelShader;
	IDirect3DTexture9 *Texture[2];

	PalEntry SourcePalette[256];
	D3DCOLOR BorderColor;
	D3DCOLOR FlashColor0, FlashColor1;
	PalEntry FlashColor;
	int FlashAmount;
	int TrueHeight;
	int PixelDoubling;
	int LBOffsetI;
	float LBOffset;
	float Gamma;
	bool UpdatePending;
	bool NeedPalUpdate;
	bool NeedGammaUpdate;
	D3DFORMAT FBFormat;
	D3DFORMAT PalFormat;
	int FBWidth, FBHeight;
	bool VSync;
	RECT BlendingRect;
	int In2D;
	bool InScene;
	bool SM14;
	bool GatheringWipeScreen;
	D3DPal *Palettes;
	D3DTex *Textures;
	PackingTexture *Packs;

	IDirect3DDevice9 *D3DDevice;
	IDirect3DTexture9 *FBTexture;
	IDirect3DTexture9 *TempRenderTexture;
	IDirect3DTexture9 *PaletteTexture;
	IDirect3DTexture9 *ScreenshotTexture;
	IDirect3DSurface9 *ScreenshotSurface;

	IDirect3DVertexBuffer9 *VertexBuffer;
	FBVERTEX *VertexData;
	IDirect3DIndexBuffer9 *IndexBuffer;
	WORD *IndexData;
	BufferedQuad *QuadExtra;
	int VertexPos;
	int IndexPos;
	int QuadBatchPos;
	enum { BATCH_None, BATCH_Quads, BATCH_Lines } BatchType;

	IDirect3DPixelShader9 *PalTexShader, *PalTexBilinearShader, *InvPalTexShader;
	IDirect3DPixelShader9 *PlainShader, *InvPlainShader;
	IDirect3DPixelShader9 *RedToAlphaShader;
	IDirect3DPixelShader9 *ColorOnlyShader;
	IDirect3DPixelShader9 *GammaFixerShader;
	IDirect3DPixelShader9 *BurnShader;

	IDirect3DSurface9 *OldRenderTarget;
	IDirect3DTexture9 *InitialWipeScreen, *FinalWipeScreen;

	D3DFB() {}

	class Wiper
	{
	public:
		virtual ~Wiper();
		virtual bool Run(int ticks, D3DFB *fb) = 0;
	};

	class Wiper_Melt;			friend class Wiper_Melt;
	class Wiper_Burn;			friend class Wiper_Burn;
	class Wiper_Crossfade;		friend class Wiper_Crossfade;

	Wiper *ScreenWipe;
};

#if 0
#define STARTLOG		do { if (!dbg) dbg = fopen ("k:/vid.log", "w"); } while(0)
#define STOPLOG			do { if (dbg) { fclose (dbg); dbg=NULL; } } while(0)
#define LOG(x)			do { if (dbg) { fprintf (dbg, x); fflush (dbg); } } while(0)
#define LOG1(x,y)		do { if (dbg) { fprintf (dbg, x, y); fflush (dbg); } } while(0)
#define LOG2(x,y,z)		do { if (dbg) { fprintf (dbg, x, y, z); fflush (dbg); } } while(0)
#define LOG3(x,y,z,zz)	do { if (dbg) { fprintf (dbg, x, y, z, zz); fflush (dbg); } } while(0)
#define LOG4(x,y,z,a,b)	do { if (dbg) { fprintf (dbg, x, y, z, a, b); fflush (dbg); } } while(0)
#define LOG5(x,y,z,a,b,c) do { if (dbg) { fprintf (dbg, x, y, z, a, b, c); fflush (dbg); } } while(0)
FILE *dbg;
#elif _DEBUG
#define STARTLOG
#define STOPLOG
#define LOG(x)			{ OutputDebugString(x); }
#define LOG1(x,y)		{ char poo[1024]; sprintf(poo, x, y); OutputDebugString(poo); }
#define LOG2(x,y,z)		{ char poo[1024]; sprintf(poo, x, y, z); OutputDebugString(poo); }
#define LOG3(x,y,z,zz)	{ char poo[1024]; sprintf(poo, x, y, z, zz); OutputDebugString(poo); }
#define LOG4(x,y,z,a,b)	{ char poo[1024]; sprintf(poo, x, y, z, a, b); OutputDebugString(poo); }
#define LOG5(x,y,z,a,b,c) { char poo[1024]; sprintf(poo, x, y, z, a, b, c); OutputDebugString(poo); }
#else
#define STARTLOG
#define STOPLOG
#define LOG(x)
#define LOG1(x,y)
#define LOG2(x,y,z)
#define LOG3(x,y,z,zz)
#define LOG4(x,y,z,a,b)
#define LOG5(x,y,z,a,b,c)
#endif

#endif // __WIN32IFACE_H
