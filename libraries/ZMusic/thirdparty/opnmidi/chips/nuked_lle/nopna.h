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

#pragma once
#ifndef NOPNA_H
#define NOPNA_H

#ifdef __cplusplus
extern "C" {
#endif

void *nopna_init(unsigned clock, unsigned int samplerate);
void nopna_set_rate(void *chip, unsigned int clock, unsigned int samplerate);
void nopna_shutdown(void *chip);
void nopna_reset(void *chip);

void nopna_getsample_one_native(void *chip, short *sndptr);

void nopna_write(void *chip, int port, int val);
void nopna_write_buffered(void *chip, int port, int val);

#ifdef __cplusplus
}
#endif

#endif /* NOPNA_H */
