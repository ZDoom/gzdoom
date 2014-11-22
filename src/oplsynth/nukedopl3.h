/*
*  Copyright (C) 2013-2014 Nuke.YKT
*
*  This library is free software; you can redistribute it and/or
*  modify it under the terms of the GNU Lesser General Public
*  License as published by the Free Software Foundation; either
*  version 2.1 of the License, or (at your option) any later version.
*
*  This library is distributed in the hope that it will be useful,
*  but WITHOUT ANY WARRANTY; without even the implied warranty of
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
*  Lesser General Public License for more details.
*
*  You should have received a copy of the GNU Lesser General Public
*  License along with this library; if not, write to the Free Software
*  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

/*
	Nuked Yamaha YMF262(aka OPL3) emulator.
	Thanks:
		MAME Development Team(Jarek Burczynski, Tatsuyuki Satoh):
			Feedback and Rhythm part calculation information.
		forums.submarine.org.uk(carbon14, opl3):
			Tremolo and phase generator calculation information.
		OPLx decapsulated(Matthew Gambrell and Olli Niemitalo):
			OPL2 ROMs.
*/

//version 1.4.2

#include "opl.h"
#include "muslib.h"

typedef uintptr_t	Bitu;
typedef intptr_t	Bits;
typedef DWORD		Bit32u;
typedef SDWORD		Bit32s;
typedef WORD		Bit16u;
typedef SWORD		Bit16s;
typedef BYTE		Bit8u;
typedef SBYTE		Bit8s;

struct channel {
	Bit8u con;
	Bit8u chtype;
	Bit8u alg;
	Bit16u offset;
	Bit8u feedback;
	Bit16u cha, chb, chc, chd;
	Bit16s out;
	Bit16u f_number;
	Bit8u block;
	Bit8u ksv;
	float panl;
	float panr;
};

struct slot {
	Bit32u PG_pos;
	Bit32u PG_inc;
	Bit16s EG_out;
	Bit8u eg_inc;
	Bit8u eg_gen;
	Bit8u eg_gennext;
	Bit16u EG_mout;
	Bit8u EG_ksl;
	Bit8u EG_ar;
	Bit8u EG_dr;
	Bit8u EG_sl;
	Bit8u EG_rr;
	Bit8u EG_state;
	Bit8u EG_type;
	Bit16s out;
	Bit16s *mod;
	Bit16s prevout[2];
	Bit16s fbmod;
	Bit16u offset;
	Bit8u mult;
	Bit8u vibrato;
	Bit8u tremolo;
	Bit8u ksr;
	Bit8u EG_tl;
	Bit8u ksl;
	Bit8u key;
	Bit8u waveform;
};


struct chip {
	Bit8u opl_memory[0x200];
	Bit8u newm;
	Bit8u nts;
	Bit8u rhythm;
	Bit8u dvb;
	Bit8u dam;
	Bit32u noise;
	Bit16u vib_pos;
	Bit16u timer;
	Bit8u trem_inc;
	Bit8u trem_tval;
	Bit8u trem_dir;
	Bit8u trem_val;
	channel Channels[18];
	slot OPs[36];
	Bit16s zm;
};

class NukedOPL3 : public OPLEmul {
private:
	chip opl3;
	bool FullPan;
public:
	void Reset();
	void Update(float* sndptr, int numsamples);
	void WriteReg(int reg, int v);
	void SetPanning(int c, float left, float right);

	NukedOPL3(bool stereo);
};
