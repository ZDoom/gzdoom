//
//  sse_to_neon.hpp
//  neon_test
//
//  Created by Tim Oberhauser on 11/16/13.
//  Copyright (c) 2013 Tim Oberhauser. All rights reserved.
//

#ifndef neon_test_sse_to_neon_hpp
#define neon_test_sse_to_neon_hpp

#include <arm_neon.h>

#if defined(__MM_MALLOC_H)
// copied from mm_malloc.h {
#include <stdlib.h>

/* We can't depend on <stdlib.h> since the prototype of posix_memalign
 may not be visible.  */
#ifndef __cplusplus
extern int posix_memalign (void **, size_t, size_t);
#else
extern "C" int posix_memalign (void **, size_t, size_t) throw ();
#endif

static __inline void *
_mm_malloc (size_t size, size_t alignment)
{
    void *ptr;
    if (alignment == 1)
        return malloc (size);
    if (alignment == 2 || (sizeof (void *) == 8 && alignment == 4))
        alignment = sizeof (void *);
    if (posix_memalign (&ptr, alignment, size) == 0)
        return ptr;
    else
        return NULL;
}

static __inline void
_mm_free (void * ptr)
{
    free (ptr);
}
// } copied from mm_malloc.h
#endif


typedef int16x8_t __m128i;
typedef float32x4_t __m128;


// ADDITION
inline __m128i _mm_add_epi16(const __m128i& a, const __m128i& b){
    return vaddq_s16(reinterpret_cast<int16x8_t>(a),reinterpret_cast<int16x8_t>(b));
}

inline __m128 _mm_add_ps(const __m128& a, const __m128& b){
    return vaddq_f32(a,b);
}


// SUBTRACTION
inline __m128i _mm_sub_epi16(const __m128i& a, const __m128i& b){
    return vsubq_s16(reinterpret_cast<int16x8_t>(a),reinterpret_cast<int16x8_t>(b));
}

inline __m128 _mm_sub_ps(const __m128& a, const __m128& b){
    return vsubq_f32(a,b);
}


// MULTIPLICATION
#if 0
inline __m128i _mm_mullo_epi16(const __m128i& a, const __m128i& b){
    return vqrdmulhq_s16(reinterpret_cast<int16x8_t>(a),reinterpret_cast<int16x8_t>(b));
}
#endif

inline __m128 _mm_mul_ps(const __m128& a, const __m128& b){
    return vmulq_f32(a,b);
}


// SET VALUE
inline __m128i _mm_set1_epi16(const int16_t w){
    return vmovq_n_s16(w);
}

inline __m128i _mm_setzero_si128(){
    return vmovq_n_s16(0);
}

inline __m128 _mm_set1_ps(const float32_t& w){
    return vmovq_n_f32(w);
}


// STORE
inline void _mm_storeu_si128(__m128i* p, __m128i& a){
    vst1q_s16(reinterpret_cast<int16_t*>(p),reinterpret_cast<int16x8_t>(a));
}

inline void _mm_store_ps(float32_t* p, __m128&a){
    vst1q_f32(p,a);
}


// LOAD
inline __m128i _mm_loadu_si128(__m128i* p){//For SSE address p does not need be 16-byte aligned
    return reinterpret_cast<__m128i>(vld1q_s16(reinterpret_cast<int16_t*>(p)));
}

inline __m128i _mm_load_si128(__m128i* p){//For SSE address p must be 16-byte aligned
    return reinterpret_cast<__m128i>(vld1q_s16(reinterpret_cast<int16_t*>(p)));
}

inline __m128 _mm_load_ps(const float32_t* p){
    return reinterpret_cast<__m128>(vld1q_f32(p));
}


// SHIFT OPERATIONS
inline __m128i _mm_srai_epi16(const __m128i& a, const int count){
    int16x8_t b = vmovq_n_s16(-count);
    return reinterpret_cast<__m128i>(vshlq_s16(a,b));
    //    return vrshrq_n_s16(a, count);// TODO Argument to '__builtin_neon_vrshrq_n_v' must be a constant integer
}


// MIN/MAX OPERATIONS
inline __m128 _mm_max_ps(const __m128& a, const __m128& b){
    return reinterpret_cast<__m128>(vmaxq_f32(reinterpret_cast<float32x4_t>(a),reinterpret_cast<float32x4_t>(b)));
}


// SINGLE ELEMENT ACCESS
inline int16_t _mm_extract_epi16(__m128i& a, int index){
    return (reinterpret_cast<int16_t*>(&a))[index];
    //    return vgetq_lane_s16(a,index);// TODO Argument to '__builtin_neon_vgetq_lane_i16' must be a constant integer
}


// MISCELLANOUS
inline __m128i _mm_sad_epu8 (__m128i a, __m128i b){
    uint64x2_t sad = reinterpret_cast<uint64x2_t>(vabdq_u8(reinterpret_cast<uint8x16_t>(a),reinterpret_cast<uint8x16_t>(b)));
    sad = reinterpret_cast<uint64x2_t>(vpaddlq_u8(reinterpret_cast<uint8x16_t>(sad)));
    sad = reinterpret_cast<uint64x2_t>(vpaddlq_u16(reinterpret_cast<uint16x8_t>(sad)));
    sad = vpaddlq_u32(reinterpret_cast<uint32x4_t>(sad));
    return reinterpret_cast<__m128i>(sad);
}


// LOGICAL OPERATIONS
inline __m128 _mm_and_ps(__m128& a, __m128& b){
    return reinterpret_cast<__m128>(vandq_u32(reinterpret_cast<uint32x4_t>(a),reinterpret_cast<uint32x4_t>(b)));
}


// CONVERSIONS
inline __m128i _mm_packus_epi16 (const __m128i a, const __m128i b){
    __m128i result = _mm_setzero_si128();
    int8x8_t* a_narrow = reinterpret_cast<int8x8_t*>(&result);
    int8x8_t* b_narrow = &a_narrow[1];
    *a_narrow = reinterpret_cast<int8x8_t>(vqmovun_s16(a));
    *b_narrow = reinterpret_cast<int8x8_t>(vqmovun_s16(b));
    return result;
}

// In my case this function was only needed to convert 8 bit to 16 bit integers by extending with zeros, the general case is not implemented!!!
inline __m128i _mm_unpacklo_epi8(__m128i a, const __m128i dummy_zero){
    // dummy_zero is a dummy variable
    uint8x8_t* a_low = reinterpret_cast<uint8x8_t*>(&a);
    return reinterpret_cast<__m128i>(vmovl_u8(*a_low));
}

// In my case this function was only needed to convert 8 bit to 16 bit integers by extending with zeros, the general case is not implemented!!!
inline __m128i _mm_unpackhi_epi8(__m128i a, const __m128i dummy_zero){
    // dummy_zero is a dummy variable
    uint8x8_t* a_low = reinterpret_cast<uint8x8_t*>(&a);
    return reinterpret_cast<__m128i>(vmovl_u8(a_low[1]));
}




#endif
