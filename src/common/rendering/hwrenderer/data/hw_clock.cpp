/*
** 
**  Hardware render profiling info
**
**---------------------------------------------------------------------------
** Copyright 2007-2018 Christoph Oelckers
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


#include "c_console.h"
#include "c_dispatch.h"
#include "v_video.h"
#include "hw_clock.h"
#include "i_time.h"
#include "i_interface.h"
#include "printf.h"

glcycle_t RenderWall,SetupWall,ClipWall;
glcycle_t RenderFlat,SetupFlat;
glcycle_t RenderSprite,SetupSprite;
glcycle_t All, Finish, PortalAll, Bsp;
glcycle_t ProcessAll, PostProcess;
glcycle_t RenderAll;
glcycle_t Dirty;
glcycle_t drawcalls;
glcycle_t twoD, Flush3D;
glcycle_t MTWait, WTTotal;
int vertexcount, flatvertices, flatprimitives;

int rendered_lines,rendered_flats,rendered_sprites,render_vertexsplit,render_texsplit,rendered_decals, rendered_portals, rendered_commandbuffers;
int iter_dlightf, iter_dlight, draw_dlight, draw_dlightf;

void ResetProfilingData()
{
	All.Reset();
	All.Clock();
	Bsp.Reset();
	PortalAll.Reset();
	RenderAll.Reset();
	ProcessAll.Reset();
	PostProcess.Reset();
	RenderWall.Reset();
	SetupWall.Reset();
	ClipWall.Reset();
	RenderFlat.Reset();
	SetupFlat.Reset();
	RenderSprite.Reset();
	SetupSprite.Reset();
	drawcalls.Reset();
	MTWait.Reset();
	WTTotal.Reset();

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
	double clipwall = ClipWall.TimeMS();
	double bsp = Bsp.TimeMS() - ClipWall.TimeMS();

	str.AppendFormat("BSP = %2.3f, Clip=%2.3f\n"
		"W: Render=%2.3f, Setup=%2.3f\n"
		"F: Render=%2.3f, Setup=%2.3f\n"
		"S: Render=%2.3f, Setup=%2.3f\n"
		"2D: %2.3f Finish3D: %2.3f\n"
		"Main thread total=%2.3f, Main thread waiting=%2.3f Worker thread total=%2.3f, Worker thread waiting=%2.3f\n"
		"All=%2.3f, Render=%2.3f, Setup=%2.3f, Portal=%2.3f, Drawcalls=%2.3f, Postprocess=%2.3f, Finish=%2.3f\n",
		bsp, clipwall,
		RenderWall.TimeMS(), setupwall, 
		RenderFlat.TimeMS(), SetupFlat.TimeMS(),
		RenderSprite.TimeMS(), SetupSprite.TimeMS(), 
		twoD.TimeMS(), Flush3D.TimeMS() - twoD.TimeMS(),
		MTWait.TimeMS() + Bsp.TimeMS(), MTWait.TimeMS(), WTTotal.TimeMS(), WTTotal.TimeMS() - setupwall - SetupFlat.TimeMS() - SetupSprite.TimeMS(),
		All.TimeMS() + Finish.TimeMS(), RenderAll.TimeMS(),	ProcessAll.TimeMS(), PortalAll.TimeMS(), drawcalls.TimeMS(), PostProcess.TimeMS(), Finish.TimeMS());
}

static void AppendRenderStats(FString &out)
{
	out.AppendFormat("Walls: %d (%d splits, %d t-splits, %d vertices)\n"
		"Flats: %d (%d primitives, %d vertices)\n"
		"Sprites: %d, Decals=%d, Portals: %d, Command buffers: %d\n",
		rendered_lines, render_vertexsplit, render_texsplit, vertexcount, rendered_flats, flatprimitives, flatvertices, rendered_sprites,rendered_decals, rendered_portals, rendered_commandbuffers );
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

		if (sysCallbacks.GetLocationDescription) compose = sysCallbacks.GetLocationDescription();

		AppendRenderStats(compose);
		AppendRenderTimes(compose);
		AppendLightStats(compose);
		compose << "\n\n\n";

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

bool glcycle_t::active = false;

void  checkBenchActive()
{
	FStat *stat = FStat::FindStat("rendertimes");
	glcycle_t::active = ((stat != NULL && stat->isActive()) || printstats);
}

