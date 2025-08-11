#include "wayland_display_backend.h"
#include "wayland_display_window.h"

#ifdef USE_DBUS
#include "window/dbus/dbus_open_file_dialog.h"
#include "window/dbus/dbus_save_file_dialog.h"
#include "window/dbus/dbus_open_folder_dialog.h"
#endif

WaylandDisplayBackend::WaylandDisplayBackend()
{
    if (!s_waylandDisplay)
        throw std::runtime_error("Wayland Display initialization failed!");

    s_waylandRegistry = s_waylandDisplay.get_registry();

    s_waylandRegistry.on_global() = [&](uint32_t name, std::string interface, uint32_t version) {
        if (interface == wayland::compositor_t::interface_name)
            s_waylandRegistry.bind(name, m_waylandCompositor, 3);
        if (interface == wayland::shm_t::interface_name)
            s_waylandRegistry.bind(name, m_waylandSHM, version);
        if (interface == wayland::output_t::interface_name)
            s_waylandRegistry.bind(name, m_waylandOutput, version);
        if (interface == wayland::seat_t::interface_name)
            s_waylandRegistry.bind(name, m_waylandSeat, 8);
        if (interface == wayland::data_device_manager_t::interface_name)
            s_waylandRegistry.bind(name, m_DataDeviceManager, 3);
        if (interface == wayland::xdg_wm_base_t::interface_name)
            s_waylandRegistry.bind(name, m_XDGWMBase, 4);
        if (interface == wayland::zxdg_output_manager_v1_t::interface_name)
            s_waylandRegistry.bind(name, m_XDGOutputManager, version);
        if (interface == wayland::zxdg_exporter_v2_t::interface_name)
            s_waylandRegistry.bind(name, m_XDGExporter, 1);
        if (interface == wayland::zwp_pointer_constraints_v1_t::interface_name)
            s_waylandRegistry.bind(name, m_PointerConstraints, 1);
        if (interface == wayland::xdg_activation_v1_t::interface_name)
            s_waylandRegistry.bind(name, m_XDGActivation, 1);
        if (interface == wayland::zxdg_decoration_manager_v1_t::interface_name)
            s_waylandRegistry.bind(name, m_XDGDecorationManager, 1);
        if (interface == wayland::fractional_scale_manager_v1_t::interface_name)
            s_waylandRegistry.bind(name, m_FractionalScaleManager, 1);
        if (interface == wayland::zwp_relative_pointer_manager_v1_t::interface_name)
            s_waylandRegistry.bind(name, m_RelativePointerManager, 1);
    };

    s_waylandDisplay.roundtrip();

    if (!m_XDGWMBase)
        throw std::runtime_error("WaylandDisplayBackend: XDG-Shell is required!");

    if (!m_XDGOutputManager)
        throw std::runtime_error("WaylandDisplayBackend: xdg-output-manager-v1 is required!");

    if (!m_XDGExporter)
        throw std::runtime_error("WaylandDisplayBackend: xdg-foreign-unstable-v2 is required!");

    if (!m_PointerConstraints)
        throw std::runtime_error("WaylandDisplayBackend: pointer-constrains-unstable-v1 is required!");

    if (!m_RelativePointerManager)
        throw std::runtime_error("WaylandDisplayBackend: relative-pointer-unstable-v1 is required!");

    m_waylandOutput.on_mode() = [this] (wayland::output_mode flags, int32_t width, int32_t height, int32_t refresh) {
        s_ScreenSize = Size(width, height);
    };

    m_XDGWMBase.on_ping() = [this] (uint32_t serial) {
        m_XDGWMBase.pong(serial);
    };

    m_waylandSeat.on_capabilities() = [this] (uint32_t capabilities) {
        hasKeyboard = capabilities & wayland::seat_capability::keyboard;
        hasPointer = capabilities & wayland::seat_capability::pointer;
    };

    m_XDGOutput = m_XDGOutputManager.get_xdg_output(m_waylandOutput);

    m_XDGOutput.on_logical_position() = [this] (int32_t x, int32_t y) {
        //m_WindowGlobalPos = Point(x, y);
    };

    m_XDGOutput.on_logical_size() = [this] (int32_t width, int32_t height) {
        s_ScreenSize = Size(width, height);
    };

    if (!m_FractionalScaleManager)
	{
        m_waylandOutput.on_scale() = [this] (int32_t scale) {
			for (WaylandDisplayWindow* w : s_Windows)
			{
				w->m_ScaleFactor = scale;
				w->m_NeedsUpdate = true;
				w->windowHost->OnWindowDpiScaleChanged();
			}
        };
	}

    s_waylandDisplay.roundtrip();

	// To do: this shouldn't really be fatal. The user might have forgotten to plug in their keyboard or mouse.
    if (!hasKeyboard)
        throw std::runtime_error("No keyboard detected!");
    if (!hasPointer)
        throw std::runtime_error("No pointer device detected!");

    m_waylandKeyboard = m_waylandSeat.get_keyboard();
    m_waylandPointer = m_waylandSeat.get_pointer();
    m_RelativePointer = m_RelativePointerManager.get_relative_pointer(m_waylandPointer);

	ConnectDeviceEvents();

    m_cursorSurface = m_waylandCompositor.create_surface();
    SetCursor(StandardCursor::arrow);

/*
    m_DataDevice = m_DataDeviceManager.get_data_device(m_waylandSeat);

    m_DataSource = m_DataDeviceManager.create_data_source();
    m_DataSource.offer("text/plain");

    m_DataSource.on_send() = [&] (std::string mime_type, int fd) {
        if (mime_type != "text/plain")
            return;

        if (!m_ClipboardContents.empty())
            write(fd, m_ClipboardContents.data(), m_ClipboardContents.size());
        close(fd);
    };

    m_DataDevice.on_selection() = [&] (wayland::data_offer_t dataOffer) {
        m_ClipboardContents.clear();

        if (!dataOffer)
            // Clipboard is empty
            return;

        int fds[2];
        pipe(fds);

        dataOffer.receive("text/plain", fds[1]);
        close(fds[1]);

        m_waylandDisplay.roundtrip();

        while (true)
        {
            char buf[1024];

            ssize_t n = read(fds[0], buf, sizeof(buf));

            if (n <= 0)
                break;

            m_ClipboardContents += buf;
        }

        close(fds[0]);

        dataOffer.proxy_release();
    };
*/
}

WaylandDisplayBackend::~WaylandDisplayBackend()
{
    if (m_KeymapContext)
        xkb_context_unref(m_KeymapContext);

    if (m_KeyboardState)
        xkb_state_unref(m_KeyboardState);
}

void WaylandDisplayBackend::ConnectDeviceEvents()
{
    m_KeymapContext = xkb_context_new(XKB_CONTEXT_NO_FLAGS);

    m_waylandKeyboard.on_keymap() = [this] (wayland::keyboard_keymap_format format, int fd, uint32_t size) {
        if (format != wayland::keyboard_keymap_format::xkb_v1)
            throw std::runtime_error("WaylandDisplayBackend: Unrecognized keymap format!");

        char* mapSHM = (char*)mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0);
        if (mapSHM == MAP_FAILED)
            throw std::runtime_error("WaylandDisplayBackend: Keymap shared memory allocation failed!");

        if (m_Keymap)
            xkb_keymap_unref(m_Keymap);

        m_Keymap = xkb_keymap_new_from_string(m_KeymapContext, mapSHM, XKB_KEYMAP_FORMAT_TEXT_V1, XKB_KEYMAP_COMPILE_NO_FLAGS);
        munmap(mapSHM, size);
        close(fd);

        if (m_KeyboardState)
            xkb_state_unref(m_KeyboardState);

        m_KeyboardState = xkb_state_new(m_Keymap);
    };

    m_waylandKeyboard.on_modifiers() = [this] (uint32_t serial, uint32_t mods_depressed, uint32_t mods_latched, uint32_t mods_locked, uint32_t group) {
        xkb_state_update_mask(m_KeyboardState, mods_depressed, mods_latched, mods_locked, 0, 0, group);
    };

    m_waylandKeyboard.on_enter() = [this] (uint32_t serial, wayland::surface_t surfaceEntered, wayland::array_t keys) {
        std::vector<uint32_t> keysVec = keys;

        m_KeyboardSerial = serial;

		// Find the window to focus on by checking the surface window owns.
        if (!m_FocusWindow || m_FocusWindow->GetWindowSurface() != surfaceEntered)
        {
            for (auto win: s_Windows)
            {
                if (win->GetWindowSurface() == surfaceEntered)
                    m_FocusWindow = win;
            }
        }


        for (auto key: keysVec)
        {
            // keys parameter represents the keys pressed when entering the surface
            // key variable is Linux evdev scancode, to translate it to XKB keycode, we must add 8
            xkb_keysym_t sym = xkb_state_key_get_one_sym(m_KeyboardState, key + 8);
            OnKeyboardKeyEvent(sym, wayland::keyboard_key_state::pressed);

            // Also cause a Char event
            char buf[128];
            xkb_state_key_get_utf8(m_KeyboardState, key + 8, buf, sizeof(buf));

            OnKeyboardCharEvent(buf, wayland::keyboard_key_state::pressed);
        }
    };

    m_waylandKeyboard.on_key() = [this] (uint32_t serial, uint32_t time, uint32_t key, wayland::keyboard_key_state state) {
        // key is Linux evdev scancode, to translate it to XKB keycode, we must add 8
        xkb_keysym_t sym = xkb_state_key_get_one_sym(m_KeyboardState, key + 8);
        OnKeyboardKeyEvent(sym, state);

        // Also cause a Char event
        char buf[128];
        xkb_state_key_get_utf8(m_KeyboardState, key + 8, buf, sizeof(buf));

        OnKeyboardCharEvent(buf, state);

        //m_DataDevice.set_selection(m_DataSource, m_KeyboardSerial);
    };

    m_waylandPointer.on_enter() = [this](uint32_t serial, wayland::surface_t surfaceEntered, double surfaceX, double surfaceY) {
		// Find the window to focus on by checking the surface window owns.
        if (!m_MouseFocusWindow || m_MouseFocusWindow->GetWindowSurface() != surfaceEntered)
        {
            for (auto win: s_Windows)
            {
                if (win->GetWindowSurface() == surfaceEntered)
                {
                    m_MouseFocusWindow = win;
                }
            }
        }

		currentPointerEvent.event_mask |= POINTER_EVENT_ENTER;
		currentPointerEvent.serial = serial;
        currentPointerEvent.surfaceX = surfaceX;
        currentPointerEvent.surfaceY = surfaceY;
    };

    m_waylandPointer.on_leave() = [this](uint32_t serial, wayland::surface_t surfaceLeft) {
        currentPointerEvent.event_mask |= POINTER_EVENT_LEAVE;
        currentPointerEvent.serial = serial;
    };

    m_waylandPointer.on_motion() = [this] (uint32_t serial, double surfaceX, double surfaceY) {
        currentPointerEvent.event_mask |= POINTER_EVENT_MOTION;
        currentPointerEvent.serial = serial;
        currentPointerEvent.surfaceX = surfaceX;
        currentPointerEvent.surfaceY = surfaceY;
    };

    m_waylandPointer.on_button() = [this] (uint32_t serial, uint32_t time, uint32_t button, wayland::pointer_button_state state) {
        currentPointerEvent.event_mask |= POINTER_EVENT_BUTTON;
        currentPointerEvent.serial = serial;
        currentPointerEvent.time = time;
        currentPointerEvent.button = button;
        currentPointerEvent.state = state;
    };

    m_waylandPointer.on_axis() = [this] (uint32_t serial, wayland::pointer_axis axis, double value) {
        currentPointerEvent.event_mask |= POINTER_EVENT_AXIS;
        currentPointerEvent.serial = serial;
        currentPointerEvent.axes[uint32_t(axis)].value = value;
        currentPointerEvent.axes[uint32_t(axis)].valid = true;
    };

    // High resolution scroll event
    m_waylandPointer.on_axis_value120() = [this] (wayland::pointer_axis axis, int32_t value) {
        currentPointerEvent.event_mask |= POINTER_EVENT_AXIS_120;
        currentPointerEvent.axes[uint32_t(axis)].valid = true;
        currentPointerEvent.axes[uint32_t(axis)].value120 = value;
    };

    m_waylandPointer.on_axis_source() = [this] (wayland::pointer_axis_source axis) {
        currentPointerEvent.event_mask |= POINTER_EVENT_AXIS_SOURCE;
        currentPointerEvent.axis_source = axis;
    };

    m_waylandPointer.on_axis_stop() = [this] (uint32_t time, wayland::pointer_axis axis) {
        currentPointerEvent.event_mask |= POINTER_EVENT_AXIS_STOP;
        currentPointerEvent.time = time;
        currentPointerEvent.axes[uint32_t(axis)].valid = true;
    };

    m_RelativePointer.on_relative_motion() = [this] (uint32_t utime_hi, uint32_t utime_lo,
                                                     double dx, double dy,
                                                     double dx_unaccel, double dy_unaccel) {
        currentPointerEvent.event_mask |= POINTER_EVENT_RELATIVE_MOTION;
        currentPointerEvent.time = utime_lo;
        currentPointerEvent.dx = dx;
        currentPointerEvent.dy = dy;
    };

    m_waylandPointer.on_frame() = [this] () {
        if (currentPointerEvent.event_mask & POINTER_EVENT_ENTER)
            OnMouseEnterEvent(currentPointerEvent.serial);

        if (currentPointerEvent.event_mask & POINTER_EVENT_LEAVE)
            OnMouseLeaveEvent();

        if (currentPointerEvent.event_mask & POINTER_EVENT_MOTION)
        {
            OnMouseMoveEvent(Point(currentPointerEvent.surfaceX, currentPointerEvent.surfaceY));
        }

        if (currentPointerEvent.event_mask & POINTER_EVENT_RELATIVE_MOTION)
        {
            if (hasMouseLock)
                OnMouseMoveRawEvent(int(currentPointerEvent.dx), int(currentPointerEvent.dy));
        }

        if (currentPointerEvent.event_mask & POINTER_EVENT_BUTTON)
        {
            InputKey ikey = LinuxInputEventCodeToInputKey(currentPointerEvent.button);
            if (currentPointerEvent.state == wayland::pointer_button_state::pressed)
                OnMousePressEvent(ikey);
            else // released
                OnMouseReleaseEvent(ikey);
        }

        uint32_t axisevents = POINTER_EVENT_AXIS | POINTER_EVENT_AXIS_120 | POINTER_EVENT_AXIS_SOURCE | POINTER_EVENT_AXIS_STOP;

        if (currentPointerEvent.event_mask & axisevents)
        {
            for (size_t idx = 0 ; idx < 2 ; idx++)
            {
                if (!currentPointerEvent.axes[idx].valid)
                    continue;

                if (currentPointerEvent.event_mask & POINTER_EVENT_AXIS_120)
                {
                    if (idx == uint32_t(wayland::pointer_axis::vertical_scroll) && currentPointerEvent.axes[idx].value > 0)
                        OnMouseWheelEvent(InputKey::MouseWheelDown);
                    if (idx == uint32_t(wayland::pointer_axis::vertical_scroll) && currentPointerEvent.axes[idx].value < 0)
                        OnMouseWheelEvent(InputKey::MouseWheelUp);
                }
                else if (currentPointerEvent.event_mask & POINTER_EVENT_AXIS)
                {
                    if (idx == uint32_t(wayland::pointer_axis::vertical_scroll) && currentPointerEvent.axes[idx].value120 > 0)
                        OnMouseWheelEvent(InputKey::MouseWheelDown);
                    if (idx == uint32_t(wayland::pointer_axis::vertical_scroll) && currentPointerEvent.axes[idx].value120 < 0)
                        OnMouseWheelEvent(InputKey::MouseWheelUp);
                }
            }
        }

        // Reset everything once all the events are processed
        currentPointerEvent = {0};
    };

    m_keyboardDelayTimer = ZTimer();
    m_keyboardRepeatTimer = ZTimer();

    m_waylandKeyboard.on_repeat_info() = [this] (int32_t rate, int32_t delay) {
        // rate is characters per second, delay is in milliseconds
        m_keyboardDelayTimer.SetDuration(ZTimer::Duration(delay));
        m_keyboardRepeatTimer.SetDuration(ZTimer::Duration(1000.0 / rate));
    };

    m_keyboardDelayTimer.SetCallback([this] () { OnKeyboardDelayEnd(); });
    m_keyboardRepeatTimer.SetCallback([this] () { OnKeyboardRepeat(); });

    m_keyboardRepeatTimer.SetRepeating(true);

    m_previousTime = ZTimer::Clock::now();
    m_currentTime = ZTimer::Clock::now();
}

void WaylandDisplayBackend::OnKeyboardKeyEvent(xkb_keysym_t xkbKeySym, wayland::keyboard_key_state state)
{
    InputKey inputKey = XKBKeySymToInputKey(xkbKeySym);

    if (state == wayland::keyboard_key_state::pressed)
    {
        inputKeyStates[inputKey] = true;
        m_FocusWindow->windowHost->OnWindowKeyDown(inputKey);
        if (inputKey != previousKey)
        {
            previousKey = inputKey;
            m_keyboardDelayTimer.Stop();
            m_keyboardRepeatTimer.Stop();
        }
        m_keyboardDelayTimer.Start();
    }
    if (state == wayland::keyboard_key_state::released)
    {
        inputKeyStates[inputKey] = false;
        if (m_FocusWindow)
            m_FocusWindow->windowHost->OnWindowKeyUp(inputKey);
        m_keyboardDelayTimer.Stop();
        m_keyboardRepeatTimer.Stop();
    }

}

void WaylandDisplayBackend::OnKeyboardCharEvent(const char* ch, wayland::keyboard_key_state state)
{
    if (state == wayland::keyboard_key_state::pressed)
    {
        previousChars = std::string(ch);
        m_FocusWindow->windowHost->OnWindowKeyChar(previousChars);
    }
}

void WaylandDisplayBackend::OnKeyboardDelayEnd()
{
    if (inputKeyStates[previousKey])
        m_keyboardRepeatTimer.Start();
}

void WaylandDisplayBackend::OnKeyboardRepeat()
{
    if (inputKeyStates[previousKey])
    {
        m_FocusWindow->windowHost->OnWindowKeyDown(previousKey);
        m_FocusWindow->windowHost->OnWindowKeyChar(previousChars);
    }
}

void WaylandDisplayBackend::OnMouseEnterEvent(uint32_t serial)
{
    m_cursorSurface.attach(!hasMouseLock ? m_cursorBuffer : nullptr, 0, 0);
    if (!hasMouseLock)
        m_cursorSurface.damage(0, 0, m_cursorImage.width(), m_cursorImage.height());
    m_cursorSurface.commit();
    m_waylandPointer.set_cursor(serial, m_cursorSurface, 0, 0);
}

void WaylandDisplayBackend::OnMouseLeaveEvent()
{
    if (m_MouseFocusWindow)
    {
        m_MouseFocusWindow->windowHost->OnWindowMouseLeave();
        //m_MouseFocusWindow = nullptr; // Borks up the menus
    }
}

void WaylandDisplayBackend::OnMousePressEvent(InputKey button)
{
    if (m_MouseFocusWindow)
        m_MouseFocusWindow->windowHost->OnWindowMouseDown(m_MouseFocusWindow->m_SurfaceMousePos, button);
}

void WaylandDisplayBackend::OnMouseReleaseEvent(InputKey button)
{
    if (m_MouseFocusWindow)
        m_MouseFocusWindow->windowHost->OnWindowMouseUp(m_MouseFocusWindow->m_SurfaceMousePos, button);
}

void WaylandDisplayBackend::OnMouseMoveEvent(Point surfacePos)
{
	if (m_MouseFocusWindow)
	{
		m_MouseFocusWindow->m_SurfaceMousePos = surfacePos / m_MouseFocusWindow->m_ScaleFactor;
		m_MouseFocusWindow->windowHost->OnWindowMouseMove(m_MouseFocusWindow->m_SurfaceMousePos);
	}
}

void WaylandDisplayBackend::OnMouseMoveRawEvent(int dx, int dy)
{
    if (m_MouseFocusWindow)
        m_MouseFocusWindow->windowHost->OnWindowRawMouseMove(dx, dy);
}

void WaylandDisplayBackend::OnMouseWheelEvent(InputKey button)
{
    if (m_MouseFocusWindow)
        m_MouseFocusWindow->windowHost->OnWindowMouseWheel(m_MouseFocusWindow->m_SurfaceMousePos, button);
}

void WaylandDisplayBackend::SetCursor(StandardCursor cursor)
{
    std::string cursorName = GetWaylandCursorName(cursor);

    // Perhaps the cursor size can be inferred from the user prefs as well?
    wayland::cursor_theme_t cursorTheme = wayland::cursor_theme_t("default", 16, m_waylandSHM);
    wayland::cursor_t obtainedCursor = cursorTheme.get_cursor(cursorName);
    m_cursorImage = obtainedCursor.image(0);
    m_cursorBuffer = m_cursorImage.get_buffer();
}

void WaylandDisplayBackend::ShowCursor(bool enable)
{
    m_cursorSurface.attach(enable ? m_cursorBuffer : nullptr, 0, 0);
    m_cursorSurface.commit();
}

std::unique_ptr<DisplayWindow> WaylandDisplayBackend::Create(DisplayWindowHost* windowHost, bool popupWindow, DisplayWindow* owner, RenderAPI renderAPI)
{
	return std::make_unique<WaylandDisplayWindow>(this, windowHost, popupWindow, static_cast<WaylandDisplayWindow*>(owner), renderAPI);
}

void WaylandDisplayBackend::OnWindowCreated(WaylandDisplayWindow* window)
{
    s_Windows.push_back(window);
    if (!window->m_PopupWindow)
    {
        m_FocusWindow = window;
        m_MouseFocusWindow = window;
    }

}

void WaylandDisplayBackend::OnWindowDestroyed(WaylandDisplayWindow* window)
{
    auto it = std::find(s_Windows.begin(), s_Windows.end(), window);
    if (it != s_Windows.end())
        s_Windows.erase(it);

    if (m_FocusWindow == window)
        m_FocusWindow = nullptr;

    if (m_MouseFocusWindow == window)
        m_MouseFocusWindow = nullptr;
}

void WaylandDisplayBackend::ProcessEvents()
{
    while (wl_display_prepare_read(s_waylandDisplay))
        s_waylandDisplay.dispatch_pending();

    while (wl_display_flush(s_waylandDisplay) < 0 && errno == EAGAIN)
        poll_single(s_waylandDisplay.get_fd(), POLLOUT, -1);

    if (poll_single(s_waylandDisplay.get_fd(), POLLIN, 0) & POLLIN)
    {
        wl_display_read_events(s_waylandDisplay);
        s_waylandDisplay.dispatch_pending();
    }
    else
        wl_display_cancel_read(s_waylandDisplay);

    if (s_waylandDisplay.get_error())
        throw std::runtime_error("Wayland Protocol Error");
}

void WaylandDisplayBackend::RunLoop()
{
    exitRunLoop = false;

    while (!exitRunLoop)
    {
        CheckNeedsUpdate();
        UpdateTimers();

        ProcessEvents();
    }
}

void WaylandDisplayBackend::UpdateTimers()
{
    m_currentTime = ZTimer::Clock::now();

    m_keyboardDelayTimer.Update(m_currentTime - m_previousTime);
    m_keyboardRepeatTimer.Update(m_currentTime - m_previousTime);

    m_previousTime = m_currentTime;
}

void WaylandDisplayBackend::CheckNeedsUpdate()
{
    for (auto window: s_Windows)
    {
        if (window->m_NeedsUpdate)
        {
            window->m_NeedsUpdate = false;
            window->windowHost->OnWindowPaint();
        }
    }
}

void WaylandDisplayBackend::ExitLoop()
{
    exitRunLoop = true;
}

Size WaylandDisplayBackend::GetScreenSize()
{
    return s_ScreenSize;
}

void* WaylandDisplayBackend::StartTimer(int timeoutMilliseconds, std::function<void()> onTimer)
{
	throw std::logic_error("unimplemented: WaylandDisplayBackend::StartTimer");
}

void WaylandDisplayBackend::StopTimer(void* timerID)
{
	throw std::logic_error("unimplemented: WaylandDisplayBackend::StopTimer");
}

#ifdef USE_DBUS
std::unique_ptr<OpenFileDialog> WaylandDisplayBackend::CreateOpenFileDialog(DisplayWindow* owner)
{
	std::string ownerHandle;
	if (owner)
		ownerHandle = "wayland:" + static_cast<WaylandDisplayWindow*>(owner)->GetWaylandWindowID();
	return std::make_unique<DBusOpenFileDialog>(ownerHandle);
}

std::unique_ptr<SaveFileDialog> WaylandDisplayBackend::CreateSaveFileDialog(DisplayWindow* owner)
{
	std::string ownerHandle;
	if (owner)
		ownerHandle = "wayland:" + static_cast<WaylandDisplayWindow*>(owner)->GetWaylandWindowID();
	return std::make_unique<DBusSaveFileDialog>(ownerHandle);
}

std::unique_ptr<OpenFolderDialog> WaylandDisplayBackend::CreateOpenFolderDialog(DisplayWindow* owner)
{
	std::string ownerHandle;
	if (owner)
		ownerHandle = "wayland:" + static_cast<WaylandDisplayWindow*>(owner)->GetWaylandWindowID();
	return std::make_unique<DBusOpenFolderDialog>(ownerHandle);
}
#endif

bool WaylandDisplayBackend::GetKeyState(InputKey key)
{
    auto it = inputKeyStates.find(key);

    // if the key isn't "registered", then it is not pressed.
    if (it == inputKeyStates.end())
        return false;

    return it->second;
}

InputKey WaylandDisplayBackend::XKBKeySymToInputKey(xkb_keysym_t keySym)
{
    switch (keySym)
    {
        case XKB_KEY_Escape:
            return InputKey::Escape;
        case XKB_KEY_1:
            return InputKey::_1;
        case XKB_KEY_2:
            return InputKey::_2;
        case XKB_KEY_3:
            return InputKey::_3;
        case XKB_KEY_4:
            return InputKey::_4;
        case XKB_KEY_5:
            return InputKey::_5;
        case XKB_KEY_6:
            return InputKey::_6;
        case XKB_KEY_7:
            return InputKey::_7;
        case XKB_KEY_8:
            return InputKey::_8;
        case XKB_KEY_9:
            return InputKey::_9;
        case XKB_KEY_0:
            return InputKey::_0;
        case XKB_KEY_KP_1:
            return InputKey::NumPad1;
        case XKB_KEY_KP_2:
            return InputKey::NumPad2;
        case XKB_KEY_KP_3:
            return InputKey::NumPad3;
        case XKB_KEY_KP_4:
            return InputKey::NumPad4;
        case XKB_KEY_KP_5:
            return InputKey::NumPad5;
        case XKB_KEY_KP_6:
            return InputKey::NumPad6;
        case XKB_KEY_KP_7:
            return InputKey::NumPad7;
        case XKB_KEY_KP_8:
            return InputKey::NumPad8;
        case XKB_KEY_KP_9:
            return InputKey::NumPad9;
        case XKB_KEY_KP_0:
            return InputKey::NumPad0;
        case XKB_KEY_F1:
            return InputKey::F1;
        case XKB_KEY_F2:
            return InputKey::F2;
        case XKB_KEY_F3:
            return InputKey::F3;
        case XKB_KEY_F4:
            return InputKey::F4;
        case XKB_KEY_F5:
            return InputKey::F5;
        case XKB_KEY_F6:
            return InputKey::F6;
        case XKB_KEY_F7:
            return InputKey::F7;
        case XKB_KEY_F8:
            return InputKey::F8;
        case XKB_KEY_F9:
            return InputKey::F9;
        case XKB_KEY_F10:
            return InputKey::F10;
        case XKB_KEY_F11:
            return InputKey::F11;
        case XKB_KEY_F12:
            return InputKey::F12;
        case XKB_KEY_F13:
            return InputKey::F13;
        case XKB_KEY_F14:
            return InputKey::F14;
        case XKB_KEY_F15:
            return InputKey::F15;
        case XKB_KEY_F16:
            return InputKey::F16;
        case XKB_KEY_F17:
            return InputKey::F17;
        case XKB_KEY_F18:
            return InputKey::F18;
        case XKB_KEY_F19:
            return InputKey::F19;
        case XKB_KEY_F20:
            return InputKey::F20;
        case XKB_KEY_F21:
            return InputKey::F21;
        case XKB_KEY_F22:
            return InputKey::F22;
        case XKB_KEY_F23:
            return InputKey::F23;
        case XKB_KEY_F24:
            return InputKey::F24;
        case XKB_KEY_minus:
        case XKB_KEY_KP_Subtract:
            return InputKey::Minus;
        case XKB_KEY_equal:
            return InputKey::Equals;
        case XKB_KEY_BackSpace:
            return InputKey::Backspace;
        case XKB_KEY_backslash:
            return InputKey::Backslash;
        case XKB_KEY_Tab:
            return InputKey::Tab;
        case XKB_KEY_braceleft:
            return InputKey::LeftBracket;
        case XKB_KEY_braceright:
            return InputKey::RightBracket;
        case XKB_KEY_Control_L:
        case XKB_KEY_Control_R:
            return InputKey::Ctrl;
        case XKB_KEY_Alt_L:
        case XKB_KEY_Alt_R:
            return InputKey::Alt;
        case XKB_KEY_Delete:
            return InputKey::Delete;
        case XKB_KEY_semicolon:
            return InputKey::Semicolon;
        case XKB_KEY_comma:
            return InputKey::Comma;
        case XKB_KEY_period:
            return InputKey::Period;
        case XKB_KEY_Num_Lock:
            return InputKey::NumLock;
        case XKB_KEY_Caps_Lock:
            return InputKey::CapsLock;
        case XKB_KEY_Scroll_Lock:
            return InputKey::ScrollLock;
        case XKB_KEY_Shift_L:
            return InputKey::LShift;
        case XKB_KEY_Shift_R:
            return InputKey::RShift;
        case XKB_KEY_grave:
            return InputKey::Tilde;
        case XKB_KEY_apostrophe:
            return InputKey::SingleQuote;
        case XKB_KEY_KP_Enter:
        case XKB_KEY_Return:
            return InputKey::Enter;
        case XKB_KEY_space:
            return InputKey::Space;

        case XKB_KEY_Up:
            return InputKey::Up;
        case XKB_KEY_Down:
            return InputKey::Down;
        case XKB_KEY_Left:
            return InputKey::Left;
        case XKB_KEY_Right:
            return InputKey::Right;

        case XKB_KEY_A:
        case XKB_KEY_a:
            return InputKey::A;
        case XKB_KEY_B:
        case XKB_KEY_b:
            return InputKey::B;
        case XKB_KEY_C:
        case XKB_KEY_c:
            return InputKey::C;
        case XKB_KEY_D:
        case XKB_KEY_d:
            return InputKey::D;
        case XKB_KEY_E:
        case XKB_KEY_e:
            return InputKey::E;
        case XKB_KEY_F:
        case XKB_KEY_f:
            return InputKey::F;
        case XKB_KEY_G:
        case XKB_KEY_g:
            return InputKey::G;
        case XKB_KEY_H:
        case XKB_KEY_h:
            return InputKey::H;
        case XKB_KEY_I:
        case XKB_KEY_i:
            return InputKey::I;
        case XKB_KEY_J:
        case XKB_KEY_j:
            return InputKey::J;
        case XKB_KEY_K:
        case XKB_KEY_k:
            return InputKey::K;
        case XKB_KEY_L:
        case XKB_KEY_l:
            return InputKey::L;
        case XKB_KEY_M:
        case XKB_KEY_m:
            return InputKey::M;
        case XKB_KEY_N:
        case XKB_KEY_n:
            return InputKey::N;
        case XKB_KEY_O:
        case XKB_KEY_o:
            return InputKey::O;
        case XKB_KEY_P:
        case XKB_KEY_p:
            return InputKey::P;
        case XKB_KEY_Q:
        case XKB_KEY_q:
            return InputKey::Q;
        case XKB_KEY_R:
        case XKB_KEY_r:
            return InputKey::R;
        case XKB_KEY_S:
        case XKB_KEY_s:
            return InputKey::S;
        case XKB_KEY_T:
        case XKB_KEY_t:
            return InputKey::T;
        case XKB_KEY_U:
        case XKB_KEY_u:
            return InputKey::U;
        case XKB_KEY_V:
        case XKB_KEY_v:
            return InputKey::V;
        case XKB_KEY_W:
        case XKB_KEY_w:
            return InputKey::W;
        case XKB_KEY_X:
        case XKB_KEY_x:
            return InputKey::X;
        case XKB_KEY_Y:
        case XKB_KEY_y:
            return InputKey::Y;
        case XKB_KEY_Z:
        case XKB_KEY_z:
            return InputKey::Z;

        case XKB_KEY_NoSymbol:
        case XKB_KEY_VoidSymbol:
            return InputKey::None;
        default:
            return InputKey::None;
    }
}

InputKey WaylandDisplayBackend::LinuxInputEventCodeToInputKey(uint32_t inputCode)
{
    switch (inputCode)
    {
        // Keyboard
        // Probably not needed due to the existence of XKBKeySym
        case KEY_ESC:
            return InputKey::Escape;
        case KEY_1:
            return InputKey::_1;
        case KEY_2:
            return InputKey::_2;
        case KEY_3:
            return InputKey::_3;
        case KEY_4:
            return InputKey::_4;
        case KEY_5:
            return InputKey::_5;
        case KEY_6:
            return InputKey::_6;
        case KEY_7:
            return InputKey::_7;
        case KEY_8:
            return InputKey::_8;
        case KEY_9:
            return InputKey::_9;
        case KEY_0:
            return InputKey::_0;
        case KEY_KP1:
            return InputKey::NumPad1;
        case KEY_KP2:
            return InputKey::NumPad2;
        case KEY_KP3:
            return InputKey::NumPad3;
        case KEY_KP4:
            return InputKey::NumPad4;
        case KEY_KP5:
            return InputKey::NumPad5;
        case KEY_KP6:
            return InputKey::NumPad6;
        case KEY_KP7:
            return InputKey::NumPad7;
        case KEY_KP8:
            return InputKey::NumPad8;
        case KEY_KP9:
            return InputKey::NumPad9;
        case KEY_KP0:
            return InputKey::NumPad0;
        case KEY_F1:
            return InputKey::F1;
        case KEY_F2:
            return InputKey::F2;
        case KEY_F3:
            return InputKey::F3;
        case KEY_F4:
            return InputKey::F4;
        case KEY_F5:
            return InputKey::F5;
        case KEY_F6:
            return InputKey::F6;
        case KEY_F7:
            return InputKey::F7;
        case KEY_F8:
            return InputKey::F8;
        case KEY_F9:
            return InputKey::F9;
        case KEY_F10:
            return InputKey::F10;
        case KEY_F11:
            return InputKey::F11;
        case KEY_F12:
            return InputKey::F12;
        case KEY_F13:
            return InputKey::F13;
        case KEY_F14:
            return InputKey::F14;
        case KEY_F15:
            return InputKey::F15;
        case KEY_F16:
            return InputKey::F16;
        case KEY_F17:
            return InputKey::F17;
        case KEY_F18:
            return InputKey::F18;
        case KEY_F19:
            return InputKey::F19;
        case KEY_F20:
            return InputKey::F20;
        case KEY_F21:
            return InputKey::F21;
        case KEY_F22:
            return InputKey::F22;
        case KEY_F23:
            return InputKey::F23;
        case KEY_F24:
            return InputKey::F24;
        case KEY_MINUS:
        case KEY_KPMINUS:
            return InputKey::Minus;
        case KEY_EQUAL:
            return InputKey::Equals;
        case KEY_BACKSPACE:
            return InputKey::Backspace;
        case KEY_BACKSLASH:
            return InputKey::Backslash;
        case KEY_TAB:
            return InputKey::Tab;
        case KEY_LEFTBRACE:
            return InputKey::LeftBracket;
        case KEY_RIGHTBRACE:
            return InputKey::RightBracket;
        case KEY_LEFTCTRL:
            return InputKey::LControl;
        case KEY_RIGHTCTRL:
            return InputKey::RControl;
        case KEY_LEFTALT:
        case KEY_RIGHTALT:
            return InputKey::Alt;
        case KEY_DELETE:
            return InputKey::Delete;
        case KEY_SEMICOLON:
            return InputKey::Semicolon;
        case KEY_COMMA:
            return InputKey::Comma;
        case KEY_DOT:
            return InputKey::Period;
        case KEY_NUMLOCK:
            return InputKey::NumLock;
        case KEY_CAPSLOCK:
            return InputKey::CapsLock;
        case KEY_SCROLLDOWN:
            return InputKey::ScrollLock;
        case KEY_LEFTSHIFT:
            return InputKey::LShift;
        case KEY_RIGHTSHIFT:
            return InputKey::RShift;
        case KEY_GRAVE:
            return InputKey::Tilde;
        case KEY_APOSTROPHE:
            return InputKey::SingleQuote;
        case KEY_SPACE:
            return InputKey::Space;
        case KEY_ENTER:
        case KEY_KPENTER:
            return InputKey::Enter;

        case KEY_UP:
            return InputKey::Up;
        case KEY_DOWN:
            return InputKey::Down;
        case KEY_LEFT:
            return InputKey::Left;
        case KEY_RIGHT:
            return InputKey::Right;

        case KEY_A:
            return InputKey::A;
        case KEY_B:
            return InputKey::B;
        case KEY_C:
            return InputKey::C;
        case KEY_D:
            return InputKey::D;
        case KEY_E:
            return InputKey::E;
        case KEY_F:
            return InputKey::F;
        case KEY_G:
            return InputKey::G;
        case KEY_H:
            return InputKey::H;
        case KEY_I:
            return InputKey::I;
        case KEY_J:
            return InputKey::J;
        case KEY_K:
            return InputKey::K;
        case KEY_L:
            return InputKey::L;
        case KEY_M:
            return InputKey::M;
        case KEY_N:
            return InputKey::N;
        case KEY_O:
            return InputKey::O;
        case KEY_P:
            return InputKey::P;
        case KEY_Q:
            return InputKey::Q;
        case KEY_R:
            return InputKey::R;
        case KEY_S:
            return InputKey::S;
        case KEY_T:
            return InputKey::T;
        case KEY_U:
            return InputKey::U;
        case KEY_V:
            return InputKey::V;
        case KEY_W:
            return InputKey::W;
        case KEY_X:
            return InputKey::X;
        case KEY_Y:
            return InputKey::Y;
        case KEY_Z:
            return InputKey::Z;

        // Mouse
        case BTN_LEFT:
            return InputKey::LeftMouse;
        case BTN_RIGHT:
            return InputKey::RightMouse;
        case BTN_MIDDLE:
            return InputKey::MiddleMouse;
        default:
            return InputKey::None;
    }
}

std::string WaylandDisplayBackend::GetWaylandCursorName(StandardCursor cursor)
{
    // Checked out Adwaita and Breeze cursors for the names.
    // Other cursor themes should adhere to the names these two have.
    switch (cursor)
    {
        case StandardCursor::arrow:
            return "default";
        case StandardCursor::appstarting:
            return "progress";
        case StandardCursor::cross:
            return "crosshair";
        case StandardCursor::hand:
            return "pointer";
        case StandardCursor::ibeam:
            return "text";
        case StandardCursor::no:
            return "not-allowed";
        case StandardCursor::size_all:
            return "fleur";
        case StandardCursor::size_nesw:
            return "nesw-resize";
        case StandardCursor::size_ns:
            return "ns-resize";
        case StandardCursor::size_nwse:
            return "nwse-resize";
        case StandardCursor::size_we:
            return "ew-resize";
        case StandardCursor::uparrow:
            // Breeze actually has an up-arrow cursor, but Adwaita doesn't, so the default cursor it is
            return "default";
        case StandardCursor::wait:
            return "wait";
        default:
            return "default";
    }
}

