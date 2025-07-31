// Konami SCC sound chip emulator

// Game_Music_Emu https://bitbucket.org/mpyne/game-music-emu/
#ifndef KSS_SCC_APU_H
#define KSS_SCC_APU_H

#include "blargg_common.h"
#include "Blip_Buffer.h"
#include <string.h>

class Scc_Apu {
public:
	// Set buffer to generate all sound into, or disable sound if NULL
	void output( Blip_Buffer* );
	
	// Reset sound chip
	void reset();
	
	// Write to register at specified time
	enum { reg_count = 0x90 };
	void write( blip_time_t time, int reg, int data );
	
	// Run sound to specified time, end current time frame, then start a new
	// time frame at time 0. Time frames have no effect on emulation and each
	// can be whatever length is convenient.
	void end_frame( blip_time_t length );

// Additional features
	
	// Set sound output of specific oscillator to buffer, where index is
	// 0 to 4. If buffer is NULL, the specified oscillator is muted.
	enum { osc_count = 5 };
	void osc_output( int index, Blip_Buffer* );
	
	// Set overall volume (default is 1.0)
	void volume( double );
	
	// Set treble equalization (see documentation)
	void treble_eq( blip_eq_t const& );
	
public:
	Scc_Apu();
private:
	enum { amp_range = 0x8000 };
	struct osc_t
	{
		int delay;
		int phase;
		int last_amp;
		Blip_Buffer* output;
	};
	osc_t oscs [osc_count];
	blip_time_t last_time;
	unsigned char regs [reg_count];
	Blip_Synth<blip_med_quality,1> synth;
	
	void run_until( blip_time_t );
};

inline void Scc_Apu::volume( double v ) { synth.volume( 0.43 / osc_count / amp_range * v ); }

inline void Scc_Apu::treble_eq( blip_eq_t const& eq ) { synth.treble_eq( eq ); }

inline void Scc_Apu::osc_output( int index, Blip_Buffer* b )
{
	assert( (unsigned) index < osc_count );
	oscs [index].output = b;
}

inline void Scc_Apu::write( blip_time_t time, int addr, int data )
{
	assert( (unsigned) addr < reg_count );
	run_until( time );
	regs [addr] = data;
}

inline void Scc_Apu::end_frame( blip_time_t end_time )
{
	if ( end_time > last_time )
		run_until( end_time );
	last_time -= end_time;
	assert( last_time >= 0 );
}

inline void Scc_Apu::output( Blip_Buffer* buf )
{
	for ( int i = 0; i < osc_count; i++ )
		oscs [i].output = buf;
}

inline Scc_Apu::Scc_Apu()
{
	output( 0 );
}

inline void Scc_Apu::reset()
{
	last_time = 0;
	
	for ( int i = 0; i < osc_count; i++ )
		memset( &oscs [i], 0, offsetof (osc_t,output) );
	
	memset( regs, 0, sizeof regs );
}

#endif
