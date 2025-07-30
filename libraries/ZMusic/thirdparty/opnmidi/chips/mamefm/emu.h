/*
 * libOPNMIDI is a free Software MIDI synthesizer library with OPN2 (YM2612) emulation
 *
 * MIDI parser and player (Original code from ADLMIDI): Copyright (c) 2010-2014 Joel Yliluoma <bisqwit@iki.fi>
 * OPNMIDI Library and YM2612 support:   Copyright (c) 2017-2025 Vitaly Novichkov <admin@wohlnet.ru>
 *
 * Library is based on the ADLMIDI, a MIDI player for Linux and Windows with OPL3 emulation:
 * http://iki.fi/bisqwit/source/adlmidi.html
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef OPNMIDI_MAMEFM_EMU_H
#define OPNMIDI_MAMEFM_EMU_H

#include <new>
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <assert.h>

#if !defined(M_PI)
#   define M_PI 3.14159265358979323846
#endif

#if __cplusplus <= 199711L
#define CONSTEXPR
#else
#define CONSTEXPR constexpr
#endif

#if __cplusplus <= 199711L
#define NULLPTR NULL
#else
#define NULLPTR nullptr
#endif

typedef uint32_t offs_t;

inline void vlogerror(const char *fmt, va_list ap)
{
#if defined(OPNMIDI_MAMEFM_EMU_VERBOSE)
    vfprintf(stderr, fmt, ap);
#else
    (void)fmt;
    (void)ap;
#endif
}

inline void logerror(const char *fmt, ...)
{
#if defined(OPNMIDI_MAMEFM_EMU_VERBOSE)
    va_list ap;
    va_start(ap, fmt);
    vlogerror(fmt, ap);
    va_end(ap);
#else
    (void)fmt;
#endif
}

struct device_t
{
    void logerror(const char *fmt, ...);
    void vlogerror(const char *fmt, va_list ap);
};

inline void device_t::logerror(const char *fmt, ...)
{
#if defined(OPNMIDI_MAMEFM_EMU_VERBOSE)
    va_list ap;
    va_start(ap, fmt);
    vlogerror(fmt, ap);
    va_end(ap);
#else
    (void)fmt;
#endif
}

inline void device_t::vlogerror(const char *fmt, va_list ap)
{
#if defined(OPNMIDI_MAMEFM_EMU_VERBOSE)
    ::vlogerror(fmt, ap);
#else
    (void)fmt;
    (void)ap;
#endif
}

#endif
