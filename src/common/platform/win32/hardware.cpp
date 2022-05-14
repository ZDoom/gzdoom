/*
** hardware.cpp
** Somewhat OS-independant interface to the screen, mouse, keyboard, and stick
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

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>

#include "hardware.h"
#include "c_dispatch.h"
#include "v_text.h"
#include "basics.h"
#include "m_argv.h"
#include "version.h"
#include "printf.h"
#include "win32glvideo.h"
#ifdef HAVE_SOFTPOLY
#include "win32polyvideo.h"
#endif
#ifdef HAVE_VULKAN
#include "win32vulkanvideo.h"
#endif
#include "engineerrors.h"
#include "i_system.h"
#include "i_mainwindow.h"

EXTERN_CVAR(Int, vid_preferbackend)

IVideo *Video;

// do not include GL headers here, only declare the necessary functions.
IVideo *gl_CreateVideo();

void I_RestartRenderer();
int currentcanvas = -1;
int currentgpuswitch = -1;
bool changerenderer;

// Optimus/Hybrid switcher
CUSTOM_CVAR(Int, vid_gpuswitch, 0, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOINITCALL)
{
	if (self != currentgpuswitch)
	{
		switch (self)
		{
		case 0:
			Printf("Selecting default GPU...\n");
			break;
		case 1:
			Printf("Selecting high-performance dedicated GPU...\n");
			break;
		case 2:
			Printf("Selecting power-saving integrated GPU...\n");
			break;
		default:
			Printf("Unknown option (%d) - falling back to 'default'\n", *vid_gpuswitch);
			self = 0;
			break;
		}
		Printf("You must restart " GAMENAME " for this change to take effect.\n");
	}
}



void I_ShutdownGraphics ()
{
	if (screen)
	{
		DFrameBuffer *s = screen;
		screen = NULL;
		delete s;
	}
	if (Video)
		delete Video, Video = NULL;
}

void I_InitGraphics ()
{
	// todo: implement ATI version of this. this only works for nvidia notebooks, for now.
	currentgpuswitch = vid_gpuswitch;
	if (currentgpuswitch == 1)
		_putenv("SHIM_MCCOMPAT=0x800000001"); // discrete
	else if (currentgpuswitch == 2)
		_putenv("SHIM_MCCOMPAT=0x800000000"); // integrated

	// If the focus window is destroyed, it doesn't go back to the active window.
	// (e.g. because the net pane was up, and a button on it had focus)
	if (GetFocus() == NULL && GetActiveWindow() == mainwindow.GetHandle())
	{
		// Make sure it's in the foreground and focused. (It probably is
		// already foregrounded but may not be focused.)
		SetForegroundWindow(mainwindow.GetHandle());
		SetFocus(mainwindow.GetHandle());
		// Note that when I start a 2-player game on the same machine, the
		// window for the game that isn't focused, active, or foregrounded
		// still receives a WM_ACTIVATEAPP message telling it that it's the
		// active window. The window that is really the active window does
		// not receive a WM_ACTIVATEAPP message, so both games think they
		// are the active app. Huh?
	}

#ifdef HAVE_SOFTPOLY
	if (vid_preferbackend == 2)
	{
		Video = new Win32PolyVideo();
	}
	else
#endif
#ifdef HAVE_VULKAN
	if (vid_preferbackend == 1)
	{
		// first try Vulkan, if that fails OpenGL
		try
		{
			Video = new Win32VulkanVideo();
		}
		catch (CVulkanError &error)
		{
			Printf(TEXTCOLOR_RED "Initialization of Vulkan failed: %s\n", error.what());
			Video = new Win32GLVideo();
		}
	}
	else
#endif
	{
		Video = new Win32GLVideo();
	}

#ifdef HAVE_SOFTPOLY
	if (Video == NULL)
		Video = new Win32PolyVideo();
#endif
	// we somehow STILL don't have a display!!
	if (Video == NULL)
		I_FatalError ("Failed to initialize display");

}
