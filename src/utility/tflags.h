/*
** tflags.h
**
**---------------------------------------------------------------------------
** Copyright 2015 Teemu Piippo
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
*/

#pragma once
#include "doomtype.h"

/*
 * TFlags
 *
 * A Qt-inspired type-safe flagset type.
 *
 * T is the enum type of individual flags,
 * TT is the underlying integer type used (defaults to uint32_t)
 */
template<typename T, typename TT = uint32_t>
class TFlags
{
	struct ZeroDummy {};

public:
	typedef TFlags<T, TT> Self;
	typedef T EnumType;
	typedef TT IntType;

	TFlags() = default;
	TFlags(const Self& other) = default;
	TFlags (T value) : Value (static_cast<TT> (value)) {}

	// This allows initializing the flagset with 0, as 0 implicitly converts into a null pointer.
	TFlags (ZeroDummy*) : Value (0) {}

	// Relation operators
	Self operator| (Self other) const { return Self::FromInt (Value | other.GetValue()); }
	Self operator& (Self other) const { return Self::FromInt (Value & other.GetValue()); }
	Self operator^ (Self other) const { return Self::FromInt (Value ^ other.GetValue()); }
	Self operator| (T value) const { return Self::FromInt (Value | value); }
	Self operator& (T value) const { return Self::FromInt (Value & value); }
	Self operator^ (T value) const { return Self::FromInt (Value ^ value); }
	Self operator~() const { return Self::FromInt (~Value); }

	// Assignment operators
	Self& operator= (Self other) { Value = other.GetValue(); return *this; }
	Self& operator|= (Self other) { Value |= other.GetValue(); return *this; }
	Self& operator&= (Self other) { Value &= other.GetValue(); return *this; }
	Self& operator^= (Self other) { Value ^= other.GetValue(); return *this; }
	Self& operator= (T value) { Value = value; return *this; }
	Self& operator|= (T value) { Value |= value; return *this; }
	Self& operator&= (T value) { Value &= value; return *this; }
	Self& operator^= (T value) { Value ^= value; return *this; }

	// Access the value of the flagset
	TT GetValue() const { return Value; }
	operator TT() const { return Value; }

	// Set the value of the flagset manually with an integer.
	// Please think twice before using this.
	static Self FromInt (TT value) { return Self (static_cast<T> (value)); }

private:
	template<typename X> Self operator| (X value) const { return Self::FromInt (Value | value); }
	template<typename X> Self operator& (X value) const { return Self::FromInt (Value & value); }
	template<typename X> Self operator^ (X value) const { return Self::FromInt (Value ^ value); }

public:	// to be removed.
	TT Value;
};

/*
 * Additional operators for TFlags types.
 */
#define DEFINE_TFLAGS_OPERATORS(T) \
inline T operator| (T::EnumType a, T::EnumType b) { return T::FromInt (T::IntType (a) | T::IntType (b)); } \
inline T operator& (T::EnumType a, T::EnumType b) { return T::FromInt (T::IntType (a) & T::IntType (b)); } \
inline T operator^ (T::EnumType a, T::EnumType b) { return T::FromInt (T::IntType (a) ^ T::IntType (b)); } \
inline T operator| (T::EnumType a, T b) { return T::FromInt (T::IntType (a) | T::IntType (b)); } \
inline T operator& (T::EnumType a, T b) { return T::FromInt (T::IntType (a) & T::IntType (b)); } \
inline T operator^ (T::EnumType a, T b) { return T::FromInt (T::IntType (a) ^ T::IntType (b)); } \
inline T operator~ (T::EnumType a) { return T::FromInt (~T::IntType (a)); }

