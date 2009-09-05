#ifndef X86_H
#define X86_H

#include "basictypes.h"

struct CPUInfo	// 92 bytes
{
	union
	{
		char VendorID[16];
		uint32 dwVendorID[4];
	};
	union
	{
		char CPUString[48];
		uint32 dwCPUString[12];
	};

	BYTE Stepping;
	BYTE Model;
	BYTE Family;
	BYTE Type;

	union
	{
		struct
		{
			BYTE BrandIndex;
			BYTE CLFlush;
			BYTE CPUCount;
			BYTE APICID;

			uint32 bSSE3:1;
			uint32 DontCare1:8;
			uint32 bSSSE3:1;
			uint32 DontCare1a:9;
			uint32 bSSE41:1;
			uint32 bSSE42:1;
			uint32 DontCare2a:11;

			uint32 bFPU:1;
			uint32 bVME:1;
			uint32 bDE:1;
			uint32 bPSE:1;
			uint32 bRDTSC:1;
			uint32 bMSR:1;
			uint32 bPAE:1;
			uint32 bMCE:1;
			uint32 bCX8:1;
			uint32 bAPIC:1;
			uint32 bReserved1:1;
			uint32 bSEP:1;
			uint32 bMTRR:1;
			uint32 bPGE:1;
			uint32 bMCA:1;
			uint32 bCMOV:1;
			uint32 bPAT:1;
			uint32 bPSE36:1;
			uint32 bPSN:1;
			uint32 bCFLUSH:1;
			uint32 bReserved2:1;
			uint32 bDS:1;
			uint32 bACPI:1;
			uint32 bMMX:1;
			uint32 bFXSR:1;
			uint32 bSSE:1;
			uint32 bSSE2:1;
			uint32 bSS:1;
			uint32 bHTT:1;
			uint32 bTM:1;
			uint32 bReserved3:1;
			uint32 bPBE:1;

			uint32 DontCare2:22;
			uint32 bMMXPlus:1;		// AMD's MMX extensions
			uint32 bMMXAgain:1;		// Just a copy of bMMX above
			uint32 DontCare3:6;
			uint32 b3DNowPlus:1;
			uint32 b3DNow:1;
		};
		uint32 FeatureFlags[4];
	};

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
		uint32 AMD_DataL1Info;
	};
};


extern "C" CPUInfo CPU;
struct PalEntry;

void CheckCPUID (CPUInfo *cpu);
void DumpCPUInfo (const CPUInfo *cpu);
void DoBlending_SSE2(const PalEntry *from, PalEntry *to, int count, int r, int g, int b, int a);

#endif

