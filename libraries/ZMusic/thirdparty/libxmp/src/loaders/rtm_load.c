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
#include "../period.h"

/* Data structures from the specification of the RTM format version 1.10 by
 * Arnaud Hasenfratz
 */

struct ObjectHeader {
	char id[4];		/* "RTMM", "RTND", "RTIN" or "RTSM" */
	char rc;		/* 0x20 */
	char name[32];		/* object name */
	char eof;		/* "\x1A" */
	uint16 version;		/* version of the format (actual : 0x110) */
	uint16 headerSize;	/* object header size */
};

struct RTMMHeader {		/* Real Tracker Music Module */
	char software[20];	/* software used for saving the module */
	char composer[32];
	uint16 flags;		/* song flags */
				/* bit 0 : linear table,
				   bit 1 : track names present */
	uint8 ntrack;		/* number of tracks */
	uint8 ninstr;		/* number of instruments */
	uint16 nposition;	/* number of positions */
	uint16 npattern;	/* number of patterns */
	uint8 speed;		/* initial speed */
	uint8 tempo;		/* initial tempo */
	char panning[32];	/* initial pannings (for S3M compatibility) */
	uint32 extraDataSize;	/* length of data after the header */

/* version 1.12 */
	char originalName[32];
};

struct RTNDHeader {		/* Real Tracker Note Data */
	uint16 flags;		/* Always 1 */
	uint8 ntrack;
	uint16 nrows;
	uint32 datasize;	/* Size of packed data */
};

struct EnvelopePoint {
	long x;
	long y;
};

struct Envelope {
	uint8 npoint;
	struct EnvelopePoint point[12];
	uint8 sustain;
	uint8 loopstart;
	uint8 loopend;
	uint16 flags;		/* bit 0 : enable envelope,
				   bit 1 : sustain, bit 2 : loop */
};

struct RTINHeader {		/* Real Tracker Instrument */
	uint8 nsample;
	uint16 flags;		/* bit 0 : default panning enabled
				   bit 1 : mute samples */
	uint8 table[120];	/* sample number for each note */
	struct Envelope volumeEnv;
	struct Envelope panningEnv;
	char vibflg;		/* vibrato type */
	char vibsweep;		/* vibrato sweep */
	char vibdepth;		/* vibrato depth */
	char vibrate;		/* vibrato rate */
	uint16 volfade;

/* version 1.10 */
	uint8 midiPort;
	uint8 midiChannel;
	uint8 midiProgram;
	uint8 midiEnable;

/* version 1.12 */
	char midiTranspose;
	uint8 midiBenderRange;
	uint8 midiBaseVolume;
	char midiUseVelocity;
};

struct RTSMHeader {		/* Real Tracker Sample */
	uint16 flags;		/* bit 1 : 16 bits,
				   bit 2 : delta encoded (always) */
	uint8 basevolume;
	uint8 defaultvolume;
	uint32 length;
	uint8 loop;		/* =0:no loop, =1:forward loop,
				   =2:bi-directional loop */
	uint8 reserved[3];
	uint32 loopbegin;
	uint32 loopend;
	uint32 basefreq;
	uint8 basenote;
	char panning;		/* Panning from -64 to 64 */
};


static int rtm_test(HIO_HANDLE *, char *, const int);
static int rtm_load (struct module_data *, HIO_HANDLE *, const int);

const struct format_loader libxmp_loader_rtm = {
	"Real Tracker",
	rtm_test,
	rtm_load
};

static int rtm_test(HIO_HANDLE *f, char *t, const int start)
{
	char buf[4];

	if (hio_read(buf, 1, 4, f) < 4)
		return -1;
	if (memcmp(buf, "RTMM", 4))
		return -1;

	if (hio_read8(f) != 0x20)
		return -1;

	libxmp_read_title(f, t, 32);

	return 0;
}


#define MAX_SAMP 1024

static int read_object_header(HIO_HANDLE *f, struct ObjectHeader *h, const char *id)
{
	hio_read(h->id, 4, 1, f);
	D_(D_WARN "object id: %02x %02x %02x %02x", h->id[0],
					h->id[1], h->id[2], h->id[3]);

	if (memcmp(id, h->id, 4))
		return -1;

	h->rc = hio_read8(f);
	if (h->rc != 0x20)
		return -1;
	if (hio_read(h->name, 1, 32, f) != 32)
		return -1;
	h->eof = hio_read8(f);
	h->version = hio_read16l(f);
	h->headerSize = hio_read16l(f);
	D_(D_INFO "object %-4.4s (%d)", h->id, h->headerSize);

	return 0;
}


static int rtm_load(struct module_data *m, HIO_HANDLE *f, const int start)
{
	struct xmp_module *mod = &m->mod;
	int i, j, r;
	struct xmp_event *event;
	struct ObjectHeader oh;
	struct RTMMHeader rh;
	struct RTNDHeader rp;
	struct RTINHeader ri;
	struct RTSMHeader rs;
	int offset, smpnum, version;
	char tracker_name[21], composer[33];

	LOAD_INIT();

	if (read_object_header(f, &oh, "RTMM") < 0)
		return -1;

	version = oh.version;

	hio_read(tracker_name, 1, 20, f);
	tracker_name[20] = 0;
	hio_read(composer, 1, 32, f);
	composer[32] = 0;
	rh.flags = hio_read16l(f);	/* bit 0: linear table, bit 1: track names */
	rh.ntrack = hio_read8(f);
	rh.ninstr = hio_read8(f);
	rh.nposition = hio_read16l(f);
	rh.npattern = hio_read16l(f);
	rh.speed = hio_read8(f);
	rh.tempo = hio_read8(f);
	hio_read(rh.panning, 32, 1, f);
	rh.extraDataSize = hio_read32l(f);

	/* Sanity check */
	if (hio_error(f) || rh.nposition > 255 || rh.ntrack > 32 || rh.npattern > 255) {
		return -1;
	}

	if (version >= 0x0112)
		hio_seek(f, 32, SEEK_CUR);		/* skip original name */

	for (i = 0; i < rh.nposition; i++) {
		mod->xxo[i] = hio_read16l(f);
		if (mod->xxo[i] >= rh.npattern) {
			return -1;
		}
	}

	strncpy(mod->name, oh.name, 32);
	mod->name[32] = '\0';
	snprintf(mod->type, XMP_NAME_SIZE, "%s RTM %x.%02x",
				tracker_name, version >> 8, version & 0xff);
	/* strncpy(m->author, composer, XMP_NAME_SIZE); */

	mod->len = rh.nposition;
	mod->pat = rh.npattern;
	mod->chn = rh.ntrack;
	mod->trk = mod->chn * mod->pat;
	mod->ins = rh.ninstr;
	mod->spd = rh.speed;
	mod->bpm = rh.tempo;

	m->c4rate = C4_NTSC_RATE;
	m->period_type = rh.flags & 0x01 ? PERIOD_LINEAR : PERIOD_AMIGA;

	MODULE_INFO();

	for (i = 0; i < mod->chn; i++)
		mod->xxc[i].pan = rh.panning[i] & 0xff;

	if (libxmp_init_pattern(mod) < 0)
		return -1;

	D_(D_INFO "Stored patterns: %d", mod->pat);

	offset = 42 + oh.headerSize + rh.extraDataSize;

	for (i = 0; i < mod->pat; i++) {
		uint8 c;

		hio_seek(f, start + offset, SEEK_SET);

		if (read_object_header(f, &oh, "RTND") < 0) {
			D_(D_CRIT "Error reading pattern %d", i);
			return -1;
		}

		rp.flags = hio_read16l(f);
		rp.ntrack = hio_read8(f);
		rp.nrows = hio_read16l(f);
		rp.datasize = hio_read32l(f);

		/* Sanity check */
		if (rp.ntrack > rh.ntrack || rp.nrows > 256) {
			return -1;
		}

		offset += 42 + oh.headerSize + rp.datasize;

		if (libxmp_alloc_pattern_tracks(mod, i, rp.nrows) < 0)
			return -1;

		for (r = 0; r < rp.nrows; r++) {
			for (j = 0; /*j < rp.ntrack */; j++) {

				c = hio_read8(f);
				if (c == 0)		/* next row */
					break;

				/* Sanity check */
				if (j >= rp.ntrack) {
					return -1;
				}

				event = &EVENT(i, j, r);

				if (c & 0x01) {		/* set track */
					j = hio_read8(f);

					/* Sanity check */
					if (j >= rp.ntrack) {
						return -1;
					}

					event = &EVENT(i, j, r);
				}
				if (c & 0x02) {		/* read note */
					event->note = hio_read8(f) + 1;
					if (event->note == 0xff) {
						event->note = XMP_KEY_OFF;
					} else {
						event->note += 12;
					}
				}
				if (c & 0x04)		/* read instrument */
					event->ins = hio_read8(f);
				if (c & 0x08)		/* read effect */
					event->fxt = hio_read8(f);
				if (c & 0x10)		/* read parameter */
					event->fxp = hio_read8(f);
				if (c & 0x20)		/* read effect 2 */
					event->f2t = hio_read8(f);
				if (c & 0x40)		/* read parameter 2 */
					event->f2p = hio_read8(f);
			}
		}
	}

	/*
	 * load instruments
	 */

	D_(D_INFO "Instruments: %d", mod->ins);

	hio_seek(f, start + offset, SEEK_SET);

	/* ESTIMATED value! We don't know the actual value at this point */
	mod->smp = MAX_SAMP;

	if (libxmp_init_instrument(m) < 0)
		return -1;

	smpnum = 0;
	for (i = 0; i < mod->ins; i++) {
		struct xmp_instrument *xxi = &mod->xxi[i];

		if (read_object_header(f, &oh, "RTIN") < 0) {
			D_(D_CRIT "Error reading instrument %d", i);
			return -1;
		}

		libxmp_instrument_name(mod, i, (uint8 *)oh.name, 32);

		if (oh.headerSize == 0) {
			D_(D_INFO "[%2X] %-26.26s %2d ", i,
						xxi->name, xxi->nsm);
			ri.nsample = 0;
			continue;
		}

		ri.nsample = hio_read8(f);
		ri.flags = hio_read16l(f);	/* bit 0 : default panning enabled */
		if (hio_read(ri.table, 1, 120, f) != 120)
			return -1;

		ri.volumeEnv.npoint = hio_read8(f);

		/* Sanity check */
		if (ri.volumeEnv.npoint >= 12)
			return -1;

		for (j = 0; j < 12; j++) {
			ri.volumeEnv.point[j].x = hio_read32l(f);
			ri.volumeEnv.point[j].y = hio_read32l(f);
		}
		ri.volumeEnv.sustain = hio_read8(f);
		ri.volumeEnv.loopstart = hio_read8(f);
		ri.volumeEnv.loopend = hio_read8(f);
		ri.volumeEnv.flags = hio_read16l(f); /* bit 0:enable 1:sus 2:loop */

		ri.panningEnv.npoint = hio_read8(f);

		/* Sanity check */
		if (ri.panningEnv.npoint >= 12)
			return -1;

		for (j = 0; j < 12; j++) {
			ri.panningEnv.point[j].x = hio_read32l(f);
			ri.panningEnv.point[j].y = hio_read32l(f);
		}
		ri.panningEnv.sustain = hio_read8(f);
		ri.panningEnv.loopstart = hio_read8(f);
		ri.panningEnv.loopend = hio_read8(f);
		ri.panningEnv.flags = hio_read16l(f);

		ri.vibflg = hio_read8(f);
		ri.vibsweep = hio_read8(f);
		ri.vibdepth = hio_read8(f);
		ri.vibrate = hio_read8(f);
		ri.volfade = hio_read16l(f);

		if (version >= 0x0110) {
			ri.midiPort = hio_read8(f);
			ri.midiChannel = hio_read8(f);
			ri.midiProgram = hio_read8(f);
			ri.midiEnable = hio_read8(f);
		}
		if (version >= 0x0112) {
			ri.midiTranspose = hio_read8(f);
			ri.midiBenderRange = hio_read8(f);
			ri.midiBaseVolume = hio_read8(f);
			ri.midiUseVelocity = hio_read8(f);
		}

		xxi->nsm = ri.nsample;

		D_(D_INFO "[%2X] %-26.26s %2d", i, xxi->name, xxi->nsm);

		if (xxi->nsm > 16)
			xxi->nsm = 16;

		if (libxmp_alloc_subinstrument(mod, i, xxi->nsm) < 0)
			return -1;

		for (j = 0; j < 120; j++)
			xxi->map[j].ins = ri.table[j];

		/* Envelope */
		xxi->rls = ri.volfade;
		xxi->aei.npt = ri.volumeEnv.npoint;
		xxi->aei.sus = ri.volumeEnv.sustain;
		xxi->aei.lps = ri.volumeEnv.loopstart;
		xxi->aei.lpe = ri.volumeEnv.loopend;
		xxi->aei.flg = ri.volumeEnv.flags;
		xxi->pei.npt = ri.panningEnv.npoint;
		xxi->pei.sus = ri.panningEnv.sustain;
		xxi->pei.lps = ri.panningEnv.loopstart;
		xxi->pei.lpe = ri.panningEnv.loopend;
		xxi->pei.flg = ri.panningEnv.flags;

		for (j = 0; j < xxi->aei.npt; j++) {
			xxi->aei.data[j * 2 + 0] = ri.volumeEnv.point[j].x;
			xxi->aei.data[j * 2 + 1] = ri.volumeEnv.point[j].y / 2;
		}
		for (j = 0; j < xxi->pei.npt; j++) {
			xxi->pei.data[j * 2 + 0] = ri.panningEnv.point[j].x;
			xxi->pei.data[j * 2 + 1] = 32 + ri.panningEnv.point[j].y / 2;
		}

		/* For each sample */
		for (j = 0; j < xxi->nsm; j++, smpnum++) {
			struct xmp_subinstrument *sub = &xxi->sub[j];
			struct xmp_sample *xxs;

			if (read_object_header(f, &oh, "RTSM") < 0) {
				D_(D_CRIT "Error reading sample %d", j);
				return -1;
			}

			rs.flags = hio_read16l(f);
			rs.basevolume = hio_read8(f);
			rs.defaultvolume = hio_read8(f);
			rs.length = hio_read32l(f);
			rs.loop = hio_read32l(f);
			rs.loopbegin = hio_read32l(f);
			rs.loopend = hio_read32l(f);
			rs.basefreq = hio_read32l(f);
			rs.basenote = hio_read8(f);
			rs.panning = hio_read8(f);

			libxmp_c2spd_to_note(rs.basefreq, &sub->xpo, &sub->fin);
			sub->xpo += 48 - rs.basenote;
			sub->vol = rs.defaultvolume * rs.basevolume / 0x40;
			sub->pan = 0x80 + rs.panning * 2;
			sub->vwf = ri.vibflg;
			sub->vde = ri.vibdepth << 2;
			sub->vra = ri.vibrate;
			sub->vsw = ri.vibsweep;
			sub->sid = smpnum;

			if (smpnum >= mod->smp) {
				if (libxmp_realloc_samples(m, mod->smp * 3 / 2) < 0)
					return -1;
			}
 			xxs = &mod->xxs[smpnum];

			libxmp_copy_adjust(xxs->name, (uint8 *)oh.name, 31);

			xxs->len = rs.length;
			xxs->lps = rs.loopbegin;
			xxs->lpe = rs.loopend;

			xxs->flg = 0;
			if (rs.flags & 0x02) {
				xxs->flg |= XMP_SAMPLE_16BIT;
				xxs->len >>= 1;
				xxs->lps >>= 1;
				xxs->lpe >>= 1;
			}

			xxs->flg |= rs.loop & 0x03 ?  XMP_SAMPLE_LOOP : 0;
			xxs->flg |= rs.loop == 2 ? XMP_SAMPLE_LOOP_BIDIR : 0;

			D_(D_INFO "  [%1x] %05x%c%05x %05x %c "
						"V%02x F%+04d P%02x R%+03d",
				j, xxs->len,
				xxs->flg & XMP_SAMPLE_16BIT ? '+' : ' ',
				xxs->lps, xxs->lpe,
				xxs->flg & XMP_SAMPLE_LOOP_BIDIR ? 'B' :
				xxs->flg & XMP_SAMPLE_LOOP ? 'L' : ' ',
				sub->vol, sub->fin, sub->pan, sub->xpo);

			if (libxmp_load_sample(m, f, SAMPLE_FLAG_DIFF, xxs, NULL) < 0)
				return -1;
		}
	}

	/* Final sample number adjustment */
	if (libxmp_realloc_samples(m, smpnum) < 0)
		return -1;

	m->quirk |= QUIRKS_FT2;
	m->read_event_type = READ_EVENT_FT2;

	return 0;
}
