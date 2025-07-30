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
#include "hio.h"
#include "callbackio.h"
#include "mdataio.h"

static long get_size(FILE *f)
{
	long size, pos;

	pos = ftell(f);
	if (pos >= 0) {
		if (fseek(f, 0, SEEK_END) < 0) {
			return -1;
		}
		size = ftell(f);
		if (fseek(f, pos, SEEK_SET) < 0) {
			return -1;
		}
		return size;
	} else {
		return pos;
	}
}

int8 hio_read8s(HIO_HANDLE *h)
{
	int err;
	int8 ret;

	switch (HIO_HANDLE_TYPE(h)) {
	case HIO_HANDLE_TYPE_FILE:
		ret = read8s(h->handle.file, &err);
		break;
	case HIO_HANDLE_TYPE_MEMORY:
		ret = mread8s(h->handle.mem, &err);
		break;
	case HIO_HANDLE_TYPE_CBFILE:
		ret = cbread8s(h->handle.cbfile, &err);
		break;
	default:
		return 0;
	}

	if (err != 0) {
		h->error = err;
	}
	return ret;
}

uint8 hio_read8(HIO_HANDLE *h)
{
	int err;
	uint8 ret;

	switch (HIO_HANDLE_TYPE(h)) {
	case HIO_HANDLE_TYPE_FILE:
		ret = read8(h->handle.file, &err);
		break;
	case HIO_HANDLE_TYPE_MEMORY:
		ret = mread8(h->handle.mem, &err);
		break;
	case HIO_HANDLE_TYPE_CBFILE:
		ret = cbread8(h->handle.cbfile, &err);
		break;
	default:
		return 0;
	}

	if (err != 0) {
		h->error = err;
	}
	return ret;
}

uint16 hio_read16l(HIO_HANDLE *h)
{
	int err;
	uint16 ret;

	switch (HIO_HANDLE_TYPE(h)) {
	case HIO_HANDLE_TYPE_FILE:
		ret = read16l(h->handle.file, &err);
		break;
	case HIO_HANDLE_TYPE_MEMORY:
		ret = mread16l(h->handle.mem, &err);
		break;
	case HIO_HANDLE_TYPE_CBFILE:
		ret = cbread16l(h->handle.cbfile, &err);
		break;
	default:
		return 0;
	}

	if (err != 0) {
		h->error = err;
	}
	return ret;
}

uint16 hio_read16b(HIO_HANDLE *h)
{
	int err;
	uint16 ret;

	switch (HIO_HANDLE_TYPE(h)) {
	case HIO_HANDLE_TYPE_FILE:
		ret = read16b(h->handle.file, &err);
		break;
	case HIO_HANDLE_TYPE_MEMORY:
		ret = mread16b(h->handle.mem, &err);
		break;
	case HIO_HANDLE_TYPE_CBFILE:
		ret = cbread16b(h->handle.cbfile, &err);
		break;
	default:
		return 0;
	}

	if (err != 0) {
		h->error = err;
	}
	return ret;
}

uint32 hio_read24l(HIO_HANDLE *h)
{
	int err;
	uint32 ret;

	switch (HIO_HANDLE_TYPE(h)) {
	case HIO_HANDLE_TYPE_FILE:
		ret = read24l(h->handle.file, &err);
		break;
	case HIO_HANDLE_TYPE_MEMORY:
		ret = mread24l(h->handle.mem, &err);
		break;
	case HIO_HANDLE_TYPE_CBFILE:
		ret = cbread24l(h->handle.cbfile, &err);
		break;
	default:
		return 0;
	}

	if (err != 0) {
		h->error = err;
	}
	return ret;
}

uint32 hio_read24b(HIO_HANDLE *h)
{
	int err;
	uint32 ret;

	switch (HIO_HANDLE_TYPE(h)) {
	case HIO_HANDLE_TYPE_FILE:
		ret = read24b(h->handle.file, &err);
		break;
	case HIO_HANDLE_TYPE_MEMORY:
		ret = mread24b(h->handle.mem, &err);
		break;
	case HIO_HANDLE_TYPE_CBFILE:
		ret = cbread24b(h->handle.cbfile, &err);
		break;
	default:
		return 0;
	}

	if (err != 0) {
		h->error = err;
	}
	return ret;
}

uint32 hio_read32l(HIO_HANDLE *h)
{
	int err;
	uint32 ret;

	switch (HIO_HANDLE_TYPE(h)) {
	case HIO_HANDLE_TYPE_FILE:
		ret = read32l(h->handle.file, &err);
		break;
	case HIO_HANDLE_TYPE_MEMORY:
		ret = mread32l(h->handle.mem, &err);
		break;
	case HIO_HANDLE_TYPE_CBFILE:
		ret = cbread32l(h->handle.cbfile, &err);
		break;
	default:
		return 0;
	}

	if (err != 0) {
		h->error = err;
	}
	return ret;
}

uint32 hio_read32b(HIO_HANDLE *h)
{
	int err;
	uint32 ret;

	switch (HIO_HANDLE_TYPE(h)) {
	case HIO_HANDLE_TYPE_FILE:
		ret = read32b(h->handle.file, &err);
		break;
	case HIO_HANDLE_TYPE_MEMORY:
		ret = mread32b(h->handle.mem, &err);
		break;
	case HIO_HANDLE_TYPE_CBFILE:
		ret = cbread32b(h->handle.cbfile, &err);
		break;
	default:
		return 0;
	}

	if (err != 0) {
		h->error = err;
	}
	return ret;
}

size_t hio_read(void *buf, size_t size, size_t num, HIO_HANDLE *h)
{
	size_t ret = 0;

	switch (HIO_HANDLE_TYPE(h)) {
	case HIO_HANDLE_TYPE_FILE:
		ret = fread(buf, size, num, h->handle.file);
		if (ret != num) {
			if (ferror(h->handle.file)) {
				h->error = errno;
			} else {
				h->error = feof(h->handle.file) ? EOF : -2;
			}
		}
		break;
	case HIO_HANDLE_TYPE_MEMORY:
		ret = mread(buf, size, num, h->handle.mem);
		if (ret != num) {
			h->error = EOF;
		}
		break;
	case HIO_HANDLE_TYPE_CBFILE:
		ret = cbread(buf, size, num, h->handle.cbfile);
		if (ret != num) {
			h->error = EOF;
		}
		break;
	}

	return ret;
}

int hio_seek(HIO_HANDLE *h, long offset, int whence)
{
	int ret = -1;

	switch (HIO_HANDLE_TYPE(h)) {
	case HIO_HANDLE_TYPE_FILE:
		ret = fseek(h->handle.file, offset, whence);
		if (ret < 0) {
			h->error = errno;
		}
		else if (h->error == EOF) {
			h->error = 0;
		}
		break;
	case HIO_HANDLE_TYPE_MEMORY:
		ret = mseek(h->handle.mem, offset, whence);
		if (ret < 0) {
			h->error = EINVAL;
		}
		else if (h->error == EOF) {
			h->error = 0;
		}
		break;
	case HIO_HANDLE_TYPE_CBFILE:
		ret = cbseek(h->handle.cbfile, offset, whence);
		if (ret < 0) {
			h->error = EINVAL;
		}
		else if (h->error == EOF) {
			h->error = 0;
		}
		break;
	}

	return ret;
}

long hio_tell(HIO_HANDLE *h)
{
	long ret = -1;

	switch (HIO_HANDLE_TYPE(h)) {
	case HIO_HANDLE_TYPE_FILE:
		ret = ftell(h->handle.file);
		if (ret < 0) {
			h->error = errno;
		}
		break;
	case HIO_HANDLE_TYPE_MEMORY:
		ret = mtell(h->handle.mem);
		if (ret < 0) {
		/* should _not_ happen! */
			h->error = EINVAL;
		}
		break;
	case HIO_HANDLE_TYPE_CBFILE:
		ret = cbtell(h->handle.cbfile);
		if (ret < 0) {
			h->error = EINVAL;
		}
		break;
	}

	return ret;
}

int hio_eof(HIO_HANDLE *h)
{
	switch (HIO_HANDLE_TYPE(h)) {
	case HIO_HANDLE_TYPE_FILE:
		return feof(h->handle.file);
	case HIO_HANDLE_TYPE_MEMORY:
		return meof(h->handle.mem);
	case HIO_HANDLE_TYPE_CBFILE:
		return cbeof(h->handle.cbfile);
	}
	return EOF;
}

int hio_error(HIO_HANDLE *h)
{
	int error = h->error;
	h->error = 0;
	return error;
}

HIO_HANDLE *hio_open(const char *path, const char *mode)
{
	HIO_HANDLE *h;

	h = (HIO_HANDLE *) calloc(1, sizeof(HIO_HANDLE));
	if (h == NULL)
		goto err;

	h->type = HIO_HANDLE_TYPE_FILE;
	h->handle.file = fopen(path, mode);
	if (h->handle.file == NULL)
		goto err2;

	h->size = get_size(h->handle.file);
	if (h->size < 0)
		goto err3;

	return h;

    err3:
	fclose(h->handle.file);
    err2:
	free(h);
    err:
	return NULL;
}

HIO_HANDLE *hio_open_mem(const void *ptr, long size, int free_after_use)
{
	HIO_HANDLE *h;

	if (size <= 0) return NULL;
	h = (HIO_HANDLE *) calloc(1, sizeof(HIO_HANDLE));
	if (h == NULL)
		return NULL;

	h->type = HIO_HANDLE_TYPE_MEMORY;
	h->handle.mem = mopen(ptr, size, free_after_use);
	h->size = size;

	if (!h->handle.mem) {
		free(h);
		h = NULL;
	}

	return h;
}

HIO_HANDLE *hio_open_file(FILE *f)
{
	HIO_HANDLE *h;

	h = (HIO_HANDLE *) calloc(1, sizeof(HIO_HANDLE));
	if (h == NULL)
		return NULL;

	h->noclose = 1;
	h->type = HIO_HANDLE_TYPE_FILE;
	h->handle.file = f;
	h->size = get_size(f);
	if (h->size < 0) {
		free(h);
		return NULL;
	}

	return h;
}

HIO_HANDLE *hio_open_file2(FILE *f)
{
	HIO_HANDLE *h = hio_open_file(f);
	if (h != NULL) {
		h->noclose = 0;
	}
	else {
		fclose(f);
	}
	return h;
}

HIO_HANDLE *hio_open_callbacks(void *priv, struct xmp_callbacks callbacks)
{
	HIO_HANDLE *h;
	CBFILE *f = cbopen(priv, callbacks);
	if (!f)
		return NULL;

	h = (HIO_HANDLE *) calloc(1, sizeof(HIO_HANDLE));
	if (h == NULL) {
		cbclose(f);
		return NULL;
	}

	h->type = HIO_HANDLE_TYPE_CBFILE;
	h->handle.cbfile = f;
	h->size = cbfilelength(f);
	if (h->size < 0) {
		cbclose(f);
		free(h);
		return NULL;
	}
	return h;
}

static int hio_close_internal(HIO_HANDLE *h)
{
	int ret = -1;

	switch (HIO_HANDLE_TYPE(h)) {
	case HIO_HANDLE_TYPE_FILE:
		ret = (h->noclose)? 0 : fclose(h->handle.file);
		break;
	case HIO_HANDLE_TYPE_MEMORY:
		ret = mclose(h->handle.mem);
		break;
	case HIO_HANDLE_TYPE_CBFILE:
		ret = cbclose(h->handle.cbfile);
		break;
	}
	return ret;
}

/* hio_close + hio_open_mem. Reuses the same HIO_HANDLE. */
int hio_reopen_mem(const void *ptr, long size, int free_after_use, HIO_HANDLE *h)
{
	MFILE *m;
	int ret;
	if (size <= 0) return -1;

	m = mopen(ptr, size, free_after_use);
	if (m == NULL) {
		return -1;
	}

	ret = hio_close_internal(h);
	if (ret < 0) {
		m->free_after_use = 0;
		mclose(m);
		return ret;
	}

	h->type = HIO_HANDLE_TYPE_MEMORY;
	h->handle.mem = m;
	h->size = size;
	return 0;
}

/* hio_close + hio_open_file. Reuses the same HIO_HANDLE. */
int hio_reopen_file(FILE *f, int close_after_use, HIO_HANDLE *h)
{
	long size = get_size(f);
	int ret;
	if (size < 0) {
		return -1;
	}

	ret = hio_close_internal(h);
	if (ret < 0) {
		return -1;
	}

	h->noclose = !close_after_use;
	h->type = HIO_HANDLE_TYPE_FILE;
	h->handle.file = f;
	h->size = size;
	return 0;
}

int hio_close(HIO_HANDLE *h)
{
	int ret = hio_close_internal(h);
	free(h);
	return ret;
}

long hio_size(HIO_HANDLE *h)
{
	return h->size;
}
