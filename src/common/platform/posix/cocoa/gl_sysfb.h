/*
 ** gl_sysfb.h
 **
 **---------------------------------------------------------------------------
 ** Copyright 2012-2018 Alexey Lysiuk
 ** All rights reserved.
 **
 ** Redistribution and use in source and binary forms, with or without
 ** modification, are permitted provided that the following conditions
 ** are met:
 **
 ** 1. Redistributions of source code must retain the above copyright
 **    notice, this list of conditions and the following disclaimer.
 ** 2. Redistributions in binary form must reproduce the above copyright
 **    notice, this list of conditions and the following disclaimer in the
 **    documentation and/or other materials provided with the distribution.
 ** 3. The name of the author may not be used to endorse or promote products
 **    derived from this software without specific prior written permission.
 **
 ** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 ** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 ** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 ** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 ** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 ** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 ** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 ** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 ** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 ** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 **---------------------------------------------------------------------------
 **
 */

#ifndef COCOA_GL_SYSFB_H_INCLUDED
#define COCOA_GL_SYSFB_H_INCLUDED

#include "v_video.h"

#ifdef __OBJC__
@class NSCursor;
@class CocoaWindow;
#else
typedef struct objc_object NSCursor;
typedef struct objc_object CocoaWindow;
#endif

class SystemBaseFrameBuffer : public DFrameBuffer
{
public:
	// This must have the same parameters as the Windows version, even if they are not used!
	SystemBaseFrameBuffer(void *hMonitor, bool fullscreen);
	~SystemBaseFrameBuffer();

	bool IsFullscreen() override;

	int GetClientWidth() override;
	int GetClientHeight() override;
	void ToggleFullscreen(bool yes) override;
	void SetWindowSize(int width, int height) override;

	virtual void SetMode(bool fullscreen, bool hiDPI);

	static void UseHiDPI(bool hiDPI);
	static void SetCursor(NSCursor* cursor);
	static void SetWindowVisible(bool visible);
	static void SetWindowTitle(const char* title);

	void SetWindow(CocoaWindow* window) { m_window = window; }

protected:
	SystemBaseFrameBuffer() {}

	void SetFullscreenMode();
	void SetWindowedMode();

	bool m_fullscreen;
	bool m_hiDPI;

	CocoaWindow* m_window;

	int GetTitleBarHeight() const;

};

class SystemGLFrameBuffer : public SystemBaseFrameBuffer
{
	typedef SystemBaseFrameBuffer Super;

public:
	SystemGLFrameBuffer(void *hMonitor, bool fullscreen);

	void SetVSync(bool vsync) override;

	void SetMode(bool fullscreen, bool hiDPI) override;

protected:
	void SwapBuffers();

	SystemGLFrameBuffer() {}
};

#endif // COCOA_GL_SYSFB_H_INCLUDED
