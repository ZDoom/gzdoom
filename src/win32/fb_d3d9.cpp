/*
** fb_d3d9.cpp
** Code to let ZDoom use Direct3D 9 as a simple framebuffer
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
** This file does _not_ implement hardware-acclerated rendering. It is just
** a means of getting the pixel data to the screen in a more reliable
** method on modern hardware by copying the entire frame to a texture,
** drawing that to the screen, and presenting.
*/

// HEADER FILES ------------------------------------------------------------

#ifdef _DEBUG
#define D3D_DEBUG_INFO
#endif
#define DIRECT3D_VERSION 0x0900
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <d3d9.h>
#include <stdio.h>

#define USE_WINDOWS_DWORD
#include "doomtype.h"

#include "c_dispatch.h"
#include "templates.h"
#include "i_system.h"
#include "i_video.h"
#include "i_input.h"
#include "v_video.h"
#include "v_pfx.h"
#include "stats.h"
#include "doomerrors.h"
#include "r_draw.h"
#include "r_translate.h"

#include "win32iface.h"

#include <mmsystem.h>

// MACROS ------------------------------------------------------------------

// The number of vertices the vertex buffer should hold.
#define NUM_VERTS	28

// TYPES -------------------------------------------------------------------

IMPLEMENT_CLASS(D3DFB)

struct FBVERTEX
{
	FLOAT x, y, z, rhw;
	FLOAT tu, tv;
};
#define D3DFVF_FBVERTEX (D3DFVF_XYZRHW|D3DFVF_TEX1)

class D3DTex : public FNativeTexture
{
public:
	D3DTex(FTexture *tex, D3DFB *fb);
	~D3DTex();

	FTexture *GameTex;
	IDirect3DTexture9 *Tex;

	D3DTex **Prev;
	D3DTex *Next;

	// Texture coordinates to use for the lower-right corner, should this
	// texture prove to be larger than the game texture it represents.
	FLOAT TX, TY;

	bool IsGray;

	bool Create(IDirect3DDevice9 *D3DDevice);
	bool Update();
	D3DFORMAT GetTexFormat();
	FTextureFormat ToTexFmt(D3DFORMAT fmt);
};

class D3DPal : public FNativeTexture
{
public:
	D3DPal(FRemapTable *remap, D3DFB *fb);
	~D3DPal();

	D3DPal **Prev;
	D3DPal *Next;

	IDirect3DTexture9 *Tex;

	bool Update();

	FRemapTable *Remap;
	int RoundedPaletteSize;
};

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

void DoBlending (const PalEntry *from, PalEntry *to, int count, int r, int g, int b, int a);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern HWND Window;
extern IVideo *Video;
extern BOOL AppActive;
extern int SessionState;
extern bool VidResizing;

EXTERN_CVAR (Bool, fullscreen)
EXTERN_CVAR (Float, Gamma)
EXTERN_CVAR (Int, vid_displaybits)
EXTERN_CVAR (Bool, vid_vsync)
EXTERN_CVAR (Float, transsouls)

extern IDirect3D9 *D3D;

extern cycle_t BlitCycles;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

#include "fb_d3d9_shaders.h"

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// CODE --------------------------------------------------------------------

D3DFB::D3DFB (int width, int height, bool fullscreen)
	: BaseWinFB (width, height)
{
	D3DPRESENT_PARAMETERS d3dpp;

	D3DDevice = NULL;
	VertexBuffer = NULL;
	FBTexture = NULL;
	WindowedRenderTexture = NULL;
	PaletteTexture = NULL;
	StencilPaletteTexture = NULL;
	ShadedPaletteTexture = NULL;
	PalTexShader = NULL;
	PlainShader = NULL;
	PlainStencilShader = NULL;
	DimShader = NULL;
	GammaFixerShader = NULL;
	FBFormat = D3DFMT_UNKNOWN;
	PalFormat = D3DFMT_UNKNOWN;
	VSync = vid_vsync;
	BlendingRect.left = 0;
	BlendingRect.top = 0;
	BlendingRect.right = FBWidth;
	BlendingRect.bottom = FBHeight;
	UseBlendingRect = false;
	In2D = 0;
	Palettes = NULL;
	Textures = NULL;
	Accel2D = true;

	Gamma = 1.0;
	FlashConstants[0][3] = FlashConstants[0][2] = FlashConstants[0][1] = FlashConstants[0][0] = 0;
	FlashConstants[1][3] = FlashConstants[1][2] = FlashConstants[1][1] = FlashConstants[1][0] = 1;
	FlashColor = 0;
	FlashAmount = 0;

	NeedGammaUpdate = false;
	NeedPalUpdate = false;

	if (MemBuffer == NULL)
	{
		return;
	}

	memcpy (SourcePalette, GPalette.BaseColors, sizeof(PalEntry)*256);

	Windowed = !(static_cast<Win32Video *>(Video)->GoFullscreen (fullscreen));

	TrueHeight = height;
	if (fullscreen)
	{
		for (Win32Video::ModeInfo *mode = static_cast<Win32Video *>(Video)->m_Modes; mode != NULL; mode = mode->next)
		{
			if (mode->width == Width && mode->height == Height)
			{
				TrueHeight = mode->realheight;
				break;
			}
		}
	}

	FillPresentParameters (&d3dpp, fullscreen, VSync);

	HRESULT hr;

	if (FAILED(hr = D3D->CreateDevice (D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, Window,
		D3DCREATE_SOFTWARE_VERTEXPROCESSING | D3DCREATE_FPU_PRESERVE, &d3dpp, &D3DDevice)))
	{
		D3DDevice = NULL;
		if (fullscreen)
		{
			d3dpp.BackBufferFormat = D3DFMT_R5G6B5;
			if (FAILED(D3D->CreateDevice (D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, Window,
				D3DCREATE_SOFTWARE_VERTEXPROCESSING | D3DCREATE_FPU_PRESERVE, &d3dpp, &D3DDevice)))
			{
				D3DDevice = NULL;
			}
		}
	}
	if (D3DDevice != NULL)
	{
		CreateResources ();

		// Be sure we know what the alpha blend is
		D3DDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
		AlphaBlendEnabled = FALSE;
		AlphaSrcBlend = D3DBLEND(0);
		AlphaDestBlend = D3DBLEND(0);
	}
}

D3DFB::~D3DFB ()
{
	KillNativeTexs();
	KillNativePals();
	ReleaseResources ();
	if (D3DDevice != NULL)
	{
		D3DDevice->Release();
	}
}

void D3DFB::FillPresentParameters (D3DPRESENT_PARAMETERS *pp, bool fullscreen, bool vsync)
{
	memset (pp, 0, sizeof(*pp));
	pp->Windowed = !fullscreen;
	pp->SwapEffect = D3DSWAPEFFECT_DISCARD;
	pp->BackBufferWidth = Width;
	pp->BackBufferHeight = TrueHeight;
	pp->BackBufferFormat = fullscreen ? D3DFMT_X8R8G8B8 : D3DFMT_UNKNOWN;
	pp->hDeviceWindow = Window;
	pp->PresentationInterval = vsync ? D3DPRESENT_INTERVAL_ONE : D3DPRESENT_INTERVAL_IMMEDIATE;
}

bool D3DFB::CreateResources ()
{
	if (!Windowed)
	{
		// Remove the window border in fullscreen mode
		SetWindowLong (Window, GWL_STYLE, WS_POPUP|WS_VISIBLE|WS_SYSMENU);
	}
	else
	{
		// Resize the window to match desired dimensions
		int sizew = Width + GetSystemMetrics (SM_CXSIZEFRAME)*2;
		int sizeh = Height + GetSystemMetrics (SM_CYSIZEFRAME) * 2 +
					 GetSystemMetrics (SM_CYCAPTION);
		LOG2 ("Resize window to %dx%d\n", sizew, sizeh);
		VidResizing = true;
		// Make sure the window has a border in windowed mode
		SetWindowLong (Window, GWL_STYLE, WS_VISIBLE|WS_OVERLAPPEDWINDOW);
		if (GetWindowLong (Window, GWL_EXSTYLE) & WS_EX_TOPMOST)
		{
			// Direct3D 9 will apparently add WS_EX_TOPMOST to fullscreen windows,
			// and removing it is a little tricky. Using SetWindowLongPtr to clear it
			// will not do the trick, but sending the window behind everything will.
			SetWindowPos (Window, HWND_BOTTOM, 0, 0, sizew, sizeh,
				SWP_DRAWFRAME | SWP_NOCOPYBITS | SWP_NOMOVE);
			SetWindowPos (Window, HWND_TOP, 0, 0, 0, 0, SWP_NOCOPYBITS | SWP_NOMOVE | SWP_NOSIZE);
		}
		else
		{
			SetWindowPos (Window, NULL, 0, 0, sizew, sizeh,
				SWP_DRAWFRAME | SWP_NOCOPYBITS | SWP_NOMOVE | SWP_NOZORDER);
		}
		I_RestoreWindowedPos ();
		VidResizing = false;
	}
	SM14 = false;
	if (FAILED(D3DDevice->CreatePixelShader (PalTexShader20Def, &PalTexShader)) &&
		(SM14 = true, FAILED(D3DDevice->CreatePixelShader (PalTexShader14Def, &PalTexShader))))
	{
		return false;
	}
	if (FAILED(D3DDevice->CreatePixelShader (PlainShaderDef, &PlainShader)) ||
		FAILED(D3DDevice->CreatePixelShader (PlainStencilDef, &PlainStencilShader)) ||
		FAILED(D3DDevice->CreatePixelShader (DimShaderDef, &DimShader)))
	{
		return false;
	}
	if (FAILED(D3DDevice->CreatePixelShader (GammaFixerDef, &GammaFixerShader)))
	{
		Printf ("Windowed mode gamma will not work.\n");
		GammaFixerShader = NULL;
	}
	CurPixelShader = NULL;
	memset(Constant, 0, sizeof(Constant));
	if (!CreateFBTexture() ||
		!CreatePaletteTexture() ||
		!CreateStencilPaletteTexture() ||
		!CreateShadedPaletteTexture())
	{
		return false;
	}
	if (!CreateVertexes())
	{
		return false;
	}
	SetGamma (Gamma);

	return true;
}

void D3DFB::ReleaseResources ()
{
	I_SaveWindowedPos ();
	if (FBTexture != NULL)
	{
		FBTexture->Release();
		FBTexture = NULL;
	}
	if (WindowedRenderTexture != NULL)
	{
		WindowedRenderTexture->Release();
		WindowedRenderTexture = NULL;
	}
	if (VertexBuffer != NULL)
	{
		VertexBuffer->Release();
		VertexBuffer = NULL;
	}
	if (PaletteTexture != NULL)
	{
		PaletteTexture->Release();
		PaletteTexture = NULL;
	}
	if (StencilPaletteTexture != NULL)
	{
		StencilPaletteTexture->Release();
		StencilPaletteTexture = NULL;
	}
	if (ShadedPaletteTexture != NULL)
	{
		ShadedPaletteTexture->Release();
		ShadedPaletteTexture = NULL;
	}
	if (PalTexShader != NULL)
	{
		PalTexShader->Release();
		PalTexShader = NULL;
	}
	if (PlainShader != NULL)
	{
		PlainShader->Release();
		PlainShader = NULL;
	}
	if (PlainStencilShader != NULL)
	{
		PlainStencilShader->Release();
		PlainStencilShader = NULL;
	}
	if (DimShader != NULL)
	{
		DimShader->Release();
		DimShader = NULL;
	}
	if (GammaFixerShader != NULL)
	{
		GammaFixerShader->Release();
		GammaFixerShader = NULL;
	}
}

bool D3DFB::Reset ()
{
	D3DPRESENT_PARAMETERS d3dpp;

	// Free resources created with D3DPOOL_DEFAULT.
	if (FBTexture != NULL)
	{
		FBTexture->Release();
		FBTexture = NULL;
	}
	if (WindowedRenderTexture != NULL)
	{
		WindowedRenderTexture->Release();
		WindowedRenderTexture = NULL;
	}
	if (VertexBuffer != NULL)
	{
		VertexBuffer->Release();
		VertexBuffer = NULL;
	}
	FillPresentParameters (&d3dpp, !Windowed, VSync);
	if (!SUCCEEDED(D3DDevice->Reset (&d3dpp)))
	{
		return false;
	}
	if (!CreateFBTexture() || !CreateVertexes())
	{
		return false;
	}
	return true;
}

//==========================================================================
//
// D3DFB :: KillNativePals
//
// Frees all native palettes.
//
//==========================================================================

void D3DFB::KillNativePals()
{
	while (Palettes != NULL)
	{
		delete Palettes;
	}
}

//==========================================================================
//
// D3DFB :: KillNativeTexs
//
// Frees all native textures.
//
//==========================================================================

void D3DFB::KillNativeTexs()
{
	while (Textures != NULL)
	{
		delete Textures;
	}
}

//==========================================================================
//
// D3DFB :: KillNativeNonPalettedTexs
//
// Frees all native textures that aren't paletted.
//
//==========================================================================

void D3DFB::KillNativeNonPalettedTexs()
{
	D3DTex *tex;
	D3DTex *next;

	for (tex = Textures; tex != NULL; tex = next)
	{
		next = tex->Next;
		if (tex->GetTexFormat() != D3DFMT_L8)
		{
			delete tex;
		}
	}
}

bool D3DFB::CreateFBTexture ()
{
	if (FAILED(D3DDevice->CreateTexture (Width, Height, 1, D3DUSAGE_DYNAMIC, D3DFMT_L8, D3DPOOL_DEFAULT, &FBTexture, NULL)))
	{
		int pow2width, pow2height, i;

		for (i = 1; i < Width; i <<= 1) {} pow2width = i;
		for (i = 1; i < Height; i <<= 1) {} pow2height = i;

		if (FAILED(D3DDevice->CreateTexture (pow2width, pow2height, 1, D3DUSAGE_DYNAMIC, D3DFMT_L8, D3DPOOL_DEFAULT, &FBTexture, NULL)))
		{
			return false;
		}
		else
		{
			FBWidth = pow2width;
			FBHeight = pow2height;
		}
	}
	else
	{
		FBWidth = Width;
		FBHeight = Height;
	}
	if (Windowed && GammaFixerShader)
	{
		if (FAILED(D3DDevice->CreateTexture (FBWidth, FBHeight, 1, D3DUSAGE_RENDERTARGET, D3DFMT_X8R8G8B8, D3DPOOL_DEFAULT, &WindowedRenderTexture, NULL)))
		{
			WindowedRenderTexture = false;
		}
	}
	return true;
}

bool D3DFB::CreatePaletteTexture ()
{
	if (FAILED(D3DDevice->CreateTexture (256, 1, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &PaletteTexture, NULL)))
	{
		return false;
	}
	PalFormat = D3DFMT_A8R8G8B8;
	return true;
}

bool D3DFB::CreateStencilPaletteTexture()
{
	// The stencil palette is a special palette where the first entry is zero alpha,
	// and everything else is white with full alpha.
	if (FAILED(D3DDevice->CreateTexture(256, 1, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &StencilPaletteTexture, NULL)))
	{
		return false;
	}
	D3DLOCKED_RECT lockrect;
	if (SUCCEEDED(StencilPaletteTexture->LockRect(0, &lockrect, NULL, 0)))
	{
		DWORD *pix = (DWORD *)lockrect.pBits;
		*pix = 0;
		memset(pix + 1, 0xFF, 255*4);
		StencilPaletteTexture->UnlockRect(0);
	}
	return true;
}

bool D3DFB::CreateShadedPaletteTexture()
{
	// The shaded palette is similar to the stencil palette, except each entry's
	// alpha is the same as its index.
	if (FAILED(D3DDevice->CreateTexture(256, 1, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &ShadedPaletteTexture, NULL)))
	{
		return false;
	}
	D3DLOCKED_RECT lockrect;
	if (SUCCEEDED(ShadedPaletteTexture->LockRect(0, &lockrect, NULL, 0)))
	{
		BYTE *pix = (BYTE *)lockrect.pBits;
		for (int i = 0; i < 256; ++i)
		{
			pix[3] = i;
			pix[2] = pix[1] = pix[0] = 255;
			pix += 4;
		}
		ShadedPaletteTexture->UnlockRect(0);
	}
	return true;
}

bool D3DFB::CreateVertexes ()
{
	if (FAILED(D3DDevice->CreateVertexBuffer (sizeof(FBVERTEX)*NUM_VERTS, D3DUSAGE_WRITEONLY, D3DFVF_FBVERTEX, D3DPOOL_DEFAULT, &VertexBuffer, NULL)) ||
		!UploadVertices())
	{
		return false;
	}
	return true;
}

bool D3DFB::UploadVertices()
{
	float top = (TrueHeight - Height) * 0.5f - 0.5f;
	float right = float(Width) - 0.5f;
	float bot = float(Height) + top;
	float texright = float(Width) / float(FBWidth);
	float texbot = float(Height) / float(FBHeight);
	void *pverts;

	if ((BlendingRect.left <= 0 && BlendingRect.right >= FBWidth &&
		 BlendingRect.top <= 0 && BlendingRect.bottom >= FBHeight) ||
		BlendingRect.left >= BlendingRect.right ||
		BlendingRect.top >= BlendingRect.bottom)
	{
		// Blending rect covers the whole screen, so only need 4 verts.
		FBVERTEX verts[4] =
		{
			{ -0.5f, top, 0.5f, 1.f,      0.f,    0.f },
			{ right, top, 0.5f, 1.f, texright,    0.f },
			{ right, bot, 0.5f, 1.f, texright, texbot },
			{ -0.5f, bot, 0.5f, 1.f,      0.f, texbot }
		};
		if (SUCCEEDED(VertexBuffer->Lock(0, sizeof(verts), &pverts, 0)))
		{
			memcpy (pverts, verts, sizeof(verts));
			VertexBuffer->Unlock();
			return true;
		}
		return false;
	}
	// Only the 3D area of the screen is effected by palette flashes.
	// So we create some boxes around it that can be drawn without the
	// flash. These include the corners of the view area so I can be
	// sure the texture interpolation is consistant. (Well, actually,
	// since it's a 1-to-1 pixel mapping, it shouldn't matter.)
	float mxl = float(BlendingRect.left) - 0.5f;
	float mxr = float(BlendingRect.right) - 0.5f;
	float myt = float(BlendingRect.top) + top;
	float myb = float(BlendingRect.bottom) + top;
	float tmxl = float(BlendingRect.left) / float(Width) * texright;
	float tmxr = float(BlendingRect.right) / float(Width) * texright;
	float tmyt = float(BlendingRect.top) / float(Height) * texbot;
	float tmyb = float(BlendingRect.bottom) / float(Height) * texbot;
	/*  +-------------------+
	    |                   |
		+-----+-------+-----+
		|     |       |     |
		|     |       |     |
		|     |       |     |
		+-----+-------+-----+
		|                   |
		+-------------------+  */
	FBVERTEX verts[28] =
	{
		// The whole screen, for when no blending is happening
		{ -0.5f, top, 0.5f, 1.f,      0.f,    0.f },		// 0
		{ right, top, 0.5f, 1.f, texright,    0.f },
		{ right, bot, 0.5f, 1.f, texright, texbot },
		{ -0.5f, bot, 0.5f, 1.f,      0.f, texbot },

		// Left area
		{ -0.5f, myt, 0.5f, 1.f,      0.f,   tmyt },		// 4
		{   mxl, myt, 0.5f, 1.f,     tmxl,   tmyt },
		{   mxl, myb, 0.5f, 1.f,     tmxl,   tmyb },
		{ -0.5f, myb, 0.5f, 1.f,      0.f,   tmyb },

		// Right area
		{   mxr, myt, 0.5f, 1.f,     tmxr,   tmyt },		// 8
		{ right, myt, 0.5f, 1.f, texright,   tmyt },
		{ right, myb, 0.5f, 1.f, texright,   tmyb },
		{   mxr, myb, 0.5f, 1.f,     tmxr,   tmyb },

		// Bottom area
		{ -0.5f, bot, 0.5f, 1.f,      0.f, texbot },		// 12
		{ -0.5f, myb, 0.5f, 1.f,      0.f,   tmyb },
		{   mxl, myb, 0.5f, 1.f,     tmxl,   tmyb },
		{   mxr, myb, 0.5f, 1.f,     tmxr,   tmyb },
		{ right, myb, 0.5f, 1.f, texright,   tmyb },
		{ right, bot, 0.5f, 1.f, texright, texbot },

		// Top area
		{ right, top, 0.5f, 1.f, texright,    0.f },		// 18
		{ right, myt, 0.5f, 1.f, texright,   tmyt },
		{   mxr, myt, 0.5f, 1.f,     tmxr,   tmyt },
		{   mxl, myt, 0.5f, 1.f,     tmxl,   tmyt },
		{ -0.5f, myt, 0.5f, 1.f,      0.f,   tmyt },
		{ -0.5f, top, 0.5f, 1.f,      0.f,    0.f },

		// Middle (blended) area
		{   mxl, myt, 0.5f, 1.f,    tmxl,    tmyt },		// 24
		{   mxr, myt, 0.5f, 1.f,    tmxr,    tmyt },
		{   mxr, myb, 0.5f, 1.f,    tmxr,    tmyb },
		{   mxl, myb, 0.5f, 1.f,    tmxl,    tmyb }
	};
	if (SUCCEEDED(VertexBuffer->Lock(0, sizeof(verts), &pverts, 0)))
	{
		memcpy (pverts, verts, sizeof(verts));
		VertexBuffer->Unlock();
		return true;
	}
	return false;
}

int D3DFB::GetPageCount ()
{
	return 1;
}

void D3DFB::PaletteChanged ()
{
}

int D3DFB::QueryNewPalette ()
{
	return 0;
}

bool D3DFB::IsValid ()
{
	return D3DDevice != NULL;
}

HRESULT D3DFB::GetHR ()
{
	return 0;
}

bool D3DFB::IsFullscreen ()
{
	return !Windowed;
}

bool D3DFB::Lock ()
{
	return Lock(true);
}

bool D3DFB::Lock (bool buffered)
{
	if (LockCount++ > 0)
	{
		return false;
	}

	Buffer = MemBuffer;
	return false;
}

void D3DFB::Unlock ()
{
	LOG1 ("Unlock     <%d>\n", LockCount);

	if (LockCount == 0)
	{
		return;
	}

	if (UpdatePending && LockCount == 1)
	{
		Update ();
	}
	else if (--LockCount == 0)
	{
		Buffer = NULL;
	}
}

// When In2D == 0: Copy buffer to screen and present
// When In2D == 1: Copy buffer to screen but do not present
// When In2D == 2: Present and set In2D to 0
void D3DFB::Update ()
{
	if (In2D == 2)
	{
		DrawRateStuff();
		DoWindowedGamma();
		D3DDevice->EndScene();
		D3DDevice->Present(NULL, NULL, NULL, NULL);
		In2D = 0;
		return;
	}

	if (LockCount != 1)
	{
		I_FatalError ("Framebuffer must have exactly 1 lock to be updated");
		if (LockCount > 0)
		{
			UpdatePending = true;
			--LockCount;
		}
		return;
	}

	if (In2D == 0)
	{
		DrawRateStuff();
	}

	if (NeedGammaUpdate)
	{
		float psgamma[4];
		float igamma;

		NeedGammaUpdate = false;
		igamma = 1 / Gamma;
		if (!Windowed)
		{
			D3DGAMMARAMP ramp;

			for (int i = 0; i < 256; ++i)
			{
				ramp.blue[i] = ramp.green[i] = ramp.red[i] = WORD(65535.f * powf(i / 255.f, igamma));
			}
			D3DDevice->SetGammaRamp(0, D3DSGR_CALIBRATE, &ramp);
		}
		psgamma[2] = psgamma[1] = psgamma[0] = igamma;
		psgamma[3] = 1;
		D3DDevice->SetPixelShaderConstantF(4, psgamma, 1);
		NeedPalUpdate = true;
	}
	
	if (NeedPalUpdate)
	{
		UploadPalette();
	}

	BlitCycles = 0;
	clock (BlitCycles);

	LockCount = 0;
	PaintToWindow ();
	if (In2D == 0)
	{
		DoWindowedGamma();
		D3DDevice->EndScene();
		D3DDevice->Present(NULL, NULL, NULL, NULL);
	}

	unclock (BlitCycles);
	LOG1 ("cycles = %d\n", BlitCycles);

	Buffer = NULL;
	UpdatePending = false;
}

bool D3DFB::PaintToWindow ()
{
	HRESULT hr;

	if (LockCount != 0)
	{
		return false;
	}
	hr = D3DDevice->TestCooperativeLevel();
	if (FAILED(hr))
	{
		if (hr != D3DERR_DEVICENOTRESET || !Reset())
		{
			Sleep (1);
			return false;
		}
	}
	Draw3DPart();
	return true;
}

void D3DFB::Draw3DPart()
{
	RECT texrect = { 0, 0, Width, Height };
	D3DLOCKED_RECT lockrect;

	if ((FBWidth == Width && FBHeight == Height &&
		SUCCEEDED(FBTexture->LockRect (0, &lockrect, NULL, D3DLOCK_DISCARD))) ||
		SUCCEEDED(FBTexture->LockRect (0, &lockrect, &texrect, 0)))
	{
		if (lockrect.Pitch == Pitch)
		{
			memcpy (lockrect.pBits, MemBuffer, Width * Height);
		}
		else
		{
			BYTE *dest = (BYTE *)lockrect.pBits;
			BYTE *src = MemBuffer;
			for (int y = 0; y < Height; y++)
			{
				memcpy (dest, src, Width);
				dest += lockrect.Pitch;
				src += Pitch;
			}
		}
		FBTexture->UnlockRect (0);
	}
	if (TrueHeight != Height)
	{
		// Letterbox! Draw black top and bottom borders.
		int topborder = (TrueHeight - Height) / 2;
		D3DRECT rects[2] = { { 0, 0, Width, topborder }, { 0, Height + topborder, Width, TrueHeight } };
		D3DDevice->Clear (2, rects, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0,0,0), 1.f, 0);
	}
	D3DDevice->BeginScene();
	OldRenderTarget = NULL;
	if (WindowedRenderTexture != NULL)
	{
		IDirect3DSurface9 *targetsurf;
		if (FAILED(WindowedRenderTexture->GetSurfaceLevel(0, &targetsurf)) ||
			FAILED(D3DDevice->GetRenderTarget(0, &OldRenderTarget)) ||
			FAILED(D3DDevice->SetRenderTarget(0, targetsurf)))
		{
			// Setting the render target failed.
			OldRenderTarget = NULL;
		}
	}

	SetTexture (0, FBTexture);
	SetPaletteTexture(PaletteTexture, 256);
	D3DDevice->SetStreamSource (0, VertexBuffer, 0, sizeof(FBVERTEX));
	D3DDevice->SetFVF (D3DFVF_FBVERTEX);
	D3DDevice->SetPixelShaderConstantF (0, FlashConstants[0], 2);
	memcpy(Constant, FlashConstants, sizeof(FlashConstants));
	SetAlphaBlend(FALSE);
	if (!UseBlendingRect || FlashConstants[1][0] == 1)
	{ // The whole screen as a single quad.
		D3DDevice->DrawPrimitive (D3DPT_TRIANGLEFAN, 0, 2);
	}
	else
	{ // The screen split up so that only the 3D view is blended.
		D3DDevice->DrawPrimitive (D3DPT_TRIANGLEFAN, 24, 2);	// middle

		if (!In2D || 1)
		{
			// The rest is drawn unblended, so reset the shader constant.
			static const float FlashZero[2][4] = { { 0, 0, 0, 0 }, { 1, 1, 1, 1 } };
			D3DDevice->SetPixelShaderConstantF (0, FlashZero[0], 2);
			memcpy(Constant, FlashZero, sizeof(FlashZero));

			D3DDevice->DrawPrimitive (D3DPT_TRIANGLEFAN,  4, 2);	// left
			D3DDevice->DrawPrimitive (D3DPT_TRIANGLEFAN,  8, 2);	// right
			D3DDevice->DrawPrimitive (D3DPT_TRIANGLEFAN, 12, 4);	// bottom
			D3DDevice->DrawPrimitive (D3DPT_TRIANGLEFAN, 18, 4);	// top
		}
	}
}

//==========================================================================
//
// D3DFB :: DoWindowedGamma
//
// Draws the render target texture to the real back buffer using a gamma-
// correcting pixel shader.
//
//==========================================================================

void D3DFB::DoWindowedGamma()
{
	if (OldRenderTarget != NULL)
	{
		D3DDevice->SetRenderTarget(0, OldRenderTarget);
		D3DDevice->SetStreamSource(0, VertexBuffer, 0, sizeof(FBVERTEX));
		D3DDevice->SetFVF(D3DFVF_FBVERTEX);
		SetTexture(0, WindowedRenderTexture);
		SetPixelShader(GammaFixerShader);
		SetAlphaBlend(FALSE);
		D3DDevice->DrawPrimitive(D3DPT_TRIANGLEFAN, 0, 2);
		OldRenderTarget = NULL;
	}
}

void D3DFB::UploadPalette ()
{
	D3DLOCKED_RECT lockrect;
	int i;

	if (SUCCEEDED(PaletteTexture->LockRect (0, &lockrect, NULL, 0)))
	{
		BYTE *pix = (BYTE *)lockrect.pBits;
		for (i = 0; i < 256; ++i, pix += 4)
		{
			pix[0] = SourcePalette[i].b;
			pix[1] = SourcePalette[i].g;
			pix[2] = SourcePalette[i].r;
			pix[3] = (i == 0 ? 0 : 255);
			// To let masked textures work, the first palette entry's alpha is 0.
		}
		PaletteTexture->UnlockRect (0);
	}
}

PalEntry *D3DFB::GetPalette ()
{
	return SourcePalette;
}

void D3DFB::UpdatePalette ()
{
	NeedPalUpdate = true;
}

bool D3DFB::SetGamma (float gamma)
{
	LOG1 ("SetGamma %g\n", gamma);
	Gamma = gamma;
	NeedGammaUpdate = true;
	return true;
}

bool D3DFB::SetFlash (PalEntry rgb, int amount)
{
	FlashColor = rgb;
	FlashAmount = amount;

	// Fill in the constants for the pixel shader to do linear interpolation between the palette and the flash:
	float r = rgb.r / 255.f, g = rgb.g / 255.f, b = rgb.b / 255.f, a = amount / 256.f;
	FlashConstants[0][0] = r * a;
	FlashConstants[0][1] = g * a;
	FlashConstants[0][2] = b * a;
	a = 1 - a;
	FlashConstants[1][0] = a;
	FlashConstants[1][1] = a;
	FlashConstants[1][2] = a;
	return true;
}

void D3DFB::GetFlash (PalEntry &rgb, int &amount)
{
	rgb = FlashColor;
	amount = FlashAmount;
}

void D3DFB::GetFlashedPalette (PalEntry pal[256])
{
	memcpy (pal, SourcePalette, 256*sizeof(PalEntry));
	if (FlashAmount)
	{
		DoBlending (pal, pal, 256, FlashColor.r, FlashColor.g, FlashColor.b, FlashAmount);
	}
}

void D3DFB::SetVSync (bool vsync)
{
	if (VSync != vsync)
	{
		VSync = vsync;
		Reset();
	}
}

void D3DFB::Blank ()
{
	// Only used by movie player, which isn't working with D3D9 yet.
}

void D3DFB::SetBlendingRect(int x1, int y1, int x2, int y2)
{
	if (BlendingRect.left != x1 ||
		BlendingRect.top != y1 ||
		BlendingRect.right != x2 ||
		BlendingRect.bottom != y2)
	{
		BlendingRect.left = x1;
		BlendingRect.top = y1;
		BlendingRect.right = x2;
		BlendingRect.bottom = y2;

		if (UploadVertices())
		{
			UseBlendingRect = ((x1 > 0 || x2 < FBWidth || y1 > 0 || y2 < FBHeight)
				&& BlendingRect.left < BlendingRect.right
				&& BlendingRect.top < BlendingRect.bottom);
		}
	}
}

/**************************************************************************/
/*                                  2D Stuff                              */
/**************************************************************************/

//==========================================================================
//
// D3DTex Constructor
//
//==========================================================================

D3DTex::D3DTex(FTexture *tex, D3DFB *fb)
{
	// Attach to the texture list for the D3DFB
	Next = fb->Textures;
	if (Next != NULL)
	{
		Next->Prev = &Next;
	}
	Prev = &fb->Textures;
	fb->Textures = this;

	GameTex = tex;
	Tex = NULL;
	IsGray = false;

	Create(fb->D3DDevice);
}

//==========================================================================
//
// D3DTex Destructor
//
//==========================================================================

D3DTex::~D3DTex()
{
	if (Tex != NULL)
	{
		Tex->Release();
		Tex = NULL;
	}
	// Detach from the texture list
	*Prev = Next;
	if (Next != NULL)
	{
		Next->Prev = Prev;
	}
	// Remove link from the game texture
	if (GameTex != NULL)
	{
		GameTex->Native = NULL;
	}
}

//==========================================================================
//
// D3DTex :: Create
//
// Creates an IDirect3DTexture9 for the texture and copies the image data
// to it. Note that unlike FTexture, this image is row-major.
//
//==========================================================================

bool D3DTex::Create(IDirect3DDevice9 *D3DDevice)
{
	HRESULT hr;
	int w, h;

	if (Tex != NULL)
	{
		Tex->Release();
		Tex = NULL;
	}

	w = GameTex->GetWidth();
	h = GameTex->GetHeight();

	hr = D3DDevice->CreateTexture(w, h, 1, 0,
		GetTexFormat(), D3DPOOL_MANAGED, &Tex, NULL);
	if (FAILED(hr))
	{ // Try again, using power-of-2 sizes
		int i;

		for (i = 1; i < w; i <<= 1) {} w = i;
		for (i = 1; i < h; i <<= 1) {} h = i;
		hr = D3DDevice->CreateTexture(w, h, 1, 0,
			GetTexFormat(), D3DPOOL_MANAGED, &Tex, NULL);
		if (FAILED(hr))
		{
			return false;
		}
	}
	if (!Update())
	{
		Tex->Release();
		Tex = NULL;
		return false;
	}
	return true;
}

//==========================================================================
//
// D3DTex :: Update
//
// Copies image data from the underlying FTexture to the D3D texture.
//
//==========================================================================

bool D3DTex::Update()
{
	D3DSURFACE_DESC desc;
	D3DLOCKED_RECT lrect;
	RECT rect;

	assert(Tex != NULL);
	assert(GameTex != NULL);

	if (FAILED(Tex->GetLevelDesc(0, &desc)))
	{
		return false;
	}
	rect.left = 0;
	rect.top = 0;
	rect.right = GameTex->GetWidth();
	rect.bottom = GameTex->GetHeight();
	if (FAILED(Tex->LockRect(0, &lrect, &rect, 0)))
	{
		return false;
	}
	GameTex->FillBuffer((BYTE *)lrect.pBits, lrect.Pitch, rect.bottom, ToTexFmt(desc.Format));
	Tex->UnlockRect(0);
	return true;
}

//==========================================================================
//
// D3DTex :: GetTexFormat
//
// Returns the texture format that would best fit this texture.
//
//==========================================================================

D3DFORMAT D3DTex::GetTexFormat()
{
	FTextureFormat fmt = GameTex->GetFormat();

	IsGray = false;

	switch (fmt)
	{
	case TEX_Pal:	return D3DFMT_L8;
	case TEX_Gray:	IsGray = true; return D3DFMT_L8;
	case TEX_RGB:	return D3DFMT_A8R8G8B8;
	case TEX_DXT1:	return D3DFMT_DXT1;
	case TEX_DXT2:	return D3DFMT_DXT2;
	case TEX_DXT3:	return D3DFMT_DXT3;
	case TEX_DXT4:	return D3DFMT_DXT4;
	case TEX_DXT5:	return D3DFMT_DXT5;
	default:		I_FatalError ("GameTex->GetFormat() returned invalid format.");
	}
	return D3DFMT_L8;
}

//==========================================================================
//
// D3DTex :: ToTexFmt
//
// Converts a D3DFORMAT constant to something the FTexture system
// understands.
//
//==========================================================================

FTextureFormat D3DTex::ToTexFmt(D3DFORMAT fmt)
{
	switch (fmt)
	{
	case D3DFMT_L8:			return IsGray ? TEX_Gray : TEX_Pal;
	case D3DFMT_A8R8G8B8:	return TEX_RGB;
	case D3DFMT_DXT1:		return TEX_DXT1;
	case D3DFMT_DXT2:		return TEX_DXT2;
	case D3DFMT_DXT3:		return TEX_DXT3;
	case D3DFMT_DXT4:		return TEX_DXT4;
	case D3DFMT_DXT5:		return TEX_DXT5;
	default:
		assert(0);	// LOL WUT?
		return TEX_Pal;
	}
}

//==========================================================================
//
// D3DPal Constructor
//
//==========================================================================

D3DPal::D3DPal(FRemapTable *remap, D3DFB *fb)
	: Tex(NULL), Remap(remap)
{
	int count;

	// Attach to the palette list for the D3DFB
	Next = fb->Palettes;
	if (Next != NULL)
	{
		Next->Prev = &Next;
	}
	Prev = &fb->Palettes;
	fb->Palettes = this;

	// Palette textures must be 256 entries for Shader Model 1.4
	if (fb->SM14)
	{
		count = 256;
	}
	else
	{
		int pow2count;

		// Round up to the nearest power of 2.
		for (pow2count = 1; pow2count < remap->NumEntries; pow2count <<= 1)
		{ }
		count = pow2count;
	}
	RoundedPaletteSize = count;
	if (SUCCEEDED(fb->D3DDevice->CreateTexture(count, 1, 1, 0, 
		D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &Tex, NULL)))
	{
		if (!Update())
		{
			Tex->Release();
			Tex = NULL;
		}
	}
}

//==========================================================================
//
// D3DPal Destructor
//
//==========================================================================

D3DPal::~D3DPal()
{
	if (Tex != NULL)
	{
		Tex->Release();
		Tex = NULL;
	}
	// Detach from the palette list
	*Prev = Next;
	if (Next != NULL)
	{
		Next->Prev = Prev;
	}
	// Remove link from the remap table
	if (Remap != NULL)
	{
		Remap->Native = NULL;
	}
}

//==========================================================================
//
// D3DPal :: Update
//
// Copies the palette to the texture.
//
//==========================================================================

bool D3DPal::Update()
{
	D3DLOCKED_RECT lrect;
	D3DCOLOR *buff;
	const PalEntry *pal;

	assert(Tex != NULL);

	if (FAILED(Tex->LockRect(0, &lrect, NULL, 0)))
	{
		return false;
	}
	buff = (D3DCOLOR *)lrect.pBits;
	pal = Remap->Palette;

	// Should I allow the source palette to specify alpha values?
	buff[0] = D3DCOLOR_ARGB(0, pal[0].r, pal[0].g, pal[0].b);
	for (int i = 1; i < Remap->NumEntries; ++i)
	{
		buff[i] = D3DCOLOR_XRGB(pal[i].r, pal[i].g, pal[i].b);
	}
	Tex->UnlockRect(0);
	return true;
}

//==========================================================================
//
// D3DFB :: Begin2D
//
// Begins 2D mode drawing operations. In particular, DrawTexture is
// rerouted to use Direct3D instead of the software renderer.
//
//==========================================================================

CVAR(Bool,test2d,true,0)
bool D3DFB::Begin2D()
{
	if (!test2d) return false;
	if (In2D)
	{
		return true;
	}
	In2D = 1;
	Update();
	In2D = 2;

	return true;
}

//==========================================================================
//
// D3DFB :: CreateTexture
//
// Returns a native texture that wraps a FTexture.
//
//==========================================================================

FNativeTexture *D3DFB::CreateTexture(FTexture *gametex)
{
	D3DTex *tex = new D3DTex(gametex, this);
	if (tex->Tex == NULL)
	{
		delete tex;
		return NULL;
	}
	return tex;
}

//==========================================================================
//
// D3DFB :: CreatePalette
//
// Returns a native texture that contains a palette.
//
//==========================================================================

FNativeTexture *D3DFB::CreatePalette(FRemapTable *remap)
{
	D3DPal *tex = new D3DPal(remap, this);
	if (tex->Tex == NULL)
	{
		delete tex;
		return NULL;
	}
	return tex;
}

//==========================================================================
//
// D3DFB :: Clear
//
// Fills the specified region with a color.
//
//==========================================================================

void D3DFB::Clear (int left, int top, int right, int bottom, int palcolor, uint32 color)
{
	if (In2D < 2)
	{
		Super::Clear(left, top, right, bottom, palcolor, color);
		return;
	}
	if (palcolor >= 0)
	{
		color = GPalette.BaseColors[palcolor];
	}
	else if (APART(color) < 255)
	{
		Dim(color, APART(color)/255.f, left, top, right - left, bottom - top);
		return;
	}
	D3DRECT rect = { left, top, right, bottom };
	D3DDevice->Clear(1, &rect, D3DCLEAR_TARGET, color | 0xFF000000, 1.f, 0);
}

//==========================================================================
//
// D3DFB :: Dim
//
//==========================================================================

void D3DFB::Dim (PalEntry color, float amount, int x1, int y1, int w, int h)
{
	if (amount <= 0)
		return;

	if (In2D < 2)
	{
		Super::Dim(color, amount, x1, y1, w, h);
		return;
	}
	if (amount >= 1)
	{
		D3DRECT rect = { x1, y1, x1 + w, y1 + h };
		D3DDevice->Clear(1, &rect, D3DCLEAR_TARGET, color | 0xFF000000, 1.f, 0);
	}
	else
	{
		FBVERTEX verts[4] =
		{
			{ x1-0.5f,   y1-0.5f,   0.5f, 1, 0, 0 },
			{ x1+w-0.5f, y1-0.5f,   0.5f, 1, 0, 0 },
			{ x1+w-0.5f, y1+h-0.5f, 0.5f, 1, 0, 0 },
			{ x1-0.5f,   y1+h-0.5f, 0.5f, 1, 0, 0 }
		};
		SetAlphaBlend(TRUE, D3DBLEND_SRCALPHA, D3DBLEND_INVSRCALPHA);
		SetPixelShader(DimShader);
		SetConstant(1, color.r/255.f, color.g/255.f, color.b/255.f, amount);
		D3DDevice->DrawPrimitiveUP(D3DPT_TRIANGLEFAN, 2, &verts, sizeof(FBVERTEX));
	}
}

//==========================================================================
//
// D3DFB :: DrawTextureV
//
// If not in 2D mode, just call the normal software version.
// If in 2D mode, then use Direct3D calls to perform the drawing.
//
//==========================================================================

void STACK_ARGS D3DFB::DrawTextureV (FTexture *img, int x, int y, uint32 tags_first, va_list tags)
{
	if (In2D < 2)
	{
		Super::DrawTextureV(img, x, y, tags_first, tags);
		return;
	}

	DrawParms parms;

	if (!ParseDrawTextureTags(img, x, y, tags_first, tags, &parms, true))
	{
		return;
	}

	D3DTex *tex = static_cast<D3DTex *>(img->GetNative());

	if (tex == NULL)
	{
		assert(tex != NULL);
		return;
	}

	float xscale = float(parms.destwidth) / parms.texwidth / 65536.f;
	float yscale = float(parms.destheight) / parms.texheight / 65536.f;
	float x0 = float(parms.x) / 65536.f - float(parms.left) * xscale;
	float y0 = float(parms.y) / 65536.f - float(parms.top) * yscale;
	float x1 = x0 + float(parms.destwidth) / 65536.f; 
	float y1 = y0 + float(parms.destheight) / 65536.f;
	float u0 = 0.f;
	float v0 = 0.f;
	float u1 = 1.f;
	float v1 = 1.f;
	float uscale = 1.f / parms.texwidth / u1;
	float vscale = 1.f / parms.texheight / v1 / yscale;

	if (y0 < parms.uclip)
	{
		v0 += float(parms.uclip - y0) * vscale;
		y0 = float(parms.uclip);
	}
	if (y1 > parms.dclip)
	{
		v1 -= float(y1 - parms.dclip) * vscale;
		y1 = float(parms.dclip);
	}

	if (parms.flipX)
	{
		swap(u0, u1);
	}
	if (parms.windowleft > 0 || parms.windowright < parms.texwidth)
	{
		x0 += parms.windowleft * xscale;
		u0 += parms.windowleft * uscale;
		x1 -= (parms.texwidth - parms.windowright) * xscale;
		u1 -= (parms.texwidth - parms.windowright) * uscale;
	}
	if (x0 < parms.lclip)
	{
		u0 += float(parms.lclip - x0) * uscale / xscale;
		x0 = float(parms.lclip);
	}
	if (x1 > parms.rclip)
	{
		u1 -= float(x1 - parms.rclip) * uscale / xscale;
		x1 = float(parms.rclip);
	}

	x0 -= 0.5f;
	y0 -= 0.5f;
	x1 -= 0.5f;
	y1 -= 0.5f;

	FBVERTEX verts[4] =
	{
		{ x0, y0, 0.5f, 1.f, u0, v0 },
		{ x1, y0, 0.5f, 1.f, u1, v0 },
		{ x1, y1, 0.5f, 1.f, u1, v1 },
		{ x0, y1, 0.5f, 1.f, u0, v1 }
	};

	if (!SetStyle(tex, parms))
	{
		return;
	}

	SetTexture(0, tex->Tex);
	D3DDevice->DrawPrimitiveUP(D3DPT_TRIANGLEFAN, 2, &verts, sizeof(FBVERTEX));
}

//==========================================================================
//
// D3DFB :: SetStyle
//
// Patterned after R_SetPatchStyle.
//
//==========================================================================

bool D3DFB::SetStyle(D3DTex *tex, DrawParms &parms)
{
	D3DFORMAT fmt = tex->GetTexFormat();
	ERenderStyle style = parms.style;
	D3DBLEND fglevel, bglevel;
	float alpha;
	bool stencilling;

	alpha = clamp<fixed_t> (parms.alpha, 0, FRACUNIT) / 65536.f;

	if (style == STYLE_OptFuzzy)
	{
		style = STYLE_Translucent;
	}
	else if (style == STYLE_SoulTrans)
	{
		style = STYLE_Translucent;
		alpha = transsouls;
	}

	// FIXME: STYLE_Fuzzy is not written
	if (style == STYLE_Fuzzy)
	{
		style = STYLE_Translucent;
		alpha = transsouls;
	}

	stencilling = false;

	switch (style)
	{
		// Special modes
	case STYLE_Shaded:
		if (alpha > 0)
		{
			SetConstant(1, RPART(parms.fillcolor)/255.f, GPART(parms.fillcolor)/255.f, BPART(parms.fillcolor)/255.f, alpha);
			SetPaletteTexture(ShadedPaletteTexture, 256);
			SetAlphaBlend(TRUE, D3DBLEND_SRCALPHA, D3DBLEND_INVSRCALPHA);
			return true;
		}
		return false;

		// Standard modes
	case STYLE_Stencil:
		stencilling = true;
	case STYLE_Normal:
		fglevel = D3DBLEND_SRCALPHA;
		bglevel = D3DBLEND_INVSRCALPHA;
		alpha = 1;
		break;

	case STYLE_TranslucentStencil:
		stencilling = true;
	case STYLE_Translucent:
		fglevel = D3DBLEND_SRCALPHA;
		bglevel = D3DBLEND_INVSRCALPHA;
		if (alpha == 0)
		{
			return false;
		}
		if (alpha == 1 && style == STYLE_Translucent)
		{
			style = STYLE_Normal;
		}
		break;

	case STYLE_Add:
		fglevel = D3DBLEND_SRCALPHA;
		bglevel = D3DBLEND_ONE;
		break;

	default:
		return false;
	}

	// Masking can only be turned off for STYLE_Normal, because it requires
	// turning off the alpha blend.
	if (!parms.masked && style == STYLE_Normal)
	{
		SetAlphaBlend(FALSE);
		SetColorOverlay(parms.colorOverlay, 1);
		if (fmt == D3DFMT_L8 && !tex->IsGray)
		{
			SetPaletteTexture(PaletteTexture, 256);
			SetPixelShader(PalTexShader);
		}
		else
		{
			SetPixelShader(PlainShader);
		}
	}
	else
	{
		SetAlphaBlend(TRUE, fglevel, bglevel);

		if (!stencilling)
		{
			if (fmt == D3DFMT_L8)
			{
				if (parms.remap != NULL)
				{
					D3DPal *pal = reinterpret_cast<D3DPal *>(parms.remap->GetNative());
					SetPaletteTexture(pal->Tex, pal->RoundedPaletteSize);
				}
				else if (tex->IsGray)
				{
					SetPixelShader(PlainShader);
				}
				else
				{
					SetPaletteTexture(PaletteTexture, 256);
				}
				SetPixelShader(PalTexShader);
			}
			else
			{
				SetPixelShader(PlainShader);
			}
			SetColorOverlay(parms.colorOverlay, alpha);
		}
		else
		{
			SetConstant(1,
				RPART(parms.fillcolor)/255.f,
				GPART(parms.fillcolor)/255.f,
				BPART(parms.fillcolor)/255.f, alpha);
			if (fmt == D3DFMT_L8)
			{
				SetPaletteTexture(StencilPaletteTexture, 256);
				SetPixelShader(PalTexShader);
			}
			else
			{
				SetPixelShader(PlainStencilShader);
			}
		}
	}
	return true;
}

void D3DFB::SetColorOverlay(DWORD color, float alpha)
{
	if (APART(color) != 0)
	{
		float a = 255.f / APART(color);
		float r = RPART(color) * a;
		float g = GPART(color) * a;
		float b = BPART(color) * a;
		SetConstant(0, r, g, b, 0);
		a = 1 - 1 / a;
		SetConstant(1, a, a, a, alpha);
	}
	else
	{
		SetConstant(0, 0, 0, 0, 0);
		SetConstant(1, 1, 1, 1, alpha);
	}
}

void D3DFB::SetAlphaBlend(BOOL enabled, D3DBLEND srcblend, D3DBLEND destblend)
{
	if (!enabled)
	{ // Disable alpha blend
		if (AlphaBlendEnabled)
		{
			AlphaBlendEnabled = FALSE;
			D3DDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
		}
	}
	else
	{ // Enable alpha blend
		assert(srcblend != 0);
		assert(destblend != 0);

		if (!AlphaBlendEnabled)
		{
			AlphaBlendEnabled = TRUE;
			D3DDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
		}
		if (AlphaSrcBlend != srcblend)
		{
			AlphaSrcBlend = srcblend;
			D3DDevice->SetRenderState(D3DRS_SRCBLEND, srcblend);
		}
		if (AlphaDestBlend != destblend)
		{
			AlphaDestBlend = destblend;
			D3DDevice->SetRenderState(D3DRS_DESTBLEND, destblend);
		}
	}
}

void D3DFB::SetConstant(int cnum, float r, float g, float b, float a)
{
	if (Constant[cnum][0] != r ||
		Constant[cnum][1] != g ||
		Constant[cnum][2] != b ||
		Constant[cnum][3] != a)
	{
		Constant[cnum][0] = r;
		Constant[cnum][1] = g;
		Constant[cnum][2] = b;
		Constant[cnum][3] = a;
		D3DDevice->SetPixelShaderConstantF(cnum, Constant[cnum], 1);
	}
}

void D3DFB::SetPixelShader(IDirect3DPixelShader9 *shader)
{
	if (CurPixelShader != shader)
	{
		CurPixelShader = shader;
		D3DDevice->SetPixelShader(shader);
	}
}

void D3DFB::SetTexture(int tnum, IDirect3DTexture9 *texture)
{
	if (Texture[tnum] != texture)
	{
		Texture[tnum] = texture;
		D3DDevice->SetTexture(tnum, texture);
	}
}

void D3DFB::SetPaletteTexture(IDirect3DTexture9 *texture, int count)
{
	if (count == 256 || SM14)
	{
		// Shader Model 1.4 only uses 256-color palettes.
		SetConstant(2, 255 / 256.f, 0.5f / 256.f, 0, 0);
	}
	else
	{
		// The pixel shader receives color indexes in the range [0.0,1.0].
		// The palette texture is also addressed in the range [0.0,1.0],
		// HOWEVER the coordinate 1.0 is the right edge of the texture and
		// not actually the texture itself. We need to scale and shift
		// the palette indexes so they lie exactly in the center of each
		// texel. For a normal palette with 256 entries, that means the
		// range we use should be [0.5,255.5], adjusted so the coordinate
		// is still with [0.0,1.0].
		//
		// The constant register c2 is used to hold the multiplier in the
		// x part and the adder in the y part.
		float fcount = 1 / float(count);
		SetConstant(2, 255 * fcount, 0.5f * fcount, 0, 0);
	}
	SetTexture(1, texture);
	SetPixelShader(PalTexShader);
}
