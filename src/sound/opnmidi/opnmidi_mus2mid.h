/*
 * MUS2MIDI: DMX (DOOM) MUS to MIDI Library Header
 *
 * Copyright (C) 2014-2016  Bret Curtis
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#ifndef MUSLIB_H
#define MUSLIB_H

#include <stdint.h>

#ifdef __DJGPP__
typedef signed char     int8_t;
typedef unsigned char   uint8_t;
typedef signed short    int16_t;
typedef unsigned short  uint16_t;
typedef signed long     int32_t;
typedef unsigned long   uint32_t;
#endif

#ifdef __cplusplus
extern "C"
{
#endif

int OpnMidi_mus2midi(uint8_t *in, uint32_t insize,
                     uint8_t **out, uint32_t *outsize,
                     uint16_t frequency);

#ifdef __cplusplus
}
#endif

#endif /* MUSLIB_H */
