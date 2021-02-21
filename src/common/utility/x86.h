#ifndef X86_H
#define X86_H

#include "basics.h"
#include "zstring.h"

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
			uint32_t bPCLMULQDQ:1;
			uint32_t bDTES64:1;
			uint32_t bMONITOR:1;
			uint32_t bDSCPL:1;
			uint32_t bVMX:1;
			uint32_t bSMX:1;
			uint32_t bEST:1;
			uint32_t bTM2:1;
			uint32_t bSSSE3:1;
			uint32_t bCNXTID:1;
			uint32_t bSDBG:1;
			uint32_t bFMA3:1;
			uint32_t bCX16:1;
			uint32_t bXTPR:1;
			uint32_t bPDCM:1;
			uint32_t bReverved1:1;
			uint32_t bPCID:1;
			uint32_t bDCA:1;
			uint32_t bSSE41:1;
			uint32_t bSSE42:1;
			uint32_t bX2APIC:1;
			uint32_t bMOVBE:1;
			uint32_t bPOPCNT:1;
			uint32_t bTSCDL:1;
			uint32_t bAES:1;
			uint32_t bXSAVE:1;
			uint32_t bOSXSAVE:1;
			uint32_t bAVX:1;
			uint32_t bF16C:1;
			uint32_t bRDRND:1;
			uint32_t bHypervisor:1;

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
			uint32_t bReserved2:1;
			uint32_t bSEP:1;
			uint32_t bMTRR:1;
			uint32_t bPGE:1;
			uint32_t bMCA:1;
			uint32_t bCMOV:1;
			uint32_t bPAT:1;
			uint32_t bPSE36:1;
			uint32_t bPSN:1;
			uint32_t bCFLUSH:1;
			uint32_t bReserved3:1;
			uint32_t bDS:1;
			uint32_t bACPI:1;
			uint32_t bMMX:1;
			uint32_t bFXSR:1;
			uint32_t bSSE:1;
			uint32_t bSSE2:1;
			uint32_t bSS:1;
			uint32_t bHTT:1;
			uint32_t bTM:1;
			uint32_t bReserved4:1;
			uint32_t bPBE:1;

			uint32_t DontCare2:22;
			uint32_t bMMXPlus:1;		// AMD's MMX extensions
			uint32_t bMMXAgain:1;		// Just a copy of bMMX above
			uint32_t DontCare3:6;
			uint32_t b3DNowPlus:1;
			uint32_t b3DNow:1;

			uint32_t bFSGSBASE:1;
			uint32_t bIA32_TSC_ADJUST:1;
			uint32_t bSGX:1;
			uint32_t bBMI1:1;
			uint32_t bHLE:1;
			uint32_t bAVX2:1;
			uint32_t bFDP_EXCPTN_ONLY:1;
			uint32_t bSMEP:1;
			uint32_t bBMI2:1;
			uint32_t bERMS:1;
			uint32_t bINVPCID:1;
			uint32_t bRTM:1;
			uint32_t bPQM:1;
			uint32_t bFPU_CS_DS:1;
			uint32_t bMPX:1;
			uint32_t bPQE:1;
			uint32_t bAVX512_F:1;
			uint32_t bAVX512_DQ:1;
			uint32_t bRDSEED:1;
			uint32_t bADX:1;
			uint32_t bSMAP:1;
			uint32_t bAVX512_IFMA:1;
			uint32_t bPCOMMIT:1;
			uint32_t bCLFLUSHOPT:1;
			uint32_t bCLWB:1;
			uint32_t bINTEL_PT:1;
			uint32_t bAVX512_PF:1;
			uint32_t bAVX512_ER:1;
			uint32_t bAVX512_CD:1;
			uint32_t bSHA:1;
			uint32_t bAVX512_BW:1;
			uint32_t bAVX512_VL:1;

			uint32_t bPREFETCHWT1:1;
			uint32_t bAVX512_VBMI:1;
			uint32_t bUMIP:1;
			uint32_t bPKU:1;
			uint32_t bOSPKE:1;
			uint32_t bWAITPKG:1;
			uint32_t bAVX512_VBMI2:1;
			uint32_t bCET_SS:1;
			uint32_t bGFNI:1;
			uint32_t bVAES:1;
			uint32_t bVPCLMULQDQ:1;
			uint32_t bAVX512_VNNI:1;
			uint32_t bAVX512_BITALG:1;
			uint32_t bReserved5:1;
			uint32_t bAVX512_VPOPCNTDQ:1;
			uint32_t bReserved6:1;
			uint32_t b5L_PAGING:1;
			uint32_t MAWAU:5;
			uint32_t bRDPID:1;
			uint32_t bReserved7:1;
			uint32_t bReserved8:1;
			uint32_t bCLDEMOTE:1;
			uint32_t bReserved9:1;
			uint32_t bMOVDIRI:1;
			uint32_t bMOVDIR64B:1;
			uint32_t bENQCMD:1;
			uint32_t bSGX_LC:1;
			uint32_t bPKS:1;

			uint32_t bReserved10:1;
			uint32_t bReserved11:1;
			uint32_t bAVX512_4VNNIW:1;
			uint32_t bAVX512_4FMAPS:1;
			uint32_t bFSRM:1;
			uint32_t bReserved12:1;
			uint32_t bReserved13:1;
			uint32_t bReserved14:1;
			uint32_t bAVX512_VP2INTERSECT:1;
			uint32_t bSRBDS_CTRL:1;
			uint32_t bMD_CLEAR:1;
			uint32_t bReserved15:1;
			uint32_t bReserved16:1;
			uint32_t bTSX_FORCE_ABORT:1;
			uint32_t bSERIALIZE:1;
			uint32_t bHYBRID:1;
			uint32_t bTSXLDTRK:1;
			uint32_t bReserved17:1;
			uint32_t bPCONFIG:1;
			uint32_t bLBR:1;
			uint32_t bCET_IBT:1;
			uint32_t bReserved18:1;
			uint32_t bAMX_BF16:1;
			uint32_t bReserved19:1;
			uint32_t bAMX_TILE:1;
			uint32_t bAMX_INT8:1;
			uint32_t bIBRS_IBPB:1;
			uint32_t bSTIBP:1;
			uint32_t bL1D_FLUSH:1;
			uint32_t bIA32_ARCH_CAPABILITIES:1;
			uint32_t bIA32_CORE_CAPABILITIES:1;
			uint32_t bSSBD:1;

			uint32_t DontCare4:5;
			uint32_t bAVX512_BF16:1;
			uint32_t DontCare5:26;
		};
		uint32_t FeatureFlags[8];
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

void CheckCPUID (CPUInfo *cpu);
FString DumpCPUInfo (const CPUInfo *cpu);

#endif

