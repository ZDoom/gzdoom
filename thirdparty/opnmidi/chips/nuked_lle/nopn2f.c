/*
 * Copyright (C) 2023-2024 nukeykt
 *
 * This file is part of YM2608-LLE.
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
 *  YM2608/YM2610/YM2612 emulator.
 *  Thanks:
 *      Raki (ika-musume):
 *          YM2608B decap
 *      John McMaster (siliconpr0n.org):
 *          YM2610 decap
 *      HardWareMan:
 *          YM2612 decap
 *
 */

#include "fmopn2.h"
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "nopn2f.h"

#define OPN_WRITEBUF_SIZE   2048
#define OPN_WRITEBUF_DELAY  1
#define RSM_FRAC 10

typedef struct _opn2_writebuf {
    uint64_t time;
    uint8_t port;
    uint8_t data;
} opn2_writebuf;

typedef struct {
    fmopn2_t chip;

    int32_t rateratio;
    int32_t samplecnt;
    int oldsample;

    int o_lro;
    int o_bco;
    int dac_shifter;
    short sample_l;
    short sample_r;

    int ymf276;

    uint64_t writebuf_samplecnt;
    uint32_t writebuf_cur;
    uint32_t writebuf_last;
    uint64_t writebuf_lasttime;
    opn2_writebuf writebuf[OPN_WRITEBUF_SIZE];
} nopn2f_t;


void *nopn2f_init(unsigned int clock, unsigned int samplerate, int ymf276_mode)
{
    nopn2f_t *chip = calloc(1, sizeof(nopn2f_t));
    chip->ymf276 = ymf276_mode;
    nopn2f_set_rate(chip, clock, samplerate);
    return chip;
}

void nopn2f_set_rate(void *chip_p, unsigned int clock, unsigned int samplerate)
{
    uint32_t rateratio;
    nopn2f_t *chip = (nopn2f_t*)chip_p;
    rateratio = chip->rateratio;

    if (samplerate != 0)
        chip->rateratio = (int32_t)((((int64_t)144 * samplerate) << RSM_FRAC) / clock);
    else
        chip->rateratio = rateratio;

    nopn2f_reset(chip);
}

void nopn2f_shutdown(void *chip)
{
    free(chip);
}

void nopn2f_reset(void *chip_p)
{
    nopn2f_t *chip = (nopn2f_t*)chip_p;
    int i = 0;
    memset(&chip->chip, 0, sizeof(fmopn2_t));

    chip->chip.input.cs = 1;
    chip->chip.input.rd = 0;
    chip->chip.input.wr = 0;
    chip->chip.input.address = 0;
    chip->chip.input.data = 0;
    chip->chip.input.ic = 0;
    chip->chip.flags = chip->ymf276 ? 0 : fmopn2_flags_ym3438;

    for(i = 0; i < 288; ++i)
    {
        FMOPN2_Clock(&chip->chip, 0);
        FMOPN2_Clock(&chip->chip, 1);
    }

    chip->chip.input.ic = 1;
    for(i = 0; i < 288; ++i)
    {
        FMOPN2_Clock(&chip->chip, 0);
        FMOPN2_Clock(&chip->chip, 1);
    }

    chip->chip.input.ic = 0;
    for(i = 0; i < 288; ++i)
    {
        FMOPN2_Clock(&chip->chip, 0);
        FMOPN2_Clock(&chip->chip, 1);
    }
}

void nopn2f_getsample_one_native(void *chip_p, short *sndptr)
{
    nopn2f_t *chip = (nopn2f_t*)chip_p;
    short *p = sndptr;
    int suml, sumr, i;

    FMOPN2_Clock(&chip->chip, 1);

    while(chip->writebuf[chip->writebuf_cur].time <= chip->writebuf_samplecnt)
    {
        if (!(chip->writebuf[chip->writebuf_cur].port & 0x04))
            break;

        chip->writebuf[chip->writebuf_cur].port &= 0x03;
        nopn2f_write(chip, chip->writebuf[chip->writebuf_cur].port,
                   chip->writebuf[chip->writebuf_cur].data);
        chip->writebuf_cur = (chip->writebuf_cur + 1) % OPN_WRITEBUF_SIZE;
    }
    chip->writebuf_samplecnt++;

    suml = 0;
    sumr = 0;

    for(i = 0; i < 144; i++)
    {
        FMOPN2_Clock(&chip->chip, 0);
        suml += chip->chip.out_l;
        sumr += chip->chip.out_r;
        FMOPN2_Clock(&chip->chip, 1);
        suml += chip->chip.out_l;
        sumr += chip->chip.out_r;

        if(chip->ymf276)
        {
            if(!chip->o_bco && chip->chip.o_bco)
            {
                if(chip->o_lro != chip->chip.o_lro)
                {
                    if(chip->o_lro)
                        chip->sample_l = chip->dac_shifter;
                    else
                        chip->sample_r = chip->dac_shifter;
                }

                chip->dac_shifter = (chip->dac_shifter << 1) | chip->chip.o_so;
                chip->o_lro = chip->chip.o_lro;
            }

            chip->o_bco = chip->chip.o_bco;
        }
    }

    if(chip->ymf276)
    {
        *p++ = chip->sample_l / 4;
        *p++ = chip->sample_r / 4;
    }
    else
    {
        suml /= 12;
        sumr /= 12;

        *p++ = suml;
        *p++ = sumr;
    }
}

void nopn2f_write(void *chip_p, int port, int val)
{
    nopn2f_t *chip = (nopn2f_t*)chip_p;
    chip->chip.input.address = port & 3;
    chip->chip.input.data = val;
    chip->chip.input.wr = 0;
    FMOPN2_Clock(&chip->chip, 1);
    chip->chip.input.wr = 1;
}

void nopn2f_write_buffered(void *chip_p, int port, int val)
{
    nopn2f_t *chip = (nopn2f_t*)chip_p;
    uint64_t time1, time2;
    uint64_t skip;

    if (chip->writebuf[chip->writebuf_last].port & 0x04)
    {
        nopn2f_write(chip, chip->writebuf[chip->writebuf_last].port & 0X03,
                    chip->writebuf[chip->writebuf_last].data);

        chip->writebuf_cur = (chip->writebuf_last + 1) % OPN_WRITEBUF_SIZE;
        skip = chip->writebuf[chip->writebuf_last].time - chip->writebuf_samplecnt;
        chip->writebuf_samplecnt = chip->writebuf[chip->writebuf_last].time;
        while(skip--)
            FMOPN2_Clock(&chip->chip, 1);
    }

    chip->writebuf[chip->writebuf_last].port = (port & 0x03) | 0x04;
    chip->writebuf[chip->writebuf_last].data = val;
    time1 = chip->writebuf_lasttime + OPN_WRITEBUF_DELAY;
    time2 = chip->writebuf_samplecnt;

    if(time1 < time2)
        time1 = time2;

    chip->writebuf[chip->writebuf_last].time = time1;
    chip->writebuf_lasttime = time1;
    chip->writebuf_last = (chip->writebuf_last + 1) % OPN_WRITEBUF_SIZE;
}
