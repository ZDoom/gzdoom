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
 * loadany.c - Code to detect and read any of the     / / \  \
 *             module formats supported by DUMB,     | <  /   \_
 *             opening and closing the file for you. |  \/ /\   /
 *                                                    \_  /  > /
 * By Chris Moeller.                                    | \ / /
 *                                                      |  ' /
 *                                                       \__/
 */

#include "dumb.h"
#include "internal/it.h"



DUH *DUMBEXPORT dumb_load_any_quick(const char *filename, int restrict_, int subsong)
{
	DUH *duh;
	DUMBFILE *f = dumbfile_open(filename);

	if (!f)
		return NULL;

    duh = dumb_read_any_quick(f, restrict_, subsong);

	dumbfile_close(f);

	return duh;
}
