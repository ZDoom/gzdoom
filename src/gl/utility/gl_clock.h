#ifndef __GL_CLOCK_H
#define __GL_CLOCK_H

#include "stats.h"
#include "x86.h"
#include "m_fixed.h"

extern bool gl_benching;

#ifdef _MSC_VER

extern double gl_SecondsPerCycle;
extern double gl_MillisecPerCycle;

__forceinline long long GetClockCycle ()
{
#if _M_X64
	return __rdtsc();
#else
	return CPU.bRDTSC ? __rdtsc() : 0;
#endif
}

#elif defined(__GNUG__) && defined(__i386__)

extern double gl_SecondsPerCycle;
extern double gl_MillisecPerCycle;

inline long long GetClockCycle()
{
	if (CPU.bRDTSC)
	{
		long long res;
		asm volatile ("rdtsc" : "=A" (res));
		return res;
	}
	else
	{
		return 0;
	}	
}

#else

extern double gl_SecondsPerCycle;
extern double gl_MillisecPerCycle;

inline long long GetClockCycle ()
{
	return 0;
}
#endif

#if defined (__APPLE__)

typedef cycle_t glcycle_t;

#else // !__APPLE__

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
		long long time = (gl_benching? GetClockCycle() : 0);
		Counter -= time;
	}
	
	__forceinline void Unclock()
	{
		long long time = (gl_benching? GetClockCycle() : 0);
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
	long long Counter;
};

#endif // __APPLE__

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