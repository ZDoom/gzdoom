#include "vm.h"

#define LI		MODE_AI | MODE_BCJOINT | MODE_BCIMMS
#define LKI		MODE_AI | MODE_BCJOINT | MODE_BCKI
#define LKF		MODE_AF | MODE_BCJOINT | MODE_BCKF
#define LKS		MODE_AS | MODE_BCJOINT | MODE_BCKS
#define LKP		MODE_AP | MODE_BCJOINT | MODE_BCKP
#define LFP		MODE_AP | MODE_BUNUSED | MODE_CUNUSED

#define RIRPKI	MODE_AI | MODE_BP | MODE_CKI
#define RIRPRI	MODE_AI | MODE_BP | MODE_CI
#define RFRPKI	MODE_AF | MODE_BP | MODE_CKI
#define RFRPRI	MODE_AF | MODE_BP | MODE_CI
#define RSRPKI	MODE_AS | MODE_BP | MODE_CKI
#define RSRPRI	MODE_AS | MODE_BP | MODE_CI
#define RPRPKI	MODE_AP | MODE_BP | MODE_CKI
#define RPRPRI	MODE_AP | MODE_BP | MODE_CI
#define RVRPKI	MODE_AV | MODE_BP | MODE_CKI
#define RVRPRI	MODE_AV | MODE_BP | MODE_CI
#define RIRPI8	MODE_AI | MODE_BP | MODE_CIMMZ

#define RPRIKI	MODE_AP | MODE_BI | MODE_CKI
#define RPRIRI	MODE_AP | MODE_BI | MODE_CI
#define RPRFKI	MODE_AP | MODE_BF | MODE_CKI
#define RPRFRI	MODE_AP | MODE_BF | MODE_CI
#define RPRSKI	MODE_AP | MODE_BS | MODE_CKI
#define RPRSRI	MODE_AP | MODE_BS | MODE_CI
#define RPRPKI	MODE_AP | MODE_BP | MODE_CKI
#define RPRPRI	MODE_AP | MODE_BP | MODE_CI
#define RPRVKI	MODE_AP | MODE_BV | MODE_CKI
#define RPRVRI	MODE_AP | MODE_BV | MODE_CI
#define RPRII8	MODE_AP | MODE_BI | MODE_CIMMZ

#define RIRI	MODE_AI | MODE_BI | MODE_CUNUSED
#define RFRF	MODE_AF | MODE_BF | MODE_CUNUSED
#define	RSRS	MODE_AS | MODE_BS | MODE_CUNUSED
#define RPRP	MODE_AP | MODE_BP | MODE_CUNUSED
#define RXRXI8	MODE_AX | MODE_BX | MODE_CIMMZ
#define RPRPRP	MODE_AP | MODE_BP | MODE_CP
#define RPRPKP	MODE_AP | MODE_BP | MODE_CKP

#define RII16	MODE_AI | MODE_BCJOINT | MODE_BCIMMS
#define I24		MODE_ABCJOINT
#define I8		MODE_AIMMZ | MODE_BUNUSED | MODE_CUNUSED
#define __BCP	MODE_AUNUSED | MODE_BCJOINT | MODE_BCPARAM
#define RPI8I8	MODE_AP | MODE_BIMMZ | MODE_CIMMZ
#define KPI8I8	MODE_AKP | MODE_BIMMZ | MODE_CIMMZ
#define I8BCP	MODE_AIMMZ | MODE_BCJOINT | MODE_BCPARAM
#define THROW	MODE_AIMMZ | MODE_BCTHROW
#define CATCH	MODE_AIMMZ | MODE_BCCATCH
#define CAST	MODE_AX | MODE_BX | MODE_CIMMZ | MODE_BCCAST

#define RSRSRS	MODE_AS | MODE_BS | MODE_CS
#define RIRS	MODE_AI | MODE_BS | MODE_CUNUSED
#define I8RXRX	MODE_AIMMZ | MODE_BX | MODE_CX

#define RIRIRI	MODE_AI | MODE_BI | MODE_CI
#define RIRII8	MODE_AI | MODE_BI | MODE_CIMMZ
#define RIRIKI	MODE_AI | MODE_BI | MODE_CKI
#define RIKIRI	MODE_AI | MODE_BKI | MODE_CI
#define RIKII8	MODE_AI | MODE_BKI | MODE_CIMMZ
#define RIRIIs	MODE_AI | MODE_BI | MODE_CIMMS
#define RIRI	MODE_AI | MODE_BI | MODE_CUNUSED
#define I8RIRI	MODE_AIMMZ | MODE_BI | MODE_CI
#define I8RIKI	MODE_AIMMZ | MODE_BI | MODE_CKI
#define I8KIRI	MODE_AIMMZ | MODE_BKI | MODE_CI

#define RFRFRF	MODE_AF | MODE_BF | MODE_CF
#define RFRFKF	MODE_AF | MODE_BF | MODE_CKF
#define RFKFRF	MODE_AF | MODE_BKF | MODE_CF
#define I8RFRF	MODE_AIMMZ | MODE_BF | MODE_CF
#define I8RFKF	MODE_AIMMZ | MODE_BF | MODE_CKF
#define I8KFRF	MODE_AIMMZ | MODE_BKF | MODE_CF
#define RFRFI8	MODE_AF | MODE_BF | MODE_CIMMZ

#define RVRV	MODE_AV | MODE_BV | MODE_CUNUSED
#define RVRVRV	MODE_AV | MODE_BV | MODE_CV
#define RVRVKV	MODE_AV | MODE_BV | MODE_CKV
#define RVKVRV	MODE_AV | MODE_BKV | MODE_CV
#define RFRV	MODE_AF | MODE_BV | MODE_CUNUSED
#define I8RVRV	MODE_AIMMZ | MODE_BV | MODE_CV
#define I8RVKV	MODE_AIMMZ | MODE_BV | MODE_CKV

#define RPRPRI	MODE_AP | MODE_BP | MODE_CI
#define RPRPKI	MODE_AP | MODE_BP | MODE_CKI
#define RIRPRP	MODE_AI | MODE_BP | MODE_CP
#define I8RPRP	MODE_AIMMZ | MODE_BP | MODE_CP
#define I8RPKP	MODE_AIMMZ | MODE_BP | MODE_CKP

const VMOpInfo OpInfo[NUM_OPS] =
{
#define xx(op, name, mode)	{ #name, mode }
#include "vmops.h"
};

#ifdef WORDS_BIGENDIAN
#define JMPOFS(x)		((*(VM_SWORD *)(x) << 8) >> 6)
#else
#define JMPOFS(x)		((*(VM_SWORD *)(x) >> 6) & ~3)
#endif

static int print_reg(int col, int arg, int mode, int immshift, const VMScriptFunction *func);

void VMDumpConstants(const VMScriptFunction *func)
{
	char tmp[21];
	int i, j, k, kk;

	if (func->KonstD != NULL && func->NumKonstD != 0)
	{
		Printf("\nConstant integers:\n");
		kk = (func->NumKonstD + 3) / 4;
		for (i = 0; i < kk; ++i)
		{
			for (j = 0, k = i; j < 4 && k < func->NumKonstD; j++, k += kk)
			{
				mysnprintf(tmp, countof(tmp), "%3d. %d", k, func->KonstD[k]);
				Printf("%-20s", tmp);
			}
			Printf("\n");
		}
	}
	if (func->KonstF != NULL && func->NumKonstF != 0)
	{
		Printf("\nConstant floats:\n");
		kk = (func->NumKonstF + 3) / 4;
		for (i = 0; i < kk; ++i)
		{
			for (j = 0, k = i; j < 4 && k < func->NumKonstF; j++, k += kk)
			{
				mysnprintf(tmp, countof(tmp), "%3d. %.16f", k, func->KonstF[k]);
				Printf("%-20s", tmp);
			}
			Printf("\n");
		}
	}
	if (func->KonstA != NULL && func->NumKonstA != 0)
	{
		Printf("\nConstant addresses:\n");
		kk = (func->NumKonstA + 3) / 4;
		for (i = 0; i < kk; ++i)
		{
			for (j = 0, k = i; j < 4 && k < func->NumKonstA; j++, k += kk)
			{
				mysnprintf(tmp, countof(tmp), "%3d. %p", k, func->KonstA[k]);
				Printf("%-20s", tmp);
			}
			Printf("\n");
		}
	}
	if (func->KonstS != NULL && func->NumKonstS != 0)
	{
		Printf("\nConstant strings:\n");
		for (i = 0; i < func->NumKonstS; ++i)
		{
			Printf("%3d. %s\n", func->KonstS[k].GetChars());
		}
	}
}

void VMDisasm(const VM_UBYTE *code, int codesize, const VMScriptFunction *func)
{
	const char *name;
	int col;
	int mode;
	int a;

	for (int i = 0; i < codesize; i += 4)
	{
		name = OpInfo[code[i]].Name;
		mode = OpInfo[code[i]].Mode;
		a = code[i+1];

		// String comparison encodes everything in a single instruction.
		if (code[i] == OP_CMPS)
		{
			switch (a & CMP_METHOD_MASK)
			{
			case CMP_EQ:	name = "eq";	break;
			case CMP_LT:	name = "lt";	break;
			case CMP_LE:	name = "le";	break;
			}
			mode = MODE_AIMMZ;
			mode |= (a & CMP_BK) ? MODE_BKS : MODE_BS;
			mode |= (a & CMP_CK) ? MODE_CKS : MODE_CS;
			a &= CMP_CHECK | CMP_APPROX;
		}

		Printf("%08x: %02x%02x%02x%02x %-8s", i, code[i], code[i+1], code[i+2], code[i+3], name);
		col = 0;
		switch (code[i])
		{
		case OP_JMP:
		case OP_TRY:
			col = Printf("%08x", i + 4 + JMPOFS(&code[i]));
			break;

		case OP_RET:
			if (code[i+2] != REGT_NIL)
			{
				if ((code[i+2] & REGT_FINAL) && a == 0)
				{
					col = print_reg(0, *(VM_UHALF *)&code[i+2], MODE_PARAM, 16, func);
				}
				else
				{
					col = print_reg(0, a, (mode & MODE_ATYPE) >> MODE_ASHIFT, 24, func);
					col += print_reg(col, *(VM_UHALF *)&code[i+2], MODE_PARAM, 16, func);
					if (code[i+2] & REGT_FINAL)
					{
						col += Printf(" [final]");
					}
				}
			}
			break;

		default:
			if ((mode & MODE_BCTYPE) == MODE_BCCAST)
			{
				switch (code[i+3])
				{
				case CAST_I2F:
					mode = MODE_AF | MODE_BI | MODE_CUNUSED;
					break;
				case CAST_I2S:
					mode = MODE_AS | MODE_BI | MODE_CUNUSED;
					break;
				case CAST_F2I:
					mode = MODE_AI | MODE_BF | MODE_CUNUSED;
					break;
				case CAST_F2S:
					mode = MODE_AS | MODE_BF | MODE_CUNUSED;
					break;
				case CAST_P2S:
					mode = MODE_AS | MODE_BP | MODE_CUNUSED;
					break;
				case CAST_S2I:
					mode = MODE_AI | MODE_BS | MODE_CUNUSED;
					break;
				case CAST_S2F:
					mode = MODE_AF | MODE_BS | MODE_CUNUSED;
					break;
				default:
					mode = MODE_AX | MODE_BX | MODE_CIMMZ;
					break;
				}
			}
			col = print_reg(0, a, (mode & MODE_ATYPE) >> MODE_ASHIFT, 24, func);
			if ((mode & MODE_BCTYPE) == MODE_BCTHROW)
			{
				mode = (code[i+1] == 0) ? (MODE_BP | MODE_CUNUSED) : (MODE_BKP | MODE_CUNUSED);
			}
			else if ((mode & MODE_BCTYPE) == MODE_BCCATCH)
			{
				switch (code[i+1])
				{
				case 0:
					mode = MODE_BUNUSED | MODE_CUNUSED;
					break;
				case 1:
					mode = MODE_BUNUSED | MODE_CP;
					break;
				case 2:
					mode = MODE_BP | MODE_CP;
					break;
				case 3:
					mode = MODE_BKP | MODE_CP;
					break;
				default:
					mode = MODE_BIMMZ | MODE_CIMMZ;
					break;
				}
			}
			if ((mode & (MODE_BTYPE | MODE_CTYPE)) == MODE_BCJOINT)
			{
				col += print_reg(col, *(VM_UHALF *)&code[i+2], (mode & MODE_BCTYPE) >> MODE_BCSHIFT, 16, func);
			}
			else
			{
				col += print_reg(col, code[i+2], (mode & MODE_BTYPE) >> MODE_BSHIFT, 24, func);
				col += print_reg(col, code[i+3], (mode & MODE_CTYPE) >> MODE_CSHIFT, 24, func);
			}
			break;
		}
		if (col > 30)
		{
			col = 30;
		}
		Printf("%*c", 30 - col, ';');
		if (code[i] == OP_JMP || code[i] == OP_TRY)
		{
			Printf("%d\n", JMPOFS(&code[i]) >> 2);
		}
		else
		{
			Printf("%d,%d,%d\n", code[i+1], code[i+2], code[i+3]);
		}
	}
}

static int print_reg(int col, int arg, int mode, int immshift, const VMScriptFunction *func)
{
	if (mode == MODE_UNUSED)
	{
		return 0;
	}
	if (col > 0)
	{
		col = Printf(",");
	}
	switch(mode)
	{
	case MODE_I:
		return col+Printf("d%d", arg);
	case MODE_F:
		return col+Printf("f%d", arg);
	case MODE_S:
		return col+Printf("s%d", arg);
	case MODE_P:
		return col+Printf("a%d", arg);
	case MODE_V:
		return col+Printf("v%d", arg);

	case MODE_KI:
		if (func != NULL)
		{
			return col+Printf("%d", func->KonstD[arg]);
		}
		return Printf("kd%d", arg);
	case MODE_KF:
		if (func != NULL)
		{
			return col+Printf("%f", func->KonstF[arg]);
		}
		return col+Printf("kf%d", arg);
	case MODE_KS:
		if (func != NULL)
		{
			return col+Printf("\"%s\"", func->KonstS[arg].GetChars());
		}
		return col+Printf("ks%d", arg);
	case MODE_KP:
		if (func != NULL)
		{
			return col+Printf("%p", func->KonstA[arg]);
		}
		return col+Printf("ka%d", arg);
	case MODE_KV:
		if (func != NULL)
		{
			return col+Printf("(%f,%f,%f)", func->KonstF[arg], func->KonstF[arg+1], func->KonstF[arg+2]);
		}
		return col+Printf("kv%d", arg);

	case MODE_IMMS:
		return col+Printf("%d", (arg << immshift) >> immshift);

	case MODE_IMMZ:
		return col+Printf("%d", arg);

	case MODE_PARAM:
		{
			union { VM_UHALF Together; struct { VM_UBYTE RegType, RegNum; }; } p;
			p.Together = arg;
			switch (p.RegType & (REGT_TYPE | REGT_KONST | REGT_MULTIREG))
			{
			case REGT_INT:
				return col+Printf("d%d", p.RegNum);
			case REGT_FLOAT:
				return col+Printf("f%d", p.RegNum);
			case REGT_STRING:
				return col+Printf("s%d", p.RegNum);
			case REGT_POINTER:
				return col+Printf("a%d", p.RegNum);
			case REGT_FLOAT | REGT_MULTIREG:
				return col+Printf("v%d", p.RegNum);
			case REGT_INT | REGT_KONST:
				return col+print_reg(0, p.RegNum, MODE_KI, 0, func);
			case REGT_FLOAT | REGT_KONST:
				return col+print_reg(0, p.RegNum, MODE_KF, 0, func);
			case REGT_STRING | REGT_KONST:
				return col+print_reg(0, p.RegNum, MODE_KS, 0, func);
			case REGT_POINTER | REGT_KONST:
				return col+print_reg(0, p.RegNum, MODE_KP, 0, func);
			case REGT_FLOAT | REGT_MULTIREG | REGT_KONST:
				return col+print_reg(0, p.RegNum, MODE_KV, 0, func);
			default:
				if (p.RegType == REGT_NIL)
				{
					return col+Printf("nil");
				}
				return col+Printf("param[t=%d,%c,%c,n=%d]",
					p.RegType & REGT_TYPE,
					p.RegType & REGT_KONST ? 'k' : 'r',
					p.RegType & REGT_MULTIREG ? 'm' : 's',
					p.RegNum);
			}
		}

	default:
		return col+Printf("$%d", arg);
	}
	return col;
}
