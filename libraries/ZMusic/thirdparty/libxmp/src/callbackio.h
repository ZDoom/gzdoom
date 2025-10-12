#ifndef LIBXMP_CALLBACKIO_H
#define LIBXMP_CALLBACKIO_H

#include <stddef.h>
#include "common.h"

typedef struct {
	void *priv;
	struct xmp_callbacks callbacks;
	int eof;
} CBFILE;

LIBXMP_BEGIN_DECLS

static inline uint8 cbread8(CBFILE *f, int *err)
{
	uint8 x = 0xff;
	size_t r = f->callbacks.read_func(&x, 1, 1, f->priv);
	f->eof = (r == 1) ? 0 : EOF;

	if (err) *err = f->eof;

	return x;
}

static inline int8 cbread8s(CBFILE *f, int *err)
{
	return (int8)cbread8(f, err);
}

static inline uint16 cbread16l(CBFILE *f, int *err)
{
	uint8 buf[2];
	uint16 x = 0xffff;
	size_t r = f->callbacks.read_func(buf, 2, 1, f->priv);
	f->eof = (r == 1) ? 0 : EOF;

	if (r)   x = readmem16l(buf);
	if (err) *err = f->eof;

	return x;
}

static inline uint16 cbread16b(CBFILE *f, int *err)
{
	uint8 buf[2];
	uint16 x = 0xffff;
	size_t r = f->callbacks.read_func(buf, 2, 1, f->priv);
	f->eof = (r == 1) ? 0 : EOF;

	if (r)   x = readmem16b(buf);
	if (err) *err = f->eof;

	return x;
}

static inline uint32 cbread24l(CBFILE *f, int *err)
{
	uint8 buf[3];
	uint32 x = 0xffffffff;
	size_t r = f->callbacks.read_func(buf, 3, 1, f->priv);
	f->eof = (r == 1) ? 0 : EOF;

	if (r)   x = readmem24l(buf);
	if (err) *err = f->eof;

	return x;
}

static inline uint32 cbread24b(CBFILE *f, int *err)
{
	uint8 buf[3];
	uint32 x = 0xffffffff;
	size_t r = f->callbacks.read_func(buf, 3, 1, f->priv);
	f->eof = (r == 1) ? 0 : EOF;

	if (r)   x = readmem24b(buf);
	if (err) *err = f->eof;

	return x;
}

static inline uint32 cbread32l(CBFILE *f, int *err)
{
	uint8 buf[4];
	uint32 x = 0xffffffff;
	size_t r = f->callbacks.read_func(buf, 4, 1, f->priv);
	f->eof = (r == 1) ? 0 : EOF;

	if (r)   x = readmem32l(buf);
	if (err) *err = f->eof;

	return x;
}

static inline uint32 cbread32b(CBFILE *f, int *err)
{
	uint8 buf[4];
	uint32 x = 0xffffffff;
	size_t r = f->callbacks.read_func(buf, 4, 1, f->priv);
	f->eof = (r == 1) ? 0 : EOF;

	if (r)   x = readmem32b(buf);
	if (err) *err = f->eof;

	return x;
}

static inline size_t cbread(void *dest, size_t len, size_t nmemb, CBFILE *f)
{
	size_t r = f->callbacks.read_func(dest, (unsigned long)len,
					(unsigned long)nmemb, f->priv);
	f->eof = (r < nmemb) ? EOF : 0;
	return r;
}

static inline int cbseek(CBFILE *f, long offset, int whence)
{
	f->eof = 0;
	return f->callbacks.seek_func(f->priv, offset, whence);
}

static inline long cbtell(CBFILE *f)
{
	return f->callbacks.tell_func(f->priv);
}

static inline int cbeof(CBFILE *f)
{
	return f->eof;
}

static inline long cbfilelength(CBFILE *f)
{
	long pos = f->callbacks.tell_func(f->priv);
	long length;
	int r;

	if (pos < 0)
		return EOF;

	r = f->callbacks.seek_func(f->priv, 0, SEEK_END);
	if (r < 0)
		return EOF;

	length = f->callbacks.tell_func(f->priv);
	r = f->callbacks.seek_func(f->priv, pos, SEEK_SET);

	return length;
}

static inline CBFILE *cbopen(void *priv, struct xmp_callbacks callbacks)
{
	CBFILE *f;
	if (priv == NULL || callbacks.read_func == NULL ||
	    callbacks.seek_func == NULL || callbacks.tell_func == NULL)
		goto err;

	f = (CBFILE *)calloc(1, sizeof(CBFILE));
	if (f == NULL)
		goto err;

	f->priv = priv;
	f->callbacks = callbacks;
	f->eof = 0;
	return f;

    err:
	if (priv && callbacks.close_func)
		callbacks.close_func(priv);

	return NULL;
}

static inline int cbclose(CBFILE *f)
{
	int r = 0;
	if (f->callbacks.close_func != NULL)
		r = f->callbacks.close_func(f->priv);

	free(f);
	return r;
}

LIBXMP_END_DECLS

#endif /* LIBXMP_CALLBACKIO_H */
