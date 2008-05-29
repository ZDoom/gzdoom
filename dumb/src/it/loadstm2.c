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
 * loadstm2.c - Function to read a ScreamTracker 2    / / \  \
 *              file, opening and closing it for     | <  /   \_
 *              you, and do an initial run-through.  |  \/ /\   /
 *                                                    \_  /  > /
 * By Chris Moeller.                                    | \ / /
 *                                                      |  ' /
 *                                                       \__/
 */

#include "dumb.h"



DUH *DUMBEXPORT dumb_load_stm(const char *filename)
{
	DUH *duh = dumb_load_stm_quick(filename);
	dumb_it_do_initial_runthrough(duh);
	return duh;
}
