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


class D3DFB : public BaseWinFB
{
	typedef BaseWinFB Super;

	DSimpleCanvas *RenderBuffer = nullptr;

public:
	D3DFB (UINT adapter, int width, int height, bool bgra, bool fullscreen);
	~D3DFB ();
	virtual DCanvas *GetCanvas() { return RenderBuffer; }

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
	void SetVSync (bool vsync);
	void NewRefreshRate();
	void GetScreenshotBuffer(const uint8_t *&buffer, int &pitch, ESSType &color_type, float &gamma) override;
	void ReleaseScreenshotBuffer();
	void SetBlendingRect (int x1, int y1, int x2, int y2);
	bool Begin2D (bool copy3d);
	void DrawBlendingRect ();
	FNativeTexture *CreateTexture (FTexture *gametex, FTextureFormat fmt, bool wrapping);
	FNativePalette *CreatePalette (FRemapTable *remap);
	bool WipeStartScreen(int type);
	void WipeEndScreen();
	bool WipeDo(int ticks);
	void WipeCleanup();
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
		PSCONST_Color1 = 3,
		PSCONST_Color2 = 4,
		PSCONST_BUFFERED_MAX,
		PSCONST_Weights = 6,
		PSCONST_Gamma = 7,

	};
	enum
	{
		SHADER_NormalColor,
		SHADER_NormalColorPal,
		SHADER_NormalColorD,
		SHADER_NormalColorPalD,
		SHADER_NormalColorInv,
		SHADER_NormalColorPalInv,
		SHADER_NormalColorOpaq,
		SHADER_NormalColorPalOpaq,
		SHADER_NormalColorInvOpaq,
		SHADER_NormalColorPalInvOpaq,

		SHADER_AlphaTex,
		SHADER_PalAlphaTex,
		SHADER_Stencil,
		SHADER_PalStencil,

		SHADER_VertexColor,

		SHADER_SpecialColormap,
		SHADER_SpecialColormapPal,

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
	bool CreateVertexes(int numv, int numi);
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
	void DrawLetterbox();
	void Draw3DPart(bool copy3d);
	static D3DBLEND GetStyleAlpha(int type);
	void DoWindowedGamma();
	void CheckQuadBatch(int numtris=2, int numverts=4);
	void BeginQuadBatch();
	void EndQuadBatch();
	void CopyNextFrontBuffer();
	void Draw2D() override;

	D3DCAPS9 DeviceCaps;

	// State
	void EnableAlphaTest(BOOL enabled);
	void SetAlphaBlend(D3DBLENDOP op, D3DBLEND srcblend=D3DBLEND(0), D3DBLEND destblend=D3DBLEND(0));
	void SetConstant(int cnum, float r, float g, float b, float a);
	void SetPixelShader(IDirect3DPixelShader9 *shader);
	void SetTexture(int tnum, IDirect3DTexture9 *texture);
	void SetPaletteTexture(IDirect3DTexture9 *texture, int count, D3DCOLOR border_color);

	BOOL AlphaTestEnabled;
	BOOL AlphaBlendEnabled;
	D3DBLENDOP AlphaBlendOp;
	D3DBLEND AlphaSrcBlend;
	D3DBLEND AlphaDestBlend;
	float Constant[PSCONST_BUFFERED_MAX][4];
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

	// This stuff is still needed for the Wiper (which will be refactored later)
	BufferedTris *QuadExtra;
	int VertexPos;
	int IndexPos;
	int QuadBatchPos;
	enum { BATCH_None, BATCH_Quads, BATCH_Lines } BatchType;

	unsigned int NumVertices = 0;
	unsigned int NumIndices = 0;

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
