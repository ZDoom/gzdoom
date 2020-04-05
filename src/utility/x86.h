#ifndef X86_H
#define X86_H

#include "basictypes.h"

struct CPUInfo	// 92 bytes
{
	union
	{
		char VendorID[16];
		uint32_t dwVendorID[4];
	};
	union
	{
		char CPUString[48];
		uint32_t dwCPUString[12];
	};

	uint8_t Stepping;
	uint8_t Model;
	uint8_t Family;
	uint8_t Type;
	uint8_t HyperThreading;

	union
	{
		struct
		{
			uint8_t BrandIndex;
			uint8_t CLFlush;
			uint8_t CPUCount;
			uint8_t APICID;

			uint32_t bSSE3:1;
			uint32_t DontCare1:8;
			uint32_t bSSSE3:1;
			uint32_t DontCare1a:9;
			uint32_t bSSE41:1;
			uint32_t bSSE42:1;
			uint32_t DontCare2a:11;

			uint32_t bFPU:1;
			uint32_t bVME:1;
			uint32_t bDE:1;
			uint32_t bPSE:1;
			uint32_t bRDTSC:1;
			uint32_t bMSR:1;
			uint32_t bPAE:1;
			uint32_t bMCE:1;
			uint32_t bCX8:1;
			uint32_t bAPIC:1;
			uint32_t bReserved1:1;
			uint32_t bSEP:1;
			uint32_t bMTRR:1;
			uint32_t bPGE:1;
			uint32_t bMCA:1;
			uint32_t bCMOV:1;
			uint32_t bPAT:1;
			uint32_t bPSE36:1;
			uint32_t bPSN:1;
			uint32_t bCFLUSH:1;
			uint32_t bReserved2:1;
			uint32_t bDS:1;
			uint32_t bACPI:1;
			uint32_t bMMX:1;
			uint32_t bFXSR:1;
			uint32_t bSSE:1;
			uint32_t bSSE2:1;
			uint32_t bSS:1;
			uint32_t bHTT:1;
			uint32_t bTM:1;
			uint32_t bReserved3:1;
			uint32_t bPBE:1;

			uint32_t DontCare2:22;
			uint32_t bMMXPlus:1;		// AMD's MMX extensions
			uint32_t bMMXAgain:1;		// Just a copy of bMMX above
			uint32_t DontCare3:6;
			uint32_t b3DNowPlus:1;
			uint32_t b3DNow:1;
		};
		uint32_t FeatureFlags[4];
	};

	uint8_t AMDStepping;
	uint8_t AMDModel;
	uint8_t AMDFamily;
	uint8_t bIsAMD;

	union
	{
		struct
		{
			uint8_t DataL1LineSize;
			uint8_t DataL1LinesPerTag;
			uint8_t DataL1Associativity;
			uint8_t DataL1SizeKB;
		};
		uint32_t AMD_DataL1Info;
	};
};


extern CPUInfo CPU;
struct PalEntry;

void CheckCPUID (CPUInfo *cpu);
void DumpCPUInfo (const CPUInfo *cpu);

#endif

