// Game_Music_Emu 0.5.2. http://www.slack.net/~ant/

#define IN_GME 1

#include "Music_Emu.h"

#if !GME_DISABLE_STEREO_DEPTH
#include "Effects_Buffer.h"
#endif
#include "blargg_endian.h"
#include <string.h>
#include <ctype.h>

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

#ifndef GME_TYPE_LIST

// Default list of all supported game music types (copy this to blargg_config.h
// if you want to modify it)
#define GME_TYPE_LIST \
	gme_ay_type,\
	gme_gbs_type,\
	gme_gym_type,\
	gme_hes_type,\
	gme_kss_type,\
	gme_nsf_type,\
	gme_nsfe_type,\
	gme_sap_type,\
	gme_spc_type,\
	gme_vgm_type,\
	gme_vgz_type

#endif

gme_type_t const* GMEAPI gme_type_list()
{
	static gme_type_t const gme_type_list_ [] = { GME_TYPE_LIST, 0 };
	return gme_type_list_;
}

const char* GMEAPI gme_identify_header( void const* header )
{
	switch ( get_be32( header ) )
	{
		case BLARGG_4CHAR('Z','X','A','Y'):  return "AY";
		case BLARGG_4CHAR('G','B','S',0x01): return "GBS";
		case BLARGG_4CHAR('G','Y','M','X'):  return "GYM";
		case BLARGG_4CHAR('H','E','S','M'):  return "HES";
		case BLARGG_4CHAR('K','S','C','C'):
		case BLARGG_4CHAR('K','S','S','X'):  return "KSS";
		case BLARGG_4CHAR('N','E','S','M'):  return "NSF";
		case BLARGG_4CHAR('N','S','F','E'):  return "NSFE";
		case BLARGG_4CHAR('S','A','P',0x0D): return "SAP";
		case BLARGG_4CHAR('S','N','E','S'):  return "SPC";
		case BLARGG_4CHAR('V','g','m',' '):  return "VGM";
	}
	return "";
}

static void to_uppercase( const char* in, int len, char* out )
{
	for ( int i = 0; i < len; i++ )
	{
		if ( !(out [i] = toupper( in [i] )) )
			return;
	}
	*out = 0; // extension too long
}

gme_type_t GMEAPI gme_identify_extension( const char* extension_ )
{
	char const* end = strrchr( extension_, '.' );
	if ( end )
		extension_ = end + 1;
	
	char extension [6];
	to_uppercase( extension_, sizeof extension, extension );
	
	for ( gme_type_t const* types = gme_type_list(); *types; types++ )
		if ( !strcmp( extension, (*types)->extension_ ) )
			return *types;
	return 0;
}

gme_err_t GMEAPI gme_identify_file( const char* path, gme_type_t* type_out )
{
	*type_out = gme_identify_extension( path );
	// TODO: don't examine header if file has extension?
	if ( !*type_out )
	{
		char header [4];
		GME_FILE_READER in;
		RETURN_ERR( in.open( path ) );
		RETURN_ERR( in.read( header, sizeof header ) );
		*type_out = gme_identify_extension( gme_identify_header( header ) );
	}
	return 0;   
}

gme_err_t GMEAPI gme_open_data( void const* data, long size, Music_Emu** out, int sample_rate )
{
	require( (data || !size) && out );
	*out = 0;
	
	gme_type_t file_type = 0;
	if ( size >= 4 )
		file_type = gme_identify_extension( gme_identify_header( data ) );
	if ( !file_type )
		return gme_wrong_file_type;
	
	Music_Emu* emu = gme_new_emu( file_type, sample_rate );
	CHECK_ALLOC( emu );
	
	gme_err_t err = gme_load_data( emu, data, size );
	
	if ( err )
		delete emu;
	else
		*out = emu;
	
	return err;
}

GMEEXPORT gme_err_t GMEAPI gme_open_file( const char* path, Music_Emu** out, int sample_rate )
{
	require( path && out );
	*out = 0;
	
	GME_FILE_READER in;
	RETURN_ERR( in.open( path ) );
	
	char header [4];
	int header_size = 0;
	
	gme_type_t file_type = gme_identify_extension( path );
	if ( !file_type )
	{
		header_size = sizeof header;
		RETURN_ERR( in.read( header, sizeof header ) );
		file_type = gme_identify_extension( gme_identify_header( header ) );
	}
	if ( !file_type )
		return gme_wrong_file_type;
	
	Music_Emu* emu = gme_new_emu( file_type, sample_rate );
	CHECK_ALLOC( emu );
	
	// optimization: avoids seeking/re-reading header
	Remaining_Reader rem( header, header_size, &in );
	gme_err_t err = emu->load( rem );
	in.close();
	
	if ( err )
		delete emu;
	else
		*out = emu;
	
	return err;
}

Music_Emu* GMEAPI gme_new_emu( gme_type_t type, int rate )
{
	if ( type )
	{
		if ( rate == gme_info_only )
			return type->new_info();
		
		Music_Emu* me = type->new_emu();
		if ( me )
		{
		#if !GME_DISABLE_STEREO_DEPTH
			if ( type->flags_ & 1 )
			{
				me->effects_buffer = BLARGG_NEW Effects_Buffer;
				if ( me->effects_buffer )
					me->set_buffer( me->effects_buffer );
			}
			
			if ( !(type->flags_ & 1) || me->effects_buffer )
		#endif
			{
				if ( !me->set_sample_rate( rate ) )
				{
					check( me->type() == type );
					return me;
				}
			}
			delete me;
		}
	}
	return 0;
}

gme_err_t GMEAPI gme_load_file( Music_Emu* me, const char* path ) { return me->load_file( path ); }

gme_err_t GMEAPI gme_load_data( Music_Emu* me, void const* data, long size )
{
	Mem_File_Reader in( data, size );
	return me->load( in );
}

gme_err_t GMEAPI gme_load_custom( Music_Emu* me, gme_reader_t func, long size, void* data )
{
	Callback_Reader in( func, size, data );
	return me->load( in );
}

void GMEAPI gme_delete( Music_Emu* me ) { delete me; }

gme_type_t GMEAPI gme_type( Music_Emu const* me ) { return me->type(); }

const char* GMEAPI gme_warning( Music_Emu* me ) { return me->warning(); }

int GMEAPI gme_track_count( Music_Emu const* me ) { return me->track_count(); }

struct gme_info_t_ : gme_info_t
{
	track_info_t info;
	
	BLARGG_DISABLE_NOTHROW
};

gme_err_t GMEAPI gme_track_info( Music_Emu const* me, gme_info_t** out, int track )
{
	*out = NULL;
	
	gme_info_t_* info = BLARGG_NEW gme_info_t_;
	CHECK_ALLOC( info );
	
	gme_err_t err = me->track_info( &info->info, track );
	if ( err )
	{
		gme_free_info( info );
		return err;
	}
	
	#define COPY(name) info->name = info->info.name;
	
	COPY( length );
	COPY( intro_length );
	COPY( loop_length );
	
	info->i4  = -1;
	info->i5  = -1;
	info->i6  = -1;
	info->i7  = -1;
	info->i8  = -1;
	info->i9  = -1;
	info->i10 = -1;
	info->i11 = -1;
	info->i12 = -1;
	info->i13 = -1;
	info->i14 = -1;
	info->i15 = -1;
	
	info->s7  = "";
	info->s8  = "";
	info->s9  = "";
	info->s10 = "";
	info->s11 = "";
	info->s12 = "";
	info->s13 = "";
	info->s14 = "";
	info->s15 = "";
	
	COPY( system );
	COPY( game );
	COPY( song );
	COPY( author );
	COPY( copyright );
	COPY( comment );
	COPY( dumper );
	
	#undef COPY
	
	info->play_length = info->length;
	if ( info->play_length <= 0 )
	{
		info->play_length = info->intro_length + 2 * info->loop_length; // intro + 2 loops
		if ( info->play_length <= 0 )
			info->play_length = 150 * 1000; // 2.5 minutes
	}
	
	*out = info;
	
	return 0;
}

void GMEAPI gme_free_info( gme_info_t* info )
{
	delete STATIC_CAST(gme_info_t_*,info);
}

void GMEAPI gme_set_stereo_depth( Music_Emu* me, double depth )
{
#if !GME_DISABLE_STEREO_DEPTH
	if ( me->effects_buffer )
		STATIC_CAST(Effects_Buffer*,me->effects_buffer)->set_depth( depth );
#endif
}

void*     GMEAPI gme_user_data      ( Music_Emu const* me )                { return me->user_data(); }
void      GMEAPI gme_set_user_data  ( Music_Emu* me, void* new_user_data ) { me->set_user_data( new_user_data ); }
void      GMEAPI gme_set_user_cleanup(Music_Emu* me, gme_user_cleanup_t func ) { me->set_user_cleanup( func ); }

gme_err_t GMEAPI gme_start_track    ( Music_Emu* me, int index )           { return me->start_track( index ); }
gme_err_t GMEAPI gme_play           ( Music_Emu* me, int n, short* p )     { return me->play( n, p ); }
gme_err_t GMEAPI gme_skip           ( Music_Emu* me, long n )              { return me->skip( n ); }
void      GMEAPI gme_set_fade       ( Music_Emu* me, int start_msec )      { me->set_fade( start_msec ); }
int       GMEAPI gme_track_ended    ( Music_Emu const* me )                { return me->track_ended(); }
int       GMEAPI gme_tell           ( Music_Emu const* me )                { return me->tell(); }
gme_err_t GMEAPI gme_seek           ( Music_Emu* me, int msec )            { return me->seek( msec ); }
int       GMEAPI gme_voice_count    ( Music_Emu const* me )                { return me->voice_count(); }
void      GMEAPI gme_ignore_silence ( Music_Emu* me, int disable )         { me->ignore_silence( disable != 0 ); }
void      GMEAPI gme_set_tempo      ( Music_Emu* me, double t )            { me->set_tempo( t ); }
void      GMEAPI gme_mute_voice     ( Music_Emu* me, int index, int mute ) { me->mute_voice( index, mute != 0 ); }
void      GMEAPI gme_mute_voices    ( Music_Emu* me, int mask )            { me->mute_voices( mask ); }

void      GMEAPI gme_set_equalizer  ( Music_Emu* me, gme_equalizer_t const* eq )
{
	Music_Emu::equalizer_t e = me->equalizer();
	e.treble = eq->treble;
	e.bass   = eq->bass;
	me->set_equalizer( e );
}

void GMEAPI gme_equalizer( Music_Emu const* me, gme_equalizer_t* out )
{
	gme_equalizer_t e = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
	e.treble = me->equalizer().treble;
	e.bass   = me->equalizer().bass;
	*out = e;
}

const char* GMEAPI gme_voice_name( Music_Emu const* me, int i )
{
	assert( (unsigned) i < (unsigned) me->voice_count() );
	return me->voice_names() [i];
}
