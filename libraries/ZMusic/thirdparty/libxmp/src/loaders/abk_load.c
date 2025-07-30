/* Extended Module Player
 * AMOS/STOS Music Bank Loader
 * Copyright (C) 2014 Stephen J Leary and Claudio Matsuoka
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
#include "../effects.h"
#include "../period.h"

#define AMOS_BANK 0x416d426b
#define AMOS_MUSIC_TYPE 0x0003
#define AMOS_MAIN_HEADER 0x14L
#define AMOS_STRING_LEN 0x10
#define AMOS_BASE_FREQ 8192
#define AMOS_ABK_CHANNELS 4
#define ABK_HEADER_SECTION_COUNT 3

static int abk_test (HIO_HANDLE *, char *, const int);
static int abk_load (struct module_data *, HIO_HANDLE *, const int);

const struct format_loader libxmp_loader_abk =
{
    "AMOS Music Bank",
    abk_test,
    abk_load
};

/**
 * @class abk_header
 * @brief represents the main ABK header.
 */
struct abk_header
{
    uint32	instruments_offset;
    uint32	songs_offset;
    uint32	patterns_offset;
};

/**
 * @class abk_song
 * @brief represents a song in an ABK module.
 */
struct abk_song
{
    uint32	playlist_offset[AMOS_ABK_CHANNELS];
    uint16	tempo;
    char   	song_name[AMOS_STRING_LEN];
};

/**
 * @class abk_playlist
 * @brief represents an ABK playlist.
 */
struct abk_playlist
{
    uint16	length;
    uint16	*pattern;
};

/**
 * @class abk_instrument
 * @brief represents an ABK instrument.
 */
struct abk_instrument
{
    uint32 sample_offset;
    uint32 sample_length;

    uint32 repeat_offset;
    uint16 repeat_end;

    uint16 sample_volume;

    char   sample_name[AMOS_STRING_LEN];
};


/**
 * @brief read the ABK playlist out from the file stream. This method malloc's some memory for the playlist
 * and can realloc if the playlist is very long.
 * @param f the stream to read from
 * @param playlist_offset the offset to the playlist sections.
 * @param playlist this structure is populated with the result.
 */
static void read_abk_playlist(HIO_HANDLE *f, uint32 playlist_offset, struct abk_playlist *playlist)
{
    uint16 playdata;
    int arraysize;

    arraysize = 64;

    /* clear the length */
    playlist->length = 0;

    /* move to the start of the songs data section. */
    hio_seek(f, playlist_offset, SEEK_SET);

    playlist->pattern = (uint16 *) malloc(arraysize * sizeof(uint16));

    playdata = hio_read16b(f);

    while((playdata != 0xFFFF) && (playdata != 0xFFFE))
    {
        /* i hate doing a realloc in a loop
           but i'd rather not traverse the list twice.*/

        if (playlist->length >= arraysize)
        {
            arraysize *= 2;
            playlist->pattern = (uint16 *) realloc(playlist->pattern , arraysize * sizeof(uint16));
        }

        playlist->pattern[playlist->length++] = playdata;
        playdata = hio_read16b(f);
    };
}

static int read_abk_song(HIO_HANDLE *f, struct abk_song *song, uint32 songs_section_offset)
{
    int i;
    uint32 song_section;

    /* move to the start of the songs data section */
    hio_seek(f, songs_section_offset, SEEK_SET);

    if (hio_read16b(f) != 1)
    {
        /* we only support a single song.
         * in a an abk file for now */
        return -1;
    }

    song_section = hio_read32b(f);

    if (hio_seek(f, songs_section_offset + song_section, SEEK_SET) < 0) {
        return -1;
    }

    for (i=0; i<AMOS_ABK_CHANNELS; i++)
    {
        song->playlist_offset[i] = hio_read16b(f) + songs_section_offset + song_section;
    }

    song->tempo = hio_read16b(f);

    /* unused. just progress the file pointer forward */
    (void) hio_read16b(f);

    if (hio_read(song->song_name, 1, AMOS_STRING_LEN, f) != AMOS_STRING_LEN) {
        return -1;
    }

    return 0;
}

/**
 * @brief reads an ABK pattern into a xmp_event structure.
 * @param f stream to read data from.
 * @param events events object to populate.
 * @param pattern_offset_abs the absolute file offset to the start of the patter to read.
 * @return returns the size of the pattern.
 */
static int read_abk_pattern(HIO_HANDLE *f, struct xmp_event *events, uint32 pattern_offset_abs)
{
    uint8 position;
    uint8 command;
    uint8 param;
    uint8 inst = 0;
    uint8 jumped = 0;
    uint8 per_command = 0;
    uint8 per_param = 0;

    uint16 delay;
    uint16 patdata;

    int storepos;
    if ((storepos = hio_tell(f)) < 0) {
        return -1;
    }

    /* count how many abk positions are used in this pattern */
    position = 0;

    hio_seek(f, pattern_offset_abs, SEEK_SET);

    /* read the first bit of pattern data */
    patdata = hio_read16b(f);

    while ((patdata != 0x8000) && (jumped == 0))
    {
        if (patdata == 0x9100)
        {
            break;
        }

        if (patdata & 0x8000)
        {
            command = (patdata >> 8) & 0x7F;
            param = patdata & 0x007F;

            if (command != 0x03 && command != 0x09 && command != 0x0b && command != 0x0c && command != 0x0d && command < 0x10) {
                per_command = 0;
                per_param = 0;
            }

            switch (command)
            {
            case 0x01:		/* portamento up */
            case 0x0e:
                events[position].fxt = FX_PORTA_UP;
                events[position].fxp = param;
                break;
            case 0x02:		/* portamento down */
            case 0x0f:
                events[position].fxt = FX_PORTA_DN;
                events[position].fxp = param;
                break;
            case 0x03:		/* set volume */
                events[position].fxt = FX_VOLSET;
                events[position].fxp = param;
                break;
            case 0x04:		/* stop effect */
                break;
            case 0x05:		/* repeat */
                events[position].fxt = FX_EXTENDED;
                if (param == 0) {
                    events[position].fxp = 0x50;
                } else {
                    events[position].fxp = 0x60 | (param & 0x0f);
                }
                break;
            case 0x06:		/* lowpass filter off */
                events[position].fxt = FX_EXTENDED;
                events[position].fxp = 0x00;
                break;
            case 0x07:		/* lowpass filter on */
                events[position].fxt = FX_EXTENDED;
                events[position].fxp = 0x01;
                break;
            case 0x08:		/* set tempo */
                if (param > 0) {
                    events[position].fxt = FX_SPEED;
                    events[position].fxp = 100/param;
                }
                break;
            case 0x09:		/* set instrument */
                inst = param + 1;
                break;
            case 0x0a:		/* arpeggio */
                per_command = FX_ARPEGGIO;
                per_param = param;
                break;
            case 0x0b:		/* tone portamento */
                per_command = FX_TONEPORTA;
                per_param = param;
                break;
            case 0x0c:		/* vibrato */
                per_command = FX_VIBRATO;
                per_param = param;
                break;
            case 0x0d:		/* volume slide */
		if (param != 0) {
                    per_command = FX_VOLSLIDE;
                    per_param = param;
                } else {
                    per_command = 0;
                    per_param = 0;
                }
                break;
            case 0x10:		/* delay */
                if (per_command != 0 || per_param != 0) {
                    int i;
                    for (i = 0; i < param && position < 64; i++) {
                        events[position].fxt = per_command;
                        events[position].fxp = per_param;
                        position++;
                    }
                } else {
		    position += param;
		}
		if (position >= 64) {
		    jumped = 1;
		}
                break;
            case 0x11:		/* position jump */
                events[position].fxt = FX_JUMP;
                events[position].fxp = param;
                /* break out of the loop because we've jumped.*/
                jumped = 1;
                break;
            default:
#if 0
                /* write out an error for any unprocessed commands.*/
                D_(D_WARN "ABK UNPROCESSED COMMAND: %x,%x\n", command, param);
                break;
#else
		return -1;
#endif
            }
        }
        else
        {
            if (patdata & 0x4000)
            {
                /* old note format.*/
                /* old note format is 2 x 2 byte words with bit 14 set on the first word */
                /* WORD 1: 0x4XDD | X = dont care, D = delay to apply after note. (Usually in 7FDD form).
                 * WORD 2: 0xXPPP | PPP = Amiga Period */

                delay = patdata & 0xff;
                patdata = hio_read16b(f);

                if (patdata == 0 && delay == 0)
                {
                    /* a zero note, with zero delay ends the pattern */
                    break;
                }

                if (patdata != 0)
                {
                    /* convert the note from amiga period format to xmp's internal format.*/
                    events[position].note = libxmp_period_to_note(patdata & 0x0fff);
                    events[position].ins = inst;
                }

                /* now add on the delay */
                position += delay;
		if (position >= 64) {
			break;
		}
            }
            else /* new note format */
            {
                /* convert the note from amiga period format to xmp's internal format.*/
                events[position].note = libxmp_period_to_note(patdata & 0x0fff);
                events[position].ins = inst;
            }
        }

        /* read the data for the next pass round the loop */
        patdata = hio_read16b(f);

        /* check for an EOF while reading */
        if (hio_eof(f))
        {
            break;
        }
    }
    if (position >= 1 && position < 64) {
	events[position - 1].f2t = FX_BREAK;
    }

    hio_seek(f, storepos, SEEK_SET);

    return position;
}

static struct abk_instrument* read_abk_insts(HIO_HANDLE *f, uint32 inst_section_size, int count)
{
    uint16 i;
    struct abk_instrument *inst;

    if (count < 1)
        return NULL;

    inst = (struct abk_instrument*) malloc(count * sizeof(struct abk_instrument));
    memset(inst, 0, count * sizeof(struct abk_instrument));

    for (i = 0; i < count; i++)
    {
        uint32 sampleLength;

        inst[i].sample_offset = hio_read32b(f);
        inst[i].repeat_offset = hio_read32b(f);
        inst[i].sample_length = hio_read16b(f);
        inst[i].repeat_end = hio_read16b(f);
        inst[i].sample_volume = hio_read16b(f);
        sampleLength = hio_read16b(f);

        /* detect a potential bug where the sample length is not specified (and we might already know the length) */
        if (sampleLength > 4)
        {
            inst[i].sample_length = sampleLength;
        }

        if (hio_read(inst[i].sample_name, 1, 16, f) != 16) {
            free(inst);
            return NULL;
	}
    }

    return inst;
}

static int abk_test(HIO_HANDLE *f, char *t, const int start)
{
    struct abk_song song;
    char music[8];
    uint32 song_section_offset;

    if (hio_read32b(f) != AMOS_BANK)
    {
        return -1;
    }

    if (hio_read16b(f) != AMOS_MUSIC_TYPE)
    {
        return -1;
    }

    /* skip over length and chip/fastmem.*/
    hio_seek(f, 6, SEEK_CUR);

    if (hio_read(music, 1, 8, f) != 8)	/* get the "Music   " */
        return -1;

    if (memcmp(music, "Music   ", 8))
    {
        return -1;
    }

    /* Attempt to read title. */
    hio_read32b(f); /* instruments_offset */
    song_section_offset = hio_read32b(f);

    if (t != NULL && read_abk_song(f, &song, AMOS_MAIN_HEADER + song_section_offset) == 0)
    {
        libxmp_copy_adjust(t, (uint8 *)song.song_name, AMOS_STRING_LEN);
    }

    return 0;
}

static int abk_load(struct module_data *m, HIO_HANDLE *f, const int start)
{
    int i,j,k;
    uint16 pattern;
    uint32 first_sample_offset;
    uint32 inst_section_size;

    struct xmp_module *mod = &m->mod;

    struct abk_header main_header;
    struct abk_instrument *ci;
    struct abk_song song;
    struct abk_playlist playlist;

    hio_seek(f, AMOS_MAIN_HEADER, SEEK_SET);

    main_header.instruments_offset = hio_read32b(f);
    main_header.songs_offset = hio_read32b(f);
    main_header.patterns_offset = hio_read32b(f);

    /* Sanity check */
    if (main_header.instruments_offset > 0x00100000 ||
        main_header.songs_offset > 0x00100000 ||
        main_header.patterns_offset > 0x00100000) {
            return -1;
    }

    inst_section_size = main_header.instruments_offset;
    D_(D_INFO "Sample Bytes: %d", inst_section_size);

    LOAD_INIT();

    libxmp_set_type(m, "AMOS Music Bank");

    if (read_abk_song(f, &song, AMOS_MAIN_HEADER + main_header.songs_offset) < 0)
    {
        return -1;
    }

    libxmp_copy_adjust(mod->name, (uint8*) song.song_name, AMOS_STRING_LEN);

    MODULE_INFO();

    hio_seek(f, AMOS_MAIN_HEADER + main_header.patterns_offset, SEEK_SET);

    mod->chn = AMOS_ABK_CHANNELS;
    mod->pat = hio_read16b(f);

    /* Sanity check */
    if (mod->pat > 256) {
        return -1;
    }

    mod->trk = mod->chn * mod->pat;

    /* move to the start of the instruments section. */
    hio_seek(f, AMOS_MAIN_HEADER + main_header.instruments_offset, SEEK_SET);
    mod->ins = hio_read16b(f);

    /* Sanity check */
    if (mod->ins > 255) {
	return -1;
    }

    mod->smp = mod->ins;

    /* Read and convert instruments and samples */

    if (libxmp_init_instrument(m) < 0)
    {
        return -1;
    }

    D_(D_INFO "Instruments: %d", mod->ins);

    /* read all the instruments in */
    ci = read_abk_insts(f, inst_section_size, mod->ins);
    if (ci == NULL) {
        return -1;
    }

    /* store the location of the first sample so we can read them later. */
    first_sample_offset = AMOS_MAIN_HEADER + main_header.instruments_offset + ci[0].sample_offset;

    for (i = 0; i < mod->ins; i++)
    {
        if (libxmp_alloc_subinstrument(mod, i, 1) < 0)
        {
            free(ci);
            return -1;
        }

        mod->xxs[i].len = ci[i].sample_length << 1;

        if (mod->xxs[i].len > 0)
        {
            mod->xxi[i].nsm = 1;
        }

        /* the repeating stuff. */
        if (ci[i].repeat_offset > ci[i].sample_offset)
        {
            mod->xxs[i].lps = (ci[i].repeat_offset - ci[i].sample_offset) << 1;
        }
        else
        {
            mod->xxs[i].lps = 0;
        }
        mod->xxs[i].lpe = ci[i].repeat_end;
        if (mod->xxs[i].lpe > 2) {
            mod->xxs[i].lpe <<= 1;
            mod->xxs[i].flg = XMP_SAMPLE_LOOP;
        }
/*printf("%02x lps=%04x lpe=%04x\n", i,  mod->xxs[i].lps, mod->xxs[i].lpe);*/

        mod->xxi[i].sub[0].vol = ci[i].sample_volume;
        mod->xxi[i].sub[0].pan = 0x80;
        mod->xxi[i].sub[0].sid = i;

        libxmp_instrument_name(mod, i, (uint8*)ci[i].sample_name, 16);

        D_(D_INFO "[%2X] %-14.14s %04x %04x %04x %c", i,
           mod->xxi[i].name, mod->xxs[i].len, mod->xxs[i].lps, mod->xxs[i].lpe,
           mod->xxs[i].flg & XMP_SAMPLE_LOOP ? 'L' : ' ');
    }

    free(ci);

    if (libxmp_init_pattern(mod) < 0)
    {
        return -1;
    }

    /* figure out the playlist order.
     * TODO: if the 4 channels arent in the same order then
     * we need to fail here. */

    read_abk_playlist(f, song.playlist_offset[0], &playlist);

    /* move to the start of the instruments section */
    /* then convert the patterns one at a time. there is a pattern for each channel.*/
    hio_seek(f, AMOS_MAIN_HEADER + main_header.patterns_offset + 2, SEEK_SET);

    mod->len = 0;

    i = 0;
    for (j = 0; j < mod->pat; j++)
    {
        if (libxmp_alloc_pattern_tracks(mod, i, 64) < 0)
        {
            free(playlist.pattern);
            return -1;
        }

        for (k = 0; k < mod->chn; k++)
        {
            pattern = hio_read16b(f);
            if (read_abk_pattern(f,  mod->xxt[(i*mod->chn)+k]->event, AMOS_MAIN_HEADER + main_header.patterns_offset + pattern) < 0) {
		free(playlist.pattern);
		return -1;
	    }
        }

        i++;
    }

    /* Sanity check */
    if (playlist.length > 256) {
    	free(playlist.pattern);
	return -1;
    }

    mod->len = playlist.length;

    /* now push all the patterns into the module and set the length */
    for (i = 0; i < playlist.length; i++)
    {
        mod->xxo[i] = playlist.pattern[i];
    }

    /* free up some memory here */
    free(playlist.pattern);

    D_(D_INFO "Stored patterns: %d", mod->pat);
    D_(D_INFO "Stored tracks: %d", mod->trk);

    /* Read samples */
    hio_seek(f, first_sample_offset, SEEK_SET);

    D_(D_INFO "Stored samples: %d", mod->smp);

    for (i = 0; i < mod->ins; i++)
    {
        if (mod->xxs[i].len <= 2)
            continue;

        if (libxmp_load_sample(m, f, 0, &mod->xxs[i], NULL) < 0)
        {
            return -1;
        }
    }

    return 0;
}
