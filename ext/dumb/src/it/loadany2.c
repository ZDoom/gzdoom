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
 * loadany2.c - Code to detect and read any of the    / / \  \
 *              module formats supported by DUMB,    | <  /   \_
 *              opening and closing the file for     |  \/ /\   /
 *              you, and do an initial run-through.   \_  /  > /
 *                                                      | \ / /
 * by Chris Moeller.                                    |  ' /
 *                                                       \__/
 */

#include "dumb.h"



DUH *DUMBEXPORT dumb_load_any(const char *filename, int restrict_, int subsong)
{
    DUH *duh = dumb_load_any_quick(filename, restrict_, subsong);
	dumb_it_do_initial_runthrough(duh);
	return duh;
}
