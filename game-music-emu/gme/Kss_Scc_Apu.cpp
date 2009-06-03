// Game_Music_Emu 0.5.2. http://www.slack.net/~ant/

#include "Kss_Scc_Apu.h"

/* Copyright (C) 2006 Shay Green. This module is free software; you
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

// Tones above this frequency are treated as disabled tone at half volume.
// Power of two is more efficient (avoids division).
unsigned const inaudible_freq = 16384;

int const wave_size = 0x20;

void Scc_Apu::run_until( blip_time_t end_time )
{
	for ( int index = 0; index < osc_count; index++ )
	{
		osc_t& osc = oscs [index];
		
		Blip_Buffer* const output = osc.output;
		if ( !output )
			continue;
		output->set_modified();
		
		blip_time_t period = (regs [0x80 + index * 2 + 1] & 0x0F) * 0x100 +
				regs [0x80 + index * 2] + 1;
		int volume = 0;
		if ( regs [0x8F] & (1 << index) )
		{
			blip_time_t inaudible_period = (blargg_ulong) (output->clock_rate() +
					inaudible_freq * 32) / (inaudible_freq * 16);
			if ( period > inaudible_period )
				volume = (regs [0x8A + index] & 0x0F) * (amp_range / 256 / 15);
		}
		
		BOOST::int8_t const* wave = (BOOST::int8_t*) regs + index * wave_size;
		if ( index == osc_count - 1 )
			wave -= wave_size; // last two oscs share wave
		{
			int amp = wave [osc.phase] * volume;
			int delta = amp - osc.last_amp;
			if ( delta )
			{
				osc.last_amp = amp;
				synth.offset( last_time, delta, output );
			}
		}
		
		blip_time_t time = last_time + osc.delay;
		if ( time < end_time )
		{
			if ( !volume )
			{
				// maintain phase
				blargg_long count = (end_time - time + period - 1) / period;
				osc.phase = (osc.phase + count) & (wave_size - 1);
				time += count * period;
			}
			else
			{
				
				int phase = osc.phase;
				int last_wave = wave [phase];
				phase = (phase + 1) & (wave_size - 1); // pre-advance for optimal inner loop
				
				do
				{
					int amp = wave [phase];
					phase = (phase + 1) & (wave_size - 1);
					int delta = amp - last_wave;
					if ( delta )
					{
						last_wave = amp;
						synth.offset( time, delta * volume, output );
					}
					time += period;
				}
				while ( time < end_time );
				
				osc.phase = phase = (phase - 1) & (wave_size - 1); // undo pre-advance
				osc.last_amp = wave [phase] * volume;
			}
		}
		osc.delay = time - end_time;
	}
	last_time = end_time;
}
