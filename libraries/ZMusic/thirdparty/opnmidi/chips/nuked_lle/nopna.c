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

#include "fmopna_2608.h"
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "nopna.h"

#define OPN_WRITEBUF_SIZE   2048
#define OPN_WRITEBUF_DELAY  1
#define RSM_FRAC 10

typedef struct _opna_writebuf {
    uint64_t time;
    uint8_t port;
    uint8_t data;
} opna_writebuf;

typedef struct {
    fmopna_t chip;

    int32_t rateratio;
    int32_t samplecnt;
    int oldsample;

    int o_s;
    int o_sh1;
    int o_sh2;
    int dac_shifter;
    int dac_shifter2;
    short sample_l;
    short sample_r;

    uint64_t writebuf_samplecnt;
    uint32_t writebuf_cur;
    uint32_t writebuf_last;
    uint64_t writebuf_lasttime;
    opna_writebuf writebuf[OPN_WRITEBUF_SIZE];
} nopna_t;


void *nopna_init(unsigned int clock, unsigned int samplerate)
{
    nopna_t *chip = calloc(1, sizeof(nopna_t));
    nopna_set_rate(chip, clock, samplerate);
    return chip;
}

void nopna_set_rate(void *chip_p, unsigned int clock, unsigned int samplerate)
{
    uint32_t rateratio;
    nopna_t *chip = (nopna_t*)chip_p;
    rateratio = chip->rateratio;

    if (samplerate != 0)
        chip->rateratio = (int32_t)((((int64_t)144 * samplerate) << RSM_FRAC) / clock);
    else
        chip->rateratio = rateratio;

    nopna_reset(chip);
}

void nopna_shutdown(void *chip)
{
    free(chip);
}

void nopna_reset(void *chip_p)
{
    nopna_t *chip = (nopna_t*)chip_p;
    int i = 0;
    memset(&chip->chip, 0, sizeof(fmopna_t));

    chip->o_s = 0;
    chip->o_sh1 = 0;
    chip->o_sh2 = 0;
    chip->dac_shifter = 0;
    chip->dac_shifter2 = 0;
    chip->sample_l = 0;
    chip->sample_r = 0;

    chip->chip.input.cs = 0;
    chip->chip.input.cs = 0;
    chip->chip.input.rd = 1;
    chip->chip.input.wr = 1;
    chip->chip.input.a0 = 0;
    chip->chip.input.a1 = 0;
    chip->chip.input.data = 0;
    chip->chip.input.ic = 1;
    chip->chip.input.test = 1;

    for(i = 0; i < 288; ++i)
    {
        FMOPNA_Clock(&chip->chip, 0);
        FMOPNA_Clock(&chip->chip, 1);
    }

    chip->chip.input.ic = 0;
    for (; i < 288 * 2; i++)
    {
        FMOPNA_Clock(&chip->chip, 0);
        FMOPNA_Clock(&chip->chip, 1);
    }
    chip->chip.input.ic = 1;

    for (; i < 288 * 2; i++)
    {
        FMOPNA_Clock(&chip->chip, 0);
        FMOPNA_Clock(&chip->chip, 1);
    }
}

void nopna_getsample_one_native(void *chip_p, short *sndptr)
{
    nopna_t *chip = (nopna_t*)chip_p;
    short *p = sndptr;
    int i;

    FMOPNA_Clock(&chip->chip, 1);

    while(chip->writebuf[chip->writebuf_cur].time <= chip->writebuf_samplecnt)
    {
        if (!(chip->writebuf[chip->writebuf_cur].port & 0x04))
            break;

        chip->writebuf[chip->writebuf_cur].port &= 0x03;
        nopna_write(chip, chip->writebuf[chip->writebuf_cur].port,
                   chip->writebuf[chip->writebuf_cur].data);
        chip->writebuf_cur = (chip->writebuf_cur + 1) % OPN_WRITEBUF_SIZE;
    }
    chip->writebuf_samplecnt++;

    for(i = 0; i < 144; i++)
    {
        FMOPNA_Clock(&chip->chip, 0);
        FMOPNA_Clock(&chip->chip, 1);

        if(chip->o_s && !chip->chip.o_s)
        {
            if(chip->o_sh1 && !chip->chip.o_sh1)
                chip->dac_shifter2 = chip->dac_shifter ^ 0x8000;

            if(chip->o_sh2 && !chip->chip.o_sh2)
                chip->dac_shifter2 = chip->dac_shifter ^ 0x8000;

            if(chip->o_sh1)
                chip->sample_l = chip->dac_shifter2;

            if(chip->o_sh2)
                chip->sample_r = chip->dac_shifter2;

            chip->dac_shifter = (chip->dac_shifter >> 1) | (chip->chip.o_opo << 15);

            chip->o_sh1 = chip->chip.o_sh1;
            chip->o_sh2 = chip->chip.o_sh2;
        }

        chip->o_s = chip->chip.o_s;
    }

    *p++ = chip->sample_l / 4;
    *p++ = chip->sample_r / 4;
}

void nopna_write(void *chip_p, int port, int val)
{
    nopna_t *chip = (nopna_t*)chip_p;
    chip->chip.input.a0 = port & 1;
    chip->chip.input.a1 = (port >> 1) & 1;
    chip->chip.input.data = val;
    chip->chip.input.wr = 0;
    FMOPNA_Clock(&chip->chip, 1);
    chip->chip.input.wr = 1;
}

void nopna_write_buffered(void *chip_p, int port, int val)
{
    nopna_t *chip = (nopna_t*)chip_p;
    uint64_t time1, time2;
    uint64_t skip;

    if (chip->writebuf[chip->writebuf_last].port & 0x04)
    {
        nopna_write(chip, chip->writebuf[chip->writebuf_last].port & 0X03,
                    chip->writebuf[chip->writebuf_last].data);

        chip->writebuf_cur = (chip->writebuf_last + 1) % OPN_WRITEBUF_SIZE;
        skip = chip->writebuf[chip->writebuf_last].time - chip->writebuf_samplecnt;
        chip->writebuf_samplecnt = chip->writebuf[chip->writebuf_last].time;
        while(skip--)
            FMOPNA_Clock(&chip->chip, 1);
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
