// Atari POKEY sound chip emulator

// Game_Music_Emu 0.5.2
#ifndef SAP_APU_H
#define SAP_APU_H

#include "blargg_common.h"
#include "Blip_Buffer.h"

class Sap_Apu_Impl;

class Sap_Apu {
public:
	enum { osc_count = 4 };
	void osc_output( int index, Blip_Buffer* );
	
	void reset( Sap_Apu_Impl* );
	
	enum { start_addr = 0xD200 };
	enum { end_addr   = 0xD209 };
	void write_data( blip_time_t, unsigned addr, int data );
	
	void end_frame( blip_time_t );
	
public:
	Sap_Apu();
private:
	struct osc_t
	{
		unsigned char regs [2];
		unsigned char phase;
		unsigned char invert;
		int last_amp;
		blip_time_t delay;
		blip_time_t period; // always recalculated before use; here for convenience
		Blip_Buffer* output;
	};
	osc_t oscs [osc_count];
	Sap_Apu_Impl* impl;
	blip_time_t last_time;
	int poly5_pos;
	int poly4_pos;
	int polym_pos;
	int control;
	
	void calc_periods();
	void run_until( blip_time_t );
	
	enum { poly4_len  = (1L <<  4) - 1 };
	enum { poly9_len  = (1L <<  9) - 1 };
	enum { poly17_len = (1L << 17) - 1 };
	friend class Sap_Apu_Impl;
};

// Common tables and Blip_Synth that can be shared among multiple Sap_Apu objects
class Sap_Apu_Impl {
public:
	Blip_Synth<blip_good_quality,1> synth;
	
	Sap_Apu_Impl();
	void volume( double d ) { synth.volume( 1.0 / Sap_Apu::osc_count / 30 * d ); }
	
private:
	typedef unsigned char byte;
	byte poly4  [Sap_Apu::poly4_len  / 8 + 1];
	byte poly9  [Sap_Apu::poly9_len  / 8 + 1];
	byte poly17 [Sap_Apu::poly17_len / 8 + 1];
	friend class Sap_Apu;
};

inline void Sap_Apu::osc_output( int i, Blip_Buffer* b )
{
	assert( (unsigned) i < osc_count );
	oscs [i].output = b;
}

#endif
