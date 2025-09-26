#include "wayland_display_window.h"

#include <cstring>
#include <dlfcn.h>

#include "core/image.h"

WaylandDisplayWindow::WaylandDisplayWindow(WaylandDisplayBackend* backend, DisplayWindowHost* windowHost, bool popupWindow, WaylandDisplayWindow* owner, RenderAPI renderAPI)
	: backend(backend), m_owner(owner), windowHost(windowHost), m_PopupWindow(popupWindow), m_renderAPI(renderAPI)
{
	m_AppSurface = backend->m_waylandCompositor.create_surface();

	m_NativeHandle.display = backend->s_waylandDisplay;
	m_NativeHandle.surface = m_AppSurface;

	if (backend->m_FractionalScaleManager)
	{
		m_FractionalScale = backend->m_FractionalScaleManager.get_fractional_scale(m_AppSurface);

		m_FractionalScale.on_preferred_scale() = [&](uint32_t scale_numerator) {
			// parameter is the numerator of a fraction with the denominator of 120
			m_ScaleFactor = scale_numerator / 120.0;

			m_NeedsUpdate = true;
			windowHost->OnWindowDpiScaleChanged();
		};
	}

	m_XDGSurface = backend->m_XDGWMBase.get_xdg_surface(m_AppSurface);
	m_XDGSurface.on_configure() = [&] (uint32_t serial) {
		m_XDGSurface.ack_configure(serial);
	};

	if (popupWindow)
	{
		InitializePopup();
	}
	else
	{
		InitializeToplevel();
	}

	backend->OnWindowCreated(this);

	this->DrawSurface();
}

WaylandDisplayWindow::~WaylandDisplayWindow()
{
	backend->OnWindowDestroyed(this);
}

void WaylandDisplayWindow::InitializeToplevel()
{
	m_XDGToplevel = m_XDGSurface.get_toplevel();
	m_XDGToplevel.set_title("ZWidget Window");

	if (m_owner)
		m_XDGToplevel.set_parent(m_owner->m_XDGToplevel);

	if (backend->m_XDGDecorationManager)
	{
		// Force server side decorations if possible
		m_XDGToplevelDecoration = backend->m_XDGDecorationManager.get_toplevel_decoration(m_XDGToplevel);
		m_XDGToplevelDecoration.set_mode(wayland::zxdg_toplevel_decoration_v1_mode::server_side);
	}

	m_AppSurface.commit();

	backend->s_waylandDisplay.roundtrip();

	// These have to be added after the roundtrip
	m_XDGToplevel.on_configure() = [&] (int32_t width, int32_t height, wayland::array_t states) {
		OnXDGToplevelConfigureEvent(width, height);
	};

	m_XDGToplevel.on_close() = [&] () {
		OnExitEvent();
	};

	m_XDGToplevel.on_configure_bounds() = [this] (int32_t width, int32_t height)
	{

	};

	m_XDGExported = backend->m_XDGExporter.export_toplevel(m_AppSurface);

	m_XDGExported.on_handle() = [&] (std::string handleStr) {
		OnExportHandleEvent(handleStr);
	};
}

void WaylandDisplayWindow::InitializePopup()
{
	if (!m_owner)
		throw std::runtime_error("Popup window must have an owner!");

	wayland::xdg_positioner_t popupPositioner = backend->m_XDGWMBase.create_positioner();

	popupPositioner.set_anchor(wayland::xdg_positioner_anchor::bottom);
	popupPositioner.set_anchor_rect(0, 0, 1, 30);
	popupPositioner.set_size(1, 1);

	m_XDGPopup = m_XDGSurface.get_popup(m_owner->m_XDGSurface, popupPositioner);

	m_XDGPopup.on_configure() = [&] (int32_t x, int32_t y, int32_t width, int32_t height) {
		SetClientFrame(Rect::xywh(x, y, width, height));
	};

	//m_XDGPopup.on_repositioned()

	m_XDGPopup.on_popup_done() = [&] () {
		OnExitEvent();
	};

	m_AppSurface.commit();

	backend->s_waylandDisplay.roundtrip();
}


void WaylandDisplayWindow::SetWindowTitle(const std::string& text)
{
	if (m_XDGToplevel)
		m_XDGToplevel.set_title(text);
}

void WaylandDisplayWindow::SetWindowIcon(const std::vector<std::shared_ptr<Image>>& images)
{
	if (backend->m_XDGToplevelIconManager)
	{
		if (images.empty())
		{
			backend->m_XDGToplevelIconManager.set_icon(m_XDGToplevel, nullptr);
			return;
		}

		m_XDGToplevelIcon = backend->m_XDGToplevelIconManager.create_icon();

		CreateAppIconBuffers(images);

		for (auto& icon_buffer : appIconBuffers)
		{
			m_XDGToplevelIcon.add_buffer(icon_buffer, 1);
		}

		backend->m_XDGToplevelIconManager.set_icon(m_XDGToplevel, m_XDGToplevelIcon);
	}
}

void WaylandDisplayWindow::SetWindowFrame(const Rect& box)
{
	// Resizing will be shown on the next commit
	CreateBuffers(box.width, box.height);
	windowHost->OnWindowGeometryChanged();
	m_NeedsUpdate = true;
	m_AppSurface.commit();
}

void WaylandDisplayWindow::SetClientFrame(const Rect& box)
{
	SetWindowFrame(box);
}

void WaylandDisplayWindow::Show()
{
	m_AppSurface.attach(m_AppSurfaceBuffer, 0, 0);
	m_AppSurface.damage(0, 0, m_WindowSize.width, m_WindowSize.height);
	m_AppSurface.commit();
}

void WaylandDisplayWindow::ShowFullscreen()
{
	if (m_XDGToplevel)
	{
		m_XDGToplevel.set_fullscreen(backend->m_waylandOutput);
		isFullscreen = true;
	}
}

void WaylandDisplayWindow::ShowMaximized()
{
	if (m_XDGToplevel)
		m_XDGToplevel.set_maximized();
}

void WaylandDisplayWindow::ShowMinimized()
{
	if (m_XDGToplevel)
		m_XDGToplevel.set_minimized();
}

void WaylandDisplayWindow::ShowNormal()
{
	if (m_XDGToplevel)
		m_XDGToplevel.unset_fullscreen();
}

bool WaylandDisplayWindow::IsWindowFullscreen()
{
	return isFullscreen;
}

void WaylandDisplayWindow::Hide()
{
	// Apparently this is how hiding a window works
	// By attaching a null buffer to the surface
	// See: https://lists.freedesktop.org/archives/wayland-devel/2017-November/035963.html
	m_AppSurface.attach(nullptr, 0, 0);
	m_AppSurface.commit();
}

void WaylandDisplayWindow::Activate()
{
	// To do: this needs to be in the backend instance if all windows share the activation token
	wayland::xdg_activation_token_v1_t xdgActivationToken = backend->m_XDGActivation.get_activation_token();

	std::string tokenString;

	xdgActivationToken.on_done() = [&tokenString] (std::string obtainedString) {
		tokenString = obtainedString;
	};

	xdgActivationToken.set_surface(m_AppSurface);
	xdgActivationToken.commit();  // This will set our token string

	backend->m_XDGActivation.activate(tokenString, m_AppSurface);
	backend->m_FocusWindow = this;
	backend->m_MouseFocusWindow = this;
	windowHost->OnWindowActivated();
}

void WaylandDisplayWindow::ShowCursor(bool enable)
{
	backend->ShowCursor(enable);
}

void WaylandDisplayWindow::LockCursor()
{
	m_LockedPointer = backend->m_PointerConstraints.lock_pointer(m_AppSurface, backend->m_waylandPointer, nullptr, wayland::zwp_pointer_constraints_v1_lifetime::persistent);
	backend->SetMouseLocked(true);
	ShowCursor(false);
}

void WaylandDisplayWindow::UnlockCursor()
{
	if (m_LockedPointer)
		m_LockedPointer.proxy_release();
	backend->SetMouseLocked(false);
	ShowCursor(true);
}

void WaylandDisplayWindow::CaptureMouse()
{
	m_ConfinedPointer = backend->m_PointerConstraints.confine_pointer(GetWindowSurface(), backend->m_waylandPointer, nullptr, wayland::zwp_pointer_constraints_v1_lifetime::persistent);
	ShowCursor(false);
}

void WaylandDisplayWindow::ReleaseMouseCapture()
{
	if (m_ConfinedPointer)
		m_ConfinedPointer.proxy_release();
	ShowCursor(true);
}

void WaylandDisplayWindow::Update()
{
	m_NeedsUpdate = true;
}

bool WaylandDisplayWindow::GetKeyState(InputKey key)
{
	return backend->GetKeyState(key);
}

void WaylandDisplayWindow::SetCursor(StandardCursor cursor)
{
	backend->SetCursor(cursor);
}

Rect WaylandDisplayWindow::GetWindowFrame() const
{
	return Rect(m_WindowGlobalPos.x, m_WindowGlobalPos.y, m_WindowSize.width, m_WindowSize.height);
}

Size WaylandDisplayWindow::GetClientSize() const
{
	return m_WindowSize / m_ScaleFactor;
}

int WaylandDisplayWindow::GetPixelWidth() const
{
	return m_WindowSize.width;
}

int WaylandDisplayWindow::GetPixelHeight() const
{
	return m_WindowSize.height;
}

double WaylandDisplayWindow::GetDpiScale() const
{
	return m_ScaleFactor;
}

void WaylandDisplayWindow::PresentBitmap(int width, int height, const uint32_t* pixels)
{
	// Make new buffers if the sizes don't match
	if (width != m_WindowSize.width || height != m_WindowSize.height)
		CreateBuffers(width, height);

	std::memcpy(shared_mem->get_mem(), (void*)pixels, width * height * 4);
}

void WaylandDisplayWindow::SetBorderColor(uint32_t bgra8)
{

}

void WaylandDisplayWindow::SetCaptionColor(uint32_t bgra8)
{

}

void WaylandDisplayWindow::SetCaptionTextColor(uint32_t bgra8)
{

}

std::string WaylandDisplayWindow::GetClipboardText()
{
	return backend->GetClipboardText();
}

void WaylandDisplayWindow::SetClipboardText(const std::string& text)
{
	m_DataSource = backend->m_DataDeviceManager.create_data_source();

	m_DataSource.on_send() = [text] (const std::string& mime_type, const int fd)
	{
		write(fd, text.c_str(), text.size());
		close(fd);
	};

	m_DataSource.offer("text/plain");
	backend->GetDataDevice().set_selection(m_DataSource, backend->GetKeyboardSerial());
}

Point WaylandDisplayWindow::MapFromGlobal(const Point& pos) const
{
	return (pos - m_WindowGlobalPos) / m_ScaleFactor;
}

Point WaylandDisplayWindow::MapToGlobal(const Point& pos) const
{
	return (m_WindowGlobalPos + pos) / m_ScaleFactor;
}

void WaylandDisplayWindow::OnXDGToplevelConfigureEvent(int32_t width, int32_t height)
{
	Rect rect = GetWindowFrame();
	rect.width = width / m_ScaleFactor;
	rect.height = height / m_ScaleFactor;
	SetWindowFrame(rect);
	windowHost->OnWindowGeometryChanged();
}

void WaylandDisplayWindow::OnExportHandleEvent(std::string exportedHandle)
{
	m_windowID = exportedHandle;
}

void WaylandDisplayWindow::OnExitEvent()
{
	windowHost->OnWindowClose();
}

void WaylandDisplayWindow::DrawSurface(uint32_t serial)
{
	m_AppSurface.attach(m_AppSurfaceBuffer, 0, 0);
	m_AppSurface.damage(0, 0, m_WindowSize.width, m_WindowSize.height);

	if (m_renderAPI == RenderAPI::Unspecified || m_renderAPI == RenderAPI::Bitmap)
	{
		m_FrameCallback = m_AppSurface.frame();

		m_FrameCallback.on_done() = bind_mem_fn(&WaylandDisplayWindow::DrawSurface, this);
	}
	m_AppSurface.commit();
}

void WaylandDisplayWindow::CreateBuffers(int32_t width, int32_t height)
{
	if (width == 0 || height == 0)
		return;

	if (shared_mem)
		shared_mem.reset();

	int scaled_width = width * m_ScaleFactor;
	int scaled_height  = height * m_ScaleFactor;

	shared_mem = std::make_shared<SharedMemHelper>(scaled_width * scaled_height * 4);
	auto pool = backend->m_waylandSHM.create_pool(shared_mem->get_fd(), scaled_width * scaled_height * 4);

	m_AppSurfaceBuffer = pool.create_buffer(0, scaled_width, scaled_height, scaled_width * 4, wayland::shm_format::xrgb8888);

	m_WindowSize = Size(scaled_width, scaled_height);
}

void WaylandDisplayWindow::CreateAppIconBuffers(const std::vector<std::shared_ptr<Image>>& images)
{
	if (images.empty())
		return;

	appIconBuffers.clear();
	appIconSharedMems.clear();

	for (auto& image: images)
	{
		const int image_size = image->GetWidth() * image->GetHeight() * 4;
		const int stride = image->GetWidth() * 4;

		auto new_shared_mem = std::make_shared<SharedMemHelper>(image_size);

		auto pool = backend->m_waylandSHM.create_pool(new_shared_mem->get_fd(), image_size);

		auto new_buffer = pool.create_buffer(0, image->GetWidth(), image->GetHeight(), stride, wayland::shm_format::argb8888);

		// Fill the buffer with the icon data
		std::memcpy(new_shared_mem->get_mem(), image->GetData(), image_size);

		appIconSharedMems.push_back(new_shared_mem);
		appIconBuffers.push_back(new_buffer);
	}
}

std::string WaylandDisplayWindow::GetWaylandWindowID()
{
	return m_windowID;
}

// This is to avoid needing all the Vulkan headers and the volk binding library just for this:
#ifndef VK_VERSION_1_0

#define VKAPI_CALL
#define VKAPI_PTR VKAPI_CALL

typedef uint32_t VkFlags;
typedef enum VkStructureType { VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR = 1000006000, VK_OBJECT_TYPE_MAX_ENUM = 0x7FFFFFFF } VkStructureType;
typedef enum VkResult { VK_SUCCESS = 0, VK_RESULT_MAX_ENUM = 0x7FFFFFFF } VkResult;
typedef struct VkAllocationCallbacks VkAllocationCallbacks;

typedef void (VKAPI_PTR* PFN_vkVoidFunction)(void);
typedef PFN_vkVoidFunction(VKAPI_PTR* PFN_vkGetInstanceProcAddr)(VkInstance instance, const char* pName);

#ifndef VK_KHR_wayland_surface

typedef VkFlags VkWaylandSurfaceCreateFlagsKHR;
typedef struct VkWaylandSurfaceCreateInfoKHR
{
	VkStructureType				   sType;
	const void*					   pNext;
	VkWaylandSurfaceCreateFlagsKHR	flags;
	struct wl_display*				display;
	struct wl_surface*				surface;
} VkWaylandSurfaceCreateInfoKHR;

typedef VkResult(VKAPI_PTR* PFN_vkCreateWaylandSurfaceKHR)(VkInstance instance, const VkWaylandSurfaceCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSurfaceKHR* pSurface);

#endif
#endif

class ZWidgetWaylandVulkanLoader
{
public:
	ZWidgetWaylandVulkanLoader()
	{
#if defined(__APPLE__)
		module = dlopen("libvulkan.dylib", RTLD_NOW | RTLD_LOCAL);
		if (!module)
			module = dlopen("libvulkan.1.dylib", RTLD_NOW | RTLD_LOCAL);
		if (!module)
			module = dlopen("libMoltenVK.dylib", RTLD_NOW | RTLD_LOCAL);
#else
		module = dlopen("libvulkan.so.1", RTLD_NOW | RTLD_LOCAL);
		if (!module)
			module = dlopen("libvulkan.so", RTLD_NOW | RTLD_LOCAL);
#endif

		if (!module)
			throw std::runtime_error("Could not load vulkan");

		vkGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr)dlsym(module, "vkGetInstanceProcAddr");
		if (!vkGetInstanceProcAddr)
		{
			dlclose(module);
			throw std::runtime_error("vkGetInstanceProcAddr not found");
		}
	}

	~ZWidgetWaylandVulkanLoader()
	{
		dlclose(module);
	}

	PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr = nullptr;
	void* module = nullptr;
};

VkSurfaceKHR WaylandDisplayWindow::CreateVulkanSurface(VkInstance instance)
{
	static ZWidgetWaylandVulkanLoader loader;

	auto vkCreateWaylandSurfaceKHR = (PFN_vkCreateWaylandSurfaceKHR)loader.vkGetInstanceProcAddr(instance, "vkCreateWaylandSurfaceKHR");
	if (!vkCreateWaylandSurfaceKHR)
		throw std::runtime_error("Could not create vulkan surface");

	VkWaylandSurfaceCreateInfoKHR createInfo = { VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR };
	createInfo.display = m_NativeHandle.display;
	createInfo.surface = m_NativeHandle.surface;

	VkSurfaceKHR surface = {};
	VkResult result = vkCreateWaylandSurfaceKHR(instance, &createInfo, nullptr, &surface);
	if (result != VK_SUCCESS)
		throw std::runtime_error("Could not create vulkan surface");
	return surface;
}

std::vector<std::string> WaylandDisplayWindow::GetVulkanInstanceExtensions()
{
	return { "VK_KHR_surface", "VK_KHR_wayland_surface" };
}
