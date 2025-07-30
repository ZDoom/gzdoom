/* Extended Module Player
 * Copyright (C) 1996-2022 Claudio Matsuoka and Hipolito Carraro Jr
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

#include <errno.h>
#include "common.h"


#define read_byte(x) do {		\
	(x) = fgetc(f);			\
	if ((x) < 0)  goto error;	\
} while (0)

#define set_error(x) do {		\
	if (err != NULL) *err = (x);	\
} while (0)

uint8 read8(FILE *f, int *err)
{
	int a;

	read_byte(a);
	set_error(0);
	return a;

   error:
	set_error(ferror(f) ? errno : EOF);
	return 0xff;
}

int8 read8s(FILE *f, int *err)
{
	int a;

	read_byte(a);
	set_error(0);
	return (int8)a;

   error:
	set_error(ferror(f) ? errno : EOF);
	return 0;
}

uint16 read16l(FILE *f, int *err)
{
	int a, b;

	read_byte(a);
	read_byte(b);

	set_error(0);
	return ((uint16)b << 8) | a;

    error:
	set_error(ferror(f) ? errno : EOF);
	return 0xffff;
}

uint16 read16b(FILE *f, int *err)
{
	int a, b;

	read_byte(a);
	read_byte(b);

	set_error(0);
	return (a << 8) | b;

    error:
	set_error(ferror(f) ? errno : EOF);
	return 0xffff;
}

uint32 read24l(FILE *f, int *err)
{
	int a, b, c;

	read_byte(a);
	read_byte(b);
	read_byte(c);

	set_error(0);
	return (c << 16) | (b << 8) | a;

    error:
	set_error(ferror(f) ? errno : EOF);
	return 0xffffffff;
}

uint32 read24b(FILE *f, int *err)
{
	int a, b, c;

	read_byte(a);
	read_byte(b);
	read_byte(c);

	set_error(0);
	return (a << 16) | (b << 8) | c;

    error:
	set_error(ferror(f) ? errno : EOF);
	return 0xffffffff;
}

uint32 read32l(FILE *f, int *err)
{
	int a, b, c, d;

	read_byte(a);
	read_byte(b);
	read_byte(c);
	read_byte(d);

	set_error(0);
	return (d << 24) | (c << 16) | (b << 8) | a;

    error:
	set_error(ferror(f) ? errno : EOF);
	return 0xffffffff;
}

uint32 read32b(FILE *f, int *err)
{
	int a, b, c, d;

	read_byte(a);
	read_byte(b);
	read_byte(c);
	read_byte(d);

	set_error(0);
	return (a << 24) | (b << 16) | (c << 8) | d;

    error:
	set_error(ferror(f) ? errno : EOF);
	return 0xffffffff;
}

uint16 readmem16l(const uint8 *m)
{
	uint32 a, b;

	a = m[0];
	b = m[1];

	return (b << 8) | a;
}

uint16 readmem16b(const uint8 *m)
{
	uint32 a, b;

	a = m[0];
	b = m[1];

	return (a << 8) | b;
}

uint32 readmem24l(const uint8 *m)
{
	uint32 a, b, c;

	a = m[0];
	b = m[1];
	c = m[2];

	return (c << 16) | (b << 8) | a;
}

uint32 readmem24b(const uint8 *m)
{
	uint32 a, b, c;

	a = m[0];
	b = m[1];
	c = m[2];

	return (a << 16) | (b << 8) | c;
}

uint32 readmem32l(const uint8 *m)
{
	uint32 a, b, c, d;

	a = m[0];
	b = m[1];
	c = m[2];
	d = m[3];

	return (d << 24) | (c << 16) | (b << 8) | a;
}

uint32 readmem32b(const uint8 *m)
{
	uint32 a, b, c, d;

	a = m[0];
	b = m[1];
	c = m[2];
	d = m[3];

	return (a << 24) | (b << 16) | (c << 8) | d;
}

#ifndef LIBXMP_CORE_PLAYER

void write16l(FILE *f, uint16 w)
{
	write8(f, w & 0x00ff);
	write8(f, (w & 0xff00) >> 8);
}

void write16b(FILE *f, uint16 w)
{
	write8(f, (w & 0xff00) >> 8);
	write8(f, w & 0x00ff);
}

void write32l(FILE *f, uint32 w)
{
	write8(f, w & 0x000000ff);
	write8(f, (w & 0x0000ff00) >> 8);
	write8(f, (w & 0x00ff0000) >> 16);
	write8(f, (w & 0xff000000) >> 24);
}

void write32b(FILE *f, uint32 w)
{
	write8(f, (w & 0xff000000) >> 24);
	write8(f, (w & 0x00ff0000) >> 16);
	write8(f, (w & 0x0000ff00) >> 8);
	write8(f, w & 0x000000ff);
}

#endif
