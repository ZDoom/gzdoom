/*
** autosegs.h
** Arrays built at link-time
**
**---------------------------------------------------------------------------
** Copyright 1998-2006 Randy Heit
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

#ifndef AUTOSEGS_H
#define AUTOSEGS_H

#include <type_traits>

#if defined(__clang__)
#if defined(__has_feature) && __has_feature(address_sanitizer)
#define NO_SANITIZE __attribute__((no_sanitize("address")))
#else
#define NO_SANITIZE
#endif
#else
#define NO_SANITIZE
#endif

class FAutoSeg
{
	const char *name;
	void **begin;
	void **end;

	template <typename T>
	struct ArgumentType;

	template <typename Ret, typename Func, typename Arg>
	struct ArgumentType<Ret(Func:: *)(Arg) const>
	{
		using Type = Arg;
	};

	template <typename Func>
	using ArgumentTypeT = typename ArgumentType<Func>::Type;

	template <typename Func>
	struct ReturnType
	{
		using Type = std::invoke_result_t<Func, ArgumentTypeT<decltype(&Func::operator())>>;
	};

	template <typename Func>
	using ReturnTypeT = typename ReturnType<Func>::Type;

	template <typename Func, typename Ret>
	struct HasReturnType
	{
		static constexpr bool Value = std::is_same_v<ReturnTypeT<Func>, Ret>;
	};

	template <typename Func, typename Ret>
	static constexpr bool HasReturnTypeV = HasReturnType<Func, Ret>::Value;

	void Initialize();

public:
	explicit FAutoSeg(const char *name)
	: name(name)
	, begin(nullptr)
	, end(nullptr)
	{
		Initialize();
	}

	FAutoSeg(void** begin, void** end)
	: name(nullptr)
	, begin(begin)
	, end(end)
	{
	}

	template <typename Func>
	void ForEach(Func func, std::enable_if_t<HasReturnTypeV<Func, void>> * = nullptr)
	{
		using CallableType = decltype(&Func::operator());
		using ArgType = typename ArgumentType<CallableType>::Type;

		for (void **it = begin; it < end; ++it)
		{
			if (*it)
			{
				func(reinterpret_cast<ArgType>(*it));
			}
		}
	}

	template <typename Func>
	void ForEach(Func func, std::enable_if_t<HasReturnTypeV<Func, bool>> * = nullptr)
	{
		using CallableType = decltype(&Func::operator());
		using ArgType = typename ArgumentType<CallableType>::Type;

		for (void **it = begin; it < end; ++it)
		{
			if (*it)
			{
				if (!func(reinterpret_cast<ArgType>(*it)))
				{
					return;
				};
			}
		}
	}
};

namespace AutoSegs
{
	extern FAutoSeg ActionFunctons;
	extern FAutoSeg TypeInfos;
	extern FAutoSeg ClassFields;
	extern FAutoSeg Properties;
	extern FAutoSeg MapInfoOptions;
}

#define AUTOSEG_AREG areg
#define AUTOSEG_CREG creg
#define AUTOSEG_FREG freg
#define AUTOSEG_GREG greg
#define AUTOSEG_YREG yreg

#define AUTOSEG_STR(string) AUTOSEG_STR2(string)
#define AUTOSEG_STR2(string) #string

#ifdef __MACH__
#define AUTOSEG_MACH_SEGMENT "__DATA"
#define AUTOSEG_MACH_SECTION(section) AUTOSEG_MACH_SEGMENT "," AUTOSEG_STR(section)
#define SECTION_AREG AUTOSEG_MACH_SECTION(AUTOSEG_AREG)
#define SECTION_CREG AUTOSEG_MACH_SECTION(AUTOSEG_CREG)
#define SECTION_FREG AUTOSEG_MACH_SECTION(AUTOSEG_FREG)
#define SECTION_GREG AUTOSEG_MACH_SECTION(AUTOSEG_GREG)
#define SECTION_YREG AUTOSEG_MACH_SECTION(AUTOSEG_YREG)
#else
#define SECTION_AREG AUTOSEG_STR(AUTOSEG_AREG)
#define SECTION_CREG AUTOSEG_STR(AUTOSEG_CREG)
#define SECTION_FREG AUTOSEG_STR(AUTOSEG_FREG)
#define SECTION_GREG AUTOSEG_STR(AUTOSEG_GREG)
#define SECTION_YREG AUTOSEG_STR(AUTOSEG_YREG)
#endif

#endif
