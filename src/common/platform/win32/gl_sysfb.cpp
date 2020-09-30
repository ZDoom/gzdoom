/*
**
**
**---------------------------------------------------------------------------
** Copyright 2003-2005 Tim Stump
** Copyright 2005-2016 Christoph Oelckers
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

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <GL/gl.h>
#include "wglext.h"

#include "gl_sysfb.h"
#include "hardware.h"
#include "templates.h"
#include "version.h"
#include "c_console.h"
#include "v_video.h"
#include "i_input.h"
#include "i_system.h"
#include "v_text.h"
#include "m_argv.h"
#include "engineerrors.h"
#include "win32glvideo.h"

extern HWND			Window;

PFNWGLSWAPINTERVALEXTPROC myWglSwapIntervalExtProc;

//==========================================================================
//
// Windows framebuffer
//
//==========================================================================


//==========================================================================
//
// 
//
//==========================================================================

SystemGLFrameBuffer::SystemGLFrameBuffer(void *hMonitor, bool fullscreen) : SystemBaseFrameBuffer(hMonitor, fullscreen)
{
	if (!static_cast<Win32GLVideo *>(Video)->InitHardware(Window, 0))
	{
		I_FatalError("Unable to initialize OpenGL");
		return;
	}

	HDC hDC = GetDC(Window);
	const char *wglext = nullptr;

	myWglSwapIntervalExtProc = (PFNWGLSWAPINTERVALEXTPROC)wglGetProcAddress("wglSwapIntervalEXT");
	auto myWglGetExtensionsStringARB = (PFNWGLGETEXTENSIONSSTRINGARBPROC)wglGetProcAddress("wglGetExtensionsStringARB");
	if (myWglGetExtensionsStringARB)
	{
		wglext = myWglGetExtensionsStringARB(hDC);
	}
	else
	{
		auto myWglGetExtensionsStringEXT = (PFNWGLGETEXTENSIONSSTRINGEXTPROC)wglGetProcAddress("wglGetExtensionsStringEXT");
		if (myWglGetExtensionsStringEXT)
		{
			wglext = myWglGetExtensionsStringEXT();
		}
	}
	SwapInterval = 1; 
	if (wglext != nullptr)
	{
		if (strstr(wglext, "WGL_EXT_swap_control_tear"))
		{
			SwapInterval = -1;
		}
	}
	ReleaseDC(Window, hDC);
}

//==========================================================================
//
// 
//
//==========================================================================
EXTERN_CVAR(Bool, vid_vsync);
CUSTOM_CVAR(Bool, gl_control_tear, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	vid_vsync.Callback();
}

void SystemGLFrameBuffer::SetVSync (bool vsync)
{
	if (myWglSwapIntervalExtProc != NULL) myWglSwapIntervalExtProc(vsync ? (gl_control_tear? SwapInterval : 1) : 0);
}

void SystemGLFrameBuffer::SwapBuffers()
{
	::SwapBuffers(static_cast<Win32GLVideo *>(Video)->m_hDC);
}

