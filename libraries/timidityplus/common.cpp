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

   common.c

   */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include <string.h>
#include <ctype.h>
#include "common.h"
#include "controls.h"

namespace TimidityPlus
{

/* This'll allocate memory or die. */
void *safe_malloc(size_t count)
{
	auto p = malloc(count);
	if (p == nullptr)
	{
		abort();
	}
	return p;
}

void *safe_large_malloc(size_t count)
{
	return safe_malloc(count);
}

void *safe_realloc(void *ptr, size_t count)
{
	auto p = realloc(ptr, count);
	if (p == nullptr)
	{
		abort();
	}
	return p;
}

char *safe_strdup(const char *s)
{
	if (s == nullptr) s = "";
	auto p = strdup(s);
	if (p == nullptr)
	{
		abort();
	}
	return p;
}

/* free ((void **)ptr_list)[0..count-1] and ptr_list itself */
void free_ptr_list(void *ptr_list, int count)
{
	int i;
	for (i = 0; i < count; i++)
		free(((void **)ptr_list)[i]);
	free(ptr_list);
}

static int atoi_limited(const char *string, int v_min, int v_max)
{
	int value = atoi(string);

	if (value <= v_min)
		value = v_min;
	else if (value > v_max)
		value = v_max;
	return value;
}

int string_to_7bit_range(const char *string_, int *start, int *end)
{
	const char *string = string_;

	if (isdigit(*string))
	{
		*start = atoi_limited(string, 0, 127);
		while (isdigit(*++string));
	}
	else
		*start = 0;
	if (*string == '-')
	{
		string++;
		*end = isdigit(*string) ? atoi_limited(string, 0, 127) : 127;
		if (*start > *end)
			*end = *start;
	}
	else
		*end = *start;
	return string != string_;
}


double GenRand_Real1()
{
	return rand() * (1. / RAND_MAX);
}

int int_rand(int n)
{
	return (int)GenRand_Real1() * n;
}

double flt_rand()
{
	return (int)GenRand_Real1();
}

timidity_file *open_file(const char *name, MusicIO::SoundFontReaderInterface *sfreader)
{
	return sfreader->open_file(name);
}

/* This closes files opened with open_file */
void tf_close(timidity_file *tf)
{
	if (tf) tf->close();
}

/* This is meant for skipping a few bytes. */
void skip(timidity_file *tf, size_t len)
{
	tf_seek(tf, (long)len, SEEK_CUR);
}

int tf_getc(timidity_file *tf)
{
	unsigned char c;
	auto read = tf_read(&c, 1, tf);
	return read == 0 ? EOF : c;
}

void default_ctl_cmsg(int type, int verbosity_level, const char* fmt, ...)
{
	if (verbosity_level >= VERB_DEBUG) return;	// Don't waste time on diagnostics.

	va_list args;
	va_start(args, fmt);

	switch (type)
	{
	case CMSG_ERROR:
		vprintf("Error: %s\n", args);
		break;

	case CMSG_WARNING:
		vprintf("Warning: %s\n", args);
		break;

	case CMSG_INFO:
		vprintf("Info: %s\n", args);
		break;
	}
}

// Allow hosting applications to capture the messages and deal with them themselves.
void (*printMessage)(int type, int verbosity_level, const char* fmt, ...) = default_ctl_cmsg;

}
