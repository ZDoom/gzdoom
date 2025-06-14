#pragma once

#include "wayland_display_backend.h"
#include "window/ztimer/ztimer.h"

#include <stdexcept>
#include <array>
#include <memory>
#include <sstream>

#include <algorithm>
#include <random>
#include <map>
#include <vector>

#include "zwidget/window/window.h"
#include "zwidget/window/waylandnativehandle.h"

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

template <typename R, typename T, typename... Args>
std::function<R(Args...)> bind_mem_fn(R(T::* func)(Args...), T *t)
{
  return [func, t] (Args... args)
    {
      return (t->*func)(args...);
    };
}

class SharedMemHelper
{
public:
    SharedMemHelper(size_t size)
        : len(size)
    {
        std::stringstream ss;
        std::random_device device;
        std::default_random_engine engine(device());
        std::uniform_int_distribution<unsigned int> distribution(0, std::numeric_limits<unsigned int>::max());
        ss << distribution(engine);
        name = ss.str();

        fd = memfd_create(name.c_str(), 0);
        if(fd < 0)
            throw std::runtime_error("shm_open failed.");

        if(ftruncate(fd, size) < 0)
            throw std::runtime_error("ftruncate failed.");

        mem = mmap(nullptr, len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        if(mem == MAP_FAILED) // NOLINT
            throw std::runtime_error("mmap failed with len " + std::to_string(len) + ".");
    }

    ~SharedMemHelper() noexcept
    {
        if(fd)
        {
            munmap(mem, len);
            close(fd);
            shm_unlink(name.c_str());
        }
    }

    int get_fd() const
    {
        return fd;
    }

    void *get_mem()
    {
        return mem;
    }

private:
    std::string name;
    int fd = 0;
    size_t len = 0;
    void *mem = nullptr;
};

class WaylandDisplayWindow : public DisplayWindow
{
public:
    WaylandDisplayWindow(WaylandDisplayBackend* backend, DisplayWindowHost* windowHost, bool popupWindow, WaylandDisplayWindow* owner, RenderAPI renderAPI);
    ~WaylandDisplayWindow();

    void SetWindowTitle(const std::string& text) override;
	void SetWindowFrame(const Rect& box) override;
	void SetClientFrame(const Rect& box) override;
	void Show() override;
	void ShowFullscreen() override;
	void ShowMaximized() override;
	void ShowMinimized() override;
	void ShowNormal() override;
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

	void* GetNativeHandle() override { return (void*)&m_NativeHandle; }

	std::vector<std::string> GetVulkanInstanceExtensions() override;
	VkSurfaceKHR CreateVulkanSurface(VkInstance instance) override;

	wayland::surface_t GetWindowSurface() { return m_AppSurface; }

	bool IsWindowFullscreen() override;

private:
    // Event handlers as otherwise linking DisplayWindowHost On...() functions with Wayland events directly crashes the app
    // Alternatively to avoid crashes one can capture by value ([=]) instead of reference ([&])
    void OnXDGToplevelConfigureEvent(int32_t width, int32_t height);
    void OnExportHandleEvent(std::string exportedHandle);
    void OnExitEvent();

    void DrawSurface(uint32_t serial = 0);

    void InitializeToplevel();
    void InitializePopup();

    WaylandDisplayBackend* backend = nullptr;
    WaylandDisplayWindow* m_owner = nullptr;
    DisplayWindowHost* windowHost = nullptr;
    bool m_PopupWindow = false;

    bool m_NeedsUpdate = true;

    Point m_WindowGlobalPos = Point(0, 0);
    Size m_WindowSize = Size(0, 0);
    double m_ScaleFactor = 1.0;

    Point m_SurfaceMousePos = Point(0, 0);

    WaylandNativeHandle m_NativeHandle;
    RenderAPI m_renderAPI;

    wayland::data_device_t m_DataDevice;
    wayland::data_source_t m_DataSource;

    wayland::zxdg_toplevel_decoration_v1_t m_XDGToplevelDecoration;

    wayland::fractional_scale_v1_t m_FractionalScale;

    wayland::surface_t m_AppSurface;
    wayland::buffer_t m_AppSurfaceBuffer;

    wayland::xdg_surface_t m_XDGSurface;
    wayland::xdg_toplevel_t m_XDGToplevel;
    wayland::xdg_popup_t m_XDGPopup;

    wayland::zxdg_exported_v2_t m_XDGExported;

    wayland::zwp_locked_pointer_v1_t m_LockedPointer;
    wayland::zwp_confined_pointer_v1_t m_ConfinedPointer;

    wayland::callback_t m_FrameCallback;

    std::string m_windowID;
    std::string m_ClipboardContents;

    std::shared_ptr<SharedMemHelper> shared_mem;

    bool isFullscreen = false;

    // Helper functions
    void CreateBuffers(int32_t width, int32_t height);
    std::string GetWaylandWindowID();

    friend WaylandDisplayBackend;
};
