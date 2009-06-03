// Game_Music_Emu 0.5.2. http://www.slack.net/~ant/

#include "Gbs_Emu.h"

#include "blargg_endian.h"
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

Gbs_Emu::equalizer_t const Gbs_Emu::handheld_eq   = { -47.0, 2000 };
Gbs_Emu::equalizer_t const Gbs_Emu::headphones_eq = {   0.0,  300 };

Gbs_Emu::Gbs_Emu()
{
	set_type( gme_gbs_type );
	
	static const char* const names [Gb_Apu::osc_count] = {
		"Square 1", "Square 2", "Wave", "Noise"
	};
	set_voice_names( names );
	
	static int const types [Gb_Apu::osc_count] = {
		wave_type | 1, wave_type | 2, wave_type | 0, mixed_type | 0
	};
	set_voice_types( types );
	
	set_silence_lookahead( 6 );
	set_max_initial_silence( 21 );
	set_gain( 1.2 );
	
	static equalizer_t const eq = { -1.0, 120 };
	set_equalizer( eq );
}

Gbs_Emu::~Gbs_Emu() { }

void Gbs_Emu::unload()
{
	rom.clear();
	Music_Emu::unload();
}

// Track info

static void copy_gbs_fields( Gbs_Emu::header_t const& h, track_info_t* out )
{
	GME_COPY_FIELD( h, out, game );
	GME_COPY_FIELD( h, out, author );
	GME_COPY_FIELD( h, out, copyright );
}

blargg_err_t Gbs_Emu::track_info_( track_info_t* out, int ) const
{
	copy_gbs_fields( header_, out );
	return 0;
}

static blargg_err_t check_gbs_header( void const* header )
{
	if ( memcmp( header, "GBS", 3 ) )
		return gme_wrong_file_type;
	return 0;
}

struct Gbs_File : Gme_Info_
{
	Gbs_Emu::header_t h;
	
	Gbs_File() { set_type( gme_gbs_type ); }
	
	blargg_err_t load_( Data_Reader& in )
	{
		blargg_err_t err = in.read( &h, Gbs_Emu::header_size );
		if ( err )
			return (err == in.eof_error ? gme_wrong_file_type : err);
		
		set_track_count( h.track_count );
		return check_gbs_header( &h );
	}
	
	blargg_err_t track_info_( track_info_t* out, int ) const
	{
		copy_gbs_fields( h, out );
		return 0;
	}
};

static Music_Emu* new_gbs_emu () { return BLARGG_NEW Gbs_Emu ; }
static Music_Emu* new_gbs_file() { return BLARGG_NEW Gbs_File; }

static gme_type_t_ const gme_gbs_type_ = { "Game Boy", 0, &new_gbs_emu, &new_gbs_file, "GBS", 1 };
gme_type_t const gme_gbs_type = &gme_gbs_type_;

// Setup

blargg_err_t Gbs_Emu::load_( Data_Reader& in )
{
	assert( offsetof (header_t,copyright [32]) == header_size );
	RETURN_ERR( rom.load( in, header_size, &header_, 0 ) );
	
	set_track_count( header_.track_count );
	RETURN_ERR( check_gbs_header( &header_ ) );
	
	if ( header_.vers != 1 )
		set_warning( "Unknown file version" );
	
	if ( header_.timer_mode & 0x78 )
		set_warning( "Invalid timer mode" );
	
	unsigned load_addr = get_le16( header_.load_addr );
	if ( (header_.load_addr [1] | header_.init_addr [1] | header_.play_addr [1]) > 0x7F ||
			load_addr < 0x400 )
		set_warning( "Invalid load/init/play address" );
	
	set_voice_count( Gb_Apu::osc_count );
	
	apu.volume( gain() );
	
	return setup_buffer( 4194304 );
}

void Gbs_Emu::update_eq( blip_eq_t const& eq )
{
	apu.treble_eq( eq );
}

void Gbs_Emu::set_voice( int i, Blip_Buffer* c, Blip_Buffer* l, Blip_Buffer* r )
{
	apu.osc_output( i, c, l, r );
}

// Emulation

// see gb_cpu_io.h for read/write functions

void Gbs_Emu::set_bank( int n )
{
	blargg_long addr = rom.mask_addr( n * (blargg_long) bank_size );
	if ( addr == 0 && rom.size() > bank_size )
	{
		// TODO: what is the correct behavior? Current Game & Watch Gallery
		// rip requires that this have no effect or set to bank 1.
		//dprintf( "Selected ROM bank 0\n" );
		return;
		//n = 1;
	}
	cpu::map_code( bank_size, bank_size, rom.at_addr( addr ) );
}

void Gbs_Emu::update_timer()
{
	if ( header_.timer_mode & 0x04 )
	{
		static byte const rates [4] = { 10, 4, 6, 8 };
		int shift = rates [ram [hi_page + 7] & 3] - (header_.timer_mode >> 7);
		play_period = (256L - ram [hi_page + 6]) << shift;
	}
	else
	{
		play_period = 70224; // 59.73 Hz
	}
	if ( tempo() != 1.0 )
		play_period = blip_time_t (play_period / tempo());
}

static BOOST::uint8_t const sound_data [Gb_Apu::register_count] = {
	0x80, 0xBF, 0x00, 0x00, 0xBF, // square 1
	0x00, 0x3F, 0x00, 0x00, 0xBF, // square 2
	0x7F, 0xFF, 0x9F, 0x00, 0xBF, // wave
	0x00, 0xFF, 0x00, 0x00, 0xBF, // noise
	0x77, 0xF3, 0xF1, // vin/volume, status, power mode
	0, 0, 0, 0, 0, 0, 0, 0, 0, // unused
	0xAC, 0xDD, 0xDA, 0x48, 0x36, 0x02, 0xCF, 0x16, // waveform data
	0x2C, 0x04, 0xE5, 0x2C, 0xAC, 0xDD, 0xDA, 0x48
};

void Gbs_Emu::cpu_jsr( gb_addr_t addr )
{
	check( cpu::r.sp == get_le16( header_.stack_ptr ) );
	cpu::r.pc = addr;
	cpu_write( --cpu::r.sp, idle_addr >> 8 );
	cpu_write( --cpu::r.sp, idle_addr&0xFF );
}

void Gbs_Emu::set_tempo_( double t )
{
	apu.set_tempo( t );
	update_timer();
}

blargg_err_t Gbs_Emu::start_track_( int track )
{
	RETURN_ERR( Classic_Emu::start_track_( track ) );
	
	memset( ram, 0, 0x4000 );
	memset( ram + 0x4000, 0xFF, 0x1F80 );
	memset( ram + 0x5F80, 0, sizeof ram - 0x5F80 );
	ram [hi_page] = 0; // joypad reads back as 0
	
	apu.reset();
	for ( int i = 0; i < (int) sizeof sound_data; i++ )
		apu.write_register( 0, i + apu.start_addr, sound_data [i] );
	
	cpu::reset( rom.unmapped() );
	
	unsigned load_addr = get_le16( header_.load_addr );
	cpu::rst_base = load_addr;
	rom.set_addr( load_addr );
	
	cpu::map_code( ram_addr, 0x10000 - ram_addr, ram );
	cpu::map_code( 0, bank_size, rom.at_addr( 0 ) );
	set_bank( rom.size() > bank_size );
	
	ram [hi_page + 6] = header_.timer_modulo;
	ram [hi_page + 7] = header_.timer_mode;
	update_timer();
	next_play = play_period;
	
	cpu::r.a  = track;
	cpu::r.pc = idle_addr;
	cpu::r.sp = get_le16( header_.stack_ptr );
	cpu_time  = 0;
	cpu_jsr( get_le16( header_.init_addr ) );
	
	return 0;
}

blargg_err_t Gbs_Emu::run_clocks( blip_time_t& duration, int )
{
	cpu_time = 0;
	while ( cpu_time < duration )
	{
		long count = duration - cpu_time;
		cpu_time = duration;
		bool result = cpu::run( count );
		cpu_time -= cpu::remain();
		
		if ( result )
		{
			if ( cpu::r.pc == idle_addr )
			{
				if ( next_play > duration )
				{
					cpu_time = duration;
					break;
				}
				
				if ( cpu_time < next_play )
					cpu_time = next_play;
				next_play += play_period;
				cpu_jsr( get_le16( header_.play_addr ) );
				GME_FRAME_HOOK( this );
				// TODO: handle timer rates different than 60 Hz
			}
			else if ( cpu::r.pc > 0xFFFF )
			{
				dprintf( "PC wrapped around\n" );
				cpu::r.pc &= 0xFFFF;
			}
			else
			{
				set_warning( "Emulation error (illegal/unsupported instruction)" );
				dprintf( "Bad opcode $%.2x at $%.4x\n",
						(int) *cpu::get_code( cpu::r.pc ), (int) cpu::r.pc );
				cpu::r.pc = (cpu::r.pc + 1) & 0xFFFF;
				cpu_time += 6;
			}
		}
	}
	
	duration = cpu_time;
	next_play -= cpu_time;
	if ( next_play < 0 ) // could go negative if routine is taking too long to return
		next_play = 0;
	apu.end_frame( cpu_time );
	
	return 0;
}
