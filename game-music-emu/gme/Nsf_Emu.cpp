// Game_Music_Emu https://bitbucket.org/mpyne/game-music-emu/

#include "Nsf_Emu.h"

#include "blargg_endian.h"
#include <string.h>
#include <stdio.h>

#if !NSF_EMU_APU_ONLY
	#include "Nes_Namco_Apu.h"
	#include "Nes_Vrc6_Apu.h"
	#include "Nes_Fme7_Apu.h"
#endif

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

int const vrc6_flag  = 0x01;
int const namco_flag = 0x10;
int const fme7_flag  = 0x20;

long const clock_divisor = 12;

Nsf_Emu::equalizer_t const Nsf_Emu::nes_eq     =
	Music_Emu::make_equalizer( -1.0, 80 );
Nsf_Emu::equalizer_t const Nsf_Emu::famicom_eq =
	Music_Emu::make_equalizer( -15.0, 80 );

int Nsf_Emu::pcm_read( void* emu, nes_addr_t addr )
{
	return *((Nsf_Emu*) emu)->cpu::get_code( addr );
}

Nsf_Emu::Nsf_Emu()
{
	vrc6  = 0;
	namco = 0;
	fme7  = 0;
	
	set_type( gme_nsf_type );
	set_silence_lookahead( 6 );
	apu.dmc_reader( pcm_read, this );
	Music_Emu::set_equalizer( nes_eq );
	set_gain( 1.4 );
	memset( unmapped_code, Nes_Cpu::bad_opcode, sizeof unmapped_code );
}

Nsf_Emu::~Nsf_Emu() { unload(); }

void Nsf_Emu::unload()
{
	#if !NSF_EMU_APU_ONLY
	{
		delete vrc6;
		vrc6  = 0;
		
		delete namco;
		namco = 0;
		
		delete fme7;
		fme7  = 0;
	}
	#endif
	
	rom.clear();
	Music_Emu::unload();
}

// Track info

static void copy_nsf_fields( Nsf_Emu::header_t const& h, track_info_t* out )
{
	GME_COPY_FIELD( h, out, game );
	GME_COPY_FIELD( h, out, author );
	GME_COPY_FIELD( h, out, copyright );
	if ( h.chip_flags )
		Gme_File::copy_field_( out->system, "Famicom" );
}

blargg_err_t Nsf_Emu::track_info_( track_info_t* out, int ) const
{
	copy_nsf_fields( header_, out );
	return 0;
}

static blargg_err_t check_nsf_header( void const* header )
{
	if ( memcmp( header, "NESM\x1A", 5 ) )
		return gme_wrong_file_type;
	return 0;
}

struct Nsf_File : Gme_Info_
{
	Nsf_Emu::header_t h;
	
	Nsf_File() { set_type( gme_nsf_type ); }
	
	blargg_err_t load_( Data_Reader& in )
	{
		blargg_err_t err = in.read( &h, Nsf_Emu::header_size );
		if ( err )
			return (err == in.eof_error ? gme_wrong_file_type : err);
		
		if ( h.chip_flags & ~(namco_flag | vrc6_flag | fme7_flag) )
			set_warning( "Uses unsupported audio expansion hardware" );
		
		set_track_count( h.track_count );
		return check_nsf_header( &h );
	}
	
	blargg_err_t track_info_( track_info_t* out, int ) const
	{
		copy_nsf_fields( h, out );
		return 0;
	}
};

static Music_Emu* new_nsf_emu () { return BLARGG_NEW Nsf_Emu ; }
static Music_Emu* new_nsf_file() { return BLARGG_NEW Nsf_File; }

static gme_type_t_ const gme_nsf_type_ = { "Nintendo NES", 0, &new_nsf_emu, &new_nsf_file, "NSF", 1 };
BLARGG_EXPORT extern gme_type_t const gme_nsf_type = &gme_nsf_type_;


// Setup

void Nsf_Emu::set_tempo_( double t )
{
	unsigned playback_rate = get_le16( header_.ntsc_speed );
	unsigned standard_rate = 0x411A;
	clock_rate_ = 1789772.72727;
	play_period = 262 * 341L * 4 - 2; // two fewer PPU clocks every four frames
	
	if ( pal_only )
	{
		play_period   = 33247 * clock_divisor;
		clock_rate_   = 1662607.125;
		standard_rate = 0x4E20;
		playback_rate = get_le16( header_.pal_speed );
	}
	
	if ( !playback_rate )
		playback_rate = standard_rate;
	
	if ( playback_rate != standard_rate || t != 1.0 )
		play_period = long (playback_rate * clock_rate_ / (1000000.0 / clock_divisor * t));

	apu.set_tempo( t );
}

blargg_err_t Nsf_Emu::init_sound()
{
	if ( header_.chip_flags & ~(namco_flag | vrc6_flag | fme7_flag) )
		set_warning( "Uses unsupported audio expansion hardware" );
	
	{
		#define APU_NAMES "Square 1", "Square 2", "Triangle", "Noise", "DMC"
		
		int const count = Nes_Apu::osc_count;
		static const char* const apu_names [count] = { APU_NAMES };
		set_voice_count( count );
		set_voice_names( apu_names );
		
	}
	
	static int const types [] = {
		wave_type  | 1, wave_type  | 2, wave_type | 0,
		noise_type | 0, mixed_type | 1,
		wave_type  | 3, wave_type  | 4, wave_type | 5,
		wave_type  | 6, wave_type  | 7, wave_type | 8, wave_type | 9,
		wave_type  |10, wave_type  |11, wave_type |12, wave_type |13
	};
	set_voice_types( types ); // common to all sound chip configurations
	
	double adjusted_gain = gain();
	
	#if NSF_EMU_APU_ONLY
	{
		if ( header_.chip_flags )
			set_warning( "Uses unsupported audio expansion hardware" );
	}
	#else
	{
		if ( header_.chip_flags & (namco_flag | vrc6_flag | fme7_flag) )
			set_voice_count( Nes_Apu::osc_count + 3 );
		
		if ( header_.chip_flags & namco_flag )
		{
			namco = BLARGG_NEW Nes_Namco_Apu;
			CHECK_ALLOC( namco );
			adjusted_gain *= 0.75;
			
			int const count = Nes_Apu::osc_count + Nes_Namco_Apu::osc_count;
			static const char* const names [count] = {
				APU_NAMES,
				"Wave 1", "Wave 2", "Wave 3", "Wave 4",
				"Wave 5", "Wave 6", "Wave 7", "Wave 8"
			};
			set_voice_count( count );
			set_voice_names( names );
		}
		
		if ( header_.chip_flags & vrc6_flag )
		{
			vrc6 = BLARGG_NEW Nes_Vrc6_Apu;
			CHECK_ALLOC( vrc6 );
			adjusted_gain *= 0.75;
			
			{
				int const count = Nes_Apu::osc_count + Nes_Vrc6_Apu::osc_count;
				static const char* const names [count] = {
					APU_NAMES,
					"Saw Wave", "Square 3", "Square 4"
				};
				set_voice_count( count );
				set_voice_names( names );
			}
			
			if ( header_.chip_flags & namco_flag )
			{
				int const count = Nes_Apu::osc_count + Nes_Vrc6_Apu::osc_count +
						Nes_Namco_Apu::osc_count;
				static const char* const names [count] = {
					APU_NAMES,
					"Saw Wave", "Square 3", "Square 4",
					"Wave 1", "Wave 2", "Wave 3", "Wave 4",
					"Wave 5", "Wave 6", "Wave 7", "Wave 8"
				};
				set_voice_count( count );
				set_voice_names( names );
			}
		}
		
		if ( header_.chip_flags & fme7_flag )
		{
			fme7 = BLARGG_NEW Nes_Fme7_Apu;
			CHECK_ALLOC( fme7 );
			adjusted_gain *= 0.75;
			
			int const count = Nes_Apu::osc_count + Nes_Fme7_Apu::osc_count;
			static const char* const names [count] = {
				APU_NAMES,
				"Square 3", "Square 4", "Square 5"
			};
			set_voice_count( count );
			set_voice_names( names );
		}
		
		if ( namco ) namco->volume( adjusted_gain );
		if ( vrc6  ) vrc6 ->volume( adjusted_gain );
		if ( fme7  ) fme7 ->volume( adjusted_gain );
	}
	#endif
	
	apu.volume( adjusted_gain );
	
	return 0;
}

blargg_err_t Nsf_Emu::load_( Data_Reader& in )
{
	assert( offsetof (header_t,unused [4]) == header_size );
	RETURN_ERR( rom.load( in, header_size, &header_, 0 ) );
	
	set_track_count( header_.track_count );
	RETURN_ERR( check_nsf_header( &header_ ) );
	
	if ( header_.vers != 1 )
		set_warning( "Unknown file version" );
	
	// sound and memory
	blargg_err_t err = init_sound();
	if ( err )
		return err;
	
	// set up data
	nes_addr_t load_addr = get_le16( header_.load_addr );
	init_addr = get_le16( header_.init_addr );
	play_addr = get_le16( header_.play_addr );
	if ( !load_addr ) load_addr = rom_begin;
	if ( !init_addr ) init_addr = rom_begin;
	if ( !play_addr ) play_addr = rom_begin;
	if ( load_addr < rom_begin || init_addr < rom_begin )
	{
		const char* w = warning();
		if ( !w )
			w = "Corrupt file (invalid load/init/play address)";
		return w;
	}
	
	rom.set_addr( load_addr % bank_size );
	int total_banks = rom.size() / bank_size;
	
	// bank switching
	int first_bank = (load_addr - rom_begin) / bank_size;
	for ( int i = 0; i < bank_count; i++ )
	{
		unsigned bank = i - first_bank;
		if ( bank >= (unsigned) total_banks )
			bank = 0;
		initial_banks [i] = bank;
		
		if ( header_.banks [i] )
		{
			// bank-switched
			memcpy( initial_banks, header_.banks, sizeof initial_banks );
			break;
		}
	}
	
	pal_only = (header_.speed_flags & 3) == 1;
	
	#if !NSF_EMU_EXTRA_FLAGS
		header_.speed_flags = 0;
	#endif
	
	set_tempo( tempo() );
	
	return setup_buffer( (long) (clock_rate_ + 0.5) );
}

void Nsf_Emu::update_eq( blip_eq_t const& eq )
{
	apu.treble_eq( eq );
	
	#if !NSF_EMU_APU_ONLY
	{
		if ( namco ) namco->treble_eq( eq );
		if ( vrc6  ) vrc6 ->treble_eq( eq );
		if ( fme7  ) fme7 ->treble_eq( eq );
	}
	#endif
}

void Nsf_Emu::set_voice( int i, Blip_Buffer* buf, Blip_Buffer*, Blip_Buffer* )
{
	if ( i < Nes_Apu::osc_count )
	{
		apu.osc_output( i, buf );
		return;
	}
	i -= Nes_Apu::osc_count;
	
	#if !NSF_EMU_APU_ONLY
	{
		if ( fme7 && i < Nes_Fme7_Apu::osc_count )
		{
			fme7->osc_output( i, buf );
			return;
		}
		
		if ( vrc6 )
		{
			if ( i < Nes_Vrc6_Apu::osc_count )
			{
				// put saw first
				if ( --i < 0 )
					i = 2;
				vrc6->osc_output( i, buf );
				return;
			}
			i -= Nes_Vrc6_Apu::osc_count;
		}
		
		if ( namco && i < Nes_Namco_Apu::osc_count )
		{
			namco->osc_output( i, buf );
			return;
		}
	}
	#endif
}

// Emulation

// see nes_cpu_io.h for read/write functions

void Nsf_Emu::cpu_write_misc( nes_addr_t addr, int data )
{
	#if !NSF_EMU_APU_ONLY
	{
		if ( namco )
		{
			switch ( addr )
			{
			case Nes_Namco_Apu::data_reg_addr:
				namco->write_data( time(), data );
				return;
			
			case Nes_Namco_Apu::addr_reg_addr:
				namco->write_addr( data );
				return;
			}
		}
		
		if ( addr >= Nes_Fme7_Apu::latch_addr && fme7 )
		{
			switch ( addr & Nes_Fme7_Apu::addr_mask )
			{
			case Nes_Fme7_Apu::latch_addr:
				fme7->write_latch( data );
				return;
			
			case Nes_Fme7_Apu::data_addr:
				fme7->write_data( time(), data );
				return;
			}
		}
		
		if ( vrc6 )
		{
			unsigned reg = addr & (Nes_Vrc6_Apu::addr_step - 1);
			unsigned osc = unsigned (addr - Nes_Vrc6_Apu::base_addr) / Nes_Vrc6_Apu::addr_step;
			if ( osc < Nes_Vrc6_Apu::osc_count && reg < Nes_Vrc6_Apu::reg_count )
			{
				vrc6->write_osc( time(), osc, reg, data );
				return;
			}
		}
	}
	#endif
	
	// unmapped write
	
	#ifndef NDEBUG
	{
		// some games write to $8000 and $8001 repeatedly
		if ( addr == 0x8000 || addr == 0x8001 ) return;
		
		// probably namco sound mistakenly turned on in mck
		if ( addr == 0x4800 || addr == 0xF800 ) return;
		
		// memory mapper?
		if ( addr == 0xFFF8 ) return;
		
		debug_printf( "write_unmapped( 0x%04X, 0x%02X )\n", (unsigned) addr, (unsigned) data );
	}
	#endif
}

blargg_err_t Nsf_Emu::start_track_( int track )
{
	RETURN_ERR( Classic_Emu::start_track_( track ) );
	
	memset( low_mem, 0, sizeof low_mem );
	memset( sram,    0, sizeof sram );
	
	cpu::reset( unmapped_code ); // also maps low_mem
	cpu::map_code( sram_addr, sizeof sram, sram );
	for ( int i = 0; i < bank_count; ++i )
		cpu_write( bank_select_addr + i, initial_banks [i] );
	
	apu.reset( pal_only, (header_.speed_flags & 0x20) ? 0x3F : 0 );
	apu.write_register( 0, 0x4015, 0x0F );
	apu.write_register( 0, 0x4017, (header_.speed_flags & 0x10) ? 0x80 : 0 );
	#if !NSF_EMU_APU_ONLY
	{
		if ( namco ) namco->reset();
		if ( vrc6  ) vrc6 ->reset();
		if ( fme7  ) fme7 ->reset();
	}
	#endif
	
	play_ready = 4;
	play_extra = 0;
	next_play = play_period / clock_divisor;
	
	saved_state.pc = badop_addr;
	low_mem [0x1FF] = (badop_addr - 1) >> 8;
	low_mem [0x1FE] = (badop_addr - 1) & 0xFF;
	r.sp = 0xFD;
	r.pc = init_addr;
	r.a  = track;
	r.x  = pal_only;
	
	return 0;
}

blargg_err_t Nsf_Emu::run_clocks( blip_time_t& duration, int )
{
	set_time( 0 );
	while ( time() < duration )
	{
		nes_time_t end = min( (blip_time_t) next_play, duration );
		end = min( end, time() + 32767 ); // allows CPU to use 16-bit time delta
		if ( cpu::run( end ) )
		{
			if ( r.pc != badop_addr )
			{
				set_warning( "Emulation error (illegal instruction)" );
				r.pc++;
			}
			else
			{
				play_ready = 1;
				if ( saved_state.pc != badop_addr )
				{
					cpu::r = saved_state;
					saved_state.pc = badop_addr;
				}
				else
				{
					set_time( end );
				}
			}
		}
		
		if ( time() >= next_play )
		{
			nes_time_t period = (play_period + play_extra) / clock_divisor;
			play_extra = play_period - period * clock_divisor;
			next_play += period;
			if ( play_ready && !--play_ready )
			{
				check( saved_state.pc == badop_addr );
				if ( r.pc != badop_addr )
					saved_state = cpu::r;
				
				r.pc = play_addr;
				low_mem [0x100 + r.sp--] = (badop_addr - 1) >> 8;
				low_mem [0x100 + r.sp--] = (badop_addr - 1) & 0xFF;
				GME_FRAME_HOOK( this );
			}
		}
	}
	
	if ( cpu::error_count() )
	{
		cpu::clear_error_count();
		set_warning( "Emulation error (illegal instruction)" );
	}
	
	duration = time();
	next_play -= duration;
	check( next_play >= 0 );
	if ( next_play < 0 )
		next_play = 0;
	
	apu.end_frame( duration );
	
	#if !NSF_EMU_APU_ONLY
	{
		if ( namco ) namco->end_frame( duration );
		if ( vrc6  ) vrc6 ->end_frame( duration );
		if ( fme7  ) fme7 ->end_frame( duration );
	}
	#endif
	
	return 0;
}
