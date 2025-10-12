// ---------------------------------------------------------------------------
//	PSG Sound Implementation
//	Copyright (C) cisc 1997, 1999.
// ---------------------------------------------------------------------------
//	$Id: psg.cpp,v 1.10 2002/05/15 21:38:01 cisc Exp $

#include "fmgen_headers.h"
#include "fmgen_misc.h"
#include "fmgen_psg.h"

// ---------------------------------------------------------------------------
//	コンストラクタ・デストラクタ
//
PSG::PSG()
{
	SetVolume(0);
	MakeNoiseTable();
	Reset();
	mask = 0x3f;
}

PSG::~PSG()
{

}

// ---------------------------------------------------------------------------
//	PSG を初期化する(RESET) 
//
void PSG::Reset()
{
	for (int i=0; i<14; i++)
		SetReg(i, 0);
	SetReg(7, 0xff);
	SetReg(14, 0xff);
	SetReg(15, 0xff);
}

// ---------------------------------------------------------------------------
//	クロック周波数の設定
//
void PSG::SetClock(int clock, int rate)
{
	tperiodbase = int((1 << toneshift ) / 4.0 * clock / rate);
	eperiodbase = int((1 << envshift  ) / 4.0 * clock / rate);
	nperiodbase = int((1 << noiseshift) / 4.0 * clock / rate);
	
	// 各データの更新
	int tmp;
	tmp = ((reg[0] + reg[1] * 256) & 0xfff);
	speriod[0] = tmp ? tperiodbase / tmp : tperiodbase;
	tmp = ((reg[2] + reg[3] * 256) & 0xfff);
	speriod[1] = tmp ? tperiodbase / tmp : tperiodbase;
	tmp = ((reg[4] + reg[5] * 256) & 0xfff);
	speriod[2] = tmp ? tperiodbase / tmp : tperiodbase;
	tmp = reg[6] & 0x1f;
	nperiod = tmp ? nperiodbase / tmp / 2 : nperiodbase / 2;
	tmp = ((reg[11] + reg[12] * 256) & 0xffff);
	eperiod = tmp ? eperiodbase / tmp : eperiodbase * 2;
}

// ---------------------------------------------------------------------------
//	ノイズテーブルを作成する
//
void PSG::MakeNoiseTable()
{
	if (!noisetable[0])
	{
		int noise = 14321;
		for (int i=0; i<noisetablesize; i++)
		{
			int n = 0;
			for (int j=0; j<32; j++)
			{
				n = n * 2 + (noise & 1);
				noise = (noise >> 1) | (((noise << 14) ^ (noise << 16)) & 0x10000);
			}
			noisetable[i] = n;
		}
	}
}

// ---------------------------------------------------------------------------
//	出力テーブルを作成
//	素直にテーブルで持ったほうが省スペース。
//
void PSG::SetVolume(int volume)
{
	double base = 0x4000 / 3.0 * pow(10.0, volume / 40.0);
	for (int i=31; i>=2; i--)
	{
		EmitTable[i] = int(base);
		base /= 1.189207115;
	}
	EmitTable[1] = 0;
	EmitTable[0] = 0;
	MakeEnvelopTable();

	SetChannelMask(~mask);
}

void PSG::SetChannelMask(int c)
{ 
	mask = ~c;
	for (int i=0; i<3; i++)
		olevel[i] = mask & (1 << i) ? EmitTable[(reg[8+i] & 15) * 2 + 1] : 0;
}

// ---------------------------------------------------------------------------
//	エンベロープ波形テーブル
//
void PSG::MakeEnvelopTable()
{
	// 0 lo  1 up 2 down 3 hi
	static uint8 table1[16*2] =
	{
		2,0, 2,0, 2,0, 2,0, 1,0, 1,0, 1,0, 1,0,
		2,2, 2,0, 2,1, 2,3, 1,1, 1,3, 1,2, 1,0,
	};
	static uint8 table2[4] = {  0,  0, 31, 31 };
	static int8 table3[4] = {  0,  1, -1,  0 };

	uint* ptr = enveloptable[0];

	for (int i=0; i<16*2; i++)
	{
		uint8 v = table2[table1[i]];
		
		for (int j=0; j<32; j++)
		{
			*ptr++ = EmitTable[v];
			v += table3[table1[i]];
		}
	}
}

// ---------------------------------------------------------------------------
//	PSG のレジスタに値をセットする
//	regnum		レジスタの番号 (0 - 15)
//	data		セットする値
//
void PSG::SetReg(uint regnum, uint8 data)
{
	if (regnum < 0x10)
	{
		reg[regnum] = data;
		switch (regnum)
		{
			int tmp;

		case 0:		// ChA Fine Tune
		case 1:		// ChA Coarse Tune
			tmp = ((reg[0] + reg[1] * 256) & 0xfff);
			speriod[0] = tmp ? tperiodbase / tmp : tperiodbase;
			break;
		
		case 2:		// ChB Fine Tune
		case 3:		// ChB Coarse Tune
			tmp = ((reg[2] + reg[3] * 256) & 0xfff);
			speriod[1] = tmp ? tperiodbase / tmp : tperiodbase;	  
			break;
		
		case 4:		// ChC Fine Tune
		case 5:		// ChC Coarse Tune
			tmp = ((reg[4] + reg[5] * 256) & 0xfff);
			speriod[2] = tmp ? tperiodbase / tmp : tperiodbase;	  
			break;

		case 6:		// Noise generator control
			data &= 0x1f;
			nperiod = data ? nperiodbase / data : nperiodbase;
			break;

		case 8:
			olevel[0] = mask & 1 ? EmitTable[(data & 15) * 2 + 1] : 0;
			break;

		case 9:
			olevel[1] = mask & 2 ? EmitTable[(data & 15) * 2 + 1] : 0;
			break;
		
		case 10:
			olevel[2] = mask & 4 ? EmitTable[(data & 15) * 2 + 1] : 0;
			break;

		case 11:	// Envelop period
		case 12:
			tmp = ((reg[11] + reg[12] * 256) & 0xffff);
			eperiod = tmp ? eperiodbase / tmp : eperiodbase * 2;
			break;

		case 13:	// Envelop shape
			ecount = 0;
			envelop = enveloptable[data & 15];
			break;
		}
	}
}

// ---------------------------------------------------------------------------
void PSG::DataSave(struct PSGData* data) {
	memcpy(data->reg, reg, 16);
	memcpy(data->olevel, olevel, sizeof(uint) * 6);
	memcpy(data->scount, scount, sizeof(uint32) * 3);
	memcpy(data->speriod, speriod, sizeof(uint32) * 3);
	data->ecount = ecount;
	data->eperiod = eperiod;
	data->ncount = ncount;
	data->nperiod = nperiod;
	data->tperiodbase = tperiodbase;
	data->eperiodbase = eperiodbase;
	data->nperiodbase = nperiodbase;
	data->volume = volume;
	data->mask = mask;
}

// ---------------------------------------------------------------------------
void PSG::DataLoad(struct PSGData* data) {
	memcpy(reg, data->reg, 16);
	memcpy(olevel, data->olevel, sizeof(uint) * 6);
	memcpy(scount, data->scount, sizeof(uint32) * 3);
	memcpy(speriod, data->speriod, sizeof(uint32) * 3);
	ecount = data->ecount;
	eperiod = data->eperiod;
	ncount = data->ncount;
	nperiod = data->nperiod;
	tperiodbase = data->tperiodbase;
	eperiodbase = data->eperiodbase;
	nperiodbase = data->nperiodbase;
	volume = data->volume;
	mask = data->mask;
}

// ---------------------------------------------------------------------------
//
//
inline void PSG::StoreSample(Sample& dest, int32 data)
{
	if (sizeof(Sample) == 2)
		dest = (Sample) Limit(dest + data, 0x7fff, -0x8000);
	else
		dest += data;
}

// ---------------------------------------------------------------------------
//	PCM データを吐き出す(2ch)
//	dest		PCM データを展開するポインタ
//	nsamples	展開する PCM のサンプル数
//
void PSG::Mix(Sample* dest, int nsamples)
{
	uint8 chenable[3], nenable[3];
	uint8 r7 = ~reg[7];

	if ((r7 & 0x3f) | ((reg[8] | reg[9] | reg[10]) & 0x1f))
	{
		chenable[0] = (r7 & 0x01) && (speriod[0] <= (1 << toneshift));
		chenable[1] = (r7 & 0x02) && (speriod[1] <= (1 << toneshift));
		chenable[2] = (r7 & 0x04) && (speriod[2] <= (1 << toneshift));
		nenable[0]  = (r7 >> 3) & 1;
		nenable[1]  = (r7 >> 4) & 1;
		nenable[2]  = (r7 >> 5) & 1;
		
		int noise, sample;
		uint env;
		uint* p1 = ((mask & 1) && (reg[ 8] & 0x10)) ? &env : &olevel[0];
		uint* p2 = ((mask & 2) && (reg[ 9] & 0x10)) ? &env : &olevel[1];
		uint* p3 = ((mask & 4) && (reg[10] & 0x10)) ? &env : &olevel[2];
		
		#define SCOUNT(ch)	(scount[ch] >> (toneshift+oversampling))
		
		if (p1 != &env && p2 != &env && p3 != &env)
		{
			// エンベロープ無し
			if ((r7 & 0x38) == 0)
			{
				// ノイズ無し
				for (int i=0; i<nsamples; i++)
				{
					sample = 0;
					for (int j=0; j < (1 << oversampling); j++)
					{
						int x, y, z;
						x = (SCOUNT(0) & chenable[0]) - 1;
						sample += (olevel[0] + x) ^ x;
						scount[0] += speriod[0];
						y = (SCOUNT(1) & chenable[1]) - 1;
						sample += (olevel[1] + y) ^ y;
						scount[1] += speriod[1];
						z = (SCOUNT(2) & chenable[2]) - 1;
						sample += (olevel[2] + z) ^ z;
						scount[2] += speriod[2];
					}
					sample /= (1 << oversampling);
					StoreSample(dest[0], sample);
					StoreSample(dest[1], sample);
					dest += 2;
				}
			}
			else
			{
				// ノイズ有り
				for (int i=0; i<nsamples; i++)
				{
					sample = 0;
					for (int j=0; j < (1 << oversampling); j++)
					{
#ifdef _M_IX86
						noise = noisetable[(ncount >> (noiseshift+oversampling+6)) & (noisetablesize-1)] 
							>> (ncount >> (noiseshift+oversampling+1));
#else
						noise = noisetable[(ncount >> (noiseshift+oversampling+6)) & (noisetablesize-1)] 
							>> (ncount >> (noiseshift+oversampling+1) & 31);
#endif
						ncount += nperiod;

						int x, y, z;
						x = ((SCOUNT(0) & chenable[0]) | (nenable[0] & noise)) - 1;		// 0 or -1
						sample += (olevel[0] + x) ^ x;
						scount[0] += speriod[0];
						y = ((SCOUNT(1) & chenable[1]) | (nenable[1] & noise)) - 1;
						sample += (olevel[1] + y) ^ y;
						scount[1] += speriod[1];
						z = ((SCOUNT(2) & chenable[2]) | (nenable[2] & noise)) - 1;
						sample += (olevel[2] + z) ^ z;
						scount[2] += speriod[2];
					}
					sample /= (1 << oversampling);
					StoreSample(dest[0], sample);
					StoreSample(dest[1], sample);
					dest += 2;
				}
			}

			// エンベロープの計算をさぼった帳尻あわせ
			ecount = (ecount >> 8) + (eperiod >> (8-oversampling)) * nsamples;
			if (ecount >= (1 << (envshift+6+oversampling-8)))
			{
				if ((reg[0x0d] & 0x0b) != 0x0a)
					ecount |= (1 << (envshift+5+oversampling-8));
				ecount &= (1 << (envshift+6+oversampling-8)) - 1;
			}
			ecount <<= 8;
		}
		else
		{
			// エンベロープあり
			for (int i=0; i<nsamples; i++)
			{
				sample = 0;
				for (int j=0; j < (1 << oversampling); j++)
				{
					env = envelop[ecount >> (envshift+oversampling)];
					ecount += eperiod;
					if (ecount >= (1 << (envshift+6+oversampling)))
					{
						if ((reg[0x0d] & 0x0b) != 0x0a)
							ecount |= (1 << (envshift+5+oversampling));
						ecount &= (1 << (envshift+6+oversampling)) - 1;
					}
#ifdef _M_IX86
					noise = noisetable[(ncount >> (noiseshift+oversampling+6)) & (noisetablesize-1)] 
						>> (ncount >> (noiseshift+oversampling+1));
#else
					noise = noisetable[(ncount >> (noiseshift+oversampling+6)) & (noisetablesize-1)] 
						>> (ncount >> (noiseshift+oversampling+1) & 31);
#endif
					ncount += nperiod;

					int x, y, z;
					x = ((SCOUNT(0) & chenable[0]) | (nenable[0] & noise)) - 1;		// 0 or -1
					sample += (*p1 + x) ^ x;
					scount[0] += speriod[0];
					y = ((SCOUNT(1) & chenable[1]) | (nenable[1] & noise)) - 1;
					sample += (*p2 + y) ^ y;
					scount[1] += speriod[1];
					z = ((SCOUNT(2) & chenable[2]) | (nenable[2] & noise)) - 1;
					sample += (*p3 + z) ^ z;
					scount[2] += speriod[2];
				}
				sample /= (1 << oversampling);
				StoreSample(dest[0], sample);
				StoreSample(dest[1], sample);
				dest += 2;
			}
		}
	}
}

// ---------------------------------------------------------------------------
//	テーブル
//
uint	PSG::noisetable[noisetablesize] = { 0, };
int		PSG::EmitTable[0x20] = { -1, };
uint	PSG::enveloptable[16][64] = { {0, } };
