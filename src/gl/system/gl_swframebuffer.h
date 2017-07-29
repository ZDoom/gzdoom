#ifndef __GL_SWFRAMEBUFFER
#define __GL_SWFRAMEBUFFER

#ifdef _WIN32
#include "win32iface.h"
#include "win32gliface.h"
#endif

#include "SkylineBinPack.h"
#include "textures.h"

#include <memory>

class FGLDebug;

#ifdef _WIN32
class OpenGLSWFrameBuffer : public Win32GLFrameBuffer
{
	typedef Win32GLFrameBuffer Super;
#else
#include "sdlglvideo.h"
class OpenGLSWFrameBuffer : public SDLGLFB
{
	typedef SDLGLFB Super;	//[C]commented, DECLARE_CLASS defines this in linux
#endif


public:

	explicit OpenGLSWFrameBuffer() {}
	OpenGLSWFrameBuffer(void *hMonitor, int width, int height, int bits, int refreshHz, bool fullscreen, bool bgra);
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
	void DoClear(int left, int top, int right, int bottom, int palcolor, uint32_t color) override;
	void DoDim(PalEntry color, float amount, int x1, int y1, int w, int h) override;
	void FlatFill(int left, int top, int right, int bottom, FTexture *src, bool local_origin) override;
	void DrawLine(int x0, int y0, int x1, int y1, int palColor, uint32_t realcolor) override;
	void DrawPixel(int x, int y, int palcolor, uint32_t rgbcolor) override;
	void FillSimplePoly(FTexture *tex, FVector2 *points, int npoints, double originx, double originy, double scalex, double scaley, DAngle rotation, const FColormap &colormap, PalEntry flatcolor, int lightlevel, int bottomclip) override;
	bool WipeStartScreen(int type) override;
	void WipeEndScreen() override;
	bool WipeDo(int ticks) override;
	void WipeCleanup() override;

#ifdef WIN32
	void PaletteChanged() override { }
	int QueryNewPalette() override { return 0; }
	void Blank() override { }
	bool PaintToWindow() override;
	bool Is8BitMode() override { return false; }
	int GetTrueHeight() override { return TrueHeight; }
#endif

	void ScaleCoordsFromWindow(int16_t &x, int16_t &y) override;

private:
	struct FBVERTEX
	{
		float x, y, z, rhw;
		uint32_t color0, color1;
		float tu, tv;
	};

	struct BURNVERTEX
	{
		float x, y, z, rhw;
		float tu0, tv0;
		float tu1, tv1;
	};

	enum
	{
		PSCONST_Desaturation,
		PSCONST_PaletteMod,
		PSCONST_Weights,
		PSCONST_Gamma,
		PSCONST_ScreenSize,
		NumPSCONST
	};

	struct GammaRamp
	{
		uint16_t red[256], green[256], blue[256];
	};

	struct LTRBRect
	{
		int left, top, right, bottom;
	};

	class HWTexture
	{
	public:
		HWTexture() { Buffers[0] = 0; Buffers[1] = 0; }
		~HWTexture();

		int Texture = 0;
		int Buffers[2];
		int CurrentBuffer = 0;
		int WrapS = 0;
		int WrapT = 0;
		int Format = 0;
		
		std::vector<uint8_t> MapBuffer;
	};

	class HWFrameBuffer
	{
	public:
		~HWFrameBuffer();

		int Framebuffer = 0;
		std::unique_ptr<HWTexture> Texture;
	};


	class HWVertexBuffer
	{
	public:
		~HWVertexBuffer();

		FBVERTEX *Lock();
		void Unlock();

		int VertexArray = 0;
		int Buffer = 0;
		int Size = 0;
	};

	class HWIndexBuffer
	{
	public:
		~HWIndexBuffer();

		uint16_t *Lock();
		void Unlock();

		int Buffer = 0;
		int Size = 0;

	private:
		int LockedOldBinding = 0;
	};

	class HWPixelShader
	{
	public:
		~HWPixelShader();

		int Program = 0;
		int VertexShader = 0;
		int FragmentShader = 0;

		int ConstantLocations[NumPSCONST];
		int ImageLocation = -1;
		int PaletteLocation = -1;
		int NewScreenLocation = -1;
		int BurnLocation = -1;
	};

	std::unique_ptr<HWFrameBuffer> CreateFrameBuffer(const FString &name, int width, int height);
	std::unique_ptr<HWPixelShader> CreatePixelShader(FString vertexsrc, FString fragmentsrc, const FString &defines);
	std::unique_ptr<HWVertexBuffer> CreateVertexBuffer(int size);
	std::unique_ptr<HWIndexBuffer> CreateIndexBuffer(int size);
	std::unique_ptr<HWTexture> CreateTexture(const FString &name, int width, int height, int levels, int format);

	void SetGammaRamp(const GammaRamp *ramp);
	void SetPixelShaderConstantF(int uniformIndex, const float *data, int vec4fcount);
	void SetHWPixelShader(HWPixelShader *shader);
	void SetStreamSource(HWVertexBuffer *vertexBuffer);
	void SetIndices(HWIndexBuffer *indexBuffer);
	void DrawTriangleFans(int count, const FBVERTEX *vertices);
	void DrawTriangleFans(int count, const BURNVERTEX *vertices);
	void DrawPoints(int count, const FBVERTEX *vertices);
	void DrawLineList(int count);
	void DrawTriangleList(int minIndex, int numVertices, int startIndex, int primitiveCount);
	void Present();

	void GetLetterboxFrame(int &x, int &y, int &width, int &height);
	
	static void BgraToRgba(uint32_t *dest, const uint32_t *src, int width, int height, int srcpitch);

	void BindFBBuffer();
	void *MappedMemBuffer = nullptr;
	bool UseMappedMemBuffer = true;

	static uint32_t ColorARGB(uint32_t a, uint32_t r, uint32_t g, uint32_t b) { return ((a & 0xff) << 24) | ((r & 0xff) << 16) | ((g & 0xff) << 8) | ((b) & 0xff); }
	static uint32_t ColorRGBA(uint32_t r, uint32_t g, uint32_t b, uint32_t a) { return ColorARGB(a, r, g, b); }
	static uint32_t ColorXRGB(uint32_t r, uint32_t g, uint32_t b) { return ColorARGB(0xff, r, g, b); }
	static uint32_t ColorValue(float r, float g, float b, float a) { return ColorRGBA((uint32_t)(r * 255.0f), (uint32_t)(g * 255.0f), (uint32_t)(b * 255.0f), (uint32_t)(a * 255.0f)); }
	
	static void *MapBuffer(int target, int size);

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
		std::unique_ptr<HWTexture> Tex;
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

		std::unique_ptr<HWTexture> Tex;
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

	struct BufferedTris
	{
		uint8_t Flags;
		uint8_t ShaderNum;
		int BlendOp;
		int SrcBlend;
		int DestBlend;

		uint8_t Desat;
		OpenGLPal *Palette;
		HWTexture *Texture;
		uint16_t NumVerts;		// Number of _unique_ vertices used by this set.
		uint16_t NumTris;		// Number of triangles used by this set.

		void ClearSetup()
		{
			Flags = 0;
			ShaderNum = 0;
			BlendOp = 0;
			SrcBlend = 0;
			DestBlend = 0;
		}

		bool IsSameSetup(const BufferedTris &other) const
		{
			return Flags == other.Flags && ShaderNum == other.ShaderNum && BlendOp == other.BlendOp && SrcBlend == other.SrcBlend && DestBlend == other.DestBlend;
		}
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
	static const char *const ShaderDefines[NUM_SHADERS];

	void Flip();
	void SetInitialState();
	bool CreateResources();
	void ReleaseResources();
	bool LoadShaders();
	bool CreateFBTexture();
	bool CreatePaletteTexture();
	bool CreateVertexes();
	void UploadPalette();
	void CalcFullscreenCoords(FBVERTEX verts[4], bool viewarea_only, uint32_t color0, uint32_t color1) const;
	bool Reset();
	std::unique_ptr<HWTexture> CopyCurrentScreen();
	void ReleaseDefaultPoolItems();
	void KillNativePals();
	void KillNativeTexs();
	PackedTexture *AllocPackedTexture(int width, int height, bool wrapping, int format);
	void DrawPackedTextures(int packnum);
	void DrawLetterbox(int x, int y, int width, int height);
	void Draw3DPart(bool copy3d);
	bool SetStyle(OpenGLTex *tex, DrawParms &parms, uint32_t &color0, uint32_t &color1, BufferedTris &quad);
	static int GetStyleAlpha(int type);
	static void SetColorOverlay(uint32_t color, float alpha, uint32_t &color0, uint32_t &color1);
	void AddColorOnlyQuad(int left, int top, int width, int height, uint32_t color);
	void AddColorOnlyRect(int left, int top, int width, int height, uint32_t color);
	void CheckQuadBatch(int numtris = 2, int numverts = 4);
	void BeginQuadBatch();
	void EndQuadBatch();
	void BeginLineBatch();
	void EndLineBatch();
	void EndBatch();

	// State
	void EnableAlphaTest(bool enabled);
	void SetAlphaBlend(int op, int srcblend = 0, int destblend = 0);
	void SetConstant(int cnum, float r, float g, float b, float a);
	void SetPixelShader(HWPixelShader *shader);
	void SetTexture(int tnum, HWTexture *texture);
	void SetSamplerWrapS(int tnum, int mode);
	void SetSamplerWrapT(int tnum, int mode);
	void SetPaletteTexture(HWTexture *texture, int count, uint32_t border_color);

	bool Valid = false;
	std::shared_ptr<FGLDebug> Debug;

	std::unique_ptr<HWVertexBuffer> StreamVertexBuffer, StreamVertexBufferBurn;
	float ShaderConstants[NumPSCONST * 4];
	HWPixelShader *CurrentShader = nullptr;

	std::unique_ptr<HWFrameBuffer> OutputFB;

	bool AlphaTestEnabled = false;
	bool AlphaBlendEnabled = false;
	int AlphaBlendOp = 0;
	int AlphaSrcBlend = 0;
	int AlphaDestBlend = 0;
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
	float Gamma;
#ifdef _WIN32
	bool UpdatePending;
#endif // _WIN32
	bool NeedPalUpdate;
	bool NeedGammaUpdate;
	LTRBRect BlendingRect;
	int In2D;
	bool InScene;
	bool GatheringWipeScreen;
	bool AALines;
	uint8_t BlockNum;
	OpenGLPal *Palettes = nullptr;
	OpenGLTex *Textures = nullptr;
	Atlas *Atlases = nullptr;

	std::unique_ptr<HWTexture> FBTexture;
	std::unique_ptr<HWTexture> PaletteTexture;
	std::unique_ptr<HWTexture> ScreenshotTexture;

	std::unique_ptr<HWVertexBuffer> VertexBuffer;
	FBVERTEX *VertexData = nullptr;
	std::unique_ptr<HWIndexBuffer> IndexBuffer;
	uint16_t *IndexData = nullptr;
	BufferedTris *QuadExtra = nullptr;
	int VertexPos;
	int IndexPos;
	int QuadBatchPos;
	enum { BATCH_None, BATCH_Quads, BATCH_Lines } BatchType;

	std::unique_ptr<HWPixelShader> Shaders[NUM_SHADERS];

	std::unique_ptr<HWTexture> InitialWipeScreen, FinalWipeScreen;

	class Wiper
	{
	public:
		virtual ~Wiper();
		virtual bool Run(int ticks, OpenGLSWFrameBuffer *fb) = 0;

		void DrawScreen(OpenGLSWFrameBuffer *fb, HWTexture *tex, int blendop = 0, uint32_t color0 = 0, uint32_t color1 = 0xFFFFFFF);
	};

	class Wiper_Melt;			friend class Wiper_Melt;
	class Wiper_Burn;			friend class Wiper_Burn;
	class Wiper_Crossfade;		friend class Wiper_Crossfade;

	Wiper *ScreenWipe;
};


#endif //__GL_SWFRAMEBUFFER
