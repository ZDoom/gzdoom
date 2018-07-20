#ifndef __GL_FRAMEBUFFER
#define __GL_FRAMEBUFFER

#include "gl_sysfb.h"

#include <memory>

class FHardwareTexture;
class FSimpleVertexBuffer;
class FGLDebug;

class OpenGLFrameBuffer : public SystemGLFrameBuffer
{
	typedef SystemGLFrameBuffer Super;

public:

	explicit OpenGLFrameBuffer() {}
	OpenGLFrameBuffer(void *hMonitor, bool fullscreen) ;
	~OpenGLFrameBuffer();

	void InitializeState();
	void Update();

	// Color correction
	void SetGamma();

	void CleanForRestart() override;
	void UpdatePalette() override;
	void InitForLevel() override;
	void SetClearColor(int color) override;
	uint32_t GetCaps() override;
	void RenderTextureView(FCanvasTexture *tex, AActor *Viewpoint, double FOV) override;
	void WriteSavePic(player_t *player, FileWriter *file, int width, int height) override;
	sector_t *RenderView(player_t *player) override;
	void SetTextureFilterMode() override;
	IHardwareTexture *CreateHardwareTexture(FTexture *tex) override;
	void PrecacheMaterial(FMaterial *mat, int translation) override;
	FModelRenderer *CreateModelRenderer(int mli) override;
	void TextureFilterChanged() override;
	void BeginFrame() override;
	void SetViewportRects(IntRect *bounds) override;
	void BlurScene(float amount) override;
    IUniformBuffer *CreateUniformBuffer(size_t size, bool staticuse = false) override;
	IShaderProgram *CreateShaderProgram() override;


	// Retrieves a buffer containing image data for a screenshot.
	// Hint: Pitch can be negative for upside-down images, in which case buffer
	// points to the last row in the buffer, which will be the first row output.
	virtual void GetScreenshotBuffer(const uint8_t *&buffer, int &pitch, ESSType &color_type, float &gamma) override;

	bool WipeStartScreen(int type);
	void WipeEndScreen();
	bool WipeDo(int ticks);
	void WipeCleanup();
	void Swap();
	bool IsHWGammaActive() const { return HWGammaActive; }

	void SetVSync(bool vsync);

	void Draw2D() override;

	bool HWGammaActive = false;			// Are we using hardware or software gamma?
	std::shared_ptr<FGLDebug> mDebug;	// Debug API
private:
	int camtexcount = 0;

	class Wiper
	{

	protected:
		FSimpleVertexBuffer *mVertexBuf;

		void MakeVBO(OpenGLFrameBuffer *fb);

	public:
		Wiper();
		virtual ~Wiper();
		virtual bool Run(int ticks, OpenGLFrameBuffer *fb) = 0;
	};

	class Wiper_Melt;			friend class Wiper_Melt;
	class Wiper_Burn;			friend class Wiper_Burn;
	class Wiper_Crossfade;		friend class Wiper_Crossfade;

	Wiper *ScreenWipe;
	FHardwareTexture *wipestartscreen;
	FHardwareTexture *wipeendscreen;


public:
};


#endif //__GL_FRAMEBUFFER
