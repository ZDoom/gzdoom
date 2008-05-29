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
 * dumb2wav.c - Utility to convert DUH to WAV.        / / \  \
 *                                                   | <  /   \_
 * By Chad Austin, based on dumbout.c by entheh.     |  \/ /\   /
 *                                                    \_  /  > /
 *                                                      | \ / /
 *                                                      |  ' /
 *                                                       \__/
 */

#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <math.h>
#include <string.h>
#include <dumb.h>

#include <internal/it.h>

union {
	float s32[4096];
	short s16[8192];
	char s8[16384];
} buffer;

sample_t ** internal_buffer;

int loop_count = 1;


static int write32_le(FILE* outf, unsigned int value) {
	int total = 0;
	total += fputc(value & 0xFF,         outf);
	total += fputc((value >> 8)  & 0xFF, outf);
	total += fputc((value >> 16) & 0xFF, outf);
	total += fputc((value >> 24) & 0xFF, outf);
	return total;
}

static int write16_le(FILE* outf, unsigned int value) {
	int total = 0;
	total += fputc(value & 0xFF,        outf);
	total += fputc((value >> 8) & 0xFF, outf);
	return total;
}


static int loop_callback(void* data) {
	return (--loop_count <= 0 ? -1 : 0);
}


int main(int argc, const char *argv[])
{
	DUH *duh;
	DUH_SIGRENDERER *sr;

	const char *fn = NULL;
	const char *fn_out = NULL;
	FILE *outf;

	int depth = 16;
	int unsign = 0;
	int freq = 44100;
	int n_channels = 2;
	int solo = -1;
	float volume = 1.0f;
	float delay = 0.0f;
	float delta;
	int bufsize;
	clock_t start, end;
	int data_written = 0;  /* total bytes written to data chunk */

	int i = 1;

	LONG_LONG length;
	LONG_LONG done;
	int dots;

	while (i < argc) {
		const char *arg = argv[i++];
		if (*arg != '-') {
			if (fn) {
				fprintf(stderr,
					"Cannot specify multiple filenames!\n"
					"Second filename found: \"%s\"\n", arg);
				return 1;
			}
			fn = arg;
			continue;
		}
		arg++;
		while (*arg) {
			char *endptr;
			switch (*arg++) {
				case 'o':
				case 'O':
					if (i >= argc) {
						fprintf(stderr, "Out of arguments; output filename expected!\n");
						return 1;
					}
					fn_out = argv[i++];
					break;
				case 'd':
				case 'D':
					if (i >= argc) {
						fprintf(stderr, "Out of arguments; delay expected!\n");
						return 1;
					}
					delay = (float)strtod(argv[i++], &endptr);
					if (*endptr != 0 || delay < 0.0f || delay > 64.0f) {
						fprintf(stderr, "Invalid delay!\n");
						return 1;
					}
					break;
				case 'v':
				case 'V':
					if (i >= argc) {
						fprintf(stderr, "Out of arguments; volume expected!\n");
						return 1;
					}
					volume = (float)strtod(argv[i++], &endptr);
					if (*endptr != 0 || volume < -8.0f || volume > 8.0f) {
						fprintf(stderr, "Invalid volume!\n");
						return 1;
					}
					break;
				case 's':
				case 'S':
					if (i >= argc) {
						fprintf(stderr, "Out of arguments; sampling rate expected!\n");
						return 1;
					}
					freq = strtol(argv[i++], &endptr, 10);
					if (*endptr != 0 || freq < 1 || freq > 960000) {
						fprintf(stderr, "Invalid sampling rate!\n");
						return 1;
					}
					break;
				case 'f':
					depth = 32;
					break;
				case '8':
					depth = 8;
					break;
				case 'l':
				case 'L':
					if (i >= argc) {
						fprintf(stderr, "Out of arguments: loop count expected!\n");
						return 1;
					}
					loop_count = strtol(argv[i++], &endptr, 10);
					break;
				case 'm':
				case 'M':
					n_channels = 1;
					break;
				case 'u':
				case 'U':
					unsign = 1;
					break;
				case 'r':
				case 'R':
					if (i >= argc) {
						fprintf(stderr, "Out of arguments; resampling quality expected!\n");
						return 1;
					}
					dumb_resampling_quality = strtol(argv[i++], &endptr, 10);
					if (*endptr != 0 || dumb_resampling_quality < 0 || dumb_resampling_quality > 2) {
						fprintf(stderr, "Invalid resampling quality!\n");
						return 1;
					}
					break;
				case 'c':
				case 'C':
					if (i >= argc) {
						fprintf(stderr, "Out of arguments; channel number expected!\n");
						return 1;
					}
					solo = strtol(argv[i++], &endptr, 10);
					if (*endptr != 0 || solo < 0 || solo >= DUMB_IT_N_CHANNELS) {
						fprintf(stderr, "Invalid channel number!\n");
						return 1;
					}
					break;
				default:
					fprintf(stderr, "Invalid switch - '%c'!\n", isprint(arg[-1]) ? arg[-1] : '?');
					return 1;
			}
		}
	}

	if (!fn) {
		fprintf(stderr,
			"Usage: dumb2wav [options] module [more-options]\n"
			"\n"
			"The module can be any IT, XM, S3M or MOD file. It will be rendered to a .wav\n"
			"file of the same name, unless you specify otherwise with the -o option.\n"
			"\n"
			"The valid options are:\n"
			"-o <file>   specify the output filename (defaults to the input filename with\n"
			"              the extension replaced with .wav); use - to write to standard\n"
			"              output or . to write nowhere (useful for measuring DUMB's\n"
			"              performance, and DOS and Windows don't have /dev/null!)\n"
			"-d <delay>  set the initial delay, in seconds (default 0.0)\n"
			"-v <volume> adjust the volume (default 1.0)\n"
			"-s <freq>   set the sampling rate in Hz (default 44100)\n"
			"-8          generate 8-bit instead of 16-bit\n"
			"-f          generate floating point samples instead of 16-bit\n"
			"-m          generate mono output instead of stereo left/right pairs\n"
			"-u          generated unsigned output instead of signed\n"
			"-r <value>  specify the resampling quality to use\n"
			"-l <value>  specify the number of times to loop (default 1)\n"
			"-c <value>  specify a channel number to solo\n");
		return 1;
	}

	atexit(&dumb_exit);
	dumb_register_stdfiles();

	dumb_it_max_to_mix = 256;

	duh = load_duh(fn);
	if (!duh) {
		duh = dumb_load_it(fn);
		if (!duh) {
			duh = dumb_load_xm(fn);
			if (!duh) {
				duh = dumb_load_s3m(fn);
				if (!duh) {
					duh = dumb_load_mod(fn);
					if (!duh) {
						fprintf(stderr, "Unable to open %s!\n", fn);
						return 1;
					}
				}
			}
		}
	}

	sr = duh_start_sigrenderer(duh, 0, n_channels, 0);
	if (!sr) {
		unload_duh(duh);
		fprintf(stderr, "Unable to play file!\n");
		return 1;
	}

	if (solo >= 0) {
		DUMB_IT_SIGRENDERER * itsr = duh_get_it_sigrenderer(sr);
		if (itsr) {
			for (i = 0; i < DUMB_IT_N_CHANNELS; i++) {
				if (i != solo) {
					IT_CHANNEL * channel = &itsr->channel[i];
					IT_PLAYING * playing = channel->playing;
					channel->flags |= IT_CHANNEL_MUTED;
					/* start_sigrenderer leaves me all of the channels the first tick triggered */
					if (playing) {
						playing->ramp_volume[0] = 0;
						playing->ramp_volume[1] = 0;
						playing->ramp_delta[0] = 0;
						playing->ramp_delta[1] = 0;
					}
				}
			}
		}
	}

	if (fn_out) {
		if (fn_out[0] == '-' && fn_out[1] == 0)
			outf = stdout;
		else if (fn_out[0] == '.' && fn_out[1] == 0)
			outf = NULL;
		else {
			outf = fopen(fn_out, "wb");
			if (!outf) {
				fprintf(stderr, "Unable to open %s for writing!\n", fn_out);
				duh_end_sigrenderer(sr);
				unload_duh(duh);
				return 1;
			}
		}
	} else {
		char *extptr = NULL, *p;
		char *fn_out = malloc(strlen(fn)+5);
		if (!fn_out) {
			fprintf(stderr, "Out of memory!\n");
			duh_end_sigrenderer(sr);
			unload_duh(duh);
			return 1;
		}
		strcpy(fn_out, fn);
		for (p = fn_out; *p; p++)
			if (*p == '.') extptr = p;
		if (!extptr) extptr = p;
		strcpy(extptr, ".wav");
		outf = fopen(fn_out, "wb");
		if (!outf) {
			fprintf(stderr, "Unable to open %s for writing!\n", fn_out);
			free(fn_out);
			duh_end_sigrenderer(sr);
			unload_duh(duh);
			return 1;
		}
		free(fn_out);
	}

	{
		DUMB_IT_SIGRENDERER *itsr = duh_get_it_sigrenderer(sr);
		dumb_it_set_ramp_style(itsr, 2);
		dumb_it_set_loop_callback(itsr, loop_callback, NULL);
		dumb_it_set_xm_speed_zero_callback(itsr, &dumb_it_callback_terminate, NULL);
		dumb_it_set_global_volume_zero_callback(itsr, &dumb_it_callback_terminate, NULL);
	}


	if (outf) {
		/* write RIFF header: fill file length later */
		fwrite("RIFF", 1, 4, outf);
		fwrite("    ", 1, 4, outf);
		fwrite("WAVE", 1, 4, outf);

		/* write format chunk */
		fwrite("fmt ", 1, 4, outf);

		if (depth == 32)
		{
			write32_le(outf, 18);
			write16_le(outf, 3);
		}
		else
		{
			write32_le(outf, 16);         /* header length */
			write16_le(outf, 1);          /* WAVE_FORMAT_PCM */
		}
		write16_le(outf, n_channels); /* channel count */
		write32_le(outf, freq);       /* frequency */
		write32_le(outf, freq * n_channels * depth / 8); /*bytes/sec*/
		write16_le(outf, n_channels * depth / 8); /* block alignment */
		write16_le(outf, depth);      /* bits per sample */

		if (depth == 32)
		{
			write16_le(outf, 0);
		}

		/* start data chunk */
		fwrite("data", 1, 4, outf);
		fwrite("    ", 1, 4, outf);  /* fill in later */
	}

	length = (LONG_LONG)_dumb_it_build_checkpoints(duh_get_it_sigdata(duh), 0) * freq >> 16;
	done = 0;
	dots = 0;
	delta = 65536.0f / freq;
	bufsize = sizeof(buffer);
	if (depth == 32) bufsize /= sizeof(*buffer.s32);
	else if (depth == 16) bufsize /= sizeof(*buffer.s16);
	bufsize /= n_channels;

	if (depth == 32) {
		internal_buffer = create_sample_buffer(n_channels, bufsize);
		if (!internal_buffer) {
			fprintf(stderr, "Out of memory!\n");
			duh_end_sigrenderer(sr);
			unload_duh(duh);
		}
	}

	{
		long l = (long)floor(delay * freq + 0.5f);
		l *= n_channels * (depth >> 3);
		if (l) {
			if (unsign && depth != 32) {
				if (depth == 16) {
					for (i = 0; i < 8192; i++) {
						buffer.s8[i*2] = 0x00;
						buffer.s8[i*2+1] = 0x80;
					}
				} else
					memset(buffer.s8, 0x80, 16384);
			} else
				memset(buffer.s8, 0, 16384);
			while (l >= 16384) {
				if (outf) fwrite(buffer.s8, 1, 16384, outf);
				l -= 16384;
				data_written += 16384;
			}
			if (l) {
				if (outf) fwrite(buffer.s8, 1, l, outf);
				data_written += 1;
			}
		}
	}

	start = clock();

	fprintf(stderr, "................................................................\n");
	for (;;) {
		int write_size;
		int l;
		
		if (depth != 32) {
			l = duh_render(sr, depth, unsign, volume, delta, bufsize, &buffer);
			if (depth == 16) {
				for (i = 0; i < l * n_channels; i++) {
					short val = buffer.s16[i];
					buffer.s8[i*2] = val;
					buffer.s8[i*2+1] = val >> 8;
				}
			}
		} else {
			int j;
			dumb_silence(internal_buffer[0], bufsize * n_channels);
			l = duh_sigrenderer_get_samples(sr, volume, delta, bufsize, internal_buffer);
			for (i = 0; i < n_channels; i++) {
				for (j = 0; j < l; j++) {
					buffer.s32[j * n_channels + i] = (float)((double)internal_buffer[i][j] * (1.0 / (double)(0x800000)));
				}
			}
		}
		write_size = l * n_channels * (depth >> 3);
		if (outf) fwrite(buffer.s8, 1, write_size, outf);
		data_written += write_size;
		if (l < bufsize) break;
		done += l;
		l = done * 64 / length;
		while (dots < 64 && l > dots) {
			fprintf(stderr, "|");
			dots++;
		}
			if (dots >= 64) {
			putchar('\n');
			dots = 0;
			done = 0;
		}
	}

	while (64 > dots) {
		fprintf(stderr, "|");
		dots++;
	}
	fprintf(stderr, "\n");

	end = clock();

	if (depth == 32) destroy_sample_buffer(internal_buffer);

	/* fill in blanks we left in WAVE file */
	if (outf) {
		/* file size, not including RIFF header */
		const int fmt_size = 8 + ((depth == 32) ? 18 : 16);
		const int data_size = 8 + data_written;
		const int file_size = fmt_size + data_size;

		/* can we seek stdout? */
		fseek(outf, 4, SEEK_SET);
		write32_le(outf, file_size);

		fseek(outf, 12 + fmt_size + 4, SEEK_SET);
		write32_le(outf, data_written);
	}


	duh_end_sigrenderer(sr);
	unload_duh(duh);
	if (outf && outf != stdout) fclose(outf);

	fprintf(stderr, "Elapsed time: %f seconds\n", (end - start) / (float)CLOCKS_PER_SEC);

	return 0;
}
