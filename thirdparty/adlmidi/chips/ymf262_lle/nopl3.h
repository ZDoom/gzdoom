/*
 * Copyright (C) 2023 nukeykt
 *
 * This file is part of YMF262-LLE.
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
 *  YMF262 emulator
 *  Thanks:
 *      John McMaster (siliconpr0n.org):
 *          YMF262 decap and die shot
 *
 */

#pragma once
#ifndef NOPL3_H
#define NOPL3_H

#ifdef __cplusplus
extern "C" {
#endif

void *nopl3_init(int clock, int samplerate);
void nopl3_set_rate(void *chip, int clock, int samplerate);
void nopl3_shutdown(void *chip);
void nopl3_reset(void *chip);

void nopl3_getsample(void *chip, short *sndptr, int numsamples);
void nopl3_getsample_one_native(void *chip, short *sndptr);

void nopl3_write(void *chip, int port, int val);

#ifdef __cplusplus
}
#endif

#endif /* NOPL3_H */
