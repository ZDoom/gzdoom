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
#else
inline cycle_t GetClockCycle ()
{
	return 0;
}
#endif

#define clock(v)	{v -= GetClockCycle();}
#define unclock(v)	{v += GetClockCycle() - 41;}

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

#define BEGIN_STAT(n) \
	static class Stat_##n : public FStat { \
		public: \
			Stat_##n () : FStat (#n) {} \
			void GetStats (char *out)

#define END_STAT(n)		} Istaticstat##n;

#endif //__STATS_H__
