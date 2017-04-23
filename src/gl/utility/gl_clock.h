#ifndef __GL_CLOCK_H
#define __GL_CLOCK_H

#include "stats.h"
#include "x86.h"
#include "m_fixed.h"

extern bool gl_benching;

extern double gl_SecondsPerCycle;
extern double gl_MillisecPerCycle;

#ifdef _MSC_VER

__forceinline int64_t GetClockCycle ()
{
	return __rdtsc();
}

#elif defined __APPLE__ && (defined __i386__ || defined __x86_64__)

inline int64_t GetClockCycle()
{
	return __builtin_ia32_rdtsc();
}

#elif defined(__GNUG__) && defined(__i386__)

inline int64_t GetClockCycle()
{
	if (CPU.bRDTSC)
	{
		int64_t res;
		asm volatile ("rdtsc" : "=A" (res));
		return res;
	}
	else
	{
		return 0;
	}	
}

#else

inline int64_t GetClockCycle ()
{
	return 0;
}
#endif

class glcycle_t
{
public:
	glcycle_t &operator= (const glcycle_t &o)
	{
		Counter = o.Counter;
		return *this;
	}

	void Reset()
	{
		Counter = 0;
	}
	
	__forceinline void Clock()
	{
		// Not using QueryPerformanceCounter directly, so we don't need
		// to pull in the Windows headers for every single file that
		// wants to do some profiling.
		int64_t time = (gl_benching? GetClockCycle() : 0);
		Counter -= time;
	}
	
	__forceinline void Unclock()
	{
		int64_t time = (gl_benching? GetClockCycle() : 0);
		Counter += time;
	}
	
	double Time()
	{
		return double(Counter) * gl_SecondsPerCycle;
	}
	
	double TimeMS()
	{
		return double(Counter) * gl_MillisecPerCycle;
	}

private:
	int64_t Counter;
};

extern glcycle_t RenderWall,SetupWall,ClipWall;
extern glcycle_t RenderFlat,SetupFlat;
extern glcycle_t RenderSprite,SetupSprite;
extern glcycle_t All, Finish, PortalAll, Bsp;
extern glcycle_t ProcessAll;
extern glcycle_t RenderAll;
extern glcycle_t Dirty;
extern glcycle_t drawcalls;

extern int iter_dlightf, iter_dlight, draw_dlight, draw_dlightf;
extern int rendered_lines,rendered_flats,rendered_sprites,rendered_decals,render_vertexsplit,render_texsplit;
extern int rendered_portals;

extern int vertexcount, flatvertices, flatprimitives;

void ResetProfilingData();
void CheckBench();
void  checkBenchActive();


#endif
