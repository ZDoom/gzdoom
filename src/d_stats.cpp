#if defined(_WIN32)
#define _WIN32_WINNT 0x0501
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
extern int sys_ostype;

#endif

#include <thread>
#include "c_cvars.h"
#include "x86.h"
#include "version.h"
#include "v_video.h"

EXTERN_CVAR(Bool, vid_glswfb)
extern int currentrenderer;
CVAR(String, sys_statshost, "gzstats.drdteam.org", CVAR_ARCHIVE|CVAR_GLOBALCONFIG|CVAR_NOSET)
CVAR(Int, sys_statsport, 80, CVAR_ARCHIVE|CVAR_GLOBALCONFIG|CVAR_NOSET)

// Each machine will only send two  reports, one when started with hardware rendering and one when started with software rendering.
CVAR(Bool, sentstats_swr, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOSET)
CVAR(Bool, sentstats_hwr, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOSET)

std::pair<double, bool> gl_getInfo();
bool I_HTTPRequest(const char* request);


static int GetOSVersion()
{
#ifdef _WIN32
	if (sys_ostype == 1) return 1;
	if (sizeof(void*) == 4)	// 32 bit
	{
		BOOL res;
		if (IsWow64Process(GetCurrentProcess(), &res))
		{
			return 6;
		}
		if (sys_ostype == 2) return 2;
		else return 4;
	}
	else
	{
		if (sys_ostype == 2) return 3;
		else return 5;
	}

#elif defined __APPLE__

	if (sizeof(void*) == 4)	// 32 bit
	{
		return 7;
	}
	else
	{
		return 8;
	}

#else

	// Todo: PPC + ARM

	if (sizeof(void*) == 4)	// 32 bit
	{
		return 11;
	}
	else
	{
		return 12;
	}


#endif
}

static int GetCoreInfo()
{
	int cores = std::thread::hardware_concurrency();
	if (CPU.HyperThreading) cores /= 2;
	return cores < 2? 0 : cores < 4? 1 : cores < 6? 2 : cores < 8? 3 : 4;
}

static int GetRenderInfo()
{
	if (currentrenderer == 0)
	{
		if (!screen->Accel2D) return 0;
		if (vid_glswfb) return 2;
		if (screen->LegacyHardware()) return 6;
		return 1;
	}
	else
	{
		auto info = gl_getInfo();
		if (info.first < 3.3) return 3;	// Legacy OpenGL. Don't care about Intel HD 3000 on Windows being run in 'risky' mode.
		if (!info.second) return 4;
		return 5;
	}
}

static void D_DoHTTPRequest(const char *request)
{
	if (I_HTTPRequest(request))
	{
		if (currentrenderer == 0)
		{
			cvar_forceset("sentstats_swr", "1");
		}
		else
		{
			cvar_forceset("sentstats_hwr", "1");
		}
	}
}

void D_DoAnonStats()
{
	static bool done = false;	// do this only once per session.
	if (done) return;
	done = true;

	// Do not repeat if already sent.
	if (currentrenderer == 0 && sentstats_swr) return;
	if (currentrenderer == 1 && sentstats_hwr) return;

	FStringf requeststring("GET /stats.php?render=%i&cores=%i&os=%i HTTP/1.1\nHost: %s\nConnection: close\nUser-Agent: %s %s\n\n",
		GetRenderInfo(), GetCoreInfo(), GetOSVersion(), sys_statshost.GetHumanString(), GAMENAME, VERSIONSTR);
	DPrintf(DMSG_NOTIFY, "Sending %s", requeststring);
	std::thread t1(D_DoHTTPRequest, requeststring);
	t1.detach();
}
