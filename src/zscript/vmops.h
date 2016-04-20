#ifndef xx
#define xx(op, name, mode) OP_##op
#endif

xx(NOP,		nop,	NOP),		// no operation

// Load constants.
xx(LI,		li,		LI),		// load immediate signed 16-bit constant
xx(LK,		lk,		LKI),		// load integer constant
xx(LKF,		lk,		LKF),		// load float constant
xx(LKS,		lk,		LKS),		// load string constant
xx(LKP,		lk,		LKP),		// load pointer constant
xx(LFP,		lf,		LFP),		// load frame pointer

// Load from memory. rA = *(rB + rkC)
xx(LB,		lb,		RIRPKI),	// load byte
xx(LB_R,	lb,		RIRPRI),
xx(LH,		lh,		RIRPKI),	// load halfword
xx(LH_R,	lh,		RIRPRI),
xx(LW,		lw,		RIRPKI),	// load word
xx(LW_R,	lw,		RIRPRI),
xx(LBU,		lbu,	RIRPKI),	// load byte unsigned
xx(LBU_R,	lbu,	RIRPRI),
xx(LHU,		lhu,	RIRPKI),	// load halfword unsigned
xx(LHU_R,	lhu,	RIRPRI),
xx(LSP,		lsp,	RFRPKI),	// load single-precision fp
xx(LSP_R,	lsp,	RFRPRI),
xx(LDP,		ldp,	RFRPKI),	// load double-precision fp
xx(LDP_R,	ldp,	RFRPRI),
xx(LS,		ls,		RSRPKI),	// load string
xx(LS_R,	ls,		RSRPRI),
xx(LO,		lo,		RPRPKI),	// load object
xx(LO_R,	lo,		RPRPRI),
xx(LP,		lp,		RPRPKI),	// load pointer
xx(LP_R,	lp,		RPRPRI),
xx(LV,		lv,		RVRPKI),	// load vector
xx(LV_R,	lv,		RVRPRI),

xx(LBIT,	lbit,	RIRPI8),	// rA = !!(*rB & C)  -- *rB is a byte

// Store instructions. *(rA + rkC) = rB
xx(SB,		sb,		RPRIKI),		// store byte
xx(SB_R,	sb,		RPRIRI),
xx(SH,		sh,		RPRIKI),		// store halfword
xx(SH_R,	sh,		RPRIRI),
xx(SW,		sw,		RPRIKI),		// store word
xx(SW_R,	sw,		RPRIRI),
xx(SSP,		ssp,	RPRFKI),		// store single-precision fp
xx(SSP_R,	ssp,	RPRFRI),
xx(SDP,		sdp,	RPRFKI),		// store double-precision fp
xx(SDP_R,	sdp,	RPRFRI),
xx(SS,		ss,		RPRSKI),		// store string
xx(SS_R,	ss,		RPRSRI),
xx(SP,		sp,		RPRPKI),		// store pointer
xx(SP_R,	sp,		RPRPRI),
xx(SV,		sv,		RPRVKI),		// store vector
xx(SV_R,	sv,		RPRVRI),

xx(SBIT,	sbit,	RPRII8),		// *rA |= C if rB is true, *rA &= ~C otherwise

// Move instructions.
xx(MOVE,		mov,	RIRI),		// dA = dB
xx(MOVEF,		mov,	RFRF),		// fA = fB
xx(MOVES,		mov,	RSRS),		// sA = sB
xx(MOVEA,		mov,	RPRP),		// aA = aB
xx(CAST,		cast,	CAST),		// xA = xB, conversion specified by C
xx(DYNCAST_R,	dyncast,RPRPRP),	// aA = aB after casting to rkC (specifying a class)
xx(DYNCAST_K,	dyncast,RPRPKP),

// Control flow.
xx(TEST,	test,	RII16),		// if (dA != BC) then pc++
xx(JMP,		jmp,	I24),		// pc += ABC		-- The ABC fields contain a signed 24-bit offset.
xx(IJMP,	ijmp,	RII16),		// pc += dA + BC	-- BC is a signed offset. The target instruction must be a JMP.
xx(PARAM,	param,	__BCP),		// push parameter encoded in BC for function call (B=regtype, C=regnum)
xx(PARAMI,	parami,	I24),		// push immediate, signed integer for function call
xx(CALL,	call,	RPI8I8),	// Call function pkA with parameter count B and expected result count C
xx(CALL_K,	call,	KPI8I8),
xx(TAIL,	tail,	RPI8),		// Call+Ret in a single instruction
xx(TAIL_K,	tail,	KPI8),
xx(RESULT,	result,	__BCP),		// Result should go in register encoded in BC (in caller, after CALL)
xx(RET,		ret,	I8BCP),		// Copy value from register encoded in BC to return value A, possibly returning
xx(RETI,	reti,	I8I16),		// Copy immediate from BC to return value A, possibly returning
xx(TRY,		try,	I24),		// When an exception is thrown, start searching for a handler at pc + ABC
xx(UNTRY,	untry,	I8),		// Pop A entries off the exception stack
xx(THROW,	throw,	THROW),		// A == 0: Throw exception object pB
								// A != 0: Throw exception object pkB
xx(CATCH,	catch,	CATCH),		// A == 0: continue search on next try
								// A == 1: continue execution at instruction immediately following CATCH (catches any exception)
								// A == 2: (pB == <type of exception thrown>) then pc++ ; next instruction must JMP to another CATCH
								// A == 3: (pkB == <type of exception thrown>) then pc++ ; next instruction must JMP to another CATCH
								// for A > 0, exception is stored in pC
xx(BOUND,	bound,	RII16),		// if rA >= BC, throw exception

// String instructions.
xx(CONCAT,		concat,	RSRSRS),		// sA = sB.. ... ..sC
xx(LENS,		lens,	RIRS),			// dA = sB.Length
xx(CMPS,		cmps,	I8RXRX),		// if ((skB op skC) != (A & 1)) then pc++

// Integer math.
xx(SLL_RR,		sll,	RIRIRI),		// dA = dkB << diC
xx(SLL_RI,		sll,	RIRII8),
xx(SLL_KR,		sll,	RIKIRI),
xx(SRL_RR,		srl,	RIRIRI),		// dA = dkB >> diC  -- unsigned
xx(SRL_RI,		srl,	RIRII8),
xx(SRL_KR,		srl,	RIKIRI),
xx(SRA_RR,		sra,	RIRIRI),		// dA = dkB >> diC  -- signed
xx(SRA_RI,		sra,	RIRII8),
xx(SRA_KR,		sra,	RIKIRI),
xx(ADD_RR,		add,	RIRIRI),		// dA = dB + dkC
xx(ADD_RK,		add,	RIRIKI),
xx(ADDI,		addi,	RIRIIs),		// dA = dB + C		-- C is a signed 8-bit constant
xx(SUB_RR,		sub,	RIRIRI),		// dA = dkB - dkC
xx(SUB_RK,		sub,	RIRIKI),
xx(SUB_KR,		sub,	RIKIRI),
xx(MUL_RR,		mul,	RIRIRI),		// dA = dB * dkC
xx(MUL_RK,		mul,	RIRIKI),
xx(DIV_RR,		div,	RIRIRI),		// dA = dkB / dkC
xx(DIV_RK,		div,	RIRIKI),
xx(DIV_KR,		div,	RIKIRI),
xx(MOD_RR,		mod,	RIRIRI),		// dA = dkB % dkC
xx(MOD_RK,		mod,	RIRIKI),
xx(MOD_KR,		mod,	RIKIRI),
xx(AND_RR,		and,	RIRIRI),		// dA = dB & dkC
xx(AND_RK,		and,	RIRIKI),
xx(OR_RR,		or,		RIRIRI),		// dA = dB | dkC
xx(OR_RK,		or,		RIRIKI),
xx(XOR_RR,		xor,	RIRIRI),		// dA = dB ^ dkC
xx(XOR_RK,		xor,	RIRIKI),
xx(MIN_RR,		min,	RIRIRI),		// dA = min(dB,dkC)
xx(MIN_RK,		min,	RIRIKI),
xx(MAX_RR,		max,	RIRIRI),		// dA = max(dB,dkC)
xx(MAX_RK,		max,	RIRIKI),
xx(ABS,			abs,	RIRI),			// dA = abs(dB)
xx(NEG,			neg,	RIRI),			// dA = -dB
xx(NOT,			not,	RIRI),			// dA = ~dB
xx(SEXT,		sext,	RIRII8),		// dA = dB, sign extended by shifting left then right by C
xx(ZAP_R,		zap,	RIRIRI),		// dA = dB, with bytes zeroed where bits in C/dC are one
xx(ZAP_I,		zap,	RIRII8),
xx(ZAPNOT_R,	zapnot,	RIRIRI),		// dA = dB, with bytes zeroed where bits in C/dC are zero
xx(ZAPNOT_I,	zapnot,	RIRII8),
xx(EQ_R,		beq,	CIRR),			// if ((dB == dkC) != A) then pc++
xx(EQ_K,		beq,	CIRK),
xx(LT_RR,		blt,	CIRR),			// if ((dkB < dkC) != A) then pc++
xx(LT_RK,		blt,	CIRK),
xx(LT_KR,		blt,	CIKR),
xx(LE_RR,		ble,	CIRR),			// if ((dkB <= dkC) != A) then pc++
xx(LE_RK,		ble,	CIRK),
xx(LE_KR,		ble,	CIKR),
xx(LTU_RR,		bltu,	CIRR),			// if ((dkB < dkC) != A) then pc++		-- unsigned
xx(LTU_RK,		bltu,	CIRK),
xx(LTU_KR,		bltu,	CIKR),	
xx(LEU_RR,		bleu,	CIRR),			// if ((dkB <= dkC) != A) then pc++		-- unsigned
xx(LEU_RK,		bleu,	CIRK),
xx(LEU_KR,		bleu,	CIKR),

// Double-precision floating point math.
xx(ADDF_RR,		add,	RFRFRF),		// fA = fB + fkC
xx(ADDF_RK,		add,	RFRFKF),
xx(SUBF_RR,		sub,	RFRFRF),		// fA = fkB - fkC
xx(SUBF_RK,		sub,	RFRFKF),
xx(SUBF_KR,		sub,	RFKFRF),
xx(MULF_RR,		mul,	RFRFRF),		// fA = fB * fkC
xx(MULF_RK,		mul,	RFRFKF),
xx(DIVF_RR,		div,	RFRFRF),		// fA = fkB / fkC
xx(DIVF_RK,		div,	RFRFKF),
xx(DIVF_KR,		div,	RFKFRF),
xx(MODF_RR,		mod,	RFRFRF),		// fA = fkB % fkC
xx(MODF_RK,		mod,	RFRFKF),
xx(MODF_KR,		mod,	RFKFRF),
xx(POWF_RR,		pow,	RFRFRF),		// fA = fkB ** fkC
xx(POWF_RK,		pow,	RFRFKF),
xx(POWF_KR,		pow,	RFKFRF),
xx(MINF_RR,		min,	RFRFRF),		// fA = min(fB),fkC)
xx(MINF_RK,		min,	RFRFKF),
xx(MAXF_RR,		max,	RFRFRF),		// fA = max(fB),fkC)
xx(MAXF_RK,		max,	RFRFKF),
xx(ATAN2,		atan2,	RFRFRF),		// fA = atan2(fB,fC), result is in degrees
xx(FLOP,		flop,	RFRFI8),		// fA = f(fB), where function is selected by C
xx(EQF_R,		beq,	CFRR),			// if ((fB == fkC) != (A & 1)) then pc++
xx(EQF_K,		beq,	CFRK),
xx(LTF_RR,		blt,	CFRR),			// if ((fkB < fkC) != (A & 1)) then pc++
xx(LTF_RK,		blt,	CFRK),
xx(LTF_KR,		blt,	CFKR),
xx(LEF_RR,		ble,	CFRR),			// if ((fkb <= fkC) != (A & 1)) then pc++
xx(LEF_RK,		ble,	CFRK),
xx(LEF_KR,		ble,	CFKR),

// Vector math.
xx(NEGV,		negv,	RVRV),			// vA = -vB
xx(ADDV_RR,		addv,	RVRVRV),		// vA = vB + vkC
xx(ADDV_RK,		addv,	RVRVKV),
xx(SUBV_RR,		subv,	RVRVRV),		// vA = vkB - vkC
xx(SUBV_RK,		subv,	RVRVKV),
xx(SUBV_KR,		subv,	RVKVRV),
xx(DOTV_RR,		dotv,	RVRVRV),		// va = vB dot vkC
xx(DOTV_RK,		dotv,	RVRVKV),
xx(CROSSV_RR,	crossv,	RVRVRV),		// vA = vkB cross vkC
xx(CROSSV_RK,	crossv,	RVRVKV),
xx(CROSSV_KR,	crossv,	RVKVRV),
xx(MULVF_RR,	mulv,	RVRVRV),		// vA = vkB * fkC
xx(MULVF_RK,	mulv,	RVRVKV),
xx(MULVF_KR,	mulv,	RVKVRV),
xx(LENV,		lenv,	RFRV),			// fA = vB.Length
xx(EQV_R,		beqv,	CVRR),			// if ((vB == vkC) != A) then pc++ (inexact if A & 32)
xx(EQV_K,		beqv,	CVRK),

// Pointer math.
xx(ADDA_RR,		add,	RPRPRI),		// pA = pB + dkC
xx(ADDA_RK,		add,	RPRPKI),
xx(SUBA,		sub,	RIRPRP),		// dA = pB - pC
xx(EQA_R,		beq,	CPRR),			// if ((pB == pkC) != A) then pc++
xx(EQA_K,		beq,	CPRK),

#undef xx
