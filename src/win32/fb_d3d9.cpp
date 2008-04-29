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
** This file does _not_ implement hardware-acclerated 3D rendering. It is
** just a means of getting the pixel data to the screen in a more reliable
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

// MACROS ------------------------------------------------------------------

// The number of points for the vertex buffer.
#define NUM_VERTS		10240

// The number of indices for the index buffer.
#define NUM_INDEXES		((NUM_VERTS * 6) / 4)

// The number of quads we can batch together.
#define MAX_QUAD_BATCH	(NUM_INDEXES / 6)

// TYPES -------------------------------------------------------------------

IMPLEMENT_CLASS(D3DFB)

struct D3DFB::PackedTexture
{
	D3DFB::PackingTexture *Owner;

	PackedTexture *Next, **Prev;

	// Pixels this image covers
	RECT Area;

	// Texture coordinates for this image
	float Left, Top, Right, Bottom;
};

struct D3DFB::PackingTexture
{
	PackingTexture(D3DFB *fb, int width, int height, D3DFORMAT format);
	~PackingTexture();

	PackedTexture *GetBestFit(int width, int height, int &area);
	void AllocateImage(PackedTexture *box, int width, int height);
	PackedTexture *AllocateBox();
	void AddEmptyBox(int left, int top, int right, int bottom);
	void FreeBox(PackedTexture *box);

	PackingTexture *Next;
	IDirect3DTexture9 *Tex;
	D3DFORMAT Format;
	PackedTexture *UsedList;	// Boxes that contain images
	PackedTexture *EmptyList;	// Boxes that contain empty space
	PackedTexture *FreeList;	// Boxes that are just waiting to be used
	int Width, Height;
	bool OneUse;
};

class D3DTex : public FNativeTexture
{
public:
	D3DTex(FTexture *tex, D3DFB *fb, bool wrapping);
	~D3DTex();

	FTexture *GameTex;
	D3DFB::PackedTexture *Box;

	D3DTex **Prev;
	D3DTex *Next;

	bool IsGray;

	bool Create(D3DFB *fb, bool wrapping);
	bool Update();
	bool CheckWrapping(bool wrapping);
	D3DFORMAT GetTexFormat();
	FTextureFormat ToTexFmt(D3DFORMAT fmt);
};

class D3DPal : public FNativePalette
{
public:
	D3DPal(FRemapTable *remap, D3DFB *fb);
	~D3DPal();

	D3DPal **Prev;
	D3DPal *Next;

	IDirect3DTexture9 *Tex;
	D3DCOLOR BorderColor;

	bool Update();

	FRemapTable *Remap;
	int RoundedPaletteSize;
};

// Flags for a buffered quad
enum
{
	BQF_GamePalette		= 1,
	BQF_CustomPalette	= 7,
		BQF_Paletted	= 7,
	BQF_Bilinear		= 8,
	BQF_WrapUV			= 16,
	BQF_InvertSource	= 32,
	BQF_DisableAlphaTest= 64,
};

// Shaders for a buffered quad
enum
{
	BQS_PalTex,
	BQS_Plain,
	BQS_RedToAlpha,
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
EXTERN_CVAR (Int, vid_refreshrate)

extern IDirect3D9 *D3D;

extern cycle_t BlitCycles;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

#include "fb_d3d9_shaders.h"

// PUBLIC DATA DEFINITIONS -------------------------------------------------

CUSTOM_CVAR(Bool, vid_hw2d, true, CVAR_NOINITCALL)
{
	BorderNeedRefresh = SB_state = screen->GetPageCount();
}

CVAR(Int, d3d_showpacks, 0, 0)

// CODE --------------------------------------------------------------------

D3DFB::D3DFB (int width, int height, bool fullscreen)
	: BaseWinFB (width, height)
{
	D3DPRESENT_PARAMETERS d3dpp;

	D3DDevice = NULL;
	VertexBuffer = NULL;
	IndexBuffer = NULL;
	FBTexture = NULL;
	TempRenderTexture = NULL;
	InitialWipeScreen = NULL;
	ScreenshotTexture = NULL;
	ScreenshotSurface = NULL;
	FinalWipeScreen = NULL;
	PaletteTexture = NULL;
	PalTexShader = NULL;
	InvPalTexShader = NULL;
	PalTexBilinearShader = NULL;
	PlainShader = NULL;
	InvPlainShader = NULL;
	RedToAlphaShader = NULL;
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
	Packs = NULL;
	PixelDoubling = 0;

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
				PixelDoubling = mode->doubling;
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
		D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_FPU_PRESERVE, &d3dpp, &D3DDevice)))
	{
		if (FAILED(D3D->CreateDevice (D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, Window,
			D3DCREATE_SOFTWARE_VERTEXPROCESSING | D3DCREATE_FPU_PRESERVE, &d3dpp, &D3DDevice)))
		{
			if (d3dpp.FullScreen_RefreshRateInHz != 0)
			{
				d3dpp.FullScreen_RefreshRateInHz = 0;
				if (FAILED(hr = D3D->CreateDevice (D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, Window,
					D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_FPU_PRESERVE, &d3dpp, &D3DDevice)))
				{
					if (FAILED(D3D->CreateDevice (D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, Window,
						D3DCREATE_SOFTWARE_VERTEXPROCESSING | D3DCREATE_FPU_PRESERVE, &d3dpp, &D3DDevice)))
					{
						D3DDevice = NULL;
					}
				}
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
	SAFE_RELEASE( D3DDevice );
	delete[] QuadExtra;
}

// Called after initial device creation and reset, when everything is set
// to D3D's defaults.
void D3DFB::SetInitialState()
{
	AlphaBlendEnabled = FALSE;
	AlphaBlendOp = D3DBLENDOP_ADD;
	AlphaSrcBlend = D3DBLEND(0);
	AlphaDestBlend = D3DBLEND(0);

	CurPixelShader = NULL;
	memset(Constant, 0, sizeof(Constant));

	Texture[0] = NULL;
	Texture[1] = NULL;
	D3DDevice->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
	D3DDevice->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
	D3DDevice->SetSamplerState(1, D3DSAMP_ADDRESSU, SM14 ? D3DTADDRESS_BORDER : D3DTADDRESS_CLAMP);
	D3DDevice->SetSamplerState(1, D3DSAMP_ADDRESSV, SM14 ? D3DTADDRESS_BORDER : D3DTADDRESS_CLAMP);

	D3DDevice->SetRenderState(D3DRS_ANTIALIASEDLINEENABLE, TRUE);

	NeedGammaUpdate = true;
	NeedPalUpdate = true;
	OldRenderTarget = NULL;

	if (!Windowed && SM14)
	{
		// Fix for Radeon 9000, possibly other R200s: When the device is
		// reset, it resets the gamma ramp, but the driver apparently keeps a
		// cached copy of the ramp that it doesn't update, so when
		// SetGammaRamp is called later to handle the NeedGammaUpdate flag,
		// it doesn't do anything, because the gamma ramp is the same as the
		// one passed in the last call, even though the visible gamma ramp 
		// actually has changed.
		//
		// So here we force the gamma ramp to something absolutely horrible and
		// trust that we will be able to properly set the gamma later when
		// NeedGammaUpdate is handled.
		D3DGAMMARAMP ramp;
		memset(&ramp, 0, sizeof(ramp));
		D3DDevice->SetGammaRamp(0, 0, &ramp);
	}

	// Used by the inverse color shaders
	float ones[4] = { 1, 1, 1, 1 };
	D3DDevice->SetPixelShaderConstantF(6, ones, 1);

	// D3DRS_ALPHATESTENABLE defaults to FALSE
	// D3DRS_ALPHAREF defaults to 0
	D3DDevice->SetRenderState(D3DRS_ALPHAFUNC, D3DCMP_NOTEQUAL);
	AlphaTestEnabled = FALSE;

	CurBorderColor = 0;
}

void D3DFB::FillPresentParameters (D3DPRESENT_PARAMETERS *pp, bool fullscreen, bool vsync)
{
	memset (pp, 0, sizeof(*pp));
	pp->Windowed = !fullscreen;
	pp->SwapEffect = D3DSWAPEFFECT_DISCARD;
	pp->BackBufferWidth = Width << PixelDoubling;
	pp->BackBufferHeight = TrueHeight << PixelDoubling;
	pp->BackBufferFormat = fullscreen ? D3DFMT_A8R8G8B8 : D3DFMT_UNKNOWN;
	pp->BackBufferCount = 1;
	pp->hDeviceWindow = Window;
	pp->PresentationInterval = vsync ? D3DPRESENT_INTERVAL_ONE : D3DPRESENT_INTERVAL_IMMEDIATE;
	if (fullscreen)
	{
		pp->FullScreen_RefreshRateInHz = vid_refreshrate;
	}
}

bool D3DFB::CreateResources ()
{
	Packs = NULL;
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
	if (FAILED(D3DDevice->CreatePixelShader (InvPalTexShader20Def, &InvPalTexShader)) &&
		(SM14 = true, FAILED(D3DDevice->CreatePixelShader (InvPalTexShader14Def, &InvPalTexShader))))
	{
		return false;
	}
	if (FAILED(D3DDevice->CreatePixelShader (PlainShaderDef, &PlainShader)) ||
		FAILED(D3DDevice->CreatePixelShader (InvPlainShaderDef, &InvPlainShader)) ||
		FAILED(D3DDevice->CreatePixelShader (RedToAlphaDef, &RedToAlphaShader)) ||
		FAILED(D3DDevice->CreatePixelShader (ColorOnlyDef, &ColorOnlyShader)))
	{
		return false;
	}
	if (FAILED(D3DDevice->CreatePixelShader (GammaFixerDef, &GammaFixerShader)))
	{
//		Printf ("Using Shader Model 1.4: Windowed mode gamma will not work.\n");
		GammaFixerShader = NULL;
	}
	if (FAILED(D3DDevice->CreatePixelShader(PalTexBilinearDef, &PalTexBilinearShader)))
	{
		PalTexBilinearShader = PalTexShader;
	}
	if (FAILED(D3DDevice->CreatePixelShader (BurnShaderDef, &BurnShader)))
	{
		BurnShader = NULL;
	}
	if (!CreateFBTexture() ||
		!CreatePaletteTexture())
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
	SAFE_RELEASE( ScreenshotSurface );
	SAFE_RELEASE( ScreenshotTexture );
	SAFE_RELEASE( PaletteTexture );
	if (PalTexBilinearShader != NULL)
	{
		if (PalTexBilinearShader != PalTexShader)
		{
			PalTexBilinearShader->Release();
		}
		PalTexBilinearShader = NULL;
	}
	SAFE_RELEASE( PalTexShader );
	SAFE_RELEASE( InvPalTexShader );
	SAFE_RELEASE( PlainShader );
	SAFE_RELEASE( InvPlainShader );
	SAFE_RELEASE( RedToAlphaShader );
	SAFE_RELEASE( ColorOnlyShader );
	SAFE_RELEASE( GammaFixerShader );
	SAFE_RELEASE( BurnShader );
	if (ScreenWipe != NULL)
	{
		delete ScreenWipe;
		ScreenWipe = NULL;
	}
	PackingTexture *pack, *next;
	for (pack = Packs; pack != NULL; pack = next)
	{
		next = pack->Next;
		delete pack;
	}
	GatheringWipeScreen = false;
}

// Free resources created with D3DPOOL_DEFAULT.
void D3DFB::ReleaseDefaultPoolItems()
{
	SAFE_RELEASE( FBTexture );
	if (FinalWipeScreen != NULL)
	{
		if (FinalWipeScreen != TempRenderTexture)
		{
			FinalWipeScreen->Release();
		}
		FinalWipeScreen = NULL;
	}
	SAFE_RELEASE( TempRenderTexture );
	SAFE_RELEASE( InitialWipeScreen );
	SAFE_RELEASE( VertexBuffer );
	SAFE_RELEASE( IndexBuffer );
}

bool D3DFB::Reset ()
{
	D3DPRESENT_PARAMETERS d3dpp;

	ReleaseDefaultPoolItems();
	FillPresentParameters (&d3dpp, !Windowed, VSync);
	if (!SUCCEEDED(D3DDevice->Reset (&d3dpp)))
	{
		if (d3dpp.FullScreen_RefreshRateInHz != 0)
		{
			d3dpp.FullScreen_RefreshRateInHz = 0;
			if (!SUCCEEDED(D3DDevice->Reset (&d3dpp)))
			{
				return false;
			}
		}
		else
		{
			return false;
		}
	}
	LOG("Device was reset\n");
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

bool D3DFB::CreateVertexes ()
{
	VertexPos = -1;
	IndexPos = -1;
	QuadBatchPos = -1;
	BatchType = BATCH_None;
	if (FAILED(D3DDevice->CreateVertexBuffer(sizeof(FBVERTEX)*NUM_VERTS, 
		D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, D3DFVF_FBVERTEX, D3DPOOL_DEFAULT, &VertexBuffer, NULL)))
	{
		return false;
	}
	if (FAILED(D3DDevice->CreateIndexBuffer(sizeof(WORD)*NUM_INDEXES,
		D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, D3DFMT_INDEX16, D3DPOOL_DEFAULT, &IndexBuffer, NULL)))
	{
		return false;
	}
	return true;
}

void D3DFB::CalcFullscreenCoords (FBVERTEX verts[4], bool viewarea_only, bool can_double, D3DCOLOR color0, D3DCOLOR color1) const
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
		mxr = float(Width << (can_double ? PixelDoubling : 0)) - 0.5f;
		myt = top;
		myb = float(Height << (can_double ? PixelDoubling : 0)) + top;
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
	assert (!In2D);
	Accel2D = vid_hw2d;
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
			DrawPackedTextures(d3d_showpacks);
			EndBatch();		// Make sure all batched primitives are drawn.
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
			LOG("SetGammaRamp\n");
			D3DDevice->SetGammaRamp(0, D3DSGR_CALIBRATE, &ramp);
		}
		psgamma[2] = psgamma[1] = psgamma[0] = igamma;
		psgamma[3] = 1;
		D3DDevice->SetPixelShaderConstantF(7, psgamma, 1);
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
	//LOG1 ("cycles = %d\n", BlitCycles);

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
	if (copy3d)
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
	}
	DrawLetterbox();
	InScene = true;
	D3DDevice->BeginScene();
	assert(OldRenderTarget == NULL);
	if (TempRenderTexture != NULL &&
		((Windowed && GammaFixerShader && TempRenderTexture != FinalWipeScreen) || GatheringWipeScreen || PixelDoubling))
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
	SetPaletteTexture(PaletteTexture, 256, BorderColor);
	SetPixelShader(PalTexShader);
	D3DDevice->SetFVF (D3DFVF_FBVERTEX);
	memset(Constant, 0, sizeof(Constant));
	SetAlphaBlend(D3DBLENDOP(0));
	EnableAlphaTest(FALSE);
	if (copy3d)
	{
		FBVERTEX verts[4];
		CalcFullscreenCoords(verts, Accel2D, false, FlashColor0, FlashColor1);
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

		CalcFullscreenCoords(verts, false, true, 0, 0xFFFFFFFF);
		D3DDevice->SetRenderTarget(0, OldRenderTarget);
		D3DDevice->SetFVF(D3DFVF_FBVERTEX);
		SetTexture(0, TempRenderTexture);
		SetPixelShader((Windowed && GammaFixerShader != NULL) ? GammaFixerShader : PlainShader);
		SetAlphaBlend(D3DBLENDOP(0));
		EnableAlphaTest(FALSE);
		D3DDevice->DrawPrimitiveUP(D3DPT_TRIANGLEFAN, 2, verts, sizeof(FBVERTEX));
		OldRenderTarget->Release();
		OldRenderTarget = NULL;
	}
}

void D3DFB::UploadPalette ()
{
	D3DLOCKED_RECT lockrect;

	if (SUCCEEDED(PaletteTexture->LockRect (0, &lockrect, NULL, 0)))
	{
		BYTE *pix = (BYTE *)lockrect.pBits;
		int i, skipat;

		// It is impossible to get the Radeon 9000 to do the proper palette
		// lookup. It *will* skip at least one entry in the palette. So we
		// let it and have it look at the texture border color for color 255.
		// I assume that every other card based on a related graphics chipset
		// is similarly affected, which basically means that all Shader Model
		// 1.4 cards suffer from this problem, since they all use some variant
		// of the ATI R200.
		skipat = SM14 ? 256 - 8 : 256;

		for (i = 0; i < skipat; ++i, pix += 4)
		{
			pix[0] = SourcePalette[i].b;
			pix[1] = SourcePalette[i].g;
			pix[2] = SourcePalette[i].r;
			pix[3] = (i == 0 ? 0 : 255);
			// To let masked textures work, the first palette entry's alpha is 0.
		}
		pix += 4;
		for (; i < 255; ++i, pix += 4)
		{
			pix[0] = SourcePalette[i].b;
			pix[1] = SourcePalette[i].g;
			pix[2] = SourcePalette[i].r;
			pix[3] = 255;
		}
		PaletteTexture->UnlockRect (0);
		BorderColor = D3DCOLOR_XRGB(SourcePalette[255].r, SourcePalette[255].g, SourcePalette[255].b);
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

void D3DFB::NewRefreshRate ()
{
	if (!Windowed)
	{
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

//==========================================================================
//
// D3DFB :: GetScreenshotBuffer
//
// Returns a pointer into a surface holding the current screen data.
//
//==========================================================================

void D3DFB::GetScreenshotBuffer(const BYTE *&buffer, int &pitch, ESSType &color_type)
{
	D3DLOCKED_RECT lrect;

	if (!Accel2D)
	{
		Super::GetScreenshotBuffer(buffer, pitch, color_type);
		return;
	}
	buffer = NULL;
	if ((ScreenshotTexture = GetCurrentScreen()) != NULL)
	{
		if (FAILED(ScreenshotTexture->GetSurfaceLevel(0, &ScreenshotSurface)))
		{
			ScreenshotTexture->Release();
			ScreenshotTexture = NULL;
		}
		else if (FAILED(ScreenshotSurface->LockRect(&lrect, NULL, D3DLOCK_READONLY | D3DLOCK_NOSYSLOCK)))
		{
			ScreenshotSurface->Release();
			ScreenshotSurface = NULL;
			ScreenshotTexture->Release();
			ScreenshotTexture = NULL;
		}
		else
		{
			buffer = (const BYTE *)lrect.pBits + lrect.Pitch * LBOffsetI;
			pitch = lrect.Pitch;
			color_type = SS_BGRA;
		}
	}
}

//==========================================================================
//
// D3DFB :: ReleaseScreenshotBuffer
//
//==========================================================================

void D3DFB::ReleaseScreenshotBuffer()
{
	if (LockCount > 0)
	{
		Super::ReleaseScreenshotBuffer();
	}
	if (ScreenshotSurface != NULL)
	{
		ScreenshotSurface->UnlockRect();
		ScreenshotSurface->Release();
		ScreenshotSurface = NULL;
	}
	SAFE_RELEASE( ScreenshotTexture );
}

//==========================================================================
//
// D3DFB :: GetCurrentScreen
//
// Returns a texture containing the pixels currently visible on-screen.
//
//==========================================================================

IDirect3DTexture9 *D3DFB::GetCurrentScreen()
{
	IDirect3DTexture9 *tex;
	IDirect3DSurface9 *tsurf, *surf;
	D3DSURFACE_DESC desc;

	if (Windowed || PixelDoubling)
	{
		// The texture we read into must have the same pixel format as
		// the TempRenderTexture.
		if (SUCCEEDED(TempRenderTexture->GetSurfaceLevel(0, &tsurf)))
		{
			if (FAILED(tsurf->GetDesc(&desc)))
			{
				tsurf->Release();
				return NULL;
			}
			tsurf->Release();
		}
		else
		{
			return NULL;
		}
	}
	else
	{
		if (SUCCEEDED(D3DDevice->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &tsurf)))
		{
			if (FAILED(tsurf->GetDesc(&desc)))
			{
				tsurf->Release();
				return NULL;
			}
			tsurf->Release();
		}
		else
		{
			return NULL;
		}
		// GetFrontBufferData works only with this format
		desc.Format = D3DFMT_A8R8G8B8;
	}

	if (FAILED(D3DDevice->CreateTexture(desc.Width, desc.Height, 1, 0,
		desc.Format, D3DPOOL_SYSTEMMEM, &tex, NULL)))
	{
		return NULL;
	}
	if (FAILED(tex->GetSurfaceLevel(0, &surf)))
	{
		tex->Release();
		return NULL;
	}

	if (!Windowed && !PixelDoubling)
	{
		if (FAILED(D3DDevice->GetFrontBufferData(0, surf)))
		{
			surf->Release();
			tex->Release();
			return NULL;
		}
	}
	else
	{
		if (SUCCEEDED(TempRenderTexture->GetSurfaceLevel(0, &tsurf)))
		{
			if (FAILED(D3DDevice->GetRenderTargetData(tsurf, surf)))
			{
				tsurf->Release();
				tex->Release();
				return NULL;
			}
			tsurf->Release();
		}
		else
		{
			tex->Release();
			return NULL;
		}
	}
	surf->Release();
	return tex;
}

/**************************************************************************/
/*                                  2D Stuff                              */
/**************************************************************************/

//==========================================================================
//
// D3DFB :: DrawPackedTextures
//
// DEBUG: Draws the packing textures to the screen, starting with the
// 1-based packnum.
//
//==========================================================================

void D3DFB::DrawPackedTextures(int packnum)
{
	D3DCOLOR empty_colors[8] =
	{
		0xFFFF9999, 0xFF99FF99, 0xFF9999FF, 0xFFFFFF99,
		0xFFFF99FF, 0xFF99FFFF, 0xFFFFCC99, 0xFF99CCFF
	};
	PackingTexture *pack;
	int x = 8, y = 8;

	if (packnum <= 0)
	{
		return;
	}
	pack = Packs;
	while (pack != NULL && pack->OneUse)
	{ // Skip textures that aren't used as packing containers
		pack = pack->Next;
	}
	while (pack != NULL && packnum != 1)
	{
		if (!pack->OneUse)
		{ // Skip textures that aren't used as packing containers
			packnum--;
		}
		pack = pack->Next;
	}
	while (pack != NULL)
	{
		if (pack->OneUse)
		{ // Skip textures that aren't used as packing containers
			pack = pack->Next;
			continue;
		}

		AddColorOnlyQuad(x-1, y-1-LBOffsetI, 258, 258, D3DCOLOR_XRGB(255,255,0));

		CheckQuadBatch();

		BufferedQuad *quad = &QuadExtra[QuadBatchPos];
		FBVERTEX *vert = &VertexData[VertexPos];

		quad->Group1 = 0;
		if (pack->Format == D3DFMT_L8/* && !tex->IsGray*/)
		{
			quad->Flags = BQF_WrapUV | BQF_GamePalette | BQF_DisableAlphaTest;
			quad->ShaderNum = BQS_PalTex;
		}
		else
		{
			quad->Flags = BQF_WrapUV | BQF_DisableAlphaTest;
			quad->ShaderNum = BQS_Plain;
		}
		quad->Palette = NULL;
		quad->Texture = pack;

		float x0 = float(x) - 0.5f;
		float y0 = float(y) - 0.5f;
		float x1 = x0 + 256.f;
		float y1 = y0 + 256.f;

		vert[0].x = x0;
		vert[0].y = y0;
		vert[0].z = 0;
		vert[0].rhw = 1;
		vert[0].color0 = 0;
		vert[0].color1 = 0xFFFFFFFF;
		vert[0].tu = 0;
		vert[0].tv = 0;

		vert[1].x = x1;
		vert[1].y = y0;
		vert[1].z = 0;
		vert[1].rhw = 1;
		vert[1].color0 = 0;
		vert[1].color1 = 0xFFFFFFFF;
		vert[1].tu = 1;
		vert[1].tv = 0;

		vert[2].x = x1;
		vert[2].y = y1;
		vert[2].z = 0;
		vert[2].rhw = 1;
		vert[2].color0 = 0;
		vert[2].color1 = 0xFFFFFFFF;
		vert[2].tu = 1;
		vert[2].tv = 1;

		vert[3].x = x0;
		vert[3].y = y1;
		vert[3].z = 0;
		vert[3].rhw = 1;
		vert[3].color0 = 0;
		vert[3].color1 = 0xFFFFFFFF;
		vert[3].tu = 0;
		vert[3].tv = 1;

		IndexData[IndexPos    ] = VertexPos;
		IndexData[IndexPos + 1] = VertexPos + 1;
		IndexData[IndexPos + 2] = VertexPos + 2;
		IndexData[IndexPos + 3] = VertexPos;
		IndexData[IndexPos + 4] = VertexPos + 2;
		IndexData[IndexPos + 5] = VertexPos + 3;

		QuadBatchPos++;
		VertexPos += 4;
		IndexPos += 6;

		// Draw entries in the empty list.
		PackedTexture *box;
		int emptynum;
		for (box = pack->EmptyList, emptynum = 0; box != NULL; box = box->Next, emptynum++)
		{
			AddColorOnlyQuad(x + box->Area.left, y + box->Area.top - LBOffsetI,
				box->Area.right - box->Area.left, box->Area.bottom - box->Area.top,
				empty_colors[emptynum & 7]);
		}

		x += 256 + 8;
		if (x > Width - 256)
		{
			x = 8;
			y += 256 + 8;
			if (y > TrueHeight - 256)
			{
				return;
			}
		}
		pack = pack->Next;
	}
}

//==========================================================================
//
// D3DFB :: AllocPackedTexture
//
// Finds space to pack an image inside a packing texture and returns it.
// Large images and those that need to wrap always get their own textures.
//
//==========================================================================

D3DFB::PackedTexture *D3DFB::AllocPackedTexture(int w, int h, bool wrapping, D3DFORMAT format)
{
	PackingTexture *bestpack;
	PackedTexture *bestbox;
	int area;

	if (w > 256 || h > 256 || wrapping)
	{ // Create a new packing texture.
		bestpack = new PackingTexture(this, w, h, format);
		bestpack->OneUse = true;
		bestbox = bestpack->GetBestFit(w, h, area);
	}
	else
	{ // Try to find space in an existing packing texture.
		int bestarea = INT_MAX;
		int bestareaever = w * h;
		bestpack = NULL;
		bestbox = NULL;
		for (PackingTexture *pack = Packs; pack != NULL; pack = pack->Next)
		{
			if (pack->Format == format)
			{
				PackedTexture *box = pack->GetBestFit(w, h, area);
				if (area == bestareaever)
				{ // An exact fit! Use it!
					bestpack = pack;
					bestbox = box;
					break;
				}
				if (area < bestarea)
				{
					bestarea = area;
					bestpack = pack;
					bestbox = box;
				}
			}
		}
		if (bestpack == NULL)
		{ // Create a new packing texture.
			bestpack = new PackingTexture(this, 256, 256, format);
			bestbox = bestpack->GetBestFit(w, h, bestarea);
		}
	}
	bestpack->AllocateImage(bestbox, w, h);
	return bestbox;
}

//==========================================================================
//
// PackingTexture Constructor
//
//==========================================================================

D3DFB::PackingTexture::PackingTexture(D3DFB *fb, int w, int h, D3DFORMAT format)
{
	Tex = NULL;
	Format = format;
	UsedList = NULL;
	EmptyList = NULL;
	FreeList = NULL;
	OneUse = false;
	Width = 0;
	Height = 0;

	Next = fb->Packs;
	fb->Packs = this;

#if 1
	if (FAILED(fb->D3DDevice->CreateTexture(w, h, 1, 0, format, D3DPOOL_MANAGED, &Tex, NULL)))
#endif
	{ // Try again, using power-of-2 sizes
		int i;

		for (i = 1; i < w; i <<= 1) {} w = i;
		for (i = 1; i < h; i <<= 1) {} h = i;
		if (FAILED(fb->D3DDevice->CreateTexture(w, h, 1, 0, format, D3DPOOL_MANAGED, &Tex, NULL)))
		{
			return;
		}
	}
	Width = w;
	Height = h;

	// The whole texture is initially empty.
	AddEmptyBox(0, 0, w, h);
}

//==========================================================================
//
// PackingTexture Destructor
//
//==========================================================================

D3DFB::PackingTexture::~PackingTexture()
{
	PackedTexture *box, *next;

	SAFE_RELEASE( Tex );
	for (box = UsedList; box != NULL; box = next)
	{
		next = box->Next;
		delete box;
	}
	for (box = EmptyList; box != NULL; box = next)
	{
		next = box->Next;
		delete box;
	}
	for (box = FreeList; box != NULL; box = next)
	{
		next = box->Next;
		delete box;
	}
}

//==========================================================================
//
// PackingTexture :: GetBestFit
//
// Returns the empty box that provides the best fit for the requested
// dimensions, or NULL if none of them are large enough.
//
//==========================================================================

D3DFB::PackedTexture *D3DFB::PackingTexture::GetBestFit(int w, int h, int &area)
{
	PackedTexture *box;
	int smallestarea = INT_MAX;
	PackedTexture *smallestbox = NULL;

	for (box = EmptyList; box != NULL; box = box->Next)
	{
		int boxw = box->Area.right - box->Area.left;
		int boxh = box->Area.bottom - box->Area.top;
		if (boxw >= w && boxh >= h)
		{
			int boxarea = boxw * boxh;
			if (boxarea < smallestarea)
			{
				smallestarea = boxarea;
				smallestbox = box;
				if (boxw == w && boxh == h)
				{ // An exact fit! Use it!
					break;
				}
			}
		}
	}
	area = smallestarea;
	return smallestbox;
}

//==========================================================================
//
// PackingTexture :: AllocateImage
//
// Moves the box from the empty list to the used list, sizing it to the
// requested dimensions and adding additional boxes to the empty list if
// needed.
//
// The passed box *MUST* be in this packing textures empty list.
//
//==========================================================================

void D3DFB::PackingTexture::AllocateImage(D3DFB::PackedTexture *box, int w, int h)
{
	RECT start = box->Area;

	box->Area.right = box->Area.left + w;
	box->Area.bottom = box->Area.top + h;

	box->Left = float(box->Area.left) / Width;
	box->Right = float(box->Area.right) / Width;
	box->Top = float(box->Area.top) / Height;
	box->Bottom = float(box->Area.bottom) / Height;

	// Remove it from the empty list.
	*(box->Prev) = box->Next;
	if (box->Next != NULL)
	{
		box->Next->Prev = box->Prev;
	}

	// Add it to the used list.
	box->Next = UsedList;
	if (box->Next != NULL)
	{
		box->Next->Prev = &box->Next;
	}
	UsedList = box;
	box->Prev = &UsedList;

	// If we didn't use the whole box, split the remainder into the empty list.

	if (box->Area.bottom + 7 < start.bottom && box->Area.right + 7 < start.right)
	{
		// Split like this:
		//   +---+------+
		//   |###|      |
		//   +---+------+
		//   |          |
		//   |          |
		//   +----------+
		if (box->Area.bottom < start.bottom)
		{
			AddEmptyBox(start.left, box->Area.bottom, start.right, start.bottom);
		}
		if (box->Area.right < start.right)
		{
			AddEmptyBox(box->Area.right, start.top, start.right, box->Area.bottom);
		}
	}
	else
	{
		// Split like this:
		//   +---+------+
		//   |###|      |
		//   +---+      |
		//   |   |      |
		//   |   |      |
		//   +---+------+
		if (box->Area.bottom < start.bottom)
		{
			AddEmptyBox(start.left, box->Area.bottom, box->Area.right, start.bottom);
		}
		if (box->Area.right < start.right)
		{
			AddEmptyBox(box->Area.right, start.top, start.right, start.bottom);
		}
	}
}

//==========================================================================
//
// PackingTexture :: AddEmptyBox
//
// Adds a box with the specified dimensions to the empty list.
//
//==========================================================================

void D3DFB::PackingTexture::AddEmptyBox(int left, int top, int right, int bottom)
{
	PackedTexture *box = AllocateBox();
	box->Area.left = left;
	box->Area.top = top;
	box->Area.right = right;
	box->Area.bottom = bottom;
	box->Next = EmptyList;
	if (box->Next != NULL)
	{
		box->Next->Prev = &box->Next;
	}
	box->Prev = &EmptyList;
	EmptyList = box;
}

//==========================================================================
//
// PackingTexture :: AllocateBox
//
// Returns a new PackedTexture box, either by retrieving one off the free
// list or by creating a new one. The box is not linked into a list.
//
//==========================================================================

D3DFB::PackedTexture *D3DFB::PackingTexture::AllocateBox()
{
	PackedTexture *box;

	if (FreeList != NULL)
	{
		box = FreeList;
		FreeList = box->Next;
		if (box->Next != NULL)
		{
			box->Next->Prev = &FreeList;
		}
	}
	else
	{
		box = new PackedTexture;
		box->Owner = this;
	}
	return box;
}

//==========================================================================
//
// PackingTexture :: FreeBox
//
// Removes a box from its current list and adds it to the empty list,
// updating EmptyArea. If there are no boxes left in the used list, then
// the empty list is replaced with a single box, so the texture can be
// subdivided again.
//
//==========================================================================

void D3DFB::PackingTexture::FreeBox(D3DFB::PackedTexture *box)
{
	*(box->Prev) = box->Next;
	if (box->Next != NULL)
	{
		box->Next->Prev = box->Prev;
	}
	box->Next = EmptyList;
	box->Prev = &EmptyList;
	if (EmptyList != NULL)
	{
		EmptyList->Prev = &box->Next;
	}
	EmptyList = box;
	if (UsedList == NULL)
	{ // No more space in use! Move all but this into the free list.
		if (box->Next != NULL)
		{
			D3DFB::PackedTexture *lastbox;

			// Find the last box in the free list.
			lastbox = FreeList;
			if (lastbox != NULL)
			{
				while (lastbox->Next != NULL)
				{
					lastbox = lastbox->Next;
				}
			}
			// Chain the empty list to the end of the free list.
			if (lastbox != NULL)
			{
				lastbox->Next = box->Next;
				box->Next->Prev = &lastbox->Next;
			}
			else
			{
				FreeList = box->Next;
				box->Next->Prev = &FreeList;
			}
			box->Next = NULL;
		}
		// Now this is the only box in the empty list, so it should
		// contain the whole texture.
		box->Area.left = 0;
		box->Area.top = 0;
		box->Area.right = Width;
		box->Area.bottom = Height;
	}
}

//==========================================================================
//
// D3DTex Constructor
//
//==========================================================================

D3DTex::D3DTex(FTexture *tex, D3DFB *fb, bool wrapping)
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
	Box = NULL;
	IsGray = false;

	Create(fb, wrapping);
}

//==========================================================================
//
// D3DTex Destructor
//
//==========================================================================

D3DTex::~D3DTex()
{
	if (Box != NULL)
	{
		Box->Owner->FreeBox(Box);
		Box = NULL;
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
// D3DTex :: CheckWrapping
//
// Returns true if the texture is compatible with the specified wrapping
// mode.
//
//==========================================================================

bool D3DTex::CheckWrapping(bool wrapping)
{
	 // If it doesn't need to wrap, then it works.
	if (!wrapping)
	{
		return true;
	}
	// If it needs to wrap, then it can't be packed inside another texture.
	return Box->Owner->OneUse;
}
//==========================================================================
//
// D3DTex :: Create
//
// Creates an IDirect3DTexture9 for the texture and copies the image data
// to it. Note that unlike FTexture, this image is row-major.
//
//==========================================================================

bool D3DTex::Create(D3DFB *fb, bool wrapping)
{
	if (Box != NULL)
	{
		Box->Owner->FreeBox(Box);
	}

	Box = fb->AllocPackedTexture(GameTex->GetWidth(), GameTex->GetHeight(), wrapping, GetTexFormat());

	if (Box == NULL)
	{
		return false;
	}
	if (!Update())
	{
		Box->Owner->FreeBox(Box);
		Box = NULL;
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

	assert(Box != NULL);
	assert(Box->Owner != NULL);
	assert(Box->Owner->Tex != NULL);
	assert(GameTex != NULL);

	if (FAILED(Box->Owner->Tex->GetLevelDesc(0, &desc)))
	{
		return false;
	}
	rect = Box->Area;
	if (FAILED(Box->Owner->Tex->LockRect(0, &lrect, &rect, 0)))
	{
		return false;
	}
	GameTex->FillBuffer((BYTE *)lrect.pBits, lrect.Pitch, GameTex->GetHeight(), ToTexFmt(desc.Format));
	Box->Owner->Tex->UnlockRect(0);
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
		// If the palette isn't big enough, then we don't need to
		// worry about setting the gamma ramp.
		BorderColor = (remap->NumEntries >= 256 - 8) ? ~0 : 0;
	}
	else
	{
		int pow2count;

		// Round up to the nearest power of 2.
		for (pow2count = 1; pow2count < remap->NumEntries; pow2count <<= 1)
		{ }
		count = pow2count;
		BorderColor = 0;
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
	SAFE_RELEASE( Tex );
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
	int skipat, i;

	assert(Tex != NULL);

	if (FAILED(Tex->LockRect(0, &lrect, NULL, 0)))
	{
		return false;
	}
	buff = (D3DCOLOR *)lrect.pBits;
	pal = Remap->Palette;

	// See explanation in UploadPalette() for skipat rationale.
	skipat = MIN(Remap->NumEntries, BorderColor != 0 ? 256 - 8 : 256);

	for (i = 0; i < skipat; ++i)
	{
		buff[i] = D3DCOLOR_ARGB(pal[i].a, pal[i].r, pal[i].g, pal[i].b);
	}
	for (++i; i < Remap->NumEntries; ++i)
	{
		buff[i] = D3DCOLOR_ARGB(pal[i].a, pal[i-1].r, pal[i-1].g, pal[i-1].b);
	}
	BorderColor = D3DCOLOR_ARGB(pal[i].a, pal[i-1].r, pal[i-1].g, pal[i-1].b);

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
	if (!Accel2D)
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

FNativeTexture *D3DFB::CreateTexture(FTexture *gametex, bool wrapping)
{
	D3DTex *tex = new D3DTex(gametex, this, wrapping);
	if (tex->Box == NULL)
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

FNativePalette *D3DFB::CreatePalette(FRemapTable *remap)
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
// D3DFB :: BeginLineBatch
//
//==========================================================================

void D3DFB::BeginLineBatch()
{
	if (In2D < 2 || !InScene || BatchType == BATCH_Lines)
	{
		return;
	}
	EndQuadBatch();		// Make sure all quads have been drawn first.
	VertexBuffer->Lock(0, 0, (void **)&VertexData, D3DLOCK_DISCARD);
	VertexPos = 0;
	BatchType = BATCH_Lines;
}

//==========================================================================
//
// D3DFB :: EndLineBatch
//
//==========================================================================

void D3DFB::EndLineBatch()
{
	if (In2D < 2 || !InScene || BatchType != BATCH_Lines)
	{
		return;
	}
	VertexBuffer->Unlock();
	if (VertexPos > 0)
	{
		SetPixelShader(ColorOnlyShader);
		SetAlphaBlend(D3DBLENDOP_ADD, D3DBLEND_SRCALPHA, D3DBLEND_INVSRCALPHA);
		D3DDevice->SetStreamSource(0, VertexBuffer, 0, sizeof(FBVERTEX));
		D3DDevice->DrawPrimitive(D3DPT_LINELIST, 0, VertexPos / 2);
	}
	VertexPos = -1;
	BatchType = BATCH_None;
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
	if (BatchType != BATCH_Lines)
	{
		BeginLineBatch();
	}
	if (VertexPos == NUM_VERTS)
	{ // Flush the buffer and refill it.
		EndLineBatch();
		BeginLineBatch();
	}
	// Add the endpoints to the vertex buffer.
	VertexData[VertexPos].x = float(x0);
	VertexData[VertexPos].y = float(y0) + LBOffset;
	VertexData[VertexPos].z = 0;
	VertexData[VertexPos].rhw = 1;
	VertexData[VertexPos].color0 = color;
	VertexData[VertexPos].color1 = 0;
	VertexData[VertexPos].tu = 0;
	VertexData[VertexPos].tv = 0;

	VertexData[VertexPos+1].x = float(x1);
	VertexData[VertexPos+1].y = float(y1) + LBOffset;
	VertexData[VertexPos+1].z = 0;
	VertexData[VertexPos+1].rhw = 1;
	VertexData[VertexPos+1].color0 = color;
	VertexData[VertexPos+1].color1 = 0;
	VertexData[VertexPos+1].tu = 0;
	VertexData[VertexPos+1].tv = 0;

	VertexPos += 2;
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
	EndBatch();		// Draw out any batched operations.
	SetPixelShader(ColorOnlyShader);
	SetAlphaBlend(D3DBLENDOP_ADD, D3DBLEND_SRCALPHA, D3DBLEND_INVSRCALPHA);
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

	D3DTex *tex = static_cast<D3DTex *>(img->GetNative(false));

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
	float u0 = tex->Box->Left;
	float v0 = tex->Box->Top;
	float u1 = tex->Box->Right;
	float v1 = tex->Box->Bottom;
	float uscale = 1.f / tex->Box->Owner->Width;
	float vscale = 1.f / tex->Box->Owner->Height / yscale;

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

	QuadExtra[QuadBatchPos].Texture = tex->Box->Owner;
	if (parms.bilinear)
	{
		QuadExtra[QuadBatchPos].Flags |= BQF_Bilinear;
	}

	float yoffs = GatheringWipeScreen ? 0.5f : 0.5f - LBOffset;

	// Coordinates are truncated to integers, because that's effectively
	// what the software renderer does. The hardware will instead round
	// to nearest, it seems.
	x0 = floorf(x0) - 0.5f;
	y0 = floorf(y0) - yoffs;
	x1 = floorf(x1) - 0.5f;
	y1 = floorf(y1) - yoffs;

	FBVERTEX *vert = &VertexData[VertexPos];

	vert[0].x = x0;
	vert[0].y = y0;
	vert[0].z = 0;
	vert[0].rhw = 1;
	vert[0].color0 = color0;
	vert[0].color1 = color1;
	vert[0].tu = u0;
	vert[0].tv = v0;

	vert[1].x = x1;
	vert[1].y = y0;
	vert[1].z = 0;
	vert[1].rhw = 1;
	vert[1].color0 = color0;
	vert[1].color1 = color1;
	vert[1].tu = u1;
	vert[1].tv = v0;

	vert[2].x = x1;
	vert[2].y = y1;
	vert[2].z = 0;
	vert[2].rhw = 1;
	vert[2].color0 = color0;
	vert[2].color1 = color1;
	vert[2].tu = u1;
	vert[2].tv = v1;

	vert[3].x = x0;
	vert[3].y = y1;
	vert[3].z = 0;
	vert[3].rhw = 1;
	vert[3].color0 = color0;
	vert[3].color1 = color1;
	vert[3].tu = u0;
	vert[3].tv = v1;

	IndexData[IndexPos    ] = VertexPos;
	IndexData[IndexPos + 1] = VertexPos + 1;
	IndexData[IndexPos + 2] = VertexPos + 2;
	IndexData[IndexPos + 3] = VertexPos;
	IndexData[IndexPos + 4] = VertexPos + 2;
	IndexData[IndexPos + 5] = VertexPos + 3;

	QuadBatchPos++;
	VertexPos += 4;
	IndexPos += 6;
}

//==========================================================================
//
// D3DFB :: FlatFill
//
// Fills an area with a repeating copy of the texture.
//
//==========================================================================

void D3DFB::FlatFill(int left, int top, int right, int bottom, FTexture *src, bool local_origin)
{
	if (In2D < 2)
	{
		Super::FlatFill(left, top, right, bottom, src, local_origin);
		return;
	}
	if (!InScene)
	{
		return;
	}
	D3DTex *tex = static_cast<D3DTex *>(src->GetNative(true));
	if (tex == NULL)
	{
		return;
	}
	float yoffs = GatheringWipeScreen ? 0.5f : 0.5f - LBOffset;
	float x0 = float(left);
	float y0 = float(top);
	float x1 = float(right);
	float y1 = float(bottom);
	float itw = 1.f / float(src->GetWidth());
	float ith = 1.f / float(src->GetHeight());
	float xo = local_origin ? x0 : 0;
	float yo = local_origin ? y0 : 0;
	float u0 = (x0 - xo) * itw;
	float v0 = (y0 - yo) * ith;
	float u1 = (x1 - xo) * itw;
	float v1 = (y1 - yo) * ith;
	x0 -= 0.5f;
	y0 -= yoffs;
	x1 -= 0.5f;
	y1 -= yoffs;

	CheckQuadBatch();

	BufferedQuad *quad = &QuadExtra[QuadBatchPos];
	FBVERTEX *vert = &VertexData[VertexPos];

	quad->Group1 = 0;
	if (tex->GetTexFormat() == D3DFMT_L8 && !tex->IsGray)
	{
		quad->Flags = BQF_WrapUV | BQF_GamePalette | BQF_DisableAlphaTest;
		quad->ShaderNum = BQS_PalTex;
	}
	else
	{
		quad->Flags = BQF_WrapUV | BQF_DisableAlphaTest;
		quad->ShaderNum = BQS_Plain;
	}
	quad->Palette = NULL;
	quad->Texture = tex->Box->Owner;

	vert[0].x = x0;
	vert[0].y = y0;
	vert[0].z = 0;
	vert[0].rhw = 1;
	vert[0].color0 = 0;
	vert[0].color1 = 0xFFFFFFFF;
	vert[0].tu = u0;
	vert[0].tv = v0;

	vert[1].x = x1;
	vert[1].y = y0;
	vert[1].z = 0;
	vert[1].rhw = 1;
	vert[1].color0 = 0;
	vert[1].color1 = 0xFFFFFFFF;
	vert[1].tu = u1;
	vert[1].tv = v0;

	vert[2].x = x1;
	vert[2].y = y1;
	vert[2].z = 0;
	vert[2].rhw = 1;
	vert[2].color0 = 0;
	vert[2].color1 = 0xFFFFFFFF;
	vert[2].tu = u1;
	vert[2].tv = v1;

	vert[3].x = x0;
	vert[3].y = y1;
	vert[3].z = 0;
	vert[3].rhw = 1;
	vert[3].color0 = 0;
	vert[3].color1 = 0xFFFFFFFF;
	vert[3].tu = u0;
	vert[3].tv = v1;

	IndexData[IndexPos    ] = VertexPos;
	IndexData[IndexPos + 1] = VertexPos + 1;
	IndexData[IndexPos + 2] = VertexPos + 2;
	IndexData[IndexPos + 3] = VertexPos;
	IndexData[IndexPos + 4] = VertexPos + 2;
	IndexData[IndexPos + 5] = VertexPos + 3;

	QuadBatchPos++;
	VertexPos += 4;
	IndexPos += 6;
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
	verts = &VertexData[VertexPos];

	float x = float(left) - 0.5f;
	float y = float(top) - 0.5f + (GatheringWipeScreen ? 0 : LBOffset);

	quad->Group1 = 0;
	quad->ShaderNum = BQS_ColorOnly;
	if ((color & 0xFF000000) != 0xFF000000)
	{
		quad->BlendOp = D3DBLENDOP_ADD;
		quad->SrcBlend = D3DBLEND_SRCALPHA;
		quad->DestBlend = D3DBLEND_INVSRCALPHA;
	}
	quad->Palette = NULL;
	quad->Texture = NULL;

	verts[0].x = x;
	verts[0].y = y;
	verts[0].z = 0;
	verts[0].rhw = 1;
	verts[0].color0 = color;
	verts[0].color1 = 0;
	verts[0].tu = 0;
	verts[0].tv = 0;

	verts[1].x = x + width;
	verts[1].y = y;
	verts[1].z = 0;
	verts[1].rhw = 1;
	verts[1].color0 = color;
	verts[1].color1 = 0;
	verts[1].tu = 0;
	verts[1].tv = 0;

	verts[2].x = x + width;
	verts[2].y = y + height;
	verts[2].z = 0;
	verts[2].rhw = 1;
	verts[2].color0 = color;
	verts[2].color1 = 0;
	verts[2].tu = 0;
	verts[2].tv = 0;

	verts[3].x = x;
	verts[3].y = y + height;
	verts[3].z = 0;
	verts[3].rhw = 1;
	verts[3].color0 = color;
	verts[3].color1 = 0;
	verts[3].tu = 0;
	verts[3].tv = 0;

	IndexData[IndexPos    ] = VertexPos;
	IndexData[IndexPos + 1] = VertexPos + 1;
	IndexData[IndexPos + 2] = VertexPos + 2;
	IndexData[IndexPos + 3] = VertexPos;
	IndexData[IndexPos + 4] = VertexPos + 2;
	IndexData[IndexPos + 5] = VertexPos + 3;

	QuadBatchPos++;
	VertexPos += 4;
	IndexPos += 6;
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
	if (BatchType == BATCH_Lines)
	{
		EndLineBatch();
	}
	else if (QuadBatchPos == MAX_QUAD_BATCH)
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
	EndLineBatch();		// Make sure all lines have been drawn first.
	VertexBuffer->Lock(0, 0, (void **)&VertexData, D3DLOCK_DISCARD);
	IndexBuffer->Lock(0, 0, (void **)&IndexData, D3DLOCK_DISCARD);
	VertexPos = 0;
	IndexPos = 0;
	QuadBatchPos = 0;
	BatchType = BATCH_Quads;
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
	if (In2D < 2 || !InScene || BatchType != BATCH_Quads)
	{
		return;
	}
	BatchType = BATCH_None;
	VertexBuffer->Unlock();
	IndexBuffer->Unlock();
	if (QuadBatchPos == 0)
	{
		QuadBatchPos = -1;
		VertexPos = -1;
		IndexPos = -1;
		return;
	}
	D3DDevice->SetStreamSource(0, VertexBuffer, 0, sizeof(FBVERTEX));
	D3DDevice->SetIndices(IndexBuffer);
	bool uv_wrapped = false;
	bool uv_should_wrap;

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
			SetPaletteTexture(PaletteTexture, 256, BorderColor);
		}
		else if ((quad->Flags & BQF_Paletted) == BQF_CustomPalette)
		{
			SetPaletteTexture(quad->Palette->Tex, quad->Palette->RoundedPaletteSize, quad->Palette->BorderColor);
		}
		// Set paletted bilinear filtering (IF IT WORKED RIGHT!)
		if ((quad->Flags & (BQF_Paletted | BQF_Bilinear)) == (BQF_Paletted | BQF_Bilinear))
		{
			SetPalTexBilinearConstants(quad->Texture);
		}

		// Set the alpha blending
		SetAlphaBlend(D3DBLENDOP(quad->BlendOp), D3DBLEND(quad->SrcBlend), D3DBLEND(quad->DestBlend));

		// Set the alpha test
		EnableAlphaTest(!(quad->Flags & BQF_DisableAlphaTest));

		// Set the pixel shader
		if (quad->ShaderNum == BQS_PalTex)
		{
			if (!(quad->Flags & BQF_Bilinear))
			{
				SetPixelShader((quad->Flags & BQF_InvertSource) ? InvPalTexShader : PalTexShader);
			}
			else
			{
				SetPixelShader(PalTexBilinearShader);
			}
		}
		else if (quad->ShaderNum == BQS_Plain)
		{
			SetPixelShader((quad->Flags & BQF_InvertSource) ? InvPlainShader : PlainShader);
		}
		else if (quad->ShaderNum == BQS_RedToAlpha)
		{
			SetPixelShader(RedToAlphaShader);
		}
		else if (quad->ShaderNum == BQS_ColorOnly)
		{
			SetPixelShader(ColorOnlyShader);
		}

		// Set the texture clamp addressing mode
		uv_should_wrap = !!(quad->Flags & BQF_WrapUV);
		if (uv_wrapped != uv_should_wrap)
		{
			DWORD mode = uv_should_wrap ? D3DTADDRESS_WRAP : D3DTADDRESS_BORDER;
			uv_wrapped = uv_should_wrap;
			D3DDevice->SetSamplerState(0, D3DSAMP_ADDRESSU, mode);
			D3DDevice->SetSamplerState(0, D3DSAMP_ADDRESSV, mode);
		}

		// Set the texture
		if (quad->Texture != NULL)
		{
			SetTexture(0, quad->Texture->Tex);
		}

		// Draw the quad
		D3DDevice->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 4 * i, 4 * (j - i), 6 * i, 2 * (j - i));
		i = j;
	}
	if (uv_wrapped)
	{
		D3DDevice->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_BORDER);
		D3DDevice->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_BORDER);
	}
	QuadBatchPos = -1;
	VertexPos = -1;
	IndexPos = -1;
}

//==========================================================================
//
// D3DFB :: EndBatch
//
// Draws whichever type of primitive is currently being batched.
//
//==========================================================================

void D3DFB::EndBatch()
{
	if (BatchType == BATCH_Quads)
	{
		EndQuadBatch();
	}
	else if (BatchType == BATCH_Lines)
	{
		EndLineBatch();
	}
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
	FRenderStyle style = parms.style;
	float alpha;
	bool stencilling;

	if (style.Flags & STYLEF_TransSoulsAlpha)
	{
		alpha = transsouls;
	}
	else if (style.Flags & STYLEF_Alpha1)
	{
		alpha = 1;
	}
	else
	{
		alpha = clamp<fixed_t> (parms.alpha, 0, FRACUNIT) / 65536.f;
	}

	// FIXME: Fuzz effect is not written
	if (style.BlendOp == STYLEOP_FuzzOrAdd || style.BlendOp == STYLEOP_Fuzz)
	{
		style.BlendOp = STYLEOP_Add;
	}
	else if (style.BlendOp == STYLEOP_FuzzOrSub)
	{
		style.BlendOp = STYLEOP_Sub;
	}
	else if (style.BlendOp == STYLEOP_FuzzOrRevSub)
	{
		style.BlendOp = STYLEOP_RevSub;
	}

	stencilling = false;
	quad.Palette = NULL;

	switch (style.BlendOp)
	{
	default:
	case STYLEOP_Add:		quad.BlendOp = D3DBLENDOP_ADD;			break;
	case STYLEOP_Sub:		quad.BlendOp = D3DBLENDOP_SUBTRACT;		break;
	case STYLEOP_RevSub:	quad.BlendOp = D3DBLENDOP_REVSUBTRACT;	break;
	case STYLEOP_None:		return false;
	}
	quad.SrcBlend = GetStyleAlpha(style.SrcAlpha);
	quad.DestBlend = GetStyleAlpha(style.DestAlpha);

	if (style.Flags & STYLEF_InvertOverlay)
	{
		// Only the overlay color is inverted, not the overlay alpha.
		parms.colorOverlay = D3DCOLOR_ARGB(APART(parms.colorOverlay),
			255 - RPART(parms.colorOverlay), 255 - GPART(parms.colorOverlay),
			255 - BPART(parms.colorOverlay));
	}

	SetColorOverlay(parms.colorOverlay, alpha, color0, color1);

	if (style.Flags & STYLEF_ColorIsFixed)
	{
		if (style.Flags & STYLEF_InvertSource)
		{ // Since the source color is a constant, we can invert it now
		  // without spending time doing it in the shader.
			parms.fillcolor = D3DCOLOR_XRGB(255 - RPART(parms.fillcolor),
				255 - GPART(parms.fillcolor), 255 - BPART(parms.fillcolor));
		}
		// Set up the color mod to replace the color from the image data.
		color0 = (color0 & D3DCOLOR_RGBA(0,0,0,255)) | (parms.fillcolor & D3DCOLOR_RGBA(255,255,255,0));
		color1 &= D3DCOLOR_RGBA(0,0,0,255);

		if (style.Flags & STYLEF_RedIsAlpha)
		{
			// Note that if the source texture is paletted, the palette is ignored.
			quad.Flags = 0;
			quad.ShaderNum = BQS_RedToAlpha;
		}
		else if (fmt == D3DFMT_L8)
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
		if (style.Flags & STYLEF_RedIsAlpha)
		{
			quad.Flags = 0;
			quad.ShaderNum = BQS_RedToAlpha;
		}
		else if (fmt == D3DFMT_L8)
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
		if (style.Flags & STYLEF_InvertSource)
		{
			quad.Flags |= BQF_InvertSource;
		}
	}

	// For unmasked images, force the alpha from the image data to be ignored.
	if (!parms.masked)
	{
		color0 = (color0 & D3DCOLOR_RGBA(255, 255, 255, 0)) | D3DCOLOR_COLORVALUE(0, 0, 0, alpha);
		color1 &= D3DCOLOR_RGBA(255, 255, 255, 0);

		// If our alpha is one and we are doing normal adding, then we can turn the blend off completely.
		if (quad.BlendOp == D3DBLENDOP_ADD &&
			((alpha == 1 && quad.SrcBlend == D3DBLEND_SRCALPHA) || quad.SrcBlend == D3DBLEND_ONE) &&
			((alpha == 1 && quad.DestBlend == D3DBLEND_INVSRCALPHA) || quad.DestBlend == D3DBLEND_ZERO))
		{
			quad.BlendOp = D3DBLENDOP(0);
		}
		quad.Flags |= BQF_DisableAlphaTest;
	}
	return true;
}

D3DBLEND D3DFB::GetStyleAlpha(int type)
{
	switch (type)
	{
	case STYLEALPHA_Zero:		return D3DBLEND_ZERO;
	case STYLEALPHA_One:		return D3DBLEND_ONE;
	case STYLEALPHA_Src:		return D3DBLEND_SRCALPHA;
	case STYLEALPHA_InvSrc:		return D3DBLEND_INVSRCALPHA;
	default:					return D3DBLEND_ZERO;
	}
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

void D3DFB::EnableAlphaTest(BOOL enabled)
{
	if (enabled != AlphaTestEnabled)
	{
		AlphaTestEnabled = enabled;
		D3DDevice->SetRenderState(D3DRS_ALPHATESTENABLE, enabled);
	}
}

void D3DFB::SetAlphaBlend(D3DBLENDOP op, D3DBLEND srcblend, D3DBLEND destblend)
{
	if (op == 0)
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
		if (AlphaBlendOp != op)
		{
			AlphaBlendOp = op;
			D3DDevice->SetRenderState(D3DRS_BLENDOP, op);
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

CVAR(Float, pal, 0.5f, 0)
CVAR(Float, pc, 255.f, 0)
void D3DFB::SetPaletteTexture(IDirect3DTexture9 *texture, int count, D3DCOLOR border_color)
{
	if (SM14)
	{
		// Shader Model 1.4 only uses 256-color palettes.
		SetConstant(2, 1.f, 0.5f / 256.f, 0, 0);
		if (border_color != 0 && CurBorderColor != border_color)
		{
			CurBorderColor = border_color;
			D3DDevice->SetSamplerState(1, D3DSAMP_BORDERCOLOR, border_color);
		}
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
		// is still within [0.0,1.0].
		//
		// The constant register c2 is used to hold the multiplier in the
		// x part and the adder in the y part.
		float fcount = 1 / float(count);
		SetConstant(2, pc * fcount, pal * fcount, 0, 0);
	}
	SetTexture(1, texture);
}

void D3DFB::SetPalTexBilinearConstants(PackingTexture *tex)
{
	float con[8];

	// Don't bother doing anything if the constants won't be used.
	if (PalTexShader == PalTexBilinearShader)
	{
		return;
	}

	con[0] = float(tex->Width);
	con[1] = float(tex->Height);
	con[2] = 0;
	con[3] = 1 / con[0];
	con[4] = 0;
	con[5] = 1 / con[1];
	con[6] = con[5];
	con[7] = con[3];

	D3DDevice->SetPixelShaderConstantF(3, con, 2);
}
