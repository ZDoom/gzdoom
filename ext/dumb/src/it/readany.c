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
 * readany.c - Code to detect and read any of the     / / \  \
 *             module formats supported by DUMB.     | <  /   \_
 *                                                   |  \/ /\   /
 * By Chris Moeller.                                  \_  /  > /
 *                                                      | \ / /
 *                                                      |  ' /
 *                                                       \__/
 */

#include <stdlib.h>
#include <string.h>

#include "dumb.h"

#ifdef _MSC_VER
	#define strnicmp _strnicmp
#else
	#if defined(unix) || defined(__unix__) || defined(__unix)
		#include <strings.h>
	#endif
	#define strnicmp strncasecmp
#endif

enum { maximum_signature_size = 0x30 };

DUH *DUMBEXPORT dumb_read_any_quick(DUMBFILE *f, int restrict_, int subsong)
{
    unsigned char signature[ maximum_signature_size ];
    unsigned long signature_size;
    DUH * duh = NULL;

    signature_size = dumbfile_get_size(f);

    signature_size = dumbfile_getnc( (char *)signature, maximum_signature_size, f );
    dumbfile_seek( f, 0, DFS_SEEK_SET );

    if (signature_size >= 4 &&
        signature[0] == 'I' && signature[1] == 'M' &&
        signature[2] == 'P' && signature[3] == 'M')
    {
        duh = dumb_read_it_quick( f );
    }
    else if (signature_size >= 17 && !memcmp(signature, "Extended Module: ", 17))
    {
        duh = dumb_read_xm_quick( f );
    }
    else if (signature_size >= 0x30 &&
        signature[0x2C] == 'S' && signature[0x2D] == 'C' &&
        signature[0x2E] == 'R' && signature[0x2F] == 'M')
    {
        duh = dumb_read_s3m_quick( f );
    }
    else if (signature_size >= 30 &&
        /*signature[28] == 0x1A &&*/ signature[29] == 2 &&
        ( ! strnicmp( ( const char * ) signature + 20, "!Scream!", 8 ) ||
        ! strnicmp( ( const char * ) signature + 20, "BMOD2STM", 8 ) ||
        ! strnicmp( ( const char * ) signature + 20, "WUZAMOD!", 8 ) ) )
    {
        duh = dumb_read_stm_quick( f );
    }
    else if (signature_size >= 2 &&
        ((signature[0] == 0x69 && signature[1] == 0x66) ||
        (signature[0] == 0x4A && signature[1] == 0x4E)))
    {
        duh = dumb_read_669_quick( f );
    }
    else if (signature_size >= 0x30 &&
        signature[0x2C] == 'P' && signature[0x2D] == 'T' &&
        signature[0x2E] == 'M' && signature[0x2F] == 'F')
    {
        duh = dumb_read_ptm_quick( f );
    }
    else if (signature_size >= 4 &&
        signature[0] == 'P' && signature[1] == 'S' &&
        signature[2] == 'M' && signature[3] == ' ')
    {
        duh = dumb_read_psm_quick( f, subsong );
    }
    else if (signature_size >= 4 &&
        signature[0] == 'P' && signature[1] == 'S' &&
        signature[2] == 'M' && signature[3] == 254)
    {
        duh = dumb_read_old_psm_quick( f );
    }
    else if (signature_size >= 3 &&
        signature[0] == 'M' && signature[1] == 'T' &&
        signature[2] == 'M')
    {
        duh = dumb_read_mtm_quick( f );
    }
    else if ( signature_size >= 4 &&
        signature[0] == 'R' && signature[1] == 'I' &&
        signature[2] == 'F' && signature[3] == 'F')
    {
        duh = dumb_read_riff_quick( f );
    }
    else if ( signature_size >= 24 &&
        !memcmp( signature, "ASYLUM Music Format", 19 ) &&
        !memcmp( signature + 19, " V1.0", 5 ) )
    {
        duh = dumb_read_asy_quick( f );
    }
    else if ( signature_size >= 3 &&
        signature[0] == 'A' && signature[1] == 'M' &&
        signature[2] == 'F')
    {
        duh = dumb_read_amf_quick( f );
    }
    else if ( signature_size >= 8 &&
        !memcmp( signature, "OKTASONG", 8 ) )
    {
        duh = dumb_read_okt_quick( f );
    }

    if ( !duh )
    {
        dumbfile_seek( f, 0, DFS_SEEK_SET );
        duh = dumb_read_mod_quick( f, restrict_ );
    }

    return duh;
}
