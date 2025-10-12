/*
 * Copyright (C) 2023 nukeykt
 *
 * This file is part of YM3812-LLE.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 *  YM3812 emulator
 *  Thanks:
 *      Travis Goodspeed:
 *          YM3812 decap and die shot
 *
 */

#include "nuked_fmopl2.h"
#include <stdint.h>
#include <stdlib.h>
#include "nopl2.h"

#define OPL_WRITEBUF_SIZE   2048
#define OPL_WRITEBUF_DELAY  1
#define RSM_FRAC 10

typedef struct _opl2_writebuf {
    uint8_t reg;
    uint8_t data;
    uint8_t reg_2;
    uint8_t data_2;
} opl2_writebuf;

typedef struct {
    fmopl2_t chip;

    int sample;
    int o_sy;
    int shifter;
    int o_sh;

    int32_t rateratio;

    uint32_t writebuf_cur;
    uint32_t writebuf_last;
    uint64_t writebuf_lasttime;
    opl2_writebuf writebuf[OPL_WRITEBUF_SIZE];
    int32_t  writebuf_size;
} nopl2_t;

static void nopl2_cycle(nopl2_t *chip)
{
    int i, mant, shift;

    for (i = 0; i < 144; i++)
    {
        chip->chip.input.mclk = i & 1;
        FMOPL2_Clock(&chip->chip);

        if (!chip->o_sy && chip->chip.o_sy)
        {

            if (chip->o_sh && !chip->chip.o_sh)
            {
                mant = chip->shifter & 0x3ff;
                mant -= 512;
                shift = (chip->shifter >> 10) & 7;

                chip->sample = 0;
                if (shift)
                {
                    shift--;
                    chip->sample = mant << shift;
                }
            }
            chip->shifter = (chip->shifter >> 1) | (chip->chip.o_mo << 12);

            chip->o_sh = chip->chip.o_sh;
        }

        chip->o_sy = chip->chip.o_sy;
    }
}

void *nopl2_init(int clock, int samplerate)
{
    nopl2_t *chip = calloc(1, sizeof(nopl2_t));
    nopl2_set_rate(chip, clock, samplerate);
    return chip;
}

void nopl2_set_rate(void* chip_p, int clock, int samplerate)
{
    nopl2_t *chip = (nopl2_t*)chip_p;

    chip->chip.input.cs = 0;
    chip->chip.input.ic = 1;
    chip->chip.input.rd = 1;
    chip->chip.input.wr = 1;

    samplerate *= 1;

    chip->rateratio = ((samplerate << RSM_FRAC) * (int64_t)72) / clock;

    /* printf("%i %i\n", clock, samplerate); */

    nopl2_reset(chip);
}

void nopl2_shutdown(void *chip)
{
    free(chip);
}

void nopl2_reset(void *chip)
{
    nopl2_t* chip2 = chip;
    int i = 0;

    chip2->chip.input.ic = 0;
    for (i = 0; i < 100; i++)
        nopl2_cycle(chip2);

    chip2->chip.input.ic = 1;
    for (i = 0; i < 100; i++)
        nopl2_cycle(chip2);
}

void nopl2_write2(nopl2_t *chip, int port, int val)
{
    chip->chip.input.address = port;
    chip->chip.input.data_i = val;
    chip->chip.input.wr = 0;
    FMOPL2_Clock(&chip->chip); /* propagate */
    chip->chip.input.wr = 1;
    FMOPL2_Clock(&chip->chip); /* propagate */
}

void nopl2_getsample_one_native(void *chip, short *sndptr)
{
    nopl2_t* chip2 = chip;
    short *p = sndptr;
    opl2_writebuf* writebuf;

    if (chip2->writebuf_size > 0)
    {
        writebuf = &chip2->writebuf[chip2->writebuf_cur];

        if(writebuf->reg & 2)
        {
            writebuf->reg &= 1;
            nopl2_write2(chip2, writebuf->reg, writebuf->data);
            nopl2_cycle(chip2);
        }
        else
        {
            writebuf->reg_2 &= 1;
            nopl2_write2(chip2, writebuf->reg_2, writebuf->data_2);
            nopl2_cycle(chip2);

            chip2->writebuf_cur = (chip2->writebuf_cur + 1) % OPL_WRITEBUF_SIZE;
            --chip2->writebuf_size;
        }
    }
    else
        nopl2_cycle(chip2);

    *p++ = chip2->sample;
    *p++ = chip2->sample;
}

void nopl2_write_buf(void *chip, unsigned short addr, unsigned char val)
{
    nopl2_t* chip2 = chip;
    opl2_writebuf *writebuf;
    uint32_t writebuf_last;

    writebuf_last = chip2->writebuf_last;
    writebuf = &chip2->writebuf[writebuf_last];

    if (writebuf->reg & 2)
    {
        nopl2_write2(chip2, writebuf->reg & 1, writebuf->data);
        nopl2_cycle(chip2);

        nopl2_write2(chip2, writebuf->reg_2 & 1, writebuf->data_2);
        nopl2_cycle(chip2);

        chip2->writebuf_cur = (writebuf_last + 1) % OPL_WRITEBUF_SIZE;
        --chip2->writebuf_size;
    }

    writebuf->reg = (2 * ((addr >> 8) & 3)) | 2;
    writebuf->data = addr & 0xFF;
    writebuf->reg_2 = 1 | 2;
    writebuf->data_2 = val;

    chip2->writebuf_last = (writebuf_last + 1) % OPL_WRITEBUF_SIZE;
    ++chip2->writebuf_size;
}
