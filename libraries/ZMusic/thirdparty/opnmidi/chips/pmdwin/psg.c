/* FIXME: move ugly-ass legalese somewhere where it won't be seen
// by anyone other than lawyers. (/dev/null would be ideal but sadly
// we live in an imperfect world). */
/* Copyright (c) 2012/2013, Peter Barfuss
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. */

/* Quick, somewhat hacky PSG implementation. Seems to work fine in most cases.
// Known bugs: volume *may* still be off for a lot of samples. Importantly,
// waveform volume is too quiet but setting it at the correct volume makes
// the noise volume too loud and vice-versa. I *think* what I have currently
// is mostly correct (I'm basing this mostly on how good Strawberry Crisis
// sounds with the given settings), but it's possible that more fine-tuning
// is needed. Apart from that, this is probably the sketchiest part of all
// of my emulator code, but then again there's a bit-exact VHDL core of
// the YM2149F/AY-3-8910, so while I do want to make this as good
// as the code in opna.c, it's the lowest-priority of all of the code here.
// --bofh */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "op.h"
#include "psg.h"

/* ---------------------------------------------------------------------------
// ????
*/
int      EmitTable[0x20] = { -1, };
uint32_t enveloptable[16][64] = { { 0, }, };

/* ---------------------------------------------------------------------------
//  PSG reset to power-on defaults
*/
void PSGReset(PSG *psg)
{
    int i;
    for (i=0; i<14; i++)
        PSGSetReg(psg, i, 0);
    PSGSetReg(psg, 7, 0xff);
    PSGSetReg(psg, 14, 0xff);
    PSGSetReg(psg, 15, 0xff);
}

/* ---------------------------------------------------------------------------
// This code is strongly inspired by some random PSG emulator code I found,
// and is probably not the optimal way to define periods. It *is* at least
// among the fastest, given that it uses the hilarious hack of using the
// integer overflow on a 32-bit unsigned integer to compute ""moduli"".
*/
void PSGSetClock(PSG *psg, uint32_t clock, uint32_t rate)
{
    int tmp;
    psg->tperiodbase = (uint32_t)((1 << toneshift ) / 4.0f * clock / rate);
    psg->eperiodbase = (uint32_t)((1 << envshift  ) / 4.0f * clock / rate);

    tmp = ((psg->reg[0] + psg->reg[1] * 256) & 0xfff);
    psg->speriod[0] = tmp ? psg->tperiodbase / tmp : psg->tperiodbase;
    tmp = ((psg->reg[2] + psg->reg[3] * 256) & 0xfff);
    psg->speriod[1] = tmp ? psg->tperiodbase / tmp : psg->tperiodbase;
    tmp = ((psg->reg[4] + psg->reg[5] * 256) & 0xfff);
    psg->speriod[2] = tmp ? psg->tperiodbase / tmp : psg->tperiodbase;
    tmp = psg->reg[6] & 0x1f;
    psg->nperiod = tmp;
    tmp = ((psg->reg[11] + psg->reg[12] * 256) & 0xffff);
    psg->eperiod = tmp ? psg->eperiodbase / tmp : psg->eperiodbase * 2;
}

/* ---------------------------------------------------------------------------
// ????????????
*/
static uint8_t table3[4] = {  0,  1, -1,  0 };
void MakeEnvelopTable(void)
{
    /* 0 lo  1 up 2 down 3 hi */
    static uint8_t table1[16*2] =
    {
        2,0, 2,0, 2,0, 2,0, 1,0, 1,0, 1,0, 1,0,
        2,2, 2,0, 2,1, 2,3, 1,1, 1,3, 1,2, 1,0,
    };
    int i, j;

    if (!enveloptable[0][0]) {
        uint32_t *ptr = enveloptable[0];
        for (i=0; i<16*2; i++) {
            uint8_t v = ((table1[i] & 0x2) ? 31 : 0);
            for (j=0; j<32; j++) {
                *ptr++ = EmitTable[v];
                v += table3[table1[i]];
            }
        }
    }
}

/* ---------------------------------------------------------------------------
//  Sets the channel output mask for the PSG device.
//  c is a bitvector where the 3 LSBs are set to 0 to disable a given
//  PSG channel and 1 to enable it.
//  TODO: Possibly allow enabling tone/noise output for each channel independently?
*/
void PSGSetChannelMask(PSG *psg, int c)
{
    int i;
    psg->mask = c;
    for (i=0; i<3; i++)
        psg->olevel[i] = psg->mask & (1 << i) ? EmitTable[(psg->reg[8+i] & 15) * 2 + 1] : 0;
}

/* ---------------------------------------------------------------------------
//  PSG register set routine. Mostly just what you'd expect from reading the manual.
//  Fairly boring code overall. regnum can be 0 - 15, data can be 0x00 - 0xFF.
//  (This should not be surprising - the YM2149F *did* use an 8-bit bus, after all).
//  Interesting quirk: the task of register 7 (channel enable/disable) is basically
//  entirely duplicated by other registers, to the point where you can basically
//  just ignore any writes to register 7 entirely. I save it here in case some
//  braindead routine wants to read its value and do something based on that
//  (Another curiosity: register 7 on the PSG appears to be the only register
//   between *both* the OPNA and the PSG which is actually *read from* by
//   pmdwin.cpp and not just written to. Amusingly enough, the only reason
//   that it is ever read is so that it can then OR something with what it just read
//   and then write that back to register 7. Hilarity).
//  HACK ALERT: The output levels for channels 0 and 1 are increased by a factor of 4
//  to make them match the actual chip in loudness, but without causing the noise channel
//  to overtake everything in intensity. This is almost certainly wrong, and moreover
//  it assumes that channel 2 will be playing back Speak Board effects which usually means
//  drum kit only (for the most part, at least), and not being used as a separate tonal
//  channel in its own right. To the best of my knowledge, this does hold for all of ZUN's
//  songs, however, once you step outside that set of music, it's trivial to find
//  all sorts of counterexamples to that assumption. Therefore, this should be fixed ASAP.
*/
void PSGSetReg(PSG *psg, uint8_t regnum, uint8_t data)
{
    if (regnum < 0x10)
    {
        psg->reg[regnum] = data;
        switch (regnum)
        {
            int tmp;

        case 0:     /* ChA Fine Tune */
        case 1:     /* ChA Coarse Tune */
            tmp = ((psg->reg[0] + psg->reg[1] * 256) & 0xfff);
            psg->speriod[0] = tmp ? psg->tperiodbase / tmp : psg->tperiodbase;
            break;

        case 2:     /* ChB Fine Tune */
        case 3:     /* ChB Coarse Tune */
            tmp = ((psg->reg[2] + psg->reg[3] * 256) & 0xfff);
            psg->speriod[1] = tmp ? psg->tperiodbase / tmp : psg->tperiodbase;
            break;

        case 4:     /* ChC Fine Tune */
        case 5:     /* ChC Coarse Tune */
            tmp = ((psg->reg[4] + psg->reg[5] * 256) & 0xfff);
            psg->speriod[2] = tmp ? psg->tperiodbase / tmp : psg->tperiodbase;
            break;

        case 6:     /* Noise generator control */
            data &= 0x1f;
            psg->nperiod = data;
            break;

        case 8:
            psg->olevel[0] = psg->mask & 1 ? EmitTable[(data & 15) * 2 + 1] : 0;
            break;

        case 9:
            psg->olevel[1] = psg->mask & 2 ? EmitTable[(data & 15) * 2 + 1] : 0;
            break;

        case 10:
            psg->olevel[2] = psg->mask & 4 ? EmitTable[(data & 15) * 2 + 1] : 0;
            break;

        case 11:    /* Envelope period */
        case 12:
            tmp = ((psg->reg[11] + psg->reg[12] * 256) & 0xffff);
            psg->eperiod = tmp ? psg->eperiodbase / tmp : psg->eperiodbase * 2;
            break;

        case 13:    /* Envelope shape */
            psg->ecount = 0;
            psg->envelop = enveloptable[data & 15];
            break;
        }
    }
}

/* ---------------------------------------------------------------------------
// Init code. Set volume to 0, reset the chip, enable all channels, seed the RNG.
// RNG seed lifted from MAME's YM2149F emulation routine, appears to be correct.
*/
void PSGInit(PSG *psg)
{
    int i;
    float base = 0x4000 / 3.0f;
    for (i=31; i>=2; i--)
    {
        EmitTable[i] = lrintf(base);
        base *= 0.840896415f; /* 1.0f / 1.189207115f */
    }
    EmitTable[1] = 0;
    EmitTable[0] = 0;
    MakeEnvelopTable();

    PSGSetChannelMask(psg, psg->mask);
    psg->rng = 14231;
    psg->ncount = 0;
    PSGReset(psg);
    psg->mask = 0x3f;
}

/* ---------------------------------------------------------------------------
// The main output routine for the PSG emulation.
// dest should be an array of size nsamples, and of type Sample
// (one of int16_t, int32_t or float - any will work here without causing
//  clipping/precision problems).
// Everything is implemented using some form of fixed-point arithmetic
// that currently needs no more than 32-bits for its implementation,
// but I'm pretty certain that you can get by with much less than that
// and still have mostly correct-to-fully-correct emulation.
//
// TODO: In the future, test the veracity of the above statement. Moreover,
// if it turns out to be correct, rewrite this routine to not use more than
// the required precision. This is irrelevant for any PC newer than, well,
// a 386DX/68040, but important for efficient hardware implementation.
*/
void PSGMix(PSG *psg, int32_t *dest, uint32_t nsamples)
{
    uint8_t chenable[3];
    uint8_t r7 = ~psg->reg[7];
    unsigned int i, k = 0;
    int x, y, z;

    if ((r7 & 0x3f) | ((psg->reg[8] | psg->reg[9] | psg->reg[10]) & 0x1f)) {
        int noise, sample;
        uint32_t env;
        uint32_t* p1;
        uint32_t* p2;
        uint32_t* p3;

        chenable[0] = (r7 & 0x01) && (psg->speriod[0] <= (1 << toneshift));
        chenable[1] = (r7 & 0x02) && (psg->speriod[1] <= (1 << toneshift));
        chenable[2] = (r7 & 0x04) && (psg->speriod[2] <= (1 << toneshift));

        p1 = ((psg->mask & 1) && (psg->reg[ 8] & 0x10)) ? &env : &psg->olevel[0];
        p2 = ((psg->mask & 2) && (psg->reg[ 9] & 0x10)) ? &env : &psg->olevel[1];
        p3 = ((psg->mask & 4) && (psg->reg[10] & 0x10)) ? &env : &psg->olevel[2];
        #define SCOUNT(ch)  (psg->scount[ch] >> toneshift)

        if (p1 != &env && p2 != &env && p3 != &env) {
            for (i=0; i<nsamples; i++) {
                psg->ncount++;
                if(psg->ncount >= psg->nperiod) {
                    if(psg->rng & 1)
                        psg->rng ^= 0x24000;
                    psg->rng >>= 1;
                    psg->ncount = 0;
                }
                noise = (psg->rng & 1);
                sample = 0;
                {
                    x = ((SCOUNT(0) & chenable[0]) | ((r7 >> 3) & noise)) - 1;     /* 0 or -1 */
                    sample += (psg->olevel[0] + x) ^ x;
                    psg->scount[0] += psg->speriod[0];
                    y = ((SCOUNT(1) & chenable[1]) | ((r7 >> 4) & noise)) - 1;
                    sample += (psg->olevel[1] + y) ^ y;
                    psg->scount[1] += psg->speriod[1];
                    /*z = ((SCOUNT(2) & chenable[2]) | ((r7 >> 5) & noise)) - 1;*/
                    z = ((r7 >> 5) & noise) - 1;
                    sample += (psg->olevel[2] + z) ^ z;
                    psg->scount[2] += psg->speriod[2];
                }
                sample = Limit16(sample);
                dest[k++] += sample;
                dest[k++] += sample;
            }

            psg->ecount = (psg->ecount >> 8) + (psg->eperiod >> 8) * nsamples;
            if (psg->ecount >= (1 << (envshift+6-8))) {
                if ((psg->reg[0x0d] & 0x0b) != 0x0a)
                    psg->ecount |= (1 << (envshift+5-8));
                psg->ecount &= (1 << (envshift+6-8)) - 1;
            }
            psg->ecount <<= 8;
        } else {
            for (i=0; i<nsamples; i++) {
                psg->ncount++;
                if(psg->ncount >= psg->nperiod) {
                    if(psg->rng & 1)
                        psg->rng ^= 0x24000;
                    psg->rng >>= 1;
                    psg->ncount = 0;
                }
                noise = (psg->rng & 1);
                sample = 0;
                {
                    env = psg->envelop[psg->ecount >> envshift];
                    psg->ecount += psg->eperiod;
                    if (psg->ecount >= (1 << (envshift+6))) {
                        if ((psg->reg[0x0d] & 0x0b) != 0x0a)
                            psg->ecount |= (1 << (envshift+5));
                        psg->ecount &= (1 << (envshift+6)) - 1;
                    }
                    x = ((SCOUNT(0) & chenable[0]) | ((r7 >> 3) & noise)) - 1;     /* 0 or -1 */
                    sample += (*p1 + x) ^ x;
                    psg->scount[0] += psg->speriod[0];
                    y = ((SCOUNT(1) & chenable[1]) | ((r7 >> 4) & noise)) - 1;
                    sample += (*p2 + y) ^ y;
                    psg->scount[1] += psg->speriod[1];
                    /*z = ((SCOUNT(2) & chenable[2]) | ((r7 >> 5) & noise)) - 1;*/
                    z = ((r7 >> 5) & noise) - 1;
                    sample += (*p3 + z) ^ z;
                    psg->scount[2] += psg->speriod[2];
                }
                sample = Limit16(sample);
                dest[k++] += sample;
                dest[k++] += sample;
            }
        }
    }
}

