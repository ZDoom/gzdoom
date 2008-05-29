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
 * loadmtm2.c - Code to read a MultiTracker Module    / / \  \
 *              file, opening and closing it for     | <  /   \_
 *              you, and do an initial run-through.  |  \/ /\   /
 *                                                    \_  /  > /
 * By Chris Moeller                                     | \ / /
 *                                                      |  ' /
 *                                                       \__/
 */

#include "dumb.h"
#include "internal/it.h"



/* dumb_load_mtm(): loads a MTM file into a DUH struct, returning a pointer
 * to the DUH struct. When you have finished with it, you must pass the
 * pointer to unload_duh() so that the memory can be freed.
 */
DUH *DUMBEXPORT dumb_load_mtm(const char *filename)
{
	DUH *duh = dumb_load_mtm_quick(filename);
	dumb_it_do_initial_runthrough(duh);
	return duh;
}
