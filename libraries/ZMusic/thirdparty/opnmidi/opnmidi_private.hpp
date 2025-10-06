/*
 * libADLMIDI is a free Software MIDI synthesizer library with OPL3 emulation
 *
 * Original ADLMIDI code: Copyright (c) 2010-2014 Joel Yliluoma <bisqwit@iki.fi>
 * ADLMIDI Library API:   Copyright (c) 2017-2025 Vitaly Novichkov <admin@wohlnet.ru>
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

#ifndef ADLMIDI_PRIVATE_HPP
#define ADLMIDI_PRIVATE_HPP

#define OPNMIDI_UNSTABLE_API

// Setup compiler defines useful for exporting required public API symbols in gme.cpp
#ifndef OPNMIDI_EXPORT
#   if defined (_WIN32) && defined(OPNMIDI_BUILD_DLL)
#       define OPNMIDI_EXPORT __declspec(dllexport)
#   elif defined (LIBOPNMIDI_VISIBILITY) && defined (__GNUC__)
#       define OPNMIDI_EXPORT __attribute__((visibility ("default")))
#   else
#       define OPNMIDI_EXPORT
#   endif
#endif

#ifdef _WIN32
#define NOMINMAX 1
#endif

#ifdef _WIN32
#   undef NO_OLDNAMES
#       include <stdint.h>
#   ifdef _MSC_VER
#       ifdef _WIN64
typedef __int64 ssize_t;
#       else
typedef __int32 ssize_t;
#       endif
#   else
#       ifdef _WIN64
typedef int64_t ssize_t;
#       else
typedef int32_t ssize_t;
#       endif
#   endif
#   include <windows.h>
#endif

#include <vector>
#include <list>
#include <string>
#include <map>
#include <set>
#include <new> // nothrow
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cassert>
#include <vector> // vector
#include <deque>  // deque
#include <cmath>  // exp, log, ceil
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits> // numeric_limit

#ifndef _WIN32
#include <errno.h>
#endif

#include <deque>
#include <algorithm>

/*
 * Workaround for some compilers are has no those macros in their headers!
 */
#ifndef INT8_MIN
#define INT8_MIN    (-0x7f - 1)
#endif
#ifndef INT16_MIN
#define INT16_MIN   (-0x7fff - 1)
#endif
#ifndef INT32_MIN
#define INT32_MIN   (-0x7fffffff - 1)
#endif
#ifndef INT8_MAX
#define INT8_MAX    0x7f
#endif
#ifndef INT16_MAX
#define INT16_MAX   0x7fff
#endif
#ifndef INT32_MAX
#define INT32_MAX   0x7fffffff
#endif

class FileAndMemReader;

#ifndef OPNMIDI_DISABLE_MIDI_SEQUENCER
// Rename class to avoid ABI collisions
#define BW_MidiSequencer OpnMidiSequencer
class BW_MidiSequencer;
typedef BW_MidiSequencer MidiSequencer;
typedef struct BW_MidiRtInterface BW_MidiRtInterface;
#endif//OPNMIDI_DISABLE_MIDI_SEQUENCER

class OPN2;
class OPNChipBase;

typedef class OPN2 Synth;

#include "opnbank.h"

#define OPNMIDI_BUILD
#include "opnmidi.h"    //Main API

#include "opnmidi_ptr.hpp"

class MIDIplay;

#define ADL_UNUSED(x) (void)x

#define OPN_MAX_CHIPS 100
#define OPN_MAX_CHIPS_STR "100"

extern std::string OPN2MIDI_ErrorString;

/*
  Sample conversions to various formats
*/
template <class Real>
inline Real opn2_cvtReal(int32_t x)
{
    return static_cast<Real>(x) * (static_cast<Real>(1) / static_cast<Real>(INT16_MAX));
}
inline int32_t opn2_cvtS16(int32_t x)
{
    x = (x < INT16_MIN) ? INT16_MIN : x;
    x = (x > INT16_MAX) ? INT16_MAX : x;
    return x;
}
inline int32_t opn2_cvtS8(int32_t x)
{
    return opn2_cvtS16(x) / 256;
}
inline int32_t opn2_cvtS24(int32_t x)
{
    return opn2_cvtS16(x) * 256;
}
inline int32_t opn2_cvtS32(int32_t x)
{
    return opn2_cvtS16(x) * 65536;
}
inline int32_t opn2_cvtU16(int32_t x)
{
    return opn2_cvtS16(x) - INT16_MIN;
}
inline int32_t opn2_cvtU8(int32_t x)
{
    return opn2_cvtS8(x) - INT8_MIN;
}
inline int32_t opn2_cvtU24(int32_t x)
{
    enum { int24_min = -(1 << 23) };
    return opn2_cvtS24(x) - int24_min;
}
inline int32_t opn2_cvtU32(int32_t x)
{
    // unsigned operation because overflow on signed integers is undefined
    return (uint32_t)opn2_cvtS32(x) - (uint32_t)INT32_MIN;
}

template<typename T>
void opn2_fill_vector(std::vector<T > &v, const T &value)
{
    for(typename std::vector<T>::iterator it = v.begin(); it != v.end(); ++it)
        *it = value;
}

#if defined(ADLMIDI_AUDIO_TICK_HANDLER)
extern void opn2_audioTickHandler(void *instance, uint32_t chipId, uint32_t rate);
#endif

#endif // ADLMIDI_PRIVATE_HPP
