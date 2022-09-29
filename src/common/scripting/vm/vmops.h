#ifndef xx
#define xx(op, name, mode, alt, kreg, ktype) OP_##op,
#endif

// first row is the opcode
// second row is the disassembly name
// third row is the disassembly flags
// fourth row is the alternative opcode if all 256 constant registers are exhausted.
// fifth row is the constant register index in the opcode
// sixth row is the constant register type.
// OP_PARAM and OP_CMPS need special treatment because they encode this information in the instruction.

xx(NOP,		nop,	NOP,		NOP,	0, 0)		// no operation

// Load constants.
xx(LI,		li,		LI,			NOP,	0, 0)		// load immediate signed 16-bit constant
xx(LK,		lk,		LKI,		NOP,	0, 0)		// load integer constant
xx(LKF,		lk,		LKF,		NOP,	0, 0)		// load float constant
xx(LKS,		lk,		LKS,		NOP,	0, 0)		// load string constant
xx(LKP,		lk,		LKP,		NOP,	0, 0)		// load pointer constant
xx(LK_R,	lk,		RIRII8,		NOP,	0, 0)		// load integer constant indexed
xx(LKF_R,	lk,		RFRII8,		NOP,	0, 0)		// load float constant indexed
xx(LKS_R,	lk,		RSRII8,		NOP,	0, 0)		// load string constant indexed
xx(LKP_R,	lk,		RPRII8,		NOP,	0, 0)		// load pointer constant indexed
xx(LFP,		lf,		LFP,		NOP,	0, 0)		// load frame pointer
xx(META,	meta,	RPRP,		NOP,	0, 0)		// load a class's meta data address
xx(CLSS,	clss,	RPRP,		NOP,	0, 0)		// load a class's descriptor address

// Load from memory. rA = *(rB + rkC)
xx(LB,		lb,		RIRPKI,		LB_R,	4, REGT_INT)	// load byte
xx(LB_R,	lb,		RIRPRI,		NOP,	0, 0)
xx(LH,		lh,		RIRPKI,		LH_R,	4, REGT_INT)	// load halfword
xx(LH_R,	lh,		RIRPRI,		NOP,	0, 0)
xx(LW,		lw,		RIRPKI,		LW_R,	4, REGT_INT)	// load word
xx(LW_R,	lw,		RIRPRI,		NOP,	0, 0)
xx(LBU,		lbu,	RIRPKI,		LBU_R,	4, REGT_INT)	// load byte unsigned
xx(LBU_R,	lbu,	RIRPRI,		NOP,	0, 0)
xx(LHU,		lhu,	RIRPKI,		LHU_R,	4, REGT_INT)	// load halfword unsigned
xx(LHU_R,	lhu,	RIRPRI,		NOP,	0, 0)
xx(LSP,		lsp,	RFRPKI,		LSP_R,	4, REGT_INT)	// load single-precision fp
xx(LSP_R,	lsp,	RFRPRI,		NOP,	0, 0)
xx(LDP,		ldp,	RFRPKI,		LDP_R,	4, REGT_INT)	// load double-precision fp
xx(LDP_R,	ldp,	RFRPRI,		NOP,	0, 0)
xx(LS,		ls,		RSRPKI,		LS_R,	4, REGT_INT)	// load string
xx(LS_R,	ls,		RSRPRI,		NOP,	0, 0)
xx(LO,		lo,		RPRPKI,		LO_R,	4, REGT_INT)	// load object
xx(LO_R,	lo,		RPRPRI,		NOP,	0, 0)
xx(LP,		lp,		RPRPKI,		LP_R,	4, REGT_INT)	// load pointer
xx(LP_R,	lp,		RPRPRI,		NOP,	0, 0)
xx(LV2,		lv2,	RVRPKI,		LV2_R,	4, REGT_INT)	// load vector2
xx(LV2_R,	lv2,	RVRPRI,		NOP,	0, 0)
xx(LV3,		lv3,	RVRPKI,		LV3_R,	4, REGT_INT)	// load vector3
xx(LV3_R,	lv3,	RVRPRI,		NOP,	0, 0)
xx(LCS,		lcs,	RSRPKI,		LCS_R,	4, REGT_INT)	// load string from char ptr.
xx(LCS_R,	lcs,	RSRPRI,		NOP,	0, 0)
xx(LFV2,	lfv2,	RVRPKI,		LFV2_R,	4, REGT_INT)	// load fvector2
xx(LFV2_R,	lfv2,	RVRPRI,		NOP,	0, 0)
xx(LFV3,	lfv3,	RVRPKI,		LFV3_R,	4, REGT_INT)	// load fvector3
xx(LFV3_R,	lfv3,	RVRPRI,		NOP,	0, 0)

xx(LBIT,	lbit,	RIRPI8,		NOP,	0, 0)	// rA = !!(*rB & C)  -- *rB is a byte

// Store instructions. *(rA + rkC) = rB
xx(SB,		sb,		RPRIKI,		SB_R,	4, REGT_INT)		// store byte
xx(SB_R,	sb,		RPRIRI,		NOP,	0, 0)
xx(SH,		sh,		RPRIKI,		SH_R,	4, REGT_INT)		// store halfword
xx(SH_R,	sh,		RPRIRI,		NOP,	0, 0)
xx(SW,		sw,		RPRIKI,		SW_R,	4, REGT_INT)		// store word
xx(SW_R,	sw,		RPRIRI,		NOP,	0, 0)
xx(SSP,		ssp,	RPRFKI,		SSP_R,	4, REGT_INT)		// store single-precision fp
xx(SSP_R,	ssp,	RPRFRI,		NOP,	0, 0)
xx(SDP,		sdp,	RPRFKI,		SDP_R,	4, REGT_INT)		// store double-precision fp
xx(SDP_R,	sdp,	RPRFRI,		NOP,	0, 0)
xx(SS,		ss,		RPRSKI,		SS_R,	4, REGT_INT)		// store string
xx(SS_R,	ss,		RPRSRI,		NOP,	0, 0)
xx(SP,		sp,		RPRPKI,		SP_R,	4, REGT_INT)		// store pointer
xx(SP_R,	sp,		RPRPRI,		NOP,	0, 0)
xx(SO,		so,		RPRPKI,		SO_R,	4, REGT_INT)		// store object pointer with write barrier (only needed for non thinkers and non types)
xx(SO_R,	so,		RPRPRI,		NOP,	0, 0)
xx(SV2,		sv2,	RPRVKI,		SV2_R,	4, REGT_INT)		// store vector2
xx(SV2_R,	sv2,	RPRVRI,		NOP,	0, 0)
xx(SV3,		sv3,	RPRVKI,		SV3_R,	4, REGT_INT)		// store vector3
xx(SV3_R,	sv3,	RPRVRI,		NOP,	0, 0)
xx(SFV2,	sfv2,	RPRVKI,		SFV2_R,	4, REGT_INT)		// store fvector2
xx(SFV2_R,	sfv2,	RPRVRI,		NOP,	0, 0)
xx(SFV3,	sfv3,	RPRVKI,		SFV3_R,	4, REGT_INT)		// store fvector3
xx(SFV3_R,	sfv3,	RPRVRI,		NOP,	0, 0)

xx(SBIT,	sbit,	RPRII8,		NOP,	0, 0)		// *rA |= C if rB is true, *rA &= ~C otherwise

// Move instructions.
xx(MOVE,	mov,	RIRI,		NOP,	0, 0)		// dA = dB
xx(MOVEF,	mov,	RFRF,		NOP,	0, 0)		// fA = fB
xx(MOVES,	mov,	RSRS,		NOP,	0, 0)		// sA = sB
xx(MOVEA,	mov,	RPRP,		NOP,	0, 0)		// aA = aB
xx(MOVEV2,	mov2,	RFRF,		NOP,	0, 0)		// fA = fB (2 elements)
xx(MOVEV3,	mov3,	RFRF,		NOP,	0, 0)		// fA = fB (3 elements)
xx(CAST,	cast,	CAST,		NOP,	0, 0)		// xA = xB, conversion specified by C
xx(CASTB,	castb,	CAST,		NOP,	0, 0)		// xA = !!xB, type specified by C
xx(DYNCAST_R,	dyncast, RPRPRP,	NOP,	0, 0)		// aA = dyn_cast<aC>(aB);
xx(DYNCAST_K,	dyncast, RPRPKP,	NOP,	0, 0)		// aA = dyn_cast<aKC>(aB);
xx(DYNCASTC_R,	dyncastc, RPRPRP,	NOP,	0, 0)		// aA = dyn_cast<aC>(aB); for class types
xx(DYNCASTC_K,	dyncastc, RPRPKP,	NOP,	0, 0)		// aA = dyn_cast<aKC>(aB);

// Control flow.
xx(TEST,	test,	RII16,		NOP,	0, 0)		// if (dA != BC) then pc++
xx(TESTN,	testn,	RII16,		NOP,	0, 0)		// if (dA != -BC) then pc++
xx(JMP,		jmp,	I24,		NOP,	0, 0)		// pc += ABC		-- The ABC fields contain a signed 24-bit offset.
xx(IJMP,	ijmp,	RII16,		NOP,	0, 0)		// pc += dA + BC	-- BC is a signed offset. The target instruction must be a JMP.
xx(PARAM,	param,	__BCP,		NOP,	0, 0)		// push parameter encoded in BC for function call (B=regtype, C=regnum)
xx(PARAMI,	parami,	I24,		NOP,	0, 0)		// push immediate, signed integer for function call
xx(CALL,	call,	RPI8I8,		NOP,	0, 0)	// Call function pkA with parameter count B and expected result count C
xx(CALL_K,	call,	KPI8I8,		CALL,	1, REGT_POINTER)
xx(VTBL,	vtbl,	RPRPI8,		NOP,	0, 0)	// dereferences a virtual method table.
xx(SCOPE,	scope,	RPI8,		NOP,	0, 0)		// Scope check at runtime.
xx(RESULT,	result,	__BCP,		NOP,	0, 0)		// Result should go in register encoded in BC (in caller, after CALL)
xx(RET,		ret,	I8BCP,		NOP,	0, 0)		// Copy value from register encoded in BC to return value A, possibly returning
xx(RETI,	reti,	I8I16,		NOP,	0, 0)		// Copy immediate from BC to return value A, possibly returning
//xx(TRY,		try,	I24,		NOP,	0, 0)		// When an exception is thrown, start searching for a handler at pc + ABC
//xx(UNTRY,	untry,	I8,			NOP,	0, 0)		// Pop A entries off the exception stack
xx(THROW,	throw,	THROW,		NOP,	0, 0)		// A == 0: Throw exception object pB
												// A == 1: Throw exception object pkB
												// A >= 2: Throw VM exception of type BC
//xx(CATCH,	catch,	CATCH,		NOP,	0, 0)		// A == 0: continue search on next try
												// A == 1: continue execution at instruction immediately following CATCH (catches any exception)
												// A == 2: (pB == <type of exception thrown>) then pc++ ; next instruction must JMP to another CATCH
												// A == 3: (pkB == <type of exception thrown>) then pc++ ; next instruction must JMP to another CATCH
												// for A > 0, exception is stored in pC
xx(BOUND,	bound,	RII16,		NOP,	0, 0)		// if rA < 0 or rA >= BC, throw exception
xx(BOUND_K,	bound,	LKI,		NOP,	0, 0)		// if rA < 0 or rA >= const[BC], throw exception
xx(BOUND_R,	bound,	RIRI,		NOP,	0, 0)		// if rA < 0 or rA >= rB, throw exception

// String instructions.
xx(CONCAT,		concat,	RSRSRS,		NOP,	0, 0)		// sA = sB..sC
xx(LENS,		lens,	RIRS,		NOP,	0, 0)			// dA = sB.Length
xx(CMPS,		cmps,	I8RXRX,		NOP,	0, 0)		// if ((skB op skC) != (A & 1)) then pc++

// Integer math.
xx(SLL_RR,		sll,	RIRIRI,		NOP,	0, 0)		// dA = dkB << diC
xx(SLL_RI,		sll,	RIRII8,		NOP,	0, 0)
xx(SLL_KR,		sll,	RIKIRI,		SLL_RR,	2, REGT_INT)
xx(SRL_RR,		srl,	RIRIRI,		NOP,	0, 0)		// dA = dkB >> diC  -- unsigned
xx(SRL_RI,		srl,	RIRII8,		NOP,	0, 0)
xx(SRL_KR,		srl,	RIKIRI,		SRL_RR,	2, REGT_INT)
xx(SRA_RR,		sra,	RIRIRI,		NOP,	0, 0)		// dA = dkB >> diC  -- signed
xx(SRA_RI,		sra,	RIRII8,		NOP,	0, 0)
xx(SRA_KR,		sra,	RIKIRI,		SRA_RR,	2, REGT_INT)
xx(ADD_RR,		add,	RIRIRI,		NOP,	0, 0)		// dA = dB + dkC
xx(ADD_RK,		add,	RIRIKI,		ADD_RR,	4, REGT_INT)
xx(ADDI,		addi,	RIRIIs,		NOP,	0, 0)		// dA = dB + C		-- C is a signed 8-bit constant
xx(SUB_RR,		sub,	RIRIRI,		NOP,	0, 0)		// dA = dkB - dkC
xx(SUB_RK,		sub,	RIRIKI,		SUB_RR,	4, REGT_INT)
xx(SUB_KR,		sub,	RIKIRI,		SUB_RR,	2, REGT_INT)
xx(MUL_RR,		mul,	RIRIRI,		NOP,	0, 0)		// dA = dB * dkC
xx(MUL_RK,		mul,	RIRIKI,		MUL_RR,	4, REGT_INT)
xx(DIV_RR,		div,	RIRIRI,		NOP,	0, 0)		// dA = dkB / dkC (signed)
xx(DIV_RK,		div,	RIRIKI,		DIV_RR,	4, REGT_INT)
xx(DIV_KR,		div,	RIKIRI,		DIV_RR,	2, REGT_INT)
xx(DIVU_RR,		divu,	RIRIRI,		NOP,	0, 0)		// dA = dkB / dkC (unsigned)
xx(DIVU_RK,		divu,	RIRIKI,		DIVU_RR,4, REGT_INT)
xx(DIVU_KR,		divu,	RIKIRI,		DIVU_RR,2, REGT_INT)
xx(MOD_RR,		mod,	RIRIRI,		NOP,	0, 0)		// dA = dkB % dkC (signed)
xx(MOD_RK,		mod,	RIRIKI,		MOD_RR,	4, REGT_INT)
xx(MOD_KR,		mod,	RIKIRI,		MOD_RR,	2, REGT_INT)
xx(MODU_RR,		modu,	RIRIRI,		NOP,	0, 0)		// dA = dkB % dkC (unsigned)
xx(MODU_RK,		modu,	RIRIKI,		MODU_RR,4, REGT_INT)
xx(MODU_KR,		modu,	RIKIRI,		MODU_RR,2, REGT_INT)
xx(AND_RR,		and,	RIRIRI,		NOP,	0, 0)		// dA = dB & dkC
xx(AND_RK,		and,	RIRIKI,		AND_RR,	4, REGT_INT)
xx(OR_RR,		or,		RIRIRI,		NOP,	0, 0)		// dA = dB | dkC
xx(OR_RK,		or,		RIRIKI,		OR_RR,	4, REGT_INT)
xx(XOR_RR,		xor,	RIRIRI,		NOP,	0, 0)		// dA = dB ^ dkC
xx(XOR_RK,		xor,	RIRIKI,		XOR_RR,	4, REGT_INT)
xx(MIN_RR,		min,	RIRIRI,		NOP,	0, 0)		// dA = min(dB,dkC)
xx(MIN_RK,		min,	RIRIKI,		MIN_RR,	4, REGT_INT)
xx(MAX_RR,		max,	RIRIRI,		NOP,	0, 0)		// dA = max(dB,dkC)
xx(MAX_RK,		max,	RIRIKI,		MAX_RR,	4, REGT_INT)
xx(MINU_RR,		minu,	RIRIRI,		NOP,	0, 0)		// dA = min(dB,dkC) unsigned
xx(MINU_RK,		minu,	RIRIKI,		MIN_RR,	4, REGT_INT)
xx(MAXU_RR,		maxu,	RIRIRI,		NOP,	0, 0)		// dA = max(dB,dkC) unsigned
xx(MAXU_RK,		maxu,	RIRIKI,		MAX_RR,	4, REGT_INT)
xx(ABS,			abs,	RIRI,		NOP,	0, 0)			// dA = abs(dB)
xx(NEG,			neg,	RIRI,		NOP,	0, 0)			// dA = -dB
xx(NOT,			not,	RIRI,		NOP,	0, 0)			// dA = ~dB
xx(EQ_R,		beq,	CIRR,		NOP,	0, 0)			// if ((dB == dkC) != A) then pc++
xx(EQ_K,		beq,	CIRK,		EQ_R,	4, REGT_INT)
xx(LT_RR,		blt,	CIRR,		NOP,	0, 0)			// if ((dkB < dkC) != A) then pc++
xx(LT_RK,		blt,	CIRK,		LT_RR,	4, REGT_INT)
xx(LT_KR,		blt,	CIKR,		LT_RR,	2, REGT_INT)
xx(LE_RR,		ble,	CIRR,		NOP,	0, 0)			// if ((dkB <= dkC) != A) then pc++
xx(LE_RK,		ble,	CIRK,		LE_RR,	4, REGT_INT)
xx(LE_KR,		ble,	CIKR,		LE_RR,	2, REGT_INT)
xx(LTU_RR,		bltu,	CIRR,		NOP,	0, 0)			// if ((dkB < dkC) != A) then pc++		-- unsigned
xx(LTU_RK,		bltu,	CIRK,		LTU_RR,	4, REGT_INT)
xx(LTU_KR,		bltu,	CIKR,		LTU_RR,	2, REGT_INT)
xx(LEU_RR,		bleu,	CIRR,		NOP,	0, 0)			// if ((dkB <= dkC) != A) then pc++		-- unsigned
xx(LEU_RK,		bleu,	CIRK,		LEU_RR,	4, REGT_INT)
xx(LEU_KR,		bleu,	CIKR,		LEU_RR,	2, REGT_INT)

// Double-precision floating point math.
xx(ADDF_RR,		add,	RFRFRF,		NOP,	0, 0)		// fA = fB + fkC
xx(ADDF_RK,		add,	RFRFKF,		ADDF_RR,4, REGT_FLOAT)
xx(SUBF_RR,		sub,	RFRFRF,		NOP,	0, 0)		// fA = fkB - fkC
xx(SUBF_RK,		sub,	RFRFKF,		SUBF_RR,4, REGT_FLOAT)
xx(SUBF_KR,		sub,	RFKFRF,		SUBF_RR,2, REGT_FLOAT)
xx(MULF_RR,		mul,	RFRFRF,		NOP,	0, 0)		// fA = fB * fkC
xx(MULF_RK,		mul,	RFRFKF,		MULF_RR,4, REGT_FLOAT)
xx(DIVF_RR,		div,	RFRFRF,		NOP,	0, 0)		// fA = fkB / fkC
xx(DIVF_RK,		div,	RFRFKF,		DIVF_RR,4, REGT_FLOAT)
xx(DIVF_KR,		div,	RFKFRF,		DIVF_RR,2, REGT_FLOAT)
xx(MODF_RR,		mod,	RFRFRF,		NOP,	0, 0)		// fA = fkB % fkC
xx(MODF_RK,		mod,	RFRFKF,		MODF_RR,4, REGT_FLOAT)
xx(MODF_KR,		mod,	RFKFRF,		MODF_RR,4, REGT_FLOAT)
xx(POWF_RR,		pow,	RFRFRF,		NOP,	0, 0)		// fA = fkB ** fkC
xx(POWF_RK,		pow,	RFRFKF,		POWF_RR,4, REGT_FLOAT)
xx(POWF_KR,		pow,	RFKFRF,		POWF_RR,2, REGT_FLOAT)
xx(MINF_RR,		min,	RFRFRF,		NOP,	0, 0)		// fA = min(fB)fkC)
xx(MINF_RK,		min,	RFRFKF,		MINF_RR,4, REGT_FLOAT)
xx(MAXF_RR,		max,	RFRFRF,		NOP,	0, 0)		// fA = max(fB)fkC)
xx(MAXF_RK,		max,	RFRFKF,		MAXF_RR,4, REGT_FLOAT)
xx(ATAN2,		atan2,	RFRFRF,		NOP,	0, 0)		// fA = atan2(fB,fC) result is in degrees
xx(FLOP,		flop,	RFRFI8,		NOP,	0, 0)		// fA = f(fB) where function is selected by C
xx(EQF_R,		beq,	CFRR,		NOP,	0, 0)			// if ((fB == fkC) != (A & 1)) then pc++
xx(EQF_K,		beq,	CFRK,		EQF_R,	4, REGT_FLOAT)
xx(LTF_RR,		blt,	CFRR,		NOP,	0, 0)			// if ((fkB < fkC) != (A & 1)) then pc++
xx(LTF_RK,		blt,	CFRK,		LTF_RR,	4, REGT_FLOAT)
xx(LTF_KR,		blt,	CFKR,		LTF_RR,	2, REGT_FLOAT)
xx(LEF_RR,		ble,	CFRR,		NOP,	0, 0)			// if ((fkb <= fkC) != (A & 1)) then pc++
xx(LEF_RK,		ble,	CFRK,		LEF_RR,	4, REGT_FLOAT)
xx(LEF_KR,		ble,	CFKR,		LEF_RR,	2, REGT_FLOAT)

// Vector math. (2D)
xx(NEGV2,		negv2,	RVRV,		NOP,	0, 0)			// vA = -vB
xx(ADDV2_RR,	addv2,	RVRVRV,		NOP,	0, 0)		// vA = vB + vkC
xx(SUBV2_RR,	subv2,	RVRVRV,		NOP,	0, 0)		// vA = vkB - vkC
xx(DOTV2_RR,	dotv2,	RVRVRV,		NOP,	0, 0)		// va = vB dot vkC
xx(MULVF2_RR,	mulv2,	RVRVRF,		NOP,	0, 0)		// vA = vkB * fkC
xx(MULVF2_RK,	mulv2,	RVRVKF,		MULVF2_RR,4, REGT_FLOAT)
xx(DIVVF2_RR,	divv2,	RVRVRF,		NOP,	0, 0)		// vA = vkB / fkC
xx(DIVVF2_RK,	divv2,	RVRVKF,		DIVVF2_RR,4, REGT_FLOAT)
xx(LENV2,		lenv2,	RFRV,		NOP,	0, 0)			// fA = vB.Length
xx(EQV2_R,		beqv2,	CVRR,		NOP,	0, 0)			// if ((vB == vkC) != A) then pc++ (inexact if A & 32)
xx(EQV2_K,		beqv2,	CVRK,		NOP,	0, 0)			// this will never be used.

// Vector math (3D)
xx(NEGV3,		negv3,	RVRV,		NOP,	0, 0)			// vA = -vB
xx(ADDV3_RR,	addv3,	RVRVRV,		NOP,	0, 0)		// vA = vB + vkC
xx(SUBV3_RR,	subv3,	RVRVRV,		NOP,	0, 0)		// vA = vkB - vkC
xx(DOTV3_RR,	dotv3,	RVRVRV,		NOP,	0, 0)		// va = vB dot vkC
xx(CROSSV_RR,	crossv,	RVRVRV,		NOP,	0, 0)		// vA = vkB cross vkC
xx(MULVF3_RR,	mulv3,	RVRVRF,		NOP,	0, 0)		// vA = vkB * fkC
xx(MULVF3_RK,	mulv3,	RVRVKF,		MULVF3_RR,4, REGT_FLOAT)
xx(DIVVF3_RR,	divv3,	RVRVRF,		NOP,	0, 0)		// vA = vkB / fkC
xx(DIVVF3_RK,	divv3,	RVRVKF,		DIVVF3_RR,4, REGT_FLOAT)
xx(LENV3,		lenv3,	RFRV,		NOP,	0, 0)			// fA = vB.Length
xx(EQV3_R,		beqv3,	CVRR,		NOP,	0, 0)			// if ((vB == vkC) != A) then pc++ (inexact if A & 33)
xx(EQV3_K,		beqv3,	CVRK,		NOP,	0, 0)			// this will never be used.

// Pointer math.
xx(ADDA_RR,		add,	RPRPRI,		NOP,	0, 0)		// pA = pB + dkC
xx(ADDA_RK,		add,	RPRPKI,		ADDA_RR,4, REGT_INT)
xx(SUBA,		sub,	RIRPRP,		NOP,	0, 0)		// dA = pB - pC
xx(EQA_R,		beq,	CPRR,		NOP,	0, 0)			// if ((pB == pkC) != A) then pc++
xx(EQA_K,		beq,	CPRK,		EQA_R,	4, REGT_POINTER)

#undef xx
