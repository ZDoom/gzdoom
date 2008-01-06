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
#include "f_wipe.h"
#include "st_stuff.h"
#include "win32iface.h"

#include <mmsystem.h>

// MACROS ------------------------------------------------------------------

// The number of vertices the vertex buffer should hold.
#define NUM_VERTS	12

// The number of line endpoints for the line vertex buffer.
#define NUM_LINE_VERTS	10240

// The number of quads we can batch together.
#define MAX_QUAD_BATCH	1024

// TYPES -------------------------------------------------------------------

IMPLEMENT_CLASS(D3DFB)

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

// Flags for a buffered quad
enum
{
	BQF_GamePalette		= 1,
	BQF_ShadedPalette	= 2,
	BQF_CustomPalette	= 3,
	BQF_StencilPalette	= 4,
		BQF_Paletted	= 7,
	BQF_Bilinear		= 8,
};

// Shaders for a buffered quad
enum
{
	BQS_PalTex,
	BQS_Plain,
	BQS_PlainStencil,
	BQS_ColorOnly
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
EXTERN_CVAR (Bool, vid_vsync)
EXTERN_CVAR (Float, transsouls)

extern IDirect3D9 *D3D;

extern cycle_t BlitCycles;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

#include "fb_d3d9_shaders.h"

// PUBLIC DATA DEFINITIONS -------------------------------------------------

CUSTOM_CVAR(Bool, test2d, true, CVAR_NOINITCALL)
{
	BorderNeedRefresh = SB_state = screen->GetPageCount();
}

// CODE --------------------------------------------------------------------

D3DFB::D3DFB (int width, int height, bool fullscreen)
	: BaseWinFB (width, height)
{
	D3DPRESENT_PARAMETERS d3dpp;

	D3DDevice = NULL;
	LineBuffer = NULL;
	QuadBuffer = NULL;
	FBTexture = NULL;
	TempRenderTexture = NULL;
	InitialWipeScreen = NULL;
	FinalWipeScreen = NULL;
	PaletteTexture = NULL;
	StencilPaletteTexture = NULL;
	ShadedPaletteTexture = NULL;
	PalTexShader = NULL;
	PalTexBilinearShader = NULL;
	PlainShader = NULL;
	PlainStencilShader = NULL;
	ColorOnlyShader = NULL;
	GammaFixerShader = NULL;
	BurnShader = NULL;
	FBFormat = D3DFMT_UNKNOWN;
	PalFormat = D3DFMT_UNKNOWN;
	VSync = vid_vsync;
	BlendingRect.left = 0;
	BlendingRect.top = 0;
	BlendingRect.right = FBWidth;
	BlendingRect.bottom = FBHeight;
	In2D = 0;
	Palettes = NULL;
	Textures = NULL;
	Accel2D = true;
	GatheringWipeScreen = false;
	ScreenWipe = NULL;
	InScene = false;
	QuadExtra = new BufferedQuad[MAX_QUAD_BATCH];

	Gamma = 1.0;
	FlashColor0 = 0;
	FlashColor1 = 0xFFFFFFFF;
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
	// Offset from top of screen to top of letterboxed screen
	LBOffsetI = (TrueHeight - Height) / 2;
	LBOffset = float(LBOffsetI);

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
		SetInitialState ();
	}
}

D3DFB::~D3DFB ()
{
	ReleaseResources ();
	if (D3DDevice != NULL)
	{
		D3DDevice->Release();
	}
	delete[] QuadExtra;
}

// Called after initial device creation and reset, when everything is set
// to D3D's defaults.
void D3DFB::SetInitialState()
{
	D3DDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
	AlphaBlendEnabled = FALSE;
	AlphaSrcBlend = D3DBLEND(0);
	AlphaDestBlend = D3DBLEND(0);

	CurPixelShader = NULL;
	memset(Constant, 0, sizeof(Constant));

	Texture[0] = NULL;
	Texture[1] = NULL;
	D3DDevice->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
	D3DDevice->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
	D3DDevice->SetSamplerState(1, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
	D3DDevice->SetSamplerState(1, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);

	D3DDevice->SetRenderState(D3DRS_ANTIALIASEDLINEENABLE, TRUE);

	SetGamma (Gamma);
	OldRenderTarget = NULL;
}

void D3DFB::FillPresentParameters (D3DPRESENT_PARAMETERS *pp, bool fullscreen, bool vsync)
{
	memset (pp, 0, sizeof(*pp));
	pp->Windowed = !fullscreen;
	pp->SwapEffect = D3DSWAPEFFECT_DISCARD;
	pp->BackBufferWidth = Width;
	pp->BackBufferHeight = TrueHeight;
	pp->BackBufferFormat = fullscreen ? D3DFMT_A8R8G8B8 : D3DFMT_UNKNOWN;
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
		FAILED(D3DDevice->CreatePixelShader (ColorOnlyDef, &ColorOnlyShader)))
	{
		return false;
	}
	if (FAILED(D3DDevice->CreatePixelShader (GammaFixerDef, &GammaFixerShader)))
	{
		Printf ("Windowed mode gamma will not work.\n");
		GammaFixerShader = NULL;
	}
	if (FAILED(D3DDevice->CreatePixelShader(PalTexBilinearDef, &PalTexBilinearShader)))
	{
		PalTexBilinearShader = PalTexShader;
	}
	if (FAILED(D3DDevice->CreatePixelShader (BurnShaderDef, &BurnShader)))
	{
		Printf ("Burn screenwipe will not work in D3D mode.\n");
		BurnShader = NULL;
	}
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
	return true;
}

void D3DFB::ReleaseResources ()
{
	I_SaveWindowedPos ();
	KillNativeTexs();
	KillNativePals();
	ReleaseDefaultPoolItems();
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
	if (PalTexBilinearShader != NULL)
	{
		if (PalTexBilinearShader != PalTexShader)
		{
			PalTexBilinearShader->Release();
		}
		PalTexBilinearShader = NULL;
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
	if (ColorOnlyShader != NULL)
	{
		ColorOnlyShader->Release();
		ColorOnlyShader = NULL;
	}
	if (GammaFixerShader != NULL)
	{
		GammaFixerShader->Release();
		GammaFixerShader = NULL;
	}
	if (BurnShader != NULL)
	{
		BurnShader->Release();
		BurnShader = NULL;
	}
	if (ScreenWipe != NULL)
	{
		delete ScreenWipe;
		ScreenWipe = NULL;
	}
	GatheringWipeScreen = false;
}

// Free resources created with D3DPOOL_DEFAULT.
void D3DFB::ReleaseDefaultPoolItems()
{
	if (FBTexture != NULL)
	{
		FBTexture->Release();
		FBTexture = NULL;
	}
	if (FinalWipeScreen != NULL)
	{
		if (FinalWipeScreen != TempRenderTexture)
		{
			FinalWipeScreen->Release();
		}
		FinalWipeScreen = NULL;
	}
	if (TempRenderTexture != NULL)
	{
		TempRenderTexture->Release();
		TempRenderTexture = NULL;
	}
	if (InitialWipeScreen != NULL)
	{
		InitialWipeScreen->Release();
		InitialWipeScreen = NULL;
	}
	if (QuadBuffer != NULL)
	{
		QuadBuffer->Release();
		QuadBuffer = NULL;
	}
	if (LineBuffer != NULL)
	{
		LineBuffer->Release();
		LineBuffer = NULL;
	}
}

bool D3DFB::Reset ()
{
	D3DPRESENT_PARAMETERS d3dpp;

	ReleaseDefaultPoolItems();
	FillPresentParameters (&d3dpp, !Windowed, VSync);
	if (!SUCCEEDED(D3DDevice->Reset (&d3dpp)))
	{
		return false;
	}
	if (!CreateFBTexture() || !CreateVertexes())
	{
		return false;
	}
	SetInitialState();
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
	if (FAILED(D3DDevice->CreateTexture (FBWidth, FBHeight, 1, D3DUSAGE_RENDERTARGET, D3DFMT_X8R8G8B8, D3DPOOL_DEFAULT, &TempRenderTexture, NULL)))
	{
		TempRenderTexture = NULL;
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
	if (FAILED(D3DDevice->CreateVertexBuffer(sizeof(FBVERTEX)*NUM_LINE_VERTS, 
		D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, D3DFVF_FBVERTEX, D3DPOOL_DEFAULT, &LineBuffer, NULL)))
	{
		return false;
	}
	LineBatchPos = -1;
	if (FAILED(D3DDevice->CreateVertexBuffer(sizeof(FBVERTEX)*MAX_QUAD_BATCH*6,
		D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, D3DFVF_FBVERTEX, D3DPOOL_DEFAULT, &QuadBuffer, NULL)))
	{
		return false;
	}
	QuadBatchPos = -1;
	return true;
}

void D3DFB::CalcFullscreenCoords (FBVERTEX verts[4], bool viewarea_only, D3DCOLOR color0, D3DCOLOR color1) const
{
	float offset = OldRenderTarget != NULL ? 0 : LBOffset;
	float top = offset - 0.5f;
	float texright = float(Width) / float(FBWidth);
	float texbot = float(Height) / float(FBHeight);
	float mxl, mxr, myt, myb, tmxl, tmxr, tmyt, tmyb;

	if (viewarea_only)
	{ // Just calculate vertices for the viewarea/BlendingRect
		mxl = float(BlendingRect.left) - 0.5f;
		mxr = float(BlendingRect.right) - 0.5f;
		myt = float(BlendingRect.top) + top;
		myb = float(BlendingRect.bottom) + top;
		tmxl = float(BlendingRect.left) / float(Width) * texright;
		tmxr = float(BlendingRect.right) / float(Width) * texright;
		tmyt = float(BlendingRect.top) / float(Height) * texbot;
		tmyb = float(BlendingRect.bottom) / float(Height) * texbot;
	}
	else
	{ // Calculate vertices for the whole screen
		mxl = -0.5f;
		mxr = float(Width) - 0.5f;
		myt = top;
		myb = float(Height) + top;
		tmxl = 0;
		tmxr = texright;
		tmyt = 0;
		tmyb = texbot;
	}

	//{   mxl, myt, 0, 1, 0, 0xFFFFFFFF,    tmxl,    tmyt },
	//{   mxr, myt, 0, 1, 0, 0xFFFFFFFF,    tmxr,    tmyt },
	//{   mxr, myb, 0, 1, 0, 0xFFFFFFFF,    tmxr,    tmyb },
	//{   mxl, myb, 0, 1, 0, 0xFFFFFFFF,    tmxl,    tmyb },

	verts[0].x = mxl;
	verts[0].y = myt;
	verts[0].z = 0;
	verts[0].rhw = 1;
	verts[0].color0 = color0;
	verts[0].color1 = color1;
	verts[0].tu = tmxl;
	verts[0].tv = tmyt;

	verts[1].x = mxr;
	verts[1].y = myt;
	verts[1].z = 0;
	verts[1].rhw = 1;
	verts[1].color0 = color0;
	verts[1].color1 = color1;
	verts[1].tu = tmxr;
	verts[1].tv = tmyt;

	verts[2].x = mxr;
	verts[2].y = myb;
	verts[2].z = 0;
	verts[2].rhw = 1;
	verts[2].color0 = color0;
	verts[2].color1 = color1;
	verts[2].tu = tmxr;
	verts[2].tv = tmyb;

	verts[3].x = mxl;
	verts[3].y = myb;
	verts[3].z = 0;
	verts[3].rhw = 1;
	verts[3].color0 = color0;
	verts[3].color1 = color1;
	verts[3].tu = tmxl;
	verts[3].tv = tmyb;
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
// When In2D == 2: Set up for 2D drawing but do not draw anything
// When In2D == 3: Present and set In2D to 0
void D3DFB::Update ()
{
	if (In2D == 3)
	{
		if (InScene)
		{
			DrawRateStuff();
			EndQuadBatch();		// Make sure all quads are drawn
			DoWindowedGamma();
			D3DDevice->EndScene();
			D3DDevice->Present(NULL, NULL, NULL, NULL);
			InScene = false;
		}
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
		D3DDevice->SetPixelShaderConstantF(7, psgamma, 1);
		NeedPalUpdate = true;
	}
	
	if (NeedPalUpdate)
	{
		UploadPalette();
	}

	BlitCycles = 0;
	clock (BlitCycles);

	LockCount = 0;
	HRESULT hr = D3DDevice->TestCooperativeLevel();
	if (FAILED(hr) && (hr != D3DERR_DEVICENOTRESET || !Reset()))
	{
		Sleep(1);
		return;
	}
	Draw3DPart(In2D <= 1);
	if (In2D == 0)
	{
		DoWindowedGamma();
		D3DDevice->EndScene();
		D3DDevice->Present(NULL, NULL, NULL, NULL);
		InScene = false;
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
	Draw3DPart(true);
	return true;
}

void D3DFB::Draw3DPart(bool copy3d)
{
	RECT texrect = { 0, 0, Width, Height };
	D3DLOCKED_RECT lockrect;

	if (copy3d)
	{
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
	}
	DrawLetterbox();
	InScene = true;
	D3DDevice->BeginScene();
	assert(OldRenderTarget == NULL);
	if (TempRenderTexture != NULL &&
		((Windowed && GammaFixerShader && TempRenderTexture != FinalWipeScreen) || GatheringWipeScreen))
	{
		IDirect3DSurface9 *targetsurf;
		if (SUCCEEDED(TempRenderTexture->GetSurfaceLevel(0, &targetsurf)))
		{
			if (SUCCEEDED(D3DDevice->GetRenderTarget(0, &OldRenderTarget)))
			{
				if (FAILED(D3DDevice->SetRenderTarget(0, targetsurf)))
				{
					// Setting the render target failed.
				}
			}
			targetsurf->Release();
		}
	}

	SetTexture (0, FBTexture);
	SetPaletteTexture(PaletteTexture, 256);
	SetPixelShader(PalTexShader);
	D3DDevice->SetFVF (D3DFVF_FBVERTEX);
	memset(Constant, 0, sizeof(Constant));
	SetAlphaBlend(FALSE);
	if (copy3d)
	{
		FBVERTEX verts[4];
		CalcFullscreenCoords(verts, test2d, FlashColor0, FlashColor1);
		D3DDevice->DrawPrimitiveUP(D3DPT_TRIANGLEFAN, 2, verts, sizeof(FBVERTEX));
	}
}

//==========================================================================
//
// D3DFB :: DrawLetterbox
//
// Draws the black bars at the top and bottom of the screen for letterboxed
// modes.
//
//==========================================================================

void D3DFB::DrawLetterbox()
{
	if (LBOffsetI != 0)
	{
		D3DRECT rects[2] = { { 0, 0, Width, LBOffsetI }, { 0, Height + LBOffsetI, Width, TrueHeight } };
		D3DDevice->Clear (2, rects, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0,0,0), 1.f, 0);
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
		FBVERTEX verts[4];

		CalcFullscreenCoords(verts, false, 0, 0xFFFFFFFF);
		D3DDevice->SetRenderTarget(0, OldRenderTarget);
		D3DDevice->SetFVF(D3DFVF_FBVERTEX);
		SetTexture(0, TempRenderTexture);
		SetPixelShader((Windowed && GammaFixerShader != NULL) ? GammaFixerShader : PlainShader);
		SetAlphaBlend(FALSE);
		D3DDevice->DrawPrimitiveUP(D3DPT_TRIANGLEFAN, 2, verts, sizeof(FBVERTEX));
		OldRenderTarget->Release();
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
	FlashColor0 = D3DCOLOR_COLORVALUE(r * a, g * a, b * a, 0);
	a = 1 - a;
	FlashColor1 = D3DCOLOR_COLORVALUE(a, a, a, 1);
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
	BlendingRect.left = x1;
	BlendingRect.top = y1;
	BlendingRect.right = x2;
	BlendingRect.bottom = y2;
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

#if 1
	hr = D3DDevice->CreateTexture(w, h, 1, 0,
		GetTexFormat(), D3DPOOL_MANAGED, &Tex, NULL);
#else
	hr = E_FAIL;
#endif
	if (SUCCEEDED(hr))
	{
		TX = 1;
		TY = 1;
	}
	else
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
		TX = GameTex->GetWidth() / float(w);
		TY = GameTex->GetHeight() / float(h);
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

bool D3DFB::Begin2D(bool copy3d)
{
	if (!test2d)
	{
		return false;
	}
	if (In2D)
	{
		return true;
	}
	In2D = 2 - copy3d;
	Update();
	In2D = 3;

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
	if (!InScene)
	{
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
	AddColorOnlyQuad(left, top, right - left, bottom - top, color | 0xFF000000);
}

//==========================================================================
//
// D3DFB :: Dim
//
//==========================================================================

void D3DFB::Dim (PalEntry color, float amount, int x1, int y1, int w, int h)
{
	if (amount <= 0)
	{
		return;
	}
	if (In2D < 2)
	{
		Super::Dim(color, amount, x1, y1, w, h);
		return;
	}
	if (!InScene)
	{
		return;
	}
	if (amount > 1)
	{
		amount = 1;
	}
	AddColorOnlyQuad(x1, y1, w, h, color | (int(amount * 255) << 24));
}

//==========================================================================
//
// D3DFB :: BeginLineDrawing
//
//==========================================================================

void D3DFB::BeginLineDrawing()
{
	if (In2D < 2 || !InScene || LineBatchPos >= 0)
	{
		return;
	}
	EndQuadBatch();		// Make sure all quads have been drawn first
	LineBuffer->Lock(0, 0, (void **)&LineData, D3DLOCK_DISCARD);
	LineBatchPos = 0;
}

//==========================================================================
//
// D3DFB :: EndLineDrawing
//
//==========================================================================

void D3DFB::EndLineDrawing()
{
	if (In2D < 2 || !InScene || LineBatchPos < 0)
	{
		return;
	}
	LineBuffer->Unlock();
	if (LineBatchPos > 0)
	{
		SetPixelShader(ColorOnlyShader);
		SetAlphaBlend(TRUE, D3DBLEND_SRCALPHA, D3DBLEND_INVSRCALPHA);
		D3DDevice->SetStreamSource(0, LineBuffer, 0, sizeof(FBVERTEX));
		D3DDevice->DrawPrimitive(D3DPT_LINELIST, 0, LineBatchPos / 2);
	}
	LineBatchPos = -1;
}

//==========================================================================
//
// D3DFB :: DrawLine
//
//==========================================================================

void D3DFB::DrawLine(int x0, int y0, int x1, int y1, int palcolor, uint32 color)
{
	if (In2D < 2)
	{
		Super::DrawLine(x0, y0, x1, y1, palcolor, color);
		return;
	}
	if (!InScene)
	{
		return;
	}
	if (LineBatchPos == NUM_LINE_VERTS)
	{ // flush the buffer and refill it
		EndLineDrawing();
		BeginLineDrawing();
	}
	if (LineBatchPos >= 0)
	{ // Batched drawing: Add the endpoints to the vertex buffer.
		LineData[LineBatchPos].x = float(x0);
		LineData[LineBatchPos].y = float(y0) + LBOffset;
		LineData[LineBatchPos].z = 0;
		LineData[LineBatchPos].rhw = 1;
		LineData[LineBatchPos].color0 = color;
		LineData[LineBatchPos].color1 = 0;
		LineData[LineBatchPos].tu = 0;
		LineData[LineBatchPos].tv = 0;
		LineData[LineBatchPos+1].x = float(x1);
		LineData[LineBatchPos+1].y = float(y1) + LBOffset;
		LineData[LineBatchPos+1].z = 0;
		LineData[LineBatchPos+1].rhw = 1;
		LineData[LineBatchPos+1].color0 = color;
		LineData[LineBatchPos+1].color1 = 0;
		LineData[LineBatchPos+1].tu = 0;
		LineData[LineBatchPos+1].tv = 0;
		LineBatchPos += 2;
	}
	else
	{ // Unbatched drawing: Draw it right now.
		FBVERTEX endpts[2] =
		{
			{ float(x0), float(y0), 0, 1, color },
			{ float(x1), float(y1), 0, 1, color }
		};
		SetPixelShader(ColorOnlyShader);
		SetAlphaBlend(TRUE, D3DBLEND_SRCALPHA, D3DBLEND_INVSRCALPHA);
		D3DDevice->DrawPrimitiveUP(D3DPT_LINELIST, 1, endpts, sizeof(FBVERTEX));
	}
}

//==========================================================================
//
// D3DFB :: DrawPixel
//
//==========================================================================

void D3DFB::DrawPixel(int x, int y, int palcolor, uint32 color)
{
	if (In2D < 2)
	{
		Super::DrawPixel(x, y, palcolor, color);
		return;
	}
	if (!InScene)
	{
		return;
	}
	FBVERTEX pt =
	{
		float(x), float(y), 0, 1, color
	};
	SetPixelShader(ColorOnlyShader);
	SetAlphaBlend(TRUE, D3DBLEND_SRCALPHA, D3DBLEND_INVSRCALPHA);
	D3DDevice->DrawPrimitiveUP(D3DPT_POINTLIST, 1, &pt, sizeof(FBVERTEX));
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

	if (!InScene || !ParseDrawTextureTags(img, x, y, tags_first, tags, &parms, true))
	{
		return;
	}

	D3DTex *tex = static_cast<D3DTex *>(img->GetNative());

	if (tex == NULL)
	{
		assert(tex != NULL);
		return;
	}

	CheckQuadBatch();

	float xscale = float(parms.destwidth) / parms.texwidth / 65536.f;
	float yscale = float(parms.destheight) / parms.texheight / 65536.f;
	float x0 = float(parms.x) / 65536.f - float(parms.left) * xscale;
	float y0 = float(parms.y) / 65536.f - float(parms.top) * yscale;
	float x1 = x0 + float(parms.destwidth) / 65536.f; 
	float y1 = y0 + float(parms.destheight) / 65536.f;
	float u0 = 0.f;
	float v0 = 0.f;
	float u1 = tex->TX;
	float v1 = tex->TY;
	float uscale = 1.f / (parms.texwidth / u1);
	float vscale = 1.f / (parms.texheight / v1) / yscale;

	if (y0 < parms.uclip)
	{
		v0 += (float(parms.uclip) - y0) * vscale;
		y0 = float(parms.uclip);
	}
	if (y1 > parms.dclip)
	{
		v1 -= (y1 - float(parms.dclip)) * vscale;
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
	parms.bilinear = false;

	D3DCOLOR color0, color1;
	if (!SetStyle(tex, parms, color0, color1, QuadExtra[QuadBatchPos]))
	{
		return;
	}

	QuadExtra[QuadBatchPos].Texture = tex;
	if (parms.bilinear)
	{
		QuadExtra[QuadBatchPos].Flags |= BQF_Bilinear;
	}

	float yoffs = GatheringWipeScreen ? 0.5f : 0.5f - LBOffset;
	x0 -= 0.5f;
	y0 -= yoffs;
	x1 -= 0.5f;
	y1 -= yoffs;

	FBVERTEX *vert = &QuadData[QuadBatchPos * 6];

	vert[3].x = vert[0].x = x0;
	vert[3].y = vert[0].y = y0;
	vert[3].z = vert[0].z = 0;
	vert[3].rhw = vert[0].rhw = 1;
	vert[3].color0 = vert[0].color0 = color0;
	vert[3].color1 = vert[0].color1 = color1;
	vert[3].tu = vert[0].tu = u0;
	vert[3].tv = vert[0].tv = v0;

	vert[1].x = x1;
	vert[1].y = y0;
	vert[1].z = 0;
	vert[1].rhw = 1;
	vert[1].color0 = color0;
	vert[1].color1 = color1;
	vert[1].tu = u1;
	vert[1].tv = v0;

	vert[4].x = vert[2].x = x1;
	vert[4].y = vert[2].y = y1;
	vert[4].z = vert[2].z = 0;
	vert[4].rhw = vert[2].rhw = 1;
	vert[4].color0 = vert[2].color0 = color0;
	vert[4].color1 = vert[2].color1 = color1;
	vert[4].tu = vert[2].tu = u1;
	vert[4].tv = vert[2].tv = v1;

	vert[5].x = x0;
	vert[5].y = y1;
	vert[5].z = 0;
	vert[5].rhw = 1;
	vert[5].color0 = color0;
	vert[5].color1 = color1;
	vert[5].tu = u0;
	vert[5].tv = v1;

	QuadBatchPos++;
}

//==========================================================================
//
// D3DFB :: AddColorOnlyQuad
//
// Adds a single-color, untextured quad to the batch.
//
//==========================================================================

void D3DFB::AddColorOnlyQuad(int left, int top, int width, int height, D3DCOLOR color)
{
	BufferedQuad *quad;
	FBVERTEX *verts;

	CheckQuadBatch();
	quad = &QuadExtra[QuadBatchPos];
	verts = &QuadData[QuadBatchPos * 6];

	float x = float(left) - 0.5f;
	float y = float(top) - 0.5f + (GatheringWipeScreen ? 0 : LBOffset);

	quad->Flags = 0;
	quad->ShaderNum = BQS_ColorOnly;
	if ((color & 0xFF000000) == 0xFF000000)
	{
		quad->SrcBlend = 0;
		quad->DestBlend = 0;
	}
	else
	{
		quad->SrcBlend = D3DBLEND_SRCALPHA;
		quad->DestBlend = D3DBLEND_INVSRCALPHA;
	}
	quad->Palette = NULL;
	quad->Texture = NULL;

	verts[3].x = verts[0].x = x;
	verts[3].y = verts[0].y = y;
	verts[3].z = verts[0].z = 0;
	verts[3].rhw = verts[0].rhw = 1;
	verts[3].color0 = verts[0].color0 = color;
	verts[3].color1 = verts[0].color1 = 0;
	verts[3].tu = verts[0].tu = 0;
	verts[3].tv = verts[0].tv = 0;

	verts[1].x = x + width;
	verts[1].y = y;
	verts[1].z = 0;
	verts[1].rhw = 1;
	verts[1].color0 = color;
	verts[1].color1 = 0;
	verts[1].tu = 0;
	verts[1].tv = 0;

	verts[4].x = verts[2].x = x + width;
	verts[4].y = verts[2].y = y + height;
	verts[4].z = verts[2].z = 0;
	verts[4].rhw = verts[2].rhw = 1;
	verts[4].color0 = verts[2].color0 = color;
	verts[4].color1 = verts[2].color1 = 0;
	verts[4].tu = verts[2].tu = 0;
	verts[4].tv = verts[2].tv = 0;

	verts[5].x = x;
	verts[5].y = y + height;
	verts[5].z = 0;
	verts[5].rhw = 1;
	verts[5].color0 = color;
	verts[5].color1 = 0;
	verts[5].tu = 0;
	verts[5].tv = 0;

	QuadBatchPos++;
}

//==========================================================================
//
// D3DFB :: CheckQuadBatch
//
// Make sure there's enough room in the batch for one more quad.
//
//==========================================================================

void D3DFB::CheckQuadBatch()
{
	if (QuadBatchPos == MAX_QUAD_BATCH)
	{
		EndQuadBatch();
	}
	if (QuadBatchPos < 0)
	{
		BeginQuadBatch();
	}
}

//==========================================================================
//
// D3DFB :: BeginQuadBatch
//
// Locks the vertex buffer for quads and sets the cursor to 0.
//
//==========================================================================

void D3DFB::BeginQuadBatch()
{
	if (In2D < 2 || !InScene || QuadBatchPos >= 0)
	{
		return;
	}
	QuadBuffer->Lock(0, 0, (void **)&QuadData, D3DLOCK_DISCARD);
	QuadBatchPos = 0;
}

//==========================================================================
//
// D3DFB :: EndQuadBatch
//
// Draws all the quads that have been batched up.
//
//==========================================================================

void D3DFB::EndQuadBatch()
{
	if (In2D < 2 || !InScene || QuadBatchPos < 0)
	{
		return;
	}
	QuadBuffer->Unlock();
	if (QuadBatchPos == 0)
	{
		QuadBatchPos = -1;
		return;
	}
	D3DDevice->SetStreamSource(0, QuadBuffer, 0, sizeof(FBVERTEX));
	for (int i = 0; i < QuadBatchPos; )
	{
		const BufferedQuad *quad = &QuadExtra[i];
		int j;

		// Quads with matching parameters should be done with a single
		// DrawPrimitive call.
		for (j = i + 1; j < QuadBatchPos; ++j)
		{
			const BufferedQuad *q2 = &QuadExtra[j];
			if (quad->Texture != q2->Texture ||
				quad->Group1 != q2->Group1 ||
				quad->Palette != q2->Palette)
			{
				break;
			}
		}

		// Set the palette (if one)
		if ((quad->Flags & BQF_Paletted) == BQF_GamePalette)
		{
			SetPaletteTexture(PaletteTexture, 256);
		}
		else if ((quad->Flags & BQF_Paletted) == BQF_CustomPalette)
		{
			SetPaletteTexture(quad->Palette->Tex, quad->Palette->RoundedPaletteSize);
		}
		else if ((quad->Flags & BQF_Paletted) == BQF_ShadedPalette)
		{
			SetPaletteTexture(ShadedPaletteTexture, 256);
		}
		else if ((quad->Flags & BQF_Paletted) == BQF_StencilPalette)
		{
			SetPaletteTexture(StencilPaletteTexture, 256);
		}
		// Set paletted bilinear filtering (IF IT WORKED RIGHT!)
		if (quad->Flags & (BQF_Paletted | BQF_Bilinear))
		{
			SetPalTexBilinearConstants(quad->Texture);
		}

		// Set the alpha blending
		if (quad->SrcBlend != 0)
		{
			SetAlphaBlend(TRUE, D3DBLEND(quad->SrcBlend), D3DBLEND(quad->DestBlend));
		}
		else
		{
			SetAlphaBlend(FALSE);
		}

		// Set the pixel shader
		if (quad->ShaderNum == BQS_PalTex)
		{
			SetPixelShader(!(quad->Flags & BQF_Bilinear) ? PalTexShader : PalTexBilinearShader);
		}
		else if (quad->ShaderNum == BQS_Plain)
		{
			SetPixelShader(PlainShader);
		}
		else if (quad->ShaderNum == BQS_PlainStencil)
		{
			SetPixelShader(PlainStencilShader);
		}
		else if (quad->ShaderNum == BQS_ColorOnly)
		{
			SetPixelShader(ColorOnlyShader);
		}

		// Set the texture
		if (quad->Texture != NULL)
		{
			SetTexture(0, quad->Texture->Tex);
		}

		// Draw the quad
		D3DDevice->DrawPrimitive(D3DPT_TRIANGLELIST, i * 6, 2 * (j - i));
		i = j;
	}
	QuadBatchPos = -1;
}

//==========================================================================
//
// D3DFB :: SetStyle
//
// Patterned after R_SetPatchStyle.
//
//==========================================================================

bool D3DFB::SetStyle(D3DTex *tex, DrawParms &parms, D3DCOLOR &color0, D3DCOLOR &color1, BufferedQuad &quad)
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
	quad.Palette = NULL;

	switch (style)
	{
		// Special modes
	case STYLE_Shaded:
		if (alpha > 0)
		{
			color0 = 0;
			color1 = parms.fillcolor | (D3DCOLOR(alpha * 255) << 24);
			quad.Flags = BQF_ShadedPalette;
			quad.SrcBlend = D3DBLEND_SRCALPHA;
			quad.DestBlend = D3DBLEND_INVSRCALPHA;
			quad.ShaderNum = BQS_PalTex;
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
		quad.SrcBlend = 0;
		quad.DestBlend = 0;
		SetColorOverlay(parms.colorOverlay, 1, color0, color1);
		if (fmt == D3DFMT_L8 && !tex->IsGray)
		{
			quad.Flags = BQF_GamePalette;
			quad.ShaderNum = BQS_PalTex;
		}
		else
		{
			quad.Flags = 0;
			quad.ShaderNum = BQS_Plain;
		}
	}
	else
	{
		quad.SrcBlend = fglevel;
		quad.DestBlend = bglevel;

		if (!stencilling)
		{
			if (fmt == D3DFMT_L8)
			{
				if (parms.remap != NULL)
				{
					quad.Flags = BQF_CustomPalette;
					quad.Palette = reinterpret_cast<D3DPal *>(parms.remap->GetNative());
					quad.ShaderNum = BQS_PalTex;
				}
				else if (tex->IsGray)
				{
					quad.Flags = 0;
					quad.ShaderNum = BQS_Plain;
				}
				else
				{
					quad.Flags = BQF_GamePalette;
					quad.ShaderNum = BQS_PalTex;
				}
			}
			else
			{
				quad.Flags = 0;
				quad.ShaderNum = BQS_Plain;
			}
			SetColorOverlay(parms.colorOverlay, alpha, color0, color1);
		}
		else
		{
			color0 = 0;
			color1 = parms.fillcolor | (D3DCOLOR(alpha * 255) << 24);
			if (fmt == D3DFMT_L8)
			{
				quad.Flags = BQF_StencilPalette;
				quad.ShaderNum = BQS_PalTex;
			}
			else
			{
				quad.Flags = 0;
				quad.ShaderNum = BQS_PlainStencil;
			}
		}
	}
	return true;
}

void D3DFB::SetColorOverlay(DWORD color, float alpha, D3DCOLOR &color0, D3DCOLOR &color1)
{
	if (APART(color) != 0)
	{
		int a = APART(color) * 256 / 255;
		color0 = D3DCOLOR_RGBA(
			(RPART(color) * a) >> 8,
			(GPART(color) * a) >> 8,
			(BPART(color) * a) >> 8,
			0);
		a = 256 - a;
		color1 = D3DCOLOR_RGBA(a, a, a, int(alpha * 255));
	}
	else
	{
		color0 = 0;
		color1 = D3DCOLOR_COLORVALUE(1, 1, 1, alpha);
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
}

void D3DFB::SetPalTexBilinearConstants(D3DTex *tex)
{
	float con[8];

	// Don't bother doing anything if the constants won't be used.
	if (PalTexShader == PalTexBilinearShader)
	{
		return;
	}

	con[0] = tex->GameTex->GetWidth() / tex->TX;
	con[1] = tex->GameTex->GetHeight() / tex->TY;
	con[2] = 0;
	con[3] = 1 / con[0];
	con[4] = 0;
	con[5] = 1 / con[1];
	con[6] = con[5];
	con[7] = con[3];

	D3DDevice->SetPixelShaderConstantF(3, con, 2);
}
