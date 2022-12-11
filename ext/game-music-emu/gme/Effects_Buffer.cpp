// Game_Music_Emu 0.6.0. http://www.slack.net/~ant/

#include "Effects_Buffer.h"

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

#ifdef BLARGG_ENABLE_OPTIMIZER
	#include BLARGG_ENABLE_OPTIMIZER
#endif

typedef blargg_long fixed_t;

#define TO_FIXED( f )   fixed_t ((f) * (1L << 15) + 0.5)
#define FMUL( x, y )    (((x) * (y)) >> 15)

const unsigned echo_size = 4096;
const unsigned echo_mask = echo_size - 1;
BOOST_STATIC_ASSERT( (echo_size & echo_mask) == 0 ); // must be power of 2

const unsigned reverb_size = 8192 * 2;
const unsigned reverb_mask = reverb_size - 1;
BOOST_STATIC_ASSERT( (reverb_size & reverb_mask) == 0 ); // must be power of 2

Effects_Buffer::config_t::config_t()
{
	pan_1           = -0.15f;
	pan_2           =  0.15f;
	reverb_delay    = 88.0f;
	reverb_level    = 0.12f;
	echo_delay      = 61.0f;
	echo_level      = 0.10f;
	delay_variance  = 18.0f;
	effects_enabled = false;
}

void Effects_Buffer::set_depth( double d )
{
	float f = (float) d;
	config_t c;
	c.pan_1             = -0.6f * f;
	c.pan_2             =  0.6f * f;
	c.reverb_delay      = 880 * 0.1f;
	c.echo_delay        = 610 * 0.1f;
	if ( f > 0.5 )
		f = 0.5; // TODO: more linear reduction of extreme reverb/echo
	c.reverb_level      = 0.5f * f;
	c.echo_level        = 0.30f * f;
	c.delay_variance    = 180 * 0.1f;
	c.effects_enabled   = (d > 0.0f);
	config( c );
}

Effects_Buffer::Effects_Buffer( bool center_only ) : Multi_Buffer( 2 )
{
	buf_count = center_only ? max_buf_count - 4 : max_buf_count;
	
	echo_pos = 0;
	reverb_pos = 0;
	
	stereo_remain = 0;
	effect_remain = 0;
	effects_enabled = false;
	set_depth( 0 );
}

Effects_Buffer::~Effects_Buffer() { }

blargg_err_t Effects_Buffer::set_sample_rate( long rate, int msec )
{
	if ( !echo_buf.size() )
		RETURN_ERR( echo_buf.resize( echo_size ) );
	
	if ( !reverb_buf.size() )
		RETURN_ERR( reverb_buf.resize( reverb_size ) );
	
	for ( int i = 0; i < buf_count; i++ )
		RETURN_ERR( bufs [i].set_sample_rate( rate, msec ) );
	
	config( config_ );
	clear();
	
	return Multi_Buffer::set_sample_rate( bufs [0].sample_rate(), bufs [0].length() );
}

void Effects_Buffer::clock_rate( long rate )
{
	for ( int i = 0; i < buf_count; i++ )
		bufs [i].clock_rate( rate );
}

void Effects_Buffer::bass_freq( int freq )
{
	for ( int i = 0; i < buf_count; i++ )
		bufs [i].bass_freq( freq );
}

void Effects_Buffer::clear()
{
	stereo_remain = 0;
	effect_remain = 0;
	if ( echo_buf.size() )
		memset( &echo_buf [0], 0, echo_size * sizeof echo_buf [0] );
	
	if ( reverb_buf.size() )
		memset( &reverb_buf [0], 0, reverb_size * sizeof reverb_buf [0] );
	
	for ( int i = 0; i < buf_count; i++ )
		bufs [i].clear();
}

inline int pin_range( int n, int max, int min = 0 )
{
	if ( n < min )
		return min;
	if ( n > max )
		return max;
	return n;
}

void Effects_Buffer::config( const config_t& cfg )
{
	channels_changed();
	
	// clear echo and reverb buffers
	if ( !config_.effects_enabled && cfg.effects_enabled && echo_buf.size() )
	{
		memset( &echo_buf [0], 0, echo_size * sizeof echo_buf [0] );
		memset( &reverb_buf [0], 0, reverb_size * sizeof reverb_buf [0] );
	}
	
	config_ = cfg;
	
	if ( config_.effects_enabled )
	{
		// convert to internal format
		
		chans.pan_1_levels [0] = TO_FIXED( 1 ) - TO_FIXED( config_.pan_1 );
		chans.pan_1_levels [1] = TO_FIXED( 2 ) - chans.pan_1_levels [0];
		
		chans.pan_2_levels [0] = TO_FIXED( 1 ) - TO_FIXED( config_.pan_2 );
		chans.pan_2_levels [1] = TO_FIXED( 2 ) - chans.pan_2_levels [0];
		
		chans.reverb_level = TO_FIXED( config_.reverb_level );
		chans.echo_level = TO_FIXED( config_.echo_level );
		
		int delay_offset = int (1.0 / 2000 * config_.delay_variance * sample_rate());
		
		int reverb_sample_delay = int (1.0 / 1000 * config_.reverb_delay * sample_rate());
		chans.reverb_delay_l = pin_range( reverb_size -
				(reverb_sample_delay - delay_offset) * 2, reverb_size - 2, 0 );
		chans.reverb_delay_r = pin_range( reverb_size + 1 -
				(reverb_sample_delay + delay_offset) * 2, reverb_size - 1, 1 );
		
		int echo_sample_delay = int (1.0 / 1000 * config_.echo_delay * sample_rate());
		chans.echo_delay_l = pin_range( echo_size - 1 - (echo_sample_delay - delay_offset),
				echo_size - 1 );
		chans.echo_delay_r = pin_range( echo_size - 1 - (echo_sample_delay + delay_offset),
				echo_size - 1 );
		
		chan_types [0].center = &bufs [0];
		chan_types [0].left   = &bufs [3];
		chan_types [0].right  = &bufs [4];
		
		chan_types [1].center = &bufs [1];
		chan_types [1].left   = &bufs [3];
		chan_types [1].right  = &bufs [4];
		
		chan_types [2].center = &bufs [2];
		chan_types [2].left   = &bufs [5];
		chan_types [2].right  = &bufs [6];
		assert( 2 < chan_types_count );
	}
	else
	{
		// set up outputs
		for ( unsigned i = 0; i < chan_types_count; i++ )
		{
			channel_t& c = chan_types [i];
			c.center = &bufs [0];
			c.left   = &bufs [1];
			c.right  = &bufs [2];
		}
	}
	
	if ( buf_count < max_buf_count )
	{
		for ( int i = 0; i < chan_types_count; i++ )
		{
			channel_t& c = chan_types [i];
			c.left   = c.center;
			c.right  = c.center;
		}
	}
}

Effects_Buffer::channel_t Effects_Buffer::channel( int i, int type )
{
	int out = 2;
	if ( !type )
	{
		out = i % 5;
		if ( out > 2 )
			out = 2;
	}
	else if ( !(type & noise_type) && (type & type_index_mask) % 3 != 0 )
	{
		out = type & 1;
	}
	return chan_types [out];
}
	
void Effects_Buffer::end_frame( blip_time_t clock_count )
{
	int bufs_used = 0;
	for ( int i = 0; i < buf_count; i++ )
	{
		bufs_used |= bufs [i].clear_modified() << i;
		bufs [i].end_frame( clock_count );
	}
	
	int stereo_mask = (config_.effects_enabled ? 0x78 : 0x06);
	if ( (bufs_used & stereo_mask) && buf_count == max_buf_count )
		stereo_remain = bufs [0].samples_avail() + bufs [0].output_latency();
	
	if ( effects_enabled || config_.effects_enabled )
		effect_remain = bufs [0].samples_avail() + bufs [0].output_latency();
	
	effects_enabled = config_.effects_enabled;
}

long Effects_Buffer::samples_avail() const
{
	return bufs [0].samples_avail() * 2;
}

long Effects_Buffer::read_samples( blip_sample_t* out, long total_samples )
{
	require( total_samples % 2 == 0 ); // count must be even
	
	long remain = bufs [0].samples_avail();
	if ( remain > (total_samples >> 1) )
		remain = (total_samples >> 1);
	total_samples = remain;
	while ( remain )
	{
		int active_bufs = buf_count;
		long count = remain;
		
		// optimizing mixing to skip any channels which had nothing added
		if ( effect_remain )
		{
			if ( count > effect_remain )
				count = effect_remain;
			
			if ( stereo_remain )
			{
				mix_enhanced( out, count );
			}
			else
			{
				mix_mono_enhanced( out, count );
				active_bufs = 3;
			}
		}
		else if ( stereo_remain )
		{
			mix_stereo( out, count );
			active_bufs = 3; 
		}
		else
		{
			mix_mono( out, count );
			active_bufs = 1; 
		}
		
		out += count * 2;
		remain -= count;
		
		stereo_remain -= count;
		if ( stereo_remain < 0 )
			stereo_remain = 0;
		
		effect_remain -= count;
		if ( effect_remain < 0 )
			effect_remain = 0;
		
		for ( int i = 0; i < buf_count; i++ )
		{
			if ( i < active_bufs )
				bufs [i].remove_samples( count );
			else
				bufs [i].remove_silence( count ); // keep time synchronized
		}
	}
	
	return total_samples * 2;
}

void Effects_Buffer::mix_mono( blip_sample_t* out_, blargg_long count )
{
	blip_sample_t* BLIP_RESTRICT out = out_;
	int const bass = BLIP_READER_BASS( bufs [0] );
	BLIP_READER_BEGIN( c, bufs [0] );
	
	// unrolled loop
	for ( blargg_long n = count >> 1; n; --n )
	{
		blargg_long cs0 = BLIP_READER_READ( c );
		BLIP_READER_NEXT( c, bass );
		
		blargg_long cs1 = BLIP_READER_READ( c );
		BLIP_READER_NEXT( c, bass );
		
		if ( (BOOST::int16_t) cs0 != cs0 )
			cs0 = 0x7FFF - (cs0 >> 24);
		((BOOST::uint32_t*) out) [0] = ((BOOST::uint16_t) cs0) | (cs0 << 16);
		
		if ( (BOOST::int16_t) cs1 != cs1 )
			cs1 = 0x7FFF - (cs1 >> 24);
		((BOOST::uint32_t*) out) [1] = ((BOOST::uint16_t) cs1) | (cs1 << 16);
		out += 4;
	}
	
	if ( count & 1 )
	{
		int s = BLIP_READER_READ( c );
		BLIP_READER_NEXT( c, bass );
		out [0] = s;
		out [1] = s;
		if ( (BOOST::int16_t) s != s )
		{
			s = 0x7FFF - (s >> 24);
			out [0] = s;
			out [1] = s;
		}
	}
	
	BLIP_READER_END( c, bufs [0] );
}

void Effects_Buffer::mix_stereo( blip_sample_t* out_, blargg_long count )
{
	blip_sample_t* BLIP_RESTRICT out = out_;
	int const bass = BLIP_READER_BASS( bufs [0] );
	BLIP_READER_BEGIN( c, bufs [0] );
	BLIP_READER_BEGIN( l, bufs [1] );
	BLIP_READER_BEGIN( r, bufs [2] );
	
	while ( count-- )
	{
		int cs = BLIP_READER_READ( c );
		BLIP_READER_NEXT( c, bass );
		int left = cs + BLIP_READER_READ( l );
		int right = cs + BLIP_READER_READ( r );
		BLIP_READER_NEXT( l, bass );
		BLIP_READER_NEXT( r, bass );
		
		if ( (BOOST::int16_t) left != left )
			left = 0x7FFF - (left >> 24);
		
		out [0] = left;
		out [1] = right;
		
		out += 2;
		
		if ( (BOOST::int16_t) right != right )
			out [-1] = 0x7FFF - (right >> 24);
	}
	
	BLIP_READER_END( r, bufs [2] );
	BLIP_READER_END( l, bufs [1] );
	BLIP_READER_END( c, bufs [0] );
}

void Effects_Buffer::mix_mono_enhanced( blip_sample_t* out_, blargg_long count )
{
	blip_sample_t* BLIP_RESTRICT out = out_;
	int const bass = BLIP_READER_BASS( bufs [2] );
	BLIP_READER_BEGIN( center, bufs [2] );
	BLIP_READER_BEGIN( sq1, bufs [0] );
	BLIP_READER_BEGIN( sq2, bufs [1] );
	
	blip_sample_t* const reverb_buf = this->reverb_buf.begin();
	blip_sample_t* const echo_buf = this->echo_buf.begin();
	int echo_pos = this->echo_pos;
	int reverb_pos = this->reverb_pos;
	
	while ( count-- )
	{
		int sum1_s = BLIP_READER_READ( sq1 );
		int sum2_s = BLIP_READER_READ( sq2 );
		
		BLIP_READER_NEXT( sq1, bass );
		BLIP_READER_NEXT( sq2, bass );
		
		int new_reverb_l = FMUL( sum1_s, chans.pan_1_levels [0] ) +
				FMUL( sum2_s, chans.pan_2_levels [0] ) +
				reverb_buf [(reverb_pos + chans.reverb_delay_l) & reverb_mask];
		
		int new_reverb_r = FMUL( sum1_s, chans.pan_1_levels [1] ) +
				FMUL( sum2_s, chans.pan_2_levels [1] ) +
				reverb_buf [(reverb_pos + chans.reverb_delay_r) & reverb_mask];
		
		fixed_t reverb_level = chans.reverb_level;
		reverb_buf [reverb_pos] = (blip_sample_t) FMUL( new_reverb_l, reverb_level );
		reverb_buf [reverb_pos + 1] = (blip_sample_t) FMUL( new_reverb_r, reverb_level );
		reverb_pos = (reverb_pos + 2) & reverb_mask;
		
		int sum3_s = BLIP_READER_READ( center );
		BLIP_READER_NEXT( center, bass );
		
		int left = new_reverb_l + sum3_s + FMUL( chans.echo_level,
				echo_buf [(echo_pos + chans.echo_delay_l) & echo_mask] );
		int right = new_reverb_r + sum3_s + FMUL( chans.echo_level,
				echo_buf [(echo_pos + chans.echo_delay_r) & echo_mask] );
		
		echo_buf [echo_pos] = sum3_s;
		echo_pos = (echo_pos + 1) & echo_mask;
		
		if ( (BOOST::int16_t) left != left )
			left = 0x7FFF - (left >> 24);
		
		out [0] = left;
		out [1] = right;
		
		out += 2;
		
		if ( (BOOST::int16_t) right != right )
			out [-1] = 0x7FFF - (right >> 24);
	}
	this->reverb_pos = reverb_pos;
	this->echo_pos = echo_pos;
	
	BLIP_READER_END( sq1, bufs [0] );
	BLIP_READER_END( sq2, bufs [1] );
	BLIP_READER_END( center, bufs [2] );
}

void Effects_Buffer::mix_enhanced( blip_sample_t* out_, blargg_long count )
{
	blip_sample_t* BLIP_RESTRICT out = out_;
	int const bass = BLIP_READER_BASS( bufs [2] );
	BLIP_READER_BEGIN( center, bufs [2] );
	BLIP_READER_BEGIN( l1, bufs [3] );
	BLIP_READER_BEGIN( r1, bufs [4] );
	BLIP_READER_BEGIN( l2, bufs [5] );
	BLIP_READER_BEGIN( r2, bufs [6] );
	BLIP_READER_BEGIN( sq1, bufs [0] );
	BLIP_READER_BEGIN( sq2, bufs [1] );
	
	blip_sample_t* const reverb_buf = this->reverb_buf.begin();
	blip_sample_t* const echo_buf = this->echo_buf.begin();
	int echo_pos = this->echo_pos;
	int reverb_pos = this->reverb_pos;
	
	while ( count-- )
	{
		int sum1_s = BLIP_READER_READ( sq1 );
		int sum2_s = BLIP_READER_READ( sq2 );
		
		BLIP_READER_NEXT( sq1, bass );
		BLIP_READER_NEXT( sq2, bass );
		
		int new_reverb_l = FMUL( sum1_s, chans.pan_1_levels [0] ) +
				FMUL( sum2_s, chans.pan_2_levels [0] ) + BLIP_READER_READ( l1 ) +
				reverb_buf [(reverb_pos + chans.reverb_delay_l) & reverb_mask];
		
		int new_reverb_r = FMUL( sum1_s, chans.pan_1_levels [1] ) +
				FMUL( sum2_s, chans.pan_2_levels [1] ) + BLIP_READER_READ( r1 ) +
				reverb_buf [(reverb_pos + chans.reverb_delay_r) & reverb_mask];
		
		BLIP_READER_NEXT( l1, bass );
		BLIP_READER_NEXT( r1, bass );
		
		fixed_t reverb_level = chans.reverb_level;
		reverb_buf [reverb_pos] = (blip_sample_t) FMUL( new_reverb_l, reverb_level );
		reverb_buf [reverb_pos + 1] = (blip_sample_t) FMUL( new_reverb_r, reverb_level );
		reverb_pos = (reverb_pos + 2) & reverb_mask;
		
		int sum3_s = BLIP_READER_READ( center );
		BLIP_READER_NEXT( center, bass );
		
		int left = new_reverb_l + sum3_s + BLIP_READER_READ( l2 ) + FMUL( chans.echo_level,
				echo_buf [(echo_pos + chans.echo_delay_l) & echo_mask] );
		int right = new_reverb_r + sum3_s + BLIP_READER_READ( r2 ) + FMUL( chans.echo_level,
				echo_buf [(echo_pos + chans.echo_delay_r) & echo_mask] );
		
		BLIP_READER_NEXT( l2, bass );
		BLIP_READER_NEXT( r2, bass );
		
		echo_buf [echo_pos] = sum3_s;
		echo_pos = (echo_pos + 1) & echo_mask;
		
		if ( (BOOST::int16_t) left != left )
			left = 0x7FFF - (left >> 24);
		
		out [0] = left;
		out [1] = right;
		
		out += 2;
		
		if ( (BOOST::int16_t) right != right )
			out [-1] = 0x7FFF - (right >> 24);
	}
	this->reverb_pos = reverb_pos;
	this->echo_pos = echo_pos;
	
	BLIP_READER_END( l1, bufs [3] );
	BLIP_READER_END( r1, bufs [4] );
	BLIP_READER_END( l2, bufs [5] );
	BLIP_READER_END( r2, bufs [6] );
	BLIP_READER_END( sq1, bufs [0] );
	BLIP_READER_END( sq2, bufs [1] );
	BLIP_READER_END( center, bufs [2] );
}

