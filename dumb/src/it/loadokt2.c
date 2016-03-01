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
 * loadokt2.c - Function to read an Oktalyzer         / / \  \
 *              module file, opening and closing     | <  /   \_
 *              it for you, and do an initial run-   |  \/ /\   /
 *              through.                              \_  /  > /
 *                                                      | \ / /
 * By Chris Moeller.                                    |  ' /
 *                                                       \__/
 */

#include "dumb.h"



DUH *DUMBEXPORT dumb_load_okt(const char *filename)
{
	DUH *duh = dumb_load_okt_quick(filename);
	dumb_it_do_initial_runthrough(duh);
	return duh;
}
