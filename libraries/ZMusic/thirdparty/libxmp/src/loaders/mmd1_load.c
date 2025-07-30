/* Extended Module Player
 * Copyright (C) 1996-2023 Claudio Matsuoka and Hipolito Carraro Jr
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

/*
 * OctaMED v1.00b: ftp://ftp.funet.fi/pub/amiga/fish/501-600/ff579
 */

#include "med.h"
#include "loader.h"
#include "../med_extras.h"

static int mmd1_test(HIO_HANDLE *, char *, const int);
static int mmd1_load (struct module_data *, HIO_HANDLE *, const int);

const struct format_loader libxmp_loader_mmd1 = {
	"MED 2.10/OctaMED",
	mmd1_test,
	mmd1_load
};

static int mmd1_test(HIO_HANDLE *f, char *t, const int start)
{
	char id[4];
	uint32 offset, len;

	if (hio_read(id, 1, 4, f) < 4)
		return -1;

	if (memcmp(id, "MMD0", 4) && memcmp(id, "MMD1", 4) && memcmp(id, "MMDC", 4))
		return -1;

	hio_seek(f, 28, SEEK_CUR);
	offset = hio_read32b(f);		/* expdata_offset */

	if (offset) {
		hio_seek(f, start + offset + 44, SEEK_SET);
		offset = hio_read32b(f);
		len = hio_read32b(f);
		hio_seek(f, start + offset, SEEK_SET);
		libxmp_read_title(f, t, len);
	} else {
		libxmp_read_title(f, t, 0);
	}

	return 0;
}


static int mmd1_load(struct module_data *m, HIO_HANDLE *f, const int start)
{
	struct xmp_module *mod = &m->mod;
	int i, j, k;
	struct MMD0 header;
	struct MMD0song song;
	struct MMD1Block block;
	struct InstrExt *exp_smp = NULL;
	struct MMD0exp expdata;
	struct xmp_event *event;
	uint32 *blockarr = NULL;
	uint32 *smplarr = NULL;
	uint8 *patbuf = NULL;
	int ver = 0;
	int mmdc = 0;
	int smp_idx = 0;
	int song_offset;
	int blockarr_offset;
	int smplarr_offset;
	int expdata_offset;
	int expsmp_offset;
	int songname_offset;
	int iinfo_offset;
	int annotxt_offset;
	int bpm_on, bpmlen, med_8ch, hexvol;
	int max_lines;
	int retval = -1;

	LOAD_INIT();

	hio_read(&header.id, 4, 1, f);

	ver = *((char *)&header.id + 3) - '1' + 1;
	if (ver > 1) {
		ver = 0;
		mmdc = 1;
	}

	D_(D_WARN "load header");
	header.modlen = hio_read32b(f);
	song_offset = hio_read32b(f);
	D_(D_INFO "song_offset = 0x%08x", song_offset);
	hio_read16b(f);
	hio_read16b(f);
	blockarr_offset = hio_read32b(f);
	D_(D_INFO "blockarr_offset = 0x%08x", blockarr_offset);
	hio_read32b(f);
	smplarr_offset = hio_read32b(f);
	D_(D_INFO "smplarr_offset = 0x%08x", smplarr_offset);
	hio_read32b(f);
	expdata_offset = hio_read32b(f);
	D_(D_INFO "expdata_offset = 0x%08x", expdata_offset);
	hio_read32b(f);
	header.pstate = hio_read16b(f);
	header.pblock = hio_read16b(f);
	header.pline = hio_read16b(f);
	header.pseqnum = hio_read16b(f);
	header.actplayline = hio_read16b(f);
	header.counter = hio_read8(f);
	header.extra_songs = hio_read8(f);

	/*
	 * song structure
	 */
	D_(D_WARN "load song");
	if (hio_seek(f, start + song_offset, SEEK_SET) != 0) {
		D_(D_CRIT "seek error at song");
		return -1;
	}
	for (i = 0; i < 63; i++) {
		song.sample[i].rep = hio_read16b(f);
		song.sample[i].replen = hio_read16b(f);
		song.sample[i].midich = hio_read8(f);
		song.sample[i].midipreset = hio_read8(f);
		song.sample[i].svol = hio_read8(f);
		song.sample[i].strans = hio_read8s(f);
	}
	song.numblocks = hio_read16b(f);
	song.songlen = hio_read16b(f);

	/* Sanity check */
	if (song.numblocks > 255 || song.songlen > 256) {
		D_(D_CRIT "unsupported block count (%d) or song length (%d)",
		   song.numblocks, song.songlen);
		return -1;
	}

	D_(D_INFO "song.songlen = %d", song.songlen);
	for (i = 0; i < 256; i++)
		song.playseq[i] = hio_read8(f);
	song.deftempo = hio_read16b(f);
	song.playtransp = hio_read8(f);
	song.flags = hio_read8(f);
	song.flags2 = hio_read8(f);
	song.tempo2 = hio_read8(f);
	for (i = 0; i < 16; i++)
		song.trkvol[i] = hio_read8(f);
	song.mastervol = hio_read8(f);
	song.numsamples = hio_read8(f);

	/* Sanity check */
	if (song.numsamples > 63) {
		D_(D_CRIT "invalid instrument count %d", song.numsamples);
		return -1;
	}

	/*
	 * convert header
	 */
	m->c4rate = C4_NTSC_RATE;
	m->quirk |= song.flags & FLAG_STSLIDE ? 0 : QUIRK_VSALL | QUIRK_PBALL;
	hexvol = song.flags & FLAG_VOLHEX;
	med_8ch = song.flags & FLAG_8CHANNEL;
	bpm_on = song.flags2 & FLAG2_BPM;
	bpmlen = 1 + (song.flags2 & FLAG2_BMASK);
	m->time_factor = MED_TIME_FACTOR;

	mmd_set_bpm(m, med_8ch, song.deftempo, bpm_on, bpmlen);

	mod->spd = song.tempo2;
	mod->pat = song.numblocks;
	mod->ins = song.numsamples;
	mod->len = song.songlen;
	mod->rst = 0;
	mod->chn = 0;
	memcpy(mod->xxo, song.playseq, mod->len);
	mod->name[0] = 0;

	/*
	 * Read smplarr
	 */
	D_(D_WARN "read smplarr");
	smplarr = (uint32 *) malloc(mod->ins * sizeof(uint32));
	if (smplarr == NULL) {
		return -1;
	}
	if (hio_seek(f, start + smplarr_offset, SEEK_SET) != 0) {
		D_(D_CRIT "seek error at smplarr");
		goto err_cleanup;
	}
	for (i = 0; i < mod->ins; i++) {
		smplarr[i] = hio_read32b(f);
		if (hio_eof(f)) {
			D_(D_CRIT "read error at smplarr pos %d", i);
			goto err_cleanup;
		}
	}

	/*
	 * Obtain number of samples from each instrument
	 */
	mod->smp = 0;
	for (i = 0; i < mod->ins; i++) {
		int16 type;
		if (smplarr[i] == 0)
			continue;
		if (hio_seek(f, start + smplarr[i], SEEK_SET) != 0) {
			D_(D_CRIT "seek error at instrument %d", i);
			goto err_cleanup;
		}

		hio_read32b(f);				/* length */
		type = hio_read16b(f);
		if (type == -1 || type == -2) {		/* type is synth? */
			int wforms;
			hio_seek(f, 14, SEEK_CUR);
			wforms = hio_read16b(f);

			/* Sanity check */
			if (wforms > 256) {
				D_(D_CRIT "invalid wform count at instrument %d", i);
				goto err_cleanup;
			}

			mod->smp += wforms;
		} else if (type >= 1 && type <= 6) {
			mod->smp += mmd_num_oct[type - 1];
		} else {
			mod->smp++;
		}
	}

	/*
	 * expdata
	 */
	D_(D_WARN "load expdata");
	expdata.s_ext_entries = 0;
	expdata.s_ext_entrsz = 0;
	expdata.i_ext_entries = 0;
	expdata.i_ext_entrsz = 0;
	expsmp_offset = 0;
	iinfo_offset = 0;
	if (expdata_offset) {
		if (hio_seek(f, start + expdata_offset, SEEK_SET) != 0) {
			D_(D_CRIT "seek error at expdata");
			goto err_cleanup;
		}
		hio_read32b(f);
		expsmp_offset = hio_read32b(f);
		D_(D_INFO "expsmp_offset = 0x%08x", expsmp_offset);
		expdata.s_ext_entries = hio_read16b(f);
		expdata.s_ext_entrsz = hio_read16b(f);
		annotxt_offset = hio_read32b(f);
		expdata.annolen = hio_read32b(f);
		iinfo_offset = hio_read32b(f);
		D_(D_INFO "iinfo_offset = 0x%08x", iinfo_offset);
		expdata.i_ext_entries = hio_read16b(f);
		expdata.i_ext_entrsz = hio_read16b(f);

		/* Sanity check */
		if (expsmp_offset < 0 ||
		    annotxt_offset < 0 ||
		    expdata.annolen > 0x10000 ||
		    iinfo_offset < 0) {
			D_(D_CRIT "invalid expdata (annotxt=0x%08x annolen=0x%08x)",
			   annotxt_offset, expdata.annolen);
			goto err_cleanup;
		}

		hio_read32b(f);
		hio_read32b(f);
		hio_read32b(f);
		hio_read32b(f);
		songname_offset = hio_read32b(f);
		expdata.songnamelen = hio_read32b(f);
		D_(D_INFO "songname_offset = 0x%08x", songname_offset);
		D_(D_INFO "expdata.songnamelen = %d", expdata.songnamelen);

		hio_seek(f, start + songname_offset, SEEK_SET);
		for (i = 0; i < expdata.songnamelen; i++) {
			if (i >= XMP_NAME_SIZE)
				break;
			mod->name[i] = hio_read8(f);
		}

		/* Read annotation */
		if (annotxt_offset != 0 && expdata.annolen != 0) {
			D_(D_INFO "annotxt_offset = 0x%08x", annotxt_offset);
			m->comment = (char *) malloc(expdata.annolen + 1);
			if (m->comment != NULL) {
				hio_seek(f, start + annotxt_offset, SEEK_SET);
				hio_read(m->comment, 1, expdata.annolen, f);
				m->comment[expdata.annolen] = 0;
			}
		}
	}

	/*
	 * Read blockarr.
	 */
	D_(D_WARN "read blockarr");
	blockarr = (uint32 *) malloc(mod->pat * sizeof(uint32));
	if (blockarr == NULL) {
		goto err_cleanup;
	}
	if (hio_seek(f, start + blockarr_offset, SEEK_SET) != 0) {
		D_(D_CRIT "seek error at blockarr");
		goto err_cleanup;
	}
	for (i = 0; i < mod->pat; i++) {
		blockarr[i] = hio_read32b(f);
		if (hio_error(f)) {
			D_(D_CRIT "read error at blockarr pos %d", i);
			goto err_cleanup;
		}
	}

	/*
	 * Quickly scan patterns to check the number of channels
	 */
	D_(D_WARN "find number of channels");

	max_lines = 1;
	for (i = 0; i < mod->pat; i++) {
		D_(D_INFO "block %d block_offset = 0x%08x", i, blockarr[i]);
		if (blockarr[i] == 0)
			continue;

		if (hio_seek(f, start + blockarr[i], SEEK_SET) != 0) {
			D_(D_CRIT "seek error at block %d", i);
			goto err_cleanup;
		}

		if (ver > 0) {
			block.numtracks = hio_read16b(f);
			block.lines = hio_read16b(f);
		} else {
			block.numtracks = hio_read8(f);
			block.lines = hio_read8(f);
		}

		/* Sanity check--Amiga OctaMED files have an upper bound of 3200 lines per block. */
		if (block.lines + 1 > 3200) {
			D_(D_CRIT "invalid line count %d in block %d", block.lines + 1, i);
			goto err_cleanup;
		}

		if (block.numtracks > mod->chn) {
			mod->chn = block.numtracks;
		}
		if (block.lines + 1 > max_lines) {
			max_lines = block.lines + 1;
		}
	}

	/* Sanity check */
	/* MMD0/MMD1 can't have more than 16 channels... */
	if (mod->chn > MIN(16, XMP_MAX_CHANNELS)) {
		D_(D_CRIT "invalid channel count %d", mod->chn);
		goto err_cleanup;
	}

	mod->trk = mod->pat * mod->chn;

	libxmp_set_type(m, ver == 0 ? mmdc ? "MED Packer MMDC" :
				mod->chn > 4 ? "OctaMED 2.00 MMD0" :
				"MED 2.10 MMD0" : "OctaMED 4.00 MMD1");

	MODULE_INFO();

	D_(D_INFO "BPM mode: %s (length = %d)", bpm_on ? "on" : "off", bpmlen);
	D_(D_INFO "Song transpose: %d", song.playtransp);
	D_(D_INFO "Stored patterns: %d", mod->pat);

	/*
	 * Read and convert patterns
	 */
	D_(D_WARN "read patterns");
	if (libxmp_init_pattern(mod) < 0)
		goto err_cleanup;

	if ((patbuf = (uint8 *)malloc(mod->chn * max_lines * 4)) == NULL) {
		goto err_cleanup;
	}

	for (i = 0; i < mod->pat; i++) {
		uint8 *pos;
		size_t size;

		if (blockarr[i] == 0)
			continue;

		if (hio_seek(f, start + blockarr[i], SEEK_SET) != 0) {
			D_(D_CRIT "seek error at block %d", i);
			goto err_cleanup;
		}

		if (ver > 0) {
			block.numtracks = hio_read16b(f);
			block.lines = hio_read16b(f);
			hio_read32b(f);
		} else {
			block.numtracks = hio_read8(f);
			block.lines = hio_read8(f);
		}

		size = block.numtracks * (block.lines + 1) * (ver ? 4 : 3);

		if (mmdc) {
			/* MMDC is just MMD0 with simple pattern packing. */
			memset(patbuf, 0, size);
			for (j = 0; j < size;) {
				unsigned pack = hio_read8(f);
				if (hio_error(f)) {
					D_(D_CRIT "read error in block %d", i);
					goto err_cleanup;
				}

				if (pack & 0x80) {
					/* Run of 0 */
					j += 256 - pack;
					continue;
				}
				/* Uncompressed block */
				pack++;
				if (pack > size - j)
					pack = size - j;

				if (hio_read(patbuf + j, 1, pack, f) < pack) {
					D_(D_CRIT "read error in block %d", i);
					goto err_cleanup;
				}
				j += pack;
			}
		} else {
			if (hio_read(patbuf, 1, size, f) < size) {
				D_(D_CRIT "read error in block %d", i);
				goto err_cleanup;
			}
		}

		if (libxmp_alloc_pattern_tracks_long(mod, i, block.lines + 1) < 0)
			goto err_cleanup;

		pos = patbuf;
		if (ver > 0) {		/* MMD1 */
			for (j = 0; j < mod->xxp[i]->rows; j++) {
				for (k = 0; k < block.numtracks; k++) {
					event = &EVENT(i, k, j);
					event->note = pos[0] & 0x7f;
					if (event->note)
						event->note +=
						    12 + song.playtransp;

					if (event->note >= XMP_MAX_KEYS)
						event->note = 0;

					event->ins = pos[1] & 0x3f;

					/* Decay */
					if (event->ins && !event->note) {
						event->f2t = FX_MED_HOLD;
					}

					event->fxt = pos[2];
					event->fxp = pos[3];
					mmd_xlat_fx(event, bpm_on, bpmlen,
							med_8ch, hexvol);
					pos += 4;
				}
			}
		} else {		/* MMD0 */
			for (j = 0; j < mod->xxp[i]->rows; j++) {
				for (k = 0; k < block.numtracks; k++) {
					event = &EVENT(i, k, j);
					event->note = pos[0] & 0x3f;
					if (event->note)
						event->note += 12 + song.playtransp;

					if (event->note >= XMP_MAX_KEYS)
						event->note = 0;

					event->ins =
					    (pos[1] >> 4) | ((pos[0] & 0x80) >> 3)
					    | ((pos[0] & 0x40) >> 1);

					/* Decay */
					if (event->ins && !event->note) {
						event->f2t = FX_MED_HOLD;
					}

					event->fxt = pos[1] & 0x0f;
					event->fxp = pos[2];
					mmd_xlat_fx(event, bpm_on, bpmlen,
							med_8ch, hexvol);
					pos += 3;
				}
			}
		}
	}
	free(patbuf);
	patbuf = NULL;

	if (libxmp_med_new_module_extras(m))
		goto err_cleanup;

	/*
	 * Read and convert instruments and samples
	 */
	D_(D_WARN "read instruments");
	if (libxmp_init_instrument(m) < 0)
		goto err_cleanup;

	D_(D_INFO "Instruments: %d", mod->ins);

	/* Instrument extras */
	exp_smp = (struct InstrExt *) calloc(mod->ins, sizeof(struct InstrExt));
	if (exp_smp == NULL) {
		goto err_cleanup;
	}

	if (expsmp_offset) {
		if (hio_seek(f, start + expsmp_offset, SEEK_SET) != 0) {
			D_(D_CRIT "seek error at expsmp");
			goto err_cleanup;
		}

		for (i = 0; i < mod->ins && i < expdata.s_ext_entries; i++) {
			int skip = expdata.s_ext_entrsz - 4;

			D_(D_INFO "sample %d expsmp_offset = 0x%08lx", i, hio_tell(f));

			exp_smp[i].hold = hio_read8(f);
			exp_smp[i].decay = hio_read8(f);
			exp_smp[i].suppress_midi_off = hio_read8(f);
			exp_smp[i].finetune = hio_read8(f);

			if (hio_error(f)) {
				D_(D_CRIT "read error at expsmp");
				goto err_cleanup;
			}

			if (skip && hio_seek(f, skip, SEEK_CUR) != 0) {
				D_(D_CRIT "seek error at expsmp");
				goto err_cleanup;
			}
		}
	}

	/* Instrument names */
	if (iinfo_offset) {
		uint8 name[40];

		if (hio_seek(f, start + iinfo_offset, SEEK_SET) != 0) {
			D_(D_CRIT "seek error at iinfo");
			goto err_cleanup;
		}

		for (i = 0; i < mod->ins && i < expdata.i_ext_entries; i++) {
			int skip = expdata.i_ext_entrsz - 40;

			D_(D_INFO "sample %d iinfo_offset = 0x%08lx", i, hio_tell(f));

			if (hio_read(name, 40, 1, f) < 1) {
				D_(D_CRIT "read error at iinfo %d", i);
				goto err_cleanup;
			}
			libxmp_instrument_name(mod, i, name, 40);

			if (skip && hio_seek(f, skip, SEEK_CUR) != 0) {
				D_(D_CRIT "seek error at iinfo %d", i);
				goto err_cleanup;
			}
		}
	}

	/* Sample data */
	for (smp_idx = i = 0; i < mod->ins; i++) {
		D_(D_INFO "sample %d smpl_offset = 0x%08x", i, smplarr[i]);
		if (smplarr[i] == 0) {
			continue;
		}

		if (hio_seek(f, start + smplarr[i], SEEK_SET) < 0) {
			D_(D_CRIT "seek error at instrument %d", i);
			goto err_cleanup;
		}

		smp_idx = mmd_load_instrument(f, m, i, smp_idx, &expdata,
			&exp_smp[i], &song.sample[i], ver);

		if (smp_idx < 0) {
			goto err_cleanup;
		}
	}

	for (i = 0; i < mod->chn; i++) {
		mod->xxc[i].vol = song.trkvol[i];
		mod->xxc[i].pan = DEFPAN((((i + 1) / 2) % 2) * 0xff);
	}

	m->read_event_type = READ_EVENT_MED;
	retval = 0;

    err_cleanup:
	free(exp_smp);
	free(blockarr);
	free(smplarr);
	free(patbuf);

	return retval;
}
