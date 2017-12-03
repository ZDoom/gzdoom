#pragma once

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

#define SAFE_RELEASE(x)		{ if (x != NULL) { x->Release(); x = NULL; } }

extern HANDLE FPSLimitEvent;

class DDrawFB : public BaseWinFB
{
	typedef BaseWinFB Super;
public:
	DDrawFB (int width, int height, bool fullscreen);
	~DDrawFB ();

	bool IsValid ();
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
	bool Is8BitMode();
    virtual int GetTrueHeight() { return TrueHeight; }

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
	uint8_t GammaTable[3][256];
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
	DWORD ClipSize;
	PalEntry Flash;
	int FlashAmount;
	int BufferCount;
	int BufferPitch;
	int TrueHeight;
	int PixelDoubling;
	float Gamma;

	bool NeedGammaUpdate;
	bool NeedPalUpdate;
	bool NeedResRecreate;
	bool PaletteChangeExpected;
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
	typedef BaseWinFB Super;
public:
	D3DFB (UINT adapter, int width, int height, bool bgra, bool fullscreen);
	~D3DFB ();

	bool IsValid ();
	bool Lock (bool buffered);
	void Unlock ();
	void Update ();
	void Flip ();
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
	void GetScreenshotBuffer(const uint8_t *&buffer, int &pitch, ESSType &color_type);
	void ReleaseScreenshotBuffer();
	void SetBlendingRect (int x1, int y1, int x2, int y2);
	bool Begin2D (bool copy3d);
	void DrawBlendingRect ();
	FNativeTexture *CreateTexture (FTexture *gametex, bool wrapping);
	FNativePalette *CreatePalette (FRemapTable *remap);
	void DrawTextureParms (FTexture *img, DrawParms &parms);
	void DoClear (int left, int top, int right, int bottom, int palcolor, uint32_t color);
	void DoDim (PalEntry color, float amount, int x1, int y1, int w, int h);
	void FlatFill (int left, int top, int right, int bottom, FTexture *src, bool local_origin);
	void DrawLine(int x0, int y0, int x1, int y1, int palColor, uint32_t realcolor);
	void DrawPixel(int x, int y, int palcolor, uint32_t rgbcolor);
	void FillSimplePoly(FTexture *tex, FVector2 *points, int npoints,
		double originx, double originy, double scalex, double scaley,
		DAngle rotation, const FColormap &colormap, PalEntry flatcolor, int lightlevel, int bottomclip) override;
	bool WipeStartScreen(int type);
	void WipeEndScreen();
	bool WipeDo(int ticks);
	void WipeCleanup();
	HRESULT GetHR ();
	bool Is8BitMode() { return false; }
    virtual int GetTrueHeight() { return TrueHeight; }

private:
	friend class D3DTex;
	friend class D3DPal;

	struct PackedTexture;
	struct Atlas;

	struct FBVERTEX
	{
		FLOAT x, y, z, rhw;
		D3DCOLOR color0, color1;
		FLOAT tu, tv;
	};
#define D3DFVF_FBVERTEX (D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_SPECULAR | D3DFVF_TEX1)

	struct BufferedTris
	{
		union
		{
			struct
			{
				uint8_t Flags;
				uint8_t ShaderNum:4;
				uint8_t BlendOp:4;
				uint8_t SrcBlend, DestBlend;
			};
			DWORD Group1;
		};
		uint8_t Desat;
		D3DPal *Palette;
		IDirect3DTexture9 *Texture;
		int NumVerts;		// Number of _unique_ vertices used by this set.
		int NumTris;		// Number of triangles used by this set.
	};

	enum
	{
		PSCONST_Desaturation = 1,
		PSCONST_PaletteMod = 2,
		PSCONST_Weights = 6,
		PSCONST_Gamma = 7,
	};
	enum
	{
		SHADER_NormalColor,
		SHADER_NormalColorPal,
		SHADER_NormalColorInv,
		SHADER_NormalColorPalInv,

		SHADER_RedToAlpha,
		SHADER_RedToAlphaInv,

		SHADER_VertexColor,

		SHADER_SpecialColormap,
		SHADER_SpecialColormapPal,

		SHADER_InGameColormap,
		SHADER_InGameColormapDesat,
		SHADER_InGameColormapInv,
		SHADER_InGameColormapInvDesat,
		SHADER_InGameColormapPal,
		SHADER_InGameColormapPalDesat,
		SHADER_InGameColormapPalInv,
		SHADER_InGameColormapPalInvDesat,

		SHADER_BurnWipe,
		SHADER_GammaCorrection,

		NUM_SHADERS
	};
	static const char *const ShaderNames[NUM_SHADERS];

	void SetInitialState();
	bool CreateResources();
	void ReleaseResources();
	bool LoadShaders();
	void CreateBlockSurfaces();
	bool CreateFBTexture();
	bool CreatePaletteTexture();
	bool CreateGammaTexture();
	bool CreateVertexes();
	void DoOffByOneCheck();
	void UploadPalette();
	void UpdateGammaTexture(float igamma);
	void FillPresentParameters (D3DPRESENT_PARAMETERS *pp, bool fullscreen, bool vsync);
	void CalcFullscreenCoords (FBVERTEX verts[4], bool viewarea_only, bool can_double, D3DCOLOR color0, D3DCOLOR color1) const;
	bool Reset();
	IDirect3DTexture9 *GetCurrentScreen(D3DPOOL pool=D3DPOOL_SYSTEMMEM);
	void ReleaseDefaultPoolItems();
	void KillNativePals();
	void KillNativeTexs();
	PackedTexture *AllocPackedTexture(int width, int height, bool wrapping, D3DFORMAT format);
	void DrawPackedTextures(int packnum);
	void DrawLetterbox();
	void Draw3DPart(bool copy3d);
	bool SetStyle(D3DTex *tex, DrawParms &parms, D3DCOLOR &color0, D3DCOLOR &color1, BufferedTris &quad);
	static D3DBLEND GetStyleAlpha(int type);
	static void SetColorOverlay(DWORD color, float alpha, D3DCOLOR &color0, D3DCOLOR &color1);
	void DoWindowedGamma();
	void AddColorOnlyQuad(int left, int top, int width, int height, D3DCOLOR color);
	void AddColorOnlyRect(int left, int top, int width, int height, D3DCOLOR color);
	void CheckQuadBatch(int numtris=2, int numverts=4);
	void BeginQuadBatch();
	void EndQuadBatch();
	void BeginLineBatch();
	void EndLineBatch();
	void EndBatch();
	void CopyNextFrontBuffer();

	D3DCAPS9 DeviceCaps;

	// State
	void EnableAlphaTest(BOOL enabled);
	void SetAlphaBlend(D3DBLENDOP op, D3DBLEND srcblend=D3DBLEND(0), D3DBLEND destblend=D3DBLEND(0));
	void SetConstant(int cnum, float r, float g, float b, float a);
	void SetPixelShader(IDirect3DPixelShader9 *shader);
	void SetTexture(int tnum, IDirect3DTexture9 *texture);
	void SetPaletteTexture(IDirect3DTexture9 *texture, int count, D3DCOLOR border_color);
	void SetPalTexBilinearConstants(Atlas *texture);

	BOOL AlphaTestEnabled;
	BOOL AlphaBlendEnabled;
	D3DBLENDOP AlphaBlendOp;
	D3DBLEND AlphaSrcBlend;
	D3DBLEND AlphaDestBlend;
	float Constant[3][4];
	D3DCOLOR CurBorderColor;
	IDirect3DPixelShader9 *CurPixelShader;
	IDirect3DTexture9 *Texture[5];

	PalEntry SourcePalette[256];
	D3DCOLOR BorderColor;
	D3DCOLOR FlashColor0, FlashColor1;
	PalEntry FlashColor;
	int FlashAmount;
	int TrueHeight;
	int PixelDoubling;
	int SkipAt;
	int LBOffsetI;
	int RenderTextureToggle;
	int CurrRenderTexture;
	float LBOffset;
	float Gamma;
	bool UpdatePending;
	bool NeedPalUpdate;
	bool NeedGammaUpdate;
	int FBWidth, FBHeight;
	D3DFORMAT FBFormat;
	bool VSync;
	RECT BlendingRect;
	int In2D;
	bool InScene;
	bool SM14;
	bool GatheringWipeScreen;
	bool AALines;
	uint8_t BlockNum;
	D3DPal *Palettes;
	D3DTex *Textures;
	Atlas *Atlases;
	HRESULT LastHR;

	UINT Adapter;
	IDirect3DDevice9 *D3DDevice;
	IDirect3DTexture9 *FBTexture;
	IDirect3DTexture9 *TempRenderTexture, *RenderTexture[2];
	IDirect3DTexture9 *PaletteTexture;
	IDirect3DTexture9 *GammaTexture;
	IDirect3DTexture9 *ScreenshotTexture;
	IDirect3DSurface9 *ScreenshotSurface;
	IDirect3DSurface9 *FrontCopySurface;

	IDirect3DVertexBuffer9 *VertexBuffer;
	FBVERTEX *VertexData;
	IDirect3DIndexBuffer9 *IndexBuffer;
	uint16_t *IndexData;
	BufferedTris *QuadExtra;
	int VertexPos;
	int IndexPos;
	int QuadBatchPos;
	enum { BATCH_None, BATCH_Quads, BATCH_Lines } BatchType;

	IDirect3DPixelShader9 *Shaders[NUM_SHADERS];
	IDirect3DPixelShader9 *GammaShader;

	IDirect3DSurface9 *BlockSurface[2];
	IDirect3DSurface9 *OldRenderTarget;
	IDirect3DTexture9 *InitialWipeScreen, *FinalWipeScreen;

	D3DFB() {}

	class Wiper
	{
	public:
		virtual ~Wiper();
		virtual bool Run(int ticks, D3DFB *fb) = 0;

		void DrawScreen(D3DFB *fb, IDirect3DTexture9 *tex,
			D3DBLENDOP blendop=D3DBLENDOP(0), D3DCOLOR color0=0, D3DCOLOR color1=0xFFFFFFF);
	};

	class Wiper_Melt;			friend class Wiper_Melt;
	class Wiper_Burn;			friend class Wiper_Burn;
	class Wiper_Crossfade;		friend class Wiper_Crossfade;

	Wiper *ScreenWipe;
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
	BQF_Desaturated		= 128,
};

// Shaders for a buffered quad
enum
{
	BQS_PalTex,
	BQS_Plain,
	BQS_RedToAlpha,
	BQS_ColorOnly,
	BQS_SpecialColormap,
	BQS_InGameColormap,
};

#if _DEBUG && 0
#define STARTLOG
#define STOPLOG
#define LOG(x)			{ OutputDebugString(x); }
#define LOG1(x,y)		{ char poo[1024]; mysnprintf(poo, countof(poo), x, y); OutputDebugString(poo); }
#define LOG2(x,y,z)		{ char poo[1024]; mysnprintf(poo, countof(poo), x, y, z); OutputDebugString(poo); }
#define LOG3(x,y,z,zz)	{ char poo[1024]; mysnprintf(poo, countof(poo), x, y, z, zz); OutputDebugString(poo); }
#define LOG4(x,y,z,a,b)	{ char poo[1024]; mysnprintf(poo, countof(poo), x, y, z, a, b); OutputDebugString(poo); }
#define LOG5(x,y,z,a,b,c) { char poo[1024]; mysnprintf(poo, countof(poo), x, y, z, a, b, c); OutputDebugString(poo); }
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
