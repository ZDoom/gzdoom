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

#ifndef _WIN32

#if defined (__APPLE__)


#include <mach/mach_time.h>


class cycle_t
{
public:
	cycle_t()
	{
		if ( !s_initialized )
		{
			mach_timebase_info( &s_info );
			s_initialized = true;
		}
		
		Reset();
	}
	
	cycle_t &operator=( const cycle_t &other )
	{
		if ( &other != this )
		{
			m_seconds = other.m_seconds;

		}
		
		return *this;
	}
	
	void Reset()
	{
		m_seconds = 0;
	}
	
	void Clock()
	{
		m_seconds -= Nanoseconds() * 1e-9;
	}
	
	void Unclock()
	{
		m_seconds += Nanoseconds() * 1e-9;
	}
	
	double Time()
	{
		return m_seconds;
	}
	
	double TimeMS()
	{
		return m_seconds * 1e3;
	}
	
private:
	double m_seconds;
	
	static mach_timebase_info_data_t s_info;
	static bool s_initialized;
	
	uint64_t Nanoseconds() const
	{
		return mach_absolute_time() * s_info.numer / s_info.denom;
	}
	
};

#else // !__APPLE__

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
	cycle_t &operator= (const cycle_t &o)
	{
		Sec = o.Sec;
		return *this;
	}

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

#endif // __APPLE__

#else

// Windows
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
#ifndef _M_X64
	if (CPU.bRDTSC)
#endif
	{
		return __rdtsc();
	}
	return 0;
}
#else
inline unsigned long long rdtsc()
{
#ifndef __amd64__
	if (CPU.bRDTSC)
#endif
	{
		register unsigned long long tsc;
		asm volatile ("\trdtsc\n" : "=A" (tsc));
		return tsc;
	}
	return 0;
}
#endif

class cycle_t
{
public:
	cycle_t &operator= (const cycle_t &o)
	{
		Counter = o.Counter;
		return *this;
	}

	void Reset()
	{
		Counter = 0;
	}
	
	void Clock()
	{
		long long time = rdtsc();
		Counter -= time;
	}
	
	void Unclock()
	{
		long long time = rdtsc();
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

	long long GetRawCounter()
	{
		return Counter;
	}

private:
	long long Counter;
};

#endif

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

	static void PrintStat ();
	static FStat *FindStat (const char *name);
	static void ToggleStat (const char *name);
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
