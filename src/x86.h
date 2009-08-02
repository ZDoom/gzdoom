#ifndef X86_H
#define X86_H

#include "basictypes.h"

struct CPUInfo	// 92 bytes
{
	union
	{
		char VendorID[16];
		DWORD dwVendorID[4];
	};
	union
	{
		char CPUString[48];
		DWORD dwCPUString[12];
	};

	union
	{
		struct
		{
			BYTE Stepping;
			BYTE Model;
			BYTE Family;
			BYTE Type;

			BYTE BrandIndex;
			BYTE CLFlush;
			BYTE CPUCount;
			BYTE APICID;

			DWORD bSSE3:1;
			DWORD DontCare1:8;
			DWORD bSSSE3:1;
			DWORD DontCare1a:9;
			DWORD bSSE41:1;
			DWORD bSSE42:1;
			DWORD DontCare2a:11;
		};
		DWORD FeatureFlags[3];
	};

	DWORD bFPU:1;
	DWORD bVME:1;
	DWORD bDE:1;
	DWORD bPSE:1;
	DWORD bRDTSC:1;
	DWORD bMSR:1;
	DWORD bPAE:1;
	DWORD bMCE:1;
	DWORD bCX8:1;
	DWORD bAPIC:1;
	DWORD bReserved1:1;
	DWORD bSEP:1;
	DWORD bMTRR:1;
	DWORD bPGE:1;
	DWORD bMCA:1;
	DWORD bCMOV:1;
	DWORD bPAT:1;
	DWORD bPSE36:1;
	DWORD bPSN:1;
	DWORD bCFLUSH:1;
	DWORD bReserved2:1;
	DWORD bDS:1;
	DWORD bACPI:1;
	DWORD bMMX:1;
	DWORD bFXSR:1;
	DWORD bSSE:1;
	DWORD bSSE2:1;
	DWORD bSS:1;
	DWORD bHTT:1;
	DWORD bTM:1;
	DWORD bReserved3:1;
	DWORD bPBE:1;

	DWORD DontCare2:22;
	DWORD bMMXPlus:1;		// AMD's MMX extensions
	DWORD bMMXAgain:1;		// Just a copy of bMMX above
	DWORD DontCare3:6;
	DWORD b3DNowPlus:1;
	DWORD b3DNow:1;

	BYTE AMDStepping;
	BYTE AMDModel;
	BYTE AMDFamily;
	BYTE bIsAMD;

	union
	{
		struct
		{
			BYTE DataL1LineSize;
			BYTE DataL1LinesPerTag;
			BYTE DataL1Associativity;
			BYTE DataL1SizeKB;
		};
		DWORD AMD_DataL1Info;
	};
};


extern "C" CPUInfo CPU;
struct PalEntry;

void CheckCPUID (CPUInfo *cpu);
void DumpCPUInfo (const CPUInfo *cpu);
void DoBlending_SSE2(const PalEntry *from, PalEntry *to, int count, int r, int g, int b, int a);

#endif

