#ifndef LIBXMP_MEMIO_H
#define LIBXMP_MEMIO_H

#include <stddef.h>
#include "common.h"

typedef struct {
	const unsigned char *start;
	ptrdiff_t pos;
	ptrdiff_t size;
	int free_after_use;
} MFILE;

LIBXMP_BEGIN_DECLS

MFILE  *mopen(const void *, long, int);
int     mgetc(MFILE *stream);
size_t  mread(void *, size_t, size_t, MFILE *);
int     mseek(MFILE *, long, int);
long    mtell(MFILE *);
int     mclose(MFILE *);
int	meof(MFILE *);

LIBXMP_END_DECLS

#endif
