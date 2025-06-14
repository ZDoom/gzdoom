#pragma once

#include <wayland-client-protocol.h>

class WaylandNativeHandle
{
public:
	wl_display* display = nullptr;
	wl_surface* surface = nullptr;
};
