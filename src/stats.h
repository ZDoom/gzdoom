/*
** stats.h
**
**---------------------------------------------------------------------------
** Copyright 1998-2001 Randy Heit
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

extern "C" BOOL HaveRDTSC;
extern "C" double SecondsPerCycle;
extern "C" double CyclesPerSecond;

typedef DWORD cycle_t;

#if _MSC_VER
inline cycle_t GetClockCycle ()
{
	if (HaveRDTSC)
	{
		cycle_t res;
		__asm {
			xor		eax,eax
			xor		edx,edx
			_emit	0x0f
			_emit	0x31
			mov		res,eax
		}
		return res;
	}
	else
	{
		return 0;
	}
}
#elif (defined __GNUG__)
inline cycle_t GetClockCycle ()
{
	if (HaveRDTSC)
	{
		cycle_t res;
		asm volatile ("rdtsc" : "=a" (res) : : "%edx");
		return res;
	}
	else
	{
		return 0;
	}	
}
#else
inline cycle_t GetClockCycle ()
{
	return 0;
}
#endif

#define clock(v)	{v -= GetClockCycle();}
#define unclock(v)	{v += GetClockCycle() /*- 41*/;}

class FStat
{
public:
	FStat (const char *name);
	virtual ~FStat ();

	virtual void GetStats (char *out) = 0;
	static void PrintStat ();
	static FStat *FindStat (const char *name);
	static void SelectStat (const char *name);
	static void SelectStat (FStat *stat);
	static void ToggleStat (const char *name);
	static void ToggleStat (FStat *stat);
	inline static FStat *ActiveStat () { return m_CurrStat; }
	static void DumpRegisteredStats ();

private:
	FStat *m_Next;
	const char *m_Name;
	static FStat *m_FirstStat;
	static FStat *m_CurrStat;
};

#define ADD_STAT(n,out) \
	static class Stat_##n : public FStat { \
		public: \
			Stat_##n () : FStat (#n) {} \
		void GetStats (char *out); } Istaticstat##n; \
	void Stat_##n::GetStats (char *out)

#endif //__STATS_H__
