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
 * loadmod2.c - Function to read a good old-          / / \  \
 *              fashioned Amiga module file,         | <  /   \_
 *              opening and closing it for you,      |  \/ /\   /
 *              and do an initial run-through.        \_  /  > /
 *                                                      | \ / /
 * Split off from loadmod.c by entheh.                  |  ' /
 *                                                       \__/
 */

#include "dumb.h"



DUH *DUMBEXPORT dumb_load_mod(const char *filename, int restrict_)
{
	DUH *duh = dumb_load_mod_quick(filename, restrict_);
	dumb_it_do_initial_runthrough(duh);
	return duh;
}
