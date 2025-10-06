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

#pragma once
#ifndef NOPL2_H
#define NOPL2_H

#ifdef __cplusplus
extern "C" {
#endif

void *nopl2_init(int clock, int samplerate);
void nopl2_set_rate(void *chip, int clock, int samplerate);
void nopl2_shutdown(void *chip);
void nopl2_reset(void *chip);

void nopl2_getsample_one_native(void *chip, short *sndptr);

void nopl2_write_buf(void *chip, unsigned short addr, unsigned char val);

#ifdef __cplusplus
}
#endif

#endif /* NOPL2_H */
