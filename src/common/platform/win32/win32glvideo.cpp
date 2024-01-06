/*
** win32video.cpp
** Code to let ZDoom draw to the screen
**
**---------------------------------------------------------------------------
** Copyright 1998-2006 Randy Heit
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

#include <windows.h>
#include <GL/gl.h>
#include <vector>
#include "wglext.h"
#include <vector>

#include "gl_sysfb.h"
#include "hardware.h"

#include "version.h"
#include "c_console.h"
#include "v_video.h"
#include "i_input.h"
#include "i_system.h"
#include "v_text.h"
#include "m_argv.h"
#include "printf.h"
#include "engineerrors.h"
#include "win32glvideo.h"

#include "gl_framebuffer.h"
#ifdef HAVE_GLES2
#include "gles_framebuffer.h"
#endif

extern "C" {
HGLRC zd_wglCreateContext(HDC Arg1);
BOOL zd_wglDeleteContext(HGLRC Arg1);
BOOL zd_wglMakeCurrent(HDC Arg1, HGLRC Arg2);
PROC zd_wglGetProcAddress(LPCSTR name);
}

EXTERN_CVAR(Int, vid_adapter)
EXTERN_CVAR(Bool, vid_hdr)

CUSTOM_CVAR(Bool, gl_debug, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOINITCALL)
{
	Printf("This won't take effect until " GAMENAME " is restarted.\n");
}

// For broadest GL compatibility, require user to explicitly enable quad-buffered stereo mode.
// Setting vr_enable_quadbuffered_stereo does not automatically invoke quad-buffered stereo,
// but makes it possible for subsequent "vr_mode 7" to invoke quad-buffered stereo
CUSTOM_CVAR(Bool, vr_enable_quadbuffered, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOINITCALL)
{
	Printf("You must restart " GAMENAME " to switch quad stereo mode\n");
}

extern bool vid_hdr_active;

// these get used before GLEW is initialized so we have to use separate pointers with different names
PFNWGLCHOOSEPIXELFORMATARBPROC myWglChoosePixelFormatARB; // = (PFNWGLCHOOSEPIXELFORMATARBPROC)zd_wglGetProcAddress("wglChoosePixelFormatARB");
PFNWGLCREATECONTEXTATTRIBSARBPROC myWglCreateContextAttribsARB;


//==========================================================================
//
// 
//
//==========================================================================

Win32GLVideo::Win32GLVideo()
{
	SetPixelFormat();
}

//==========================================================================
//
// 
//
//==========================================================================

DFrameBuffer *Win32GLVideo::CreateFrameBuffer()
{
	SystemGLFrameBuffer *fb;

#ifdef HAVE_GLES2
	if (V_GetBackend() != 0)
		fb = new OpenGLESRenderer::OpenGLFrameBuffer(m_hMonitor, vid_fullscreen);
	else
#endif
		fb = new OpenGLRenderer::OpenGLFrameBuffer(m_hMonitor, vid_fullscreen);

	return fb;
}

//==========================================================================
//
// 
//
//==========================================================================

HWND Win32GLVideo::InitDummy()
{
	HMODULE g_hInst = GetModuleHandle(NULL);
	HWND dummy;
	//Create a rect structure for the size/position of the window
	RECT windowRect;
	windowRect.left = 0;
	windowRect.right = 64;
	windowRect.top = 0;
	windowRect.bottom = 64;

	//Window class structure
	WNDCLASS wc;

	//Fill in window class struct
	wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
	wc.lpfnWndProc = (WNDPROC)DefWindowProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = g_hInst;
	wc.hIcon = LoadIcon(NULL, IDI_WINLOGO);
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = NULL;
	wc.lpszMenuName = NULL;
	wc.lpszClassName = L"GZDoomOpenGLDummyWindow";

	//Register window class
	if (!RegisterClass(&wc))
	{
		return 0;
	}

	//Set window style & extended style
	DWORD style, exStyle;
	exStyle = WS_EX_CLIENTEDGE;
	style = WS_SYSMENU | WS_BORDER | WS_CAPTION;// | WS_VISIBLE;

												//Adjust the window size so that client area is the size requested
	AdjustWindowRectEx(&windowRect, style, false, exStyle);

	//Create Window
	if (!(dummy = CreateWindowExW(exStyle,
		L"GZDoomOpenGLDummyWindow",
		WGAMENAME,
		WS_CLIPSIBLINGS | WS_CLIPCHILDREN | style,
		0, 0,
		windowRect.right - windowRect.left,
		windowRect.bottom - windowRect.top,
		NULL, NULL,
		g_hInst,
		NULL)))
	{
		UnregisterClassW(L"GZDoomOpenGLDummyWindow", g_hInst);
		return 0;
	}
	ShowWindow(dummy, SW_HIDE);

	return dummy;
}

//==========================================================================
//
// 
//
//==========================================================================

void Win32GLVideo::ShutdownDummy(HWND dummy)
{
	DestroyWindow(dummy);
	UnregisterClassW(L"GZDoomOpenGLDummyWindow", GetModuleHandle(NULL));
}


//==========================================================================
//
// 
//
//==========================================================================

bool Win32GLVideo::SetPixelFormat()
{
	HDC hDC;
	HGLRC hRC;
	HWND dummy;

	PIXELFORMATDESCRIPTOR pfd = {
		sizeof(PIXELFORMATDESCRIPTOR),
		1,
		PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
		PFD_TYPE_RGBA,
		32, // color depth
		0, 0, 0, 0, 0, 0,
		0,
		0,
		0,
		0, 0, 0, 0,
		16, // z depth
		0, // stencil buffer
		0,
		PFD_MAIN_PLANE,
		0,
		0, 0, 0
	};

	int pixelFormat;

	// we have to create a dummy window to init stuff from or the full init stuff fails
	dummy = InitDummy();

	hDC = GetDC(dummy);
	pixelFormat = ChoosePixelFormat(hDC, &pfd);
	DescribePixelFormat(hDC, pixelFormat, sizeof(pfd), &pfd);

	::SetPixelFormat(hDC, pixelFormat, &pfd);

	hRC = zd_wglCreateContext(hDC);
	zd_wglMakeCurrent(hDC, hRC);

	myWglChoosePixelFormatARB = (PFNWGLCHOOSEPIXELFORMATARBPROC)zd_wglGetProcAddress("wglChoosePixelFormatARB");
	myWglCreateContextAttribsARB = (PFNWGLCREATECONTEXTATTRIBSARBPROC)zd_wglGetProcAddress("wglCreateContextAttribsARB");
	// any extra stuff here?

	zd_wglMakeCurrent(NULL, NULL);
	zd_wglDeleteContext(hRC);
	ReleaseDC(dummy, hDC);
	ShutdownDummy(dummy);

	return true;
}

//==========================================================================
//
// 
//
//==========================================================================

static void append(std::vector<int> &list1, std::initializer_list<int> list2)
{
	list1.insert(list1.end(), list2);
}

bool Win32GLVideo::SetupPixelFormat(int multisample)
{
	int colorDepth;
	HDC deskDC;
	std::vector<int> attributes;
	int pixelFormat;
	unsigned int numFormats;
	float attribsFloat[] = { 0.0f, 0.0f };

	deskDC = GetDC(GetDesktopWindow());
	colorDepth = GetDeviceCaps(deskDC, BITSPIXEL);
	ReleaseDC(GetDesktopWindow(), deskDC);

	if (myWglChoosePixelFormatARB)
	{
	again:
		append(attributes, { WGL_DEPTH_BITS_ARB, 24 });
		append(attributes, { WGL_STENCIL_BITS_ARB, 8 });

		//required to be true
		append(attributes, { WGL_DRAW_TO_WINDOW_ARB, GL_TRUE });
		append(attributes, { WGL_SUPPORT_OPENGL_ARB, GL_TRUE });
		append(attributes, { WGL_DOUBLE_BUFFER_ARB, GL_TRUE });

		if (multisample > 0)
		{
			append(attributes, { WGL_SAMPLE_BUFFERS_ARB, GL_TRUE });
			append(attributes, { WGL_SAMPLES_ARB, multisample });
		}

		append(attributes, { WGL_ACCELERATION_ARB, WGL_FULL_ACCELERATION_ARB }); //required to be FULL_ACCELERATION_ARB

		if (vr_enable_quadbuffered)
		{
			// [BB] Starting with driver version 314.07, NVIDIA GeForce cards support OpenGL quad buffered
			// stereo rendering with 3D Vision hardware. Select the corresponding attribute here.
			append(attributes, { WGL_STEREO_ARB, GL_TRUE });
		}

		size_t bitsPos = attributes.size();

		if (vid_hdr)
		{
			append(attributes, { WGL_RED_BITS_ARB, 16 });
			append(attributes, { WGL_GREEN_BITS_ARB, 16 });
			append(attributes, { WGL_BLUE_BITS_ARB, 16 });
			append(attributes, { WGL_ALPHA_BITS_ARB, 16 });
			append(attributes, { WGL_PIXEL_TYPE_ARB, WGL_TYPE_RGBA_FLOAT_ARB });
		}
		else
		{
			append(attributes, { WGL_RED_BITS_ARB, 8 });
			append(attributes, { WGL_GREEN_BITS_ARB, 8 });
			append(attributes, { WGL_BLUE_BITS_ARB, 8 });
			append(attributes, { WGL_ALPHA_BITS_ARB, 8 });
		}

		append(attributes, { 0, 0 });

		if (!myWglChoosePixelFormatARB(m_hDC, attributes.data(), attribsFloat, 1, &pixelFormat, &numFormats))
		{
			I_FatalError("R_OPENGL: Couldn't choose pixel format. Retrying in compatibility mode\n");
		}

		if (vid_hdr && numFormats == 0) // This card/driver doesn't support the rgb16f pixel format. Fall back to 8bpc
		{
			Printf("R_OPENGL: This card/driver does not support RGBA16F. HDR will not work.\n");

			attributes.erase(attributes.begin() + bitsPos, attributes.end());
			append(attributes, { WGL_RED_BITS_ARB, 8 });
			append(attributes, { WGL_GREEN_BITS_ARB, 8 });
			append(attributes, { WGL_BLUE_BITS_ARB, 8 });
			append(attributes, { WGL_ALPHA_BITS_ARB, 8 });
			append(attributes, { 0, 0 });

			if (!myWglChoosePixelFormatARB(m_hDC, attributes.data(), attribsFloat, 1, &pixelFormat, &numFormats))
			{
				I_FatalError("R_OPENGL: Couldn't choose pixel format.");
			}
		}
		else if (vid_hdr)
		{
			vid_hdr_active = true;
		}

		if (numFormats == 0)
		{
			if (vr_enable_quadbuffered)
			{
				Printf("R_OPENGL: No valid pixel formats found for VR quadbuffering. Retrying without this feature\n");
				vr_enable_quadbuffered = false;
				goto again;
			}
			I_FatalError("R_OPENGL: No valid pixel formats found.");
		}
	}
	else
	{
		I_FatalError("R_OPENGL: Unable to create an OpenGL render context. Insufficient driver support for context creation\n");
	}

	if (!::SetPixelFormat(m_hDC, pixelFormat, NULL))
	{
		I_Error("R_OPENGL: Couldn't set pixel format.\n");
		return false;
	}
	return true;
}

//==========================================================================
//
// 
//
//==========================================================================

bool Win32GLVideo::InitHardware(HWND Window, int multisample)
{
	m_Window = Window;
	m_hDC = GetDC(Window);

	if (!SetupPixelFormat(multisample))
	{
		return false;
	}

	int prof = WGL_CONTEXT_CORE_PROFILE_BIT_ARB;

	for (; prof <= WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB; prof++)
	{
		m_hRC = NULL;
		if (myWglCreateContextAttribsARB != NULL)
		{
			// let's try to get the best version possible. Some drivers only give us the version we request
			// which breaks all version checks for feature support. The highest used features we use are from version 4.4, and 3.3 is a requirement.
			static int versions[] = { 46, 45, 44, 43, 42, 41, 40, 33, -1 };

			for (int i = 0; versions[i] > 0; i++)
			{
				int ctxAttribs[] = {
					WGL_CONTEXT_MAJOR_VERSION_ARB, versions[i] / 10,
					WGL_CONTEXT_MINOR_VERSION_ARB, versions[i] % 10,
					WGL_CONTEXT_FLAGS_ARB, gl_debug ? WGL_CONTEXT_DEBUG_BIT_ARB : 0,
					WGL_CONTEXT_PROFILE_MASK_ARB, prof,
					0
				};

				m_hRC = myWglCreateContextAttribsARB(m_hDC, 0, ctxAttribs);
				if (m_hRC != NULL) break;
			}
		}

		if (m_hRC == NULL && prof == WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB)
		{
			m_hRC = zd_wglCreateContext(m_hDC);
			if (m_hRC == NULL)
			{
				I_FatalError("R_OPENGL: Unable to create an OpenGL render context.\n");
			}
		}

		if (m_hRC != NULL)
		{
			zd_wglMakeCurrent(m_hDC, m_hRC);
			return true;
		}
	}
	// We get here if the driver doesn't support the modern context creation API which always means an old driver.
	I_FatalError("R_OPENGL: Unable to create an OpenGL render context. Insufficient driver support for context creation\n");
	return false;
}

//==========================================================================
//
// 
//
//==========================================================================

void Win32GLVideo::Shutdown()
{
	if (m_hRC)
	{
		zd_wglMakeCurrent(0, 0);
		zd_wglDeleteContext(m_hRC);
	}
	if (m_hDC) ReleaseDC(m_Window, m_hDC);
}





