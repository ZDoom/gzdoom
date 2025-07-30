#ifndef LIBXMP_IFF_H
#define LIBXMP_IFF_H

#include "../hio.h"
#include "../list.h"

#define IFF_NOBUFFER 0x0001

#define IFF_LITTLE_ENDIAN	0x01
#define IFF_FULL_CHUNK_SIZE	0x02
#define IFF_CHUNK_ALIGN2	0x04
#define IFF_CHUNK_ALIGN4	0x08
#define IFF_SKIP_EMBEDDED	0x10
#define IFF_CHUNK_TRUNC4	0x20

#define IFF_MAX_CHUNK_SIZE	0x800000

typedef void *iff_handle;

struct iff_header {
	char form[4];		/* FORM */
	int len;		/* File length */
	char id[4];		/* IFF type identifier */
};

struct iff_info {
	char id[4];
	int (*loader)(struct module_data *, int, HIO_HANDLE *, void *);
	struct list_head list;
};

iff_handle libxmp_iff_new(void);
int	libxmp_iff_load(iff_handle, struct module_data *, HIO_HANDLE *, void *);
/* int libxmp_iff_chunk(iff_handle, struct module_data *, HIO_HANDLE *, void *); */
int 	libxmp_iff_register(iff_handle, const char *,
	int (*loader)(struct module_data *, int, HIO_HANDLE *, void *));
void 	libxmp_iff_id_size(iff_handle, int);
void 	libxmp_iff_set_quirk(iff_handle, int);
void 	libxmp_iff_release(iff_handle);
/* int 	libxmp_iff_process(iff_handle, struct module_data *, char *, long,
	HIO_HANDLE *, void *); */

#endif /* LIBXMP_IFF_H */
