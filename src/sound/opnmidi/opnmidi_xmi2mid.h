/*
 * XMIDI: Miles XMIDI to MID Library Header
 *
 * Copyright (C) 2001  Ryan Nunn
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

/* XMIDI Converter */

#ifndef XMIDILIB_H
#define XMIDILIB_H

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

/* Conversion types for Midi files */
#define XMIDI_CONVERT_NOCONVERSION      0x00
#define XMIDI_CONVERT_MT32_TO_GM        0x01
#define XMIDI_CONVERT_MT32_TO_GS        0x02
#define XMIDI_CONVERT_MT32_TO_GS127     0x03 /* This one is broken, don't use */
#define XMIDI_CONVERT_MT32_TO_GS127DRUM 0x04 /* This one is broken, don't use */
#define XMIDI_CONVERT_GS127_TO_GS       0x05

int OpnMidi_xmi2midi(uint8_t *in, uint32_t insize,
                     uint8_t **out, uint32_t *outsize,
                     uint32_t convert_type);

#ifdef __cplusplus
}
#endif

#endif /* XMIDILIB_H */
