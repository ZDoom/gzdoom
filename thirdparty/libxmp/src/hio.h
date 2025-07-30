#ifndef XMP_HIO_H
#define XMP_HIO_H

#include "callbackio.h"
#include "memio.h"

#define HIO_HANDLE_TYPE(x) ((x)->type)

enum hio_type {
	HIO_HANDLE_TYPE_FILE,
	HIO_HANDLE_TYPE_MEMORY,
	HIO_HANDLE_TYPE_CBFILE
};

typedef struct {
	enum hio_type type;
	long size;
	union {
		FILE *file;
		MFILE *mem;
		CBFILE *cbfile;
	} handle;
	int error;
	int noclose;
} HIO_HANDLE;

int8	hio_read8s	(HIO_HANDLE *);
uint8	hio_read8	(HIO_HANDLE *);
uint16	hio_read16l	(HIO_HANDLE *);
uint16	hio_read16b	(HIO_HANDLE *);
uint32	hio_read24l	(HIO_HANDLE *);
uint32	hio_read24b	(HIO_HANDLE *);
uint32	hio_read32l	(HIO_HANDLE *);
uint32	hio_read32b	(HIO_HANDLE *);
size_t	hio_read	(void *, size_t, size_t, HIO_HANDLE *);
int	hio_seek	(HIO_HANDLE *, long, int);
long	hio_tell	(HIO_HANDLE *);
int	hio_eof		(HIO_HANDLE *);
int	hio_error	(HIO_HANDLE *);
HIO_HANDLE *hio_open	(const char *, const char *);
HIO_HANDLE *hio_open_mem  (const void *, long, int);
HIO_HANDLE *hio_open_file (FILE *);
HIO_HANDLE *hio_open_file2 (FILE *);/* allows fclose()ing the file by libxmp */
HIO_HANDLE *hio_open_callbacks (void *, struct xmp_callbacks);
int	hio_reopen_mem	(const void *, long, int, HIO_HANDLE *);
int	hio_reopen_file	(FILE *, int, HIO_HANDLE *);
int	hio_close	(HIO_HANDLE *);
long	hio_size	(HIO_HANDLE *);

#endif
