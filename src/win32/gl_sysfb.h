#pragma once

#include "base_sysfb.h"

class SystemGLFrameBuffer : public SystemBaseFrameBuffer
{
	typedef SystemBaseFrameBuffer Super;

public:
	SystemGLFrameBuffer() {}
	// Actually, hMonitor is a HMONITOR, but it's passed as a void * as there
    // look to be some cross-platform bits in the way.
	SystemGLFrameBuffer(void *hMonitor, bool fullscreen);

	void SetVSync (bool vsync);
	void SwapBuffers();

protected:
	int SwapInterval;
};
