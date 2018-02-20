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

    output.h

*/

#ifndef ___OUTPUT_H_
#define ___OUTPUT_H_

namespace TimidityPlus
{



/* Data format encoding bits */
#define PE_16BIT 	(1u<<2)  /* versus 8-bit */
#define PE_24BIT	(1u<<6)  /* versus 8-bit, 16-bit */

/* for play_mode->acntl() */
enum {
    PM_REQ_DISCARD,	/* ARG: not-used
			 * Discard the audio device buffer and returns
			 * immediatly.
			 */

    PM_REQ_FLUSH,	/* ARG: not-used
			 * Wait until all audio data is out.
			 */

};


/* Flag bits */
#define PF_PCM_STREAM	(1u<<0)	/* Enable output PCM data */
#define PF_BUFF_FRAGM_OPT (1u<<3) /* Enable set extra_param[0] to specify
				   the number of audio buffer fragments */
#define PF_AUTO_SPLIT_FILE (1u<<4) /* Split PCM files automatically */
#define PF_FILE_OUTPUT (1u<<5) /* Output is to file rather than device */

struct PlayMode {
    int32_t rate, encoding, flag;
    int fd; /* file descriptor for the audio device
	       -1 means closed otherwise opened. It must be -1 by default. */
    int32_t extra_param[5]; /* System depended parameters
			     e.g. buffer fragments, ... */
	const char *id_name;
	char id_character;
	const char *name; /* default device or file name */
    int (* open_output)(void); /* 0=success, 1=warning, -1=fatal error */
    void (* close_output)(void);

    int (* output_data)(char *buf, int32_t bytes);
    /* return: -1=error, otherwise success */

    int (* acntl)(int request, void *arg); /* see PM_REQ_* above
					    * return: 0=success, -1=fail
					    */
    int (* detect)(void); /* 0=not available, 1=available */
};

extern PlayMode *play_mode_list[], *play_mode;

/* 16-bit */
extern void s32tos16(int32_t *lp, int32_t c);

/* 24-bit */
extern void s32tos24(int32_t *lp, int32_t c);

extern int32_t general_output_convert(int32_t *buf, int32_t count);


}

#endif /* ___OUTPUT_H_ */

