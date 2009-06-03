// Game_Music_Emu 0.5.2. http://www.slack.net/~ant/

#include "Dual_Resampler.h"

#include <stdlib.h>
#include <string.h>

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

unsigned const resampler_extra = 256;

Dual_Resampler::Dual_Resampler() { }

Dual_Resampler::~Dual_Resampler() { }

blargg_err_t Dual_Resampler::reset( int pairs )
{
	// expand allocations a bit
	RETURN_ERR( sample_buf.resize( (pairs + (pairs >> 2)) * 2 ) );
	resize( pairs );
	resampler_size = oversamples_per_frame + (oversamples_per_frame >> 2);
	return resampler.buffer_size( resampler_size );
}

void Dual_Resampler::resize( int pairs )
{
	int new_sample_buf_size = pairs * 2;
	if ( sample_buf_size != new_sample_buf_size )
	{
		if ( (unsigned) new_sample_buf_size > sample_buf.size() )
		{
			check( false );
			return;
		}
		sample_buf_size = new_sample_buf_size;
		oversamples_per_frame = int (pairs * resampler.ratio()) * 2 + 2;
		clear();
	}
}

void Dual_Resampler::play_frame_( Blip_Buffer& blip_buf, dsample_t* out )
{
	long pair_count = sample_buf_size >> 1;
	blip_time_t blip_time = blip_buf.count_clocks( pair_count );
	int sample_count = oversamples_per_frame - resampler.written();
	
	int new_count = play_frame( blip_time, sample_count, resampler.buffer() );
	assert( new_count < resampler_size );
	
	blip_buf.end_frame( blip_time );
	assert( blip_buf.samples_avail() == pair_count );
	
	resampler.write( new_count );
	
	long count = resampler.read( sample_buf.begin(), sample_buf_size );
	assert( count == (long) sample_buf_size );
	
	mix_samples( blip_buf, out );
	blip_buf.remove_samples( pair_count );
}

void Dual_Resampler::dual_play( long count, dsample_t* out, Blip_Buffer& blip_buf )
{
	// empty extra buffer
	long remain = sample_buf_size - buf_pos;
	if ( remain )
	{
		if ( remain > count )
			remain = count;
		count -= remain;
		memcpy( out, &sample_buf [buf_pos], remain * sizeof *out );
		out += remain;
		buf_pos += remain;
	}
	
	// entire frames
	while ( count >= (long) sample_buf_size )
	{
		play_frame_( blip_buf, out );
		out += sample_buf_size;
		count -= sample_buf_size;
	}
	
	// extra
	if ( count )
	{
		play_frame_( blip_buf, sample_buf.begin() );
		buf_pos = count;
		memcpy( out, sample_buf.begin(), count * sizeof *out );
		out += count;
	}
}

void Dual_Resampler::mix_samples( Blip_Buffer& blip_buf, dsample_t* out )
{
	Blip_Reader sn;
	int bass = sn.begin( blip_buf );
	const dsample_t* in = sample_buf.begin();
	
	for ( int n = sample_buf_size >> 1; n--; )
	{
		int s = sn.read();
		blargg_long l = (blargg_long) in [0] * 2 + s;
		if ( (BOOST::int16_t) l != l )
			l = 0x7FFF - (l >> 24);
		
		sn.next( bass );
		blargg_long r = (blargg_long) in [1] * 2 + s;
		if ( (BOOST::int16_t) r != r )
			r = 0x7FFF - (r >> 24);
		
		in += 2;
		out [0] = l;
		out [1] = r;
		out += 2;
	}
	
	sn.end( blip_buf );
}

