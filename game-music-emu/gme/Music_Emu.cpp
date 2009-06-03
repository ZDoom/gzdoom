// Game_Music_Emu 0.5.2. http://www.slack.net/~ant/

#include "Music_Emu.h"

#include "Multi_Buffer.h"
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

int const stereo = 2; // number of channels for stereo
int const silence_max = 6; // seconds
int const silence_threshold = 0x10;
long const fade_block_size = 512;
int const fade_shift = 8; // fade ends with gain at 1.0 / (1 << fade_shift)

Music_Emu::equalizer_t const Music_Emu::tv_eq = { -8.0, 180 };

void Music_Emu::clear_track_vars()
{
	current_track_   = -1;
	out_time         = 0;
	emu_time         = 0;
	emu_track_ended_ = true;
	track_ended_     = true;
	fade_start       = INT_MAX / 2 + 1;
	fade_step        = 1;
	silence_time     = 0;
	silence_count    = 0;
	buf_remain       = 0;
	warning(); // clear warning
}

void Music_Emu::unload()
{
	voice_count_ = 0;
	clear_track_vars();
	Gme_File::unload();
}

Music_Emu::Music_Emu()
{
	effects_buffer = 0;
	
	sample_rate_ = 0;
	mute_mask_   = 0;
	tempo_       = 1.0;
	gain_        = 1.0;
	
	// defaults
	max_initial_silence = 2;
	silence_lookahead   = 3;
	ignore_silence_     = false;
	equalizer_.treble   = -1.0;
	equalizer_.bass     = 60;
	
	static const char* const names [] = {
		"Voice 1", "Voice 2", "Voice 3", "Voice 4",
		"Voice 5", "Voice 6", "Voice 7", "Voice 8"
	};
	set_voice_names( names );
	Music_Emu::unload(); // non-virtual
}

Music_Emu::~Music_Emu() { delete effects_buffer; }

blargg_err_t Music_Emu::set_sample_rate( long rate )
{
	require( !sample_rate() ); // sample rate can't be changed once set
	RETURN_ERR( set_sample_rate_( rate ) );
	RETURN_ERR( buf.resize( buf_size ) );
	sample_rate_ = rate;
	return 0;
}

void Music_Emu::pre_load()
{
	require( sample_rate() ); // set_sample_rate() must be called before loading a file
	Gme_File::pre_load();
}

void Music_Emu::set_equalizer( equalizer_t const& eq )
{
	equalizer_ = eq;
	set_equalizer_( eq );
}

void Music_Emu::mute_voice( int index, bool mute )
{
	require( (unsigned) index < (unsigned) voice_count() );
	int bit = 1 << index;
	int mask = mute_mask_ | bit;
	if ( !mute )
		mask ^= bit;
	mute_voices( mask );
}

void Music_Emu::mute_voices( int mask )
{
	require( sample_rate() ); // sample rate must be set first
	mute_mask_ = mask;
	mute_voices_( mask );
}

void Music_Emu::set_tempo( double t )
{
	require( sample_rate() ); // sample rate must be set first
	double const min = 0.02;
	double const max = 4.00;
	if ( t < min ) t = min;
	if ( t > max ) t = max;
	tempo_ = t;
	set_tempo_( t );
}

void Music_Emu::post_load_()
{
	set_tempo( tempo_ );
	remute_voices();
}

blargg_err_t Music_Emu::start_track( int track )
{
	clear_track_vars();
	
	int remapped = track;
	RETURN_ERR( remap_track_( &remapped ) );
	current_track_ = track;
	RETURN_ERR( start_track_( remapped ) );
	
	emu_track_ended_ = false;
	track_ended_     = false;
	
	if ( !ignore_silence_ )
	{
		// play until non-silence or end of track
		for ( long end = max_initial_silence * stereo * sample_rate(); emu_time < end; )
		{
			fill_buf();
			if ( buf_remain | (int) emu_track_ended_ )
				break;
		}
		
		emu_time      = buf_remain;
		out_time      = 0;
		silence_time  = 0;
		silence_count = 0;
	}
	return track_ended() ? warning() : 0;
}

void Music_Emu::end_track_if_error( blargg_err_t err )
{
	if ( err )
	{
		emu_track_ended_ = true;
		set_warning( err );
	}
}

// Tell/Seek

blargg_long Music_Emu::msec_to_samples( blargg_long msec ) const
{
	blargg_long sec = msec / 1000;
	msec -= sec * 1000;
	return (sec * sample_rate() + msec * sample_rate() / 1000) * stereo;
}

long Music_Emu::tell() const
{
	blargg_long rate = sample_rate() * stereo;
	blargg_long sec = out_time / rate;
	return sec * 1000 + (out_time - sec * rate) * 1000 / rate;
}

blargg_err_t Music_Emu::seek( long msec )
{
	blargg_long time = msec_to_samples( msec );
	if ( time < out_time )
		RETURN_ERR( start_track( current_track_ ) );
	return skip( time - out_time );
}

blargg_err_t Music_Emu::skip( long count )
{
	require( current_track() >= 0 ); // start_track() must have been called already
	out_time += count;
	
	// remove from silence and buf first
	{
		long n = min( count, silence_count );
		silence_count -= n;
		count -= n;
		
		n = min( count, buf_remain );
		buf_remain -= n;
		count -= n;
	}
		
	if ( count && !emu_track_ended_ )
	{
		emu_time += count;
		end_track_if_error( skip_( count ) );
	}
	
	if ( !(silence_count | buf_remain) ) // caught up to emulator, so update track ended
		track_ended_ |= emu_track_ended_;
	
	return 0;
}

blargg_err_t Music_Emu::skip_( long count )
{
	// for long skip, mute sound
	const long threshold = 30000;
	if ( count > threshold )
	{
		int saved_mute = mute_mask_;
		mute_voices( ~0 );
		
		while ( count > threshold / 2 && !emu_track_ended_ )
		{
			RETURN_ERR( play_( buf_size, buf.begin() ) );
			count -= buf_size;
		}
		
		mute_voices( saved_mute );
	}
	
	while ( count && !emu_track_ended_ )
	{
		long n = buf_size;
		if ( n > count )
			n = count;
		count -= n;
		RETURN_ERR( play_( n, buf.begin() ) );
	}
	return 0;
}

// Fading

void Music_Emu::set_fade( long start_msec, long length_msec )
{
	fade_step = sample_rate() * length_msec / (fade_block_size * fade_shift * 1000 / stereo);
	fade_start = msec_to_samples( start_msec );
}

// unit / pow( 2.0, (double) x / step )
static int int_log( blargg_long x, int step, int unit )
{
	int shift = x / step;
	int fraction = (x - shift * step) * unit / step;
	return ((unit - fraction) + (fraction >> 1)) >> shift;
}

void Music_Emu::handle_fade( long out_count, sample_t* out )
{
	for ( int i = 0; i < out_count; i += fade_block_size )
	{
		int const shift = 14;
		int const unit = 1 << shift;
		int gain = int_log( (out_time + i - fade_start) / fade_block_size,
				fade_step, unit );
		if ( gain < (unit >> fade_shift) )
			track_ended_ = emu_track_ended_ = true;
		
		sample_t* io = &out [i];
		for ( int count = min( fade_block_size, out_count - i ); count; --count )
		{
			*io = sample_t ((*io * gain) >> shift);
			++io;
		}
	}
}

// Silence detection

void Music_Emu::emu_play( long count, sample_t* out )
{
	check( current_track_ >= 0 );
	emu_time += count;
	if ( current_track_ >= 0 && !emu_track_ended_ )
		end_track_if_error( play_( count, out ) );
	else
		memset( out, 0, count * sizeof *out );
}

// number of consecutive silent samples at end
static long count_silence( Music_Emu::sample_t* begin, long size )
{
	Music_Emu::sample_t first = *begin;
	*begin = silence_threshold; // sentinel
	Music_Emu::sample_t* p = begin + size;
	while ( (unsigned) (*--p + silence_threshold / 2) <= (unsigned) silence_threshold ) { }
	*begin = first;
	return size - long(p - begin);
}

// fill internal buffer and check it for silence
void Music_Emu::fill_buf()
{
	assert( !buf_remain );
	if ( !emu_track_ended_ )
	{
		emu_play( buf_size, buf.begin() );
		long silence = count_silence( buf.begin(), buf_size );
		if ( silence < buf_size )
		{
			silence_time = emu_time - silence;
			buf_remain   = buf_size;
			return;
		}
	}
	silence_count += buf_size;
}

blargg_err_t Music_Emu::play( long out_count, sample_t* out )
{
	if ( track_ended_ )
	{
		memset( out, 0, out_count * sizeof *out );
	}
	else
	{
		require( current_track() >= 0 );
		require( out_count % stereo == 0 );
		
		assert( emu_time >= out_time );
		
		// prints nifty graph of how far ahead we are when searching for silence
		//dprintf( "%*s \n", int ((emu_time - out_time) * 7 / sample_rate()), "*" );
		
		long pos = 0;
		if ( silence_count )
		{
			// during a run of silence, run emulator at >=2x speed so it gets ahead
			long ahead_time = silence_lookahead * (out_time + out_count - silence_time) + silence_time;
			while ( emu_time < ahead_time && !(buf_remain || emu_track_ended_) )
				fill_buf();
			
			// fill with silence
			pos = min( silence_count, out_count );
			memset( out, 0, pos * sizeof *out );
			silence_count -= pos;
			
			if ( emu_time - silence_time > silence_max * stereo * sample_rate() )
			{
				track_ended_  = emu_track_ended_ = true;
				silence_count = 0;
				buf_remain    = 0;
			}
		}
		
		if ( buf_remain )
		{
			// empty silence buf
			long n = min( buf_remain, out_count - pos );
			memcpy( &out [pos], buf.begin() + (buf_size - buf_remain), n * sizeof *out );
			buf_remain -= n;
			pos += n;
		}
		
		// generate remaining samples normally
		long remain = out_count - pos;
		if ( remain )
		{
			emu_play( remain, out + pos );
			track_ended_ |= emu_track_ended_;
			
			if ( !ignore_silence_ || out_time > fade_start )
			{
				// check end for a new run of silence
				long silence = count_silence( out + pos, remain );
				if ( silence < remain )
					silence_time = emu_time - silence;
				
				if ( emu_time - silence_time >= buf_size )
					fill_buf(); // cause silence detection on next play()
			}
		}
		
		if ( out_time > fade_start )
			handle_fade( out_count, out );
	}
	out_time += out_count;
	return 0;
}

// Gme_Info_

blargg_err_t Gme_Info_::set_sample_rate_( long )            { return 0; }
void         Gme_Info_::pre_load()                          { Gme_File::pre_load(); } // skip Music_Emu
void         Gme_Info_::post_load_()                        { Gme_File::post_load_(); } // skip Music_Emu
void         Gme_Info_::set_equalizer_( equalizer_t const& ){ check( false ); }
void         Gme_Info_::mute_voices_( int )                 { check( false ); }
void         Gme_Info_::set_tempo_( double )                { }
blargg_err_t Gme_Info_::start_track_( int )                 { return "Use full emulator for playback"; }
blargg_err_t Gme_Info_::play_( long, sample_t* )            { return "Use full emulator for playback"; }
