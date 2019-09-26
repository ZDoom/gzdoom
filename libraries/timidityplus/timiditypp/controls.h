/*
    TiMidity++ -- MIDI to WAVE converter and player
    Copyright (C) 1999-2002 Masanao Izumo <mo@goice.co.jp>
    Copyright (C) 1995 Tuukka Toivonen <tt@cgs.fi>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

    controls.h
*/

#ifndef ___CONTROLS_H_
#define ___CONTROLS_H_


namespace TimidityPlus
{

enum
{
	/* Return values for ControlMode.read */
	RC_ERROR = -1,
	RC_OK = 0,
	RC_QUIT = 1,
	RC_TUNE_END = 3,
	RC_STOP = 4,	/* Stop to play */

	CMSG_INFO = 0,
	CMSG_WARNING = 1,
	CMSG_ERROR = 2,

	VERB_NORMAL = 0,
	VERB_VERBOSE = 1,
	VERB_NOISY = 2,
	VERB_DEBUG = 3,
};

inline bool RC_IS_SKIP_FILE(int rc)
{
	return ((rc) == RC_QUIT || (rc) == RC_ERROR || (rc) == RC_STOP || (rc) == RC_TUNE_END);
}


extern void (*printMessage)(int type, int verbosity_level, const char* fmt, ...);


}

#endif /* ___CONTROLS_H_ */
