/*  _______         ____    __         ___    ___
 * \    _  \       \    /  \  /       \   \  /   /       '   '  '
 *  |  | \  \       |  |    ||         |   \/   |         .      .
 *  |  |  |  |      |  |    ||         ||\  /|  |
 *  |  |  |  |      |  |    ||         || \/ |  |         '  '  '
 *  |  |  |  |      |  |    ||         ||    |  |         .      .
 *  |  |_/  /        \  \__//          ||    |  |
 * /_______/ynamic    \____/niversal  /__\  /____\usic   /|  .  . ibliotheque
 *                                                      /  \
 *                                                     / .  \
 * readriff.c - Code to read a RIFF module file       / / \  \
 *              from memory.                         | <  /   \_
 *                                                   |  \/ /\   /
 *                                                    \_  /  > /
 * By Chris Moeller.                                    | \ / /
 *                                                      |  ' /
 *                                                       \__/
 */

#include "dumb.h"
#include "internal/it.h"
#include "internal/riff.h"


DUH *dumb_read_riff_amff( struct riff * stream );
DUH *dumb_read_riff_am( struct riff * stream );
DUH *dumb_read_riff_dsmf( struct riff * stream );

/* dumb_read_riff_quick(): reads a RIFF file into a DUH struct, returning a
 * pointer to the DUH struct. When you have finished with it, you must pass
 * the pointer to unload_duh() so that the memory can be freed.
 */
DUH *DUMBEXPORT dumb_read_riff_quick( DUMBFILE * f )
{
	DUH * duh;
	struct riff * stream;

	{
		unsigned char * buffer = 0;
		int32 size = 0;
		int32 read;
		do
		{
			buffer = realloc( buffer, 32768 + size );
			if ( ! buffer ) return 0;
			read = dumbfile_getnc( buffer + size, 32768, f );
			if ( read < 0 )
			{
				free( buffer );
				return 0;
			}
			size += read;
		}
		while ( read == 32768 );
		stream = riff_parse( buffer, size, 1 );
		if ( ! stream ) stream = riff_parse( buffer, size, 0 );
		free( buffer );
	}

	if ( ! stream ) return 0;

	if ( stream->type == DUMB_ID( 'A', 'M', ' ', ' ' ) )
		duh = dumb_read_riff_am( stream );
	else if ( stream->type == DUMB_ID( 'A', 'M', 'F', 'F' ) )
		duh = dumb_read_riff_amff( stream );
	else if ( stream->type == DUMB_ID( 'D', 'S', 'M', 'F' ) )
		duh = dumb_read_riff_dsmf( stream );
	else duh = 0;

	riff_free( stream );

	return duh;
}
