// Game_Music_Emu https://bitbucket.org/mpyne/game-music-emu/

#include "Kss_Emu.h"

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

long const clock_rate = 3579545;
int const osc_count = Ay_Apu::osc_count + Scc_Apu::osc_count;

Kss_Emu::Kss_Emu()
{
	sn = 0;
	set_type( gme_kss_type );
	set_silence_lookahead( 6 );
	static const char* const names [osc_count] = {
		"Square 1", "Square 2", "Square 3",
		"Wave 1", "Wave 2", "Wave 3", "Wave 4", "Wave 5"
	};
	set_voice_names( names );
	
	static int const types [osc_count] = {
		wave_type | 0, wave_type | 1, wave_type | 2,
		wave_type | 3, wave_type | 4, wave_type | 5, wave_type | 6, wave_type | 7
	};
	set_voice_types( types );
	
	memset( unmapped_read, 0xFF, sizeof unmapped_read );
}

Kss_Emu::~Kss_Emu() { unload(); }

void Kss_Emu::unload()
{
	delete sn;
	sn = 0;
	Classic_Emu::unload();
}

// Track info

static void copy_kss_fields( Kss_Emu::header_t const& h, track_info_t* out )
{
	const char* system = "MSX";
	if ( h.device_flags & 0x02 )
	{
		system = "Sega Master System";
		if ( h.device_flags & 0x04 )
			system = "Game Gear";
	}
	Gme_File::copy_field_( out->system, system );
}

blargg_err_t Kss_Emu::track_info_( track_info_t* out, int ) const
{
	copy_kss_fields( header_, out );
	return 0;
}

static blargg_err_t check_kss_header( void const* header )
{
	if ( memcmp( header, "KSCC", 4 ) && memcmp( header, "KSSX", 4 ) )
		return gme_wrong_file_type;
	return 0;
}

struct Kss_File : Gme_Info_
{
	Kss_Emu::header_t header_;
	
	Kss_File() { set_type( gme_kss_type ); }
	
	blargg_err_t load_( Data_Reader& in )
	{
		blargg_err_t err = in.read( &header_, Kss_Emu::header_size );
		if ( err )
			return (err == in.eof_error ? gme_wrong_file_type : err);
		return check_kss_header( &header_ );
	}
	
	blargg_err_t track_info_( track_info_t* out, int ) const
	{
		copy_kss_fields( header_, out );
		return 0;
	}
};

static Music_Emu* new_kss_emu () { return BLARGG_NEW Kss_Emu ; }
static Music_Emu* new_kss_file() { return BLARGG_NEW Kss_File; }

static gme_type_t_ const gme_kss_type_ = { "MSX", 256, &new_kss_emu, &new_kss_file, "KSS", 0x03 };
BLARGG_EXPORT extern gme_type_t const gme_kss_type = &gme_kss_type_;


// Setup

void Kss_Emu::update_gain()
{
	double g = gain() * 1.4;
	if ( scc_accessed )
		g *= 1.5;
	ay.volume( g );
	scc.volume( g );
	if ( sn )
		sn->volume( g );
}

blargg_err_t Kss_Emu::load_( Data_Reader& in )
{
	memset( &header_, 0, sizeof header_ );
	assert( offsetof (header_t,device_flags) == header_size - 1 );
	assert( offsetof (ext_header_t,msx_audio_vol) == ext_header_size - 1 );
	RETURN_ERR( rom.load( in, header_size, STATIC_CAST(header_t*,&header_), 0 ) );
	
	RETURN_ERR( check_kss_header( header_.tag ) );
	
	if ( header_.tag [3] == 'C' )
	{
		if ( header_.extra_header )
		{
			header_.extra_header = 0;
			set_warning( "Unknown data in header" );
		}
		if ( header_.device_flags & ~0x0F )
		{
			header_.device_flags &= 0x0F;
			set_warning( "Unknown data in header" );
		}
	}
	else
	{
		ext_header_t& ext = header_;
		memcpy( &ext, rom.begin(), min( (int) ext_header_size, (int) header_.extra_header ) );
		if ( header_.extra_header > 0x10 )
			set_warning( "Unknown data in header" );
	}
	
	if ( header_.device_flags & 0x09 )
		set_warning( "FM sound not supported" );
	
	scc_enabled = 0xC000;
	if ( header_.device_flags & 0x04 )
		scc_enabled = 0;
	
	if ( header_.device_flags & 0x02 && !sn )
		CHECK_ALLOC( sn = BLARGG_NEW( Sms_Apu ) );
	
	set_voice_count( osc_count );
	
	return setup_buffer( ::clock_rate );
}

void Kss_Emu::update_eq( blip_eq_t const& eq )
{
	ay.treble_eq( eq );
	scc.treble_eq( eq );
	if ( sn )
		sn->treble_eq( eq );
}

void Kss_Emu::set_voice( int i, Blip_Buffer* center, Blip_Buffer* left, Blip_Buffer* right )
{
	int i2 = i - ay.osc_count;
	if ( i2 >= 0 )
		scc.osc_output( i2, center );
	else
		ay.osc_output( i, center );
	if ( sn && i < sn->osc_count )
		sn->osc_output( i, center, left, right );
}

// Emulation

void Kss_Emu::set_tempo_( double t )
{
	blip_time_t period =
			(header_.device_flags & 0x40 ? ::clock_rate / 50 : ::clock_rate / 60);
	play_period = blip_time_t (period / t);
}

blargg_err_t Kss_Emu::start_track_( int track )
{
	RETURN_ERR( Classic_Emu::start_track_( track ) );

	memset( ram, 0xC9, 0x4000 );
	memset( ram + 0x4000, 0, sizeof ram - 0x4000 );
	
	// copy driver code to lo RAM
	static byte const bios [] = {
		0xD3, 0xA0, 0xF5, 0x7B, 0xD3, 0xA1, 0xF1, 0xC9, // $0001: WRTPSG
		0xD3, 0xA0, 0xDB, 0xA2, 0xC9                    // $0009: RDPSG
	};
	static byte const vectors [] = {
		0xC3, 0x01, 0x00,   // $0093: WRTPSG vector
		0xC3, 0x09, 0x00,   // $0096: RDPSG vector
	};
	memcpy( ram + 0x01, bios,    sizeof bios );
	memcpy( ram + 0x93, vectors, sizeof vectors );
	
	// copy non-banked data into RAM
	unsigned load_addr = get_le16( header_.load_addr );
	long orig_load_size = get_le16( header_.load_size );
	long load_size = min( orig_load_size, rom.file_size() );
	load_size = min( load_size, long (mem_size - load_addr) );
	if ( load_size != orig_load_size )
		set_warning( "Excessive data size" );
	memcpy( ram + load_addr, rom.begin() + header_.extra_header, load_size );
	
	rom.set_addr( -load_size - header_.extra_header );
	
	// check available bank data
	blargg_long const bank_size = this->bank_size();
	int max_banks = (rom.file_size() - load_size + bank_size - 1) / bank_size;
	bank_count = header_.bank_mode & 0x7F;
	if ( bank_count > max_banks )
	{
		bank_count = max_banks;
		set_warning( "Bank data missing" );
	}
	//debug_printf( "load_size : $%X\n", load_size );
	//debug_printf( "bank_size : $%X\n", bank_size );
	//debug_printf( "bank_count: %d (%d claimed)\n", bank_count, header_.bank_mode & 0x7F );
	
	ram [idle_addr] = 0xFF;
	cpu::reset( unmapped_write, unmapped_read );
	cpu::map_mem( 0, mem_size, ram, ram );
	
	ay.reset();
	scc.reset();
	if ( sn )
		sn->reset();
	r.sp = 0xF380;
	ram [--r.sp] = idle_addr >> 8;
	ram [--r.sp] = idle_addr & 0xFF;
	r.b.a = track;
	r.pc = get_le16( header_.init_addr );
	next_play = play_period;
	scc_accessed = false;
	gain_updated = false;
	update_gain();
	ay_latch = 0;
	
	return 0;
}

void Kss_Emu::set_bank( int logical, int physical )
{
	unsigned const bank_size = this->bank_size();
	
	unsigned addr = 0x8000;
	if ( logical && bank_size == 8 * 1024 )
		addr = 0xA000;
	
	physical -= header_.first_bank;
	if ( (unsigned) physical >= (unsigned) bank_count )
	{
		byte* data = ram + addr;
		cpu::map_mem( addr, bank_size, data, data );
	}
	else
	{
		long phys = physical * (blargg_long) bank_size;
		for ( unsigned offset = 0; offset < bank_size; offset += page_size )
			cpu::map_mem( addr + offset, page_size,
					unmapped_write, rom.at_addr( phys + offset ) );
	}
}

void Kss_Emu::cpu_write( unsigned addr, int data )
{
	data &= 0xFF;
	switch ( addr )
	{
	case 0x9000:
		set_bank( 0, data );
		return;
	
	case 0xB000:
		set_bank( 1, data );
		return;
	}
	
	int scc_addr = (addr & 0xDFFF) ^ 0x9800;
	if ( scc_addr < scc.reg_count )
	{
		scc_accessed = true;
		scc.write( time(), scc_addr, data );
		return;
	}
	
	debug_printf( "LD ($%04X),$%02X\n", addr, data );
}

void kss_cpu_write( Kss_Cpu* cpu, unsigned addr, int data )
{
	*cpu->write( addr ) = data;
	if ( (addr & STATIC_CAST(Kss_Emu&,*cpu).scc_enabled) == 0x8000 )
		STATIC_CAST(Kss_Emu&,*cpu).cpu_write( addr, data );
}

void kss_cpu_out( Kss_Cpu* cpu, cpu_time_t time, unsigned addr, int data )
{
	data &= 0xFF;
	Kss_Emu& emu = STATIC_CAST(Kss_Emu&,*cpu);
	switch ( addr & 0xFF )
	{
	case 0xA0:
		emu.ay_latch = data & 0x0F;
		return;
	
	case 0xA1:
		GME_APU_HOOK( &emu, emu.ay_latch, data );
		emu.ay.write( time, emu.ay_latch, data );
		return;
	
	case 0x06:
		if ( emu.sn && (emu.header_.device_flags & 0x04) )
		{
			emu.sn->write_ggstereo( time, data );
			return;
		}
		break;
	
	case 0x7E:
	case 0x7F:
		if ( emu.sn )
		{
			GME_APU_HOOK( &emu, 16, data );
			emu.sn->write_data( time, data );
			return;
		}
		break;
	
	case 0xFE:
		emu.set_bank( 0, data );
		return;
	
	#ifndef NDEBUG
	case 0xF1: // FM data
		if ( data )
			break; // trap non-zero data
	case 0xF0: // FM addr
	case 0xA8: // PPI
		return;
	#endif
	}
	
	debug_printf( "OUT $%04X,$%02X\n", addr, data );
}

int kss_cpu_in( Kss_Cpu*, cpu_time_t, unsigned addr )
{
	//Kss_Emu& emu = STATIC_CAST(Kss_Emu&,*cpu);
	//switch ( addr & 0xFF )
	//{
	//}
	
	debug_printf( "IN $%04X\n", addr );
	return 0;
}

// Emulation

blargg_err_t Kss_Emu::run_clocks( blip_time_t& duration, int )
{
	while ( time() < duration )
	{
		blip_time_t end = min( duration, next_play );
		cpu::run( min( duration, next_play ) );
		if ( r.pc == idle_addr )
			set_time( end );
		
		if ( time() >= next_play )
		{
			next_play += play_period;
			if ( r.pc == idle_addr )
			{
				if ( !gain_updated )
				{
					gain_updated = true;
					if ( scc_accessed )
						update_gain();
				}
				
				ram [--r.sp] = idle_addr >> 8;
				ram [--r.sp] = idle_addr & 0xFF;
				r.pc = get_le16( header_.play_addr );
				GME_FRAME_HOOK( this );
			}
		}
	}
	
	duration = time();
	next_play -= duration;
	check( next_play >= 0 );
	adjust_time( -duration );
	ay.end_frame( duration );
	scc.end_frame( duration );
	if ( sn )
		sn->end_frame( duration );
	
	return 0;
}
