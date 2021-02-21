/*
** autostart.cpp
** This file contains the heads of lists stored in special data segments
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
** The particular scheme used here was chosen because it's small.
**
** An alternative that will work with any C++ compiler is to use static
** classes to build these lists at run time. Under Visual C++, doing things
** that way can require a lot of extra space, which is why I'm doing things
** this way.
**
** In the case of PClass lists (section creg), I orginally used the
** constructor to do just that, and the code for that still exists if you
** compile with something other than Visual C++ or GCC.
*/

#include "autosegs.h"

#ifdef _WIN32
#include <windows.h>
#include <dbghelp.h>
#elif defined __MACH__
#include <mach-o/getsect.h>
#include <mach-o/ldsyms.h>
#endif


#if defined _WIN32 || defined __MACH__

#define AUTOSEG_VARIABLE(name, autoseg) namespace AutoSegs{ FAutoSeg name{ AUTOSEG_STR(autoseg) }; }

#else // Linux and others with ELF executables

#define AUTOSEG_START(name) __start_##name
#define AUTOSEG_STOP(name) __stop_##name
#define AUTOSEG_VARIABLE(name, autoseg) \
	void* name##DummyPointer __attribute__((section(AUTOSEG_STR(autoseg)))) __attribute__((used)); \
	extern void* AUTOSEG_START(autoseg); \
	extern void* AUTOSEG_STOP(autoseg); \
	namespace AutoSegs { FAutoSeg name{ &AUTOSEG_START(autoseg), &AUTOSEG_STOP(autoseg) }; }

#endif

AUTOSEG_VARIABLE(ActionFunctons, AUTOSEG_AREG)
AUTOSEG_VARIABLE(TypeInfos, AUTOSEG_CREG)
AUTOSEG_VARIABLE(ClassFields, AUTOSEG_FREG)
AUTOSEG_VARIABLE(Properties, AUTOSEG_GREG)
AUTOSEG_VARIABLE(MapInfoOptions, AUTOSEG_YREG)

#undef AUTOSEG_VARIABLE
#undef AUTOSEG_STOP
#undef AUTOSEG_START


void FAutoSeg::Initialize()
{
#ifdef _WIN32

	const HMODULE selfModule = GetModuleHandle(nullptr);
	const SIZE_T baseAddress = reinterpret_cast<SIZE_T>(selfModule);

	const PIMAGE_NT_HEADERS header = ImageNtHeader(selfModule);
	PIMAGE_SECTION_HEADER section = IMAGE_FIRST_SECTION(header);

	for (WORD i = 0; i < header->FileHeader.NumberOfSections; ++i, ++section)
	{
		if (strncmp(reinterpret_cast<char *>(section->Name), name, IMAGE_SIZEOF_SHORT_NAME) == 0)
		{
			begin = reinterpret_cast<void **>(baseAddress + section->VirtualAddress);
			end = reinterpret_cast<void **>(baseAddress + section->VirtualAddress + section->SizeOfRawData);
			break;
		}
	}

#elif defined __MACH__

	unsigned long size;

	if (uint8_t *const section = getsectiondata(&_mh_execute_header, AUTOSEG_MACH_SEGMENT, name, &size))
	{
		begin = reinterpret_cast<void **>(section);
		end = reinterpret_cast<void **>(section + size);
	}

#else // Linux and others with ELF executables

	assert(false);

#endif
}


#if defined(_MSC_VER)

// We want visual styles support under XP
#if defined _M_IX86

#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='x86' publicKeyToken='6595b64144ccf1df' language='*'\"")

#elif defined _M_IA64

#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='ia64' publicKeyToken='6595b64144ccf1df' language='*'\"")

#elif defined _M_X64

#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='amd64' publicKeyToken='6595b64144ccf1df' language='*'\"")

#else

#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#endif

#endif
