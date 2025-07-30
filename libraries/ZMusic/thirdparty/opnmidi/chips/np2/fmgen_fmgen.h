// ---------------------------------------------------------------------------
//	FM Sound Generator
//	Copyright (C) cisc 1998, 2001.
// ---------------------------------------------------------------------------
//	$Id: fmgen.h,v 1.37 2003/08/25 13:33:11 cisc Exp $

#ifndef FM_GEN_H
#define FM_GEN_H

#include "fmgen_types.h"

// ---------------------------------------------------------------------------
//	出力サンプルの型
//
// libOPNMIDI: change int32 to int16
#define FM_SAMPLETYPE	int16				// int16 or int32

// ---------------------------------------------------------------------------
//	定数その１
//	静的テーブルのサイズ

#define FM_LFOBITS		8					// 変更不可
#define FM_TLBITS		7

// ---------------------------------------------------------------------------

#define FM_TLENTS		(1 << FM_TLBITS)
#define FM_LFOENTS		(1 << FM_LFOBITS)
#define FM_TLPOS		(FM_TLENTS/4)

//	サイン波の精度は 2^(1/256)
#define FM_CLENTS		(0x1000 * 2)	// sin + TL + LFO

// ---------------------------------------------------------------------------

namespace FM
{	
	//	Types ----------------------------------------------------------------
	typedef FM_SAMPLETYPE	Sample;
	typedef int32 			ISample;

	enum OpType { typeN=0, typeM=1 };
	enum EGPhase { next, attack, decay, sustain, release, off };

	void StoreSample(ISample& dest, int data);

	class Chip;
	struct ChipData;

	//	Operator -------------------------------------------------------------
	struct OperatorData {
		ISample	out_;
		ISample	out2_;
		ISample in2_;
		uint	dp_;
		uint	detune_;
		uint	detune2_;
		uint	multiple_;
		uint32	pg_count_;
		uint32	pg_diff_;
		int32	pg_diff_lfo_;
		OpType	type_;
		uint	bn_;
		int		eg_level_;
		int		eg_level_on_next_phase_;
		int		eg_count_;
		int		eg_count_diff_;
		int		eg_out_;
		int		tl_out_;
		int		eg_rate_;
		int		eg_curve_count_;
#if 0  // libOPNMIDI: experimental SSG-EG
		int		ssg_offset_;
		int		ssg_vector_;
		int		ssg_phase_;
#endif
		uint	key_scale_rate_;
		EGPhase	eg_phase_;
		uint	ms_;
		
		uint	tl_;
		uint	tl_latch_;
		uint	ar_;
		uint	dr_;
		uint	sr_;
		uint	sl_;
		uint	rr_;
		uint	ks_;
		uint	ssg_type_;

		bool	keyon_;
		bool	amon_;
		bool	param_changed_;
		bool	mute_;
		bool	inverted_;
		bool	held_;
	};

	class Operator
	{
	public:
		Operator();
		void	SetChip(Chip* chip) { chip_ = chip; }

		static void	MakeTimeTable(uint ratio);
		
		ISample	Calc(ISample in);
		ISample	CalcL(ISample in);
		ISample CalcFB(uint fb);
		ISample CalcFBL(uint fb);
		ISample CalcN(uint noise);
		void	Prepare();
		void	KeyOn();
		void	KeyOff();
		void	Reset();
		void	ResetFB();
		int		IsOn();

		void	SetDT(uint dt);
		void	SetDT2(uint dt2);
		void	SetMULTI(uint multi);
		void	SetTL(uint tl, bool csm);
		void	SetKS(uint ks);
		void	SetAR(uint ar);
		void	SetDR(uint dr);
		void	SetSR(uint sr);
		void	SetRR(uint rr);
		void	SetSL(uint sl);
		void	SetSSGEC(uint ssgec);
		void	SetFNum(uint fnum);
		void	SetDPBN(uint dp, uint bn);
		void	SetMode(bool modulator);
		void	SetAMON(bool on);
		void	SetMS(uint ms);
		void	Mute(bool);
		
//		static void SetAML(uint l);
//		static void SetPML(uint l);

		int		Out() { return out_; }

		int		dbgGetIn2() { return in2_; } 
		void	dbgStopPG() { pg_diff_ = 0; pg_diff_lfo_ = 0; }
		
		void	DataSave(struct OperatorData* data);
		void	DataLoad(struct OperatorData* data);
	
	private:
		typedef uint32 Counter;
		
		Chip*	chip_;
		ISample	out_, out2_;
		ISample in2_;

	//	Phase Generator ------------------------------------------------------
		uint32	PGCalc();
		uint32	PGCalcL();

		uint	dp_;		// ΔP
		uint	detune_;		// Detune
		uint	detune2_;	// DT2
		uint	multiple_;	// Multiple
		uint32	pg_count_;	// Phase 現在値
		uint32	pg_diff_;	// Phase 差分値
		int32	pg_diff_lfo_;	// Phase 差分値 >> x

	//	Envelop Generator ---------------------------------------------------
		void	EGCalc();
		void	EGStep();
		void	ShiftPhase(EGPhase nextphase);
		void	SSGShiftPhase(int mode);
		void	SetEGRate(uint);
		void	EGUpdate();
		int		FBCalc(int fb);
		ISample LogToLin(uint a);

		
		OpType	type_;		// OP の種類 (M, N...)
		uint	bn_;		// Block/Note
		int		eg_level_;	// EG の出力値
		int		eg_level_on_next_phase_;	// 次の eg_phase_ に移る値
		int		eg_count_;		// EG の次の変移までの時間
		int		eg_count_diff_;	// eg_count_ の差分
		int		eg_out_;		// EG+TL を合わせた出力値
		int		tl_out_;		// TL 分の出力値
//		int		pm_depth_;		// PM depth
//		int		am_depth_;		// AM depth
		int		eg_rate_;
		int		eg_curve_count_;
#if 0  // libOPNMIDI: experimental SSG-EG
		int		ssg_offset_;
		int		ssg_vector_;
		int		ssg_phase_;
#endif

		uint	key_scale_rate_;		// key scale rate
		EGPhase	eg_phase_;
		uint*	ams_;
		uint	ms_;
		
		uint	tl_;			// Total Level	 (0-127)
		uint	tl_latch_;		// Total Level Latch (for CSM mode)
		uint	ar_;			// Attack Rate   (0-63)
		uint	dr_;			// Decay Rate    (0-63)
		uint	sr_;			// Sustain Rate  (0-63)
		uint	sl_;			// Sustain Level (0-127)
		uint	rr_;			// Release Rate  (0-63)
		uint	ks_;			// Keyscale      (0-3)
		uint	ssg_type_;	// SSG-Type Envelop Control

		bool	keyon_;
		bool	amon_;		// enable Amplitude Modulation
		bool	param_changed_;	// パラメータが更新された
		bool	mute_;
		bool	inverted_;
		bool	held_;
		
	//	Tables ---------------------------------------------------------------
		static Counter rate_table[16];
		static uint32 multable[4][16];

		static const uint8 notetable[128];
		static const int8 dttable[256];
		static const int8 decaytable1[64][8];
		static const int decaytable2[16];
		static const int8 attacktable[64][8];
		static const int ssgenvtable[8][2][3][2];

		static uint	sinetable[1024];
		static int32 cltable[FM_CLENTS];

		static bool tablehasmade;
		static void MakeTable();



	//	friends --------------------------------------------------------------
		friend class Channel4;

	public:
		int		dbgopout_;
		int		dbgpgout_;
		static const int32* dbgGetClTable() { return cltable; }
		static const uint* dbgGetSineTable() { return sinetable; }
	};
	
	//	4-op Channel ---------------------------------------------------------
	struct Channel4Data {
		uint	fb;
		int		buf[4];
		int		algo_;
		struct OperatorData op[4];
	};

	class Channel4
	{
	public:
		Channel4();
		void SetChip(Chip* chip);
		void SetType(OpType type);
		
		ISample Calc();
		ISample CalcL();
		ISample CalcN(uint noise);
		ISample CalcLN(uint noise);
		void SetFNum(uint fnum);
		void SetFB(uint fb);
		void SetKCKF(uint kc, uint kf);
		void SetAlgorithm(uint algo);
		int Prepare();
		void KeyControl(uint key);
		void Reset();
		void SetMS(uint ms);
		void Mute(bool);
		void Refresh();

		void dbgStopPG() { for (int i=0; i<4; i++) op[i].dbgStopPG(); }
		
		void DataSave(struct Channel4Data* data);
		void DataLoad(struct Channel4Data* data);
	
	private:
		static const uint8 fbtable[8];
		uint	fb;
		int		buf[4];
		int*	in[3];			// 各 OP の入力ポインタ
		int*	out[3];			// 各 OP の出力ポインタ
		int*	pms;
		int		algo_;
		Chip*	chip_;

		static void MakeTable();

		static bool tablehasmade;
		static int 	kftable[64];


	public:
		Operator op[4];
	};

	//	Chip resource
	struct ChipData {
		uint	ratio_;
		uint	aml_;
		uint	pml_;
		int		pmv_;
		OpType	optype_;
		uint32	multable_[4][16];
	};

	class Chip
	{
	public:
		Chip();
		void	SetRatio(uint ratio);
		void	SetAML(uint l);
		void	SetPML(uint l);
		void	SetPMV(int pmv) { pmv_ = pmv; }

		uint32	GetMulValue(uint dt2, uint mul) { return multable_[dt2][mul]; }
		uint	GetAML() { return aml_; }
		uint	GetPML() { return pml_; }
		int		GetPMV() { return pmv_; }
		uint	GetRatio() { return ratio_; }

		void	DataSave(struct ChipData* data);
		void	DataLoad(struct ChipData* data);
	
	private:
		void	MakeTable();

		uint	ratio_;
		uint	aml_;
		uint	pml_;
		int		pmv_;
//		OpType	optype_;
		uint32	multable_[4][16];
	};
}

#endif // FM_GEN_H
