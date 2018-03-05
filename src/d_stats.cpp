#if defined(_WIN32)
#include <thread>

EXTERN_CVAR(Bool, gl_legacy_mode)
EXTERN_CVAR(Bool, vid_glswfb)
extern int currentrenderer, sys_ostype, restart;
CVAR(String, sys_statshost, "gzstats.drdteam.org", CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR(Int, sys_statsport, 80, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
void D_DoHTTPRequest(char* request);

#endif

void D_DoAnonStats()
{
#if defined(_WIN32)
	uint8_t astat_render, astat_sysbits;
	static char* requeststring = new char[512];
	// astat_render:
	//  0: Unaccelerated (Software)
	//  1: Direct3D (Software)
	//  2: OpenGL (Software)
	//  3: Legacy OpenGL mode
	//  4: Modern OpenGL (>3.3) path

	// sys_ostype:
	//  0: unknown/outdated
	//  1: legacy (XP/Vista)
	//  2: supported (7/8/8.1)
	//  3: modern (10+)

	// astat_sysbits:
	//  0: 32-bit
	//  1: 64-bit

	if (!restart)
	{
		astat_render = (currentrenderer == 1) ?
			(gl_legacy_mode ? 3 : 4) : // opengl
			(!(screen->Accel2D)) ? 0 : (vid_glswfb ? 2 : 1); // software
#ifdef _WIN64
		astat_sysbits = 1;
#else
		astat_sysbits = 0;
#endif
		sprintf(requeststring, "GET /stats.php?render=%i&bits=%i&os=%i HTTP/1.1\nHost: %s\nConnection: close\nUser-Agent: %s %s\n\n",
			astat_render, astat_sysbits, sys_ostype, sys_statshost.GetHumanString(), GAMENAME, VERSIONSTR);
		//Printf("%s", requeststring);
		std::thread t1(D_DoHTTPRequest, requeststring);
		t1.detach();
	}
#endif
}
