/*
    TiMidity++ -- MIDI to WAVE converter and player
    Copyright (C) 1999-2008 Masanao Izumo <iz@onicos.co.jp>
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
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/

#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include <shlobj.h>
#endif
#include <time.h>

#include <signal.h>

#include "timidity.h"
#include "common.h"
#include "instrum.h"
#include "playmidi.h"
#include "tables.h"
#include "reverb.h"
#include "resample.h"
#include "recache.h"
#include "aq.h"
#include "mix.h"
#include "quantity.h"
#include "c_cvars.h"

namespace TimidityPlus
{
	Instruments *instruments;

/* main interfaces (To be used another main) */

int timidity_pre_load_configuration(void);
void timidity_init_player(void);
int timidity_play_main(int nfiles, char **files);
int got_a_configuration;


CRITICAL_SECTION critSect;

/* -------- functions for getopt_long ends here --------- */



int timidity_pre_load_configuration(void)
{
    /* Windows */
    char *strp;
    char local[1024];


	/* First, try read configuration file which is in the
     * TiMidity directory.
     */
    if(GetModuleFileNameA(NULL, local, 1023))
    {
        local[1023] = '\0';
		if (strp = strrchr(local, '\\'))
		{
			*(++strp) = '\0';
			strncat(local, "TIMIDITY.CFG", sizeof(local) - strlen(local) - 1);
			if (true)
			{
				if (!instruments->load("timidity.cfg"))
				{
					got_a_configuration = 1;
					return 0;
				}
			}
		}
    }

    return 0;
}


int dumb_pass_playing_list(int number_of_files, char *list_of_files[]);

int timidity_play_main(int nfiles, char **files)
{
    int need_stdin = 0, need_stdout = 0;
    int output_fail = 0;
    int retval;


#ifdef _WIN32


	InitializeCriticalSection(&critSect);

#endif

	/* Open output device */

	if(play_mode->open_output() < 0)
	{
	    output_fail = 1;
	    return 2;
	}

	retval=dumb_pass_playing_list(nfiles, files);


	play_mode->close_output();
#ifdef _WIN32
	DeleteCriticalSection (&critSect);
#endif

    return retval;
}

}

using namespace TimidityPlus;

void main(int argc, char **argv)
{
	int  err;
	char *files_nbuf = NULL;
	int main_ret;
	instruments = new Instruments;

	if ((err = timidity_pre_load_configuration()) != 0)
			return;

	main_ret = timidity_play_main(1, &argv[1]);
	//free_readmidi();
	free_global_mblock();
}

