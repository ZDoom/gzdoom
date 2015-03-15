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


DUH *dumb_read_riff_amff( DUMBFILE * f, struct riff * stream );
DUH *dumb_read_riff_am( DUMBFILE * f, struct riff * stream );
DUH *dumb_read_riff_dsmf( DUMBFILE * f, struct riff * stream );

/* dumb_read_riff_quick(): reads a RIFF file into a DUH struct, returning a
 * pointer to the DUH struct. When you have finished with it, you must pass
 * the pointer to unload_duh() so that the memory can be freed.
 */
DUH *DUMBEXPORT dumb_read_riff_quick( DUMBFILE * f )
{
	DUH * duh;
	struct riff * stream;
    long size;

    size = dumbfile_get_size(f);

    stream = riff_parse( f, 0, size, 1 );
    if ( ! stream ) stream = riff_parse( f, 0, size, 0 );

	if ( ! stream ) return 0;

	if ( stream->type == DUMB_ID( 'A', 'M', ' ', ' ' ) )
        duh = dumb_read_riff_am( f, stream );
	else if ( stream->type == DUMB_ID( 'A', 'M', 'F', 'F' ) )
        duh = dumb_read_riff_amff( f, stream );
	else if ( stream->type == DUMB_ID( 'D', 'S', 'M', 'F' ) )
        duh = dumb_read_riff_dsmf( f, stream );
	else duh = 0;

	riff_free( stream );

	return duh;
}
