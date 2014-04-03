// AY-3-8910 sound chip emulator

// Game_Music_Emu 0.6.0
#ifndef AY_APU_H
#define AY_APU_H

#include "blargg_common.h"
#include "Blip_Buffer.h"

class Ay_Apu {
public:
	// Set buffer to generate all sound into, or disable sound if NULL
	void output( Blip_Buffer* );
	
	// Reset sound chip
	void reset();
	
	// Write to register at specified time
	enum { reg_count = 16 };
	void write( blip_time_t time, int addr, int data );
	
	// Run sound to specified time, end current time frame, then start a new
	// time frame at time 0. Time frames have no effect on emulation and each
	// can be whatever length is convenient.
	void end_frame( blip_time_t length );
	
// Additional features
	
	// Set sound output of specific oscillator to buffer, where index is
	// 0, 1, or 2. If buffer is NULL, the specified oscillator is muted.
	enum { osc_count = 3 };
	void osc_output( int index, Blip_Buffer* );
	
	// Set overall volume (default is 1.0)
	void volume( double );
	
	// Set treble equalization (see documentation)
	void treble_eq( blip_eq_t const& );
	
public:
	Ay_Apu();
	typedef unsigned char byte;
private:
	struct osc_t
	{
		blip_time_t period;
		blip_time_t delay;
		short last_amp;
		short phase;
		Blip_Buffer* output;
	} oscs [osc_count];
	blip_time_t last_time;
	byte regs [reg_count];
	
	struct {
		blip_time_t delay;
		blargg_ulong lfsr;
	} noise;
	
	struct {
		blip_time_t delay;
		byte const* wave;
		int pos;
		byte modes [8] [48]; // values already passed through volume table
	} env;
	
	void run_until( blip_time_t );
	void write_data_( int addr, int data );
public:
	enum { amp_range = 255 };
	Blip_Synth<blip_good_quality,1> synth_;
};

inline void Ay_Apu::volume( double v ) { synth_.volume( 0.7 / osc_count / amp_range * v ); }

inline void Ay_Apu::treble_eq( blip_eq_t const& eq ) { synth_.treble_eq( eq ); }

inline void Ay_Apu::write( blip_time_t time, int addr, int data )
{
	run_until( time );
	write_data_( addr, data );
}

inline void Ay_Apu::osc_output( int i, Blip_Buffer* buf )
{
	assert( (unsigned) i < osc_count );
	oscs [i].output = buf;
}

inline void Ay_Apu::output( Blip_Buffer* buf )
{
	osc_output( 0, buf );
	osc_output( 1, buf );
	osc_output( 2, buf );
}

inline void Ay_Apu::end_frame( blip_time_t time )
{
	if ( time > last_time )
		run_until( time );
	
	assert( last_time >= time );
	last_time -= time;
}

#endif
