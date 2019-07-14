// File_Extractor 0.4.0. http://www.slack.net/~ant/

#include "Data_Reader.h"

#include "blargg_endian.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>

/* Copyright (C) 2005-2006 Shay Green. This module is free software; you
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

#ifdef HAVE_ZLIB_H
#include <zlib.h>
#include <stdlib.h>
#include <errno.h>
static const unsigned char gz_magic[2] = {0x1f, 0x8b}; /* gzip magic header */
#endif /* HAVE_ZLIB_H */

const char Data_Reader::eof_error [] = "Unexpected end of file";

#define RETURN_VALIDITY_CHECK( cond ) \
	do { if ( unlikely( !(cond) ) ) return "Corrupt file"; } while(0)

blargg_err_t Data_Reader::read( void* p, long s )
{
	RETURN_VALIDITY_CHECK( s > 0 );

	long result = read_avail( p, s );
	if ( result != s )
	{
		if ( result >= 0 && result < s )
			return eof_error;

		return "Read error";
	}

	return 0;
}

blargg_err_t Data_Reader::skip( long count )
{
	RETURN_VALIDITY_CHECK( count >= 0 );

	char buf [512];
	while ( count )
	{
		long n = sizeof buf;
		if ( n > count )
			n = count;
		count -= n;
		RETURN_ERR( read( buf, n ) );
	}
	return 0;
}

long File_Reader::remain() const { return size() - tell(); }

blargg_err_t File_Reader::skip( long n )
{
	RETURN_VALIDITY_CHECK( n >= 0 );

	if ( !n )
		return 0;
	return seek( tell() + n );
}

// Subset_Reader

Subset_Reader::Subset_Reader( Data_Reader* dr, long size )
{
	in = dr;
	remain_ = dr->remain();
	if ( remain_ > size )
		remain_ = max( 0l, size );
}

long Subset_Reader::remain() const { return remain_; }

long Subset_Reader::read_avail( void* p, long s )
{
	s = max( 0l, s );
	if ( s > remain_ )
		s = remain_;
	remain_ -= s;
	return in->read_avail( p, s );
}

// Remaining_Reader

Remaining_Reader::Remaining_Reader( void const* h, long size, Data_Reader* r )
{
	header = (char const*) h;
	header_end = header + max( 0l, size );
	in = r;
}

long Remaining_Reader::remain() const { return header_end - header + in->remain(); }

long Remaining_Reader::read_first( void* out, long count )
{
	count = max( 0l, count );
	long first = header_end - header;
	if ( first )
	{
		if ( first > count || first < 0 )
			first = count;
		void const* old = header;
		header += first;
		memcpy( out, old, (size_t) first );
	}
	return first;
}

long Remaining_Reader::read_avail( void* out, long count )
{
	count = max( 0l, count );
	long first = read_first( out, count );
	long second = max( 0l, count - first );
	if ( second )
	{
		second = in->read_avail( (char*) out + first, second );
		if ( second <= 0 )
			return second;
	}
	return first + second;
}

blargg_err_t Remaining_Reader::read( void* out, long count )
{
	count = max( 0l, count );
	long first = read_first( out, count );
	long second = max( 0l, count - first );
	if ( !second )
		return 0;
	return in->read( (char*) out + first, second );
}

// Mem_File_Reader

Mem_File_Reader::Mem_File_Reader( const void* p, long s ) :
	m_begin( (const char*) p ),
	m_size( max( 0l, s ) ),
	m_pos( 0l )
{
#ifdef HAVE_ZLIB_H
	if( !m_begin )
		return;

	if ( gz_decompress() )
	{
		debug_printf( "Loaded compressed data\n" );
		m_ownedPtr = true;
	}
#endif /* HAVE_ZLIB_H */
}

#ifdef HAVE_ZLIB_H
Mem_File_Reader::~Mem_File_Reader()
{
	if ( m_ownedPtr )
		free( const_cast<char*>( m_begin ) ); // see gz_compress for the malloc
}
#endif

long Mem_File_Reader::size() const { return m_size; }

long Mem_File_Reader::read_avail( void* p, long s )
{
	long r = remain();
	if ( s > r || s < 0 )
		s = r;
	memcpy( p, m_begin + m_pos, static_cast<size_t>(s) );
	m_pos += s;
	return s;
}

long Mem_File_Reader::tell() const { return m_pos; }

blargg_err_t Mem_File_Reader::seek( long n )
{
	RETURN_VALIDITY_CHECK( n >= 0 );
	if ( n > m_size )
		return eof_error;
	m_pos = n;
	return 0;
}

#ifdef HAVE_ZLIB_H

bool Mem_File_Reader::gz_decompress()
{
	if ( m_size >= 2 && memcmp(m_begin, gz_magic, 2) != 0 )
	{
		/* Don't try to decompress non-GZ files, just assign input pointer */
		return false;
	}

	using vec_size = size_t;
	const vec_size full_length = static_cast<vec_size>( m_size );
	const vec_size half_length = static_cast<vec_size>( m_size / 2 );

	// We use malloc/friends here so we can realloc to grow buffer if needed
	char *raw_data = reinterpret_cast<char *> ( malloc( full_length ) );
	size_t raw_data_size = full_length;
	if ( !raw_data )
		return false;

	z_stream strm;
	strm.next_in   = const_cast<Bytef *>( reinterpret_cast<const Bytef *>( m_begin ) );
	strm.avail_in  = static_cast<uInt>( m_size );
	strm.total_out = 0;
	strm.zalloc    = Z_NULL;
	strm.zfree     = Z_NULL;

	bool done = false;

	// Adding 16 sets bit 4, which enables zlib to auto-detect the
	// header.
	if ( inflateInit2(&strm, (16 + MAX_WBITS)) != Z_OK )
	{
		free( raw_data );
		return false;
	}

	while ( !done )
	{
		/* If our output buffer is too small */
		if ( strm.total_out >= raw_data_size )
		{
			raw_data_size += half_length;
			raw_data = reinterpret_cast<char *>( realloc( raw_data, raw_data_size ) );
			if ( !raw_data ) {
				return false;
			}
		}

		strm.next_out  = reinterpret_cast<Bytef *>( raw_data + strm.total_out );
		strm.avail_out = static_cast<uInt>( static_cast<uLong>( raw_data_size ) - strm.total_out );

		/* Inflate another chunk. */
		int err = inflate( &strm, Z_SYNC_FLUSH );
		if ( err == Z_STREAM_END )
			done = true;
		else if ( err != Z_OK )
			break;
	}

	if ( inflateEnd(&strm) != Z_OK )
	{
		free( raw_data );
		return false;
	}

	m_begin = raw_data;
	m_size  = static_cast<long>( strm.total_out );

	return true;
}

#endif /* HAVE_ZLIB_H */


// Callback_Reader

Callback_Reader::Callback_Reader( callback_t c, long size, void* d ) :
	callback( c ),
	data( d )
{
	remain_ = max( 0l, size );
}

long Callback_Reader::remain() const { return remain_; }

long Callback_Reader::read_avail( void* out, long count )
{
	if ( count > remain_ )
		count = remain_;
	if ( count < 0 || Callback_Reader::read( out, count ) )
		count = -1;
	return count;
}

blargg_err_t Callback_Reader::read( void* out, long count )
{
	RETURN_VALIDITY_CHECK( count >= 0 );
	if ( count > remain_ )
		return eof_error;
	return callback( data, out, (int) count );
}

// Std_File_Reader

#if 0//[ZDOOM:unneeded]def HAVE_ZLIB_H

static const char* get_gzip_eof( const char* path, long* eof )
{
	FILE* file = fopen( path, "rb" );
	if ( !file )
		return "Couldn't open file";

	unsigned char buf [4];
	bool found_eof = false;
	if ( fread( buf, 2, 1, file ) > 0 && buf [0] == 0x1F && buf [1] == 0x8B )
	{
		fseek( file, -4, SEEK_END );
		if ( fread( buf, 4, 1, file ) > 0 ) {
			*eof = get_le32( buf );
			found_eof = true;
		}
	}
	if ( !found_eof )
	{
		fseek( file, 0, SEEK_END );
		*eof = ftell( file );
	}
	const char* err = (ferror( file ) || feof( file )) ? "Couldn't get file size" : nullptr;
	fclose( file );
	return err;
}
#endif


Std_File_Reader::Std_File_Reader() :
	file_( nullptr )
#if 0//[ZDOOM:unneeded]def HAVE_ZLIB_H
	, size_( 0 )
#endif
{ }

Std_File_Reader::~Std_File_Reader() { close(); }

blargg_err_t Std_File_Reader::open( const char* path )
{
#if 0//[ZDOOM:unneeded]def HAVE_ZLIB_H
	// zlib transparently handles uncompressed data if magic header
	// not present but we still need to grab size
	RETURN_ERR( get_gzip_eof( path, &size_ ) );
	file_ = gzopen( path, "rb" );
#else
	file_ = fopen( path, "rb" );
#endif

	if ( !file_ )
		return "Couldn't open file";
	return nullptr;
}

long Std_File_Reader::size() const
{
#if 0//[ZDOOM:unneeded]def HAVE_ZLIB_H
	if ( file_ )
		return size_; // Set for both compressed and uncompressed modes
#endif
	long pos = tell();
	fseek( (FILE*) file_, 0, SEEK_END );
	long result = tell();
	fseek( (FILE*) file_, pos, SEEK_SET );
	return result;
}

long Std_File_Reader::read_avail( void* p, long s )
{
#if 0//[ZDOOM:unneeded]def HAVE_ZLIB_H
	if ( file_ && s > 0 && s <= UINT_MAX ) {
		return gzread( reinterpret_cast<gzFile>(file_),
			p, static_cast<unsigned>(s) );
	}
	return 0l;
#else
	const size_t readLength = static_cast<size_t>( max( 0l, s ) );
	const auto result = fread( p, 1, readLength, reinterpret_cast<FILE*>(file_) );
	return static_cast<long>( result );
#endif /* HAVE_ZLIB_H */
}

blargg_err_t Std_File_Reader::read( void* p, long s )
{
	RETURN_VALIDITY_CHECK( s > 0 && s <= UINT_MAX );
#if 0//[ZDOOM:unneeded]def HAVE_ZLIB_H
	if ( file_ )
	{
		const auto &gzfile = reinterpret_cast<gzFile>( file_ );
		if ( s == gzread( gzfile, p, static_cast<unsigned>( s ) ) )
			return nullptr;
		if ( gzeof( gzfile ) )
			return eof_error;
		return "Couldn't read from GZ file";
	}
#endif
	const auto &file = reinterpret_cast<FILE*>( file_ );
	if ( s == static_cast<long>( fread( p, 1, static_cast<size_t>(s), file ) ) )
		return 0;
	if ( feof( file ) )
		return eof_error;
	return "Couldn't read from file";
}

long Std_File_Reader::tell() const
{
#if 0//[ZDOOM:unneeded]def HAVE_ZLIB_H
	if ( file_ )
		return gztell( reinterpret_cast<gzFile>( file_ ) );
#endif
	return ftell( reinterpret_cast<FILE*>( file_ ) );
}

blargg_err_t Std_File_Reader::seek( long n )
{
#if 0//[ZDOOM:unneeded]def HAVE_ZLIB_H
	if ( file_ )
	{
		if ( gzseek( reinterpret_cast<gzFile>( file_ ), n, SEEK_SET ) >= 0 )
			return nullptr;
		if ( n > size_ )
			return eof_error;
		return "Error seeking in GZ file";
	}
#endif
	if ( !fseek( reinterpret_cast<FILE*>( file_ ), n, SEEK_SET ) )
		return nullptr;
	if ( n > size() )
		return eof_error;
	return "Error seeking in file";
}

void Std_File_Reader::close()
{
	if ( file_ )
	{
#if 0//[ZDOOM:unneeded]def HAVE_ZLIB_H
		gzclose( reinterpret_cast<gzFile>( file_ ) );
#else
		fclose( reinterpret_cast<FILE*>( file_ ) );
#endif
		file_ = nullptr;
	}
}

