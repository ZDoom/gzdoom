/* Extended Module Player
 * Copyright (C) 1996-2024 Claudio Matsuoka and Hipolito Carraro Jr
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

#include <stddef.h> /* offsetof() */
#include "loader.h"

static int umx_test (HIO_HANDLE *, char *, const int);
static int umx_load (struct module_data *, HIO_HANDLE *, const int);

const struct format_loader libxmp_loader_umx = {
	"Epic Games UMX",
	umx_test,
	umx_load
};

/* UPKG parsing partially based on Unreal Media Ripper (UMR) v0.3
 * by Andy Ward <wardwh@swbell.net>, with additional fixes & updates
 * by O. Sezer - see git repo at https://github.com/sezero/umr.git
 */
typedef int32 fci_t;		/* FCompactIndex */

#define UPKG_HDR_TAG		0x9e2a83c1

struct _genhist {	/* for upkg versions >= 68 */
	int32 export_count;
	int32 name_count;
};

struct upkg_hdr {
	uint32 tag;	/* UPKG_HDR_TAG */
	int32 file_version;
	uint32 pkg_flags;
	int32 name_count;	/* number of names in name table (>= 0) */
	int32 name_offset;		/* offset to name table  (>= 0) */
	int32 export_count;	/* num. exports in export table  (>= 0) */
	int32 export_offset;		/* offset to export table (>= 0) */
	int32 import_count;	/* num. imports in export table  (>= 0) */
	int32 import_offset;		/* offset to import table (>= 0) */

	/* number of GUIDs in heritage table (>= 1) and table's offset:
	 * only with versions < 68. */
	int32 heritage_count;
	int32 heritage_offset;
	/* with versions >= 68:  a GUID, a dword for generation count
	 * and export_count and name_count dwords for each generation: */
	uint32 guid[4];
	int32 generation_count;
#define UPKG_HDR_SIZE 64			/* 64 bytes up until here */
	struct _genhist *gen;
};
/* compile time assert for upkg_hdr size */
typedef int _check_hdrsize[2 * (offsetof(struct upkg_hdr, gen) == UPKG_HDR_SIZE) - 1];

#define UMUSIC_IT	0
#define UMUSIC_S3M	1
#define UMUSIC_XM	2
#define UMUSIC_MOD	3
#define UMUSIC_WAV	4
#define UMUSIC_MP2	5

static const char *const mustype[] = {
	"IT", "S3M", "XM", "MOD",
	"WAV", "MP2", NULL
};

/* decode an FCompactIndex.
 * original documentation by Tim Sweeney was at
 * http://unreal.epicgames.com/Packages.htm
 * also see Unreal Wiki:
 * http://wiki.beyondunreal.com/Legacy:Package_File_Format/Data_Details
 */
static fci_t get_fci (const char *in, int *pos)
{
	int32 a;
	int size;

	size = 1;
	a = in[0] & 0x3f;

	if (in[0] & 0x40) {
		size++;
		a |= (in[1] & 0x7f) << 6;

		if (in[1] & 0x80) {
			size++;
			a |= (in[2] & 0x7f) << 13;

			if (in[2] & 0x80) {
				size++;
				a |= (in[3] & 0x7f) << 20;

				if (in[3] & 0x80) {
					size++;
					a |= (in[4] & 0x3f) << 27;
				}
			}
		}
	}

	if (in[0] & 0x80)
		a = -a;

	*pos += size;

	return a;
}

static int get_objtype (HIO_HANDLE *f, int32 ofs, int type)
{
	char sig[16];
_retry:
	memset(sig, 0, sizeof(sig));
	hio_seek(f, ofs, SEEK_SET);
	hio_read(sig, 16, 1, f);
	if (type == UMUSIC_IT) {
		if (memcmp(sig, "IMPM", 4) == 0)
			return UMUSIC_IT;
		return -1;
	}
	if (type == UMUSIC_XM) {
		if (memcmp(sig, "Extended Module:", 16) != 0)
			return -1;
		hio_read(sig, 16, 1, f);
		if (sig[0] != ' ') return -1;
		hio_read(sig, 16, 1, f);
		if (sig[5] != 0x1a) return -1;
		return UMUSIC_XM;
	}
	if (type == UMUSIC_MP2) {
		unsigned char *p = (unsigned char *)sig;
		uint16 u = ((p[0] << 8) | p[1]) & 0xFFFE;
		if (u == 0xFFFC || u == 0xFFF4)
			return UMUSIC_MP2;
		return -1;
	}
	if (type == UMUSIC_WAV) {
		if (memcmp(sig, "RIFF", 4) == 0 && memcmp(&sig[8], "WAVE", 4) == 0)
			return UMUSIC_WAV;
		return -1;
	}

	hio_seek(f, ofs + 44, SEEK_SET);
	hio_read(sig, 4, 1, f);
	if (type == UMUSIC_S3M) {
		if (memcmp(sig, "SCRM", 4) == 0)
			return UMUSIC_S3M;
		/*return -1;*/
		/* SpaceMarines.umx and Starseek.umx from Return to NaPali
		 * report as "s3m" whereas the actual music format is "it" */
		type = UMUSIC_IT;
		goto _retry;
	}

	hio_seek(f, ofs + 1080, SEEK_SET);
	hio_read(sig, 4, 1, f);
	if (type == UMUSIC_MOD) {
		if (memcmp(sig, "M.K.", 4) == 0 || memcmp(sig, "M!K!", 4) == 0)
			return UMUSIC_MOD;
		return -1;
	}

	return -1;
}

static int read_export (HIO_HANDLE *f, const struct upkg_hdr *hdr,
			int32 *ofs, int32 *objsize)
{
	char buf[40];
	int idx = 0, t;

	hio_seek(f, *ofs, SEEK_SET);
	if (hio_read(buf, 4, 10, f) < 10)
		return -1;

	if (hdr->file_version < 40) idx += 8;	/* 00 00 00 00 00 00 00 00 */
	if (hdr->file_version < 60) idx += 16;	/* 81 00 00 00 00 00 FF FF FF FF FF FF FF FF 00 00 */
	get_fci(&buf[idx], &idx);		/* skip junk */
	t = get_fci(&buf[idx], &idx);		/* type_name */
	if (hdr->file_version > 61) idx += 4;	/* skip export size */
	*objsize = get_fci(&buf[idx], &idx);
	*ofs += idx;	/* offset for real data */

	return t;	/* return type_name index */
}

static int read_typname(HIO_HANDLE *f, const struct upkg_hdr *hdr,
			int idx, char *out)
{
	int i, s;
	long l;
	char buf[64];

	if (idx >= hdr->name_count) return -1;
	memset(buf, 0, 64);
	for (i = 0, l = 0; i <= idx; i++) {
		if (hio_seek(f, hdr->name_offset + l, SEEK_SET) < 0) return -1;
		if (!hio_read(buf, 1, 63, f)) return -1;
		if (hdr->file_version >= 64) {
			s = *(signed char *)buf; /* numchars *including* terminator */
			if (s <= 0) return -1;
			l += s + 5;	/* 1 for buf[0], 4 for int32 name_flags */
		} else {
			l += (long)strlen(buf);
			l +=  5;	/* 1 for terminator, 4 for int32 name_flags */
		}
	}

	strcpy(out, (hdr->file_version >= 64)? &buf[1] : buf);
	return 0;
}

static void umx_strupr(char *str)
{
	while (*str) {
		if (*str >= 'a' && *str <= 'z') {
		    *str -= ('a' - 'A');
		}
		str++;
	}
}

static int probe_umx   (HIO_HANDLE *f, const struct upkg_hdr *hdr,
			int32 *ofs, int32 *objsize)
{
	int i, idx, t;
	int32 s, pos;
	long fsiz;
	char buf[64];

	idx = 0;
	fsiz = hio_size(f);

	if (hdr->name_offset	>= fsiz ||
	    hdr->export_offset	>= fsiz ||
	    hdr->import_offset	>= fsiz) {
		D_(D_INFO "UMX: Illegal values in header.\n");
		return -1;
	}

	/* Find the offset and size of the first IT, S3M or XM
	 * by parsing the exports table. The umx files should
	 * have only one export. Kran32.umx from Unreal has two,
	 * but both pointing to the same music. */
	if (hdr->export_offset >= fsiz) return -1;
	memset(buf, 0, 64);
	hio_seek(f, hdr->export_offset, SEEK_SET);
	hio_read(buf, 1, 64, f);

	get_fci(&buf[idx], &idx);	/* skip class_index */
	get_fci(&buf[idx], &idx);	/* skip super_index */
	if (hdr->file_version >= 60) idx += 4; /* skip int32 package_index */
	get_fci(&buf[idx], &idx);	/* skip object_name */
	idx += 4;			/* skip int32 object_flags */

	s = get_fci(&buf[idx], &idx);	/* get serial_size */
	if (s <= 0) return -1;
	pos = get_fci(&buf[idx],&idx);	/* get serial_offset */
	if (pos < 0 || pos > fsiz - 40) return -1;

	if ((t = read_export(f, hdr, &pos, &s)) < 0) return -1;
	if (s <= 0 || s > fsiz - pos) return -1;

	if (read_typname(f, hdr, t, buf) < 0) return -1;
	umx_strupr(buf);
	for (i = 0; mustype[i] != NULL; i++) {
		if (!strcmp(buf, mustype[i])) {
			t = i;
			break;
		}
	}
	if (mustype[i] == NULL) return -1;
	if ((t = get_objtype(f, pos, t)) < 0) return -1;

	*ofs = pos;
	*objsize = s;
	return t;
}

static int32 probe_header (HIO_HANDLE *f, struct upkg_hdr *hdr)
{
	hdr->tag           = hio_read32l(f);
	hdr->file_version  = (int32) hio_read32l(f);
	hdr->pkg_flags     = hio_read32l(f);
	hdr->name_count    = (int32) hio_read32l(f);
	hdr->name_offset   = (int32) hio_read32l(f);
	hdr->export_count  = (int32) hio_read32l(f);
	hdr->export_offset = (int32) hio_read32l(f);
	hdr->import_count  = (int32) hio_read32l(f);
	hdr->import_offset = (int32) hio_read32l(f);

	if (hdr->tag != UPKG_HDR_TAG) {
		D_(D_INFO "UMX: Unknown header tag 0x%x\n", hdr->tag);
		return -1;
	}
	if (hdr->name_count	< 0	||
	    hdr->export_count	< 0	||
	    hdr->import_count	< 0	||
	    hdr->name_offset	< 36	||
	    hdr->export_offset	< 36	||
	    hdr->import_offset	< 36) {
		D_(D_INFO "UMX: Illegal values in header.\n");
		return -1;
	}

#if 1 /* no need being overzealous */
	return 0;
#else
	switch (hdr->file_version) {
	case 35: case 37:	/* Unreal beta - */
	case 40: case 41:				/* 1998 */
	case 61:/* Unreal */
	case 62:/* Unreal Tournament */
	case 63:/* Return to NaPali */
	case 64:/* Unreal Tournament */
	case 66:/* Unreal Tournament */
	case 68:/* Unreal Tournament */
	case 69:/* Tactical Ops */
	case 75:/* Harry Potter and the Philosopher's Stone */
	case 76:			/* mpeg layer II data */
	case 83:/* Mobile Forces */
		return 0;
	}

	D_(D_INFO "UMX: Unknown upkg version %d\n", hdr->file_version);
	return -1;
#endif /* #if 0  */
}

static int process_upkg (HIO_HANDLE *f, int32 *ofs, int32 *objsize)
{
	struct upkg_hdr header;

	memset(&header, 0, sizeof(header));
	if (probe_header(f, &header) < 0) return -1;
	return probe_umx(f, &header, ofs, objsize);
}

static int umx_test(HIO_HANDLE *f, char *t, const int start)
{
	int32 ofs, size;
	int type;

	type = process_upkg(f, &ofs, &size);
	(void) hio_error(f);
	if (type < 0) {
		return -1;
	}

	ofs += start; /** FIXME? **/
	switch (type) {
	case UMUSIC_IT:
		hio_seek(f, ofs + 4, SEEK_SET);
		libxmp_read_title(f, t, 26);
		return 0;
	case UMUSIC_S3M:
		hio_seek(f, ofs, SEEK_SET);
		libxmp_read_title(f, t, 28);
		return 0;
	case UMUSIC_XM:
		hio_seek(f, ofs + 17, SEEK_SET);
		libxmp_read_title(f, t, 20);
		return 0;
	case UMUSIC_MOD:
		hio_seek(f, ofs, SEEK_SET);
		libxmp_read_title(f, t, 20);
		return 0;
	}

	return -1;
}

static int umx_load(struct module_data *m, HIO_HANDLE *f, const int start)
{
	int32 ofs, size;
	int type;

	LOAD_INIT();

	D_(D_INFO "Container type : Epic Games UMX");

	type = process_upkg(f, &ofs, &size);
	(void) hio_error(f);
	if (type < 0) {
		return -1;
	}

	D_(D_INFO "UMX: %s data @ 0x%x, %d bytes\n", mustype[type], ofs, size);

	ofs += start; /** FIXME? **/
	hio_seek(f, ofs, SEEK_SET);
	switch (type) {
	case UMUSIC_IT:
		return libxmp_loader_it.loader(m, f, ofs);
	case UMUSIC_S3M:
		return libxmp_loader_s3m.loader(m, f, ofs);
	case UMUSIC_XM:
		return libxmp_loader_xm.loader(m, f, ofs);
	case UMUSIC_MOD:
		return libxmp_loader_mod.loader(m, f, ofs);
	}

	return -1;
}
