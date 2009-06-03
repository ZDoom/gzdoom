// Game_Music_Emu 0.5.2. http://www.slack.net/~ant/

#include "Ay_Emu.h"

#include "blargg_endian.h"
#include <string.h>

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

long const spectrum_clock = 3546900;
long const cpc_clock      = 2000000;

unsigned const ram_start = 0x4000;
int const osc_count = Ay_Apu::osc_count + 1;

Ay_Emu::Ay_Emu()
{
	beeper_output = 0;
	set_type( gme_ay_type );
	
	static const char* const names [osc_count] = {
		"Wave 1", "Wave 2", "Wave 3", "Beeper"
	};
	set_voice_names( names );
	
	static int const types [osc_count] = {
		wave_type | 0, wave_type | 1, wave_type | 2, mixed_type | 0
	};
	set_voice_types( types );
	set_silence_lookahead( 6 );
}

Ay_Emu::~Ay_Emu() { }

// Track info

static byte const* get_data( Ay_Emu::file_t const& file, byte const* ptr, int min_size )
{
	long pos = long(ptr - (byte const*) file.header);
	long file_size = long(file.end - (byte const*) file.header);
	assert( (unsigned long) pos <= (unsigned long) file_size - 2 );
	int offset = (BOOST::int16_t) get_be16( ptr );
	if ( !offset || blargg_ulong (pos + offset) > blargg_ulong (file_size - min_size) )
		return 0;
	return ptr + offset;
}

static blargg_err_t parse_header( byte const* in, long size, Ay_Emu::file_t* out )
{
	typedef Ay_Emu::header_t header_t;
	out->header = (header_t const*) in;
	out->end    = in + size;
	
	if ( size < Ay_Emu::header_size )
		return gme_wrong_file_type;
	
	header_t const& h = *(header_t const*) in;
	if ( memcmp( h.tag, "ZXAYEMUL", 8 ) )
		return gme_wrong_file_type;
	
	out->tracks = get_data( *out, h.track_info, (h.max_track + 1) * 4 );
	if ( !out->tracks )
		return "Missing track data";
	
	return 0;
}

static void copy_ay_fields( Ay_Emu::file_t const& file, track_info_t* out, int track )
{
	Gme_File::copy_field_( out->song, (char const*) get_data( file, file.tracks + track * 4, 1 ) );
	byte const* track_info = get_data( file, file.tracks + track * 4 + 2, 6 );
	if ( track_info )
		out->length = get_be16( track_info + 4 ) * (1000L / 50); // frames to msec
	
	Gme_File::copy_field_( out->author,  (char const*) get_data( file, file.header->author, 1 ) );
	Gme_File::copy_field_( out->comment, (char const*) get_data( file, file.header->comment, 1 ) );
}

blargg_err_t Ay_Emu::track_info_( track_info_t* out, int track ) const
{
	copy_ay_fields( file, out, track );
	return 0;
}

struct Ay_File : Gme_Info_
{
	Ay_Emu::file_t file;
	
	Ay_File() { set_type( gme_ay_type ); }
	
	blargg_err_t load_mem_( byte const* begin, long size )
	{
		RETURN_ERR( parse_header( begin, size, &file ) );
		set_track_count( file.header->max_track + 1 );
		return 0;
	}
	
	blargg_err_t track_info_( track_info_t* out, int track ) const
	{
		copy_ay_fields( file, out, track );
		return 0;
	}
};

static Music_Emu* new_ay_emu () { return BLARGG_NEW Ay_Emu ; }
static Music_Emu* new_ay_file() { return BLARGG_NEW Ay_File; }

static gme_type_t_ const gme_ay_type_ = { "ZX Spectrum", 0, &new_ay_emu, &new_ay_file, "AY", 1 };
gme_type_t const gme_ay_type = &gme_ay_type_;

// Setup

blargg_err_t Ay_Emu::load_mem_( byte const* in, long size )
{
	assert( offsetof (header_t,track_info [2]) == header_size );
	
	RETURN_ERR( parse_header( in, size, &file ) );
	set_track_count( file.header->max_track + 1 );
	
	if ( file.header->vers > 2 )
		set_warning( "Unknown file version" );
	
	set_voice_count( osc_count );
	apu.volume( gain() );
	
	return setup_buffer( spectrum_clock );
}
	
void Ay_Emu::update_eq( blip_eq_t const& eq )
{
	apu.treble_eq( eq );
}

void Ay_Emu::set_voice( int i, Blip_Buffer* center, Blip_Buffer*, Blip_Buffer* )
{
	if ( i >= Ay_Apu::osc_count )
		beeper_output = center;
	else
		apu.osc_output( i, center );
}

// Emulation

void Ay_Emu::set_tempo_( double t )
{
	play_period = blip_time_t (clock_rate() / 50 / t);
}

blargg_err_t Ay_Emu::start_track_( int track )
{
	RETURN_ERR( Classic_Emu::start_track_( track ) );
	
	memset( mem.ram + 0x0000, 0xC9, 0x100 ); // fill RST vectors with RET
	memset( mem.ram + 0x0100, 0xFF, 0x4000 - 0x100 );
	memset( mem.ram + ram_start, 0x00, sizeof mem.ram - ram_start );
	memset( mem.padding1, 0xFF, sizeof mem.padding1 );
	memset( mem.ram + 0x10000, 0xFF, sizeof mem.ram - 0x10000 );
	
	// locate data blocks
	byte const* const data = get_data( file, file.tracks + track * 4 + 2, 14 );
	if ( !data ) return "File data missing";
	
	byte const* const more_data = get_data( file, data + 10, 6 );
	if ( !more_data ) return "File data missing";
	
	byte const* blocks = get_data( file, data + 12, 8 );
	if ( !blocks ) return "File data missing";
	
	// initial addresses
	cpu::reset( mem.ram );
	r.sp = get_be16( more_data );
	r.b.a = r.b.b = r.b.d = r.b.h = data [8];
	r.b.flags = r.b.c = r.b.e = r.b.l = data [9];
	r.alt.w = r.w;
	r.ix = r.iy = r.w.hl;
	
	unsigned addr = get_be16( blocks );
	if ( !addr ) return "File data missing";
	
	unsigned init = get_be16( more_data + 2 );
	if ( !init )
		init = addr;
	
	// copy blocks into memory
	do
	{
		blocks += 2;
		unsigned len = get_be16( blocks ); blocks += 2;
		if ( addr + len > 0x10000 )
		{
			set_warning( "Bad data block size" );
			len = 0x10000 - addr;
		}
		check( len );
		byte const* in = get_data( file, blocks, 0 ); blocks += 2;
		if ( len > blargg_ulong (file.end - in) )
		{
			set_warning( "Missing file data" );
			len = unsigned(file.end - in);
		}
		//dprintf( "addr: $%04X, len: $%04X\n", addr, len );
		if ( addr < ram_start && addr >= 0x400 ) // several tracks use low data
			dprintf( "Block addr in ROM\n" );
		memcpy( mem.ram + addr, in, len );
		
		if ( file.end - blocks < 8 )
		{
			set_warning( "Missing file data" );
			break;
		}
	}
	while ( (addr = get_be16( blocks )) != 0 );
	
	// copy and configure driver
	static byte const passive [] = {
		0xF3,       // DI
		0xCD, 0, 0, // CALL init
		0xED, 0x5E, // LOOP: IM 2
		0xFB,       // EI
		0x76,       // HALT
		0x18, 0xFA  // JR LOOP
	};
	static byte const active [] = {
		0xF3,       // DI
		0xCD, 0, 0, // CALL init
		0xED, 0x56, // LOOP: IM 1
		0xFB,       // EI
		0x76,       // HALT
		0xCD, 0, 0, // CALL play
		0x18, 0xF7  // JR LOOP
	};
	memcpy( mem.ram, passive, sizeof passive );
	unsigned play_addr = get_be16( more_data + 4 );
	//dprintf( "Play: $%04X\n", play_addr );
	if ( play_addr )
	{
		memcpy( mem.ram, active, sizeof active );
		mem.ram [ 9] = play_addr;
		mem.ram [10] = play_addr >> 8;
	}
	mem.ram [2] = init;
	mem.ram [3] = init >> 8;
	
	mem.ram [0x38] = 0xFB; // Put EI at interrupt vector (followed by RET)
	
	memcpy( mem.ram + 0x10000, mem.ram, 0x80 ); // some code wraps around (ugh)
	
	beeper_delta = int (apu.amp_range * 0.65);
	last_beeper = 0;
	apu.reset();
	next_play = play_period;
	
	// start at spectrum speed
	change_clock_rate( spectrum_clock );
	set_tempo( tempo() );
	
	spectrum_mode = false;
	cpc_mode      = false;
	cpc_latch     = 0;
	
	return 0;
}

// Emulation

void Ay_Emu::cpu_out_misc( cpu_time_t time, unsigned addr, int data )
{
	if ( !cpc_mode )
	{
		switch ( addr & 0xFEFF )
		{
		case 0xFEFD:
			spectrum_mode = true;
			apu_addr = data & 0x0F;
			return;
		
		case 0xBEFD:
			spectrum_mode = true;
			apu.write( time, apu_addr, data );
			return;
		}
	}
	
	if ( !spectrum_mode )
	{
		switch ( addr >> 8 )
		{
		case 0xF6:
			switch ( data & 0xC0 )
			{
			case 0xC0:
				apu_addr = cpc_latch & 0x0F;
				goto enable_cpc;
			
			case 0x80:
				apu.write( time, apu_addr, cpc_latch );
				goto enable_cpc;
			}
			break;
		
		case 0xF4:
			cpc_latch = data;
			goto enable_cpc;
		}
	}
	
	dprintf( "Unmapped OUT: $%04X <- $%02X\n", addr, data );
	return;
	
enable_cpc:
	if ( !cpc_mode )
	{
		cpc_mode = true;
		change_clock_rate( cpc_clock );
		set_tempo( tempo() );
	}
}

void ay_cpu_out( Ay_Cpu* cpu, cpu_time_t time, unsigned addr, int data )
{
	Ay_Emu& emu = STATIC_CAST(Ay_Emu&,*cpu);
	
	if ( (addr & 0xFF) == 0xFE && !emu.cpc_mode )
	{
		int delta = emu.beeper_delta;
		data &= 0x10;
		if ( emu.last_beeper != data )
		{
			emu.last_beeper = data;
			emu.beeper_delta = -delta;
			emu.spectrum_mode = true;
			if ( emu.beeper_output )
				emu.apu.synth_.offset( time, delta, emu.beeper_output );
		}
	}
	else
	{
		emu.cpu_out_misc( time, addr, data );
	}
}

int ay_cpu_in( Ay_Cpu*, unsigned addr )
{
	// keyboard read and other things
	if ( (addr & 0xFF) == 0xFE )
		return 0xFF; // other values break some beeper tunes
	
	dprintf( "Unmapped IN : $%04X\n", addr );
	return 0xFF;
}

blargg_err_t Ay_Emu::run_clocks( blip_time_t& duration, int )
{
	set_time( 0 );
	if ( !(spectrum_mode | cpc_mode) )
		duration /= 2; // until mode is set, leave room for halved clock rate
	
	while ( time() < duration )
	{
		cpu::run( min( duration, (blip_time_t) next_play ) );
		
		if ( time() >= next_play )
		{
			next_play += play_period;
			
			if ( r.iff1 )
			{
				if ( mem.ram [r.pc] == 0x76 )
					r.pc++;
				
				r.iff1 = r.iff2 = 0;
				
				mem.ram [--r.sp] = uint8_t (r.pc >> 8);
				mem.ram [--r.sp] = uint8_t (r.pc);
				r.pc = 0x38;
				cpu::adjust_time( 12 );
				if ( r.im == 2 )
				{
					cpu::adjust_time( 6 );
					unsigned addr = r.i * 0x100u + 0xFF;
					r.pc = mem.ram [(addr + 1) & 0xFFFF] * 0x100u + mem.ram [addr];
				}
			}
		}
	}
	duration = time();
	next_play -= duration;
	check( next_play >= 0 );
	adjust_time( -duration );
	
	apu.end_frame( duration );
	
	return 0;
}
