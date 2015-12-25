//hqnx filter library
//----------------------------------------------------------
//Copyright (C) 2003 MaxSt ( maxst@hiend3d.com )
//Copyright (C) 2009 Benjamin Berkels
//Copyright (C) 2012-2014 Alexey Lysiuk
//
//This program is free software; you can redistribute it and/or
//modify it under the terms of the GNU Lesser General Public
//License as published by the Free Software Foundation; either
//version 2.1 of the License, or (at your option) any later version.
//
//This program is distributed in the hope that it will be useful,
//but WITHOUT ANY WARRANTY; without even the implied warranty of
//MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
//Lesser General Public License for more details.
//
//You should have received a copy of the GNU Lesser General Public
//License along with this program; if not, write to the Free Software
//Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

#ifndef __HQNX_H__
#define __HQNX_H__

#ifdef _MSC_VER
#pragma warning(disable:4799)
#endif // _MSC_VER

#include "hqnx_asm_Image.h"

// IMPORTANT NOTE!
// The following is not a generic vectorized math class
// Each member function or overloaded operator does specific task to simplify client code
// To re-implement this class for different platform you need check very carefully
// the Intel C++ Intrinsic Reference at http://software.intel.com/file/18072/

#if defined _MSC_VER && defined _M_X64

// Implementation via SSE2 intrinsics
// MSVC doesn't support MMX intrinsics on x64

#include <emmintrin.h>

class hq_vec
{
public:
  hq_vec(const int value)
  : m_value(_mm_cvtsi32_si128(value))
  {
  }

  static hq_vec load(const int source)
  {
    return _mm_unpacklo_epi8(_mm_cvtsi32_si128(source), _mm_cvtsi32_si128(0));
  }

  static hq_vec expand(const short source)
  {
    return _mm_set_epi16(source, source, source, source, source, source, source, source);
  }

  void store(unsigned char* const destination) const
  {
    *reinterpret_cast<int*>(destination) = _mm_cvtsi128_si32(_mm_packus_epi16(m_value, _mm_cvtsi32_si128(0)));
  }

  static void reset()
  {
  }

  hq_vec& operator+=(const hq_vec& right)
  {
    m_value = _mm_add_epi16(m_value, right.m_value);
    return *this;
  }

  hq_vec& operator*=(const hq_vec& right)
  {
    m_value = _mm_mullo_epi16(m_value, right.m_value);
    return *this;
  }

  hq_vec& operator<<(const int count)
  {
    m_value = _mm_sll_epi16(m_value, _mm_cvtsi32_si128(count));
    return *this;
  }

  hq_vec& operator>>(const int count)
  {
    m_value = _mm_srl_epi16(m_value, _mm_cvtsi32_si128(count));
    return *this;
  }

private:
  __m128i m_value;

  hq_vec(const __m128i value)
  : m_value(value)
  {
  }

  friend hq_vec operator- (const hq_vec&, const hq_vec&);
  friend hq_vec operator* (const hq_vec&, const hq_vec&);
  friend hq_vec operator| (const hq_vec&, const hq_vec&);
  friend bool   operator!=(const int,     const hq_vec&);
};

inline hq_vec operator-(const hq_vec& left, const hq_vec& right)
{
  return _mm_subs_epu8(left.m_value, right.m_value);
}

inline hq_vec operator*(const hq_vec& left, const hq_vec& right)
{
  return _mm_mullo_epi16(left.m_value, right.m_value);
}

inline hq_vec operator|(const hq_vec& left, const hq_vec& right)
{
  return _mm_or_si128(left.m_value, right.m_value);
}

inline bool operator!=(const int left, const hq_vec& right)
{
  return left != _mm_cvtsi128_si32(right.m_value);
}

#else // _M_X64

// Implementation via MMX intrinsics

#include <mmintrin.h>

class hq_vec
{
public:
  hq_vec(const int value)
  : m_value(_mm_cvtsi32_si64(value))
  {
  }

  static hq_vec load(const int source)
  {
    return _mm_unpacklo_pi8(_mm_cvtsi32_si64(source), _mm_cvtsi32_si64(0));
  }

  static hq_vec expand(const short source)
  {
    return _mm_set_pi16(source, source, source, source);
  }

  void store(unsigned char* const destination) const
  {
    *reinterpret_cast<int*>(destination) = _mm_cvtsi64_si32(_mm_packs_pu16(m_value, _mm_cvtsi32_si64(0)));
  }

  static void reset()
  {
    _mm_empty();
  }

  hq_vec& operator+=(const hq_vec& right)
  {
    m_value = _mm_add_pi16(m_value, right.m_value);
    return *this;
  }

  hq_vec& operator*=(const hq_vec& right)
  {
    m_value = _mm_mullo_pi16(m_value, right.m_value);
    return *this;
  }

  hq_vec& operator<<(const int count)
  {
    m_value = _mm_sll_pi16(m_value, _mm_cvtsi32_si64(count));
    return *this;
  }

  hq_vec& operator>>(const int count)
  {
    m_value = _mm_srl_pi16(m_value, _mm_cvtsi32_si64(count));
    return *this;
  }

private:
  __m64 m_value;

  hq_vec(const __m64 value)
  : m_value(value)
  {
  }

  friend hq_vec operator- (const hq_vec&, const hq_vec&);
  friend hq_vec operator* (const hq_vec&, const hq_vec&);
  friend hq_vec operator| (const hq_vec&, const hq_vec&);
  friend bool   operator!=(const int,     const hq_vec&);
};

inline hq_vec operator-(const hq_vec& left, const hq_vec& right)
{
  return _mm_subs_pu8(left.m_value, right.m_value);
}

inline hq_vec operator*(const hq_vec& left, const hq_vec& right)
{
  return _mm_mullo_pi16(left.m_value, right.m_value);
}

inline hq_vec operator|(const hq_vec& left, const hq_vec& right)
{
  return _mm_or_si64(left.m_value, right.m_value);
}

inline bool operator!=(const int left, const hq_vec& right)
{
  return left != _mm_cvtsi64_si32(right.m_value);
}

#endif // _MSC_VER && _M_X64

namespace HQnX_asm
{
void DLL hq2x_32( int * pIn, unsigned char * pOut, int Xres, int Yres, int BpL );
void DLL hq3x_32( int * pIn, unsigned char * pOut, int Xres, int Yres, int BpL );
void DLL hq4x_32( int * pIn, unsigned char * pOut, int Xres, int Yres, int BpL );
int DLL hq4x_32 ( CImage &ImageIn, CImage &ImageOut );

void DLL InitLUTs();

}


#endif //__HQNX_H__