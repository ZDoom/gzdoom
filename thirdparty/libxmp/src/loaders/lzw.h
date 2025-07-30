#ifndef LIBXMP_LOADER_LZW_H
#define LIBXMP_LOADER_LZW_H

#include "loader.h"

#define LZW_FLAG_MAXBITS(x)	((x) & 15)
#define LZW_FLAG_SYMQUIRKS	0x100

#define LZW_FLAGS_SYM		LZW_FLAG_MAXBITS(13) | LZW_FLAG_SYMQUIRKS

int libxmp_read_lzw(void *dest, size_t dest_len, size_t max_read_len,
		    int flags, HIO_HANDLE *f);

int libxmp_read_sigma_delta(void *dest, size_t dest_len, size_t max_read_len,
			    HIO_HANDLE *f);

#endif /* LIBXMP_LOADER_LZW_H */
