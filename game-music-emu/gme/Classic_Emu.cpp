// Game_Music_Emu 0.5.2. http://www.slack.net/~ant/

#include "Classic_Emu.h"

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

Classic_Emu::Classic_Emu()
{
	buf           = 0;
	stereo_buffer = 0;
	voice_types   = 0;
	
	// avoid inconsistency in our duplicated constants
	assert( (int) wave_type  == (int) Multi_Buffer::wave_type );
	assert( (int) noise_type == (int) Multi_Buffer::noise_type );
	assert( (int) mixed_type == (int) Multi_Buffer::mixed_type );
}

Classic_Emu::~Classic_Emu()
{
	delete stereo_buffer;
}

void Classic_Emu::set_equalizer_( equalizer_t const& eq )
{
	Music_Emu::set_equalizer_( eq );
	update_eq( eq.treble );
	if ( buf )
		buf->bass_freq( (int) equalizer().bass );
}
	
blargg_err_t Classic_Emu::set_sample_rate_( long rate )
{
	if ( !buf )
	{
		if ( !stereo_buffer )
			CHECK_ALLOC( stereo_buffer = BLARGG_NEW Stereo_Buffer );
		buf = stereo_buffer;
	}
	return buf->set_sample_rate( rate, 1000 / 20 );
}

void Classic_Emu::mute_voices_( int mask )
{
	Music_Emu::mute_voices_( mask );
	for ( int i = voice_count(); i--; )
	{
		if ( mask & (1 << i) )
		{
			set_voice( i, 0, 0, 0 );
		}
		else
		{
			Multi_Buffer::channel_t ch = buf->channel( i, (voice_types ? voice_types [i] : 0) );
			assert( (ch.center && ch.left && ch.right) ||
					(!ch.center && !ch.left && !ch.right) ); // all or nothing
			set_voice( i, ch.center, ch.left, ch.right );
		}
	}
}

void Classic_Emu::change_clock_rate( long rate )
{
	clock_rate_ = rate;
	buf->clock_rate( rate );
}

blargg_err_t Classic_Emu::setup_buffer( long rate )
{
	change_clock_rate( rate );
	RETURN_ERR( buf->set_channel_count( voice_count() ) );
	set_equalizer( equalizer() );
	buf_changed_count = buf->channels_changed_count();
	return 0;
}

blargg_err_t Classic_Emu::start_track_( int track )
{
	RETURN_ERR( Music_Emu::start_track_( track ) );
	buf->clear();
	return 0;
}

blargg_err_t Classic_Emu::play_( long count, sample_t* out )
{
	long remain = count;
	while ( remain )
	{
		remain -= buf->read_samples( &out [count - remain], remain );
		if ( remain )
		{
			if ( buf_changed_count != buf->channels_changed_count() )
			{
				buf_changed_count = buf->channels_changed_count();
				remute_voices();
			}
			int msec = buf->length();
			blip_time_t clocks_emulated = (blargg_long) msec * clock_rate_ / 1000;
			RETURN_ERR( run_clocks( clocks_emulated, msec ) );
			assert( clocks_emulated );
			buf->end_frame( clocks_emulated );
		}
	}
	return 0;
}

// Rom_Data

blargg_err_t Rom_Data_::load_rom_data_( Data_Reader& in,
		int header_size, void* header_out, int fill, long pad_size )
{
	long file_offset = pad_size - header_size;
	
	rom_addr = 0;
	mask     = 0;
	size_    = 0;
	rom.clear();
	
	file_size_ = in.remain();
	if ( file_size_ <= header_size ) // <= because there must be data after header
		return gme_wrong_file_type;
	blargg_err_t err = rom.resize( file_offset + file_size_ + pad_size );
	if ( !err )
		err = in.read( rom.begin() + file_offset, file_size_ );
	if ( err )
	{
		rom.clear();
		return err;
	}
	
	file_size_ -= header_size;
	memcpy( header_out, &rom [file_offset], header_size );
	
	memset( rom.begin()         , fill, pad_size );
	memset( rom.end() - pad_size, fill, pad_size );
	
	return 0;
}

void Rom_Data_::set_addr_( long addr, int unit )
{
	rom_addr = addr - unit - pad_extra;
	
	long rounded = (addr + file_size_ + unit - 1) / unit * unit;
	if ( rounded <= 0 )
	{
		rounded = 0;
	}
	else
	{
		int shift = 0;
		unsigned long max_addr = (unsigned long) (rounded - 1);
		while ( max_addr >> shift )
			shift++;
		mask = (1L << shift) - 1;
	}
	
	if ( addr < 0 )
		addr = 0;
	size_ = rounded;
	if ( rom.resize( rounded - rom_addr + pad_extra ) ) { } // OK if shrink fails

	if ( 0 )
	{
		dprintf( "addr: %X\n", addr );
		dprintf( "file_size: %d\n", file_size_ );
		dprintf( "rounded: %d\n", rounded );
		dprintf( "mask: $%X\n", mask );
	}
}
