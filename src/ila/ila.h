/*******************************************************************************
Copyright (c) 2017 Rachael Alexanderson

Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice,
 this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
 this list of conditions and the following disclaimer in the documentation
 and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors
 may be used to endorse or promote products derived from this software
 without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 POSSIBILITY OF SUCH DAMAGE.

================================================================================

Simple In-Line Assembly support for ARM

The purpose of this file is to provide SSE2 translations to ARM NEON. Note
 that this requires a special GCC flag (which should automatically be set by
 CMAKE).

As stated in the other included file - this will never be as fast as remaking
 the SSE2 instructions from the ground up. But since I have no interest in
 doing so, myself - have at it if you want proper ARM support!

For Raspberry Pi, note that this requires RPI2 or higher.

*******************************************************************************/

#if defined(__ARM_NEON__)
#include "ila/sse_to_neon.hpp"
// credit to Magnus Norddahl
inline __m128i _mm_mullo_epi16(const __m128i& a, const __m128i& b){
    return vqrdmulhq_s16(reinterpret_cast<int16x8_t>(a),reinterpret_cast<int16x8_t>(b));
}
#else
// Just standard old Intel!
#include <emmintrin.h>
#endif

