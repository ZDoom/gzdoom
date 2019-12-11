// Game_Music_Emu https://bitbucket.org/mpyne/game-music-emu/

#include "Music_Emu.h"

#include "gme_types.h"
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

BLARGG_EXPORT gme_type_t const* gme_type_list()
{
	static gme_type_t const gme_type_list_ [] = {
#ifdef GME_TYPE_LIST
	GME_TYPE_LIST,
#else
	#ifdef USE_GME_AY
	            gme_ay_type,
	#endif
	#ifdef USE_GME_GBS
	            gme_gbs_type,
	#endif
	#ifdef USE_GME_GYM
	            gme_gym_type,
	#endif
	#ifdef USE_GME_HES
	            gme_hes_type,
	#endif
	#ifdef USE_GME_KSS
	            gme_kss_type,
	#endif
	#ifdef USE_GME_NSF
	            gme_nsf_type,
	#endif
	#ifdef USE_GME_NSFE
	            gme_nsfe_type,
	#endif
	#ifdef USE_GME_SAP
	            gme_sap_type,
	#endif
	#ifdef USE_GME_SPC
	            gme_spc_type,
	#endif
	#ifdef USE_GME_VGM
	            gme_vgm_type,
	            gme_vgz_type,
	#endif
#endif
        0
    };

	return gme_type_list_;
}

BLARGG_EXPORT const char* gme_identify_header( void const* header )
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
	if (get_be16(header) == BLARGG_2CHAR(0x1F, 0x8B))
		return "VGZ";
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

BLARGG_EXPORT gme_type_t gme_identify_extension( const char* extension_ )
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

BLARGG_EXPORT const char *gme_type_extension( gme_type_t music_type )
{
	const gme_type_t_ *const music_typeinfo = static_cast<const gme_type_t_ *>( music_type );
	if ( music_type )
		return music_typeinfo->extension_;
	return "";
}

BLARGG_EXPORT gme_err_t gme_identify_file( const char* path, gme_type_t* type_out )
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

BLARGG_EXPORT gme_err_t gme_open_data( void const* data, long size, Music_Emu** out, int sample_rate )
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

BLARGG_EXPORT gme_err_t gme_open_file( const char* path, Music_Emu** out, int sample_rate )
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

BLARGG_EXPORT void gme_set_autoload_playback_limit( Music_Emu *emu, int do_autoload_limit )
{
	emu->set_autoload_playback_limit( do_autoload_limit != 0 );
}

BLARGG_EXPORT int gme_autoload_playback_limit( Music_Emu *const emu )
{
	return emu->autoload_playback_limit();
}

// Used to implement gme_new_emu and gme_new_emu_multi_channel
Music_Emu* gme_internal_new_emu_( gme_type_t type, int rate, bool multi_channel )
{
	if ( type )
	{
		if ( rate == gme_info_only )
			return type->new_info();

		Music_Emu* me = type->new_emu();
		if ( me )
		{
		#if !GME_DISABLE_STEREO_DEPTH
			me->set_multi_channel( multi_channel );

			if ( type->flags_ & 1 )
			{
				if ( me->multi_channel() )
				{
					me->effects_buffer = BLARGG_NEW Effects_Buffer(8);
				}
				else
				{
					me->effects_buffer = BLARGG_NEW Effects_Buffer(1);
				}
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

BLARGG_EXPORT Music_Emu* gme_new_emu( gme_type_t type, int rate )
{
    return gme_internal_new_emu_( type, rate, false /* no multichannel */);
}

BLARGG_EXPORT Music_Emu* gme_new_emu_multi_channel( gme_type_t type, int rate )
{
    // multi-channel emulator (if possible, not all emu types support multi-channel)
    return gme_internal_new_emu_( type, rate, true /* multichannel */);
}

BLARGG_EXPORT gme_err_t gme_load_file( Music_Emu* me, const char* path ) { return me->load_file( path ); }

BLARGG_EXPORT gme_err_t gme_load_data( Music_Emu* me, void const* data, long size )
{
	Mem_File_Reader in( data, size );
	return me->load( in );
}

BLARGG_EXPORT gme_err_t gme_load_custom( Music_Emu* me, gme_reader_t func, long size, void* data )
{
	Callback_Reader in( func, size, data );
	return me->load( in );
}

BLARGG_EXPORT void gme_delete( Music_Emu* me ) { delete me; }

BLARGG_EXPORT gme_type_t gme_type( Music_Emu const* me ) { return me->type(); }

BLARGG_EXPORT const char* gme_warning( Music_Emu* me ) { return me->warning(); }

BLARGG_EXPORT int gme_track_count( Music_Emu const* me ) { return me->track_count(); }

struct gme_info_t_ : gme_info_t
{
	track_info_t info;

	BLARGG_DISABLE_NOTHROW
};

BLARGG_EXPORT gme_err_t gme_track_info( Music_Emu const* me, gme_info_t** out, int track )
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

BLARGG_EXPORT void gme_free_info( gme_info_t* info )
{
	delete STATIC_CAST(gme_info_t_*,info);
}

BLARGG_EXPORT void gme_set_stereo_depth( Music_Emu* me, double depth )
{
#if !GME_DISABLE_STEREO_DEPTH
	if ( me->effects_buffer )
		STATIC_CAST(Effects_Buffer*,me->effects_buffer)->set_depth( depth );
#endif
}

BLARGG_EXPORT void*     gme_user_data      ( Music_Emu const* me )                { return me->user_data(); }
BLARGG_EXPORT void      gme_set_user_data  ( Music_Emu* me, void* new_user_data ) { me->set_user_data( new_user_data ); }
BLARGG_EXPORT void      gme_set_user_cleanup(Music_Emu* me, gme_user_cleanup_t func ) { me->set_user_cleanup( func ); }

BLARGG_EXPORT gme_err_t gme_start_track    ( Music_Emu* me, int index )           { return me->start_track( index ); }
BLARGG_EXPORT gme_err_t gme_play           ( Music_Emu* me, int n, short* p )     { return me->play( n, p ); }
BLARGG_EXPORT void      gme_set_fade       ( Music_Emu* me, int start_msec )      { me->set_fade( start_msec ); }
BLARGG_EXPORT int       gme_track_ended    ( Music_Emu const* me )                { return me->track_ended(); }
BLARGG_EXPORT int       gme_tell           ( Music_Emu const* me )                { return me->tell(); }
BLARGG_EXPORT int       gme_tell_samples   ( Music_Emu const* me )                { return me->tell_samples(); }
BLARGG_EXPORT gme_err_t gme_seek           ( Music_Emu* me, int msec )            { return me->seek( msec ); }
BLARGG_EXPORT gme_err_t gme_seek_samples   ( Music_Emu* me, int n )               { return me->seek_samples( n ); }
BLARGG_EXPORT int       gme_voice_count    ( Music_Emu const* me )                { return me->voice_count(); }
BLARGG_EXPORT void      gme_ignore_silence ( Music_Emu* me, int disable )         { me->ignore_silence( disable != 0 ); }
BLARGG_EXPORT void      gme_set_tempo      ( Music_Emu* me, double t )            { me->set_tempo( t ); }
BLARGG_EXPORT void      gme_mute_voice     ( Music_Emu* me, int index, int mute ) { me->mute_voice( index, mute != 0 ); }
BLARGG_EXPORT void      gme_mute_voices    ( Music_Emu* me, int mask )            { me->mute_voices( mask ); }
BLARGG_EXPORT void      gme_enable_accuracy( Music_Emu* me, int enabled )         { me->enable_accuracy( enabled ); }
BLARGG_EXPORT void      gme_clear_playlist ( Music_Emu* me )                      { me->clear_playlist(); }
BLARGG_EXPORT int       gme_type_multitrack( gme_type_t t )                       { return t->track_count != 1; }
BLARGG_EXPORT int       gme_multi_channel  ( Music_Emu const* me )                { return me->multi_channel(); }

BLARGG_EXPORT void      gme_set_equalizer  ( Music_Emu* me, gme_equalizer_t const* eq )
{
	Music_Emu::equalizer_t e = me->equalizer();
	e.treble = eq->treble;
	e.bass   = eq->bass;
	me->set_equalizer( e );
}

BLARGG_EXPORT void gme_equalizer( Music_Emu const* me, gme_equalizer_t* out )
{
	gme_equalizer_t e = gme_equalizer_t(); // Default-init all fields to 0.0f
	e.treble = me->equalizer().treble;
	e.bass   = me->equalizer().bass;
	*out = e;
}

BLARGG_EXPORT const char* gme_voice_name( Music_Emu const* me, int i )
{
	assert( (unsigned) i < (unsigned) me->voice_count() );
	return me->voice_names() [i];
}

BLARGG_EXPORT const char* gme_type_system( gme_type_t type )
{
	assert( type );
	return type->system;
}
