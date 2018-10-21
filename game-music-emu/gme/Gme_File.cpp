// Game_Music_Emu https://bitbucket.org/mpyne/game-music-emu/

#include "Gme_File.h"

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

const char* const gme_wrong_file_type = "Wrong file type for this emulator";

void Gme_File::clear_playlist()
{
	playlist.clear();
	clear_playlist_();
	track_count_ = raw_track_count_;
}

void Gme_File::unload()
{
	clear_playlist(); // *before* clearing track count
	track_count_     = 0;
	raw_track_count_ = 0;
	file_data.clear();
}

Gme_File::Gme_File()
{
	type_         = 0;
	user_data_    = 0;
	user_cleanup_ = 0;
	unload(); // clears fields
	blargg_verify_byte_order(); // used by most emulator types, so save them the trouble
}

Gme_File::~Gme_File()
{
	if ( user_cleanup_ )
		user_cleanup_( user_data_ );
}

blargg_err_t Gme_File::load_mem_( byte const* data, long size )
{
	require( data != file_data.begin() ); // load_mem_() or load_() must be overridden
	Mem_File_Reader in( data, size );
	return load_( in );
}

blargg_err_t Gme_File::load_( Data_Reader& in )
{
	RETURN_ERR( file_data.resize( in.remain() ) );
	RETURN_ERR( in.read( file_data.begin(), file_data.size() ) );
	return load_mem_( file_data.begin(), file_data.size() );
}

// public load functions call this at beginning
void Gme_File::pre_load() { unload(); }

void Gme_File::post_load_() { }

// public load functions call this at end
blargg_err_t Gme_File::post_load( blargg_err_t err )
{
	if ( !track_count() )
		set_track_count( type()->track_count );
	if ( !err )
		post_load_();
	else
		unload();
	
	return err;
}

// Public load functions

blargg_err_t Gme_File::load_mem( void const* in, long size )
{
	pre_load();
	return post_load( load_mem_( (byte const*) in, size ) );
}

blargg_err_t Gme_File::load( Data_Reader& in )
{
	pre_load();
	return post_load( load_( in ) );
}

blargg_err_t Gme_File::load_file( const char* path )
{
	pre_load();
	GME_FILE_READER in;
	RETURN_ERR( in.open( path ) );
	return post_load( load_( in ) );
}

blargg_err_t Gme_File::load_remaining_( void const* h, long s, Data_Reader& in )
{
	Remaining_Reader rem( h, s, &in );
	return load( rem );
}

// Track info

void Gme_File::copy_field_( char* out, const char* in, int in_size )
{
	if ( !in || !*in )
		return;
	
	// remove spaces/junk from beginning
	while ( in_size && unsigned (*in - 1) <= ' ' - 1 )
	{
		in++;
		in_size--;
	}
	
	// truncate
	if ( in_size > max_field_ )
		in_size = max_field_;
	
	// find terminator
	int len = 0;
	while ( len < in_size && in [len] )
		len++;
	
	// remove spaces/junk from end
	while ( len && unsigned (in [len - 1]) <= ' ' )
		len--;
	
	// copy
	out [len] = 0;
	memcpy( out, in, len );
	
	// strip out stupid fields that should have been left blank
	if ( !strcmp( out, "?" ) || !strcmp( out, "<?>" ) || !strcmp( out, "< ? >" ) )
		out [0] = 0;
}

void Gme_File::copy_field_( char* out, const char* in )
{
	copy_field_( out, in, max_field_ );
}

blargg_err_t Gme_File::remap_track_( int* track_io ) const
{
	if ( (unsigned) *track_io >= (unsigned) track_count() )
		return "Invalid track";
	
	if ( (unsigned) *track_io < (unsigned) playlist.size() )
	{
		M3u_Playlist::entry_t const& e = playlist [*track_io];
		*track_io = 0;
		if ( e.track >= 0 )
		{
			*track_io = e.track;
			if ( !(type_->flags_ & 0x02) )
				*track_io -= e.decimal_track;
		}
		if ( *track_io >= raw_track_count_ )
			return "Invalid track in m3u playlist";
	}
	else
	{
		check( !playlist.size() );
	}
	return 0;
}

blargg_err_t Gme_File::track_info( track_info_t* out, int track ) const
{
	out->track_count = track_count();
	out->length        = -1;
	out->loop_length   = -1;
	out->intro_length  = -1;
	out->song [0]      = 0;
	
	out->game [0]      = 0;
	out->author [0]    = 0;
	out->copyright [0] = 0;
	out->comment [0]   = 0;
	out->dumper [0]    = 0;
	out->system [0]    = 0;
	
	copy_field_( out->system, type()->system );
	
	int remapped = track;
	RETURN_ERR( remap_track_( &remapped ) );
	RETURN_ERR( track_info_( out, remapped ) );
	
	// override with m3u info
	if ( playlist.size() )
	{
		M3u_Playlist::info_t const& i = playlist.info();
		copy_field_( out->game  , i.title );
		copy_field_( out->author, i.engineer );
		copy_field_( out->author, i.composer );
		copy_field_( out->dumper, i.ripping );
		
		M3u_Playlist::entry_t const& e = playlist [track];
		copy_field_( out->song, e.name );
		if ( e.length >= 0 ) out->length       = e.length * 1000L;
		if ( e.intro  >= 0 ) out->intro_length = e.intro  * 1000L;
		if ( e.loop   >= 0 ) out->loop_length  = e.loop   * 1000L;
	}
	return 0;
}
