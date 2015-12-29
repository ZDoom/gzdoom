/*
 wm_error.c
 error reporting

 Copyright (C) Chris Ison  2001-2011
 Copyright (C) Bret Curtis 2013-2014

 This file is part of WildMIDI.

 WildMIDI is free software: you can redistribute and/or modify the player
 under the terms of the GNU General Public License and you can redistribute
 and/or modify the library under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation, either version 3 of
 the licenses, or(at your option) any later version.

 WildMIDI is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License and
 the GNU Lesser General Public License for more details.

 You should have received a copy of the GNU General Public License and the
 GNU Lesser General Public License along with WildMIDI.  If not,  see
 <http://www.gnu.org/licenses/>.
 */

//#include "config.h"

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include "wm_error.h"
#include "doomtype.h"
#include "v_text.h"

void _WM_ERROR_NEW(const char * wmfmt, ...) {
	va_list args;
	fprintf(stderr, "\r");
	va_start(args, wmfmt);
	vfprintf(stderr, wmfmt, args);
	va_end(args);
	fprintf(stderr, "\n");
}

void _WM_ERROR(const char * func, unsigned int lne, int wmerno,
		const char * wmfor, int error) {

	static const char *errors[WM_ERR_MAX+1] = {
		"No error",
		"Unable to obtain memory",
		"Unable to stat",
		"Unable to load",
		"Unable to open",
		"Unable to read",
		"Invalid or Unsuported file format",
		"File corrupt",
		"Library not Initialized",
		"Invalid argument",
		"Library Already Initialized",
		"Not a midi file",
		"Refusing to load unusually long file",

		"Invalid error code"
	};

	if (wmerno < 0 || wmerno > WM_ERR_MAX)
		wmerno = WM_ERR_MAX;

	if (wmfor != NULL) {
		if (error != 0) {
			Printf(TEXTCOLOR_RED "libWildMidi(%s:%u): ERROR %s %s (%s)\n", func,
					lne, errors[wmerno], wmfor, strerror(error));
		} else {
			Printf(TEXTCOLOR_RED "libWildMidi(%s:%u): ERROR %s %s\n", func, lne,
					errors[wmerno], wmfor);
		}
	} else {
		if (error != 0) {
			Printf(TEXTCOLOR_RED "libWildMidi(%s:%u): ERROR %s (%s)\n", func, lne,
					errors[wmerno], strerror(error));
		} else {
			Printf(TEXTCOLOR_RED "libWildMidi(%s:%u): ERROR %s\n", func, lne,
					errors[wmerno]);
		}
	}
}

