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
#include "wglext.h"

#include "gl_sysfb.h"
#include "hardware.h"
#include "x86.h"
#include "templates.h"
#include "version.h"
#include "c_console.h"
#include "v_video.h"
#include "i_input.h"
#include "i_system.h"
#include "doomstat.h"
#include "v_text.h"
#include "m_argv.h"
#include "doomerrors.h"
#include "Win32BaseVideo.h"

#include "gl/system/gl_framebuffer.h"

CVAR(Int, vid_adapter, 1, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)

EXTERN_CVAR(Bool, vr_enable_quadbuffered)

//==========================================================================
//
// 
//
//==========================================================================

Win32BaseVideo::Win32BaseVideo()
{
	I_SetWndProc();

	GetDisplayDeviceName();
}

//==========================================================================
//
// 
//
//==========================================================================

struct MonitorEnumState
{
	int curIdx;
	HMONITOR hFoundMonitor;
};

static BOOL CALLBACK GetDisplayDeviceNameMonitorEnumProc(HMONITOR hMonitor, HDC, LPRECT, LPARAM dwData)
{
	MonitorEnumState *state = reinterpret_cast<MonitorEnumState *>(dwData);

	MONITORINFOEX mi;
	mi.cbSize = sizeof mi;
	GetMonitorInfo(hMonitor, &mi);

	// This assumes the monitors are returned by EnumDisplayMonitors in the
	// order they're found in the Direct3D9 adapters list. Fingers crossed...
	if (state->curIdx == vid_adapter)
	{
		state->hFoundMonitor = hMonitor;

		// Don't stop enumeration; this makes EnumDisplayMonitors fail. I like
		// proper fails.
	}

	++state->curIdx;

	return TRUE;
}

//==========================================================================
//
// 
//
//==========================================================================

void Win32BaseVideo::GetDisplayDeviceName()
{
	// If anything goes wrong, anything at all, everything uses the primary
	// monitor.
	m_DisplayDeviceName = 0;
	m_hMonitor = 0;

	MonitorEnumState mes;

	mes.curIdx = 1;
	mes.hFoundMonitor = 0;

	// Could also use EnumDisplayDevices, I guess. That might work.
	if (EnumDisplayMonitors(0, 0, &GetDisplayDeviceNameMonitorEnumProc, LPARAM(&mes)))
	{
		if (mes.hFoundMonitor)
		{
			MONITORINFOEX mi;

			mi.cbSize = sizeof mi;

			if (GetMonitorInfo(mes.hFoundMonitor, &mi))
			{
				strcpy(m_DisplayDeviceBuffer, mi.szDevice);
				m_DisplayDeviceName = m_DisplayDeviceBuffer;

				m_hMonitor = mes.hFoundMonitor;
			}
		}
	}
}

//==========================================================================
//
// 
//
//==========================================================================

struct DumpAdaptersState
{
	unsigned index;
	char *displayDeviceName;
};

static BOOL CALLBACK DumpAdaptersMonitorEnumProc(HMONITOR hMonitor, HDC, LPRECT, LPARAM dwData)
{
	DumpAdaptersState *state = reinterpret_cast<DumpAdaptersState *>(dwData);

	MONITORINFOEX mi;
	mi.cbSize = sizeof mi;

	char moreinfo[64] = "";

	bool active = true;

	if (GetMonitorInfo(hMonitor, &mi))
	{
		bool primary = !!(mi.dwFlags & MONITORINFOF_PRIMARY);

		mysnprintf(moreinfo, countof(moreinfo), " [%ldx%ld @ (%ld,%ld)]%s",
			mi.rcMonitor.right - mi.rcMonitor.left,
			mi.rcMonitor.bottom - mi.rcMonitor.top,
			mi.rcMonitor.left, mi.rcMonitor.top,
			primary ? " (Primary)" : "");

		if (!state->displayDeviceName && !primary)
			active = false;//primary selected, but this ain't primary
		else if (state->displayDeviceName && strcmp(state->displayDeviceName, mi.szDevice) != 0)
			active = false;//this isn't the selected one
	}

	Printf("%s%u. %s\n",
		active ? TEXTCOLOR_BOLD : "",
		state->index,
		moreinfo);

	++state->index;

	return TRUE;
}

//==========================================================================
//
// 
//
//==========================================================================

void Win32BaseVideo::DumpAdapters()
{
	DumpAdaptersState das;

	das.index = 1;
	das.displayDeviceName = m_DisplayDeviceName;

	EnumDisplayMonitors(0, 0, DumpAdaptersMonitorEnumProc, LPARAM(&das));
}

