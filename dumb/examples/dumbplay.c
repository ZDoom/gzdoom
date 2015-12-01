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
 * dumbplay.c - Not-so-simple program to play         / / \  \
 *              music. It used to be simpler!        | <  /   \_
 *                                                   |  \/ /\   /
 * By entheh.                                         \_  /  > /
 *                                                      | \ / /
 * IMPORTANT NOTE: This file is not very friendly.      |  ' /
 * I strongly recommend AGAINST using it as a            \__/
 * reference for writing your own code. If you would
 * like to write a program that uses DUMB, or add DUMB to an existing
 * project, please use docs/howto.txt. It will help you a lot more than this
 * file can. (If you have difficulty reading documentation, you are lacking
 * an important coding skill, and now is as good a time as any to learn.)
 */

#include <stdlib.h>
#include <allegro.h>

#ifndef ALLEGRO_DOS
#include <string.h>
#endif

/* Note that your own programs should use <aldumb.h> not "aldumb.h". <> tells
 * the compiler to look in the compiler's default header directory, which is
 * where DUMB should be installed before you use it (make install does this).
 * Use "" when it is your own header file. This example uses "" because DUMB
 * might not have been installed yet when the makefile builds it.
 */
#include "aldumb.h"



#ifndef ALLEGRO_DOS
static volatile int closed = 0;
static void closehook(void) { closed = 1; }
#else
#define closed 0
#endif

#ifdef ALLEGRO_WINDOWS
#define GFX_DUMB_MODE GFX_GDI
#include <winalleg.h>
#define YIELD() Sleep(1)
#else
#define GFX_DUMB_MODE GFX_AUTODETECT_WINDOWED
#ifdef ALLEGRO_UNIX
#include <sys/time.h>
static void YIELD(void)
{
	struct timeval tv;
	tv.tv_sec = 0;
	tv.tv_usec = 1;
	select(0, NULL, NULL, NULL, &tv);
}
#else
#define YIELD() yield_timeslice()
#endif
#endif



#ifdef ALLEGRO_DOS
static int loop_callback(void *data)
{
	(void)data;
	printf("Music has looped.\n");
	return 0;
}

static int xm_speed_zero_callback(void *data)
{
	(void)data;
	printf("Music has stopped.\n");
	return 0;
}
#else
static int gfx_half_width;

static int loop_callback(void *data)
{
	(void)data;
	if (gfx_half_width) {
		acquire_screen();
		textout_centre(screen, font, "Music has looped.", gfx_half_width, 36, 10);
		release_screen();
	}
	return 0;
}

static int xm_speed_zero_callback(void *data)
{
	(void)data;
	if (gfx_half_width) {
		text_mode(0); /* In case this is overwriting "Music has looped." */
		acquire_screen();
		textout_centre(screen, font, "Music has stopped.", gfx_half_width, 36, 10);
		release_screen();
	}
	return 0;
}
#endif



static void usage(const char *exename)
{
	allegro_message(
#ifdef ALLEGRO_WINDOWS
		"Usage:\n"
		"  At the command line: %s file\n"
		"  In Windows Explorer: drag a file on to this program's icon.\n"
#else
		"Usage: %s file\n"
#endif
		"This will play the music file specified.\n"
		"File formats supported: IT XM S3M MOD.\n"
		"You can control playback quality by editing dumb.ini.\n", exename);

	exit(1);
}



int main(int argc, const char *const *argv) /* I'm const-crazy! */
{
	DUH *duh;
	AL_DUH_PLAYER *dp;

	if (allegro_init())
		return 1;

	if (argc != 2)
		usage(argv[0]);

	set_config_file("dumb.ini");

	if (install_keyboard()) {
		allegro_message("Failed to initialise keyboard driver!\n");
		return 1;
	}

	set_volume_per_voice(0);

	if (install_sound(DIGI_AUTODETECT, MIDI_NONE, NULL)) {
		allegro_message("Failed to initialise sound driver!\n%s\n", allegro_error);
		return 1;
	}

	atexit(&dumb_exit);

	dumb_register_packfiles();

	duh = dumb_load_it(argv[1]);
	if (!duh) {
		duh = dumb_load_xm(argv[1]);
		if (!duh) {
			duh = dumb_load_s3m(argv[1]);
			if (!duh) {
				duh = dumb_load_mod(argv[1]);
				if (!duh) {
					allegro_message("Failed to load %s!\n", argv[1]);
					return 1;
				}
			}
		}
	}

	dumb_resampling_quality = get_config_int("sound", "dumb_resampling_quality", 4);
	dumb_it_max_to_mix = get_config_int("sound", "dumb_it_max_to_mix", 128);

#ifndef ALLEGRO_DOS
	{
		const char *fn = get_filename(argv[1]);
		gfx_half_width = strlen(fn);
		if (gfx_half_width < 22) gfx_half_width = 22;
		gfx_half_width = (gfx_half_width + 2) * 4;

		/* set_window_title() is not const-correct (yet). */
		set_window_title((char *)"DUMB Music Player");

		if (set_gfx_mode(GFX_DUMB_MODE, gfx_half_width*2, 80, 0, 0) == 0) {
			acquire_screen();
			textout_centre(screen, font, fn, gfx_half_width, 20, 14);
			textout_centre(screen, font, "Press any key to exit.", gfx_half_width, 52, 11);
			release_screen();
		} else
			gfx_half_width = 0;
	}

#if ALLEGRO_VERSION*10000 + ALLEGRO_SUB_VERSION*100 + ALLEGRO_WIP_VERSION >= 40105
	set_close_button_callback(&closehook);
#else
	set_window_close_hook(&closehook);
#endif

#endif

	set_display_switch_mode(SWITCH_BACKGROUND);

	dp = al_start_duh(duh, 2, 0, 1.0f,
		get_config_int("sound", "buffer_size", 4096),
		get_config_int("sound", "sound_freq", 44100));

	{
		DUH_SIGRENDERER *sr = al_duh_get_sigrenderer(dp);
		DUMB_IT_SIGRENDERER *itsr = duh_get_it_sigrenderer(sr);
		dumb_it_set_loop_callback(itsr, &loop_callback, NULL);
		dumb_it_set_xm_speed_zero_callback(itsr, &xm_speed_zero_callback, NULL);
	}

	for (;;) {
		if (keypressed()) {
			readkey();
			break;
		}

		if (al_poll_duh(dp) || closed)
			break;

		YIELD();
	}

	al_stop_duh(dp);

	unload_duh(duh);

	return 0;
}
END_OF_MAIN();
