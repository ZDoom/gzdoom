/* Extended Module Player
 * Copyright (C) 1996-2021 Claudio Matsuoka and Hipolito Carraro Jr
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

#include "common.h"
#include "memio.h"

static inline ptrdiff_t CAN_READ(MFILE *m)
{
	return m->pos >= 0 ? m->size - m->pos : 0;
}


int mgetc(MFILE *m)
{
	if (CAN_READ(m) >= 1)
		return *(uint8 *)(m->start + m->pos++);
	return EOF;
}

size_t mread(void *buf, size_t size, size_t num, MFILE *m)
{
	size_t should_read = size * num;
	ptrdiff_t can_read = CAN_READ(m);

	if (!size || !num || can_read <= 0) {
		return 0;
	}

	if (should_read > can_read) {
		memcpy(buf, m->start + m->pos, can_read);
		m->pos += can_read;

		return can_read / size;
	} else {
		memcpy(buf, m->start + m->pos, should_read);
		m->pos += should_read;

		return num;
	}
}


int mseek(MFILE *m, long offset, int whence)
{
	ptrdiff_t ofs = offset;

	switch (whence) {
	case SEEK_SET:
		break;
	case SEEK_CUR:
		ofs += m->pos;
		break;
	case SEEK_END:
		ofs += m->size;
		break;
	default:
		return -1;
	}
	if (ofs < 0) return -1;
	if (ofs > m->size)
		ofs = m->size;
	m->pos = ofs;
	return 0;
}

long mtell(MFILE *m)
{
	return (long)m->pos;
}

int meof(MFILE *m)
{
	return CAN_READ(m) <= 0;
}

MFILE *mopen(const void *ptr, long size, int free_after_use)
{
	MFILE *m;

	m = (MFILE *) malloc(sizeof(MFILE));
	if (m == NULL)
		return NULL;

	m->start = (const unsigned char *)ptr;
	m->pos = 0;
	m->size = size;
	m->free_after_use = free_after_use;

	return m;
}

int mclose(MFILE *m)
{
	if (m->free_after_use)
		free((void *)m->start);
	free(m);
	return 0;
}

