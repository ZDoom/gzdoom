// Turbo Grafx 16 (PC Engine) PSG sound chip emulator

// Game_Music_Emu 0.5.2
#ifndef HES_APU_H
#define HES_APU_H

#include "blargg_common.h"
#include "Blip_Buffer.h"

struct Hes_Osc
{
	unsigned char wave [32];
	short volume [2];
	int last_amp [2];
	int delay;
	int period;
	unsigned char noise;
	unsigned char phase;
	unsigned char balance;
	unsigned char dac;
	blip_time_t last_time;
	
	Blip_Buffer* outputs [2];
	Blip_Buffer* chans [3];
	unsigned noise_lfsr;
	unsigned char control;
	
	enum { amp_range = 0x8000 };
	typedef Blip_Synth<blip_med_quality,1> synth_t;
	
	void run_until( synth_t& synth, blip_time_t );
};

class Hes_Apu {
public:
	void treble_eq( blip_eq_t const& );
	void volume( double );
	
	enum { osc_count = 6 };
	void osc_output( int index, Blip_Buffer* center, Blip_Buffer* left, Blip_Buffer* right );
	
	void reset();
	
	enum { start_addr = 0x0800 };
	enum { end_addr   = 0x0809 };
	void write_data( blip_time_t, int addr, int data );
	
	void end_frame( blip_time_t );
	
public:
	Hes_Apu();
private:
	Hes_Osc oscs [osc_count];
	int latch;
	int balance;
	Hes_Osc::synth_t synth;
	
	void balance_changed( Hes_Osc& );
	void recalc_chans();
};

inline void Hes_Apu::volume( double v ) { synth.volume( 1.8 / osc_count / Hes_Osc::amp_range * v ); }

inline void Hes_Apu::treble_eq( blip_eq_t const& eq ) { synth.treble_eq( eq ); }

#endif
