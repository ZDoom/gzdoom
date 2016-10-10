#ifndef __GL_SWFRAMEBUFFER
#define __GL_SWFRAMEBUFFER

#ifdef _WIN32
#include "win32iface.h"
#include "win32gliface.h"
#endif

#include "SkylineBinPack.h"

#include <memory>

class FGLDebug;

#ifdef _WIN32
class OpenGLSWFrameBuffer : public Win32GLFrameBuffer
{
	typedef Win32GLFrameBuffer Super;
	DECLARE_CLASS(OpenGLSWFrameBuffer, Win32GLFrameBuffer)
#else
#include "sdlglvideo.h"
class OpenGLFrameBuffer : public SDLGLFB
{
//	typedef SDLGLFB Super;	//[C]commented, DECLARE_CLASS defines this in linux
	DECLARE_CLASS(OpenGLSWFrameBuffer, SDLGLFB)
#endif


public:

	explicit OpenGLSWFrameBuffer() {}
	OpenGLSWFrameBuffer(void *hMonitor, int width, int height, int bits, int refreshHz, bool fullscreen);
	~OpenGLSWFrameBuffer();

	bool IsValid() override;
	bool Lock(bool buffered) override;
	void Unlock() override;
	void Update() override;
	PalEntry *GetPalette() override;
	void GetFlashedPalette(PalEntry palette[256]) override;
	void UpdatePalette() override;
	bool SetGamma(float gamma) override;
	bool SetFlash(PalEntry rgb, int amount) override;
	void GetFlash(PalEntry &rgb, int &amount) override;
	int GetPageCount() override;
	bool IsFullscreen() override;
	void PaletteChanged() override;
	int QueryNewPalette() override;
	void Blank() override;
	bool PaintToWindow() override;
	void SetVSync(bool vsync) override;
	void NewRefreshRate() override;
	void GetScreenshotBuffer(const uint8_t *&buffer, int &pitch, ESSType &color_type) override;
	void ReleaseScreenshotBuffer() override;
	void SetBlendingRect(int x1, int y1, int x2, int y2) override;
	bool Begin2D(bool copy3d) override;
	void DrawBlendingRect() override;
	FNativeTexture *CreateTexture(FTexture *gametex, bool wrapping) override;
	FNativePalette *CreatePalette(FRemapTable *remap) override;
	void DrawTextureParms(FTexture *img, DrawParms &parms) override;
	void Clear(int left, int top, int right, int bottom, int palcolor, uint32 color) override;
	void Dim(PalEntry color, float amount, int x1, int y1, int w, int h) override;
	void FlatFill(int left, int top, int right, int bottom, FTexture *src, bool local_origin) override;
	void DrawLine(int x0, int y0, int x1, int y1, int palColor, uint32 realcolor) override;
	void DrawPixel(int x, int y, int palcolor, uint32 rgbcolor) override;
	void FillSimplePoly(FTexture *tex, FVector2 *points, int npoints, double originx, double originy, double scalex, double scaley, DAngle rotation, FDynamicColormap *colormap, int lightlevel) override;
	//bool WipeStartScreen(int type) override;
	//void WipeEndScreen() override;
	//bool WipeDo(int ticks) override;
	//void WipeCleanup() override;
	bool Is8BitMode() override { return false; }
	int GetTrueHeight() override { return TrueHeight; }

private:
	struct FBVERTEX
	{
		FLOAT x, y, z, rhw;
		uint32_t color0, color1;
		FLOAT tu, tv;
	};
	//#define D3DFVF_FBVERTEX (D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_SPECULAR | D3DFVF_TEX1)

	struct GammaRamp
	{
		uint16_t red[256], green[256], blue[256];
	};

	struct LockedRect
	{
		int Pitch;
		void *pBits;
	};

	struct LTRBRect
	{
		int left, top, right, bottom;
	};

	class HWSurface
	{
	public:
		bool LockRect(LockedRect *outRect, LTRBRect *srcRect, bool discard) { outRect->Pitch = 0; outRect->pBits = nullptr; return false; }
		void UnlockRect() { }
	};

	class HWTexture
	{
	public:
		bool LockRect(LockedRect *outRect, LTRBRect *srcRect, bool discard) { outRect->Pitch = 0; outRect->pBits = nullptr; return false; }
		void UnlockRect() { }
		bool GetSurfaceLevel(int level, HWSurface **outSurface) { *outSurface = nullptr; return false; }

		int Texture = 0;
		int WrapS = 0;
		int WrapT = 0;
		int Format = 0;
	};

	class HWVertexBuffer
	{
	public:
		~HWVertexBuffer()
		{
			if (Buffer != 0) glDeleteVertexArrays(1, (GLuint*)&VertexArray);
			if (Buffer != 0) glDeleteBuffers(1, (GLuint*)&Buffer);
		}

		FBVERTEX *Lock() { return nullptr; }
		void Unlock() { }

		int Buffer = 0;
		int VertexArray = 0;
	};

	class HWIndexBuffer
	{
	public:
		~HWIndexBuffer()
		{
			if (Buffer != 0) glDeleteBuffers(1, (GLuint*)&Buffer);
		}

		uint16_t *Lock() { return nullptr; }
		void Unlock() { }

		int Buffer = 0;
	};

	class HWPixelShader
	{
	public:
		~HWPixelShader()
		{
			if (Program != 0) glDeleteProgram(Program);
			if (VertexShader != 0) glDeleteShader(VertexShader);
			if (FragmentShader != 0) glDeleteShader(FragmentShader);
		}

		int Program = 0;
		int VertexShader = 0;
		int FragmentShader = 0;
	};

	bool CreatePixelShader(const void *vertexsrc, const void *fragmentsrc, HWPixelShader **outShader);
	bool CreateVertexBuffer(int size, HWVertexBuffer **outVertexBuffer);
	bool CreateIndexBuffer(int size, HWIndexBuffer **outIndexBuffer);
	bool CreateOffscreenPlainSurface(int width, int height, int format, HWSurface **outSurface);
	bool CreateTexture(int width, int height, int levels, int format, HWTexture **outTexture);
	bool CreateRenderTarget(int width, int height, int format, HWSurface **outSurface);
	bool GetBackBuffer(HWSurface **outSurface);
	bool GetRenderTarget(int index, HWSurface **outSurface);
	void GetRenderTargetData(HWSurface *a, HWSurface *b);
	void ColorFill(HWSurface *surface, float red, float green, float blue);
	void StretchRect(HWSurface *src, const LTRBRect *srcrect, HWSurface *dest);
	bool SetRenderTarget(int index, HWSurface *surface);
	void SetGammaRamp(const GammaRamp *ramp);
	void SetPixelShaderConstantF(int uniformIndex, const float *data, int vec4fcount);
	void SetHWPixelShader(HWPixelShader *shader);
	void SetStreamSource(HWVertexBuffer *vertexBuffer);
	void SetIndices(HWIndexBuffer *indexBuffer);
	void DrawTriangleFans(int count, const FBVERTEX *vertices);
	void DrawPoints(int count, const FBVERTEX *vertices);
	void DrawLineList(int count);
	void DrawTriangleList(int minIndex, int numVertices, int startIndex, int primitiveCount);
	void Present();

	static uint32_t ColorARGB(uint32_t a, uint32_t r, uint32_t g, uint32_t b) { return ((a & 0xff) << 24) | ((r & 0xff) << 16) | ((g & 0xff) << 8) | ((b) & 0xff); }
	static uint32_t ColorRGBA(uint32_t a, uint32_t r, uint32_t g, uint32_t b) { return ColorARGB(a, r, g, b); }
	static uint32_t ColorXRGB(uint32_t r, uint32_t g, uint32_t b) { return ColorARGB(0xff, r, g, b); }
	static uint32_t ColorValue(float r, float g, float b, float a) { return ColorRGBA((uint32_t)(r * 255.0f), (uint32_t)(g * 255.0f), (uint32_t)(b * 255.0f), (uint32_t)(a * 255.0f)); }

	// The number of points for the vertex buffer.
	enum { NUM_VERTS = 10240 };

	// The number of indices for the index buffer.
	enum { NUM_INDEXES = ((NUM_VERTS * 6) / 4) };

	// The number of quads we can batch together.
	enum { MAX_QUAD_BATCH = (NUM_INDEXES / 6) };

	// The default size for a texture atlas.
	enum { DEF_ATLAS_WIDTH = 512 };
	enum { DEF_ATLAS_HEIGHT = 512 };

	// TYPES -------------------------------------------------------------------

	struct Atlas;

	struct PackedTexture
	{
		Atlas *Owner;

		PackedTexture **Prev, *Next;

		// Pixels this image covers
		LTRBRect Area;

		// Texture coordinates for this image
		float Left, Top, Right, Bottom;

		// Texture has extra space on the border?
		bool Padded;
	};

	struct Atlas
	{
		Atlas(OpenGLSWFrameBuffer *fb, int width, int height, int format);
		~Atlas();

		PackedTexture *AllocateImage(const Rect &rect, bool padded);
		void FreeBox(PackedTexture *box);

		SkylineBinPack Packer;
		Atlas *Next;
		HWTexture *Tex;
		int Format;
		PackedTexture *UsedList;	// Boxes that contain images
		int Width, Height;
		bool OneUse;
	};

	class OpenGLTex : public FNativeTexture
	{
	public:
		OpenGLTex(FTexture *tex, OpenGLSWFrameBuffer *fb, bool wrapping);
		~OpenGLTex();

		FTexture *GameTex;
		PackedTexture *Box;

		OpenGLTex **Prev;
		OpenGLTex *Next;

		bool IsGray;

		bool Create(OpenGLSWFrameBuffer *fb, bool wrapping);
		bool Update();
		bool CheckWrapping(bool wrapping);
		int GetTexFormat();
		FTextureFormat ToTexFmt(int fmt);
	};

	class OpenGLPal : public FNativePalette
	{
	public:
		OpenGLPal(FRemapTable *remap, OpenGLSWFrameBuffer *fb);
		~OpenGLPal();

		OpenGLPal **Prev;
		OpenGLPal *Next;

		HWTexture *Tex;
		uint32_t BorderColor;
		bool DoColorSkip;

		bool Update();

		FRemapTable *Remap;
		int RoundedPaletteSize;
	};

	// Flags for a buffered quad
	enum
	{
		BQF_GamePalette = 1,
		BQF_CustomPalette = 7,
		BQF_Paletted = 7,
		BQF_Bilinear = 8,
		BQF_WrapUV = 16,
		BQF_InvertSource = 32,
		BQF_DisableAlphaTest = 64,
		BQF_Desaturated = 128,
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

	struct PackedTexture;
	struct Atlas;

	struct BufferedTris
	{
		union
		{
			struct
			{
				uint8_t Flags;
				uint8_t ShaderNum : 4;
				int BlendOp;
				int SrcBlend, DestBlend;
			};
			uint32_t Group1;
		};
		uint8_t Desat;
		OpenGLPal *Palette;
		HWTexture *Texture;
		uint16_t NumVerts;		// Number of _unique_ vertices used by this set.
		uint16_t NumTris;		// Number of triangles used by this set.
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

	void Flip();
	void SetInitialState();
	bool CreateResources();
	void ReleaseResources();
	bool LoadShaders();
	void CreateBlockSurfaces();
	bool CreateFBTexture();
	bool CreatePaletteTexture();
	bool CreateVertexes();
	void UploadPalette();
	void UpdateGammaTexture(float igamma);
	void CalcFullscreenCoords(FBVERTEX verts[4], bool viewarea_only, bool can_double, uint32_t color0, uint32_t color1) const;
	bool Reset();
	HWTexture *GetCurrentScreen();
	void ReleaseDefaultPoolItems();
	void KillNativePals();
	void KillNativeTexs();
	PackedTexture *AllocPackedTexture(int width, int height, bool wrapping, int format);
	void DrawPackedTextures(int packnum);
	void DrawLetterbox();
	void Draw3DPart(bool copy3d);
	bool SetStyle(OpenGLTex *tex, DrawParms &parms, uint32_t &color0, uint32_t &color1, BufferedTris &quad);
	static int GetStyleAlpha(int type);
	static void SetColorOverlay(uint32_t color, float alpha, uint32_t &color0, uint32_t &color1);
	void DoWindowedGamma();
	void AddColorOnlyQuad(int left, int top, int width, int height, uint32_t color);
	void AddColorOnlyRect(int left, int top, int width, int height, uint32_t color);
	void CheckQuadBatch(int numtris = 2, int numverts = 4);
	void BeginQuadBatch();
	void EndQuadBatch();
	void BeginLineBatch();
	void EndLineBatch();
	void EndBatch();
	void CopyNextFrontBuffer();

	// State
	void EnableAlphaTest(BOOL enabled);
	void SetAlphaBlend(int op, int srcblend = 0, int destblend = 0);
	void SetConstant(int cnum, float r, float g, float b, float a);
	void SetPixelShader(HWPixelShader *shader);
	void SetTexture(int tnum, HWTexture *texture);
	void SetSamplerWrapS(int tnum, int mode);
	void SetSamplerWrapT(int tnum, int mode);
	void SetPaletteTexture(HWTexture *texture, int count, uint32_t border_color);

	template<typename T> static void SafeRelease(T &x) { if (x != nullptr) { delete x; x = nullptr; } }

	std::unique_ptr<HWVertexBuffer> StreamVertexBuffer;

	BOOL AlphaTestEnabled;
	BOOL AlphaBlendEnabled;
	int AlphaBlendOp;
	int AlphaSrcBlend;
	int AlphaDestBlend;
	float Constant[3][4];
	uint32_t CurBorderColor;
	HWPixelShader *CurPixelShader;
	HWTexture *Texture[5];
	int SamplerWrapS[5], SamplerWrapT[5];

	PalEntry SourcePalette[256];
	uint32_t BorderColor;
	uint32_t FlashColor0, FlashColor1;
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
	bool VSync;
	LTRBRect BlendingRect;
	int In2D;
	bool InScene;
	bool GatheringWipeScreen;
	bool AALines;
	uint8_t BlockNum;
	OpenGLPal *Palettes;
	OpenGLTex *Textures;
	Atlas *Atlases;

	HWTexture *FBTexture;
	HWTexture *TempRenderTexture, *RenderTexture[2];
	HWTexture *PaletteTexture;
	HWTexture *GammaTexture;
	HWTexture *ScreenshotTexture;
	HWSurface *ScreenshotSurface;
	HWSurface *FrontCopySurface;

	HWVertexBuffer *VertexBuffer;
	FBVERTEX *VertexData;
	HWIndexBuffer *IndexBuffer;
	uint16_t *IndexData;
	BufferedTris *QuadExtra;
	int VertexPos;
	int IndexPos;
	int QuadBatchPos;
	enum { BATCH_None, BATCH_Quads, BATCH_Lines } BatchType;

	HWPixelShader *Shaders[NUM_SHADERS];
	HWPixelShader *GammaShader;

	HWSurface *BlockSurface[2];
	HWSurface *OldRenderTarget;
	HWTexture *InitialWipeScreen, *FinalWipeScreen;

	class Wiper
	{
	public:
		virtual ~Wiper();
		virtual bool Run(int ticks, OpenGLSWFrameBuffer *fb) = 0;

		//void DrawScreen(OpenGLSWFrameBuffer *fb, HWTexture *tex, int blendop = 0, uint32_t color0 = 0, uint32_t color1 = 0xFFFFFFF);
	};

	class Wiper_Melt;			friend class Wiper_Melt;
	class Wiper_Burn;			friend class Wiper_Burn;
	class Wiper_Crossfade;		friend class Wiper_Crossfade;

	Wiper *ScreenWipe;
};


#endif //__GL_SWFRAMEBUFFER
