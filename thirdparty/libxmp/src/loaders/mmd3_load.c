/* Extended Module Player
 * Copyright (C) 1996-2025 Claudio Matsuoka and Hipolito Carraro Jr
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

#include "med.h"
#include "loader.h"
#include "../med_extras.h"

static int mmd3_test (HIO_HANDLE *, char *, const int);
static int mmd3_load (struct module_data *, HIO_HANDLE *, const int);

const struct format_loader libxmp_loader_mmd3 = {
	"OctaMED",
	mmd3_test,
	mmd3_load
};

static int mmd3_test(HIO_HANDLE *f, char *t, const int start)
{
	char id[4];
	uint32 offset, len;

	if (hio_read(id, 1, 4, f) < 4)
		return -1;

	if (memcmp(id, "MMD2", 4) && memcmp(id, "MMD3", 4))
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


static int mmd3_load(struct module_data *m, HIO_HANDLE *f, const int start)
{
	struct xmp_module *mod = &m->mod;
	int i, j, k;
	struct MMD0 header;
	struct MMD2song song;
	struct MMD1Block block;
	struct InstrExt *exp_smp = NULL;
	struct MMD0exp expdata;
	struct xmp_event *event;
	uint32 *blockarr = NULL;
	uint32 *smplarr = NULL;
	uint8 *patbuf = NULL;
	int ver = 0;
	int smp_idx = 0;
	int song_offset;
	int seqtable_offset;
	int trackvols_offset;
	int trackpans_offset;
	int blockarr_offset;
	int smplarr_offset;
	int expdata_offset;
	int expsmp_offset;
	int songname_offset;
	int iinfo_offset;
	int mmdinfo_offset;
	int playseq_offset;
	int bpm_on, bpmlen, med_8ch, hexvol;
	int max_lines;
	int retval = -1;

	LOAD_INIT();

	hio_read(&header.id, 4, 1, f);

	ver = *((char *)&header.id + 3) - '1' + 1;

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
	hio_seek(f, start + song_offset, SEEK_SET);
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
	D_(D_INFO "song.songlen = %d", song.songlen);
	seqtable_offset = hio_read32b(f);
	hio_read32b(f);
	trackvols_offset = hio_read32b(f);
	song.numtracks = hio_read16b(f);
	song.numpseqs = hio_read16b(f);
	trackpans_offset = hio_read32b(f);
	song.flags3 = hio_read32b(f);
	song.voladj = hio_read16b(f);
	song.channels = hio_read16b(f);
	song.mix_echotype = hio_read8(f);
	song.mix_echodepth = hio_read8(f);
	song.mix_echolen = hio_read16b(f);
	song.mix_stereosep = hio_read8(f);

	hio_seek(f, 223, SEEK_CUR);

	song.deftempo = hio_read16b(f);
	song.playtransp = hio_read8(f);
	song.flags = hio_read8(f);
	song.flags2 = hio_read8(f);
	song.tempo2 = hio_read8(f);
	for (i = 0; i < 16; i++)
		hio_read8(f);		/* reserved */
	song.mastervol = hio_read8(f);
	song.numsamples = hio_read8(f);

	/* Sanity check */
	if (song.numsamples > 63) {
		D_(D_CRIT "invalid instrument count %d", song.numsamples);
		return -1;
	}

	/*
	 * read sequence
	 */
	hio_seek(f, start + seqtable_offset, SEEK_SET);
	playseq_offset = hio_read32b(f);
	hio_seek(f, start + playseq_offset, SEEK_SET);
	hio_seek(f, 32, SEEK_CUR);	/* skip name */
	hio_read32b(f);
	hio_read32b(f);
	mod->len = hio_read16b(f);

	/* Sanity check */
	if (mod->len > 255) {
		D_(D_CRIT "unsupported song length %d", mod->len);
		return -1;
	}

	for (i = 0; i < mod->len; i++) {
		mod->xxo[i] = hio_read16b(f);
	}

	/*
	 * convert header
	 */
	m->c4rate = C4_NTSC_RATE;
	m->quirk |= QUIRK_RTONCE; /* FF1 */
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
	mod->rst = 0;
	mod->chn = 0;
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
		hio_seek(f, start + smplarr[i], SEEK_SET);
		hio_read32b(f);				/* length */
		type = hio_read16b(f);
		if (type == -1 || type == -2) {		/* type is synth? */
			hio_seek(f, 14, SEEK_CUR);
			mod->smp += hio_read16b(f);	/* wforms */
		} else if (type >= 1 && type <= 6) {	/* octave samples */
			mod->smp += mmd_num_oct[type - 1];
		} else {
			mod->smp++;
		}
		if (hio_error(f)) {
			D_(D_CRIT "read error at sample %d", i);
			goto err_cleanup;
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
		hio_read32b(f);		/* annotxt */
		hio_read32b(f);		/* annolen */
		iinfo_offset = hio_read32b(f);
		D_(D_INFO "iinfo_offset = 0x%08x", iinfo_offset);
		expdata.i_ext_entries = hio_read16b(f);
		expdata.i_ext_entrsz = hio_read16b(f);

		/* Sanity check */
		if (expsmp_offset < 0 || iinfo_offset < 0) {
			D_(D_CRIT "invalid expdata");
			goto err_cleanup;
		}

		hio_read32b(f);
		hio_read32b(f);
		hio_read32b(f);
		hio_read32b(f);
		songname_offset = hio_read32b(f);
		D_(D_INFO "songname_offset = 0x%08x", songname_offset);
		expdata.songnamelen = hio_read32b(f);
		hio_read32b(f);		/* dumps */
		mmdinfo_offset = hio_read32b(f);

		if (hio_error(f)) {
			D_(D_CRIT "read error in expdata");
			goto err_cleanup;
		}

		hio_seek(f, start + songname_offset, SEEK_SET);
		D_(D_INFO "expdata.songnamelen = %d", expdata.songnamelen);
		for (i = 0; i < expdata.songnamelen; i++) {
			if (i >= XMP_NAME_SIZE)
				break;
			mod->name[i] = hio_read8(f);
		}

		if (mmdinfo_offset != 0) {
			D_(D_INFO "mmdinfo_offset = 0x%08x", mmdinfo_offset);
			hio_seek(f, start + mmdinfo_offset, SEEK_SET);
			mmd_info_text(f, m, mmdinfo_offset);
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

		hio_seek(f, start + blockarr[i], SEEK_SET);

		block.numtracks = hio_read16b(f);
		block.lines = hio_read16b(f);
		if (hio_error(f)) {
			D_(D_CRIT "read error at block %d", i);
			goto err_cleanup;
		}

		/* Sanity check--Amiga OctaMED files have an upper bound of 3200 lines per block,
		 * but MED Soundstudio for Windows allows up to 9999 lines.
		  */
		if (block.lines + 1 > 9999) {
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
	if (mod->chn <= 0 || mod->chn > XMP_MAX_CHANNELS) {
		D_(D_CRIT "invalid channel count %d", mod->chn);
		goto err_cleanup;
	}

	mod->trk = mod->pat * mod->chn;

	mmd_tracker_version(m, ver, 0, expdata_offset ? &expdata : NULL);

	MODULE_INFO();

	D_(D_INFO "BPM mode: %s (length = %d)", bpm_on ? "on" : "off", bpmlen);
	D_(D_INFO "Song transpose : %d", song.playtransp);
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

		hio_seek(f, start + blockarr[i], SEEK_SET);

		block.numtracks = hio_read16b(f);
		block.lines = hio_read16b(f);
		hio_read32b(f); /* FIXME: should try to load extra command pages when they exist. */

		size = block.numtracks * (block.lines + 1) * 4;
		if (hio_read(patbuf, 1, size, f) < size) {
			D_(D_CRIT "read error in block %d", i);
			goto err_cleanup;
		}

		if (libxmp_alloc_pattern_tracks_long(mod, i, block.lines + 1) < 0)
			goto err_cleanup;

		pos = patbuf;
		for (j = 0; j < mod->xxp[i]->rows; j++) {
			for (k = 0; k < block.numtracks; k++) {
				event = &EVENT(i, k, j);
				event->note = pos[0] & 0x7f;
				if (event->note) {
					event->note += 12 + song.playtransp;
				}

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
	}
	free(patbuf);
	patbuf = NULL;

	if (libxmp_med_new_module_extras(m) != 0)
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

			if (expdata.s_ext_entrsz >= 8) {	/* Octamed V5 */
				exp_smp[i].default_pitch = hio_read8(f);
				exp_smp[i].instr_flags = hio_read8(f);
				hio_read16b(f);
				skip -= 4;
			}
			if (expdata.s_ext_entrsz >= 10) {	/* OctaMED V5.02 */
				hio_read16b(f);
				skip -= 2;
			}
			if (expdata.s_ext_entrsz >= 18) {	/* OctaMED V7 */
				exp_smp[i].long_repeat = hio_read32b(f);
				exp_smp[i].long_replen = hio_read32b(f);
				skip -= 8;
			}

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

	hio_seek(f, start + trackvols_offset, SEEK_SET);
	for (i = 0; i < mod->chn; i++)
		mod->xxc[i].vol = hio_read8(f);;

	if (trackpans_offset) {
		hio_seek(f, start + trackpans_offset, SEEK_SET);
		for (i = 0; i < mod->chn; i++) {
			int p = 8 * hio_read8s(f);
			mod->xxc[i].pan = 0x80 + (p > 127 ? 127 : p);
		}
	} else {
		for (i = 0; i < mod->chn; i++)
			mod->xxc[i].pan = 0x80;
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
