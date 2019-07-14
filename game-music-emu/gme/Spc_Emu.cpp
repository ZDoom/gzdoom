// Game_Music_Emu https://bitbucket.org/mpyne/game-music-emu/

#include "Spc_Emu.h"

#include "blargg_endian.h"
#include <stdlib.h>
#include <string.h>

/* Copyright (C) 2004-2006 Shay Green. This module is free software; you
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

// TODO: support Spc_Filter's bass

Spc_Emu::Spc_Emu()
{
	set_type( gme_spc_type );
	
	static const char* const names [Snes_Spc::voice_count] = {
		"DSP 1", "DSP 2", "DSP 3", "DSP 4", "DSP 5", "DSP 6", "DSP 7", "DSP 8"
	};
	set_voice_names( names );
	
	set_gain( 1.4 );
}

Spc_Emu::~Spc_Emu() { }

// Track info

long const trailer_offset = 0x10200;

byte const* Spc_Emu::trailer() const { return &file_data [min( file_size, trailer_offset )]; }

long Spc_Emu::trailer_size() const { return max( 0L, file_size - trailer_offset ); }

static void get_spc_xid6( byte const* begin, long size, track_info_t* out )
{
	// header
	byte const* end = begin + size;
	if ( size < 8 || memcmp( begin, "xid6", 4 ) )
	{
		check( false );
		return;
	}
	long info_size = get_le32( begin + 4 );
	byte const* in = begin + 8; 
	if ( end - in > info_size )
	{
		debug_printf( "Extra data after SPC xid6 info\n" );
		end = in + info_size;
	}
	
	int year = 0;
	char copyright [256 + 5];
	int copyright_len = 0;
	int const year_len = 5;
	
	while ( end - in >= 4 )
	{
		// header
		int id   = in [0];
		int data = in [3] * 0x100 + in [2];
		int type = in [1];
		int len  = type ? data : 0;
		in += 4;
		if ( len > end - in )
		{
			check( false );
			break; // block goes past end of data
		}
		
		// handle specific block types
		char* field = 0;
		switch ( id )
		{
			case 0x01: field = out->song;    break;
			case 0x02: field = out->game;    break;
			case 0x03: field = out->author;  break;
			case 0x04: field = out->dumper;  break;
			case 0x07: field = out->comment; break;
			case 0x14: year = data;          break;
			
			//case 0x30: // intro length
			// Many SPCs have intro length set wrong for looped tracks, making it useless
			/*
			case 0x30:
				check( len == 4 );
				if ( len >= 4 )
				{
					out->intro_length = get_le32( in ) / 64;
					if ( out->length > 0 )
					{
						long loop = out->length - out->intro_length;
						if ( loop >= 2000 )
							out->loop_length = loop;
					}
				}
				break;
			*/
			
			case 0x13:
				copyright_len = min( len, (int) sizeof copyright - year_len );
				memcpy( &copyright [year_len], in, copyright_len );
				break;
			
			default:
				if ( id < 0x01 || (id > 0x07 && id < 0x10) ||
						(id > 0x14 && id < 0x30) || id > 0x36 )
					debug_printf( "Unknown SPC xid6 block: %X\n", (int) id );
				break;
		}
		if ( field )
		{
			check( type == 1 );
			Gme_File::copy_field_( field, (char const*) in, len );
		}
		
		// skip to next block
		in += len;
		
		// blocks are supposed to be 4-byte aligned with zero-padding...
		byte const* unaligned = in;
		while ( (in - begin) & 3 && in < end )
		{
			if ( *in++ != 0 )
			{
				// ...but some files have no padding
				in = unaligned;
				debug_printf( "SPC info tag wasn't properly padded to align\n" );
				break;
			}
		}
	}
	
	char* p = &copyright [year_len];
	if ( year )
	{
		*--p = ' ';
		for ( int n = 4; n--; )
		{
			*--p = char (year % 10 + '0');
			year /= 10;
		}
		copyright_len += year_len;
	}
	if ( copyright_len )
		Gme_File::copy_field_( out->copyright, p, copyright_len );
	
	check( in == end );
}

static void get_spc_info( Spc_Emu::header_t const& h, byte const* xid6, long xid6_size,
		track_info_t* out )
{
	// decode length (can be in text or binary format, sometimes ambiguous ugh)
	long len_secs = 0;
	for ( int i = 0; i < 3; i++ )
	{
		unsigned n = h.len_secs [i] - '0';
		if ( n > 9 )
		{
			// ignore single-digit text lengths
			// (except if author field is present and begins at offset 1, ugh)
			if ( i == 1 && (h.author [0] || !h.author [1]) )
				len_secs = 0;
			break;
		}
		len_secs *= 10;
		len_secs += n;
	}
	if ( !len_secs || len_secs > 0x1FFF )
		len_secs = get_le16( h.len_secs );
	if ( len_secs < 0x1FFF )
		out->length = len_secs * 1000;
	
	int offset = (h.author [0] < ' ' || unsigned (h.author [0] - '0') <= 9);
	Gme_File::copy_field_( out->author, &h.author [offset], sizeof h.author - offset );
	
	GME_COPY_FIELD( h, out, song );
	GME_COPY_FIELD( h, out, game );
	GME_COPY_FIELD( h, out, dumper );
	GME_COPY_FIELD( h, out, comment );
	
	if ( xid6_size )
		get_spc_xid6( xid6, xid6_size, out );
}

blargg_err_t Spc_Emu::track_info_( track_info_t* out, int ) const
{
	get_spc_info( header(), trailer(), trailer_size(), out );
	return 0;
}

static blargg_err_t check_spc_header( void const* header )
{
	if ( memcmp( header, "SNES-SPC700 Sound File Data", 27 ) )
		return gme_wrong_file_type;
	return 0;
}

struct Spc_File : Gme_Info_
{
	Spc_Emu::header_t header;
	blargg_vector<byte> xid6;
	
	Spc_File() { set_type( gme_spc_type ); }
	
	blargg_err_t load_( Data_Reader& in )
	{
		long file_size = in.remain();
		if ( file_size < Snes_Spc::spc_min_file_size )
			return gme_wrong_file_type;
		RETURN_ERR( in.read( &header, Spc_Emu::header_size ) );
		RETURN_ERR( check_spc_header( header.tag ) );
		long const xid6_offset = 0x10200;
		long xid6_size = file_size - xid6_offset;
		if ( xid6_size > 0 )
		{
			RETURN_ERR( xid6.resize( xid6_size ) );
			RETURN_ERR( in.skip( xid6_offset - Spc_Emu::header_size ) );
			RETURN_ERR( in.read( xid6.begin(), xid6.size() ) );
		}
		return 0;
	}
	
	blargg_err_t track_info_( track_info_t* out, int ) const
	{
		get_spc_info( header, xid6.begin(), xid6.size(), out );
		return 0;
	}
};

static Music_Emu* new_spc_emu () { return BLARGG_NEW Spc_Emu ; }
static Music_Emu* new_spc_file() { return BLARGG_NEW Spc_File; }

static gme_type_t_ const gme_spc_type_ = { "Super Nintendo", 1, &new_spc_emu, &new_spc_file, "SPC", 0 };
BLARGG_EXPORT extern gme_type_t const gme_spc_type = &gme_spc_type_;


// Setup

blargg_err_t Spc_Emu::set_sample_rate_( long sample_rate )
{
	RETURN_ERR( apu.init() );
	enable_accuracy( false );
	if ( sample_rate != native_sample_rate )
	{
		RETURN_ERR( resampler.buffer_size( native_sample_rate / 20 * 2 ) );
		resampler.time_ratio( (double) native_sample_rate / sample_rate, 0.9965 );
	}
	return 0;
}

void Spc_Emu::enable_accuracy_( bool b )
{
	Music_Emu::enable_accuracy_( b );
	filter.enable( b );
}

void Spc_Emu::mute_voices_( int m )
{
	Music_Emu::mute_voices_( m );
	apu.mute_voices( m );
}

blargg_err_t Spc_Emu::load_mem_( byte const* in, long size )
{
	assert( offsetof (header_t,unused2 [46]) == header_size );
	file_data = in;
	file_size = size;
	set_voice_count( Snes_Spc::voice_count );
	if ( size < Snes_Spc::spc_min_file_size )
		return gme_wrong_file_type;
	return check_spc_header( in );
}

// Emulation

void Spc_Emu::set_tempo_( double t )
{
	apu.set_tempo( (int) (t * apu.tempo_unit) );
}

blargg_err_t Spc_Emu::start_track_( int track )
{
	RETURN_ERR( Music_Emu::start_track_( track ) );
	resampler.clear();
	filter.clear();
	RETURN_ERR( apu.load_spc( file_data, file_size ) );
	filter.set_gain( (int) (gain() * SPC_Filter::gain_unit) );
	apu.clear_echo();
	return 0;
}

blargg_err_t Spc_Emu::play_and_filter( long count, sample_t out [] )
{
	RETURN_ERR( apu.play( count, out ) );
	filter.run( out, count );
	return 0;
}

blargg_err_t Spc_Emu::skip_( long count )
{
	if ( sample_rate() != native_sample_rate )
	{
		count = long (count * resampler.ratio()) & ~1;
		count -= resampler.skip_input( count );
	}
	
	// TODO: shouldn't skip be adjusted for the 64 samples read afterwards?
	
	if ( count > 0 )
	{
		RETURN_ERR( apu.skip( count ) );
		filter.clear();
	}
	
	// eliminate pop due to resampler
	const int resampler_latency = 64;
	sample_t buf [resampler_latency];
	return play_( resampler_latency, buf );
}

blargg_err_t Spc_Emu::play_( long count, sample_t* out )
{
	if ( sample_rate() == native_sample_rate )
		return play_and_filter( count, out );
	
	long remain = count;
	while ( remain > 0 )
	{
		remain -= resampler.read( &out [count - remain], remain );
		if ( remain > 0 )
		{
			long n = resampler.max_write();
			RETURN_ERR( play_and_filter( n, resampler.buffer() ) );
			resampler.write( n );
		}
	}
	check( remain == 0 );
	return 0;
}
