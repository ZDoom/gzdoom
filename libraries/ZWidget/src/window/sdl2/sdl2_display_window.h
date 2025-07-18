#pragma once

#include <list>
#include <unordered_map>
#include <zwidget/window/window.h>
#include <zwidget/window/sdl2nativehandle.h>
#include <SDL2/SDL.h>

class SDL2DisplayWindow : public DisplayWindow
{
public:
	SDL2DisplayWindow(DisplayWindowHost* windowHost, bool popupWindow, SDL2DisplayWindow* owner, RenderAPI renderAPI);
	~SDL2DisplayWindow();

	void SetWindowTitle(const std::string& text) override;
	void SetWindowFrame(const Rect& box) override;
	void SetClientFrame(const Rect& box) override;
	void Show() override;
	void ShowFullscreen() override;
	void ShowMaximized() override;
	void ShowMinimized() override;
	void ShowNormal() override;
	bool IsWindowFullscreen() override;
	void Hide() override;
	void Activate() override;
	void ShowCursor(bool enable) override;
	void LockCursor() override;
	void UnlockCursor() override;
	void CaptureMouse() override;
	void ReleaseMouseCapture() override;
	void Update() override;
	bool GetKeyState(InputKey key) override;
	void SetCursor(StandardCursor cursor) override;

	Rect GetWindowFrame() const override;
	Size GetClientSize() const override;
	int GetPixelWidth() const override;
	int GetPixelHeight() const override;
	double GetDpiScale() const override;

	void PresentBitmap(int width, int height, const uint32_t* pixels) override;

	void SetBorderColor(uint32_t bgra8) override;
	void SetCaptionColor(uint32_t bgra8) override;
	void SetCaptionTextColor(uint32_t bgra8) override;

	std::string GetClipboardText() override;
	void SetClipboardText(const std::string& text) override;

	Point MapFromGlobal(const Point& pos) const override;
	Point MapToGlobal(const Point& pos) const override;

	void* GetNativeHandle() override { return &Handle; }

	std::vector<std::string> GetVulkanInstanceExtensions() override;
	VkSurfaceKHR CreateVulkanSurface(VkInstance instance) override;

	static void DispatchEvent(const SDL_Event& event);
	static SDL2DisplayWindow* FindEventWindow(const SDL_Event& event);

	void OnWindowEvent(const SDL_WindowEvent& event);
	void OnTextInput(const SDL_TextInputEvent& event);
	void OnKeyUp(const SDL_KeyboardEvent& event);
	void OnKeyDown(const SDL_KeyboardEvent& event);
	void OnMouseButtonUp(const SDL_MouseButtonEvent& event);
	void OnMouseButtonDown(const SDL_MouseButtonEvent& event);
	void OnMouseWheel(const SDL_MouseWheelEvent& event);
	void OnMouseMotion(const SDL_MouseMotionEvent& event);
	void OnPaintEvent();

	InputKey GetMouseButtonKey(const SDL_MouseButtonEvent& event);

	static InputKey ScancodeToInputKey(SDL_Scancode keycode);
	static SDL_Scancode InputKeyToScancode(InputKey inputkey);

	template<typename T>
	Point GetMousePos(const T& event)
	{
		double uiscale = GetDpiScale();
		return Point(event.x / uiscale, event.y / uiscale);
	}

	static void ProcessEvents();
	static void RunLoop();
	static void ExitLoop();
	static Size GetScreenSize();

	static void* StartTimer(int timeoutMilliseconds, std::function<void()> onTimer);
	static void StopTimer(void* timerID);

	DisplayWindowHost* WindowHost = nullptr;
	SDL2NativeHandle Handle;
	SDL_Renderer* RendererHandle = nullptr;
	SDL_Texture* BackBufferTexture = nullptr;
	int BackBufferWidth = 0;
	int BackBufferHeight = 0;

	bool CursorLocked = false;
	bool isFullscreen = false;

	static bool ExitRunLoop;
	static Uint32 PaintEventNumber;
	static std::unordered_map<int, SDL2DisplayWindow*> WindowList;
};
