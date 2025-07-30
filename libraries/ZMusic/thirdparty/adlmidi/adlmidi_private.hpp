/*
 * libADLMIDI is a free Software MIDI synthesizer library with OPL3 emulation
 *
 * Original ADLMIDI code: Copyright (c) 2010-2014 Joel Yliluoma <bisqwit@iki.fi>
 * ADLMIDI Library API:   Copyright (c) 2015-2025 Vitaly Novichkov <admin@wohlnet.ru>
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

// Setup compiler defines useful for exporting required public API symbols in gme.cpp
#ifndef ADLMIDI_EXPORT
#   if defined (_WIN32) && defined(ADLMIDI_BUILD_DLL)
#       define ADLMIDI_EXPORT __declspec(dllexport)
#   elif defined (LIBADLMIDI_VISIBILITY) && defined (__GNUC__)
#       define ADLMIDI_EXPORT __attribute__((visibility ("default")))
#   else
#       define ADLMIDI_EXPORT
#   endif
#endif


#ifdef _WIN32
#define NOMINMAX 1
#endif

#if defined(_WIN32) && !defined(__WATCOMC__)
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

#if defined(__DJGPP__) || (defined(__WATCOMC__) && (defined(__DOS__) || defined(__DOS4G__) || defined(__DOS4GNZ__)))
#   include <conio.h>
#   ifdef __DJGPP__
#       include <pc.h>
#       include <dpmi.h>
#       include <go32.h>
#       include <sys/farptr.h>
#       include <dos.h>
#   endif
#endif

#include <vector>
#include <list>
#include <string>
//#ifdef __WATCOMC__
//#include <myset.h> //TODO: Implemnet a workaround for OpenWatcom to fix a crash while using those containers
//#include <mymap.h>
//#else
#include <map>
#include <set>
#include <new> // nothrow
//#endif
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cassert>
#include <vector> // vector
#include <deque>  // deque
#include <cmath>  // exp, log, ceil
#if defined(__WATCOMC__)
#include <math.h> // round, sqrt
#endif
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

#ifndef ADLMIDI_DISABLE_MIDI_SEQUENCER
// Rename class to avoid ABI collisions
#define BW_MidiSequencer AdlMidiSequencer
class BW_MidiSequencer;
typedef BW_MidiSequencer MidiSequencer;
typedef struct BW_MidiRtInterface BW_MidiRtInterface;
#endif//ADLMIDI_DISABLE_MIDI_SEQUENCER

class OPL3;
class OPLChipBase;

typedef class OPL3 Synth;

#include "oplinst.h"
#include "adlmidi_db.h"

#define ADLMIDI_BUILD
#include "adlmidi.h"    //Main API

#include "adlmidi_ptr.hpp"

class MIDIplay;

#define ADL_UNUSED(x) (void)x

#ifdef ENABLE_HW_OPL_DOS
#define ADL_MAX_CHIPS 1
#define ADL_MAX_CHIPS_STR "1" //Why not just "#MaxCards" ? Watcom fails to pass this with "syntax error" :-P
#else
#define ADL_MAX_CHIPS 100
#define ADL_MAX_CHIPS_STR "100"
#endif

extern std::string ADLMIDI_ErrorString;

/*
  Sample conversions to various formats
*/
template <class Real>
inline Real adl_cvtReal(int32_t x)
{
    return static_cast<Real>(x) * (static_cast<Real>(1) / static_cast<Real>(INT16_MAX));
}

inline int32_t adl_cvtS16(int32_t x)
{
    x = (x < INT16_MIN) ? (INT16_MIN) : x;
    x = (x > INT16_MAX) ? (INT16_MAX) : x;
    return x;
}

inline int32_t adl_cvtS8(int32_t x)
{
    return adl_cvtS16(x) / 256;
}
inline int32_t adl_cvtS24(int32_t x)
{
    return adl_cvtS16(x) * 256;
}
inline int32_t adl_cvtS32(int32_t x)
{
    return adl_cvtS16(x) * 65536;
}
inline int32_t adl_cvtU16(int32_t x)
{
    return adl_cvtS16(x) - INT16_MIN;
}
inline int32_t adl_cvtU8(int32_t x)
{
    return (adl_cvtS16(x) / 256) - INT8_MIN;
}
inline int32_t adl_cvtU24(int32_t x)
{
    enum { int24_min = -(1 << 23) };
    return adl_cvtS24(x) - int24_min;
}
inline int32_t adl_cvtU32(int32_t x)
{
    // unsigned operation because overflow on signed integers is undefined
    return (uint32_t)adl_cvtS32(x) - (uint32_t)INT32_MIN;
}

template<typename T>
void adl_fill_vector(std::vector<T > &v, const T &value)
{
    for(typename std::vector<T>::iterator it = v.begin(); it != v.end(); ++it)
        *it = value;
}

#if defined(ADLMIDI_AUDIO_TICK_HANDLER)
extern void adl_audioTickHandler(void *instance, uint32_t chipId, uint32_t rate);
#endif

/**
 * @brief Automatically calculate and enable necessary count of 4-op channels on emulated chips
 * @param device Library context
 * @param silent Don't re-count channel categories
 * @return Always 0
 */
extern int adlCalculateFourOpChannels(MIDIplay *play, bool silent = false);

#ifndef DISABLE_EMBEDDED_BANKS
extern void adlFromInstrument(const BanksDump::InstrumentEntry &instIn, OplInstMeta &instOut);
#endif

#endif // ADLMIDI_PRIVATE_HPP
