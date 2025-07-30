// ---------------------------------------------------------------------------
//	FM Sound Generator
//	Copyright (C) cisc 1998, 2003.
// ---------------------------------------------------------------------------
//	$Id: fmgeninl.h,v 1.26 2003/06/12 13:14:36 cisc Exp $

#ifndef FM_GEN_INL_H
#define FM_GEN_INL_H

// ---------------------------------------------------------------------------
//	定数その２
//
#define FM_PI			3.14159265358979323846

#define FM_SINEPRESIS	2			// EGとサイン波の精度の差  0(低)-2(高)


#define FM_OPSINBITS	10
#define FM_OPSINENTS	(1 << FM_OPSINBITS)

#define FM_EGCBITS		18			// eg の count のシフト値
#define FM_LFOCBITS		14

#ifdef FM_TUNEBUILD
 #define FM_PGBITS		2
 #define FM_RATIOBITS	0
#else
 #define FM_PGBITS		9
 #define FM_RATIOBITS	7			// 8-12 くらいまで？
#endif

#define FM_EGBITS		16

//extern int paramcount[];
//#define PARAMCHANGE(i) paramcount[i]++;
#define PARAMCHANGE(i)

namespace FM
{

// ---------------------------------------------------------------------------
//	Operator
//
//	フィードバックバッファをクリア
inline void Operator::ResetFB()
{
	out_ = out2_ = 0;
}

//	キーオン
inline void Operator::KeyOn()
{
	if (!keyon_)
	{
		keyon_ = true;
		held_ = false;
		if (eg_phase_ == off || eg_phase_ == release)
		{
#if 1  // libOPNMIDI: experimental SSG-EG
			inverted_ = (ssg_type_ & 4) != 0;
			inverted_ ^= (ssg_type_ & 2) && ar_ != 62; // try to match polarity with nuked OPN
#else
			ssg_phase_ = -1;
#endif
			ShiftPhase(attack);
			EGUpdate();
			in2_ = out_ = out2_ = 0;
			pg_count_ = 0;
		}
	}
}

//	キーオフ
inline void	Operator::KeyOff()
{
	if (keyon_)
	{
		keyon_ = false;
		ShiftPhase(release);
	}
}

//	オペレータは稼働中か？
inline int Operator::IsOn()
{
	return eg_phase_ - off;
}

//	Detune (0-7)
inline void Operator::SetDT(uint dt)
{
	detune_ = dt * 0x20, param_changed_ = true;
	PARAMCHANGE(4);
}

//	DT2 (0-3)
inline void Operator::SetDT2(uint dt2)
{
	detune2_ = dt2 & 3, param_changed_ = true;
	PARAMCHANGE(5);
}

//	Multiple (0-15)
inline void Operator::SetMULTI(uint mul)
{
	multiple_ = mul, param_changed_ = true;
	PARAMCHANGE(6);
}

//	Total Level (0-127) (0.75dB step)
inline void Operator::SetTL(uint tl, bool csm)
{
	if (!csm)
	{
		tl_ = tl, param_changed_ = true;
		PARAMCHANGE(7);
	}
	tl_latch_ = tl;
}

//	Attack Rate (0-63)
inline void Operator::SetAR(uint ar)
{
	ar_ = ar;
	param_changed_ = true;
	PARAMCHANGE(8);
}

//	Decay Rate (0-63)
inline void Operator::SetDR(uint dr)
{
	dr_ = dr;
	param_changed_ = true;
	PARAMCHANGE(9);
}

//	Sustain Rate (0-63)
inline void Operator::SetSR(uint sr)
{
	sr_ = sr;
	param_changed_ = true;
	PARAMCHANGE(10);
}

//	Sustain Level (0-127)
inline void Operator::SetSL(uint sl)
{
	sl_ = sl;
	param_changed_ = true;
	PARAMCHANGE(11);
}

//	Release Rate (0-63)
inline void Operator::SetRR(uint rr)
{
	rr_ = rr;
	param_changed_ = true;
	PARAMCHANGE(12);
}

//	Keyscale (0-3)
inline void Operator::SetKS(uint ks)
{
	ks_ = ks;
	param_changed_ = true;
	PARAMCHANGE(13);
}

//	SSG-type Envelop (0-15)
inline void Operator::SetSSGEC(uint ssgec)
{
	if (ssgec & 8)
		ssg_type_ = ssgec;
	else
		ssg_type_ = 0;
}

inline void Operator::SetAMON(bool amon)
{
	amon_ = amon;
	param_changed_ = true;
	PARAMCHANGE(14);
}

inline void Operator::Mute(bool mute)
{
	mute_ = mute;
	param_changed_ = true;
	PARAMCHANGE(15);
}

inline void Operator::SetMS(uint ms)
{
	ms_ = ms;
	param_changed_ = true;
	PARAMCHANGE(16);
}

// ---------------------------------------------------------------------------
//	4-op Channel

//	オペレータの種類 (LFO) を設定
inline void Channel4::SetType(OpType type)
{
	for (int i=0; i<4; i++)
		op[i].type_ = type;
}

//	セルフ・フィードバックレートの設定 (0-7)
inline void Channel4::SetFB(uint feedback)
{
	fb = fbtable[feedback];
}

//	OPNA 系 LFO の設定
inline void Channel4::SetMS(uint ms)
{
	op[0].SetMS(ms);
	op[1].SetMS(ms);
	op[2].SetMS(ms);
	op[3].SetMS(ms);
}

//	チャンネル・マスク
inline void Channel4::Mute(bool m)
{
	for (int i=0; i<4; i++)
		op[i].Mute(m);
}

//	内部パラメータを再計算
inline void Channel4::Refresh()
{
	for (int i=0; i<4; i++)
		op[i].param_changed_ = true;
	PARAMCHANGE(3);
}

inline void Channel4::SetChip(Chip* chip)
{
	chip_ = chip;
	for (int i=0; i<4; i++)
		op[i].SetChip(chip);
}

// ---------------------------------------------------------------------------
//
//
inline void StoreSample(Sample& dest, ISample data)
{
	if (sizeof(Sample) == 2)
		dest = (Sample) Limit(dest + data, 0x7fff, -0x8000);
	else
		dest += data;
}


// ---------------------------------------------------------------------------
//	AM のレベルを設定
inline void Chip::SetAML(uint l)
{
	aml_ = l & (FM_LFOENTS - 1);
}

//	PM のレベルを設定
inline void Chip::SetPML(uint l)
{
	pml_ = l & (FM_LFOENTS - 1);
}


}

#endif // FM_GEN_INL_H
