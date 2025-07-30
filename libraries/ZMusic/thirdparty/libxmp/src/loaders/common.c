/* Extended Module Player
 * Copyright (C) 1996-2023 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <ctype.h>
#if defined(HAVE_DIRENT)
#include <sys/types.h>
#include <dirent.h>
#endif

#include "../common.h"

#include "xmp.h"
#include "../period.h"
#include "loader.h"

int libxmp_init_instrument(struct module_data *m)
{
	struct xmp_module *mod = &m->mod;

	if (mod->ins > 0) {
		mod->xxi = (struct xmp_instrument *) calloc(mod->ins, sizeof(struct xmp_instrument));
		if (mod->xxi == NULL)
			return -1;
	}

	if (mod->smp > 0) {
		int i;
		/* Sanity check */
		if (mod->smp > MAX_SAMPLES) {
			D_(D_CRIT "sample count %d exceeds maximum (%d)",
			   mod->smp, MAX_SAMPLES);
			return -1;
		}

		mod->xxs = (struct xmp_sample *) calloc(mod->smp, sizeof(struct xmp_sample));
		if (mod->xxs == NULL)
			return -1;
		m->xtra = (struct extra_sample_data *) calloc(mod->smp, sizeof(struct extra_sample_data));
		if (m->xtra == NULL)
			return -1;

		for (i = 0; i < mod->smp; i++) {
			m->xtra[i].c5spd = m->c4rate;
		}
	}

	return 0;
}

/* Sample number adjustment (originally by Vitamin/CAIG).
 * Only use this AFTER a previous usage of libxmp_init_instrument,
 * and don't use this to free samples that have already been loaded. */
int libxmp_realloc_samples(struct module_data *m, int new_size)
{
	struct xmp_module *mod = &m->mod;
	struct xmp_sample *xxs;
	struct extra_sample_data *xtra;

	/* Sanity check */
	if (new_size < 0)
		return -1;

	if (new_size == 0) {
		/* Don't rely on implementation-defined realloc(x,0) behavior. */
		mod->smp = 0;
		free(mod->xxs);
		mod->xxs = NULL;
		free(m->xtra);
		m->xtra = NULL;
		return 0;
	}

	xxs = (struct xmp_sample *) realloc(mod->xxs, sizeof(struct xmp_sample) * new_size);
	if (xxs == NULL)
		return -1;
	mod->xxs = xxs;

	xtra = (struct extra_sample_data *) realloc(m->xtra, sizeof(struct extra_sample_data) * new_size);
	if (xtra == NULL)
		return -1;
	m->xtra = xtra;

	if (new_size > mod->smp) {
		int clear_size = new_size - mod->smp;
		int i;

		memset(xxs + mod->smp, 0, sizeof(struct xmp_sample) * clear_size);
		memset(xtra + mod->smp, 0, sizeof(struct extra_sample_data) * clear_size);

		for (i = mod->smp; i < new_size; i++) {
			m->xtra[i].c5spd = m->c4rate;
		}
	}
	mod->smp = new_size;
	return 0;
}

int libxmp_alloc_subinstrument(struct xmp_module *mod, int i, int num)
{
	if (num == 0)
		return 0;

	mod->xxi[i].sub = (struct xmp_subinstrument *) calloc(num, sizeof(struct xmp_subinstrument));
	if (mod->xxi[i].sub == NULL)
		return -1;

	return 0;
}

int libxmp_init_pattern(struct xmp_module *mod)
{
	mod->xxt = (struct xmp_track **) calloc(mod->trk, sizeof(struct xmp_track *));
	if (mod->xxt == NULL)
		return -1;

	mod->xxp = (struct xmp_pattern **) calloc(mod->pat, sizeof(struct xmp_pattern *));
	if (mod->xxp == NULL)
		return -1;

	return 0;
}

int libxmp_alloc_pattern(struct xmp_module *mod, int num)
{
	/* Sanity check */
	if (num < 0 || num >= mod->pat || mod->xxp[num] != NULL)
		return -1;

	mod->xxp[num] = (struct xmp_pattern *) calloc(1, sizeof(struct xmp_pattern) +
							 sizeof(int) * (mod->chn - 1));
	if (mod->xxp[num] == NULL)
		return -1;

	return 0;
}

int libxmp_alloc_track(struct xmp_module *mod, int num, int rows)
{
	/* Sanity check */
	if (num < 0 || num >= mod->trk || mod->xxt[num] != NULL || rows <= 0)
		return -1;

	mod->xxt[num] = (struct xmp_track *) calloc(1,  sizeof(struct xmp_track) +
							sizeof(struct xmp_event) * (rows - 1));
	if (mod->xxt[num] == NULL)
		return -1;

	mod->xxt[num]->rows = rows;

	return 0;
}

int libxmp_alloc_tracks_in_pattern(struct xmp_module *mod, int num)
{
	int i;

	D_(D_INFO "Alloc %d tracks of %d rows", mod->chn, mod->xxp[num]->rows);
	for (i = 0; i < mod->chn; i++) {
		int t = num * mod->chn + i;
		int rows = mod->xxp[num]->rows;

		if (libxmp_alloc_track(mod, t, rows) < 0)
			return -1;

		mod->xxp[num]->index[i] = t;
	}

	return 0;
}

int libxmp_alloc_pattern_tracks(struct xmp_module *mod, int num, int rows)
{
	/* Sanity check */
	if (rows <= 0 || rows > 256)
		return -1;

	if (libxmp_alloc_pattern(mod, num) < 0)
		return -1;

	mod->xxp[num]->rows = rows;

	if (libxmp_alloc_tracks_in_pattern(mod, num) < 0)
		return -1;

	return 0;
}

#ifndef LIBXMP_CORE_PLAYER
/* Some formats explicitly allow more than 256 rows (e.g. OctaMED). This function
 * allows those formats to work without disrupting the sanity check for other formats.
 */
int libxmp_alloc_pattern_tracks_long(struct xmp_module *mod, int num, int rows)
{
	/* Sanity check */
	if (rows <= 0 || rows > 32768)
		return -1;

	if (libxmp_alloc_pattern(mod, num) < 0)
		return -1;

	mod->xxp[num]->rows = rows;

	if (libxmp_alloc_tracks_in_pattern(mod, num) < 0)
		return -1;

	return 0;
}
#endif

char *libxmp_instrument_name(struct xmp_module *mod, int i, uint8 *r, int n)
{
	CLAMP(n, 0, 31);

	return libxmp_copy_adjust(mod->xxi[i].name, r, n);
}

char *libxmp_copy_adjust(char *s, uint8 *r, int n)
{
	int i;

	memset(s, 0, n + 1);
	strncpy(s, (char *)r, n);

	for (i = 0; s[i] && i < n; i++) {
		if (!isprint((unsigned char)s[i]) || ((uint8)s[i] > 127))
			s[i] = '.';
	}

	while (*s && (s[strlen(s) - 1] == ' '))
		s[strlen(s) - 1] = 0;

	return s;
}

void libxmp_read_title(HIO_HANDLE *f, char *t, int s)
{
	uint8 buf[XMP_NAME_SIZE];

	if (t == NULL || s < 0)
		return;

	if (s >= XMP_NAME_SIZE)
		s = XMP_NAME_SIZE -1;

	memset(t, 0, s + 1);

	s = hio_read(buf, 1, s, f);
	buf[s] = 0;
	libxmp_copy_adjust(t, buf, s);
}

#ifndef LIBXMP_CORE_PLAYER

int libxmp_test_name(const uint8 *s, int n, int flags)
{
	int i;

	for (i = 0; i < n; i++) {
		if (s[i] == '\0' && (flags & TEST_NAME_IGNORE_AFTER_0))
			break;
		if (s[i] == '\r' && (flags & TEST_NAME_IGNORE_AFTER_CR))
			break;
		if (s[i] > 0x7f)
			return -1;
		/* ACS_Team2.mod has a backspace in instrument name */
		/* Numerous ST modules from Music Channel BBS have char 14. */
		if (s[i] > 0 && s[i] < 32 && s[i] != 0x08 && s[i] != 0x0e)
			return -1;
	}

	return 0;
}

int libxmp_copy_name_for_fopen(char *dest, const char *name, int n)
{
	int converted_colon = 0;
	int i;

	/* libxmp_copy_adjust, but make sure the filename won't do anything
	 * malicious when given to fopen. This should only be used on song files.
	 */
	if (!strcmp(name, ".") || strstr(name, "..") ||
	    name[0] == '\\' || name[0] == '/' || name[0] == ':')
		return -1;

	for (i = 0; i < n - 1; i++) {
		uint8 t = name[i];
		if (!t)
			break;

		/* Reject non-ASCII symbols as they have poorly defined behavior.
		 */
		if (t < 32 || t >= 0x7f)
			return -1;

		/* Reject anything resembling a Windows-style root path. Allow
		 * converting a single : to / so things like ST-01:samplename
		 * work. (Leave the : as-is on Amiga.)
		 */
		if (i > 0 && t == ':' && !converted_colon) {
			uint8 t2 = name[i + 1];
			if (!t2 || t2 == '/' || t2 == '\\')
				return -1;

			converted_colon = 1;
#ifndef LIBXMP_AMIGA
			dest[i] = '/';
			continue;
#endif
		}

		if (t == '\\') {
			dest[i] = '/';
			continue;
		}

		dest[i] = t;
	}
	dest[i] = '\0';
	return 0;
}

/*
 * Honor Noisetracker effects:
 *
 *  0 - arpeggio
 *  1 - portamento up
 *  2 - portamento down
 *  3 - Tone-portamento
 *  4 - Vibrato
 *  A - Slide volume
 *  B - Position jump
 *  C - Set volume
 *  D - Pattern break
 *  E - Set filter (keep the led off, please!)
 *  F - Set speed (now up to $1F)
 *
 * Pex Tufvesson's notes from http://www.livet.se/mahoney/:
 *
 * Note that some of the modules will have bugs in the playback with all
 * known PC module players. This is due to that in many demos where I synced
 * events in the demo with the music, I used commands that these newer PC
 * module players erroneously interpret as "newer-version-trackers commands".
 * Which they aren't.
 */
void libxmp_decode_noisetracker_event(struct xmp_event *event, const uint8 *mod_event)
{
	int fxt;

	memset(event, 0, sizeof (struct xmp_event));
	event->note = libxmp_period_to_note((LSN(mod_event[0]) << 8) + mod_event[1]);
	event->ins = ((MSN(mod_event[0]) << 4) | MSN(mod_event[2]));
	fxt = LSN(mod_event[2]);

	if (fxt <= 0x06 || (fxt >= 0x0a && fxt != 0x0e)) {
		event->fxt = fxt;
		event->fxp = mod_event[3];
	}

	libxmp_disable_continue_fx(event);
}
#endif

void libxmp_decode_protracker_event(struct xmp_event *event, const uint8 *mod_event)
{
	int fxt = LSN(mod_event[2]);

	memset(event, 0, sizeof (struct xmp_event));
	event->note = libxmp_period_to_note((LSN(mod_event[0]) << 8) + mod_event[1]);
	event->ins = ((MSN(mod_event[0]) << 4) | MSN(mod_event[2]));

	if (fxt != 0x08) {
		event->fxt = fxt;
		event->fxp = mod_event[3];
	}

	libxmp_disable_continue_fx(event);
}

void libxmp_disable_continue_fx(struct xmp_event *event)
{
	if (event->fxp == 0) {
		switch (event->fxt) {
		case 0x05:
			event->fxt = 0x03;
			break;
		case 0x06:
			event->fxt = 0x04;
			break;
		case 0x01:
		case 0x02:
		case 0x0a:
			event->fxt = 0x00;
		}
	} else if (event->fxt == 0x0e) {
		if (event->fxp == 0xa0 || event->fxp == 0xb0) {
			event->fxt = event->fxp = 0;
		}
	}
}

#ifndef LIBXMP_CORE_PLAYER
/* libxmp_check_filename_case(): */
/* Given a directory, see if file exists there, ignoring case */

#if defined(_WIN32) || defined(__DJGPP__)  || \
    defined(__OS2__) || defined(__EMX__)   || \
    defined(_DOS) || defined(LIBXMP_AMIGA) || \
    defined(__riscos__) || \
    /* case-insensitive file system: directly probe the file */\
    \
   !defined(HAVE_DIRENT) /* or, target does not have dirent. */
int libxmp_check_filename_case(const char *dir, const char *name, char *new_name, int size)
{
	char path[XMP_MAXPATH];
	snprintf(path, sizeof(path), "%s/%s", dir, name);
	if (! (libxmp_get_filetype(path) & XMP_FILETYPE_FILE))
		return 0;
	strncpy(new_name, name, size);
	return 1;
}
#else /* target has dirent */
int libxmp_check_filename_case(const char *dir, const char *name, char *new_name, int size)
{
	int found = 0;
	DIR *dirp;
	struct dirent *d;

	dirp = opendir(dir);
	if (dirp == NULL)
		return 0;

	while ((d = readdir(dirp)) != NULL) {
		if (!strcasecmp(d->d_name, name)) {
			found = 1;
			strncpy(new_name, d->d_name, size);
			break;
		}
	}

	closedir(dirp);

	return found;
}
#endif

void libxmp_get_instrument_path(struct module_data *m, char *path, int size)
{
	if (m->instrument_path) {
		strncpy(path, m->instrument_path, size);
	} else if (getenv("XMP_INSTRUMENT_PATH")) {
		strncpy(path, getenv("XMP_INSTRUMENT_PATH"), size);
	} else {
		strncpy(path, ".", size);
	}
}
#endif /* LIBXMP_CORE_PLAYER */

void libxmp_set_type(struct module_data *m, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);

	vsnprintf(m->mod.type, XMP_NAME_SIZE, fmt, ap);
	va_end(ap);
}

#ifndef LIBXMP_CORE_PLAYER
static int schism_tracker_date(int year, int month, int day)
{
	int mm = (month + 9) % 12;
	int yy = year - mm / 10;

	yy = yy * 365 + (yy / 4) - (yy / 100) + (yy / 400);
	mm = (mm * 306 + 5) / 10;

	return yy + mm + (day - 1);
}

/* Generate a Schism Tracker version string.
 * Schism Tracker versions are stored as follows:
 *
 * s_ver <= 0x50:		0.s_ver
 * s_ver > 0x50, < 0xfff:	days from epoch=(s_ver - 0x50)
 * s_ver = 0xfff:		days from epoch=l_ver
 */
void libxmp_schism_tracker_string(char *buf, size_t size, int s_ver, int l_ver)
{
	if (s_ver >= 0x50) {
		/* time_t epoch_sec = 1256947200; */
		int64 t = schism_tracker_date(2009, 10, 31);
		int year, month, day, dayofyear;

		if (s_ver == 0xfff) {
			t += l_ver;
		} else
			t += s_ver - 0x50;

		/* Date algorithm reimplemented from OpenMPT.
		 */
		year = (int)((t * 10000L + 14780) / 3652425);
		dayofyear = t - (365L * year + (year / 4) - (year / 100) + (year / 400));
		if (dayofyear < 0) {
			year--;
			dayofyear = t - (365L * year + (year / 4) - (year / 100) + (year / 400));
		}
		month = (100 * dayofyear + 52) / 3060;
		day = dayofyear - (month * 306 + 5) / 10 + 1;

		year += (month + 2) / 12;
		month = (month + 2) % 12 + 1;

		snprintf(buf, size, "Schism Tracker %04d-%02d-%02d",
			year, month, day);
	} else {
		snprintf(buf, size, "Schism Tracker 0.%x", s_ver);
	}
}

/* Old MPT modules (from MPT <=1.16, older versions of OpenMPT) rely on a
 * pre-amp routine that scales mix volume down. This is based on the module's
 * channel count and a tracker pre-amp setting that isn't saved in the module.
 * This setting defaults to 128. When fixed to 128, it can be optimized out.
 *
 * In OpenMPT, this pre-amp routine is only available in the MPT and OpenMPT
 * 1.17 RC1 and RC2 mix modes. Changing a module to the compatible or 1.17 RC3
 * mix modes will permanently disable it for that module. OpenMPT applies the
 * old mix modes to MPT <=1.16 modules, "IT 8.88", and in old OpenMPT-made
 * modules that specify one of these mix modes in their extended properties.
 *
 * Set mod->chn and m->mvol first!
 */
void libxmp_apply_mpt_preamp(struct module_data *m)
{
	/* OpenMPT uses a slightly different table. */
	static const uint8 preamp_table[16] =
	{
		0x60, 0x60, 0x60, 0x70,	/* 0-7 */
		0x80, 0x88, 0x90, 0x98,	/* 8-15 */
		0xA0, 0xA4, 0xA8, 0xB0,	/* 16-23 */
		0xB4, 0xB8, 0xBC, 0xC0,	/* 24-31 */
	};

	int chn = m->mod.chn;
	CLAMP(chn, 1, 31);

	m->mvol = (m->mvol * 96) / preamp_table[chn >> 1];

	/* Pre-amp is applied like this in the mixers of libmodplug/libopenmpt
	 * (still vastly simplified).

	int preamp = 128;

	if (preamp > 128) {
		preamp = 128 + ((preamp - 128) * (chn + 4)) / 16;
	}
	preamp = preamp * m->mvol / 64;
	preamp = (preamp << 7) / preamp_table[chn >> 1];

	...

	channel_volume_16bit = (channel_volume_16bit * preamp) >> 7;
	*/
}
#endif

char *libxmp_strdup(const char *src)
{
	size_t len = strlen(src) + 1;
	char *buf = (char *) malloc(len);
	if (buf) {
		memcpy(buf, src, len);
	}
	return buf;
}
