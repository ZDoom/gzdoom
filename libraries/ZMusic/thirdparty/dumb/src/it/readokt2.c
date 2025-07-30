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
 * readokt2.c - Function to read an Oktalyzer         / / \  \
 *              module from an open file and do      | <  /   \_
 *              an initial run-through.              |  \/ /\   /
 *                                                    \_  /  > /
 *                                                      | \ / /
 * By Chris Moeller.                                    |  ' /
 *                                                       \__/
 */

#include "dumb.h"



DUH *DUMBEXPORT dumb_read_okt(DUMBFILE *f)
{
	DUH *duh = dumb_read_okt_quick(f);
	dumb_it_do_initial_runthrough(duh);
	return duh;
}
