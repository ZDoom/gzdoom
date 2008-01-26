/*
 Author: Marcus Boerger <helly@users.sourceforge.net>
*/

/* $Id: mbo_getopt.h 539 2006-05-25 13:37:38Z helly $ */

/* Define structure for one recognized option (both single char and long name).
 * If short_open is '-' this is the last option.
 */

#ifndef RE2C_MBO_GETOPT_H_INCLUDE_GUARD_
#define RE2C_MBO_GETOPT_H_INCLUDE_GUARD_

namespace re2c
{

struct mbo_opt_struct
{
	mbo_opt_struct(char _opt_char, int _need_param, const char * _opt_name)
		: opt_char(_opt_char), need_param(_need_param), opt_name(_opt_name)
	{
	}

	const char opt_char;
	const int need_param;
	const char * opt_name;
};

int mbo_getopt(int argc, char* const *argv, const mbo_opt_struct *opts, char **optarg, int *optind, int show_err);

} // end namespace re2c

#endif

