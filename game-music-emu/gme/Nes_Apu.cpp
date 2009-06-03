// Nes_Snd_Emu 0.1.8. http://www.slack.net/~ant/

#include "Nes_Apu.h"

/* Copyright (C) 2003-2006 Shay Green. This module is free software; you
can redistribute it and/or modify it under the terms of the GNU Lesser
General Public License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version. This
module is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more
details. You should have received a copy of the GNU Lesser General Public
License along with this module; if not, write to the Free Software Foundation,
Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA */

#include "blargg_source.h"

int const amp_range = 15;

Nes_Apu::Nes_Apu() :
	square1( &square_synth ),
	square2( &square_synth )
{
	tempo_ = 1.0;
	dmc.apu = this;
	dmc.prg_reader = NULL;
	irq_notifier_ = NULL;
	
	oscs [0] = &square1;
	oscs [1] = &square2;
	oscs [2] = &triangle;
	oscs [3] = &noise;
	oscs [4] = &dmc;
	
	output( NULL );
	volume( 1.0 );
	reset( false );
}

void Nes_Apu::treble_eq( const blip_eq_t& eq )
{
	square_synth.treble_eq( eq );
	triangle.synth.treble_eq( eq );
	noise.synth.treble_eq( eq );
	dmc.synth.treble_eq( eq );
}

void Nes_Apu::enable_nonlinear( double v )
{
	dmc.nonlinear = true;
	square_synth.volume( 1.3 * 0.25751258 / 0.742467605 * 0.25 / amp_range * v );
	
	const double tnd = 0.48 / 202 * nonlinear_tnd_gain();
	triangle.synth.volume( 3.0 * tnd );
	noise.synth.volume( 2.0 * tnd );
	dmc.synth.volume( tnd );
	
	square1 .last_amp = 0;
	square2 .last_amp = 0;
	triangle.last_amp = 0;
	noise   .last_amp = 0;
	dmc     .last_amp = 0;
}

void Nes_Apu::volume( double v )
{
	dmc.nonlinear = false;
	square_synth.volume(   0.1128  / amp_range * v );
	triangle.synth.volume( 0.12765 / amp_range * v );
	noise.synth.volume(    0.0741  / amp_range * v );
	dmc.synth.volume(      0.42545 / 127 * v );
}

void Nes_Apu::output( Blip_Buffer* buffer )
{
	for ( int i = 0; i < osc_count; i++ )
		osc_output( i, buffer );
}

void Nes_Apu::set_tempo( double t )
{
	tempo_ = t;
	frame_period = (dmc.pal_mode ? 8314 : 7458);
	if ( t != 1.0 )
		frame_period = (int) (frame_period / t) & ~1; // must be even
}

void Nes_Apu::reset( bool pal_mode, int initial_dmc_dac )
{
	dmc.pal_mode = pal_mode;
	set_tempo( tempo_ );
	
	square1.reset();
	square2.reset();
	triangle.reset();
	noise.reset();
	dmc.reset();
	
	last_time = 0;
	last_dmc_time = 0;
	osc_enables = 0;
	irq_flag = false;
	earliest_irq_ = no_irq;
	frame_delay = 1;
	write_register( 0, 0x4017, 0x00 );
	write_register( 0, 0x4015, 0x00 );
	
	for ( nes_addr_t addr = start_addr; addr <= 0x4013; addr++ )
		write_register( 0, addr, (addr & 3) ? 0x00 : 0x10 );
	
	dmc.dac = initial_dmc_dac;
	if ( !dmc.nonlinear )
		triangle.last_amp = 15;
	if ( !dmc.nonlinear ) // TODO: remove?
		dmc.last_amp = initial_dmc_dac; // prevent output transition
}

void Nes_Apu::irq_changed()
{
	nes_time_t new_irq = dmc.next_irq;
	if ( dmc.irq_flag | irq_flag ) {
		new_irq = 0;
	}
	else if ( new_irq > next_irq ) {
		new_irq = next_irq;
	}
	
	if ( new_irq != earliest_irq_ ) {
		earliest_irq_ = new_irq;
		if ( irq_notifier_ )
			irq_notifier_( irq_data );
	}
}

// frames

void Nes_Apu::run_until( nes_time_t end_time )
{
	require( end_time >= last_dmc_time );
	if ( end_time > next_dmc_read_time() )
	{
		nes_time_t start = last_dmc_time;
		last_dmc_time = end_time;
		dmc.run( start, end_time );
	}
}

void Nes_Apu::run_until_( nes_time_t end_time )
{
	require( end_time >= last_time );
	
	if ( end_time == last_time )
		return;
	
	if ( last_dmc_time < end_time )
	{
		nes_time_t start = last_dmc_time;
		last_dmc_time = end_time;
		dmc.run( start, end_time );
	}
	
	while ( true )
	{
		// earlier of next frame time or end time
		nes_time_t time = last_time + frame_delay;
		if ( time > end_time )
			time = end_time;
		frame_delay -= time - last_time;
		
		// run oscs to present
		square1.run( last_time, time );
		square2.run( last_time, time );
		triangle.run( last_time, time );
		noise.run( last_time, time );
		last_time = time;
		
		if ( time == end_time )
			break; // no more frames to run
		
		// take frame-specific actions
		frame_delay = frame_period;
		switch ( frame++ )
		{
			case 0:
				if ( !(frame_mode & 0xC0) ) {
		 			next_irq = time + frame_period * 4 + 2;
		 			irq_flag = true;
		 		}
		 		// fall through
		 	case 2:
		 		// clock length and sweep on frames 0 and 2
				square1.clock_length( 0x20 );
				square2.clock_length( 0x20 );
				noise.clock_length( 0x20 );
				triangle.clock_length( 0x80 ); // different bit for halt flag on triangle
				
				square1.clock_sweep( -1 );
				square2.clock_sweep( 0 );
				
				// frame 2 is slightly shorter in mode 1
				if ( dmc.pal_mode && frame == 3 )
					frame_delay -= 2;
		 		break;
		 	
			case 1:
				// frame 1 is slightly shorter in mode 0
				if ( !dmc.pal_mode )
					frame_delay -= 2;
				break;
			
		 	case 3:
		 		frame = 0;
		 		
		 		// frame 3 is almost twice as long in mode 1
		 		if ( frame_mode & 0x80 )
					frame_delay += frame_period - (dmc.pal_mode ? 2 : 6);
				break;
		}
		
		// clock envelopes and linear counter every frame
		triangle.clock_linear_counter();
		square1.clock_envelope();
		square2.clock_envelope();
		noise.clock_envelope();
	}
}

template<class T>
inline void zero_apu_osc( T* osc, nes_time_t time )
{
	Blip_Buffer* output = osc->output;
	int last_amp = osc->last_amp;
	osc->last_amp = 0;
	if ( output && last_amp )
		osc->synth.offset( time, -last_amp, output );
}

void Nes_Apu::end_frame( nes_time_t end_time )
{
	if ( end_time > last_time )
		run_until_( end_time );
	
	if ( dmc.nonlinear )
	{
		zero_apu_osc( &square1,  last_time );
		zero_apu_osc( &square2,  last_time );
		zero_apu_osc( &triangle, last_time );
		zero_apu_osc( &noise,    last_time );
		zero_apu_osc( &dmc,      last_time );
	}
	
	// make times relative to new frame
	last_time -= end_time;
	require( last_time >= 0 );
	
	last_dmc_time -= end_time;
	require( last_dmc_time >= 0 );
	
	if ( next_irq != no_irq ) {
		next_irq -= end_time;
		check( next_irq >= 0 );
	}
	if ( dmc.next_irq != no_irq ) {
		dmc.next_irq -= end_time;
		check( dmc.next_irq >= 0 );
	}
	if ( earliest_irq_ != no_irq ) {
		earliest_irq_ -= end_time;
		if ( earliest_irq_ < 0 )
			earliest_irq_ = 0;
	}
}

// registers

static const unsigned char length_table [0x20] = {
	0x0A, 0xFE, 0x14, 0x02, 0x28, 0x04, 0x50, 0x06,
	0xA0, 0x08, 0x3C, 0x0A, 0x0E, 0x0C, 0x1A, 0x0E, 
	0x0C, 0x10, 0x18, 0x12, 0x30, 0x14, 0x60, 0x16,
	0xC0, 0x18, 0x48, 0x1A, 0x10, 0x1C, 0x20, 0x1E
};

void Nes_Apu::write_register( nes_time_t time, nes_addr_t addr, int data )
{
	require( addr > 0x20 ); // addr must be actual address (i.e. 0x40xx)
	require( (unsigned) data <= 0xFF );
	
	// Ignore addresses outside range
	if ( unsigned (addr - start_addr) > end_addr - start_addr )
		return;
	
	run_until_( time );
	
	if ( addr < 0x4014 )
	{
		// Write to channel
		int osc_index = (addr - start_addr) >> 2;
		Nes_Osc* osc = oscs [osc_index];
		
		int reg = addr & 3;
		osc->regs [reg] = data;
		osc->reg_written [reg] = true;
		
		if ( osc_index == 4 )
		{
			// handle DMC specially
			dmc.write_register( reg, data );
		}
		else if ( reg == 3 )
		{
			// load length counter
			if ( (osc_enables >> osc_index) & 1 )
				osc->length_counter = length_table [(data >> 3) & 0x1F];
			
			// reset square phase
			if ( osc_index < 2 )
				((Nes_Square*) osc)->phase = Nes_Square::phase_range - 1;
		}
	}
	else if ( addr == 0x4015 )
	{
		// Channel enables
		for ( int i = osc_count; i--; )
			if ( !((data >> i) & 1) )
				oscs [i]->length_counter = 0;
		
		bool recalc_irq = dmc.irq_flag;
		dmc.irq_flag = false;
		
		int old_enables = osc_enables;
		osc_enables = data;
		if ( !(data & 0x10) ) {
			dmc.next_irq = no_irq;
			recalc_irq = true;
		}
		else if ( !(old_enables & 0x10) ) {
			dmc.start(); // dmc just enabled
		}
		
		if ( recalc_irq )
			irq_changed();
	}
	else if ( addr == 0x4017 )
	{
		// Frame mode
		frame_mode = data;
		
		bool irq_enabled = !(data & 0x40);
		irq_flag &= irq_enabled;
		next_irq = no_irq;
		
		// mode 1
		frame_delay = (frame_delay & 1);
		frame = 0;
		
		if ( !(data & 0x80) )
		{
			// mode 0
			frame = 1;
			frame_delay += frame_period;
			if ( irq_enabled )
				next_irq = time + frame_delay + frame_period * 3 + 1;
		}
		
		irq_changed();
	}
}

int Nes_Apu::read_status( nes_time_t time )
{
	run_until_( time - 1 );
	
	int result = (dmc.irq_flag << 7) | (irq_flag << 6);
	
	for ( int i = 0; i < osc_count; i++ )
		if ( oscs [i]->length_counter )
			result |= 1 << i;
	
	run_until_( time );
	
	if ( irq_flag )
	{
		result |= 0x40;
		irq_flag = false;
		irq_changed();
	}
	
	//dprintf( "%6d/%d Read $4015->$%02X\n", frame_delay, frame, result );
	
	return result;
}
