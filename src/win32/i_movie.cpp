/*
** i_movie.cpp
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

/* !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 * If you do not have dshow.h, either download the latest DirectX SDK
 * or #define I_DO_NOT_LIKE_BIG_DOWNLOADS
 * !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 */

#ifndef I_DO_NOT_LIKE_BIG_DOWNLOADS
#define I_DO_NOT_LIKE_BIG_DOWNLOADS
#endif

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#define USE_WINDOWS_DWORD

#include "c_cvars.h"

CUSTOM_CVAR (Float, snd_movievolume, 1.f, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	if (self < 0.f)
		self = 0.f;
	else if (self > 1.f)
		self = 1.f;
}

#ifdef I_DO_NOT_LIKE_BIG_DOWNLOADS

#include "i_movie.h"

int I_PlayMovie (const char *movie)
{
	return MOVIE_Failed;
}

#else

#include <dshow.h>
#include "i_movie.h"
#include "i_sound.h"
#include "v_video.h"
#include "c_console.h"
#include "win32iface.h"
#include "sbar.h"

EXTERN_CVAR (String, language)

#define WM_GRAPHNOTIFY	(WM_APP+321)

#ifdef _DEBUG
#define INGAME_PRIORITY_CLASS	NORMAL_PRIORITY_CLASS
#else
//#define INGAME_PRIORITY_CLASS	HIGH_PRIORITY_CLASS
#define INGAME_PRIORITY_CLASS	NORMAL_PRIORITY_CLASS
#endif

extern HWND Window;
extern IVideo *Video;
extern LRESULT CALLBACK WndProc (HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
extern void I_CheckNativeMouse (bool preferNative);

static bool MovieNotDone;
static bool MovieInterrupted;
static bool MovieDestroyed;
static bool FullVideo;
static bool NoVideo;
static IVideoWindow *vidwin;
static IMediaEventEx *event;
static IGraphBuilder *graph;
static IMediaControl *control;
static IBasicAudio *audio;
static IBasicVideo *video;

static void CheckIfVideo ();
static void SetMovieSize ();
static void SetTheVolume ();
static void SizeWindowForVideo ();

LRESULT CALLBACK MovieWndProc (HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_GRAPHNOTIFY:
		{
			long code;
			LONG_PTR parm1, parm2;

			while (event->GetEvent (&code, &parm1, &parm2, 0) == S_OK)
			{
				if (code == EC_COMPLETE)
				{
					MovieNotDone = false;
				}
				event->FreeEventParams (code, parm1, parm2);
			}
		}
		break;

	case WM_SIZE:
		if (vidwin == NULL)
		{
			InvalidateRect (Window, NULL, FALSE);
		}
		else if ((wParam == SIZE_MAXIMIZED || wParam == SIZE_RESTORED) && !FullVideo)
		{
			SetMovieSize ();
			return 0;
		}
		break;

	case WM_PAINT:
		if (vidwin == NULL)
		{
			if (screen != NULL)
			{
				static_cast<BaseWinFB *> (screen)->PaintToWindow ();
			}
		}
		else if (!FullVideo)
		{
			PAINTSTRUCT ps;
			HBRUSH br;
			HDC dc;
			long left, top, width, height;

			if (S_OK == vidwin->GetWindowPosition (&left, &top, &width, &height))
			{
				dc = BeginPaint (Window, &ps);
				if (dc != NULL)
				{
					RECT rect = { left, top, left+width, top+height };
					ScreenToClient (hWnd, (LPPOINT)&rect.left);
					ScreenToClient (hWnd, (LPPOINT)&rect.right);
					br = (HBRUSH)GetStockObject (BLACK_BRUSH);
					switch (ExcludeClipRect (dc, rect.left, rect.top, rect.right, rect.bottom))
					{
					case SIMPLEREGION:
					case COMPLEXREGION:
						FillRect (dc, &ps.rcPaint, br);
						break;
					default:
						break;
					}
					EndPaint (Window, &ps);
					//return 0;
				}
			}
		}
		break;

	case WM_DESTROY:
	case WM_KEYDOWN:
	case WM_SYSKEYDOWN:
		/*
	case WM_LBUTTONDOWN:
	case WM_RBUTTONDOWN:
	case WM_MBUTTONDOWN:
		*/
		if (MovieNotDone)
		{
			if (wParam == VK_ESCAPE || message == WM_CANCELMODE || message == WM_DESTROY)
			{
				control->Stop ();
				MovieNotDone = false;
				MovieInterrupted = true;
			}
		}
		if (message == WM_DESTROY)
		{
			MovieDestroyed = true;
		}
		return 0;

	case WM_GETMINMAXINFO:
		if (screen && !FullVideo)
		{
			LPMINMAXINFO mmi = (LPMINMAXINFO)lParam;
			RECT rect = { 0, 0, screen->GetWidth(), screen->GetHeight() };
			AdjustWindowRectEx(&rect, WS_VISIBLE|WS_OVERLAPPEDWINDOW, FALSE, WS_EX_APPWINDOW);
			mmi->ptMinTrackSize.x = rect.right - rect.left;
			mmi->ptMinTrackSize.y = rect.bottom - rect.top;
			return 0;
		}
		break;

	case WM_SETTINGCHANGE:
		// In case regional settings were changed, reget preferred languages
		language.Callback ();
		return 0;
	}
	return DefWindowProc (hWnd, message, wParam, lParam);
}

int I_PlayMovie (const char *name)
{
	HRESULT hr;
	int returnval = MOVIE_Failed;
	size_t namelen = strlen (name) + 1;
	wchar_t *uniname = new wchar_t[namelen];
	bool returnSound = false;
	bool runningFull = false;
	bool hotkey = false;
	size_t i;
	MSG msg;

	MovieNotDone = true;

	if (MultiByteToWideChar (CP_ACP, MB_PRECOMPOSED, name, -1, uniname, (int)namelen) == 0)
	{ // Could not convert name to Unicode
		goto bomb1;
	}

	// Convert slashes to backslashes because IGraphBuilder cannot deal with them
	for (i = 0; i < namelen; ++i)
	{
		if (uniname[i] == L'/')
			uniname[i] = L'\\';
	}

	if (FAILED(hr = CoCreateInstance (CLSID_FilterGraph, NULL, CLSCTX_INPROC_SERVER,
		IID_IGraphBuilder, (void **)&graph)))
	{
		goto bomb1;
	}

	control = NULL;
	event = NULL;
	vidwin = NULL;
	audio = NULL;
	video = NULL;

	if (FAILED(hr = graph->RenderFile (uniname, NULL)))
	{
		goto bomb2;
	}

	graph->QueryInterface (IID_IMediaControl, (void **)&control);
	graph->QueryInterface (IID_IMediaEventEx, (void **)&event);
	graph->QueryInterface (IID_IVideoWindow, (void **)&vidwin);
	graph->QueryInterface (IID_IBasicAudio, (void **)&audio);
	graph->QueryInterface (IID_IBasicVideo, (void **)&video);

	if (control == NULL || event == NULL)
	{
		goto bomb3;
	}

	GSnd->MovieDisableSound ();
	returnSound = true;

	CheckIfVideo ();
	SetTheVolume ();

	FullVideo = false;

	if (vidwin != NULL)
	{
		FullVideo = runningFull = screen->IsFullscreen ();

		// If the message drain cannot be set, then we simply won't be able
		// to catch mouse button presses in the video window.
		HRESULT drainhr = vidwin->put_MessageDrain ((OAHWND)Window);

		if (FullVideo)
		{
			// Try to avoid nasty palette flashes by clearing the screen to black.
			// Does not always work. :-(
			static_cast<Win32Video *> (Video)->BlankForGDI ();
			static_cast<Win32Video *> (Video)->GoFullscreen (false);
			static_cast<BaseWinFB *> (screen)->ReleaseResources ();
			if (FAILED (drainhr) || FAILED(hr = vidwin->put_FullScreenMode (OATRUE)))
			{
				SizeWindowForVideo ();
				FullVideo = false;
			}
		}
		if (!FullVideo)
		{
			vidwin->put_Owner ((OAHWND)Window);
			vidwin->put_WindowStyle (WS_CHILD | WS_CLIPSIBLINGS | WS_CLIPCHILDREN);
			SetMovieSize ();
		}
		else
		{
			RegisterHotKey (Window, 0, MOD_ALT, VK_TAB);
			hotkey = true;
		}
	}

	if (FAILED (hr = event->SetNotifyWindow ((OAHWND)Window, WM_GRAPHNOTIFY, 0)))
	{
		goto bomb3;
	}

	SetPriorityClass (GetCurrentProcess(), NORMAL_PRIORITY_CLASS);

	I_CheckNativeMouse (true);
	SetWindowLongPtr (Window, GWLP_WNDPROC, (LONG_PTR)MovieWndProc);

	if (FAILED (hr = control->Run ()))
	{
		goto bomb4;
	}

	MovieInterrupted = false;
	MovieDestroyed = false;

	while (MovieNotDone && GetMessage (&msg, NULL, 0, 0))
	{
		TranslateMessage (&msg);
		DispatchMessage (&msg);
	}

	returnval = MovieInterrupted ? MOVIE_Played_Aborted :
				NoVideo			 ? MOVIE_Played_NoVideo :
								   MOVIE_Played;

bomb4:
	SetWindowLongPtr (Window, GWLP_WNDPROC, (LONG_PTR)WndProc);
	SetPriorityClass (GetCurrentProcess(), INGAME_PRIORITY_CLASS);

bomb3:
	if (hotkey)
	{
		UnregisterHotKey (Window, 0);
	}
	if (vidwin != NULL)
	{
		if (!FullVideo)
		{
			vidwin->put_Visible (OAFALSE);
			vidwin->put_Owner (NULL);
		}
		vidwin->Release ();
	}
	if (video != NULL)
	{
		video->Release ();
	}
	if (audio != NULL)
	{
		audio->Release ();
	}
	if (event != NULL)
	{
		event->Release ();
	}
	if (control != NULL)
	{
		control->Release ();
	}

	SetActiveWindow (Window);

bomb2:
	graph->Release ();

	if (returnSound)
	{
		GSnd->MovieResumeSound ();
		C_FlushDisplay ();
	}
	if (runningFull)
	{
		static_cast<Win32Video *> (Video)->GoFullscreen (true);
		static_cast<BaseWinFB *> (screen)->CreateResources ();
	}

bomb1:
	delete[] uniname;

	if (MovieDestroyed)
	{
		exit (0);
	}

	return returnval;
}

static void CheckIfVideo ()
{
    long visible;

    if (vidwin == NULL || video == NULL)
    {
		NoVideo = true;
        return;
    }
	else if (vidwin->get_Visible (&visible) == E_NOINTERFACE)
	{
		NoVideo = true;
	}
	else
	{
		NoVideo = false;
	}

	if (NoVideo)
	{
		if (vidwin != NULL)
		{
			vidwin->Release ();
			vidwin = NULL;
		}

		DHUDMessage *message = new DHUDMessage (
			"This movie either does not contain a video stream "
			"or no suitable decompressor could be found.",
			1.5f, 0.375f, 0, 0, CR_RED, 1.f);
		StatusBar->AttachMessage (message);
		screen->Lock (false);
		StatusBar->Draw (HUD_None);
		StatusBar->DrawTopStuff (HUD_None);
		screen->Update ();
		StatusBar->DetachMessage (message);
		delete message;
	}
}

static void SetTheVolume ()
{
	if (audio != NULL)
	{
		// Convert snd_movievolume from a linear range to 1/100th
		// decibels and pass that to the IBasicAudio interface.
		long volume;

		if (snd_movievolume == 0.f)
		{
			volume = -10000;
		}
		else
		{
			volume = (long)(log10 (snd_movievolume) * 10000.0);
		}
		audio->put_Volume (volume);
	}
}

static void SizeWindowForVideo ()
{
	LONG width, height;

	if (video == NULL || FAILED (video->GetVideoSize (&width, &height)))
		return;

	SetWindowPos (Window, NULL, 0, 0,
		width + 2*GetSystemMetrics(SM_CXBORDER),
		height + GetSystemMetrics(SM_CYCAPTION) + 2*GetSystemMetrics(SM_CYBORDER),
		SWP_NOMOVE|SWP_NOOWNERZORDER);
}

static void SetMovieSize ()
{
	LONG width, height, left, top;
	RECT grc;
	HRESULT hr;

	GetClientRect (Window, &grc);

	hr = video->GetVideoSize (&width, &height);
	if (FAILED (hr))
	{
		vidwin->SetWindowPosition (0, 0, grc.right, grc.bottom);
		return;
	}

	double aspect = (double)height / (double)width;

	// Calculate size of video window that fits in our window
	if (grc.right > grc.bottom)
	{
		height = grc.bottom;
		width = (LONG)((double)height / aspect);
	}
	if (grc.right <= grc.bottom || width > grc.right)
	{
		width = grc.right;
		height = (LONG)((double)width * aspect);
	}

	// Then center it
	left = (grc.right - width) / 2;
	top = (grc.bottom - height) / 2;

	// And use it
	vidwin->SetWindowPosition (left, top, width, height);

	InvalidateRect (Window, NULL, FALSE);
}

#endif		// I_DO_NOT_LIKE_BIG_DOWNLOADS
