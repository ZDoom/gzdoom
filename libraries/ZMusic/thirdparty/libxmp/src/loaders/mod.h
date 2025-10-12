/* Extended Module Player
 * Copyright (C) 1996-2021 Claudio Matsuoka and Hipolito Carraro Jr
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

#ifndef LIBXMP_LOADERS_MOD_H
#define LIBXMP_LOADERS_MOD_H

struct mod_instrument {
	uint8 name[22];		/* Instrument name */
	uint16 size;		/* Sample length in 16-bit words */
	int8 finetune;		/* Finetune (signed nibble) */
	int8 volume;		/* Linear playback volume */
	uint16 loop_start;	/* Loop start in 16-bit words */
	uint16 loop_size;	/* Loop length in 16-bit words */
};

struct mod_header {
	uint8 name[20];
	struct mod_instrument ins[31];
	uint8 len;
	uint8 restart;		/* Number of patterns in Soundtracker,
				 * Restart in Noisetracker/Startrekker,
				 * 0x7F in Protracker
				 */
	uint8 order[128];
	uint8 magic[4];
};

#ifndef LIBXMP_CORE_PLAYER
/* Soundtracker 15-instrument module header */

struct st_header {
	uint8 name[20];
	struct mod_instrument ins[15];
	uint8 len;
	uint8 restart;
	uint8 order[128];
};
#endif

#endif  /* LIBXMP_LOADERS_MOD_H */
