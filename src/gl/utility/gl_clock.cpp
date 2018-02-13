/*
** 
** This is a copy of the regular cycle_t from a time when that was based
** on QueryPerformanceCounter which is too costly for real-time profiling.
**
**---------------------------------------------------------------------------
** Copyright 1998-2016 Randy Heit
** Copyright 2007-2016 Christoph Oelckers
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

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <intrin.h>

#elif defined __APPLE__
#include <sys/sysctl.h>
#endif

#include <inttypes.h>

#include "i_system.h"
#include "g_level.h"
#include "c_console.h"
#include "c_dispatch.h"
#include "r_utility.h"
#include "v_video.h"
#include "g_levellocals.h"
#include "gl/utility/gl_clock.h"
#include "gl/utility/gl_convert.h"
#include "i_time.h"

glcycle_t RenderWall,SetupWall,ClipWall;
glcycle_t RenderFlat,SetupFlat;
glcycle_t RenderSprite,SetupSprite;
glcycle_t All, Finish, PortalAll, Bsp;
glcycle_t ProcessAll;
glcycle_t RenderAll;
glcycle_t Dirty;
glcycle_t drawcalls;
int vertexcount, flatvertices, flatprimitives;

int rendered_lines,rendered_flats,rendered_sprites,render_vertexsplit,render_texsplit,rendered_decals, rendered_portals;
int iter_dlightf, iter_dlight, draw_dlight, draw_dlightf;

double		gl_SecondsPerCycle = 1e-8;
double		gl_MillisecPerCycle = 1e-5;		// 100 MHz

// For GL timing the performance counter is far too costly so we still need RDTSC
// even though it may not be perfect.

void gl_CalculateCPUSpeed ()
{
	#ifdef _WIN32
		LARGE_INTEGER freq;

		QueryPerformanceFrequency (&freq);

		if (freq.QuadPart != 0)
		{
			LARGE_INTEGER count1, count2;
			unsigned minDiff;
			int64_t ClockCalibration = 0;

			// Count cycles for at least 55 milliseconds.
			// The performance counter is very low resolution compared to CPU
			// speeds today, so the longer we count, the more accurate our estimate.
			// On the other hand, we don't want to count too long, because we don't
			// want the user to notice us spend time here, since most users will
			// probably never use the performance statistics.
			minDiff = freq.LowPart * 11 / 200;

			// Minimize the chance of task switching during the testing by going very
			// high priority. This is another reason to avoid timing for too long.
			SetPriorityClass (GetCurrentProcess (), REALTIME_PRIORITY_CLASS);
			SetThreadPriority (GetCurrentThread (), THREAD_PRIORITY_TIME_CRITICAL);
			ClockCalibration = __rdtsc();
			QueryPerformanceCounter (&count1);
			do
			{
				QueryPerformanceCounter (&count2);
			} while ((uint32_t)((uint64_t)count2.QuadPart - (uint64_t)count1.QuadPart) < minDiff);
			ClockCalibration = __rdtsc() - ClockCalibration;
			QueryPerformanceCounter (&count2);
			SetPriorityClass (GetCurrentProcess (), NORMAL_PRIORITY_CLASS);
			SetThreadPriority (GetCurrentThread (), THREAD_PRIORITY_NORMAL);

			double CyclesPerSecond = (double)ClockCalibration *
				(double)freq.QuadPart /
				(double)((__int64)count2.QuadPart - (__int64)count1.QuadPart);
			gl_SecondsPerCycle = 1.0 / CyclesPerSecond;
			gl_MillisecPerCycle = 1000.0 / CyclesPerSecond;
		}
	#elif defined __APPLE__
		long long frequency;
		size_t size = sizeof frequency;

		if (0 == sysctlbyname("machdep.tsc.frequency", &frequency, &size, nullptr, 0) && 0 != frequency)
		{
			gl_SecondsPerCycle = 1.0 / frequency;
			gl_MillisecPerCycle = 1000.0 / frequency;
		}
	#endif
}


void ResetProfilingData()
{
	All.Reset();
	All.Clock();
	Bsp.Reset();
	PortalAll.Reset();
	RenderAll.Reset();
	ProcessAll.Reset();
	RenderWall.Reset();
	SetupWall.Reset();
	ClipWall.Reset();
	RenderFlat.Reset();
	SetupFlat.Reset();
	RenderSprite.Reset();
	SetupSprite.Reset();
	drawcalls.Reset();

	flatvertices=flatprimitives=vertexcount=0;
	render_texsplit=render_vertexsplit=rendered_lines=rendered_flats=rendered_sprites=rendered_decals=rendered_portals = 0;
}

//-----------------------------------------------------------------------------
//
// Rendering statistics
//
//-----------------------------------------------------------------------------

static void AppendRenderTimes(FString &str)
{
	double setupwall = SetupWall.TimeMS();
	double clipwall = ClipWall.TimeMS() - SetupWall.TimeMS();
	double bsp = Bsp.TimeMS() - ClipWall.TimeMS() - SetupFlat.TimeMS() - SetupSprite.TimeMS();

	str.AppendFormat("W: Render=%2.3f, Setup=%2.3f, Clip=%2.3f\n"
		"F: Render=%2.3f, Setup=%2.3f\n"
		"S: Render=%2.3f, Setup=%2.3f\n"
		"All=%2.3f, Render=%2.3f, Setup=%2.3f, BSP = %2.3f, Portal=%2.3f, Drawcalls=%2.3f, Finish=%2.3f\n",
	RenderWall.TimeMS(), setupwall, clipwall, RenderFlat.TimeMS(), SetupFlat.TimeMS(),
	RenderSprite.TimeMS(), SetupSprite.TimeMS(), All.TimeMS() + Finish.TimeMS(), RenderAll.TimeMS(),
	ProcessAll.TimeMS(), bsp, PortalAll.TimeMS(), drawcalls.TimeMS(), Finish.TimeMS());
}

static void AppendRenderStats(FString &out)
{
	out.AppendFormat("Walls: %d (%d splits, %d t-splits, %d vertices)\n"
		"Flats: %d (%d primitives, %d vertices)\n"
		"Sprites: %d, Decals=%d, Portals: %d\n",
		rendered_lines, render_vertexsplit, render_texsplit, vertexcount, rendered_flats, flatprimitives, flatvertices, rendered_sprites,rendered_decals, rendered_portals );
}

static void AppendLightStats(FString &out)
{
	out.AppendFormat("DLight - Walls: %d processed, %d rendered - Flats: %d processed, %d rendered\n", 
		iter_dlight, draw_dlight, iter_dlightf, draw_dlightf );
}

ADD_STAT(rendertimes)
{
	static FString buff;
	static int64_t lasttime=0;
	int64_t t=I_msTime();
	if (t-lasttime>1000) 
	{
		buff.Truncate(0);
		AppendRenderTimes(buff);
		lasttime=t;
	}
	return buff;
}

ADD_STAT(renderstats)
{
	FString out;
	AppendRenderStats(out);
	return out;
}

ADD_STAT(lightstats)
{
	FString out;
	AppendLightStats(out);
	return out;
}

void AppendMissingTextureStats(FString &out);


static int printstats;
static bool switchfps;
static uint64_t waitstart;
EXTERN_CVAR(Bool, vid_fps)

void CheckBench()
{
	if (printstats && ConsoleState == c_up)
	{
		// if we started the FPS counter ourselves or ran from the console 
		// we need to wait for it to stabilize before using it.
		if (waitstart > 0 && I_msTime() - waitstart < 5000) return;

		FString compose;

		compose.Format("Map %s: \"%s\",\nx = %1.4f, y = %1.4f, z = %1.4f, angle = %1.4f, pitch = %1.4f\n",
			level.MapName.GetChars(), level.LevelName.GetChars(), r_viewpoint.Pos.X, r_viewpoint.Pos.Y, r_viewpoint.Pos.Z, r_viewpoint.Angles.Yaw.Degrees, r_viewpoint.Angles.Pitch.Degrees);
		
		AppendRenderStats(compose);
		AppendRenderTimes(compose);
		AppendLightStats(compose);
		AppendMissingTextureStats(compose);
		compose.AppendFormat("%" PRIu64 " fps\n\n", screen->GetLastFPS());

		FILE *f = fopen("benchmarks.txt", "at");
		if (f != NULL)
		{
			fputs(compose.GetChars(), f);
			fclose(f);
		}
		Printf("Benchmark info saved\n");
		if (switchfps) vid_fps = false;
		printstats = false;
	}
}

CCMD(bench)
{
	printstats = true;
	if (vid_fps == 0) 
	{
		vid_fps = 1;
		waitstart = I_msTime();
		switchfps = true;
	}
	else
	{
		if (ConsoleState == c_up) waitstart = I_msTime();
		switchfps = false;
	}
	C_HideConsole ();
}

bool gl_benching = false;

void  checkBenchActive()
{
	FStat *stat = FStat::FindStat("rendertimes");
	gl_benching = ((stat != NULL && stat->isActive()) || printstats);
}

