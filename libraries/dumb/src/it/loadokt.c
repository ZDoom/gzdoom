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
 * loadokt.c - Code to read an Oktalyzer module       / / \  \
 *             file, opening and closing it for      | <  /   \_
 *             you.                                  |  \/ /\   /
 *                                                    \_  /  > /
 * By Chris Moeller.                                    | \ / /
 *                                                      |  ' /
 *                                                       \__/
 */

#include "dumb.h"
#include "internal/it.h"



/* dumb_load_okt_quick(): loads an OKT file into a DUH struct, returning a
 * pointer to the DUH struct. When you have finished with it, you must
 * pass the pointer to unload_duh() so that the memory can be freed.
 */
DUH *DUMBEXPORT dumb_load_okt_quick(const char *filename)
{
	DUH *duh;
	DUMBFILE *f = dumbfile_open(filename);

	if (!f)
		return NULL;

	duh = dumb_read_okt_quick(f);

	dumbfile_close(f);

	return duh;
}
