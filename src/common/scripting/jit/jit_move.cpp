
#include "jitintern.h"

void JitCompiler::EmitMOVE()
{
	StoreD(LoadD(B), A);
}
void JitCompiler::EmitMOVEF()
{
	StoreF(LoadF(B), A);
}

void JitCompiler::EmitMOVES()
{
	cc.CreateCall(stringAssignmentOperator, { LoadS(A), LoadS(B) });
}

void JitCompiler::EmitMOVEA()
{
	StoreA(LoadA(B), A);
}

void JitCompiler::EmitMOVEV2()
{
	StoreF(LoadF(B), A);
	StoreF(LoadF(B + 1), A + 1);
}

void JitCompiler::EmitMOVEV3()
{
	StoreF(LoadF(B), A);
	StoreF(LoadF(B + 1), A + 1);
	StoreF(LoadF(B + 2), A + 2);
}

void JitCompiler::EmitCAST()
{
	switch (C)
	{
	case CAST_I2F:
		StoreF(cc.CreateSIToFP(LoadD(B), doubleTy), A);
		break;
	case CAST_U2F:
		StoreF(cc.CreateUIToFP(LoadD(B), doubleTy), A);
		break;
	case CAST_F2I:
		StoreD(cc.CreateFPToSI(LoadF(B), int32Ty), A);
		break;
	case CAST_F2U:
		StoreD(cc.CreateFPToUI(LoadF(B), int32Ty), A);
		break;
	case CAST_I2S:
		cc.CreateCall(castI2S, { LoadS(A), LoadD(B) });
		break;
	case CAST_U2S:
		cc.CreateCall(castU2S, { LoadS(A), LoadD(B) });
		break;
	case CAST_F2S:
		cc.CreateCall(castF2S, { LoadS(A), LoadF(B) });
		break;
	case CAST_V22S:
		cc.CreateCall(castV22S, { LoadS(A), LoadF(B), LoadF(B + 1) });
		break;
	case CAST_V32S:
		cc.CreateCall(castV32S, { LoadS(A), LoadF(B), LoadF(B + 1), LoadF(B + 2) });
		break;
	case CAST_P2S:
		cc.CreateCall(castP2S, { LoadS(A), LoadA(B) });
		break;
	case CAST_S2I:
		StoreD(cc.CreateCall(castS2I, { LoadS(B) }), A);
		break;
	case CAST_S2F:
		StoreF(cc.CreateCall(castS2F, { LoadS(B) }), A);
		break;
	case CAST_S2N:
		StoreD(cc.CreateCall(castS2N, { LoadS(B) }), A);
		break;
	case CAST_N2S:
		cc.CreateCall(castN2S, { LoadS(A), LoadD(B) });
		break;
	case CAST_S2Co:
		StoreD(cc.CreateCall(castS2Co, { LoadS(B) }), A);
		break;
	case CAST_Co2S:
		cc.CreateCall(castCo2S, { LoadS(A), LoadD(B) });
		break;
	case CAST_S2So:
		StoreD(cc.CreateCall(castS2So, { LoadS(B) }), A);
		break;
	case CAST_So2S:
		cc.CreateCall(castSo2S, { LoadS(A), LoadD(B) });
		break;
	case CAST_SID2S:
		cc.CreateCall(castSID2S, { LoadS(A), LoadD(B) });
		break;
	case CAST_TID2S:
		cc.CreateCall(castTID2S, { LoadS(A), LoadD(B) });
		break;
	default:
		I_Error("Unknown OP_CAST type\n");
	}
}

void JitCompiler::EmitCASTB()
{
	if (C == CASTB_I)
	{
		StoreD(cc.CreateZExt(cc.CreateICmpNE(LoadD(B), ConstValueD(0)), int32Ty), A);
	}
	else if (C == CASTB_F)
	{
		StoreD(cc.CreateZExt(cc.CreateFCmpUNE(LoadF(B), ConstValueF(0.0)), int32Ty), A);
	}
	else if (C == CASTB_A)
	{
		StoreD(cc.CreateZExt(cc.CreateICmpNE(LoadA(B), ConstValueA(0)), int32Ty), A);
	}
	else
	{
		StoreD(cc.CreateCall(castB_S, { LoadS(B) }), A);
	}
}

void JitCompiler::EmitDYNCAST_R()
{
	StoreA(cc.CreateCall(dynCast, { LoadA(B), LoadA(C) }), A);
}

void JitCompiler::EmitDYNCAST_K()
{
	StoreA(cc.CreateCall(dynCast, { LoadA(B), ConstA(C) }), A);
}

void JitCompiler::EmitDYNCASTC_R()
{
	StoreA(cc.CreateCall(dynCastC, { LoadA(B), LoadA(C) }), A);
}

void JitCompiler::EmitDYNCASTC_K()
{
	StoreA(cc.CreateCall(dynCastC, { LoadA(B), ConstA(C) }), A);
}
