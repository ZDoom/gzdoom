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
 * memfile.c - Module for reading data from           / / \  \
 *             memory using a DUMBFILE.              | <  /   \_
 *                                                   |  \/ /\   /
 * By entheh.                                         \_  /  > /
 *                                                      | \ / /
 *                                                      |  ' /
 *                                                       \__/
 */

#include <stdlib.h>
#include <string.h>

#include "dumb.h"



typedef struct MEMFILE MEMFILE;

struct MEMFILE
{
	const char *ptr, *ptr_begin;
	long left, size;
};



static int DUMBCALLBACK dumb_memfile_skip(void *f, long n)
{
	MEMFILE *m = f;
	if (n > m->left) return -1;
	m->ptr += n;
	m->left -= n;
	return 0;
}



static int DUMBCALLBACK dumb_memfile_getc(void *f)
{
	MEMFILE *m = f;
	if (m->left <= 0) return -1;
	m->left--;
	return *(const unsigned char *)m->ptr++;
}



static int32 DUMBCALLBACK dumb_memfile_getnc(char *ptr, int32 n, void *f)
{
	MEMFILE *m = f;
	if (n > m->left) n = m->left;
	memcpy(ptr, m->ptr, n);
	m->ptr += n;
	m->left -= n;
	return n;
}



static void DUMBCALLBACK dumb_memfile_close(void *f)
{
	free(f);
}


static int DUMBCALLBACK dumb_memfile_seek(void *f, long n)
{
	MEMFILE *m = f;

	m->ptr = m->ptr_begin + n;
	m->left = m->size - n;

	return 0;
}


static long DUMBCALLBACK dumb_memfile_get_size(void *f)
{
	MEMFILE *m = f;
	return m->size;
}


static const DUMBFILE_SYSTEM memfile_dfs = {
	NULL,
	&dumb_memfile_skip,
	&dumb_memfile_getc,
	&dumb_memfile_getnc,
	&dumb_memfile_close,
	&dumb_memfile_seek,
	&dumb_memfile_get_size
};



DUMBFILE *DUMBEXPORT dumbfile_open_memory(const char *data, int32 size)
{
	MEMFILE *m = malloc(sizeof(*m));
	if (!m) return NULL;

	m->ptr_begin = data;
	m->ptr = data;
	m->left = size;
	m->size = size;

	return dumbfile_open_ex(m, &memfile_dfs);
}
