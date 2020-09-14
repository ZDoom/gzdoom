/*
** stats.h
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

#ifndef __STATS_H__
#define __STATS_H__

#include "zstring.h"

#if !defined _WIN32 && !defined __APPLE__

#ifdef NO_CLOCK_GETTIME
class cycle_t
{
public:
	cycle_t &operator= (const cycle_t &o) { return *this; }
	void Reset() {}
	void Clock() {}
	void Unclock() {}
	double Time() { return 0; }
	double TimeMS() { return 0; }
};

#else

#include <time.h>

class cycle_t
{
public:
	void Reset()
	{
		Sec = 0;
	}
	
	void Clock()
	{
		timespec ts;
		
		clock_gettime(CLOCK_MONOTONIC, &ts);
		Sec -= ts.tv_sec + ts.tv_nsec * 1e-9;
	}
	
	void Unclock()
	{
		timespec ts;
		
		clock_gettime(CLOCK_MONOTONIC, &ts);
		Sec += ts.tv_sec + ts.tv_nsec * 1e-9;
	}
	
	double Time()
	{
		return Sec;
	}
	
	double TimeMS()
	{
		return Sec * 1e3;
	}

private:
	double Sec;
};

#endif

#else

// Windows and macOS
#include "x86.h"

extern double PerfToSec, PerfToMillisec;

#ifdef _MSC_VER
// Trying to include intrin.h here results in some bizarre errors, so I'm just
// going to duplicate the function prototype instead.
//#include <intrin.h>
extern "C" unsigned __int64 __rdtsc(void);
#pragma intrinsic(__rdtsc)
inline unsigned __int64 rdtsc()
{
	return __rdtsc();
}
#elif defined __APPLE__ && (defined __i386__ || defined __x86_64__)
inline uint64_t rdtsc()
{
	return __builtin_ia32_rdtsc();
}
#else
inline uint64_t rdtsc()
{
#ifdef __amd64__
	uint64_t tsc;
	asm volatile ("rdtsc; shlq $32, %%rdx; orq %%rdx, %%rax" : "=a" (tsc) :: "%rdx");
	return tsc;
#elif defined __ppc__
	unsigned int lower, upper, temp;
	do
	{
		asm volatile ("mftbu %0 \n mftb %1 \n mftbu %2 \n" 
			: "=r"(upper), "=r"(lower), "=r"(temp));
	}
	while (upper != temp);
	return (static_cast<unsigned long long>(upper) << 32) | lower;
#elif defined __aarch64__
	// TODO: Implement and test on ARM64
	return 0;
#else // i386
	if (CPU.bRDTSC)
	{
		uint64_t tsc;
		asm volatile ("\trdtsc\n" : "=A" (tsc));
		return tsc;
	}
	return 0;
#endif // __amd64__
}
#endif

class cycle_t
{
public:
	void Reset()
	{
		Counter = 0;
	}
	
	void Clock()
	{
		int64_t time = rdtsc();
		Counter -= time;
	}
	
	void Unclock(bool checkvar = true)
	{
		int64_t time = rdtsc();
		Counter += time;
	}
	
	double Time()
	{
		return Counter * PerfToSec;
	}
	
	double TimeMS()
	{
		return Counter * PerfToMillisec;
	}

	int64_t GetRawCounter()
	{
		return Counter;
	}

private:
	int64_t Counter;
};

#endif

class glcycle_t : public cycle_t
{
public:
	static bool active;
	void Clock()
	{
		if (active) cycle_t::Clock();		
	}

	void Unclock()
	{
		if (active) cycle_t::Unclock();
	}
};

// Helper for code that uses a timer and has multiple exit points.
class Clocker
{
public:

	explicit Clocker(glcycle_t& clck)
		: clock(clck)
	{
		clock.Clock();
	}

	~Clocker()
	{	// unlock
		clock.Unclock();
	}

	Clocker(const Clocker&) = delete;
	Clocker& operator=(const Clocker&) = delete;
private:
	glcycle_t & clock;
};


class F2DDrawer;

class FStat
{
public:
	FStat (const char *name);
	virtual ~FStat ();

	virtual FString GetStats () = 0;

	void ToggleStat ();
	bool isActive() const
	{
		return m_Active;
	}

	static void PrintStat (F2DDrawer *drawer);
	static FStat *FindStat (const char *name);
	static void ToggleStat (const char *name);
	static void EnableStat(const char* name, bool on);
	static void DumpRegisteredStats ();

private:
	FStat *m_Next;
	const char *m_Name;
	bool m_Active;

	static FStat *FirstStat;
};

#define ADD_STAT(n) \
	static class Stat_##n : public FStat { \
		public: \
			Stat_##n () : FStat (#n) {} \
		FString GetStats (); } Istaticstat##n; \
	FString Stat_##n::GetStats ()

#endif //__STATS_H__
