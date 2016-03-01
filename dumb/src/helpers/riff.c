#include "dumb.h"
#include "internal/riff.h"

#include <stdlib.h>
#include <string.h>

struct riff * riff_parse( DUMBFILE * f, int32 offset, int32 size, unsigned proper )
{
	unsigned stream_size;
	struct riff * stream;


    if ( size < 8 ) return 0;

    if ( dumbfile_seek(f, offset, DFS_SEEK_SET) ) return 0;
    if ( dumbfile_mgetl(f) != DUMB_ID('R','I','F','F') ) return 0;

    stream_size = dumbfile_igetl(f);
    if ( stream_size + 8 > (unsigned)size ) return 0;
	if ( stream_size < 4 ) return 0;

    stream = (struct riff *) malloc( sizeof( struct riff ) );
	if ( ! stream ) return 0;

    stream->type = dumbfile_mgetl(f);
	stream->chunk_count = 0;
	stream->chunks = 0;

	stream_size -= 4;

    while ( stream_size && !dumbfile_error(f) )
	{
		struct riff_chunk * chunk;
		if ( stream_size < 8 ) break;
        stream->chunks = ( struct riff_chunk * ) realloc( stream->chunks, ( stream->chunk_count + 1 ) * sizeof( struct riff_chunk ) );
		if ( ! stream->chunks ) break;
		chunk = stream->chunks + stream->chunk_count;
        chunk->type = dumbfile_mgetl(f);
        chunk->size = dumbfile_igetl(f);
        chunk->offset = dumbfile_pos(f);
		stream_size -= 8;
		if ( stream_size < chunk->size ) break;
        if ( chunk->type == DUMB_ID('R','I','F','F') )
		{
            chunk->nested = riff_parse( f, chunk->offset - 8, chunk->size + 8, proper );
            if ( ! chunk->nested ) break;
		}
		else
		{
            chunk->nested = 0;
		}
        dumbfile_seek(f, chunk->offset + chunk->size, DFS_SEEK_SET);
		stream_size -= chunk->size;
		if ( proper && ( chunk->size & 1 ) )
		{
            dumbfile_skip(f, 1);
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
                if ( chunk->nested ) riff_free( chunk->nested );
			}
			free( stream->chunks );
		}
		free( stream );
	}
}
