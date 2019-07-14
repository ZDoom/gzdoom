// Game_Music_Emu https://bitbucket.org/mpyne/game-music-emu/

#include "Vgm_Emu.h"

#include "blargg_endian.h"
#include <string.h>
#include <math.h>

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

double const fm_gain = 3.0; // FM emulators are internally quieter to avoid 16-bit overflow
double const rolloff = 0.990;
double const oversample_factor = 1.5;

Vgm_Emu::Vgm_Emu()
{
	disable_oversampling_ = false;
	psg_rate   = 0;
	set_type( gme_vgm_type );
	
	static int const types [8] = {
		wave_type | 1, wave_type | 0, wave_type | 2, noise_type | 0
	};
	set_voice_types( types );
	
	set_silence_lookahead( 1 ); // tracks should already be trimmed
	
	set_equalizer( make_equalizer( -14.0, 80 ) );
}

Vgm_Emu::~Vgm_Emu() { }

// Track info

static byte const* skip_gd3_str( byte const* in, byte const* end )
{
	while ( end - in >= 2 )
	{
		in += 2;
		if ( !(in [-2] | in [-1]) )
			break;
	}
	return in;
}

static byte const* get_gd3_str( byte const* in, byte const* end, char* field )
{
	byte const* mid = skip_gd3_str( in, end );
	int len = (mid - in) / 2 - 1;
	if ( len > 0 )
	{
		len = min( len, (int) Gme_File::max_field_ );
		field [len] = 0;
		for ( int i = 0; i < len; i++ )
			field [i] = (in [i * 2 + 1] ? '?' : in [i * 2]); // TODO: convert to utf-8
	}
	return mid;
}

static byte const* get_gd3_pair( byte const* in, byte const* end, char* field )
{
	return skip_gd3_str( get_gd3_str( in, end, field ), end );
}

static void parse_gd3( byte const* in, byte const* end, track_info_t* out )
{
	in = get_gd3_pair( in, end, out->song );
	in = get_gd3_pair( in, end, out->game );
	in = get_gd3_pair( in, end, out->system );
	in = get_gd3_pair( in, end, out->author );
	in = get_gd3_str ( in, end, out->copyright );
	in = get_gd3_pair( in, end, out->dumper );
	in = get_gd3_str ( in, end, out->comment );
}

int const gd3_header_size = 12;

static long check_gd3_header( byte const* h, long remain )
{
	if ( remain < gd3_header_size ) return 0;
	if ( memcmp( h, "Gd3 ", 4 ) ) return 0;
	if ( get_le32( h + 4 ) >= 0x200 ) return 0;
	
	long gd3_size = get_le32( h + 8 );
	if ( gd3_size > remain - gd3_header_size ) return 0;
	
	return gd3_size;
}

byte const* Vgm_Emu::gd3_data( int* size ) const
{
	if ( size )
		*size = 0;
	
	long gd3_offset = get_le32( header().gd3_offset ) - 0x2C;
	if ( gd3_offset < 0 )
		return 0;
	
	byte const* gd3 = data + header_size + gd3_offset;
	long gd3_size = check_gd3_header( gd3, data_end - gd3 );
	if ( !gd3_size )
		return 0;
	
	if ( size )
		*size = gd3_size + gd3_header_size;
	
	return gd3;
}

static void get_vgm_length( Vgm_Emu::header_t const& h, track_info_t* out )
{
	long length = get_le32( h.track_duration ) * 10 / 441;
	if ( length > 0 )
	{
		long loop = get_le32( h.loop_duration );
		if ( loop > 0 && get_le32( h.loop_offset ) )
		{
			out->loop_length = loop * 10 / 441;
			out->intro_length = length - out->loop_length;
		}
		else
		{
			out->length = length; // 1000 / 44100 (VGM files used 44100 as timebase)
			out->intro_length = length; // make it clear that track is no longer than length
			out->loop_length = 0;
		}
	}
}

blargg_err_t Vgm_Emu::track_info_( track_info_t* out, int ) const
{
	get_vgm_length( header(), out );
	
	int size;
	byte const* gd3 = gd3_data( &size );
	if ( gd3 )
		parse_gd3( gd3 + gd3_header_size, gd3 + size, out );
	
	return 0;
}

static blargg_err_t check_vgm_header( Vgm_Emu::header_t const& h )
{
	if ( memcmp( h.tag, "Vgm ", 4 ) )
		return gme_wrong_file_type;
	return 0;
}

struct Vgm_File : Gme_Info_
{
	Vgm_Emu::header_t h;
	blargg_vector<byte> gd3;
	
	Vgm_File() { set_type( gme_vgm_type ); }
	
	blargg_err_t load_( Data_Reader& in )
	{
		long file_size = in.remain();
		if ( file_size <= Vgm_Emu::header_size )
			return gme_wrong_file_type;
		
		RETURN_ERR( in.read( &h, Vgm_Emu::header_size ) );
		RETURN_ERR( check_vgm_header( h ) );
		
		long gd3_offset = get_le32( h.gd3_offset ) - 0x2C;
		long remain = file_size - Vgm_Emu::header_size - gd3_offset;
		byte gd3_h [gd3_header_size];
		if ( gd3_offset > 0 && remain >= gd3_header_size )
		{
			RETURN_ERR( in.skip( gd3_offset ) );
			RETURN_ERR( in.read( gd3_h, sizeof gd3_h ) );
			long gd3_size = check_gd3_header( gd3_h, remain );
			if ( gd3_size )
			{
				RETURN_ERR( gd3.resize( gd3_size ) );
				RETURN_ERR( in.read( gd3.begin(), gd3.size() ) );
			}
		}
		return 0;
	}
	
	blargg_err_t track_info_( track_info_t* out, int ) const
	{
		get_vgm_length( h, out );
		if ( gd3.size() )
			parse_gd3( gd3.begin(), gd3.end(), out );
		return 0;
	}
};

static Music_Emu* new_vgm_emu () { return BLARGG_NEW Vgm_Emu ; }
static Music_Emu* new_vgm_file() { return BLARGG_NEW Vgm_File; }

static gme_type_t_ const gme_vgm_type_ = { "Sega SMS/Genesis", 1, &new_vgm_emu, &new_vgm_file, "VGM", 1 };
BLARGG_EXPORT extern gme_type_t const gme_vgm_type = &gme_vgm_type_;

static gme_type_t_ const gme_vgz_type_ = { "Sega SMS/Genesis", 1, &new_vgm_emu, &new_vgm_file, "VGZ", 1 };
BLARGG_EXPORT extern gme_type_t const gme_vgz_type = &gme_vgz_type_;


// Setup

void Vgm_Emu::set_tempo_( double t )
{
	if ( psg_rate )
	{
		vgm_rate = (long) (44100 * t + 0.5);
		blip_time_factor = (long) floor( double (1L << blip_time_bits) / vgm_rate * psg_rate + 0.5 );
		//debug_printf( "blip_time_factor: %ld\n", blip_time_factor );
		//debug_printf( "vgm_rate: %ld\n", vgm_rate );
		// TODO: remove? calculates vgm_rate more accurately (above differs at most by one Hz only)
		//blip_time_factor = (long) floor( double (1L << blip_time_bits) * psg_rate / 44100 / t + 0.5 );
		//vgm_rate = (long) floor( double (1L << blip_time_bits) * psg_rate / blip_time_factor + 0.5 );
		
		fm_time_factor = 2 + (long) floor( fm_rate * (1L << fm_time_bits) / vgm_rate + 0.5 );
	}
}

blargg_err_t Vgm_Emu::set_sample_rate_( long sample_rate )
{
	RETURN_ERR( blip_buf.set_sample_rate( sample_rate, 1000 / 30 ) );
	return Classic_Emu::set_sample_rate_( sample_rate );
}

blargg_err_t Vgm_Emu::set_multi_channel ( bool is_enabled )
{
	// we acutally should check here whether this is classic emu or not
	// however set_multi_channel() is called before setup_fm() resulting in uninited is_classic_emu()
	// hard code it to unsupported
#if 0
	if ( is_classic_emu() )
	{
		RETURN_ERR( Music_Emu::set_multi_channel_( is_enabled ) );
		return 0;
	}
	else
#endif
	{
		(void) is_enabled;
		return "multichannel rendering not supported for YM2*** FM sound chip emulators";
	}
}

void Vgm_Emu::update_eq( blip_eq_t const& eq )
{
	psg.treble_eq( eq );
	dac_synth.treble_eq( eq );
}

void Vgm_Emu::set_voice( int i, Blip_Buffer* c, Blip_Buffer* l, Blip_Buffer* r )
{
	if ( i < psg.osc_count )
		psg.osc_output( i, c, l, r );
}

void Vgm_Emu::mute_voices_( int mask )
{
	Classic_Emu::mute_voices_( mask );
	dac_synth.output( &blip_buf );
	if ( uses_fm )
	{
		psg.output( (mask & 0x80) ? 0 : &blip_buf );
		if ( ym2612.enabled() )
		{
			dac_synth.volume( (mask & 0x40) ? 0.0 : 0.1115 / 256 * fm_gain * gain() );
			ym2612.mute_voices( mask );
		}
		
		if ( ym2413.enabled() )
		{
			int m = mask & 0x3F;
			if ( mask & 0x20 )
				m |= 0x01E0; // channels 5-8
			if ( mask & 0x40 )
				m |= 0x3E00;
			ym2413.mute_voices( m );
		}
	}
}

blargg_err_t Vgm_Emu::load_mem_( byte const* new_data, long new_size )
{
	assert( offsetof (header_t,unused2 [8]) == header_size );
	
	if ( new_size <= header_size )
		return gme_wrong_file_type;
	
	header_t const& h = *(header_t const*) new_data;
	
	RETURN_ERR( check_vgm_header( h ) );
	
	check( get_le32( h.version ) <= 0x150 );
	
	// psg rate
	psg_rate = get_le32( h.psg_rate );
	if ( !psg_rate )
		psg_rate = 3579545;
	blip_buf.clock_rate( psg_rate );
	
	data     = new_data;
	data_end = new_data + new_size;
	
	// get loop
	loop_begin = data_end;
	if ( get_le32( h.loop_offset ) )
		loop_begin = &data [get_le32( h.loop_offset ) + offsetof (header_t,loop_offset)];
	
	set_voice_count( psg.osc_count );
	
	RETURN_ERR( setup_fm() );
	
	static const char* const fm_names [] = {
		"FM 1", "FM 2", "FM 3", "FM 4", "FM 5", "FM 6", "PCM", "PSG"
	};
	static const char* const psg_names [] = { "Square 1", "Square 2", "Square 3", "Noise" };
	set_voice_names( uses_fm ? fm_names : psg_names );
	
	// do after FM in case output buffer is changed
	return Classic_Emu::setup_buffer( psg_rate );
}

blargg_err_t Vgm_Emu::setup_fm()
{
	long ym2612_rate = get_le32( header().ym2612_rate );
	long ym2413_rate = get_le32( header().ym2413_rate );
	if ( ym2413_rate && get_le32( header().version ) < 0x110 )
		update_fm_rates( &ym2413_rate, &ym2612_rate );
	
	uses_fm = false;
	
	fm_rate = blip_buf.sample_rate() * oversample_factor;
	
	if ( ym2612_rate )
	{
		uses_fm = true;
		if ( disable_oversampling_ )
			fm_rate = ym2612_rate / 144.0;
		Dual_Resampler::setup( fm_rate / blip_buf.sample_rate(), rolloff, fm_gain * gain() );
		RETURN_ERR( ym2612.set_rate( fm_rate, ym2612_rate ) );
		ym2612.enable( true );
		set_voice_count( 8 );
	}
	
	if ( !uses_fm && ym2413_rate )
	{
		uses_fm = true;
		if ( disable_oversampling_ )
			fm_rate = ym2413_rate / 72.0;
		Dual_Resampler::setup( fm_rate / blip_buf.sample_rate(), rolloff, fm_gain * gain() );
		int result = ym2413.set_rate( fm_rate, ym2413_rate );
		if ( result == 2 )
			return "YM2413 FM sound isn't supported";
		CHECK_ALLOC( !result );
		ym2413.enable( true );
		set_voice_count( 8 );
	}
	
	if ( uses_fm )
	{
		RETURN_ERR( Dual_Resampler::reset( blip_buf.length() * blip_buf.sample_rate() / 1000 ) );
		psg.volume( 0.135 * fm_gain * gain() );
	}
	else
	{
		ym2612.enable( false );
		ym2413.enable( false );
		psg.volume( gain() );
	}
	
	return 0;
}

// Emulation

blargg_err_t Vgm_Emu::start_track_( int track )
{
	RETURN_ERR( Classic_Emu::start_track_( track ) );
	psg.reset( get_le16( header().noise_feedback ), header().noise_width );
	
	dac_disabled = -1;
	pos          = data + header_size;
	pcm_data     = pos;
	pcm_pos      = pos;
	dac_amp      = -1;
	vgm_time     = 0;
	if ( get_le32( header().version ) >= 0x150 )
	{
		long data_offset = get_le32( header().data_offset );
		check( data_offset );
		if ( data_offset )
			pos += data_offset + offsetof (header_t,data_offset) - 0x40;
	}
	
	if ( uses_fm )
	{
		if ( ym2413.enabled() )
			ym2413.reset();
		
		if ( ym2612.enabled() )
			ym2612.reset();
		
		fm_time_offset = 0;
		blip_buf.clear();
		Dual_Resampler::clear();
	}
	return 0;
}

blargg_err_t Vgm_Emu::run_clocks( blip_time_t& time_io, int msec )
{
	time_io = run_commands( msec * vgm_rate / 1000 );
	psg.end_frame( time_io );
	return 0;
}

blargg_err_t Vgm_Emu::play_( long count, sample_t* out )
{
	if ( !uses_fm )
		return Classic_Emu::play_( count, out );
		
	Dual_Resampler::dual_play( count, out, blip_buf );
	return 0;
}
