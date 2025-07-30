// ---------------------------------------------------------------------------
//	OPN/A/B interface with ADPCM support
//	Copyright (C) cisc 1998, 2001.
// ---------------------------------------------------------------------------
//	$Id: opna.cpp,v 1.68 2003/06/12 14:03:44 cisc Exp $

#include "fmgen_types.h"
#include "fmgen_headers.h"
#include "fmgen_misc.h"
#include "fmgen_opna.h"
#include "fmgen_fmgeninl.h"

#define BUILD_OPN
#define BUILD_OPNA
#define BUILD_OPNB


//	TOFIX:
//	 OPN ch3 �����Prepare�̑ΏۂƂȂ��Ă��܂���Q


// ---------------------------------------------------------------------------
//	OPNA: ADPCM �f�[�^�̊i�[�����̈Ⴂ (8bit/1bit) ���G�~�����[�g���Ȃ�
//	���̃I�v�V������L���ɂ���� ADPCM �������ւ̃A�N�Z�X(���� 8bit ���[�h)��
//	�����y���Ȃ邩��
//
//#define NO_BITTYPE_EMULATION

#ifdef BUILD_OPNA
#include "fmgen_file.h"
#endif

namespace FM
{

// ---------------------------------------------------------------------------
//	OPNBase

#if defined(BUILD_OPN) || defined(BUILD_OPNA) || defined (BUILD_OPNB)

uint32	OPNBase::lfotable[8];			// OPNA/B �p

OPNBase::OPNBase()
{
	prescale = 0;
}

//	�p�����[�^�Z�b�g
void OPNBase::SetParameter(Channel4* ch, uint addr, uint data)
{
	const static uint slottable[4] = { 0, 2, 1, 3 };
	const static uint8 sltable[16] =
	{
		  0,   4,   8,  12,  16,  20,  24,  28,
		 32,  36,  40,  44,  48,  52,  56, 124,
	};

	if ((addr & 3) < 3)
	{
		uint slot = slottable[(addr >> 2) & 3];
		Operator* op = &ch->op[slot];

		switch ((addr >> 4) & 15)
		{
		case 3:	// 30-3E DT/MULTI
			op->SetDT((data >> 4) & 0x07);
			op->SetMULTI(data & 0x0f);
			break;

		case 4: // 40-4E TL
			op->SetTL(data & 0x7f, (regtc & 0x80) && (csmch == ch));
			break;

		case 5: // 50-5E KS/AR
			op->SetKS((data >> 6) & 3);
			op->SetAR((data & 0x1f) * 2);
			break;

		case 6: // 60-6E DR/AMON
			op->SetDR((data & 0x1f) * 2);
			op->SetAMON((data & 0x80) != 0);
			break;

		case 7: // 70-7E SR
			op->SetSR((data & 0x1f) * 2);
			break;

		case 8:	// 80-8E SL/RR
			op->SetSL(sltable[(data >> 4) & 15]);
			op->SetRR((data & 0x0f) * 4 + 2);
			break;

		case 9: // 90-9E SSG-EC
			op->SetSSGEC(data & 0x0f);
			break;
		}
	}
}

//	���Z�b�g
void OPNBase::Reset()
{
	status = 0;
	SetPrescaler(0);
	Timer::Reset();
	psg.Reset();
}

//	�v���X�P�[���ݒ�
void OPNBase::SetPrescaler(uint p)
{
	static const char table[3][2] = { { 6, 4 }, { 3, 2 }, { 2, 1 } };
	static const uint8 table2[8] = { 108,  77,  71,  67,  62,  44,  8,  5 };
	// 512
	if (prescale != p)
	{
		prescale = p;
		/*warning: comparison is always true due to limited range of data type [-Wtype-limits]*/
		// assert(0 <= prescale && prescale < 3);
		assert(prescale < 3);

		uint fmclock = clock / table[p][0] / 12;

		rate = psgrate;

		// �������g���Əo�͎��g���̔�
		assert(fmclock < (0x80000000 >> FM_RATIOBITS));
		uint ratio = ((fmclock << FM_RATIOBITS) + rate/2) / rate;

		SetTimerBase(fmclock);
//		MakeTimeTable(ratio);
		chip.SetRatio(ratio);
		psg.SetClock(clock / table[p][1], psgrate);

		for (int i=0; i<8; i++)
		{
			lfotable[i] = (ratio << (2+FM_LFOCBITS-FM_RATIOBITS)) / table2[i];
		}
	}
}

//	������
bool OPNBase::Init(uint c, uint r)
{
	clock = c;
	psgrate = r;

	return true;
}

//	���ʐݒ�
void OPNBase::SetVolumeFM(int db)
{
	db = Min(db, 20);
	if (db > -192)
		fmvolume = int(16384.0 * pow(10.0, db / 40.0));
	else
		fmvolume = 0;
}

//	�^�C�}�[���ԏ���
void OPNBase::TimerA()
{
	if (regtc & 0x80)
	{
		csmch->KeyControl(0x00);
		csmch->KeyControl(0x0f);
	}
}

void OPNBase::DataSave(struct OPNBaseData* data) {
	Timer::DataSave(&data->timer);
	data->fmvolume = fmvolume;
	data->clock = clock;
	data->rate = rate;
	data->psgrate = psgrate;
	data->status = status;
	data->prescale = prescale;
	chip.DataSave(&data->chip);
	psg.DataSave(&data->psg);
}

void OPNBase::DataLoad(struct OPNBaseData* data) {
	Timer::DataLoad(&data->timer);
	fmvolume = data->fmvolume;
	clock = data->clock;
	rate = data->rate;
	psgrate = data->psgrate;
	status = data->status;
	prescale = data->prescale;
	chip.DataLoad(&data->chip);
	psg.DataLoad(&data->psg);
}

#endif // defined(BUILD_OPN) || defined(BUILD_OPNA) || defined (BUILD_OPNB)

// ---------------------------------------------------------------------------
//	YM2203
//
#ifdef BUILD_OPN

OPN::OPN()
{
	SetVolumeFM(0);
	SetVolumePSG(0);

	csmch = &ch[2];

	for (int i=0; i<3; i++)
	{
		ch[i].SetChip(&chip);
		ch[i].SetType(typeN);
	}
}

//	������
bool OPN::Init(uint c, uint r, bool ip, const char*)
{
	if (!SetRate(c, r, ip))
		return false;

	Reset();

	SetVolumeFM(0);
	SetVolumePSG(0);
	SetChannelMask(0);
	return true;
}

//	�T���v�����O���[�g�ύX
bool OPN::SetRate(uint c, uint r, bool)
{
	OPNBase::Init(c, r);
	RebuildTimeTable();
	return true;
}


//	���Z�b�g
void OPN::Reset()
{
	int i;
	for (i=0x20; i<0x28; i++) SetReg(i, 0);
	for (i=0x30; i<0xc0; i++) SetReg(i, 0);
	OPNBase::Reset();
	ch[0].Reset();
	ch[1].Reset();
	ch[2].Reset();
}


//	���W�X�^�ǂݍ���
uint OPN::GetReg(uint addr)
{
	if (addr < 0x10)
		return psg.GetReg(addr);
	else
		return 0;
}


//	���W�X�^�A���C�Ƀf�[�^��ݒ�
void OPN::SetReg(uint addr, uint data)
{
//	LOG2("reg[%.2x] <- %.2x\n", addr, data);
	if (addr >= 0x100)
		return;

	int c = addr & 3;
	switch (addr)
	{
	case  0: case  1: case  2: case  3: case  4: case  5: case  6: case  7:
	case  8: case  9: case 10: case 11: case 12: case 13: case 14: case 15:
		psg.SetReg(addr, data);
		break;

	case 0x24: case 0x25:
		SetTimerA(addr, data);
		break;

	case 0x26:
		SetTimerB(data);
		break;

	case 0x27:
		SetTimerControl(data);
		break;

	case 0x28:		// Key On/Off
		if ((data & 3) < 3)
			ch[data & 3].KeyControl(data >> 4);
		break;

	case 0x2d: case 0x2e: case 0x2f:
		SetPrescaler(addr-0x2d);
		break;

	// F-Number
	case 0xa0: case 0xa1: case 0xa2:
		fnum[c] = data + fnum2[c] * 0x100;
		break;

	case 0xa4: case 0xa5: case 0xa6:
		fnum2[c] = uint8(data);
		break;

	case 0xa8: case 0xa9: case 0xaa:
		fnum3[c] = data + fnum2[c+3] * 0x100;
		break;

	case 0xac: case 0xad: case 0xae:
		fnum2[c+3] = uint8(data);
		break;

	case 0xb0:	case 0xb1:  case 0xb2:
		ch[c].SetFB((data >> 3) & 7);
		ch[c].SetAlgorithm(data & 7);
		break;

	default:
		if (c < 3)
		{
			if ((addr & 0xf0) == 0x60)
				data &= 0x1f;
			OPNBase::SetParameter(&ch[c], addr, data);
		}
		break;
	}
}

//	�X�e�[�^�X�t���O�ݒ�
void OPN::SetStatus(uint bits)
{
	if (!(status & bits))
	{
		status |= bits;
		Intr(true);
	}
}

void OPN::ResetStatus(uint bit)
{
	status &= ~bit;
	if (!status)
		Intr(false);
}

//	�}�X�N�ݒ�
void OPN::SetChannelMask(uint mask)
{
	for (int i=0; i<3; i++)
		ch[i].Mute(!!(mask & (1 << i)));
	psg.SetChannelMask(mask >> 6);
}

void OPN::DataSave(struct OPNData* data) {
	OPNBase::DataSave(&data->opnbase);
	memcpy(data->fnum, fnum, sizeof(uint) * 3);
	memcpy(data->fnum3, fnum3, sizeof(uint) * 3);
	memcpy(data->fnum2, fnum2, 6);
	for(int i = 0; i < 3; i++) {
		ch[i].DataSave(&data->ch[i]);
	}
}

void OPN::DataLoad(struct OPNData* data) {
	OPNBase::DataLoad(&data->opnbase);
	memcpy(fnum, data->fnum, sizeof(uint) * 3);
	memcpy(fnum3, data->fnum3, sizeof(uint) * 3);
	memcpy(fnum2, data->fnum2, 6);
	for(int i = 0; i < 3; i++) {
		ch[i].DataLoad(&data->ch[i]);
	}
	csmch = &ch[2];
	for (int i=0; i<3; i++)
	{
		ch[i].SetChip(&chip);
		ch[i].SetType(typeN);
	}
}

//	����(2ch)
void OPN::Mix(Sample* buffer, int nsamples)
{
//printf("M:%d\n",nsamples);
#define IStoSample(s)	((Limit(s, 0x7fff, -0x8000) * fmvolume) >> 14)

	psg.Mix(buffer, nsamples);

	// Set F-Number
	ch[0].SetFNum(fnum[0]);
	ch[1].SetFNum(fnum[1]);
	if (!(regtc & 0xc0))
		ch[2].SetFNum(fnum[2]);
	else
	{	// ���ʉ�
		ch[2].op[0].SetFNum(fnum3[1]);
		ch[2].op[1].SetFNum(fnum3[2]);
		ch[2].op[2].SetFNum(fnum3[0]);
		ch[2].op[3].SetFNum(fnum[2]);
	}

	int actch = (((ch[2].Prepare() << 2) | ch[1].Prepare()) << 2) | ch[0].Prepare();
//printf("a %X\n",actch);
	if (actch & 0x15)
	{
		Sample* limit = buffer + nsamples * 2;
		for (Sample* dest = buffer; dest < limit; dest+=2)
		{
			ISample s = 0;
			if (actch & 0x01) s  = ch[0].Calc();
			if (actch & 0x04) s += ch[1].Calc();
			if (actch & 0x10) s += ch[2].Calc();
			s = IStoSample(s);
			StoreSample(dest[0], s);
			StoreSample(dest[1], s);
//printf("%08X,%08X\n",dest[0],dest[1]);
		}
	}
#undef IStoSample
}

#endif // BUILD_OPN

// ---------------------------------------------------------------------------
//	YM2608/2610 common part
// ---------------------------------------------------------------------------

#if defined(BUILD_OPNA) || defined(BUILD_OPNB)

int OPNABase::amtable[FM_LFOENTS] = { -1, };
int OPNABase::pmtable[FM_LFOENTS];

int32 OPNABase::tltable[FM_TLENTS+FM_TLPOS];
bool OPNABase::tablehasmade = false;

OPNABase::OPNABase()
{
	adpcmbuf = 0;
	memaddr = 0;
	startaddr = 0;
	deltan = 256;

	adpcmvol = 0;
	control2 = 0;

	MakeTable2();
	BuildLFOTable();
	for (int i=0; i<6; i++)
	{
		ch[i].SetChip(&chip);
		ch[i].SetType(typeN);
	}
}

OPNABase::~OPNABase()
{
}

// ---------------------------------------------------------------------------
//	������
//
bool OPNABase::Init(uint, uint, bool)
{
	RebuildTimeTable();

	Reset();

	SetVolumeFM(0);
	SetVolumePSG(0);
	SetChannelMask(0);
	return true;
}

// ---------------------------------------------------------------------------
//	�e�[�u���쐬
//
void OPNABase::MakeTable2()
{
	if (!tablehasmade)
	{
		for (int i=-FM_TLPOS; i<FM_TLENTS; i++)
		{
			tltable[i+FM_TLPOS] = uint(65536. * pow(2.0, i * -16. / FM_TLENTS))-1;
		}

		tablehasmade = true;
	}
}

// libOPNMIDI: soft panning
static const uint16 panlawtable[] =
{
    65535, 65529, 65514, 65489, 65454, 65409, 65354, 65289,
    65214, 65129, 65034, 64929, 64814, 64689, 64554, 64410,
    64255, 64091, 63917, 63733, 63540, 63336, 63123, 62901,
    62668, 62426, 62175, 61914, 61644, 61364, 61075, 60776,
    60468, 60151, 59825, 59489, 59145, 58791, 58428, 58057,
    57676, 57287, 56889, 56482, 56067, 55643, 55211, 54770,
    54320, 53863, 53397, 52923, 52441, 51951, 51453, 50947,
    50433, 49912, 49383, 48846, 48302, 47750, 47191,
    46340, /* Center left */
    46340, /* Center right */
    45472, 44885, 44291, 43690, 43083, 42469, 41848, 41221,
    40588, 39948, 39303, 38651, 37994, 37330, 36661, 35986,
    35306, 34621, 33930, 33234, 32533, 31827, 31116, 30400,
    29680, 28955, 28225, 27492, 26754, 26012, 25266, 24516,
    23762, 23005, 22244, 21480, 20713, 19942, 19169, 18392,
    17613, 16831, 16046, 15259, 14469, 13678, 12884, 12088,
    11291, 10492, 9691, 8888, 8085, 7280, 6473, 5666,
    4858, 4050, 3240, 2431, 1620, 810, 0
};

// ---------------------------------------------------------------------------
//	���Z�b�g
//
void OPNABase::Reset()
{
	int i;

	OPNBase::Reset();
	for (i=0x20; i<0x28; i++) SetReg(i, 0);
	for (i=0x30; i<0xc0; i++) SetReg(i, 0);
	for (i=0x130; i<0x1c0; i++) SetReg(i, 0);
	for (i=0x100; i<0x110; i++) SetReg(i, 0);
	for (i=0x10; i<0x20; i++) SetReg(i, 0);
	for (i=0; i<6; i++)
	{
		pan[i] = 3;
		panvolume_l[i] = panvolume_r[i] = panlawtable[64];
		ch[i].Reset();
	}

	stmask = ~0x1c;
	statusnext = 0;
	memaddr = 0;
	adpcmd = 127;
	adpcmx = 0;
	lfocount = 0;
	adpcmplay = false;
	adplc = 0;
	adpld = 0x100;
	status = 0;
	UpdateStatus();
}

// ---------------------------------------------------------------------------
//	�T���v�����O���[�g�ύX
//
bool OPNABase::SetRate(uint c, uint r, bool)
{
	c /= 2;		// �]���łƂ̌݊������d�����������R�����g�A�E�g���悤

	OPNBase::Init(c, r);

	adplbase = int(8192. * (clock/72.) / r);
	adpld = deltan * adplbase >> 16;

	RebuildTimeTable();

	lfodcount = reg22 & 0x08 ? lfotable[reg22 & 7] : 0;
	return true;
}


// ---------------------------------------------------------------------------
//	�`�����l���}�X�N�̐ݒ�
//
void OPNABase::SetChannelMask(uint mask)
{
	for (int i=0; i<6; i++)
		ch[i].Mute(!!(mask & (1 << i)));
	psg.SetChannelMask(mask >> 6);
	adpcmmask_ = (mask & (1 << 9)) != 0;
	rhythmmask_ = (mask >> 10) & ((1 << 6) - 1);
}

void OPNABase::SetPan(uint c, uint8 p)
{
	panvolume_l[c] = panlawtable[p & 0x7f];
	panvolume_r[c] = panlawtable[0x7f - (p & 0x7f)];
}

// ---------------------------------------------------------------------------
void OPNABase::DataSave(struct OPNABaseData* data) {
	OPNBase::DataSave(&data->opnbase);
	memcpy(data->pan, pan, 6);
	memcpy(data->panvolume_l, panvolume_l, sizeof(uint16) * 6);
	memcpy(data->panvolume_r, panvolume_r, sizeof(uint16) * 6);
	memcpy(data->fnum2, fnum2, 9);
	data->reg22 = reg22;
	data->reg29 = reg29;
	data->stmask = stmask;
	data->statusnext = statusnext;
	data->lfocount = lfocount;
	data->lfodcount = lfodcount;
	memcpy(data->fnum, fnum, sizeof(uint) * 6);
	memcpy(data->fnum3, fnum3, sizeof(uint) * 3);
	data->is_adpcmbuf = 0;
	if(adpcmbuf) {
		data->is_adpcmbuf = 1;
		memcpy(data->adpcmbuf, adpcmbuf, 0x40000);
	}
	data->adpcmmask = adpcmmask;
	data->adpcmnotice = adpcmnotice;
	data->startaddr = startaddr;
	data->stopaddr = stopaddr;
	data->memaddr = memaddr;
	data->limitaddr = limitaddr;
	data->adpcmlevel = adpcmlevel;
	data->adpcmvolume = adpcmvolume;
	data->adpcmvol = adpcmvol;
	data->deltan = deltan;
	data->adplc = adplc;
	data->adpld = adpld;
	data->adplbase = adplbase;
	data->adpcmx = adpcmx;
	data->adpcmd = adpcmd;
	data->adpcmout = adpcmout;
	data->apout0 = apout0;
	data->apout1 = apout1;
	data->adpcmreadbuf = adpcmreadbuf;
	data->adpcmplay = adpcmplay;
	data->granuality = granuality;
	data->control1 = control1;
	data->control2 = control2;
	memcpy(data->adpcmreg, adpcmreg, 8);
	data->rhythmmask_ = rhythmmask_;
	for(int i = 0; i < 6; i++) {
		ch[i].DataSave(&data->ch[i]);
	}
}

// ---------------------------------------------------------------------------
void OPNABase::DataLoad(struct OPNABaseData* data) {
	OPNBase::DataLoad(&data->opnbase);
	memcpy(pan, data->pan, 6);
	memcpy(panvolume_l, data->panvolume_l, sizeof(uint16) * 6);
	memcpy(panvolume_r, data->panvolume_r, sizeof(uint16) * 6);
	memcpy(fnum2, data->fnum2, 9);
	reg22 = data->reg22;
	reg29 = data->reg29;
	stmask = data->stmask;
	statusnext = data->statusnext;
	lfocount = data->lfocount;
	lfodcount = data->lfodcount;
	memcpy(fnum, data->fnum, sizeof(uint) * 6);
	memcpy(fnum3, data->fnum3, sizeof(uint) * 3);
	if(data->is_adpcmbuf) {
		if(!adpcmbuf)
			adpcmbuf = new uint8[0x40000];
		memcpy(adpcmbuf, data->adpcmbuf, 0x40000);
	}
	adpcmmask = data->adpcmmask;
	adpcmnotice = data->adpcmnotice;
	startaddr = data->startaddr;
	stopaddr = data->stopaddr;
	memaddr = data->memaddr;
	limitaddr = data->limitaddr;
	adpcmlevel = data->adpcmlevel;
	adpcmvolume = data->adpcmvolume;
	adpcmvol = data->adpcmvol;
	deltan = data->deltan;
	adplc = data->adplc;
	adpld = data->adpld;
	adplbase = data->adplbase;
	adpcmx = data->adpcmx;
	adpcmd = data->adpcmd;
	adpcmout = data->adpcmout;
	apout0 = data->apout0;
	apout1 = data->apout1;
	adpcmreadbuf = data->adpcmreadbuf;
	adpcmplay = data->adpcmplay;
	granuality = data->granuality;
	control1 = data->control1;
	control2 = data->control2;
	memcpy(adpcmreg, data->adpcmreg, 8);
	rhythmmask_ = data->rhythmmask_;
	for(int i = 0; i < 6; i++) {
		ch[i].DataLoad(&data->ch[i]);
		ch[i].SetChip(&chip);
	}
}

// ---------------------------------------------------------------------------
//	���W�X�^�A���C�Ƀf�[�^��ݒ�
//
void OPNABase::SetReg(uint addr, uint data)
{
	int	c = addr & 3;
	switch (addr)
	{
		uint modified;

	// Timer -----------------------------------------------------------------
		case 0x24: case 0x25:
			SetTimerA(addr, data);
			break;

		case 0x26:
			SetTimerB(data);
			break;

		case 0x27:
			SetTimerControl(data);
			break;

	// Misc ------------------------------------------------------------------
	case 0x28:		// Key On/Off
		if ((data & 3) < 3)
		{
			c = (data & 3) + (data & 4 ? 3 : 0);
			ch[c].KeyControl(data >> 4);
		}
		break;

	// Status Mask -----------------------------------------------------------
	case 0x29:
		reg29 = data;
//		UpdateStatus(); //?
		break;

	// Prescaler -------------------------------------------------------------
	case 0x2d: case 0x2e: case 0x2f:
		SetPrescaler(addr-0x2d);
		break;

	// F-Number --------------------------------------------------------------
	case 0x1a0:	case 0x1a1: case 0x1a2:
		c += 3;
		// fall through
	case 0xa0:	case 0xa1: case 0xa2:
		fnum[c] = data + fnum2[c] * 0x100;
		ch[c].SetFNum(fnum[c]);
		break;

	case 0x1a4:	case 0x1a5: case 0x1a6:
		c += 3;
		// fall through
	case 0xa4 : case 0xa5: case 0xa6:
		fnum2[c] = uint8(data);
		break;

	case 0xa8:	case 0xa9: case 0xaa:
		fnum3[c] = data + fnum2[c+6] * 0x100;
		break;

	case 0xac : case 0xad: case 0xae:
		fnum2[c+6] = uint8(data);
		break;

	// Algorithm -------------------------------------------------------------

	case 0x1b0:	case 0x1b1:  case 0x1b2:
		c += 3;
		// fall through
	case 0xb0:	case 0xb1:  case 0xb2:
		ch[c].SetFB((data >> 3) & 7);
		ch[c].SetAlgorithm(data & 7);
		break;

	case 0x1b4: case 0x1b5: case 0x1b6:
		c += 3;
		// fall through
	case 0xb4: case 0xb5: case 0xb6:
		pan[c] = (data >> 6) & 3;
		ch[c].SetMS(data);
		break;

	// LFO -------------------------------------------------------------------
	case 0x22:
		modified = reg22 ^ data;
		reg22 = data;
		if (modified & 0x8)
			lfocount = 0;
		lfodcount = reg22 & 8 ? lfotable[reg22 & 7] : 0;
		break;

	// PSG -------------------------------------------------------------------
	case  0: case  1: case  2: case  3: case  4: case  5: case  6: case  7:
	case  8: case  9: case 10: case 11: case 12: case 13: case 14: case 15:
		psg.SetReg(addr, data);
		break;

	// ���F ------------------------------------------------------------------
	default:
		if (c < 3)
		{
			if (addr & 0x100)
				c += 3;
			OPNBase::SetParameter(&ch[c], addr, data);
		}
		break;
	}
}

// ---------------------------------------------------------------------------
//	ADPCM B
//
void OPNABase::SetADPCMBReg(uint addr, uint data)
{
	switch (addr)
	{
	case 0x00:		// Control Register 1
		if ((data & 0x80) && !adpcmplay)
		{
			adpcmplay = true;
			memaddr = startaddr;
			adpcmx = 0, adpcmd = 127;
			adplc = 0;
		}
		if (data & 1)
		{
			adpcmplay = false;
		}
		control1 = data;
		break;

	case 0x01:		// Control Register 2
		control2 = data;
		granuality = control2 & 2 ? 1 : 4;
		break;

	case 0x02:		// Start Address L
	case 0x03:		// Start Address H
		adpcmreg[addr - 0x02 + 0] = data;
		startaddr = (adpcmreg[1]*256+adpcmreg[0]) << 6;
		memaddr = startaddr;
//		LOG1("  startaddr %.6x", startaddr);
		break;

	case 0x04:		// Stop Address L
	case 0x05:		// Stop Address H
		adpcmreg[addr - 0x04 + 2] = data;
		stopaddr = (adpcmreg[3]*256+adpcmreg[2] + 1) << 6;
//		LOG1("  stopaddr %.6x", stopaddr);
		break;

	case 0x08:		// ADPCM data
		if ((control1 & 0x60) == 0x60)
		{
//			LOG2("  Wr [0x%.5x] = %.2x", memaddr, data);
			WriteRAM(data);
		}
		break;

	case 0x09:		// delta-N L
	case 0x0a:		// delta-N H
		adpcmreg[addr - 0x09 + 4] = data;
		deltan = adpcmreg[5]*256+adpcmreg[4];
		deltan = Max(256, deltan);
		adpld = deltan * adplbase >> 16;
		break;

	case 0x0b:		// Level Control
		adpcmlevel = data;
		adpcmvolume = (adpcmvol * adpcmlevel) >> 12;
		break;

	case 0x0c:		// Limit Address L
	case 0x0d:		// Limit Address H
		adpcmreg[addr - 0x0c + 6] = data;
		limitaddr = (adpcmreg[7]*256+adpcmreg[6] + 1) << 6;
//		LOG1("  limitaddr %.6x", limitaddr);
		break;

	case 0x10:		// Flag Control
		if (data & 0x80)
		{
			status = 0;
			UpdateStatus();
		}
		else
		{
			stmask = ~(data & 0x1f);
//			UpdateStatus();					//???
		}
		break;
	}
}


// ---------------------------------------------------------------------------
//	���W�X�^�擾
//
uint OPNA::GetReg(uint addr)
{
	if (addr < 0x10)
		return psg.GetReg(addr);

	if (addr == 0x108)
	{
//		LOG1("%d:reg[108] ->   ", Diag::GetCPUTick());

		uint data = adpcmreadbuf & 0xff;
		adpcmreadbuf >>= 8;
		if ((control1 & 0x60) == 0x20)
		{
			adpcmreadbuf |= ReadRAM() << 8;
//			LOG2("Rd [0x%.6x:%.2x] ", memaddr, adpcmreadbuf >> 8);
		}
//		LOG0("%.2x\n");
		return data;
	}

	if (addr == 0xff)
		return 1;

	return 0;
}




// ---------------------------------------------------------------------------
//	�X�e�[�^�X�t���O�ݒ�
//
void OPNABase::SetStatus(uint bits)
{
	if (!(status & bits))
	{
//		LOG2("SetStatus(%.2x %.2x)\n", bits, stmask);
		status |= bits & stmask;
		UpdateStatus();
	}
//	else
//		LOG1("SetStatus(%.2x) - ignored\n", bits);
}

void OPNABase::ResetStatus(uint bits)
{
	status &= ~bits;
//	LOG1("ResetStatus(%.2x)\n", bits);
	UpdateStatus();
}

inline void OPNABase::UpdateStatus()
{
//	LOG2("%d:INT = %d\n", Diag::GetCPUTick(), (status & stmask & reg29) != 0);
	Intr((status & stmask & reg29) != 0);
}

// ---------------------------------------------------------------------------
//	ADPCM RAM �ւ̏����ݑ���
//
void OPNABase::WriteRAM(uint data)
{
#ifndef NO_BITTYPE_EMULATION
	if (!(control2 & 2))
	{
		// 1 bit mode
		adpcmbuf[(memaddr >> 4) & 0x3ffff] = data;
		memaddr += 16;
	}
	else
	{
		// 8 bit mode
		uint8* p = &adpcmbuf[(memaddr >> 4) & 0x7fff];
		uint bank = (memaddr >> 1) & 7;
		uint8 mask = 1 << bank;
		data <<= bank;

		p[0x00000] = (p[0x00000] & ~mask) | (uint8(data) & mask); data >>= 1;
		p[0x08000] = (p[0x08000] & ~mask) | (uint8(data) & mask); data >>= 1;
		p[0x10000] = (p[0x10000] & ~mask) | (uint8(data) & mask); data >>= 1;
		p[0x18000] = (p[0x18000] & ~mask) | (uint8(data) & mask); data >>= 1;
		p[0x20000] = (p[0x20000] & ~mask) | (uint8(data) & mask); data >>= 1;
		p[0x28000] = (p[0x28000] & ~mask) | (uint8(data) & mask); data >>= 1;
		p[0x30000] = (p[0x30000] & ~mask) | (uint8(data) & mask); data >>= 1;
		p[0x38000] = (p[0x38000] & ~mask) | (uint8(data) & mask);
		memaddr += 2;
	}
#else
	adpcmbuf[(memaddr >> granuality) & 0x3ffff] = data;
	memaddr += 1 << granuality;
#endif

	if (memaddr == stopaddr)
	{
		SetStatus(4);
		statusnext = 0x04;	// EOS
		memaddr &= 0x3fffff;
	}
	if (memaddr == limitaddr)
	{
//		LOG1("Limit ! (%.8x)\n", limitaddr);
		memaddr = 0;
	}
	SetStatus(8);
}

// ---------------------------------------------------------------------------
//	ADPCM RAM ����̓ǂݍ��ݑ���
//
uint OPNABase::ReadRAM()
{
	uint data;
#ifndef NO_BITTYPE_EMULATION
	if (!(control2 & 2))
	{
		// 1 bit mode
		data = adpcmbuf[(memaddr >> 4) & 0x3ffff];
		memaddr += 16;
	}
	else
	{
		// 8 bit mode
		uint8* p = &adpcmbuf[(memaddr >> 4) & 0x7fff];
		uint bank = (memaddr >> 1) & 7;
		uint8 mask = 1 << bank;

		data =            (p[0x38000] & mask);
		data = data * 2 + (p[0x30000] & mask);
		data = data * 2 + (p[0x28000] & mask);
		data = data * 2 + (p[0x20000] & mask);
		data = data * 2 + (p[0x18000] & mask);
		data = data * 2 + (p[0x10000] & mask);
		data = data * 2 + (p[0x08000] & mask);
		data = data * 2 + (p[0x00000] & mask);
		data >>= bank;
		memaddr += 2;
	}
#else
	data = adpcmbuf[(memaddr >> granuality) & 0x3ffff];
	memaddr += 1 << granuality;
#endif
	if (memaddr == stopaddr)
	{
		SetStatus(4);
		statusnext = 0x04;	// EOS
		memaddr &= 0x3fffff;
	}
	if (memaddr == limitaddr)
	{
//		LOG1("Limit ! (%.8x)\n", limitaddr);
		memaddr = 0;
	}
	if (memaddr < stopaddr)
		SetStatus(8);
	return data;
}


inline int OPNABase::DecodeADPCMBSample(uint data)
{
	static const int table1[16] =
	{
		  1,   3,   5,   7,   9,  11,  13,  15,
		 -1,  -3,  -5,  -7,  -9, -11, -13, -15,
	};
	static const int table2[16] =
	{
		 57,  57,  57,  57,  77, 102, 128, 153,
		 57,  57,  57,  57,  77, 102, 128, 153,
	};
	adpcmx = Limit(adpcmx + table1[data] * adpcmd / 8, 32767, -32768);
	adpcmd = Limit(adpcmd * table2[data] / 64, 24576, 127);
	return adpcmx;
}


// ---------------------------------------------------------------------------
//	ADPCM RAM ����� nibble �ǂݍ��݋y�� ADPCM �W�J
//
int OPNABase::ReadRAMN()
{
	uint data;
	if (granuality > 0)
	{
#ifndef NO_BITTYPE_EMULATION
		if (!(control2 & 2))
		{
			data = adpcmbuf[(memaddr >> 4) & 0x3ffff];
			memaddr += 8;
			if (memaddr & 8)
				return DecodeADPCMBSample(data >> 4);
			data &= 0x0f;
		}
		else
		{
			uint8* p = &adpcmbuf[(memaddr >> 4) & 0x7fff] + ((~memaddr & 1) << 17);
			uint bank = (memaddr >> 1) & 7;
			uint8 mask = 1 << bank;

			data =            (p[0x18000] & mask);
			data = data * 2 + (p[0x10000] & mask);
			data = data * 2 + (p[0x08000] & mask);
			data = data * 2 + (p[0x00000] & mask);
			data >>= bank;
			memaddr ++;
			if (memaddr & 1)
				return DecodeADPCMBSample(data);
		}
#else
		data = adpcmbuf[(memaddr >> granuality) & adpcmmask];
		memaddr += 1 << (granuality-1);
		if (memaddr & (1 << (granuality-1)))
			return DecodeADPCMBSample(data >> 4);
		data &= 0x0f;
#endif
	}
	else
	{
		data = adpcmbuf[(memaddr >> 1) & adpcmmask];
		++memaddr;
		if (memaddr & 1)
			return DecodeADPCMBSample(data >> 4);
		data &= 0x0f;
	}

	DecodeADPCMBSample(data);

	// check
	if (memaddr == stopaddr)
	{
		if (control1 & 0x10)
		{
			memaddr = startaddr;
			data = adpcmx;
			adpcmx = 0, adpcmd = 127;
			return data;
		}
		else
		{
			memaddr &= adpcmmask;	//0x3fffff;
			SetStatus(adpcmnotice);
			adpcmplay = false;
		}
	}

	if (memaddr == limitaddr)
		memaddr = 0;

	return adpcmx;
}

// ---------------------------------------------------------------------------
//	�g���X�e�[�^�X��ǂ݂���
//
uint OPNABase::ReadStatusEx()
{
	uint r = ((status | 8) & stmask) | (adpcmplay ? 0x20 : 0);
	status |= statusnext;
	statusnext = 0;
	return r;
}

// ---------------------------------------------------------------------------
//	ADPCM �W�J
//
inline void OPNABase::DecodeADPCMB()
{
	apout0 = apout1;
	int n = (ReadRAMN() * adpcmvolume) >> 13;
	apout1 = adpcmout + n;
	adpcmout = n;
}

// ---------------------------------------------------------------------------
//	ADPCM ����
//
void OPNABase::ADPCMBMix(Sample* dest, uint count)
{
	uint maskl = control2 & 0x80 ? -1 : 0;
	uint maskr = control2 & 0x40 ? -1 : 0;
	if (adpcmmask_)
	{
		maskl = maskr = 0;
	}

	if (adpcmplay)
	{
//		LOG2("ADPCM Play: %d   DeltaN: %d\n", adpld, deltan);
		if (adpld <= 8192)		// fplay < fsamp
		{
			for (; count>0; count--)
			{
				if (adplc < 0)
				{
					adplc += 8192;
					DecodeADPCMB();
					if (!adpcmplay)
						break;
				}
				int s = (adplc * apout0 + (8192-adplc) * apout1) >> 13;
				StoreSample(dest[0], s & maskl);
				StoreSample(dest[1], s & maskr);
				dest += 2;
				adplc -= adpld;
			}
			for (; count>0 && apout0; count--)
			{
				if (adplc < 0)
				{
					apout0 = apout1, apout1 = 0;
					adplc += 8192;
				}
				int s = (adplc * apout1) >> 13;
				StoreSample(dest[0], s & maskl);
				StoreSample(dest[1], s & maskr);
				dest += 2;
				adplc -= adpld;
			}
		}
		else	// fplay > fsamp	(adpld = fplay/famp*8192)
		{
			int t = (-8192*8192)/adpld;
			for (; count>0; count--)
			{
				int s = apout0 * (8192+adplc);
				while (adplc < 0)
				{
					DecodeADPCMB();
					if (!adpcmplay)
						goto stop;
					s -= apout0 * Max(adplc, t);
					adplc -= t;
				}
				adplc -= 8192;
				s >>= 13;
				StoreSample(dest[0], s & maskl);
				StoreSample(dest[1], s & maskr);
				dest += 2;
			}
stop:
			;
		}
	}
	if (!adpcmplay)
	{
		apout0 = apout1 = adpcmout = 0;
		adplc = 0;
	}
}

// ---------------------------------------------------------------------------
//	����
//	in:		buffer		������
//			nsamples	�����T���v����
//
void OPNABase::FMMix(Sample* buffer, int nsamples)
{
	if (fmvolume > 0)
	{
		// ����
		// Set F-Number
		if (!(regtc & 0xc0))
			csmch->SetFNum(fnum[csmch-ch]);
		else
		{
			// ���ʉ����[�h
			csmch->op[0].SetFNum(fnum3[1]);	csmch->op[1].SetFNum(fnum3[2]);
			csmch->op[2].SetFNum(fnum3[0]);	csmch->op[3].SetFNum(fnum[2]);
		}

		int act = (((ch[2].Prepare() << 2) | ch[1].Prepare()) << 2) | ch[0].Prepare();
		if (reg29 & 0x80)
			act |= (ch[3].Prepare() | ((ch[4].Prepare() | (ch[5].Prepare() << 2)) << 2)) << 6;
		if (!(reg22 & 0x08))
			act &= 0x555;

		if (act & 0x555)
		{
			Mix6(buffer, nsamples, act);
		}
	}
}

// ---------------------------------------------------------------------------

void OPNABase::MixSubSL(int activech, ISample** dest)
{
	if (activech & 0x001) (*dest[0]  = ch[0].CalcL());
	if (activech & 0x004) (*dest[1] += ch[1].CalcL());
	if (activech & 0x010) (*dest[2] += ch[2].CalcL());
	if (activech & 0x040) (*dest[3] += ch[3].CalcL());
	if (activech & 0x100) (*dest[4] += ch[4].CalcL());
	if (activech & 0x400) (*dest[5] += ch[5].CalcL());
}

inline void OPNABase::MixSubS(int activech, ISample** dest)
{
	if (activech & 0x001) (*dest[0]  = ch[0].Calc());
	if (activech & 0x004) (*dest[1] += ch[1].Calc());
	if (activech & 0x010) (*dest[2] += ch[2].Calc());
	if (activech & 0x040) (*dest[3] += ch[3].Calc());
	if (activech & 0x100) (*dest[4] += ch[4].Calc());
	if (activech & 0x400) (*dest[5] += ch[5].Calc());
}

// ---------------------------------------------------------------------------

void OPNABase::BuildLFOTable()
{
	if (amtable[0] == -1)
	{
		for (int c=0; c<256; c++)
		{
			int v;
			if (c < 0x40)		v = c * 2 + 0x80;
			else if (c < 0xc0)	v = 0x7f - (c - 0x40) * 2 + 0x80;
			else				v = (c - 0xc0) * 2;
			pmtable[c] = c;

			if (c < 0x80)		v = 0xff - c * 2;
			else				v = (c - 0x80) * 2;
			amtable[c] = v & ~3;
		}
	}
}

// ---------------------------------------------------------------------------

inline void OPNABase::LFO()
{
//	LOG3("%4d - %8d, %8d\n", c, lfocount, lfodcount);

//	Operator::SetPML(pmtable[(lfocount >> (FM_LFOCBITS+1)) & 0xff]);
//	Operator::SetAML(amtable[(lfocount >> (FM_LFOCBITS+1)) & 0xff]);
	chip.SetPML(pmtable[(lfocount >> (FM_LFOCBITS+1)) & 0xff]);
	chip.SetAML(amtable[(lfocount >> (FM_LFOCBITS+1)) & 0xff]);
	lfocount += lfodcount;
}

// ---------------------------------------------------------------------------
//	����
//
#define IStoSample(s)	((Limit(s, 0x7fff, -0x8000) * fmvolume) >> 14)

void OPNABase::Mix6(Sample* buffer, int nsamples, int activech)
{
	// Mix
	// libOPNMIDI: rewrite for panning support

	const uint activechmask[6] = {0x001, 0x004, 0x010, 0x040, 0x100, 0x400};

	Sample* limit = buffer + nsamples * 2;
	for (Sample* dest = buffer; dest < limit; dest+=2)
	{
		ISample out[6];
		if (activech & 0xaaa)
		{
			LFO();
			for (uint c = 0; c<6; ++c)
				out[c] = (activechmask[c] & activech) ? ch[c].CalcL() : 0;
		}
		else
		{
			for (uint c = 0; c<6; ++c)
				out[c] = (activechmask[c] & activech) ? ch[c].Calc() : 0;
		}

		int lrouts[2] = {0, 0};
		for (uint c = 0; c<6; ++c)
		{
			int panl = panvolume_l[c];
			int panr = panvolume_r[c];
			panl = (pan[c] & 2) ? panl : 0;
			panr = (pan[c] & 1) ? panr : 0;
			lrouts[0] += out[c] * panl / 65535;
			lrouts[1] += out[c] * panr / 65535;
		}

		StoreSample(dest[0], lrouts[0]);
		StoreSample(dest[1], lrouts[1]);
	}
}

#endif // defined(BUILD_OPNA) || defined(BUILD_OPNB)

// ---------------------------------------------------------------------------
//	YM2608(OPNA)
// ---------------------------------------------------------------------------

#ifdef BUILD_OPNA

// ---------------------------------------------------------------------------
//	�\�z
//
OPNA::OPNA()
{
	for (int i=0; i<6; i++)
	{
		rhythm[i].sample = 0;
		rhythm[i].pos = 0;
		rhythm[i].size = 0;
		rhythm[i].volume = 0;
	}
	rhythmtvol = 0;
	adpcmmask = 0x3ffff;
	adpcmnotice = 4;
	csmch = &ch[2];
}

// ---------------------------------------------------------------------------

OPNA::~OPNA()
{
	delete[] adpcmbuf;
	for (int i=0; i<6; i++)
		delete[] rhythm[i].sample;
}



// ---------------------------------------------------------------------------
//	������
//
bool OPNA::Init(uint c, uint r, bool ipflag, const char* path)
{
	rate = 8000;
	LoadRhythmSample(path);

	if (!adpcmbuf)
		adpcmbuf = new uint8[0x40000];
	if (!adpcmbuf)
		return false;

	if (!SetRate(c, r, ipflag))
		return false;
	if (!OPNABase::Init(c, r, ipflag))
		return false;

	Reset();

	SetVolumeADPCM(0);
	SetVolumeRhythmTotal(0);
	for (int i=0; i<6; i++)
		SetVolumeRhythm(0, 0);
	return true;
}

// ---------------------------------------------------------------------------
//	���Z�b�g
//
void OPNA::Reset()
{
	reg29 = 0x1f;
	rhythmkey = 0;
	limitaddr = 0x3ffff;
	OPNABase::Reset();
}

// ---------------------------------------------------------------------------
//	�T���v�����O���[�g�ύX
//
bool OPNA::SetRate(uint c, uint r, bool ipflag)
{
	if (!OPNABase::SetRate(c, r, ipflag))
		return false;

	for (int i=0; i<6; i++)
	{
		rhythm[i].step = rhythm[i].rate * 1024 / r;
	}
	return true;
}


// ---------------------------------------------------------------------------
//	���Y������ǂ݂���
//
bool OPNA::LoadRhythmSample(const char* path)
{
	static const char* rhythmname[6] =
	{
		"bd", "sd", "top", "hh", "tom", "rim",
	};

	int i;
	for (i=0; i<6; i++)
		rhythm[i].pos = ~0u;

	for (i=0; i<6; i++)
	{
		FileIO file;
		uint32 fsize;
		char buf[MAX_PATH + 1] = "";
		if (path)
			strncpy(buf, path, MAX_PATH);
		strncat(buf, "2608_", MAX_PATH);
		strncat(buf, rhythmname[i], MAX_PATH);
		strncat(buf, ".wav", MAX_PATH);

		if (!file.Open(buf, FileIO::readonly))
		{
			if (i != 5)
				break;
			if (path)
				strncpy(buf, path, MAX_PATH);
			strncpy(buf, "2608_rym.wav", MAX_PATH);
			if (!file.Open(buf, FileIO::readonly))
				break;
		}

		struct
		{
			uint32 chunksize;
			uint16 tag;
			uint16 nch;
			uint32 rate;
			uint32 avgbytes;
			uint16 align;
			uint16 bps;
			uint16 size;
		} whdr;

		file.Seek(0x10, FileIO::begin);
		file.Read(&whdr, sizeof(whdr));

		uint8 subchunkname[4];
		fsize = 4 + whdr.chunksize - sizeof(whdr);
		do
		{
			file.Seek(fsize, FileIO::current);
			file.Read(&subchunkname, 4);
			file.Read(&fsize, 4);
		} while (memcmp("data", subchunkname, 4));

		fsize /= 2;
		if (fsize >= 0x100000 || whdr.tag != 1 || whdr.nch != 1)
			break;
		fsize = Max(fsize, (1<<31)/1024);

		if(!rhythm[i].sample)
			delete rhythm[i].sample;
		rhythm[i].sample = new int16[fsize];
		if (!rhythm[i].sample)
			break;

		file.Read(rhythm[i].sample, fsize * 2);

		rhythm[i].rate = whdr.rate;
		rhythm[i].step = rhythm[i].rate * 1024 / rate;
		rhythm[i].pos = rhythm[i].size = fsize * 1024;
	}
	if (i != 6)
	{
		for (i=0; i<6; i++)
		{
			delete[] rhythm[i].sample;
			rhythm[i].sample = 0;
		}
		return false;
	}
	return true;
}



// ---------------------------------------------------------------------------
//	���W�X�^�A���C�Ƀf�[�^��ݒ�
//
void OPNA::SetReg(uint addr, uint data)
{
	addr &= 0x1ff;

	switch (addr)
	{
	case 0x29:
		reg29 = data;
//		UpdateStatus(); //?
		break;

	// Rhythm ----------------------------------------------------------------
	case 0x10:			// DM/KEYON
		if (!(data & 0x80))  // KEY ON
		{
			rhythmkey |= data & 0x3f;
			if (data & 0x01) rhythm[0].pos = 0;
			if (data & 0x02) rhythm[1].pos = 0;
			if (data & 0x04) rhythm[2].pos = 0;
			if (data & 0x08) rhythm[3].pos = 0;
			if (data & 0x10) rhythm[4].pos = 0;
			if (data & 0x20) rhythm[5].pos = 0;
		}
		else
		{					// DUMP
			rhythmkey &= ~data;
		}
		break;

	case 0x11:
		rhythmtl = ~data & 63;
		break;

	case 0x18: 		// Bass Drum
	case 0x19:		// Snare Drum
	case 0x1a:		// Top Cymbal
	case 0x1b:		// Hihat
	case 0x1c:		// Tom-tom
	case 0x1d:		// Rim shot
		rhythm[addr & 7].pan   = (data >> 6) & 3;
		rhythm[addr & 7].level = ~data & 31;
		break;

	case 0x100: case 0x101:
	case 0x102: case 0x103:
	case 0x104: case 0x105:
	case 0x108:	case 0x109:
	case 0x10a:	case 0x10b:
	case 0x10c:	case 0x10d:
	case 0x110:
		OPNABase::SetADPCMBReg(addr - 0x100, data);
		break;

	default:
		OPNABase::SetReg(addr, data);
		break;
	}
}

// ---------------------------------------------------------------------------
void OPNA::DataSave(struct OPNAData* data) {
	OPNABase::DataSave(&data->opnabase);
	memcpy(data->rhythm, rhythm, sizeof(Rhythm) * 6);
	data->rhythmtl = rhythmtl;
	data->rhythmtvol = rhythmtvol;
	data->rhythmkey = rhythmkey;
}

// ---------------------------------------------------------------------------
void OPNA::DataLoad(struct OPNAData* data) {
	OPNABase::DataLoad(&data->opnabase);
	memcpy(rhythm, data->rhythm, sizeof(Rhythm) * 6);
	rhythmtl = data->rhythmtl;
	rhythmtvol = data->rhythmtvol;
	rhythmkey = data->rhythmkey;
	csmch = &ch[2];
}

// ---------------------------------------------------------------------------
//	���Y������
//
void OPNA::RhythmMix(Sample* buffer, uint count)
{
	if (rhythmtvol < 128 && rhythm[0].sample && (rhythmkey & 0x3f))
	{
		Sample* limit = buffer + count * 2;
		for (int i=0; i<6; i++)
		{
			Rhythm& r = rhythm[i];
			if ((rhythmkey & (1 << i)) /*&& r.level < 128*/) // libOPNMIDI: Useless condition, signed int 8t has 127 max
			{
				int db = Limit(rhythmtl+rhythmtvol+r.level+r.volume, 127, -31);
				int vol = tltable[FM_TLPOS+(db << (FM_TLBITS-7))] >> 4;
				int maskl = -((r.pan >> 1) & 1);
				int maskr = -(r.pan & 1);

				if (rhythmmask_ & (1 << i))
				{
					maskl = maskr = 0;
				}

				for (Sample* dest = buffer; dest<limit && r.pos < r.size; dest+=2)
				{
					int sample = (r.sample[r.pos / 1024] * vol) >> 12;
					r.pos += r.step;
					StoreSample(dest[0], sample & maskl);
					StoreSample(dest[1], sample & maskr);
				}
			}
		}
	}
}

// ---------------------------------------------------------------------------
//	���ʐݒ�
//
void OPNA::SetVolumeRhythmTotal(int db)
{
	db = Min(db, 20);
	rhythmtvol = -(db * 2 / 3);
}

void OPNA::SetVolumeRhythm(int index, int db)
{
	db = Min(db, 20);
	rhythm[index].volume = -(db * 2 / 3);
}

void OPNA::SetVolumeADPCM(int db)
{
	db = Min(db, 20);
	if (db > -192)
		adpcmvol = int(65536.0 * pow(10.0, db / 40.0));
	else
		adpcmvol = 0;

	adpcmvolume = (adpcmvol * adpcmlevel) >> 12;
}

// ---------------------------------------------------------------------------
//	����
//	in:		buffer		������
//			nsamples	�����T���v����
//
void OPNA::Mix(Sample* buffer, int nsamples)
{
	FMMix(buffer, nsamples);
	psg.Mix(buffer, nsamples);
	ADPCMBMix(buffer, nsamples);
	RhythmMix(buffer, nsamples);
}

#endif // BUILD_OPNA

// ---------------------------------------------------------------------------
//	YM2610(OPNB)
// ---------------------------------------------------------------------------

#ifdef BUILD_OPNB

// ---------------------------------------------------------------------------
//	�\�z
//
OPNB::OPNB()
{
	adpcmabuf = 0;
	adpcmasize = 0;
	for (int i=0; i<6; i++)
	{
		adpcma[i].pan = 0;
		adpcma[i].level = 0;
		adpcma[i].volume = 0;
		adpcma[i].pos = 0;
		adpcma[i].step = 0;
		adpcma[i].volume = 0;
		adpcma[i].start = 0;
		adpcma[i].stop = 0;
		adpcma[i].adpcmx = 0;
		adpcma[i].adpcmd = 0;
	}
	adpcmatl = 0;
	adpcmakey = 0;
	adpcmatvol = 0;
	adpcmmask = 0;
	adpcmnotice = 0x8000;
	granuality = -1;
	csmch = &ch[2];

	InitADPCMATable();
}

OPNB::~OPNB()
{
}

// ---------------------------------------------------------------------------
//	������
//
bool OPNB::Init(uint c, uint r, bool ipflag,
				uint8 *_adpcma, int _adpcma_size,
				uint8 *_adpcmb, int _adpcmb_size)
{
	int i;
	if (!SetRate(c, r, ipflag))
		return false;
	if (!OPNABase::Init(c, r, ipflag))
		return false;

	adpcmabuf = _adpcma;
	adpcmasize = _adpcma_size;
	adpcmbuf = _adpcmb;

	for (i=0; i<=24; i++)		// max 16M bytes
	{
		if (_adpcmb_size <= (1 << i))
		{
			adpcmmask = (1 << i) - 1;
			break;
		}
	}

//	adpcmmask = _adpcmb_size - 1;
	limitaddr = adpcmmask;

	Reset();

	SetVolumeFM(0);
	SetVolumePSG(0);
	SetVolumeADPCMB(0);
	SetVolumeADPCMATotal(0);
	for (i=0; i<6; i++)
		SetVolumeADPCMA(0, 0);
	SetChannelMask(0);
	return true;
}

// ---------------------------------------------------------------------------
//	���Z�b�g
//
void OPNB::Reset()
{
	OPNABase::Reset();

	stmask = ~0;
	adpcmakey = 0;
	reg29 = ~0;

	for (int i=0; i<6; i++)
	{
		adpcma[i].pan = 0;
		adpcma[i].level = 0;
		adpcma[i].volume = 0;
		adpcma[i].pos = 0;
		adpcma[i].step = 0;
		adpcma[i].volume = 0;
		adpcma[i].start = 0;
		adpcma[i].stop = 0;
		adpcma[i].adpcmx = 0;
		adpcma[i].adpcmd = 0;
	}
}

// ---------------------------------------------------------------------------
//	�T���v�����O���[�g�ύX
//
bool OPNB::SetRate(uint c, uint r, bool ipflag)
{
	if (!OPNABase::SetRate(c, r, ipflag))
		return false;

	adpcmastep = int(double(c) / 54 * 8192 / r);
	return true;
}

// ---------------------------------------------------------------------------
//	���W�X�^�A���C�Ƀf�[�^��ݒ�
//
void OPNB::SetReg(uint addr, uint data)
{
	addr &= 0x1ff;

	switch (addr)
	{
	// omitted registers
	case 0x29:
	case 0x2d: case 0x2e: case 0x2f:
		break;

	// ADPCM A ---------------------------------------------------------------
	case 0x100:			// DM/KEYON
		if (!(data & 0x80))  // KEY ON
		{
			adpcmakey |= data & 0x3f;
			for (int c=0; c<6; c++)
			{
				if (data & (1<<c))
				{
					ResetStatus(0x100 << c);
					adpcma[c].pos = adpcma[c].start;
//					adpcma[c].step = 0x10000 - adpcma[c].step;
					adpcma[c].step = 0;
					adpcma[c].adpcmx = 0;
					adpcma[c].adpcmd = 0;
					adpcma[c].nibble = 0;
				}
			}
		}
		else
		{					// DUMP
			adpcmakey &= ~data;
		}
		break;

	case 0x101:
		adpcmatl = ~data & 63;
		break;

	case 0x108:	case 0x109:	case 0x10a:
	case 0x10b: case 0x10c:	case 0x10d:
		adpcma[addr & 7].pan   = (data >> 6) & 3;
		adpcma[addr & 7].level = ~data & 31;
		break;

	case 0x110: case 0x111: case 0x112:	// START ADDRESS (L)
	case 0x113: case 0x114:	case 0x115:
	case 0x118: case 0x119: case 0x11a:	// START ADDRESS (H)
	case 0x11b: case 0x11c: case 0x11d:
		adpcmareg[addr - 0x110] = data;
		adpcma[addr & 7].pos = adpcma[addr & 7].start =
			(adpcmareg[(addr&7)+8]*256+adpcmareg[addr&7]) << 9;
		break;

	case 0x120: case 0x121: case 0x122:	// END ADDRESS (L)
	case 0x123: case 0x124: case 0x125:
	case 0x128: case 0x129: case 0x12a:	// END ADDRESS (H)
	case 0x12b: case 0x12c: case 0x12d:
		adpcmareg[addr - 0x110] = data;
		adpcma[addr & 7].stop =
			(adpcmareg[(addr&7)+24]*256+adpcmareg[(addr&7)+16] + 1) << 9;
		break;

	// ADPCMB -----------------------------------------------------------------
	case 0x10:
		if ((data & 0x80) && !adpcmplay)
		{
			adpcmplay = true;
			memaddr = startaddr;
			adpcmx = 0, adpcmd = 127;
			adplc = 0;
		}
		if (data & 1)
			adpcmplay = false;
		control1 = data & 0x91;
		break;


	case 0x11:		// Control Register 2
		control2 = data & 0xc0;
		break;

	case 0x12:		// Start Address L
	case 0x13:		// Start Address H
		adpcmreg[addr - 0x12 + 0] = data;
		startaddr = (adpcmreg[1]*256+adpcmreg[0]) << 9;
		memaddr = startaddr;
		break;

	case 0x14:		// Stop Address L
	case 0x15:		// Stop Address H
		adpcmreg[addr - 0x14 + 2] = data;
		stopaddr = (adpcmreg[3]*256+adpcmreg[2] + 1) << 9;
//		LOG1("  stopaddr %.6x", stopaddr);
		break;

	case 0x19:		// delta-N L
	case 0x1a:		// delta-N H
		adpcmreg[addr - 0x19 + 4] = data;
		deltan = adpcmreg[5]*256+adpcmreg[4];
		deltan = Max(256, deltan);
		adpld = deltan * adplbase >> 16;
		break;

	case 0x1b:		// Level Control
		adpcmlevel = data;
		adpcmvolume = (adpcmvol * adpcmlevel) >> 12;
		break;

	case 0x1c:		// Flag Control
		stmask = ~((data & 0xbf) << 8);
		status &= stmask;
		UpdateStatus();
		break;

	default:
		OPNABase::SetReg(addr, data);
		break;
	}
//	LOG0("\n");
}

// ---------------------------------------------------------------------------
//	���W�X�^�擾
//
uint OPNB::GetReg(uint addr)
{
	if (addr < 0x10)
		return psg.GetReg(addr);

	return 0;
}

// ---------------------------------------------------------------------------
//	�g���X�e�[�^�X��ǂ݂���
//
uint OPNB::ReadStatusEx()
{
	return (status & stmask) >> 8;
}

// ---------------------------------------------------------------------------
//	YM2610
//
int OPNB::jedi_table[(48+1)*16];

void OPNB::InitADPCMATable()
{
	const static int8 table2[] =
	{
		 1,  3,  5,  7,  9, 11, 13, 15,
		-1, -3, -5, -7, -9,-11,-13,-15,
	};

	for (int i=0; i<=48; i++)
	{
		int s = int(16.0 * pow (1.1, i) * 3);
		for (int j=0; j<16; j++)
		{
			jedi_table[i*16+j] = s * table2[j] / 8;
		}
	}
}

// ---------------------------------------------------------------------------
//	ADPCMA ����
//
void OPNB::ADPCMAMix(Sample* buffer, uint count)
{
	const static int decode_tableA1[16] =
	{
		-1*16, -1*16, -1*16, -1*16, 2*16, 5*16, 7*16, 9*16,
		-1*16, -1*16, -1*16, -1*16, 2*16, 5*16, 7*16, 9*16
	};

	if (adpcmatvol < 128 && (adpcmakey & 0x3f))
	{
		Sample* limit = buffer + count * 2;
		for (int i=0; i<6; i++)
		{
			ADPCMA& r = adpcma[i];
			if ((adpcmakey & (1 << i))/* && r.level < 128*/) // libOPNMIDI: Useless condition, signed int 8t has 127 max
			{
				uint maskl = r.pan & 2 ? -1 : 0;
				uint maskr = r.pan & 1 ? -1 : 0;
				if (rhythmmask_ & (1 << i))
				{
					maskl = maskr = 0;
				}

				int db = Limit(adpcmatl+adpcmatvol+r.level+r.volume, 127, -31);
				int vol = tltable[FM_TLPOS+(db << (FM_TLBITS-7))] >> 4;

				Sample* dest = buffer;
				for ( ; dest<limit; dest+=2)
				{
					r.step += adpcmastep;
					if (r.pos >= r.stop)
					{
						SetStatus(0x100 << i);
						adpcmakey &= ~(1<<i);
						break;
					}

					for (; r.step > 0x10000; r.step -= 0x10000)
					{
						int data;
						if (!(r.pos & 1))
						{
							r.nibble = adpcmabuf[r.pos>>1];
							data = r.nibble >> 4;
						}
						else
						{
							data = r.nibble & 0x0f;
						}
						r.pos++;

						r.adpcmx += jedi_table[r.adpcmd + data];
						r.adpcmx = Limit(r.adpcmx, 2048*3-1, -2048*3);
						r.adpcmd += decode_tableA1[data];
						r.adpcmd = Limit(r.adpcmd, 48*16, 0);
					}
					int sample = (r.adpcmx * vol) >> 10;
					StoreSample(dest[0], sample & maskl);
					StoreSample(dest[1], sample & maskr);
				}
			}
		}
	}
}

// ---------------------------------------------------------------------------
//	���ʐݒ�
//
void OPNB::SetVolumeADPCMATotal(int db)
{
	db = Min(db, 20);
	adpcmatvol = -(db * 2 / 3);
}

void OPNB::SetVolumeADPCMA(int index, int db)
{
	db = Min(db, 20);
	adpcma[index].volume = -(db * 2 / 3);
}

void OPNB::SetVolumeADPCMB(int db)
{
	db = Min(db, 20);
	if (db > -192)
		adpcmvol = int(65536.0 * pow(10, db / 40.0));
	else
		adpcmvol = 0;
}

// ---------------------------------------------------------------------------
void OPNB::DataSave(struct OPNBData* data, void* adpcmadata) {
	OPNABase::DataSave(&data->opnabase);
	if(adpcmasize) {
		adpcmadata = malloc(adpcmasize);
		memcpy(adpcmadata, adpcmabuf, adpcmasize);
	}
	data->adpcmasize = adpcmasize;
	memcpy(data->adpcma, adpcma, sizeof(ADPCMA) * 6);
	data->adpcmatl = adpcmatl;
	data->adpcmatvol = adpcmatvol;
	data->adpcmakey = adpcmakey;
	data->adpcmastep = adpcmastep;
	memcpy(data->adpcmareg, adpcmareg, 32);
	for(int i = 0; i < 6; i++) {
		ch[i].DataSave(&data->ch[i]);
	}
}

// ---------------------------------------------------------------------------
void OPNB::DataLoad(struct OPNBData* data, void* adpcmadata) {
	OPNABase::DataLoad(&data->opnabase);  // libOPNMIDI: bugfix DataLoad (was DataSave here)
	if(data->adpcmasize) {
		adpcmabuf = (uint8*)malloc(data->adpcmasize);
		memcpy(adpcmabuf, adpcmadata, data->adpcmasize);
	}
	adpcmasize = data->adpcmasize;
	memcpy(adpcma, data->adpcma, sizeof(ADPCMA) * 6);
	adpcmatl = data->adpcmatl;
	adpcmatvol = data->adpcmatvol;
	adpcmakey = data->adpcmakey;
	adpcmastep = data->adpcmastep;
	memcpy(adpcmareg, data->adpcmareg, 32);
	for(int i = 0; i < 6; i++) {
		ch[i].DataLoad(&data->ch[i]);
	}
	csmch = &ch[2];
}

// ---------------------------------------------------------------------------
//	����
//	in:		buffer		������
//			nsamples	�����T���v����
//
void OPNB::Mix(Sample* buffer, int nsamples)
{
	FMMix(buffer, nsamples);
	psg.Mix(buffer, nsamples);
	ADPCMBMix(buffer, nsamples);
	ADPCMAMix(buffer, nsamples);
}

#endif // BUILD_OPNB

}	// namespace FM
