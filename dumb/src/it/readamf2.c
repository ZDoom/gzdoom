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
 * readamf2.c - Function to read a DSMI AMF module    / / \  \
 *              from an open file and do an initial  | <  /   \_
 *              run-through.                         |  \/ /\   /
 *                                                    \_  /  > /
 *                                                      | \ / /
 *                                                      |  ' /
 *                                                       \__/
 */

#include "dumb.h"



DUH *DUMBEXPORT dumb_read_amf(DUMBFILE *f)
{
	DUH *duh = dumb_read_amf_quick(f);
	dumb_it_do_initial_runthrough(duh);
	return duh;
}
