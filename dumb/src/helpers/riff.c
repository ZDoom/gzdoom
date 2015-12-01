#include "dumb.h"
#include "internal/riff.h"
#include "internal/dumb.h"

#include <stdlib.h>
#include <string.h>

struct riff * riff_parse( unsigned char * ptr, unsigned size, unsigned proper )
{
	unsigned stream_size;
	struct riff * stream;

	if ( size < 8 ) return 0;

	if ( ptr[0] != 'R' || ptr[1] != 'I' || ptr[2] != 'F' || ptr[3] != 'F' ) return 0;

	stream_size = ptr[4] | ( ptr[5] << 8 ) | ( ptr[6] << 16 ) | ( ptr[7] << 24 );
	if ( stream_size + 8 > size ) return 0;
	if ( stream_size < 4 ) return 0;

	stream = malloc( sizeof( struct riff ) );
	if ( ! stream ) return 0;

	stream->type = ( ptr[8] << 24 ) | ( ptr[9] << 16 ) | ( ptr[10] << 8 ) | ptr[11];
	stream->chunk_count = 0;
	stream->chunks = 0;

	ptr += 12;
	stream_size -= 4;

	while ( stream_size )
	{
		struct riff_chunk * chunk;
		if ( stream_size < 8 ) break;
		stream->chunks = realloc( stream->chunks, ( stream->chunk_count + 1 ) * sizeof( struct riff_chunk ) );
		if ( ! stream->chunks ) break;
		chunk = stream->chunks + stream->chunk_count;
		chunk->type = ( ptr[0] << 24 ) | ( ptr[1] << 16 ) | ( ptr[2] << 8 ) | ptr[3];
		chunk->size = ptr[4] | ( ptr[5] << 8 ) | ( ptr[6] << 16 ) | ( ptr[7] << 24 );
		ptr += 8;
		stream_size -= 8;
		if ( stream_size < chunk->size ) break;
		if ( chunk->type == DUMB_ID('R','I','F','F') )
		{
			chunk->data = riff_parse( ptr - 8, chunk->size + 8, proper );
			if ( ! chunk->data ) break;
		}
		else
		{
			chunk->data = malloc( chunk->size );
			if ( ! chunk->data ) break;
			memcpy( chunk->data, ptr, chunk->size );
		}
		ptr += chunk->size;
		stream_size -= chunk->size;
		if ( proper && ( chunk->size & 1 ) )
		{
			++ ptr;
			-- stream_size;
		}
		++stream->chunk_count;
	}
	
	if ( stream_size )
	{
		riff_free( stream );
		stream = 0;
	}

	return stream;
}

void riff_free( struct riff * stream )
{
	if ( stream )
	{
		if ( stream->chunks )
		{
			unsigned i;
			for ( i = 0; i < stream->chunk_count; ++i )
			{
				struct riff_chunk * chunk = stream->chunks + i;
				if ( chunk->type == DUMB_ID('R','I','F','F') ) riff_free( ( struct riff * ) chunk->data );
				else free( chunk->data );
			}
			free( stream->chunks );
		}
		free( stream );
	}
}
