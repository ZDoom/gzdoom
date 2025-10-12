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

#include "loader.h"
#include "../miniz.h"

static int muse_test(HIO_HANDLE *, char *, const int);
static int muse_load(struct module_data *, HIO_HANDLE *, const int);

const struct format_loader libxmp_loader_muse = {
	"MUSE container",
	muse_test,
	muse_load
};

static int muse_test(HIO_HANDLE * f, char *t, const int start)
{
	uint8 in[8];
	uint32 r;

	if (hio_read(in, 1, 8, f) != 8) {
		return -1;
	}
	if (memcmp(in, "MUSE", 4) != 0) {
		return -1;
	}
	r = readmem32b(in + 4);
	if (r != 0xdeadbeaf && r != 0xdeadbabe) {
		return -1;
	}
	if (t) {
		*t = '\0';		/* FIXME */
	}
	return 0;
}

static int muse_load(struct module_data *m, HIO_HANDLE *f, const int start)
{
	void *in, *out;
	long inlen;
	size_t outlen;
	int err;

	inlen = hio_size(f);
	if (inlen < 24 || inlen >= LIBXMP_DEPACK_LIMIT) {
		D_(D_CRIT "bad file size");
		return -1;
	}
	if (hio_seek(f, 24, SEEK_SET) < 0) {
		D_(D_CRIT "hio_seek() failed");
		return -1;
	}

	inlen -= 24;
	in = (uint8 *)malloc(inlen);
	if (!in) {
		D_(D_CRIT "Out of memory");
		return -1;
	}
	if (hio_read(in, 1, inlen, f) != inlen) {
		D_(D_CRIT "Failed reading input file");
		free(in);
		return -1;
	}

	out = tinfl_decompress_mem_to_heap(in, inlen, &outlen, TINFL_FLAG_PARSE_ZLIB_HEADER);
	if (!out) {
		free(in);
		D_(D_CRIT "tinfl_decompress_mem_to_heap() failed");
		return -1;
	}
	free(in);

	if (hio_reopen_mem(out, outlen, 1, f) < 0) {
		free(out);
		return -1;
	}
	err = libxmp_loader_gal5.test(f, NULL, 0);
	hio_seek(f, 0, SEEK_SET);
	if (err == 0) {
		err = libxmp_loader_gal5.loader(m, f, 0);
	} else {
		err = libxmp_loader_gal4.test(f, NULL, 0);
		hio_seek(f, 0, SEEK_SET);
		if (err == 0) {
			err = libxmp_loader_gal4.loader(m, f, 0);
		}
	}

	return err;
}
